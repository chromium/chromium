// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_CHROMEOS_DIRECTORY_LOADER_H_
#define COMPONENTS_DRIVE_CHROMEOS_DIRECTORY_LOADER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "base/time/default_clock.h"
#include "components/drive/chromeos/file_system_interface.h"
#include "components/drive/file_errors.h"
#include "google_apis/drive/drive_api_error_codes.h"
#include "google_apis/drive/drive_common_callbacks.h"

namespace base {
class Clock;
class SequencedTaskRunner;
}  // namespace base

namespace drive {

class EventLogger;
class JobScheduler;
class ResourceEntry;

namespace internal {

class RootFolderIdLoader;
class ChangeListLoaderObserver;
class DirectoryFetchInfo;
class LoaderController;
class ResourceMetadata;
class StartPageTokenLoader;

// DirectoryLoader is used to load directory contents.
class DirectoryLoader {
 public:
  // |clock| can be mocked for testing
  DirectoryLoader(EventLogger* logger,
                  base::SequencedTaskRunner* blocking_task_runner,
                  ResourceMetadata* resource_metadata,
                  JobScheduler* scheduler,
                  RootFolderIdLoader* root_folder_id_loader,
                  StartPageTokenLoader* start_page_token_loader,
                  LoaderController* apply_task_controller,
                  const base::FilePath& root_entry_path,
                  const std::string& team_drive_id,
                  const base::Clock* clock = base::DefaultClock::GetInstance());
  ~DirectoryLoader();

  // Adds and removes the observer.
  void AddObserver(ChangeListLoaderObserver* observer);
  void RemoveObserver(ChangeListLoaderObserver* observer);

  // Reads the directory contents.
  // |entries_callback| can be null.
  // |completion_callback| must not be null.
  void ReadDirectory(const base::FilePath& directory_path,
                     ReadDirectoryEntriesCallback entries_callback,
                     const FileOperationCallback& completion_callback);

 private:
  class FeedFetcher;
  struct ReadDirectoryCallbackState;

  // Part of ReadDirectory().
  void ReadDirectoryAfterGetEntry(
      const base::FilePath& directory_path,
      ReadDirectoryEntriesCallback entries_callback,
      const FileOperationCallback& completion_callback,
      bool should_try_loading_parent,
      const ResourceEntry* entry,
      FileError error);
  void ReadDirectoryAfterLoadParent(
      const base::FilePath& directory_path,
      ReadDirectoryEntriesCallback entries_callback,
      const FileOperationCallback& completion_callback,
      FileError error);
  void ReadDirectoryAfterGetRootFolderId(
      const base::FilePath& directory_path,
      const std::string& local_id,
      FileError error,
      base::Optional<std::string> root_folder_id);
  void ReadDirectoryAfterGetStartPageToken(
      const base::FilePath& directory_path,
      const std::string& local_id,
      const std::string& root_folder_id,
      google_apis::DriveApiErrorCode status,
      std::unique_ptr<google_apis::StartPageToken> start_page_token);

  void ReadDirectoryAfterCheckLocalState(
      const base::FilePath& directory_path,
      const std::string& remote_start_page_token,
      const std::string& local_id,
      const std::string& root_folder_id,
      const ResourceEntry* entry,
      const std::string* local_start_page_token,
      FileError error);

  // Part of ReadDirectory().
  // This function should be called when the directory load is complete.
  // Flushes the callbacks waiting for the directory to be loaded.
  void OnDirectoryLoadComplete(const std::string& local_id, FileError error);
  void OnDirectoryLoadCompleteAfterRead(const std::string& local_id,
                                        const ResourceEntryVector* entries,
                                        FileError error);

  // Sends |entries| to the callbacks.
  void SendEntries(const std::string& local_id,
                   const ResourceEntryVector& entries);

  // ================= Implementation for directory loading =================
  // Loads the directory contents from server, and updates the local metadata.
  // Runs |callback| when it is finished.
  void LoadDirectoryFromServer(const DirectoryFetchInfo& directory_fetch_info,
                               const std::string& root_folder_id);

  // Part of LoadDirectoryFromServer() for a normal directory.
  void LoadDirectoryFromServerAfterLoad(
      const DirectoryFetchInfo& directory_fetch_info,
      FeedFetcher* fetcher,
      FileError error);

  // Part of LoadDirectoryFromServer().
  void LoadDirectoryFromServerAfterUpdateStartPageToken(
      const DirectoryFetchInfo& directory_fetch_info,
      const base::FilePath* directory_path,
      FileError error);

  EventLogger* logger_;  // Not owned.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  ResourceMetadata* resource_metadata_;  // Not owned.
  JobScheduler* scheduler_;  // Not owned.
  RootFolderIdLoader* root_folder_id_loader_;      // Not owned.
  StartPageTokenLoader* start_page_token_loader_;  // Not owned
  LoaderController* loader_controller_;  // Not owned.
  base::ObserverList<ChangeListLoaderObserver>::Unchecked observers_;
  typedef std::map<std::string, std::vector<ReadDirectoryCallbackState> >
      LoadCallbackMap;
  LoadCallbackMap pending_load_callback_;

  // Set of the running feed fetcher for the fast fetch.
  std::set<std::unique_ptr<FeedFetcher>> fast_fetch_feed_fetcher_set_;

  // The root entry path for changes being loaded by this directory loader.
  // Can be a team drive root entry or for the users default corpus will be the
  // drive root entry.
  const base::FilePath root_entry_path_;

  // The team drive id for this directory loader. Used to retrieve the start
  // page token when performing a fast fetch.
  const std::string team_drive_id_;

  const base::Clock* clock_;  // Not Owned

  THREAD_CHECKER(thread_checker_);

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DirectoryLoader> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DirectoryLoader);
};

}  // namespace internal
}  // namespace drive

#endif  // COMPONENTS_DRIVE_CHROMEOS_DIRECTORY_LOADER_H_
