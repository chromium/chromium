// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/service/dummy_drive_service.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/functional/bind.h"

using google_apis::AboutResourceCallback;
using google_apis::AuthStatusCallback;
using google_apis::CancelCallbackOnce;
using google_apis::ChangeListCallback;
using google_apis::DownloadActionCallback;
using google_apis::EntryActionCallback;
using google_apis::FileListCallback;
using google_apis::FileResourceCallback;
using google_apis::GetContentCallback;
using google_apis::InitiateUploadCallback;
using google_apis::ProgressCallback;
using google_apis::TeamDriveListCallback;
using google_apis::drive::UploadRangeCallback;

namespace drive {

DummyDriveService::DummyDriveService() = default;

DummyDriveService::~DummyDriveService() = default;

void DummyDriveService::Initialize(const CoreAccountId& account_id) {}

void DummyDriveService::AddObserver(DriveServiceObserver* observer) {}

void DummyDriveService::RemoveObserver(DriveServiceObserver* observer) {}

bool DummyDriveService::CanSendRequest() const { return true; }

bool DummyDriveService::HasAccessToken() const { return true; }

void DummyDriveService::RequestAccessToken(AuthStatusCallback callback) {
  std::move(callback).Run(google_apis::HTTP_NOT_MODIFIED, "fake_access_token");
}

bool DummyDriveService::HasRefreshToken() const { return true; }

void DummyDriveService::ClearAccessToken() { }

void DummyDriveService::ClearRefreshToken() { }

std::string DummyDriveService::GetRootResourceId() const {
  return "dummy_root";
}

CancelCallbackOnce DummyDriveService::GetAllTeamDriveList(
    TeamDriveListCallback callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::GetAllFileList(
    const std::string& team_drive_id,
    FileListCallback callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::GetFileListInDirectory(
    const std::string& directory_resource_id,
    FileListCallback callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::Search(const std::string& search_query,
                                             FileListCallback callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::SearchByTitle(
    const std::string& title,
    const std::string& directory_resource_id,
    FileListCallback callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::GetChangeList(
    int64_t start_changestamp,
    ChangeListCallback callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::GetChangeListByToken(
    const std::string& team_drive_id,
    const std::string& start_page_token,
    ChangeListCallback callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::GetRemainingChangeList(
    const GURL& next_link,
    ChangeListCallback callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::GetRemainingTeamDriveList(
    const std::string& page_token,
    TeamDriveListCallback callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::GetRemainingFileList(
    const GURL& next_link,
    FileListCallback callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::GetFileResource(
    const std::string& resource_id,
    FileResourceCallback callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::GetAboutResource(
    AboutResourceCallback callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::GetStartPageToken(
    const std::string& team_drive_id,
    google_apis::StartPageTokenCallback callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::DeleteResource(
    const std::string& resource_id,
    const std::string& etag,
    EntryActionCallback callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::TrashResource(
    const std::string& resource_id,
    EntryActionCallback callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::DownloadFile(
    const base::FilePath& local_cache_path,
    const std::string& resource_id,
    DownloadActionCallback download_action_callback,
    const GetContentCallback& get_content_callback,
    ProgressCallback progress_callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::CopyResource(
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& new_title,
    const base::Time& last_modified,
    FileResourceCallback callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::UpdateResource(
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& new_title,
    const base::Time& last_modified,
    const base::Time& last_viewed_by_me,
    const google_apis::drive::Properties& properties,
    FileResourceCallback callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::AddResourceToDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    EntryActionCallback callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::RemoveResourceFromDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    EntryActionCallback callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::AddNewDirectory(
    const std::string& parent_resource_id,
    const std::string& directory_title,
    const AddNewDirectoryOptions& options,
    FileResourceCallback callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::InitiateUploadNewFile(
    const std::string& content_type,
    int64_t content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    const UploadNewFileOptions& options,
    InitiateUploadCallback callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::InitiateUploadExistingFile(
    const std::string& content_type,
    int64_t content_length,
    const std::string& resource_id,
    const UploadExistingFileOptions& options,
    InitiateUploadCallback callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::ResumeUpload(
    const GURL& upload_url,
    int64_t start_position,
    int64_t end_position,
    int64_t content_length,
    const std::string& content_type,
    const base::FilePath& local_file_path,
    UploadRangeCallback callback,
    ProgressCallback progress_callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::GetUploadStatus(
    const GURL& upload_url,
    int64_t content_length,
    UploadRangeCallback callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::MultipartUploadNewFile(
    const std::string& content_type,
    std::optional<std::string_view> converted_mime_type,
    int64_t content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    const base::FilePath& local_file_path,
    const UploadNewFileOptions& options,
    FileResourceCallback callback,
    ProgressCallback progress_callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::MultipartUploadExistingFile(
    const std::string& content_type,
    int64_t content_length,
    const std::string& resource_id,
    const base::FilePath& local_file_path,
    const UploadExistingFileOptions& options,
    FileResourceCallback callback,
    ProgressCallback progress_callback) {
  return CancelCallbackOnce();
}

CancelCallbackOnce DummyDriveService::AddPermission(
    const std::string& resource_id,
    const std::string& email,
    google_apis::drive::PermissionRole role,
    EntryActionCallback callback) {
  return CancelCallbackOnce();
}
std::unique_ptr<BatchRequestConfiguratorInterface>
DummyDriveService::StartBatchRequest() {
  return nullptr;
}

}  // namespace drive
