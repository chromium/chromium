// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_SERVICE_FAKE_DRIVE_SERVICE_H_
#define COMPONENTS_DRIVE_SERVICE_FAKE_DRIVE_SERVICE_H_

#include <stdint.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/drive/service/drive_service_interface.h"

namespace google_apis {
class AboutResource;
class ChangeResource;
class FileResource;
class TeamDriveResource;
}  // namespace google_apis

namespace drive {

// This class implements a fake DriveService which acts like a real Drive
// service. The fake service works as follows:
//
// 1) Load JSON files and construct the in-memory resource list.
// 2) Return valid responses based on the the in-memory resource list.
// 3) Update the in-memory resource list by requests like DeleteResource().
class FakeDriveService : public DriveServiceInterface {
 public:
  class ChangeObserver {
   public:
    virtual ~ChangeObserver() = default;
    virtual void OnNewChangeAvailable() = 0;
  };

  FakeDriveService();

  FakeDriveService(const FakeDriveService&) = delete;
  FakeDriveService& operator=(const FakeDriveService&) = delete;

  ~FakeDriveService() override;

  // Adds a Team Drive to the Team Drive resource list.
  void AddTeamDrive(const std::string& id, const std::string& name);

  // Adds a Team Drive to the Team Drive resource list with a start page token.
  void AddTeamDrive(const std::string& id,
                    const std::string& name,
                    const std::string& start_page_token);

  // Changes the offline state. All functions fail with NO_CONNECTION
  // when offline. By default the offline state is false.
  void set_offline(bool offline) { offline_ = offline; }

  // GetAllFileList never returns result when this is set to true.
  // Used to emulate the real server's slowness.
  void set_never_return_all_file_list(bool value) {
    never_return_all_file_list_ = value;
  }

  // Changes the default max results returned from GetAllFileList().
  // By default, it's set to 0, which is unlimited.
  void set_default_max_results(int default_max_results) {
    default_max_results_ = default_max_results;
  }

  // Changes the quota fields returned from GetAboutResource().
  void SetQuotaValue(int64_t used, int64_t total);

  // Returns the AboutResource.
  const google_apis::AboutResource& about_resource() const {
    return *about_resource_;
  }

  // Returns the start page token
  const google_apis::StartPageToken& start_page_token() const {
    return *start_page_token_;
  }

  // Returns the number of times the Team Drive list is successfully loaded by
  // GetAllTeamDriveList().
  int team_drive_list_load_count() const { return team_drive_list_load_count_; }

  // Returns the number of times the file list is successfully loaded by
  // GetAllFileList().
  int file_list_load_count() const { return file_list_load_count_; }

  // Returns the number of times the resource list is successfully loaded by
  // GetChangeList().
  int change_list_load_count() const { return change_list_load_count_; }

  // Returns the number of times the resource list is successfully loaded by
  // GetFileListInDirectory().
  int directory_load_count() const { return directory_load_count_; }

  // Returns the number of times the about resource is successfully loaded
  // by GetAboutResource().
  int about_resource_load_count() const { return about_resource_load_count_; }

  // Returns the number of times GetAllFileList are blocked due to
  // set_never_return_all_file_list().
  int blocked_file_list_load_count() const {
    return blocked_file_list_load_count_;
  }

  // Returns the number of times the start page token is successfully loaded
  // by GetStartPageToken().
  int start_page_token_load_count() const {
    return start_page_token_load_count_;
  }

  // Returns the file path whose request is cancelled just before this method
  // invocation.
  const base::FilePath& last_cancelled_file() const {
    return last_cancelled_file_;
  }

