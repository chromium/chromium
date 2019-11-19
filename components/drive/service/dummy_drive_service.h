// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_SERVICE_DUMMY_DRIVE_SERVICE_H_
#define COMPONENTS_DRIVE_SERVICE_DUMMY_DRIVE_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "components/drive/service/drive_service_interface.h"
#include "google_apis/drive/auth_service_interface.h"

namespace drive {

// Dummy implementation of DriveServiceInterface.
// All functions do nothing, or return place holder values like 'true'.
class DummyDriveService : public DriveServiceInterface {
 public:
  DummyDriveService();
  ~DummyDriveService() override;

  // DriveServiceInterface Overrides
  void Initialize(const CoreAccountId& account_id) override;
  void AddObserver(DriveServiceObserver* observer) override;
  void RemoveObserver(DriveServiceObserver* observer) override;
  bool CanSendRequest() const override;
  bool HasAccessToken() const override;
  void RequestAccessToken(
      const google_apis::AuthStatusCallback& callback) override;
  bool HasRefreshToken() const override;
  void ClearAccessToken() override;
  void ClearRefreshToken() override;
  std::string GetRootResourceId() const override;
  google_apis::CancelCallback GetAllTeamDriveList(
      const google_apis::TeamDriveListCallback& callback) override;
  google_apis::CancelCallback GetAllFileList(
      const std::string& team_drive_id,
      const google_apis::FileListCallback& callback) override;
  google_apis::CancelCallback GetFileListInDirectory(
      const std::string& directory_resource_id,
      const google_apis::FileListCallback& callback) override;
  google_apis::CancelCallback Search(
      const std::string& search_query,
      const google_apis::FileListCallback& callback) override;
  google_apis::CancelCallback SearchByTitle(
      const std::string& title,
      const std::string& directory_resource_id,
      const google_apis::FileListCallback& callback) override;
  google_apis::CancelCallback GetChangeList(
      int64_t start_changestamp,
      const google_apis::ChangeListCallback& callback) override;
  google_apis::CancelCallback GetChangeListByToken(
      const std::string& team_drive_id,
      const std::string& start_page_token,
      const google_apis::ChangeListCallback& callback) override;
  google_apis::CancelCallback GetRemainingChangeList(
      const GURL& next_link,
      const google_apis::ChangeListCallback& callback) override;
  google_apis::CancelCallback GetRemainingTeamDriveList(
      const std::string& page_token,
      const google_apis::TeamDriveListCallback& callback) override;
  google_apis::CancelCallback GetRemainingFileList(
      const GURL& next_link,
      const google_apis::FileListCallback& callback) override;
  google_apis::CancelCallback GetFileResource(
      const std::string& resource_id,
      const google_apis::FileResourceCallback& callback) override;
  google_apis::CancelCallback GetAboutResource(
      const google_apis::AboutResourceCallback& callback) override;
  google_apis::CancelCallback GetStartPageToken(
      const std::string& team_drive_id,
      const google_apis::StartPageTokenCallback& callback) override;
  google_apis::CancelCallback DeleteResource(
      const std::string& resource_id,
      const std::string& etag,
      const google_apis::EntryActionCallback& callback) override;
  google_apis::CancelCallback TrashResource(
      const std::string& resource_id,
      const google_apis::EntryActionCallback& callback) override;
  google_apis::CancelCallback DownloadFile(
      const base::FilePath& local_cache_path,
      const std::string& resource_id,
      const google_apis::DownloadActionCallback& download_action_callback,
      const google_apis::GetContentCallback& get_content_callback,
      const google_apis::ProgressCallback& progress_callback) override;
  google_apis::CancelCallback CopyResource(
      const std::string& resource_id,
      const std::string& parent_resource_id,
      const std::string& new_title,
      const base::Time& last_modified,
      const google_apis::FileResourceCallback& callback) override;
  google_apis::CancelCallback UpdateResource(
      const std::string& resource_id,
      const std::string& parent_resource_id,
      const std::string& new_title,
      const base::Time& last_modified,
      const base::Time& last_viewed_by_me,
      const google_apis::drive::Properties& properties,
      const google_apis::FileResourceCallback& callback) override;
  google_apis::CancelCallback AddResourceToDirectory(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      const google_apis::EntryActionCallback& callback) override;
  google_apis::CancelCallback RemoveResourceFromDirectory(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      const google_apis::EntryActionCallback& callback) override;
  google_apis::CancelCallback AddNewDirectory(
      const std::string& parent_resource_id,
      const std::string& directory_title,
      const AddNewDirectoryOptions& options,
      const google_apis::FileResourceCallback& callback) override;
  google_apis::CancelCallback InitiateUploadNewFile(
      const std::string& content_type,
      int64_t content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const UploadNewFileOptions& options,
      const google_apis::InitiateUploadCallback& callback) override;
  google_apis::CancelCallback InitiateUploadExistingFile(
      const std::string& content_type,
      int64_t content_length,
      const std::string& resource_id,
      const UploadExistingFileOptions& options,
      const google_apis::InitiateUploadCallback& callback) override;
  google_apis::CancelCallback ResumeUpload(
      const GURL& upload_url,
      int64_t start_position,
      int64_t end_position,
      int64_t content_length,
      const std::string& content_type,
      const base::FilePath& local_file_path,
      const google_apis::drive::UploadRangeCallback& callback,
      const google_apis::ProgressCallback& progress_callback) override;
  google_apis::CancelCallback GetUploadStatus(
      const GURL& upload_url,
      int64_t content_length,
      const google_apis::drive::UploadRangeCallback& callback) override;
  google_apis::CancelCallback MultipartUploadNewFile(
      const std::string& content_type,
      int64_t content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const base::FilePath& local_file_path,
      const UploadNewFileOptions& options,
      const google_apis::FileResourceCallback& callback,
      const google_apis::ProgressCallback& progress_callback) override;
  google_apis::CancelCallback MultipartUploadExistingFile(
      const std::string& content_type,
      int64_t content_length,
      const std::string& resource_id,
      const base::FilePath& local_file_path,
      const UploadExistingFileOptions& options,
      const google_apis::FileResourceCallback& callback,
      const google_apis::ProgressCallback& progress_callback) override;
  google_apis::CancelCallback AddPermission(
      const std::string& resource_id,
      const std::string& email,
      google_apis::drive::PermissionRole role,
      const google_apis::EntryActionCallback& callback) override;
  std::unique_ptr<BatchRequestConfiguratorInterface> StartBatchRequest()
      override;
};

}  // namespace drive

#endif  // COMPONENTS_DRIVE_SERVICE_DUMMY_DRIVE_SERVICE_H_
