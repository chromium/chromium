// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_SERVICE_DRIVE_SERVICE_INTERFACE_H_
#define COMPONENTS_DRIVE_SERVICE_DRIVE_SERVICE_INTERFACE_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/time/time.h"
#include "google_apis/common/auth_service_interface.h"
#include "google_apis/drive/drive_api_requests.h"
#include "google_apis/drive/drive_base_requests.h"
#include "google_apis/drive/drive_common_callbacks.h"
#include "google_apis/gaia/core_account_id.h"

namespace base {
class Time;
}

namespace drive {

// Observer interface for DriveServiceInterface.
class DriveServiceObserver {
 public:
  // Triggered when the service gets ready to send requests.
  virtual void OnReadyToSendRequests() {}

  // Called when the refresh token was found to be invalid.
  virtual void OnRefreshTokenInvalid() {}

 protected:
  virtual ~DriveServiceObserver() = default;
};

// Optional parameters for AddNewDirectory().
struct AddNewDirectoryOptions {
  AddNewDirectoryOptions();
  AddNewDirectoryOptions(const AddNewDirectoryOptions& other);
  ~AddNewDirectoryOptions();

  // visibility of the new directory.
  google_apis::drive::FileVisibility visibility;

  // modified_date of the directory.
  // Pass the null Time if you are not interested in setting this property.
  base::Time modified_date;

  // last_viewed_by_me_date of the directory.
  // Pass the null Time if you are not interested in setting this property.
  base::Time last_viewed_by_me_date;

  // List of properties for a new directory.
  google_apis::drive::Properties properties;
};

// Optional parameters for InitiateUploadNewFile() and
// MultipartUploadNewFile().
struct UploadNewFileOptions {
  UploadNewFileOptions();
  UploadNewFileOptions(const UploadNewFileOptions& other);
  ~UploadNewFileOptions();

  // modified_date of the file.
  // Pass the null Time if you are not interested in setting this property.
  base::Time modified_date;

  // last_viewed_by_me_date of the file.
  // Pass the null Time if you are not interested in setting this property.
  base::Time last_viewed_by_me_date;

  // List of properties for a new file.
  google_apis::drive::Properties properties;
};

// Optional parameters for InitiateUploadExistingFile() and
// MultipartUploadExistingFile().
struct UploadExistingFileOptions {
  UploadExistingFileOptions();
  UploadExistingFileOptions(const UploadExistingFileOptions& other);
  ~UploadExistingFileOptions();

  // Expected ETag of the file. UPLOAD_ERROR_CONFLICT error is generated when
  // matching fails.
  // Pass the empty string to disable this behavior.
  std::string etag;

  // New parent of the file.
  // Pass the empty string to keep the property unchanged.
  std::string parent_resource_id;

  // New title of the file.
  // Pass the empty string to keep the property unchanged.
  std::string title;

  // New modified_date of the file.
  // Pass the null Time if you are not interested in setting this property.
  base::Time modified_date;

  // New last_viewed_by_me_date of the file.
  // Pass the null Time if you are not interested in setting this property.
  base::Time last_viewed_by_me_date;

  // List of new properties for an existing file (it will be merged with
  // existing properties).
  google_apis::drive::Properties properties;
};

// Interface where we define operations that can be sent in batch requests.
class DriveServiceBatchOperationsInterface {
 public:
  virtual ~DriveServiceBatchOperationsInterface() = default;

  // Uploads a file by a single request with multipart body. It's more efficient
  // for small files than using |InitiateUploadNewFile| and |ResumeUpload|.
  // |content_type| and |content_length| should be the ones of the file to be
  // uploaded.
  // `converted_mime_type` is the desired MIME type of the file after uploading.
  // This should only be set if it is expected that Drive will convert the
  // uploaded file into another format such as Google Docs:
  // https://developers.google.com/drive/api/guides/manage-uploads#import-docs
  // |callback| must not be null. |progress_callback| may be null.
  virtual google_apis::CancelCallbackOnce MultipartUploadNewFile(
      const std::string& content_type,
      std::optional<std::string_view> converted_mime_type,
      int64_t content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const base::FilePath& local_file_path,
      const UploadNewFileOptions& options,
      google_apis::FileResourceCallback callback,
      google_apis::ProgressCallback progress_callback) = 0;

