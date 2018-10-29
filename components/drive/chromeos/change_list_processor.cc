// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/chromeos/change_list_processor.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/cancellation_flag.h"
#include "components/drive/chromeos/drive_file_util.h"
#include "components/drive/chromeos/resource_metadata.h"
#include "components/drive/drive.pb.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/file_change.h"
#include "components/drive/file_system_core_util.h"
#include "components/drive/resource_entry_conversion.h"
#include "google_apis/drive/drive_api_parser.h"

namespace drive {
namespace internal {

namespace {

// Returns true if it's OK to overwrite the local entry with the remote one.
bool ShouldApplyChange(const ResourceEntry& local_entry,
                       const ResourceEntry& remote_entry) {
  if (local_entry.metadata_edit_state() == ResourceEntry::CLEAN)
    return true;
  return base::Time::FromInternalValue(remote_entry.modification_date()) >
         base::Time::FromInternalValue(local_entry.modification_date());
}

}  // namespace

DirectoryFetchInfo::DirectoryFetchInfo() = default;

DirectoryFetchInfo::~DirectoryFetchInfo() = default;

DirectoryFetchInfo::DirectoryFetchInfo(const std::string& local_id,
                                       const std::string& resource_id,
                                       const std::string& start_page_token,
                                       const base::FilePath& root_entry_path,
                                       const base::FilePath& directory_path)
    : local_id_(local_id),
      resource_id_(resource_id),
      start_page_token_(start_page_token),
      root_entry_path_(root_entry_path),
      directory_path_(directory_path) {}

DirectoryFetchInfo::DirectoryFetchInfo(const DirectoryFetchInfo& other) =
    default;

std::string DirectoryFetchInfo::ToString() const {
  return ("local_id: " + local_id_ + ", resource_id: " + resource_id_ +
          ", start_page_token: " + start_page_token_ +
          ", root_entry_path: " + root_entry_path_.value() +
          ", directory_path: " + directory_path_.value());
}

ChangeList::ChangeList() = default;

ChangeList::ChangeList(const google_apis::TeamDriveList& team_drive_list) {
  const std::vector<std::unique_ptr<google_apis::TeamDriveResource>>& items =
      team_drive_list.items();
  entries_.resize(items.size());
  parent_resource_ids_.resize(items.size(), "");
  for (size_t i = 0; i < items.size(); ++i) {
    ConvertTeamDriveResourceToResourceEntry(*items[i], &entries_[i]);
  }
}

ChangeList::ChangeList(const google_apis::ChangeList& change_list)
    : next_url_(change_list.next_link()),
      new_start_page_token_(change_list.new_start_page_token()) {
  const std::vector<std::unique_ptr<google_apis::ChangeResource>>& items =
      change_list.items();
  entries_.resize(items.size());
  parent_resource_ids_.resize(items.size());
  for (size_t i = 0; i < items.size(); ++i) {
    ConvertChangeResourceToResourceEntry(*items[i], &entries_[i],
                                         &parent_resource_ids_[i]);
  }
}

ChangeList::ChangeList(const google_apis::FileList& file_list)
    : next_url_(file_list.next_link()) {
  const std::vector<std::unique_ptr<google_apis::FileResource>>& items =
      file_list.items();
  entries_.resize(items.size());
  parent_resource_ids_.resize(items.size());
  for (size_t i = 0; i < items.size(); ++i) {
    ConvertFileResourceToResourceEntry(*items[i], &entries_[i],
                                       &parent_resource_ids_[i]);
  }
}

ChangeList::~ChangeList() = default;

class ChangeListProcessor::ChangeListToEntryMapUMAStats {
 public:
  ChangeListToEntryMapUMAStats()
      : num_regular_files_(0), num_hosted_documents_(0) {}

  // Increments number of files.
  void IncrementNumFiles(bool is_hosted_document) {
    is_hosted_document ? num_hosted_documents_++ : num_regular_files_++;
  }

  // Updates UMA histograms with file counts.
  void UpdateFileCountUmaHistograms() {
    const int num_total_files = num_hosted_documents_ + num_regular_files_;
    UMA_HISTOGRAM_COUNTS_1M("Drive.NumberOfRegularFiles", num_regular_files_);
    UMA_HISTOGRAM_COUNTS_1M("Drive.NumberOfHostedDocuments",
                            num_hosted_documents_);
    UMA_HISTOGRAM_COUNTS_1M("Drive.NumberOfTotalFiles", num_total_files);
  }

