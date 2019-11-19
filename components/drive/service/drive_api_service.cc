// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/service/drive_api_service.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "components/drive/drive_api_util.h"
#include "google_apis/drive/auth_service.h"
#include "google_apis/drive/base_requests.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/drive_api_requests.h"
#include "google_apis/drive/files_list_request_runner.h"
#include "google_apis/drive/request_sender.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using google_apis::AboutResourceCallback;
using google_apis::AuthStatusCallback;
using google_apis::CancelCallback;
using google_apis::ChangeList;
using google_apis::ChangeListCallback;
using google_apis::DRIVE_OTHER_ERROR;
using google_apis::DRIVE_PARSE_ERROR;
using google_apis::DownloadActionCallback;
using google_apis::DriveApiErrorCode;
using google_apis::EntryActionCallback;
using google_apis::FileList;
using google_apis::FileListCallback;
using google_apis::FileResource;
using google_apis::FileResourceCallback;
using google_apis::FilesListCorpora;
using google_apis::FilesListRequestRunner;
using google_apis::GetContentCallback;
using google_apis::HTTP_NOT_IMPLEMENTED;
using google_apis::HTTP_SUCCESS;
using google_apis::InitiateUploadCallback;
using google_apis::ProgressCallback;
using google_apis::RequestSender;
using google_apis::StartPageTokenCallback;
using google_apis::TeamDriveListCallback;
using google_apis::UploadRangeResponse;
using google_apis::drive::AboutGetRequest;
using google_apis::drive::ChangesListNextPageRequest;
using google_apis::drive::ChangesListRequest;
using google_apis::drive::ChildrenDeleteRequest;
using google_apis::drive::ChildrenInsertRequest;
using google_apis::drive::DownloadFileRequest;
using google_apis::drive::FilesCopyRequest;
using google_apis::drive::FilesDeleteRequest;
using google_apis::drive::FilesGetRequest;
using google_apis::drive::FilesInsertRequest;
using google_apis::drive::FilesListNextPageRequest;
using google_apis::drive::FilesListRequest;
using google_apis::drive::FilesPatchRequest;
using google_apis::drive::FilesTrashRequest;
using google_apis::drive::GetUploadStatusRequest;
using google_apis::drive::InitiateUploadExistingFileRequest;
using google_apis::drive::InitiateUploadNewFileRequest;
using google_apis::drive::ResumeUploadRequest;
using google_apis::drive::StartPageTokenRequest;
using google_apis::drive::TeamDriveListRequest;
using google_apis::drive::UploadRangeCallback;

