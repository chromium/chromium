// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_CHROMEOS_CHANGE_LIST_PROCESSOR_H_
#define COMPONENTS_DRIVE_CHROMEOS_CHANGE_LIST_PROCESSOR_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/drive/file_errors.h"
#include "url/gurl.h"

namespace base {
class CancellationFlag;
}  // namespace base

namespace google_apis {
class ChangeList;
class FileList;
class TeamDriveList;
}  // namespace google_apis

namespace drive {

class FileChange;
class ResourceEntry;

namespace internal {

class ResourceMetadata;

// Holds information needed to fetch contents of a directory.
// This object is copyable.
class DirectoryFetchInfo {
 public:
  DirectoryFetchInfo();
  ~DirectoryFetchInfo();

  DirectoryFetchInfo(const std::string& local_id,
                     const std::string& resource_id,
                     const std::string& start_page_token,
                     const base::FilePath& root_entry_path,
                     const base::FilePath& directory_path);

  DirectoryFetchInfo(const DirectoryFetchInfo& other);

  // Returns true if the object is empty.
  bool empty() const { return local_id_.empty(); }

  // Local ID of the directory.
  const std::string& local_id() const { return local_id_; }

  // Resource ID of the directory.
  const std::string& resource_id() const { return resource_id_; }

  // Start Page Token of the directory. The start page token is used to
  // determine if the directory contents should be fetched.
  const std::string& start_page_token() const { return start_page_token_; }

  // The root path of the directory being fetched.
  const base::FilePath& root_entry_path() const { return root_entry_path_; }

  // The directory path that we are fetching. Used for logging.
  const base::FilePath& directory_path() const { return directory_path_; }

  // Returns a string representation of this object.
  std::string ToString() const;

 private:
  const std::string local_id_;
  const std::string resource_id_;
  const std::string start_page_token_;
  const base::FilePath root_entry_path_;
  const base::FilePath directory_path_;
};

// Class to represent a change list.
class ChangeList {
 public:
  ChangeList();  // For tests.
  explicit ChangeList(const google_apis::ChangeList& change_list);
  explicit ChangeList(const google_apis::FileList& file_list);
  explicit ChangeList(const google_apis::TeamDriveList& team_drive_list);
  ~ChangeList();

  const std::vector<ResourceEntry>& entries() const { return entries_; }
  std::vector<ResourceEntry>* mutable_entries() { return &entries_; }
  const std::vector<std::string>& parent_resource_ids() const {
    return parent_resource_ids_;
  }
  std::vector<std::string>* mutable_parent_resource_ids() {
    return &parent_resource_ids_;
  }
  const GURL& next_url() const { return next_url_; }

  const std::string& new_start_page_token() const {
    return new_start_page_token_;
  }

  void set_new_start_page_token(const std::string& start_page_token) {
    new_start_page_token_ = start_page_token;
  }

 private:
  std::vector<ResourceEntry> entries_;
  std::vector<std::string> parent_resource_ids_;
  GURL next_url_;
  std::string new_start_page_token_;

  DISALLOW_COPY_AND_ASSIGN(ChangeList);
};

// ChangeListProcessor is used to process change lists, or full resource
// lists from WAPI (codename for Documents List API) or Google Drive API, and
// updates the resource metadata stored locally.
class ChangeListProcessor {
 public:
  ChangeListProcessor(const std::string& team_drive_id,
                      const base::FilePath& root_entry_path,
                      ResourceMetadata* resource_metadata,
                      base::CancellationFlag* in_shutdown);
  ~ChangeListProcessor();

  // Applies user's change lists or full resource lists to
  // |resource_metadata_|.
  //
  // |is_delta_update| determines the type of input data to process, whether
  // it is full resource lists (false) or change lists (true).
  //
  // Must be run on the same task runner as |resource_metadata_| uses.
  // |start_page_token| is the start page token used to retrieve the change
  // list.
  // |root_resource_id| is the resource id to lookup the root folder of the
  // changeslists in resource metadata.
  FileError ApplyUserChangeList(
      const std::string& start_page_token,
      const std::string& root_resource_id,
      std::vector<std::unique_ptr<ChangeList>> change_lists,
      bool is_delta_update);

  // The set of changed files as a result of change list processing.
  const FileChange& changed_files() const { return *changed_files_; }

  // The set of team drives changes as a result of change list processing.
  // Note that a team drive change will appear in both changed_files() and
  // changed_team_drives()
  const FileChange& changed_team_drives() const {
    return *changed_team_drives_;
  }

  // Adds or refreshes the child entries from |change_list| to the directory.
  static FileError RefreshDirectory(
      ResourceMetadata* resource_metadata,
      const DirectoryFetchInfo& directory_fetch_info,
      std::unique_ptr<ChangeList> change_list,
      std::vector<ResourceEntry>* out_refreshed_entries);

  // Sets |entry|'s parent_local_id.
  static FileError SetParentLocalIdOfEntry(
      ResourceMetadata* resource_metadata,
      ResourceEntry* entry,
      const std::string& parent_resource_id);

 private:
  class ChangeListToEntryMapUMAStats;

  typedef std::map<std::string /* resource_id */, ResourceEntry>
      ResourceEntryMap;
  typedef std::map<std::string /* resource_id */,
                   std::string /* parent_resource_id*/> ParentResourceIdMap;

  // Common logic between ApplyTeamDriveChangeList and ApplyUserChangeList.
  // Applies the |change_lists| to |resource_metadta_|.
  FileError ApplyChangeListInternal(
      std::vector<std::unique_ptr<ChangeList>> change_lists,
      const std::string& start_page_token,
      ResourceEntry* root,
      ChangeListToEntryMapUMAStats* uma_stats);

  // Converts the |change_lists| to |entry_map_| and |parent_resource_id_map_|,
  // to be applied by ApplyEntryMap() later.
  void ConvertChangeListsToMap(
      std::vector<std::unique_ptr<ChangeList>> change_lists,
      const std::string& start_page_token,
      ChangeListToEntryMapUMAStats* uma_stats);

  // Applies the pre-processed metadata from entry_map_ onto the resource
  // metadata.
  FileError ApplyEntryMap(const std::string& root_resource_id);

  // Apply |entry| to resource_metadata_.
  FileError ApplyEntry(const ResourceEntry& entry);

  // Adds the directories changed by the update on |entry| to |changed_dirs_|.
  void UpdateChangedDirs(const ResourceEntry& entry);

  ResourceMetadata* resource_metadata_;  // Not owned.
  base::CancellationFlag* in_shutdown_;  // Not owned.

  ResourceEntryMap entry_map_;
  ParentResourceIdMap parent_resource_id_map_;
  std::unique_ptr<FileChange> changed_files_;
  std::unique_ptr<FileChange> changed_team_drives_;
  const std::string team_drive_id_;
  const base::FilePath& root_entry_path_;

  DISALLOW_COPY_AND_ASSIGN(ChangeListProcessor);
};

}  // namespace internal
}  // namespace drive

#endif  // COMPONENTS_DRIVE_CHROMEOS_CHANGE_LIST_PROCESSOR_H_