  // DriveServiceInterface Overrides
  void Initialize(const CoreAccountId& account_id) override;
  void AddObserver(DriveServiceObserver* observer) override;
  void RemoveObserver(DriveServiceObserver* observer) override;
  bool CanSendRequest() const override;
  std::string GetRootResourceId() const override;
  bool HasAccessToken() const override;
  void RequestAccessToken(google_apis::AuthStatusCallback callback) override;
  bool HasRefreshToken() const override;
  void ClearAccessToken() override;
  void ClearRefreshToken() override;
  google_apis::CancelCallbackOnce GetAllTeamDriveList(
      google_apis::TeamDriveListCallback callback) override;
  google_apis::CancelCallbackOnce GetAllFileList(
      const std::string& team_drive_id,
      google_apis::FileListCallback callback) override;
  google_apis::CancelCallbackOnce GetFileListInDirectory(
      const std::string& directory_resource_id,
      google_apis::FileListCallback callback) override;
  // See the comment for EntryMatchWidthQuery() in .cc file for details about
  // the supported search query types.
  google_apis::CancelCallbackOnce Search(
      const std::string& search_query,
      google_apis::FileListCallback callback) override;
  google_apis::CancelCallbackOnce SearchByTitle(
      const std::string& title,
      const std::string& directory_resource_id,
      google_apis::FileListCallback callback) override;
  google_apis::CancelCallbackOnce GetChangeList(
      int64_t start_changestamp,
      google_apis::ChangeListCallback callback) override;
  google_apis::CancelCallbackOnce GetChangeListByToken(
      const std::string& team_drive_id,
      const std::string& start_page_token,
      google_apis::ChangeListCallback callback) override;
  google_apis::CancelCallbackOnce GetRemainingChangeList(
      const GURL& next_link,
      google_apis::ChangeListCallback callback) override;
  google_apis::CancelCallbackOnce GetRemainingTeamDriveList(
      const std::string& page_token,
      google_apis::TeamDriveListCallback callback) override;
  google_apis::CancelCallbackOnce GetRemainingFileList(
      const GURL& next_link,
      google_apis::FileListCallback callback) override;
  google_apis::CancelCallbackOnce GetFileResource(
      const std::string& resource_id,
      google_apis::FileResourceCallback callback) override;
  google_apis::CancelCallbackOnce GetAboutResource(
      google_apis::AboutResourceCallback callback) override;
  google_apis::CancelCallbackOnce GetStartPageToken(
      const std::string& team_drive_id,
      google_apis::StartPageTokenCallback callback) override;
  google_apis::CancelCallbackOnce DeleteResource(
      const std::string& resource_id,
      const std::string& etag,
      google_apis::EntryActionCallback callback) override;
  google_apis::CancelCallbackOnce TrashResource(
      const std::string& resource_id,
      google_apis::EntryActionCallback callback) override;
  google_apis::CancelCallbackOnce DownloadFile(
      const base::FilePath& local_cache_path,
      const std::string& resource_id,
      google_apis::DownloadActionCallback download_action_callback,
      const google_apis::GetContentCallback& get_content_callback,
      google_apis::ProgressCallback progress_callback) override;
  google_apis::CancelCallbackOnce CopyResource(
      const std::string& resource_id,
      const std::string& parent_resource_id,
      const std::string& new_title,
      const base::Time& last_modified,
      google_apis::FileResourceCallback callback) override;
  google_apis::CancelCallbackOnce UpdateResource(
      const std::string& resource_id,
      const std::string& parent_resource_id,
      const std::string& new_title,
      const base::Time& last_modified,
      const base::Time& last_viewed_by_me,
      const google_apis::drive::Properties& properties,
      google_apis::FileResourceCallback callback) override;
  google_apis::CancelCallbackOnce AddResourceToDirectory(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      google_apis::EntryActionCallback callback) override;
  google_apis::CancelCallbackOnce RemoveResourceFromDirectory(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      google_apis::EntryActionCallback callback) override;
  google_apis::CancelCallbackOnce AddNewDirectory(
      const std::string& parent_resource_id,
      const std::string& directory_title,
      const AddNewDirectoryOptions& options,
      google_apis::FileResourceCallback callback) override;
  google_apis::CancelCallbackOnce InitiateUploadNewFile(
      const std::string& content_type,
      int64_t content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const UploadNewFileOptions& options,
      google_apis::InitiateUploadCallback callback) override;
  google_apis::CancelCallbackOnce InitiateUploadExistingFile(
      const std::string& content_type,
      int64_t content_length,
      const std::string& resource_id,
      const UploadExistingFileOptions& options,
      google_apis::InitiateUploadCallback callback) override;
  google_apis::CancelCallbackOnce ResumeUpload(
      const GURL& upload_url,
      int64_t start_position,
      int64_t end_position,
      int64_t content_length,
      const std::string& content_type,
      const base::FilePath& local_file_path,
      google_apis::drive::UploadRangeCallback callback,
      google_apis::ProgressCallback progress_callback) override;
  google_apis::CancelCallbackOnce GetUploadStatus(
      const GURL& upload_url,
      int64_t content_length,
      google_apis::drive::UploadRangeCallback callback) override;
  google_apis::CancelCallbackOnce MultipartUploadNewFile(
      const std::string& content_type,
      std::optional<std::string_view> converted_mime_type,
      int64_t content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const base::FilePath& local_file_path,
      const UploadNewFileOptions& options,
      google_apis::FileResourceCallback callback,
      google_apis::ProgressCallback progress_callback) override;
  google_apis::CancelCallbackOnce MultipartUploadExistingFile(
      const std::string& content_type,
      int64_t content_length,
      const std::string& resource_id,
      const base::FilePath& local_file_path,
      const UploadExistingFileOptions& options,
      google_apis::FileResourceCallback callback,
      google_apis::ProgressCallback progress_callback) override;
  google_apis::CancelCallbackOnce AddPermission(
      const std::string& resource_id,
      const std::string& email,
      google_apis::drive::PermissionRole role,
      google_apis::EntryActionCallback callback) override;
  std::unique_ptr<BatchRequestConfiguratorInterface> StartBatchRequest()
      override;