namespace drive {

namespace {

// OAuth2 scopes for Drive API.
const char kDriveScope[] = "https://www.googleapis.com/auth/drive";
const char kDriveAppsReadonlyScope[] =
    "https://www.googleapis.com/auth/drive.apps.readonly";
const char kDriveAppsScope[] = "https://www.googleapis.com/auth/drive.apps";

// Mime type to create a directory.
const char kFolderMimeType[] = "application/vnd.google-apps.folder";

// Max number of Team Drive entries to be fetched in a single http request.
const int kMaxNumTeamDriveResourcePerRequest = 100;

// Max number of file entries to be fetched in a single http request.
//
// The larger the number is,
// - The total running time to fetch the whole file list will become shorter.
// - The running time for a single request tends to become longer.
// Since the file list fetching is a completely background task, for our side,
// only the total time matters. However, the server seems to have a time limit
// per single request, which disables us to set the largest value (1000).
// TODO(kinaba): make it larger when the server gets faster.
const int kMaxNumFilesResourcePerRequest = 300;
const int kMaxNumFilesResourcePerRequestForSearch = 100;

// For performance, we declare all fields we use.
const char kAboutResourceFields[] =
    "kind,quotaBytesTotal,quotaBytesUsedAggregate,largestChangeId,rootFolderId";
const char kFileResourceFields[] =
    "kind,id,title,createdDate,sharedWithMeDate,mimeType,"
    "md5Checksum,fileSize,labels/trashed,labels/starred,"
    "imageMediaMetadata/width,"
    "imageMediaMetadata/height,imageMediaMetadata/rotation,etag,"
    "parents(id,parentLink),alternateLink,"
    "modifiedDate,lastViewedByMeDate,shared,modifiedByMeDate";
const char kFileListFields[] =
    "kind,items(kind,id,title,createdDate,sharedWithMeDate,"
    "mimeType,md5Checksum,fileSize,labels/trashed,labels/starred,"
    "imageMediaMetadata/width,"
    "imageMediaMetadata/height,imageMediaMetadata/rotation,etag,"
    "parents(id,parentLink),alternateLink,"
    "modifiedDate,lastViewedByMeDate,shared,modifiedByMeDate,capabilities),"
    "nextLink";
const char kChangeListFields[] =
    "kind,items(type,file(kind,id,title,createdDate,sharedWithMeDate,"
    "mimeType,md5Checksum,fileSize,labels/trashed,labels/starred,"
    "imageMediaMetadata/width,"
    "imageMediaMetadata/height,imageMediaMetadata/rotation,etag,"
    "parents(id,parentLink),alternateLink,modifiedDate,"
    "lastViewedByMeDate,shared,modifiedByMeDate,capabilities),"
    "teamDrive(kind,id,name,capabilities),teamDriveId,"
    "deleted,id,fileId,modificationDate),nextLink,"
    "largestChangeId,newStartPageToken";
const char kTeamDrivesListFields[] =
    "nextPageToken,kind,items(kind,id,name,capabilities)";

// Ignores the |entry|, and runs the |callback|.
void EntryActionCallbackAdapter(const EntryActionCallback& callback,
                                DriveApiErrorCode error,
                                std::unique_ptr<FileResource> entry) {
  callback.Run(error);
}

// The resource ID for the root directory for Drive API is defined in the spec:
// https://developers.google.com/drive/folder
const char kDriveApiRootDirectoryResourceId[] = "root";

}  // namespace

BatchRequestConfigurator::BatchRequestConfigurator(
    const base::WeakPtr<google_apis::drive::BatchUploadRequest>& batch_request,
    base::SequencedTaskRunner* task_runner,
    const google_apis::DriveApiUrlGenerator& url_generator,
    const google_apis::CancelCallback& cancel_callback)
    : batch_request_(batch_request),
      task_runner_(task_runner),
      url_generator_(url_generator),
      cancel_callback_(cancel_callback) {
}

BatchRequestConfigurator::~BatchRequestConfigurator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The batch requst has not been committed.
  if (batch_request_)
    cancel_callback_.Run();
}

google_apis::CancelCallback BatchRequestConfigurator::MultipartUploadNewFile(
    const std::string& content_type,
    int64_t content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    const base::FilePath& local_file_path,
    const UploadNewFileOptions& options,
    const google_apis::FileResourceCallback& callback,
    const google_apis::ProgressCallback& progress_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());

  std::unique_ptr<google_apis::BatchableDelegate> delegate(
      new google_apis::drive::MultipartUploadNewFileDelegate(
          task_runner_.get(), title, parent_resource_id, content_type,
          content_length, options.modified_date, options.last_viewed_by_me_date,
          local_file_path, options.properties, url_generator_, callback,
          progress_callback));
  // Batch request can be null when pre-authorization for the requst is failed
  // in request sender.
  if (batch_request_)
    batch_request_->AddRequest(delegate.release());
  else
    delegate->NotifyError(DRIVE_OTHER_ERROR);
  return cancel_callback_;
}

google_apis::CancelCallback
BatchRequestConfigurator::MultipartUploadExistingFile(
    const std::string& content_type,
    int64_t content_length,
    const std::string& resource_id,
    const base::FilePath& local_file_path,
    const UploadExistingFileOptions& options,
    const google_apis::FileResourceCallback& callback,
    const google_apis::ProgressCallback& progress_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());

  std::unique_ptr<google_apis::BatchableDelegate> delegate(
      new google_apis::drive::MultipartUploadExistingFileDelegate(
          task_runner_.get(), options.title, resource_id,
          options.parent_resource_id, content_type, content_length,
          options.modified_date, options.last_viewed_by_me_date,
          local_file_path, options.etag, options.properties, url_generator_,
          callback, progress_callback));
  // Batch request can be null when pre-authorization for the requst is failed
  // in request sender.
  if (batch_request_)
    batch_request_->AddRequest(delegate.release());
  else
    delegate->NotifyError(DRIVE_OTHER_ERROR);
  return cancel_callback_;
}