 private:
  int num_regular_files_;
  int num_hosted_documents_;
};

ChangeListProcessor::ChangeListProcessor(const std::string& team_drive_id,
                                         const base::FilePath& root_entry_path,
                                         ResourceMetadata* resource_metadata,
                                         base::CancellationFlag* in_shutdown)
    : resource_metadata_(resource_metadata),
      in_shutdown_(in_shutdown),
      changed_files_(new FileChange),
      changed_team_drives_(new FileChange),
      team_drive_id_(team_drive_id),
      root_entry_path_(root_entry_path) {}

ChangeListProcessor::~ChangeListProcessor() = default;

FileError ChangeListProcessor::ApplyUserChangeList(
    const std::string& start_page_token,
    const std::string& root_resource_id,
    std::vector<std::unique_ptr<ChangeList>> change_lists,
    bool is_delta_update) {
  std::string new_start_page_token = start_page_token;
  if (is_delta_update) {
    if (!change_lists.empty()) {
      // The start_page_token appears in the first page of the change list.
      // The start_page_token does not appear in the full resource list.
      new_start_page_token = change_lists[0]->new_start_page_token();
      DCHECK(!new_start_page_token.empty());
    }
  }

  // Update the resource ID of the entry, if required.

  // Multiple team drives can have the same root_entry_path_, so try looking up
  // via the team_drive_id first.
  ResourceEntry root;
  FileError error = FILE_ERROR_OK;
  if (!team_drive_id_.empty()) {
    std::string local_id;
    error = resource_metadata_->GetIdByResourceId(team_drive_id_, &local_id);
    if (error != FILE_ERROR_OK) {
      LOG(ERROR) << "Failed to get team drive local id: "
                 << FileErrorToString(error);
      return error;
    }
    error = resource_metadata_->GetResourceEntryById(local_id, &root);
    if (error != FILE_ERROR_OK) {
      LOG(ERROR) << "Failed to get team drive root entry: "
                 << FileErrorToString(error);
      return error;
    }
  } else {
    error = resource_metadata_->GetResourceEntryByPath(root_entry_path_, &root);
    if (error != FILE_ERROR_OK) {
      LOG(ERROR) << "Failed to get root entry: " << FileErrorToString(error);
      return error;
    }
  }
  // Only update if the root resource id has changed. This will happen for the
  // default corpus on the first load, as we obtain the resource id lazily.
  if (root_resource_id != root.resource_id()) {
    root.set_resource_id(root_resource_id);
    error = resource_metadata_->RefreshEntry(root);
    if (error != FILE_ERROR_OK) {
      LOG(ERROR) << "Failed to update root entry: " << FileErrorToString(error);
      return error;
    }
  }

  ChangeListToEntryMapUMAStats uma_stats;
  error = ApplyChangeListInternal(std::move(change_lists), new_start_page_token,
                                  &root, &uma_stats);
  if (error != FILE_ERROR_OK)
    return error;

  // Update start_page_token in the metadata header.
  error = SetStartPageToken(resource_metadata_, team_drive_id_,
                            new_start_page_token);
  if (error != FILE_ERROR_OK) {
    DLOG(ERROR) << "SetStartPageToken failed: " << FileErrorToString(error);
    return error;
  }

  // Shouldn't record histograms when processing delta update.
  if (!is_delta_update)
    uma_stats.UpdateFileCountUmaHistograms();

  return FILE_ERROR_OK;
}

FileError ChangeListProcessor::ApplyChangeListInternal(
    std::vector<std::unique_ptr<ChangeList>> change_lists,
    const std::string& start_page_token,
    ResourceEntry* root,
    ChangeListToEntryMapUMAStats* uma_stats) {
  ConvertChangeListsToMap(std::move(change_lists), start_page_token, uma_stats);
  FileError error = ApplyEntryMap(root->resource_id());
  if (error != FILE_ERROR_OK) {
    DLOG(ERROR) << "ApplyEntryMap failed: " << FileErrorToString(error);
    return error;
  }
  // Update start_page_token of the root entry.
  root->mutable_directory_specific_info()->set_start_page_token(
      start_page_token);
  error = resource_metadata_->RefreshEntry(*root);
  if (error != FILE_ERROR_OK)
    DLOG(ERROR) << "RefreshEntry failed: " << FileErrorToString(error);
  return error;
}

void ChangeListProcessor::ConvertChangeListsToMap(
    std::vector<std::unique_ptr<ChangeList>> change_lists,
    const std::string& start_page_token,
    ChangeListToEntryMapUMAStats* uma_stats) {
  for (size_t i = 0; i < change_lists.size(); ++i) {
    ChangeList* change_list = change_lists[i].get();

    std::vector<ResourceEntry>* entries = change_list->mutable_entries();
    for (size_t i = 0; i < entries->size(); ++i) {
      ResourceEntry* entry = &(*entries)[i];

      // Count the number of files.
      if (!entry->file_info().is_directory()) {
        uma_stats->IncrementNumFiles(
            entry->file_specific_info().is_hosted_document());
      }
      parent_resource_id_map_[entry->resource_id()] =
          change_list->parent_resource_ids()[i];
      entry_map_[entry->resource_id()].Swap(entry);
      LOG_IF(WARNING, !entry->resource_id().empty())
          << "Found duplicated file: " << entry->base_name();
    }
  }

  // Add the largest start_page_token for directories.
  for (ResourceEntryMap::iterator it = entry_map_.begin();
       it != entry_map_.end(); ++it) {
    if (it->second.file_info().is_directory()) {
      it->second.mutable_directory_specific_info()->set_start_page_token(
          start_page_token);
    }
  }
}

FileError ChangeListProcessor::ApplyEntryMap(
    const std::string& root_resource_id) {
  // Gather the set of changes in the old path.
  // Note that we want to notify the change in both old and new paths (suppose
  // /a/b/c is moved to /x/y/c. We want to notify both "/a/b" and "/x/y".)
  // The old paths must be calculated before we apply any actual changes.
  // The new paths are calculated after each change is applied. It correctly
  // sets the new path because we apply changes in such an order (see below).
  for (ResourceEntryMap::iterator it = entry_map_.begin();
       it != entry_map_.end(); ++it) {
    UpdateChangedDirs(it->second);
  }

  // Apply all entries except deleted ones to the metadata.
  std::vector<std::string> deleted_resource_ids;
  while (!entry_map_.empty()) {
    if (in_shutdown_ && in_shutdown_->IsSet())
      return FILE_ERROR_ABORT;

    ResourceEntryMap::iterator it = entry_map_.begin();

    // Process deleted entries later to avoid deleting moved entries under it.
    if (it->second.deleted()) {
      deleted_resource_ids.push_back(it->first);
      entry_map_.erase(it);
      continue;
    }

    // Start from entry_map_.begin() and traverse ancestors using the
    // parent-child relationships in the result (after this apply) tree.
    // Then apply the topmost change first.
    //
    // By doing this, assuming the result tree does not contain any cycles, we
    // can guarantee that no cycle is made during this apply (i.e. no entry gets
    // moved under any of its descendants) because the following conditions are
    // always satisfied in any move:
    // - The new parent entry is not a descendant of the moved entry.
    // - The new parent and its ancestors will no longer move during this apply.
    std::vector<ResourceEntryMap::iterator> entries;
    for (ResourceEntryMap::iterator it = entry_map_.begin();
         it != entry_map_.end();) {
      entries.push_back(it);

      DCHECK(parent_resource_id_map_.count(it->first)) << it->first;
      const std::string& parent_resource_id =
          parent_resource_id_map_[it->first];

      if (parent_resource_id.empty())  // This entry has no parent.
        break;

      ResourceEntryMap::iterator it_parent =
          entry_map_.find(parent_resource_id);
      if (it_parent == entry_map_.end()) {
        // Current entry's parent is already updated or not going to be updated,
        // get the parent from the local tree.
        std::string parent_local_id;
        FileError error = resource_metadata_->GetIdByResourceId(
            parent_resource_id, &parent_local_id);
        if (error != FILE_ERROR_OK) {
          // See crbug.com/326043. In some complicated situations, parent folder
          // for shared entries may be accessible (and hence its resource id is
          // included), but not in the change/file list.
          // In such a case, clear the parent and move it to drive/other.
          if (error == FILE_ERROR_NOT_FOUND) {
            parent_resource_id_map_[it->first] = "";
          } else {
            LOG(ERROR) << "Failed to get local ID: " << parent_resource_id
                       << ", error = " << FileErrorToString(error);
          }
          break;
        }
        ResourceEntry parent_entry;
        while (it_parent == entry_map_.end() && !parent_local_id.empty()) {
          error = resource_metadata_->GetResourceEntryById(
              parent_local_id, &parent_entry);
          if (error != FILE_ERROR_OK) {
            LOG(ERROR) << "Failed to get local entry: "
                       << FileErrorToString(error);
            break;
          }
          it_parent = entry_map_.find(parent_entry.resource_id());
          parent_local_id = parent_entry.parent_local_id();
        }
      }
      it = it_parent;
    }

    // Apply the parent first.
    std::reverse(entries.begin(), entries.end());
    for (size_t i = 0; i < entries.size(); ++i) {
      // Skip root entry in the change list. We don't expect servers to send
      // root entry, but we should better be defensive (see crbug.com/297259).
      ResourceEntryMap::iterator it = entries[i];
      if (it->first != root_resource_id) {
        FileError error = ApplyEntry(it->second);
        if (error != FILE_ERROR_OK) {
          LOG(ERROR) << "ApplyEntry failed: " << FileErrorToString(error)
                     << ", title = " << it->second.title();
          return error;
        }
      }
      entry_map_.erase(it);
    }
  }

  // Apply deleted entries.
  for (size_t i = 0; i < deleted_resource_ids.size(); ++i) {
    std::string local_id;
    FileError error = resource_metadata_->GetIdByResourceId(
        deleted_resource_ids[i], &local_id);
    switch (error) {
      case FILE_ERROR_OK:
        error = resource_metadata_->RemoveEntry(local_id);
        break;
      case FILE_ERROR_NOT_FOUND:
        error = FILE_ERROR_OK;
        break;
      default:
        break;
    }
    if (error != FILE_ERROR_OK) {
      LOG(ERROR) << "Failed to delete: " << FileErrorToString(error)
                 << ", resource_id = " << deleted_resource_ids[i];
      return error;
    }
  }

  return FILE_ERROR_OK;
}

FileError ChangeListProcessor::ApplyEntry(const ResourceEntry& entry) {
  DCHECK(!entry.deleted());
  DCHECK(!entry.resource_id().empty());
  DCHECK(parent_resource_id_map_.count(entry.resource_id()));
  const std::string& parent_resource_id =
      parent_resource_id_map_[entry.resource_id()];

  ResourceEntry new_entry(entry);
  FileError error = SetParentLocalIdOfEntry(resource_metadata_, &new_entry,
                                            parent_resource_id);
  if (error != FILE_ERROR_OK)
    return error;

  // Lookup the entry.
  std::string local_id;
  error = resource_metadata_->GetIdByResourceId(entry.resource_id(), &local_id);

  ResourceEntry existing_entry;
  if (error == FILE_ERROR_OK)
    error = resource_metadata_->GetResourceEntryById(local_id, &existing_entry);

  switch (error) {
    case FILE_ERROR_OK:
      if (ShouldApplyChange(existing_entry, new_entry)) {
        // Entry exists and needs to be refreshed.
        new_entry.set_local_id(local_id);
        // Keep the to-be-synced properties of the existing resource entry.
        new_entry.mutable_new_properties()->CopyFrom(
            existing_entry.new_properties());
        error = resource_metadata_->RefreshEntry(new_entry);
      } else {
        if (entry.file_info().is_directory()) {
          // No need to refresh, but update the start_page_token.
          new_entry = existing_entry;
          new_entry.mutable_directory_specific_info()->set_start_page_token(
              new_entry.directory_specific_info().start_page_token());
          error = resource_metadata_->RefreshEntry(new_entry);
        }
        DVLOG(1) << "Change was discarded for: " << entry.resource_id();
      }
      break;
    case FILE_ERROR_NOT_FOUND: {  // Adding a new entry.
      std::string local_id;
      error = resource_metadata_->AddEntry(new_entry, &local_id);
      break;
    }
    default:
      return error;
  }
  if (error != FILE_ERROR_OK)
    return error;

  UpdateChangedDirs(entry);
  return FILE_ERROR_OK;
}

// static
FileError ChangeListProcessor::RefreshDirectory(
    ResourceMetadata* resource_metadata,
    const DirectoryFetchInfo& directory_fetch_info,
    std::unique_ptr<ChangeList> change_list,
    std::vector<ResourceEntry>* out_refreshed_entries) {
  DCHECK(!directory_fetch_info.empty());

  ResourceEntry directory;
  FileError error = resource_metadata->GetResourceEntryById(
      directory_fetch_info.local_id(), &directory);
  if (error != FILE_ERROR_OK)
    return error;

  if (!directory.file_info().is_directory())
    return FILE_ERROR_NOT_A_DIRECTORY;

  std::vector<ResourceEntry>* entries = change_list->mutable_entries();
  for (size_t i = 0; i < entries->size(); ++i) {
    ResourceEntry* entry = &(*entries)[i];
    const std::string& parent_resource_id =
        change_list->parent_resource_ids()[i];

    // Skip if the parent resource ID does not match. This is needed to
    // handle entries with multiple parents. For such entries, the first
    // parent is picked and other parents are ignored, hence some entries may
    // have a parent resource ID which does not match the target directory's.
    if (parent_resource_id != directory_fetch_info.resource_id()) {
      DVLOG(1) << "Wrong-parent entry rejected: " << entry->resource_id();
      continue;
    }

    entry->set_parent_local_id(directory_fetch_info.local_id());

    std::string local_id;
    error = resource_metadata->GetIdByResourceId(entry->resource_id(),
                                                 &local_id);
    if (error == FILE_ERROR_OK) {
      entry->set_local_id(local_id);
      error = resource_metadata->RefreshEntry(*entry);
    }

    if (error == FILE_ERROR_NOT_FOUND) {  // If refreshing fails, try adding.
      entry->clear_local_id();
      error = resource_metadata->AddEntry(*entry, &local_id);
    }

    if (error != FILE_ERROR_OK)
      return error;

    ResourceEntry result_entry;
    error = resource_metadata->GetResourceEntryById(local_id, &result_entry);
    if (error != FILE_ERROR_OK)
      return error;
    out_refreshed_entries->push_back(result_entry);
  }
  return FILE_ERROR_OK;
}

// static
FileError ChangeListProcessor::SetParentLocalIdOfEntry(
    ResourceMetadata* resource_metadata,
    ResourceEntry* entry,
    const std::string& parent_resource_id) {
  if (entry->parent_local_id() == util::kDriveTeamDrivesDirLocalId) {
    // When |entry| is a root directory of a Team Drive, the parent directory
    // of it is "/team_drives", which doesn't have resource ID.
    return FILE_ERROR_OK;
  }
  std::string parent_local_id;
  if (parent_resource_id.empty()) {
    // Entries without parents should go under "other" directory.
    parent_local_id = util::kDriveOtherDirLocalId;
  } else {
    FileError error = resource_metadata->GetIdByResourceId(
        parent_resource_id, &parent_local_id);
    if (error != FILE_ERROR_OK)
      return error;
  }
  entry->set_parent_local_id(parent_local_id);
  return FILE_ERROR_OK;
}

void ChangeListProcessor::UpdateChangedDirs(const ResourceEntry& entry) {
  DCHECK(!entry.resource_id().empty());

  std::string local_id;
  base::FilePath file_path;
  if (resource_metadata_->GetIdByResourceId(
          entry.resource_id(), &local_id) == FILE_ERROR_OK)
    resource_metadata_->GetFilePath(local_id, &file_path);

  if (!file_path.empty()) {
    FileChange::ChangeType type = entry.deleted()
                                      ? FileChange::CHANGE_TYPE_DELETE
                                      : FileChange::CHANGE_TYPE_ADD_OR_UPDATE;
    changed_files_->Update(file_path, entry, type);

    if (entry.file_info().is_team_drive_root()) {
      changed_team_drives_->Update(file_path, entry, type);
    }
  }
}

}  // namespace internal
}  // namespace drive
