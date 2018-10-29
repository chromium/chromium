// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_CHROMEOS_FILE_SYSTEM_H_
#define COMPONENTS_DRIVE_CHROMEOS_FILE_SYSTEM_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "base/time/default_clock.h"
#include "components/drive/chromeos/change_list_loader_observer.h"
#include "components/drive/chromeos/drive_operation_queue.h"
#include "components/drive/chromeos/file_system/operation_delegate.h"
#include "components/drive/chromeos/file_system_interface.h"
#include "components/drive/chromeos/team_drive_change_list_loader.h"
#include "components/drive/chromeos/team_drive_list_observer.h"
#include "google_apis/drive/drive_api_error_codes.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace google_apis {
class AboutResource;
class ResourceEntry;
}  // namespace google_apis

namespace drive {
struct ClientContext;
class EventLogger;
class FileSystemObserver;
class JobScheduler;

namespace internal {
class AboutResourceLoader;
class DriveChangeListLoader;
class FileCache;
class LoaderController;
class ResourceMetadata;
class SyncClient;
class TeamDrive;
}  // namespace internal

namespace file_system {
class CopyOperation;
class CreateDirectoryOperation;
class CreateFileOperation;
class DownloadOperation;
class GetFileForSavingOperation;
class MoveOperation;
class OpenFileOperation;
class RemoveOperation;
class SearchOperation;
class SetPropertyOperation;
class TouchOperation;
class TruncateOperation;
}  // namespace file_system

// The production implementation of FileSystemInterface.
class FileSystem : public FileSystemInterface,
                   public internal::ChangeListLoaderObserver,
                   public internal::TeamDriveListObserver,
                   public file_system::OperationDelegate {
 public:
  // |clock| can be mocked for testing.
  FileSystem(EventLogger* logger,
             internal::FileCache* cache,
             JobScheduler* scheduler,
             internal::ResourceMetadata* resource_metadata,
             base::SequencedTaskRunner* blocking_task_runner,
             const base::FilePath& temporary_file_directory,
             const base::Clock* clock = base::DefaultClock::GetInstance());
  ~FileSystem() override;

  // FileSystemInterface overrides.
  void AddObserver(FileSystemObserver* observer) override;
  void RemoveObserver(FileSystemObserver* observer) override;
  void CheckForUpdates() override;
  void CheckForUpdates(const std::set<std::string>& ids) override;
  void Search(const std::string& search_query,
              const GURL& next_link,
              SearchCallback callback) override;
  void SearchMetadata(const std::string& query,
                      int options,
                      int at_most_num_matches,
                      MetadataSearchOrder order,
                      SearchMetadataCallback callback) override;
  void SearchByHashes(const std::set<std::string>& hashes,
                      SearchByHashesCallback callback) override;
  void TransferFileFromLocalToRemote(
      const base::FilePath& local_src_file_path,
      const base::FilePath& remote_dest_file_path,
      const FileOperationCallback& callback) override;
  void OpenFile(const base::FilePath& file_path,
                OpenMode open_mode,
                const std::string& mime_type,
                OpenFileCallback callback) override;
  void Copy(const base::FilePath& src_file_path,
            const base::FilePath& dest_file_path,
            bool preserve_last_modified,
            const FileOperationCallback& callback) override;
  void Move(const base::FilePath& src_file_path,
            const base::FilePath& dest_file_path,
            const FileOperationCallback& callback) override;
  void Remove(const base::FilePath& file_path,
              bool is_recursive,
              const FileOperationCallback& callback) override;
  void CreateDirectory(const base::FilePath& directory_path,
                       bool is_exclusive,
                       bool is_recursive,
                       const FileOperationCallback& callback) override;
  void CreateFile(const base::FilePath& file_path,
                  bool is_exclusive,
                  const std::string& mime_type,
                  const FileOperationCallback& callback) override;
  void TouchFile(const base::FilePath& file_path,
                 const base::Time& last_access_time,
                 const base::Time& last_modified_time,
                 const FileOperationCallback& callback) override;
  void TruncateFile(const base::FilePath& file_path,
                    int64_t length,
                    const FileOperationCallback& callback) override;
  void Pin(const base::FilePath& file_path,
           const FileOperationCallback& callback) override;
  void Unpin(const base::FilePath& file_path,
             const FileOperationCallback& callback) override;
  void GetFile(const base::FilePath& file_path,
               GetFileCallback callback) override;
  void GetFileForSaving(const base::FilePath& file_path,
                        GetFileCallback callback) override;
  base::Closure GetFileContent(
      const base::FilePath& file_path,
      GetFileContentInitializedCallback initialized_callback,
      const google_apis::GetContentCallback& get_content_callback,
      const FileOperationCallback& completion_callback) override;
  void GetResourceEntry(const base::FilePath& file_path,
                        GetResourceEntryCallback callback) override;
  void ReadDirectory(const base::FilePath& directory_path,
                     ReadDirectoryEntriesCallback entries_callback,
                     const FileOperationCallback& completion_callback) override;
  void GetAvailableSpace(GetAvailableSpaceCallback callback) override;
  void GetMetadata(GetFilesystemMetadataCallback callback) override;
  void MarkCacheFileAsMounted(const base::FilePath& drive_file_path,
                              MarkMountedCallback callback) override;
  void IsCacheFileMarkedAsMounted(const base::FilePath& drive_file_path,
                                  IsMountedCallback callback) override;
  void MarkCacheFileAsUnmounted(const base::FilePath& cache_file_path,
                                const FileOperationCallback& callback) override;
  void AddPermission(const base::FilePath& drive_file_path,
                     const std::string& email,
                     google_apis::drive::PermissionRole role,
                     const FileOperationCallback& callback) override;
  void SetProperty(const base::FilePath& drive_file_path,
                   google_apis::drive::Property::Visibility visibility,
                   const std::string& key,
                   const std::string& value,
                   const FileOperationCallback& callback) override;
  void Reset(const FileOperationCallback& callback) override;
  void GetPathFromResourceId(const std::string& resource_id,
                             const GetFilePathCallback& callback) override;
  void FreeDiskSpaceIfNeededFor(int64_t num_bytes,
                                const FreeDiskSpaceCallback& callback) override;
  void CalculateCacheSize(const CacheSizeCallback& callback) override;
  void CalculateEvictableCacheSize(const CacheSizeCallback& callback) override;

  // file_system::OperationDelegate overrides.
  void OnFileChangedByOperation(const FileChange& changed_files) override;
  void OnEntryUpdatedByOperation(const ClientContext& context,
                                 const std::string& local_id) override;
  void OnDriveSyncError(file_system::DriveSyncErrorType type,
                        const std::string& local_id) override;
  bool WaitForSyncComplete(const std::string& local_id,
                           const FileOperationCallback& callback) override;

  // ChangeListLoader::Observer overrides.
  // Used to propagate events from ChangeListLoader.
  void OnDirectoryReloaded(const base::FilePath& directory_path) override;
  void OnFileChanged(const FileChange& changed_files) override;
  void OnTeamDrivesChanged(const FileChange& changed_team_drives) override;
  void OnLoadFromServerComplete() override;
  void OnInitialLoadComplete() override;

  // TeamDriveListObserver overrides.
  void OnTeamDriveListLoaded(
      const std::vector<internal::TeamDrive>& team_drives_list,
      const std::vector<internal::TeamDrive>& added_team_drives,
      const std::vector<internal::TeamDrive>& removed_team_drives) override;

  // Used by tests.
  internal::DriveChangeListLoader* change_list_loader_for_testing() {
    return default_corpus_change_list_loader_.get();
  }
  internal::SyncClient* sync_client_for_testing() { return sync_client_.get(); }

  internal::DriveBackgroundOperationQueue<internal::TeamDriveChangeListLoader>*
  team_drive_operation_queue_for_testing() {
    return team_drive_operation_queue_.get();
  }

 private:
  struct CreateDirectoryParams;

  // Used for initialization and Reset(). (Re-)initializes sub components that
  // need to be recreated during the reset of resource metadata and the cache.
  void ResetComponents();

  // Part of CreateDirectory(). Called after ReadDirectory()
  // is called and made sure that the resource metadata is loaded.
  void CreateDirectoryAfterRead(const CreateDirectoryParams& params,
                                FileError error);

  void FinishPin(const FileOperationCallback& callback,
                 const std::string* local_id,
                 FileError error);

  void FinishUnpin(const FileOperationCallback& callback,
                   const std::string* local_id,
                   FileError error);

  // Callback for handling about resource fetch.
  void OnGetAboutResource(
      GetAvailableSpaceCallback callback,
      google_apis::DriveApiErrorCode status,
      std::unique_ptr<google_apis::AboutResource> about_resource);

  // Stores any file error as a result of Checking updates.
  void OnUpdateChecked(const std::string& team_drive_id,
                       const base::RepeatingClosure& closure,
                       FileError error);

  // Part of CheckForUpdates(). Called when
  // ChangeListLoader::CheckForUpdates() is complete.
  void OnUpdateCompleted();

  // Part of GetResourceEntry().
  // Called when ReadDirectory() is complete.
  void GetResourceEntryAfterRead(const base::FilePath& file_path,
                                 GetResourceEntryCallback callback,
                                 FileError error);

  void OnGetMetadata(
      GetFilesystemMetadataCallback callback,
      drive::FileSystemMetadata* default_corpus_metadata,
      std::map<std::string, drive::FileSystemMetadata>* team_drive_metadata);

  // Part of AddPermission.
  void AddPermissionAfterGetResourceEntry(
      const std::string& email,
      google_apis::drive::PermissionRole role,
      const FileOperationCallback& callback,
      ResourceEntry* entry,
      FileError error);

  // Part of OnDriveSyncError().
  virtual void OnDriveSyncErrorAfterGetFilePath(
      file_system::DriveSyncErrorType type,
      const base::FilePath* file_path,
      FileError error);

  // Sub components owned by DriveIntegrationService.
  EventLogger* logger_;
  internal::FileCache* cache_;
  JobScheduler* scheduler_;
  internal::ResourceMetadata* resource_metadata_;

  // Stores debug update metadata for default corpus and team drive.
  std::map<std::string, FileSystemMetadata> last_update_metadata_;

  // Used to load about resource.
  std::unique_ptr<internal::AboutResourceLoader> about_resource_loader_;

  // Used to control ChangeListLoader.
  std::unique_ptr<internal::LoaderController> loader_controller_;

  // Used to retrieve changelists from the default corpus.
  std::unique_ptr<internal::DriveChangeListLoader>
      default_corpus_change_list_loader_;

  std::unique_ptr<internal::DriveBackgroundOperationQueue<
      internal::TeamDriveChangeListLoader>>
      team_drive_operation_queue_;

  // Used to retrieve changelists for team drives. The key for the map is the
  // team_drive_id.
  std::map<std::string, std::unique_ptr<internal::TeamDriveChangeListLoader>>
      team_drive_change_list_loaders_;

  std::unique_ptr<internal::SyncClient> sync_client_;

  base::ObserverList<FileSystemObserver>::Unchecked observers_;

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  base::FilePath temporary_file_directory_;

  const base::Clock* clock_;  // Not owned.

  // Implementation of each file system operation.
  std::unique_ptr<file_system::CopyOperation> copy_operation_;
  std::unique_ptr<file_system::CreateDirectoryOperation>
      create_directory_operation_;
  std::unique_ptr<file_system::CreateFileOperation> create_file_operation_;
  std::unique_ptr<file_system::MoveOperation> move_operation_;
  std::unique_ptr<file_system::OpenFileOperation> open_file_operation_;
  std::unique_ptr<file_system::RemoveOperation> remove_operation_;
  std::unique_ptr<file_system::TouchOperation> touch_operation_;
  std::unique_ptr<file_system::TruncateOperation> truncate_operation_;
  std::unique_ptr<file_system::DownloadOperation> download_operation_;
  std::unique_ptr<file_system::SearchOperation> search_operation_;
  std::unique_ptr<file_system::GetFileForSavingOperation>
      get_file_for_saving_operation_;
  std::unique_ptr<file_system::SetPropertyOperation> set_property_operation_;

  THREAD_CHECKER(thread_checker_);

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FileSystem> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileSystem);
};

}  // namespace drive

#endif  // COMPONENTS_DRIVE_CHROMEOS_FILE_SYSTEM_H_