void BatchRequestConfigurator::Commit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!batch_request_)
    return;
  batch_request_->Commit();
  batch_request_.reset();
}

DriveAPIService::DriveAPIService(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::SequencedTaskRunner* blocking_task_runner,
    const GURL& base_url,
    const GURL& base_thumbnail_url,
    const std::string& custom_user_agent,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory),
      blocking_task_runner_(blocking_task_runner),
      url_generator_(base_url, base_thumbnail_url),
      custom_user_agent_(custom_user_agent),
      traffic_annotation_(traffic_annotation) {}

DriveAPIService::~DriveAPIService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (sender_)
    sender_->auth_service()->RemoveObserver(this);
}

void DriveAPIService::Initialize(const CoreAccountId& account_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::vector<std::string> scopes;
  scopes.push_back(kDriveScope);
  scopes.push_back(kDriveAppsReadonlyScope);
  scopes.push_back(kDriveAppsScope);

  sender_ = std::make_unique<RequestSender>(
      std::make_unique<google_apis::AuthService>(identity_manager_, account_id,
                                                 url_loader_factory_, scopes),
      url_loader_factory_, blocking_task_runner_.get(), custom_user_agent_,
      traffic_annotation_);
  sender_->auth_service()->AddObserver(this);

  files_list_request_runner_ =
      std::make_unique<FilesListRequestRunner>(sender_.get(), url_generator_);
}

void DriveAPIService::AddObserver(DriveServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void DriveAPIService::RemoveObserver(DriveServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool DriveAPIService::CanSendRequest() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  return HasRefreshToken();
}

std::string DriveAPIService::GetRootResourceId() const {
  return kDriveApiRootDirectoryResourceId;
}

CancelCallback DriveAPIService::GetAllTeamDriveList(
    const TeamDriveListCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  std::unique_ptr<TeamDriveListRequest> request =
      std::make_unique<TeamDriveListRequest>(sender_.get(), url_generator_,
                                             callback);
  request->set_max_results(kMaxNumTeamDriveResourcePerRequest);
  request->set_fields(kTeamDrivesListFields);
  return sender_->StartRequestWithAuthRetry(std::move(request));
}

CancelCallback DriveAPIService::GetAllFileList(
    const std::string& team_drive_id,
    const FileListCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  std::unique_ptr<FilesListRequest> request =
      std::make_unique<FilesListRequest>(sender_.get(), url_generator_,
                                         callback);
  request->set_max_results(kMaxNumFilesResourcePerRequest);
  request->set_q("trashed = false");  // Exclude trashed files.
  request->set_fields(kFileListFields);
  if (team_drive_id.empty()) {
    request->set_corpora(google_apis::FilesListCorpora::DEFAULT);
  } else {
    request->set_team_drive_id(team_drive_id);
    request->set_corpora(google_apis::FilesListCorpora::TEAM_DRIVE);
  }
  return sender_->StartRequestWithAuthRetry(std::move(request));
}

CancelCallback DriveAPIService::GetFileListInDirectory(
    const std::string& directory_resource_id,
    const FileListCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!directory_resource_id.empty());
  DCHECK(!callback.is_null());

  // TODO(yamaguchi): Use FileListScope::CreateForTeamDrive instead of
  // kAllTeamDrives for efficiency. It'll require to add a new parameter to tell
  // which team drive the directory resource belongs to.
  FilesListCorpora corpora = google_apis::FilesListCorpora::ALL_TEAM_DRIVES;

  // Because children.list method on Drive API v2 returns only the list of
  // children's references, but we need all file resource list.
  // So, here we use files.list method instead, with setting parents query.
  // After the migration from GData WAPI to Drive API v2, we should clean the
  // code up by moving the responsibility to include "parents" in the query
  // to client side.
  // We aren't interested in files in trash in this context, neither.
  return files_list_request_runner_->CreateAndStartWithSizeBackoff(
      kMaxNumFilesResourcePerRequest, corpora, std::string(),
      base::StringPrintf(
          "'%s' in parents and trashed = false",
          util::EscapeQueryStringValue(directory_resource_id).c_str()),
      kFileListFields, callback);
}