  // Uploads a file by a single request with multipart body. It's more efficient
  // for small files than using |InitiateUploadExistingFile| and |ResumeUpload|.
  // |content_type| and |content_length| should be the ones of the file to be
  // uploaded.  |callback| must not be null. |progress_callback| may be null.
  virtual google_apis::CancelCallbackOnce MultipartUploadExistingFile(
      const std::string& content_type,
      int64_t content_length,
      const std::string& resource_id,
      const base::FilePath& local_file_path,
      const UploadExistingFileOptions& options,
      google_apis::FileResourceCallback callback,
      google_apis::ProgressCallback progress_callback) = 0;
};

// Builder returned by DriveServiceInterface to build batch request.
class BatchRequestConfiguratorInterface
    : public DriveServiceBatchOperationsInterface {
 public:
  ~BatchRequestConfiguratorInterface() override = default;

  // Commits and sends the batch request.
  virtual void Commit() = 0;
};

// This defines an interface for sharing by DriveService and MockDriveService
// so that we can do testing of clients of DriveService.
//
// All functions must be called on UI thread. DriveService is built on top of
// URLFetcher that runs on UI thread.
class DriveServiceInterface : public DriveServiceBatchOperationsInterface {
 public:
  ~DriveServiceInterface() override = default;

  // Common service:

  // Initializes the documents service with |account_id|.
  virtual void Initialize(const CoreAccountId& account_id) = 0;

  // Adds an observer.
  virtual void AddObserver(DriveServiceObserver* observer) = 0;

  // Removes an observer.
  virtual void RemoveObserver(DriveServiceObserver* observer) = 0;

  // True if ready to send requests.
  virtual bool CanSendRequest() const = 0;

  // Authentication service:

  // True if OAuth2 access token is retrieved and believed to be fresh.
  virtual bool HasAccessToken() const = 0;

  // Gets the cached OAuth2 access token or if empty, then fetches a new one.
  virtual void RequestAccessToken(google_apis::AuthStatusCallback callback) = 0;

  // True if OAuth2 refresh token is present.
  virtual bool HasRefreshToken() const = 0;

  // Clears OAuth2 access token.
  virtual void ClearAccessToken() = 0;

  // Clears OAuth2 refresh token.
  virtual void ClearRefreshToken() = 0;

  // Document access:

  // Returns the resource id for the root directory.
  virtual std::string GetRootResourceId() const = 0;

  // Fetches a Team Drive list of the account. |callback| will be called upon
  // completion.
  // If the list is too long, it may be paged. In such a case, a URL to fetch
  // remaining results will be included in the returned result. See also
  // GetRemainingDriveList.
  //
  // |callback| must not be null.
  virtual google_apis::CancelCallbackOnce GetAllTeamDriveList(
      google_apis::TeamDriveListCallback callback) = 0;

  // Fetches a file list of the account. |callback| will be called upon
  // completion.
  // If the list is too long, it may be paged. In such a case, a URL to fetch
  // remaining results will be included in the returned result. See also
  // GetRemainingFileList.
  //
  // If |team_drive_id| is empty will retrieve the file list for the users
  // default corpus, otherwise will fetch the file list for the specified
  // team drive.
  // |callback| must not be null.
  virtual google_apis::CancelCallbackOnce GetAllFileList(
      const std::string& team_drive_id,
      google_apis::FileListCallback callback) = 0;

  // Fetches a file list in the directory with |directory_resource_id|.
  // |callback| will be called upon completion.
  // If the list is too long, it may be paged. In such a case, a URL to fetch
  // remaining results will be included in the returned result. See also
  // GetRemainingFileList.
  //
  // |directory_resource_id| must not be empty.
  // |callback| must not be null.
  virtual google_apis::CancelCallbackOnce GetFileListInDirectory(
      const std::string& directory_resource_id,
      google_apis::FileListCallback callback) = 0;

  // Searches the resources for the |search_query| from all the user's
  // resources. |callback| will be called upon completion.
  // If the list is too long, it may be paged. In such a case, a URL to fetch
  // remaining results will be included in the returned result. See also
  // GetRemainingFileList.
  //
  // |search_query| must not be empty.
  // |callback| must not be null.
  virtual google_apis::CancelCallbackOnce Search(
      const std::string& search_query,
      google_apis::FileListCallback callback) = 0;

  // Searches the resources with the |title|.
  // |directory_resource_id| is an optional parameter. If it is empty,
  // the search target is all the existing resources. Otherwise, it is
  // the resources directly under the directory with |directory_resource_id|.
  // If the list is too long, it may be paged. In such a case, a URL to fetch
  // remaining results will be included in the returned result. See also
  // GetRemainingFileList.
  //
  // |title| must not be empty, and |callback| must not be null.
  virtual google_apis::CancelCallbackOnce SearchByTitle(
      const std::string& title,
      const std::string& directory_resource_id,
      google_apis::FileListCallback callback) = 0;

  // Fetches change list since |start_changestamp|. |callback| will be
  // called upon completion.
  // If the list is too long, it may be paged. In such a case, a URL to fetch
  // remaining results will be included in the returned result. See also
  // GetRemainingChangeList.
  //
  // |callback| must not be null.
  virtual google_apis::CancelCallbackOnce GetChangeList(
      int64_t start_changestamp,
      google_apis::ChangeListCallback callback) = 0;

