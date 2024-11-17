// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_SERVICE_DRIVE_API_SERVICE_H_
#define COMPONENTS_DRIVE_SERVICE_DRIVE_API_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/threading/thread_checker.h"
#include "components/drive/service/drive_service_interface.h"
#include "google_apis/common/auth_service_interface.h"
#include "google_apis/common/auth_service_observer.h"
#include "google_apis/drive/drive_api_url_generator.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

class GURL;

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace google_apis {
class FilesListRequestRunner;
class RequestSender;
namespace drive {
class BatchUploadRequest;
}  // namespace drive
}  // namespace google_apis

namespace signin {
class IdentityManager;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace drive {

// Builder for batch request returned by |DriveAPIService|.
class BatchRequestConfigurator : public BatchRequestConfiguratorInterface {
 public:
  BatchRequestConfigurator(
      const base::WeakPtr<google_apis::drive::BatchUploadRequest>&
          batch_request,
      base::SequencedTaskRunner* task_runner,
      const google_apis::DriveApiUrlGenerator& url_generator,
      const google_apis::CancelCallbackRepeating& cancel_callback);

  BatchRequestConfigurator(const BatchRequestConfigurator&) = delete;
  BatchRequestConfigurator& operator=(const BatchRequestConfigurator&) = delete;

  ~BatchRequestConfigurator() override;

  // BatchRequestConfiguratorInterface overrides.
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
  void Commit() override;

 private:
  // Reference to batch request. It turns to null after committing.
  base::WeakPtr<google_apis::drive::BatchUploadRequest> batch_request_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  google_apis::DriveApiUrlGenerator url_generator_;
  google_apis::CancelCallbackRepeating cancel_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// This class provides Drive request calls using Drive V2 API.
// Details of API call are abstracted in each request class and this class
// works as a thin wrapper for the API.
class DriveAPIService : public DriveServiceInterface,
                        public google_apis::AuthServiceObserver {
 public:
  // |identity_manager| is used for interacting with the identity system.
  // |url_request_context_getter| is used to initialize URLFetcher.
  // |url_loader_factory| is used to create SimpleURLLoaders used to create
  // OAuth tokens.
  // |blocking_task_runner| is used to run blocking tasks (like parsing JSON).
  // |base_url| is used to generate URLs for communication with the drive API.
  // |base_thumbnail_url| is used to generate URLs for downloading thumbnail
  // from image server.
  // |custom_user_agent| will be used for the User-Agent header in HTTP
  // requests issues through the service if the value is not empty.
  // |traffic_annotation| will be used to annotate the network request that will
  // be created to perform this service.
  DriveAPIService(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::SequencedTaskRunner* blocking_task_runner,
      const GURL& base_url,
      const GURL& base_thumbnail_url,
      const std::string& custom_user_agent,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

  DriveAPIService(const DriveAPIService&) = delete;
  DriveAPIService& operator=(const DriveAPIService&) = delete;

  ~DriveAPIService() override;

  // DriveServiceInterface Overrides
  void Initialize(const CoreAccountId& account_id) override;
  void AddObserver(DriveServiceObserver* observer) override;
  void RemoveObserver(DriveServiceObserver* observer) override;
  bool CanSendRequest() const override;
  bool HasAccessToken() const override;
  void RequestAccessToken(google_apis::AuthStatusCallback callback) override;
  bool HasRefreshToken() const override;
  void ClearAccessToken() override;
  void ClearRefreshToken() override;
  std::string GetRootResourceId() const override;
  google_apis::CancelCallbackOnce GetAllTeamDriveList(
      google_apis::TeamDriveListCallback callback) override;
  google_apis::CancelCallbackOnce GetAllFileList(
      const std::string& team_drive_id,
      google_apis::FileListCallback callback) override;
  google_apis::CancelCallbackOnce GetFileListInDirectory(
      const std::string& directory_resource_id,
      google_apis::FileListCallback callback) override;
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
  google_apis::CancelCallbackOnce GetRemainingTeamDriveList(
      const std::string& page_token,
      google_apis::TeamDriveListCallback callback) override;
  google_apis::CancelCallbackOnce GetRemainingChangeList(
      const GURL& next_link,
      google_apis::ChangeListCallback callback) override;
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
      const drive::UploadNewFileOptions& options,
      google_apis::FileResourceCallback callback,
      google_apis::ProgressCallback progress_callback) override;
  google_apis::CancelCallbackOnce MultipartUploadExistingFile(
      const std::string& content_type,
      int64_t content_length,
      const std::string& resource_id,
      const base::FilePath& local_file_path,
      const drive::UploadExistingFileOptions& options,
      google_apis::FileResourceCallback callback,
      google_apis::ProgressCallback progress_callback) override;
  google_apis::CancelCallbackOnce AddPermission(
      const std::string& resource_id,
      const std::string& email,
      google_apis::drive::PermissionRole role,
      google_apis::EntryActionCallback callback) override;
  std::unique_ptr<BatchRequestConfiguratorInterface> StartBatchRequest()
      override;

 private:
  // AuthServiceObserver override.
  void OnOAuth2RefreshTokenChanged() override;

  // The class is expected to run on UI thread.
  base::ThreadChecker thread_checker_;

  raw_ptr<signin::IdentityManager> identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  std::unique_ptr<google_apis::RequestSender> sender_;
  std::unique_ptr<google_apis::FilesListRequestRunner>
      files_list_request_runner_;
  base::ObserverList<DriveServiceObserver>::Unchecked observers_;
  google_apis::DriveApiUrlGenerator url_generator_;
  const std::string custom_user_agent_;
  const net::NetworkTrafficAnnotationTag traffic_annotation_;
};

}  // namespace drive

#endif  // COMPONENTS_DRIVE_SERVICE_DRIVE_API_SERVICE_H_