CancelCallback DriveAPIService::Search(
    const std::string& search_query,
    const FileListCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!search_query.empty());
  DCHECK(!callback.is_null());

  FilesListCorpora corpora = google_apis::FilesListCorpora::ALL_TEAM_DRIVES;

  std::string query = util::TranslateQuery(search_query);
  if (!query.empty())
    query += " and ";
  query += "trashed = false";

  return files_list_request_runner_->CreateAndStartWithSizeBackoff(
      kMaxNumFilesResourcePerRequestForSearch, corpora, std::string(), query,
      kFileListFields, callback);
}

CancelCallback DriveAPIService::SearchByTitle(
    const std::string& title,
    const std::string& directory_resource_id,
    const FileListCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!title.empty());
  DCHECK(!callback.is_null());

  std::string query;
  base::StringAppendF(&query, "title = '%s'",
                      util::EscapeQueryStringValue(title).c_str());
  if (!directory_resource_id.empty()) {
    base::StringAppendF(
        &query, " and '%s' in parents",
        util::EscapeQueryStringValue(directory_resource_id).c_str());
  }
  query += " and trashed = false";

  std::unique_ptr<FilesListRequest> request =
      std::make_unique<FilesListRequest>(sender_.get(), url_generator_,
                                         callback);
  request->set_max_results(kMaxNumFilesResourcePerRequest);
  request->set_q(query);
  request->set_fields(kFileListFields);
  return sender_->StartRequestWithAuthRetry(std::move(request));
}

CancelCallback DriveAPIService::GetChangeList(
    int64_t start_changestamp,
    const ChangeListCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  std::unique_ptr<ChangesListRequest> request =
      std::make_unique<ChangesListRequest>(sender_.get(), url_generator_,
                                           callback);
  request->set_max_results(kMaxNumFilesResourcePerRequest);
  request->set_start_change_id(start_changestamp);
  request->set_fields(kChangeListFields);
  return sender_->StartRequestWithAuthRetry(std::move(request));
}

CancelCallback DriveAPIService::GetChangeListByToken(
    const std::string& team_drive_id,
    const std::string& start_page_token,
    const ChangeListCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  std::unique_ptr<ChangesListRequest> request =
      std::make_unique<ChangesListRequest>(sender_.get(), url_generator_,
                                           callback);
  request->set_max_results(kMaxNumFilesResourcePerRequest);
  request->set_page_token(start_page_token);
  request->set_team_drive_id(team_drive_id);
  request->set_fields(kChangeListFields);
  return sender_->StartRequestWithAuthRetry(std::move(request));
}

CancelCallback DriveAPIService::GetRemainingChangeList(
    const GURL& next_link,
    const ChangeListCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!next_link.is_empty());
  DCHECK(!callback.is_null());

  std::unique_ptr<ChangesListNextPageRequest> request =
      std::make_unique<ChangesListNextPageRequest>(sender_.get(), callback);
  request->set_next_link(next_link);
  request->set_fields(kChangeListFields);
  return sender_->StartRequestWithAuthRetry(std::move(request));
}

CancelCallback DriveAPIService::GetRemainingTeamDriveList(
    const std::string& page_token,
    const TeamDriveListCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!page_token.empty());
  DCHECK(!callback.is_null());

  std::unique_ptr<TeamDriveListRequest> request =
      std::make_unique<TeamDriveListRequest>(sender_.get(), url_generator_,
                                             callback);
  request->set_page_token(page_token);
  request->set_max_results(kMaxNumTeamDriveResourcePerRequest);
  request->set_fields(kTeamDrivesListFields);
  return sender_->StartRequestWithAuthRetry(std::move(request));
}

CancelCallback DriveAPIService::GetRemainingFileList(
    const GURL& next_link,
    const FileListCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!next_link.is_empty());
  DCHECK(!callback.is_null());

  std::unique_ptr<FilesListNextPageRequest> request =
      std::make_unique<FilesListNextPageRequest>(sender_.get(), callback);
  request->set_next_link(next_link);
  request->set_fields(kFileListFields);
  return sender_->StartRequestWithAuthRetry(std::move(request));
}