  // Adds a new file with the given parameters. On success, returns
  // HTTP_CREATED with the parsed entry.
  // |callback| must not be null.
  void AddNewFile(const std::string& content_type,
                  const std::string& content_data,
                  const std::string& parent_resource_id,
                  const std::string& title,
                  bool shared_with_me,
                  google_apis::FileResourceCallback callback);

  // Adds a new file with the given |resource_id|. If the id already exists,
  // it's an error. This is used for testing cross profile file sharing that
  // needs to have matching resource IDs in different fake service instances.
  // |callback| must not be null.
  void AddNewFileWithResourceId(const std::string& resource_id,
                                const std::string& content_type,
                                const std::string& content_data,
                                const std::string& parent_resource_id,
                                const std::string& title,
                                bool shared_with_me,
                                google_apis::FileResourceCallback callback);

  // Adds a new directory with the given |resource_id|.
  // |callback| must not be null.
  google_apis::CancelCallbackOnce AddNewDirectoryWithResourceId(
      const std::string& resource_id,
      const std::string& parent_resource_id,
      const std::string& directory_title,
      const AddNewDirectoryOptions& options,
      google_apis::FileResourceCallback callback);

  // Sets the last modified time for an entry specified by |resource_id|.
  // On success, returns HTTP_SUCCESS with the parsed entry.
  // |callback| must not be null.
  void SetLastModifiedTime(const std::string& resource_id,
                           const base::Time& last_modified_time,
                           google_apis::FileResourceCallback callback);

  // Sets the capabilities for an entry specified by |resource_id|.
  // On success, returns HTTP_SUCCESS with the parsed entry.
  // |callback| must not be null.
  void SetFileCapabilities(
      const std::string& resource_id,
      const google_apis::FileResourceCapabilities& capabilities,
      google_apis::FileResourceCallback callback);

  // Sets the capabilities for the team drive with ID |team_drive_id|.
  // On success, returns true.
  bool SetTeamDriveCapabilities(
      const std::string& team_drive_id,
      const google_apis::TeamDriveCapabilities& capabilities);

  // Sets the user's permission for an entry specified by |resource_id|.
  google_apis::ApiErrorCode SetUserPermission(
      const std::string& resource_id,
      google_apis::drive::PermissionRole user_permission);