  // Fetches change list since |start_page_token|. |callback| will be
  // called upon completion.
  // If |team_drive_id| is empty, then it will retrieve the change list for
  // the users changelog.
  // If the list is too long, it may be paged. In such a case, a URL to fetch
  // remaining results will be included in the returned result. See also
  // GetRemainingChangeList.
  //
  // |callback| must not be null.
  virtual google_apis::CancelCallbackOnce GetChangeListByToken(
      const std::string& team_drive_id,
      const std::string& start_page_token,
      google_apis::ChangeListCallback callback) = 0;

  // The result of GetChangeList() may be paged.
  // In such a case, a next link to fetch remaining result is returned.
  // The page token can be used for this method. |callback| will be called upon
  // completion.
  //
  // |next_link| must not be empty. |callback| must not be null.
  virtual google_apis::CancelCallbackOnce GetRemainingChangeList(
      const GURL& next_link,
      google_apis::ChangeListCallback callback) = 0;

  // The result of GetAllTeamDrives() may be paged. In such a case, a token to
  // fetch remaining result is returned. The page token can be used for this
  // method. |callback| will be called upon completion.
  //
  // |next_link| must not be empty. |callback| must not be null.
  virtual google_apis::CancelCallbackOnce GetRemainingTeamDriveList(
      const std::string& page_token,
      google_apis::TeamDriveListCallback callback) = 0;

  // The result of GetAllFileList(), GetFileListInDirectory(), Search()
  // and SearchByTitle() may be paged. In such a case, a next link to fetch
  // remaining result is returned. The page token can be used for this method.
  // |callback| will be called upon completion.
  //
  // |next_link| must not be empty. |callback| must not be null.
  virtual google_apis::CancelCallbackOnce GetRemainingFileList(
      const GURL& next_link,
      google_apis::FileListCallback callback) = 0;

  // Fetches single entry metadata from server. The entry's file id equals
  // |resource_id|.
  // Upon completion, invokes |callback| with results on the calling thread.
  // |callback| must not be null.
  virtual google_apis::CancelCallbackOnce GetFileResource(
      const std::string& resource_id,
      google_apis::FileResourceCallback callback) = 0;

  // Gets the about resource information from the server.
  // Upon completion, invokes |callback| with results on the calling thread.
  // |callback| must not be null.
  virtual google_apis::CancelCallbackOnce GetAboutResource(
      google_apis::AboutResourceCallback callback) = 0;

  // Gets the start page token information from the server.
  // If |team_drive_id| is empty, then it will retrieve the start page token for
  // the users changelog.
  // Upon completion, invokes |callback| with results on the calling thread.
  // |callback| must not be null.
  virtual google_apis::CancelCallbackOnce GetStartPageToken(
      const std::string& team_drive_id,
      google_apis::StartPageTokenCallback callback) = 0;

  // Permanently deletes a resource identified by its |resource_id|.
  // If |etag| is not empty and did not match, the deletion fails with
  // HTTP_PRECONDITION error.
  // Upon completion, invokes |callback| with results on the calling thread.
  // |callback| must not be null.
  virtual google_apis::CancelCallbackOnce DeleteResource(
      const std::string& resource_id,
      const std::string& etag,
      google_apis::EntryActionCallback callback) = 0;

  // Trashes a resource identified by its |resource_id|.
  // Upon completion, invokes |callback| with results on the calling thread.
  // |callback| must not be null.
  virtual google_apis::CancelCallbackOnce TrashResource(
      const std::string& resource_id,
      google_apis::EntryActionCallback callback) = 0;

  // Makes a copy of a resource with |resource_id|.
  // The new resource will be put under a directory with |parent_resource_id|,
  // and it'll be named |new_title|.
  // If |last_modified| is not null, the modified date of the resource on the
  // server will be set to the date.
  // Upon completion, invokes |callback| with results on the calling thread.
  // |callback| must not be null.
  virtual google_apis::CancelCallbackOnce CopyResource(
      const std::string& resource_id,
      const std::string& parent_resource_id,
      const std::string& new_title,
      const base::Time& last_modified,
      google_apis::FileResourceCallback callback) = 0;

  // Updates a resource with |resource_id| to the directory of
  // |parent_resource_id| with renaming to |new_title|.
  // If |last_modified| or |last_accessed| is not null, the modified/accessed
  // date of the resource on the server will be set to the date.
  // If |properties| are specified, then they will be set on |resource_id|.
  // Upon completion, invokes |callback| with results on the calling thread.
  // |callback| must not be null.
  virtual google_apis::CancelCallbackOnce UpdateResource(
      const std::string& resource_id,
      const std::string& parent_resource_id,
      const std::string& new_title,
      const base::Time& last_modified,
      const base::Time& last_viewed_by_me,
      const google_apis::drive::Properties& properties,
      google_apis::FileResourceCallback callback) = 0;