CancelCallback DriveAPIService::GetFileResource(
    const std::string& resource_id,
    const FileResourceCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  std::unique_ptr<FilesGetRequest> request = std::make_unique<FilesGetRequest>(
      sender_.get(), url_generator_, callback);
  request->set_file_id(resource_id);
  request->set_fields(kFileResourceFields);
  return sender_->StartRequestWithAuthRetry(std::move(request));
}

CancelCallback DriveAPIService::GetAboutResource(
    const AboutResourceCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  std::unique_ptr<AboutGetRequest> request = std::make_unique<AboutGetRequest>(
      sender_.get(), url_generator_, callback);
  request->set_fields(kAboutResourceFields);
  return sender_->StartRequestWithAuthRetry(std::move(request));
}

CancelCallback DriveAPIService::GetStartPageToken(
    const std::string& team_drive_id,
    const StartPageTokenCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  std::unique_ptr<StartPageTokenRequest> request =
      std::make_unique<StartPageTokenRequest>(sender_.get(), url_generator_,
                                              callback);
  request->set_team_drive_id(team_drive_id);
  return sender_->StartRequestWithAuthRetry(std::move(request));
}

CancelCallback DriveAPIService::DownloadFile(
    const base::FilePath& local_cache_path,
    const std::string& resource_id,
    const DownloadActionCallback& download_action_callback,
    const GetContentCallback& get_content_callback,
    const ProgressCallback& progress_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!download_action_callback.is_null());
  // get_content_callback may be null.

  return sender_->StartRequestWithAuthRetry(
      std::make_unique<DownloadFileRequest>(
          sender_.get(), url_generator_, resource_id, local_cache_path,
          download_action_callback, get_content_callback, progress_callback));
}

CancelCallback DriveAPIService::DeleteResource(
    const std::string& resource_id,
    const std::string& etag,
    const EntryActionCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  std::unique_ptr<FilesDeleteRequest> request =
      std::make_unique<FilesDeleteRequest>(sender_.get(), url_generator_,
                                           callback);
  request->set_file_id(resource_id);
  request->set_etag(etag);
  return sender_->StartRequestWithAuthRetry(std::move(request));
}

CancelCallback DriveAPIService::TrashResource(
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  std::unique_ptr<FilesTrashRequest> request =
      std::make_unique<FilesTrashRequest>(
          sender_.get(), url_generator_,
          base::Bind(&EntryActionCallbackAdapter, callback));
  request->set_file_id(resource_id);
  request->set_fields(kFileResourceFields);
  return sender_->StartRequestWithAuthRetry(std::move(request));
}

CancelCallback DriveAPIService::AddNewDirectory(
    const std::string& parent_resource_id,
    const std::string& directory_title,
    const AddNewDirectoryOptions& options,
    const FileResourceCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  std::unique_ptr<FilesInsertRequest> request =
      std::make_unique<FilesInsertRequest>(sender_.get(), url_generator_,
                                           callback);
  request->set_visibility(options.visibility);
  request->set_last_viewed_by_me_date(options.last_viewed_by_me_date);
  request->set_mime_type(kFolderMimeType);
  request->set_modified_date(options.modified_date);
  request->add_parent(parent_resource_id);
  request->set_title(directory_title);
  request->set_properties(options.properties);
  request->set_fields(kFileResourceFields);
  return sender_->StartRequestWithAuthRetry(std::move(request));
}

CancelCallback DriveAPIService::CopyResource(
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& new_title,
    const base::Time& last_modified,
    const FileResourceCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  std::unique_ptr<FilesCopyRequest> request =
      std::make_unique<FilesCopyRequest>(sender_.get(), url_generator_,
                                         callback);
  request->set_file_id(resource_id);
  request->add_parent(parent_resource_id);
  request->set_title(new_title);
  request->set_modified_date(last_modified);
  request->set_fields(kFileResourceFields);
  return sender_->StartRequestWithAuthRetry(std::move(request));
}