  google_apis::ApiErrorCode SetFileVisibility(
      const std::string& resource_id,
      google_apis::drive::FileVisibility visibility);
  google_apis::ApiErrorCode GetFileVisibility(
      const std::string& resource_id,
      google_apis::drive::FileVisibility* visibility);

  google_apis::ApiErrorCode SetFileAsSharedWithMe(
      const std::string& resource_id);

  void AddChangeObserver(ChangeObserver* observer);
  void RemoveChangeObserver(ChangeObserver* observer);

 private:
  struct EntryInfo;
  struct UploadSession;

  // Returns a pointer to the entry that matches |resource_id|, or NULL if
  // not found.
  EntryInfo* FindEntryByResourceId(const std::string& resource_id);

  // Returns a new resource ID, which looks like "resource_id_<num>" where
  // <num> is a monotonically increasing number starting from 1.
  std::string GetNewResourceId();

  // Increments |largest_changestamp_| and adds the new changestamp.
  void AddNewChangestamp(google_apis::ChangeResource* change,
                         const std::string& team_drive_id);

  // Update ETag of |file| based on |largest_changestamp_|.
  void UpdateETag(google_apis::FileResource* file);

  // Update the latest changelist id
  void UpdateLatestChangelistId(int64_t change_list_id,
                                const std::string& team_drive_id);

  // Adds a new entry based on the given parameters.
  // |resource_id| can be empty, in the case, the id is automatically generated.
  // Returns a pointer to the newly added entry, or NULL if failed.
  const EntryInfo* AddNewEntry(const std::string& resource_id,
                               const std::string& content_type,
                               const std::string& content_data,
                               const std::string& parent_resource_id,
                               const std::string& title,
                               bool shared_with_me);

  // Adds a new entry for a team drive.
  // Returns a pointer to the newly added entry, or NULL if failed.
  const EntryInfo* AddNewTeamDriveEntry(const std::string& team_drive_id,
                                        const std::string& team_drive_name);

  // Core implementation of GetChangeList.
  // This method returns the slice of the all matched entries, and its range
  // is between |start_offset| (inclusive) and |start_offset| + |max_results|
  // (exclusive).
  // Increments *load_counter by 1 before it returns successfully.
  void GetChangeListInternal(int64_t start_changestamp,
                             const std::string& search_query,
                             const std::string& directory_resource_id,
                             const std::string& team_drive_id,
                             int start_offset,
                             int max_results,
                             int* load_counter,
                             google_apis::ChangeListOnceCallback callback);

  void GetTeamDriveListInternal(int start_offset,
                                int max_results,
                                int* load_counter,
                                google_apis::TeamDriveListCallback callback);

  // Returns new upload session URL.
  GURL GetNewUploadSessionUrl();

  void NotifyObservers();

  // The class is expected to run on UI thread.
  base::ThreadChecker thread_checker_;

  std::map<std::string, std::unique_ptr<EntryInfo>> entries_;
  std::unique_ptr<google_apis::AboutResource> about_resource_;
  std::unique_ptr<google_apis::StartPageToken> start_page_token_;
  std::vector<std::unique_ptr<google_apis::TeamDriveResource>>
      team_drive_value_;
  std::map<std::string, std::unique_ptr<google_apis::StartPageToken>>
      team_drive_start_page_tokens_;

  std::map<GURL, UploadSession> upload_sessions_;
  int64_t date_seq_;
  int64_t next_upload_sequence_number_;
  int64_t largest_changestamp_;
  int default_max_results_;
  int resource_id_count_;
  int team_drive_list_load_count_;
  int file_list_load_count_;
  int change_list_load_count_;
  int directory_load_count_;
  int about_resource_load_count_;
  int blocked_file_list_load_count_;
  int start_page_token_load_count_;
  bool offline_;
  bool never_return_all_file_list_;
  base::FilePath last_cancelled_file_;
  std::string app_json_template_;

  base::ObserverList<ChangeObserver>::Unchecked change_observers_;

  base::WeakPtrFactory<FakeDriveService> weak_ptr_factory_{this};
};

}  // namespace drive

#endif  // COMPONENTS_DRIVE_SERVICE_FAKE_DRIVE_SERVICE_H_