  // Adds a resource (document, file, or collection) identified by its
  // |resource_id| to a collection represented by the |parent_resource_id|.
  // Upon completion, invokes |callback| with results on the calling thread.
  // |callback| must not be null.
  virtual google_apis::CancelCallbackOnce AddResourceToDirectory(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      google_apis::EntryActionCallback callback) = 0;

  // Removes a resource (document, file, collection) identified by its
  // |resource_id| from a collection represented by the |parent_resource_id|.
  // Upon completion, invokes |callback| with results on the calling thread.
  // |callback| must not be null.
  virtual google_apis::CancelCallbackOnce RemoveResourceFromDirectory(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      google_apis::EntryActionCallback callback) = 0;

  // Adds new collection with |directory_title| under parent directory
  // identified with |parent_resource_id|. |parent_resource_id| can be the
  // value returned by GetRootResourceId to represent the root directory.
  // Upon completion, invokes |callback| and passes newly created entry on
  // the calling thread.
  // This function cannot be named as "CreateDirectory" as it conflicts with
  // a macro on Windows.
  // |callback| must not be null.
  virtual google_apis::CancelCallbackOnce AddNewDirectory(
      const std::string& parent_resource_id,
      const std::string& directory_title,
      const AddNewDirectoryOptions& options,
      google_apis::FileResourceCallback callback) = 0;

  // Downloads a file with |resourced_id|. The downloaded file will
  // be stored at |local_cache_path| location. Upon completion, invokes
  // |download_action_callback| with results on the calling thread.
  // If |get_content_callback| is not empty,
  // URLFetcherDelegate::OnURLFetchDownloadData will be called, which will in
  // turn invoke |get_content_callback| on the calling thread.
  // If |progress_callback| is not empty, it is invoked periodically when
  // the download made some progress.
  //
  // |download_action_callback| must not be null.
  // |get_content_callback| and |progress_callback| may be null.
  virtual google_apis::CancelCallbackOnce DownloadFile(
      const base::FilePath& local_cache_path,
      const std::string& resource_id,
      google_apis::DownloadActionCallback download_action_callback,
      const google_apis::GetContentCallback& get_content_callback,
      google_apis::ProgressCallback progress_callback) = 0;

  // Initiates uploading of a new document/file.
  // |content_type| and |content_length| should be the ones of the file to be
  // uploaded.
  // |callback| must not be null.
  virtual google_apis::CancelCallbackOnce InitiateUploadNewFile(
      const std::string& content_type,
      int64_t content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const UploadNewFileOptions& options,
      google_apis::InitiateUploadCallback callback) = 0;

  // Initiates uploading of an existing document/file.
  // |content_type| and |content_length| should be the ones of the file to be
  // uploaded.
  // |callback| must not be null.
  virtual google_apis::CancelCallbackOnce InitiateUploadExistingFile(
      const std::string& content_type,
      int64_t content_length,
      const std::string& resource_id,
      const UploadExistingFileOptions& options,
      google_apis::InitiateUploadCallback callback) = 0;

  // Resumes uploading of a document/file on the calling thread.
  // |callback| must not be null. |progress_callback| may be null.
  virtual google_apis::CancelCallbackOnce ResumeUpload(
      const GURL& upload_url,
      int64_t start_position,
      int64_t end_position,
      int64_t content_length,
      const std::string& content_type,
      const base::FilePath& local_file_path,
      google_apis::drive::UploadRangeCallback callback,
      google_apis::ProgressCallback progress_callback) = 0;

  // Gets the current status of the uploading to |upload_url| from the server.
  // |drive_file_path| and |content_length| should be set to the same value
  // which is used for ResumeUpload.
  // |callback| must not be null.
  virtual google_apis::CancelCallbackOnce GetUploadStatus(
      const GURL& upload_url,
      int64_t content_length,
      google_apis::drive::UploadRangeCallback callback) = 0;

  // Authorizes the account |email| to access |resource_id| as a |role|.
  // |callback| must not be null.
  virtual google_apis::CancelCallbackOnce AddPermission(
      const std::string& resource_id,
      const std::string& email,
      google_apis::drive::PermissionRole role,
      google_apis::EntryActionCallback callback) = 0;

  // Starts batch request and returns |BatchRequestConfigurator|.
  virtual std::unique_ptr<BatchRequestConfiguratorInterface>
  StartBatchRequest() = 0;
};

}  // namespace drive

#endif  // COMPONENTS_DRIVE_SERVICE_DRIVE_SERVICE_INTERFACE_H_