CancelCallback DriveAPIService::UpdateResource(
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& new_title,
    const base::Time& last_modified,
    const base::Time& last_viewed_by_me,
    const google_apis::drive::Properties& properties,
    const FileResourceCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  std::unique_ptr<FilesPatchRequest> request =
      std::make_unique<FilesPatchRequest>(sender_.get(), url_generator_,
                                          callback);
  request->set_file_id(resource_id);
  request->set_title(new_title);
  if (!parent_resource_id.empty())
    request->add_parent(parent_resource_id);
  if (!last_modified.is_null()) {
    // Need to set setModifiedDate to true to overwrite modifiedDate.
    request->set_set_modified_date(true);
    request->set_modified_date(last_modified);
  }
  if (!last_viewed_by_me.is_null()) {
    // Need to set updateViewedDate to false, otherwise the lastViewedByMeDate
    // will be set to the request time (not the specified time via request).
    request->set_update_viewed_date(false);
    request->set_last_viewed_by_me_date(last_viewed_by_me);
  }
  request->set_fields(kFileResourceFields);
  request->set_properties(properties);
  return sender_->StartRequestWithAuthRetry(std::move(request));
}

CancelCallback DriveAPIService::AddResourceToDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  std::unique_ptr<ChildrenInsertRequest> request =
      std::make_unique<ChildrenInsertRequest>(sender_.get(), url_generator_,
                                              callback);
  request->set_folder_id(parent_resource_id);
  request->set_id(resource_id);
  return sender_->StartRequestWithAuthRetry(std::move(request));
}

CancelCallback DriveAPIService::RemoveResourceFromDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  std::unique_ptr<ChildrenDeleteRequest> request =
      std::make_unique<ChildrenDeleteRequest>(sender_.get(), url_generator_,
                                              callback);
  request->set_child_id(resource_id);
  request->set_folder_id(parent_resource_id);
  return sender_->StartRequestWithAuthRetry(std::move(request));
}

CancelCallback DriveAPIService::InitiateUploadNewFile(
    const std::string& content_type,
    int64_t content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    const UploadNewFileOptions& options,
    const InitiateUploadCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  std::unique_ptr<InitiateUploadNewFileRequest> request =
      std::make_unique<InitiateUploadNewFileRequest>(
          sender_.get(), url_generator_, content_type, content_length,
          parent_resource_id, title, callback);
  request->set_modified_date(options.modified_date);
  request->set_last_viewed_by_me_date(options.last_viewed_by_me_date);
  request->set_properties(options.properties);
  return sender_->StartRequestWithAuthRetry(std::move(request));
}

CancelCallback DriveAPIService::InitiateUploadExistingFile(
    const std::string& content_type,
    int64_t content_length,
    const std::string& resource_id,
    const UploadExistingFileOptions& options,
    const InitiateUploadCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  std::unique_ptr<InitiateUploadExistingFileRequest> request =
      std::make_unique<InitiateUploadExistingFileRequest>(
          sender_.get(), url_generator_, content_type, content_length,
          resource_id, options.etag, callback);
  request->set_parent_resource_id(options.parent_resource_id);
  request->set_title(options.title);
  request->set_modified_date(options.modified_date);
  request->set_last_viewed_by_me_date(options.last_viewed_by_me_date);
  request->set_properties(options.properties);
  return sender_->StartRequestWithAuthRetry(std::move(request));
}

CancelCallback DriveAPIService::ResumeUpload(
    const GURL& upload_url,
    int64_t start_position,
    int64_t end_position,
    int64_t content_length,
    const std::string& content_type,
    const base::FilePath& local_file_path,
    const UploadRangeCallback& callback,
    const ProgressCallback& progress_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithAuthRetry(
      std::make_unique<ResumeUploadRequest>(
          sender_.get(), upload_url, start_position, end_position,
          content_length, content_type, local_file_path, callback,
          progress_callback));
}

CancelCallback DriveAPIService::GetUploadStatus(
    const GURL& upload_url,
    int64_t content_length,
    const UploadRangeCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithAuthRetry(
      std::make_unique<GetUploadStatusRequest>(sender_.get(), upload_url,
                                               content_length, callback));
}

CancelCallback DriveAPIService::MultipartUploadNewFile(
    const std::string& content_type,
    int64_t content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    const base::FilePath& local_file_path,
    const drive::UploadNewFileOptions& options,
    const FileResourceCallback& callback,
    const google_apis::ProgressCallback& progress_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithAuthRetry(
      std::make_unique<google_apis::drive::SingleBatchableDelegateRequest>(
          sender_.get(),
          std::make_unique<google_apis::drive::MultipartUploadNewFileDelegate>(
              sender_->blocking_task_runner(), title, parent_resource_id,
              content_type, content_length, options.modified_date,
              options.last_viewed_by_me_date, local_file_path,
              options.properties, url_generator_, callback,
              progress_callback)));
}

CancelCallback DriveAPIService::MultipartUploadExistingFile(
    const std::string& content_type,
    int64_t content_length,
    const std::string& resource_id,
    const base::FilePath& local_file_path,
    const drive::UploadExistingFileOptions& options,
    const FileResourceCallback& callback,
    const google_apis::ProgressCallback& progress_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithAuthRetry(
      std::make_unique<google_apis::drive::SingleBatchableDelegateRequest>(
          sender_.get(),
          std::make_unique<
              google_apis::drive::MultipartUploadExistingFileDelegate>(
              sender_->blocking_task_runner(), options.title, resource_id,
              options.parent_resource_id, content_type, content_length,
              options.modified_date, options.last_viewed_by_me_date,
              local_file_path, options.etag, options.properties, url_generator_,
              callback, progress_callback)));
}

google_apis::CancelCallback DriveAPIService::AddPermission(
    const std::string& resource_id,
    const std::string& email,
    google_apis::drive::PermissionRole role,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  std::unique_ptr<google_apis::drive::PermissionsInsertRequest> request =
      std::make_unique<google_apis::drive::PermissionsInsertRequest>(
          sender_.get(), url_generator_, callback);
  request->set_id(resource_id);
  request->set_role(role);
  request->set_type(google_apis::drive::PERMISSION_TYPE_USER);
  request->set_value(email);
  return sender_->StartRequestWithAuthRetry(std::move(request));
}

bool DriveAPIService::HasAccessToken() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return sender_->auth_service()->HasAccessToken();
}

void DriveAPIService::RequestAccessToken(const AuthStatusCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  const std::string access_token = sender_->auth_service()->access_token();
  if (!access_token.empty()) {
    callback.Run(google_apis::HTTP_NOT_MODIFIED, access_token);
    return;
  }

  // Retrieve the new auth token.
  sender_->auth_service()->StartAuthentication(callback);
}

bool DriveAPIService::HasRefreshToken() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return sender_->auth_service()->HasRefreshToken();
}

void DriveAPIService::ClearAccessToken() {
  DCHECK(thread_checker_.CalledOnValidThread());
  sender_->auth_service()->ClearAccessToken();
}

void DriveAPIService::ClearRefreshToken() {
  DCHECK(thread_checker_.CalledOnValidThread());
  sender_->auth_service()->ClearRefreshToken();
}

void DriveAPIService::OnOAuth2RefreshTokenChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (CanSendRequest()) {
    for (auto& observer : observers_)
      observer.OnReadyToSendRequests();
  } else if (!HasRefreshToken()) {
    for (auto& observer : observers_)
      observer.OnRefreshTokenInvalid();
  }
}

std::unique_ptr<BatchRequestConfiguratorInterface>
DriveAPIService::StartBatchRequest() {
  std::unique_ptr<google_apis::drive::BatchUploadRequest> request =
      std::make_unique<google_apis::drive::BatchUploadRequest>(sender_.get(),
                                                               url_generator_);
  const base::WeakPtr<google_apis::drive::BatchUploadRequest> weak_ref =
      request->GetWeakPtrAsBatchUploadRequest();
  // Have sender_ manage the lifetime of the request.
  // TODO(hirono): Currently we need to pass the ownership of the request to
  // RequestSender before the request is committed because the request has a
  // reference to RequestSender and we should ensure to delete the request when
  // the sender is deleted. Resolve the circulating dependency and fix it.
  const google_apis::CancelCallback callback =
      sender_->StartRequestWithAuthRetry(std::move(request));
  return std::make_unique<BatchRequestConfigurator>(
      weak_ref, sender_->blocking_task_runner(), url_generator_, callback);
}

}  // namespace drive
