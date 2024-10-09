// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/service/fake_drive_service.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/hash/md5.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/file_system_core_util.h"
#include "google_apis/common/test_util.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/drive_common_callbacks.h"
#include "net/base/url_util.h"

using google_apis::AboutResource;
using google_apis::AboutResourceCallback;
using google_apis::ApiErrorCode;
using google_apis::AuthStatusCallback;
using google_apis::CancelCallbackOnce;
using google_apis::ChangeList;
using google_apis::ChangeListCallback;
using google_apis::ChangeListOnceCallback;
using google_apis::ChangeResource;
using google_apis::DownloadActionCallback;
using google_apis::DRIVE_FILE_ERROR;
using google_apis::EntryActionCallback;
using google_apis::FileList;
using google_apis::FileListCallback;
using google_apis::FileResource;
using google_apis::FileResourceCallback;
using google_apis::GetContentCallback;
using google_apis::HTTP_BAD_REQUEST;
using google_apis::HTTP_CREATED;
using google_apis::HTTP_FORBIDDEN;
using google_apis::HTTP_NO_CONTENT;
using google_apis::HTTP_NOT_FOUND;
using google_apis::HTTP_PRECONDITION;
using google_apis::HTTP_RESUME_INCOMPLETE;
using google_apis::HTTP_SUCCESS;
using google_apis::InitiateUploadCallback;
using google_apis::NO_CONNECTION;
using google_apis::OTHER_ERROR;
using google_apis::ParentReference;
using google_apis::ProgressCallback;
using google_apis::StartPageToken;
using google_apis::TeamDriveList;
using google_apis::TeamDriveListCallback;
using google_apis::TeamDriveResource;
using google_apis::UploadRangeResponse;
using google_apis::drive::UploadRangeCallback;
namespace test_util = google_apis::test_util;

namespace drive {
namespace {

// Returns true if the entry matches with the search query.
// Supports queries consist of following format.
// - Phrases quoted by double/single quotes
// - AND search for multiple words/phrases segmented by space
// - Limited attribute search.  Only "title:" is supported.
bool EntryMatchWithQuery(const ChangeResource& entry,
                         const std::string& query) {
  base::StringTokenizer tokenizer(query, " ");
  tokenizer.set_quote_chars("\"'");
  while (tokenizer.GetNext()) {
    std::string key, value;
    const std::string& token = tokenizer.token();
    if (token.find(':') == std::string::npos) {
      base::TrimString(token, "\"'", &value);
    } else {
      base::StringTokenizer key_value(token, ":");
      key_value.set_quote_chars("\"'");
      if (!key_value.GetNext())
        return false;
      key = key_value.token();
      if (!key_value.GetNext())
        return false;
      base::TrimString(key_value.token(), "\"'", &value);
    }

    // TODO(peria): Deal with other attributes than title.
    if (!key.empty() && key != "title")
      return false;
    // Search query in the title.
    if (!entry.file() || entry.file()->title().find(value) == std::string::npos)
      return false;
  }
  return true;
}

void ScheduleUploadRangeCallback(UploadRangeCallback callback,
                                 int64_t start_position,
                                 int64_t end_position,
                                 ApiErrorCode error,
                                 std::unique_ptr<FileResource> entry) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     UploadRangeResponse(error, start_position, end_position),
                     std::move(entry)));
}

void FileListCallbackAdapter(FileListCallback callback,
                             ApiErrorCode error,
                             std::unique_ptr<ChangeList> change_list) {
  std::unique_ptr<FileList> file_list;
  if (!change_list) {
    std::move(callback).Run(error, std::move(file_list));
    return;
  }

  file_list = std::make_unique<FileList>();
  file_list->set_next_link(change_list->next_link());
  for (size_t i = 0; i < change_list->items().size(); ++i) {
    const ChangeResource& entry = *change_list->items()[i];
    if (entry.type() == ChangeResource::FILE && entry.file())
      file_list->mutable_items()->push_back(
          std::make_unique<FileResource>(*entry.file()));
  }
  std::move(callback).Run(error, std::move(file_list));
}

bool UserHasWriteAccess(google_apis::drive::PermissionRole user_permission) {
  switch (user_permission) {
    case google_apis::drive::PERMISSION_ROLE_OWNER:
    case google_apis::drive::PERMISSION_ROLE_WRITER:
      return true;
    case google_apis::drive::PERMISSION_ROLE_READER:
    case google_apis::drive::PERMISSION_ROLE_COMMENTER:
      break;
  }
  return false;
}

void CallFileResourceCallback(FileResourceCallback callback,
                              const UploadRangeResponse& response,
                              std::unique_ptr<FileResource> entry) {
  std::move(callback).Run(response.code, std::move(entry));
}

struct CallResumeUpload {
  CallResumeUpload() = default;
  ~CallResumeUpload() = default;

  void Run(ApiErrorCode code, const GURL& upload_url) {
    if (service) {
      service->ResumeUpload(
          upload_url,
          /* start position */ 0,
          /* end position */ content_length, content_length, content_type,
          local_file_path,
          base::BindOnce(&CallFileResourceCallback, std::move(callback)),
          progress_callback);
    }
  }

  base::WeakPtr<FakeDriveService> service;
  int64_t content_length;
  std::string content_type;
  base::FilePath local_file_path;
  FileResourceCallback callback;
  ProgressCallback progress_callback;
};

std::string GetTeamDriveId(const google_apis::ChangeResource& change_resource) {
  std::string team_drive_id;
  switch (change_resource.type()) {
    case ChangeResource::FILE:
      team_drive_id = change_resource.file()->team_drive_id();
      break;
    case ChangeResource::TEAM_DRIVE:
      team_drive_id = change_resource.team_drive_id();
      break;
    case ChangeResource::UNKNOWN:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return team_drive_id;
}

}  // namespace

struct FakeDriveService::EntryInfo {
  EntryInfo()
      : user_permission(google_apis::drive::PERMISSION_ROLE_OWNER),
        visibility(google_apis::drive::FILE_VISIBILITY_DEFAULT) {}

  google_apis::ChangeResource change_resource;
  GURL share_url;
  std::string content_data;

  // Behaves in the same way as "userPermission" described in
  // https://developers.google.com/drive/v2/reference/files
  google_apis::drive::PermissionRole user_permission;

  google_apis::drive::FileVisibility visibility;
};

struct FakeDriveService::UploadSession {
  std::string content_type;
  int64_t content_length;
  std::string parent_resource_id;
  std::string resource_id;
  std::string etag;
  std::string title;

  int64_t uploaded_size;

  UploadSession() : content_length(0), uploaded_size(0) {}

  UploadSession(std::string content_type,
                int64_t content_length,
                std::string parent_resource_id,
                std::string resource_id,
                std::string etag,
                std::string title)
      : content_type(content_type),
        content_length(content_length),
        parent_resource_id(parent_resource_id),
        resource_id(resource_id),
        etag(etag),
        title(title),
        uploaded_size(0) {}
};

FakeDriveService::FakeDriveService()
    : about_resource_(new AboutResource),
      start_page_token_(new StartPageToken),
      date_seq_(0),
      next_upload_sequence_number_(0),
      largest_changestamp_(654321),
      default_max_results_(0),
      resource_id_count_(0),
      team_drive_list_load_count_(0),
      file_list_load_count_(0),
      change_list_load_count_(0),
      directory_load_count_(0),
      about_resource_load_count_(0),
      blocked_file_list_load_count_(0),
      start_page_token_load_count_(0),
      offline_(false),
      never_return_all_file_list_(false) {
  UpdateLatestChangelistId(largest_changestamp_, std::string());
  about_resource_->set_quota_bytes_total(9876543210);
  about_resource_->set_quota_bytes_used_aggregate(6789012345);
  about_resource_->set_root_folder_id(GetRootResourceId());
}

FakeDriveService::~FakeDriveService() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void FakeDriveService::AddTeamDrive(const std::string& id,
                                    const std::string& name) {
  AddTeamDrive(id, name, "");
}

void FakeDriveService::AddTeamDrive(const std::string& id,
                                    const std::string& name,
                                    const std::string& start_page_token) {
  DCHECK(entries_.find(id) == entries_.end());
  auto team_drive = std::make_unique<TeamDriveResource>();
  team_drive->set_id(id);
  team_drive->set_name(name);
  team_drive_value_.push_back(std::move(team_drive));

  team_drive_start_page_tokens_[id] = std::make_unique<StartPageToken>();
  team_drive_start_page_tokens_[id]->set_start_page_token(start_page_token);

  const EntryInfo* new_entry = AddNewTeamDriveEntry(id, name);
  DCHECK(new_entry);
}

void FakeDriveService::SetQuotaValue(int64_t used, int64_t total) {
  DCHECK(thread_checker_.CalledOnValidThread());

  about_resource_->set_quota_bytes_used_aggregate(used);
  about_resource_->set_quota_bytes_total(total);
}

void FakeDriveService::Initialize(const CoreAccountId& account_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void FakeDriveService::AddObserver(DriveServiceObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void FakeDriveService::RemoveObserver(DriveServiceObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool FakeDriveService::CanSendRequest() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return true;
}

bool FakeDriveService::HasAccessToken() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return true;
}

void FakeDriveService::RequestAccessToken(AuthStatusCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);
  std::move(callback).Run(google_apis::HTTP_NOT_MODIFIED, "fake_access_token");
}

bool FakeDriveService::HasRefreshToken() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return true;
}

void FakeDriveService::ClearAccessToken() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void FakeDriveService::ClearRefreshToken() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

std::string FakeDriveService::GetRootResourceId() const {
  return "fake_root";
}

void FakeDriveService::GetTeamDriveListInternal(
    int start_offset,
    int max_results,
    int* load_counter,
    google_apis::TeamDriveListCallback callback) {
  if (offline_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), NO_CONNECTION,
                                  std::unique_ptr<TeamDriveList>()));
    return;
  }
  if (load_counter)
    ++*load_counter;

  // If max_results is zero then we want to return the entire list.
  if (max_results <= 0) {
    max_results = team_drive_value_.size();
  }

  auto result = std::make_unique<TeamDriveList>();
  size_t next_start_offset = start_offset + max_results;
  if (next_start_offset < team_drive_value_.size()) {
    // Embed next start offset to next page token to be read in
    // GetRemainingTeamDriveList next time.
    result->set_next_page_token(base::NumberToString(next_start_offset));
  }
  for (size_t i = start_offset;
       i < std::min(next_start_offset, team_drive_value_.size()); ++i) {
    std::unique_ptr<TeamDriveResource> team_drive(new TeamDriveResource);
    team_drive->set_id(team_drive_value_[i]->id());
    team_drive->set_name(team_drive_value_[i]->name());
    team_drive->set_capabilities(team_drive_value_[i]->capabilities());
    result->mutable_items()->push_back(std::move(team_drive));
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), HTTP_SUCCESS, std::move(result)));
}

CancelCallbackOnce FakeDriveService::GetAllTeamDriveList(
    TeamDriveListCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);

  GetTeamDriveListInternal(0, default_max_results_,
                           &team_drive_list_load_count_, std::move(callback));

  return CancelCallbackOnce();
}

CancelCallbackOnce FakeDriveService::GetAllFileList(
    const std::string& team_drive_id,
    FileListCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (never_return_all_file_list_) {
    ++blocked_file_list_load_count_;
    return CancelCallbackOnce();
  }

  GetChangeListInternal(
      0,              // start changestamp
      std::string(),  // empty search query
      std::string(),  // no directory resource id,
      team_drive_id,
      0,  // start offset
      default_max_results_, &file_list_load_count_,
      base::BindOnce(&FileListCallbackAdapter, std::move(callback)));
  return CancelCallbackOnce();
}

CancelCallbackOnce FakeDriveService::GetFileListInDirectory(
    const std::string& directory_resource_id,
    FileListCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!directory_resource_id.empty());

  GetChangeListInternal(
      0,              // start changestamp
      std::string(),  // empty search query
      directory_resource_id,
      std::string(),  // empty team drive id.
      0,              // start offset
      default_max_results_, &directory_load_count_,
      base::BindOnce(&FileListCallbackAdapter, std::move(callback)));
  return CancelCallbackOnce();
}

CancelCallbackOnce FakeDriveService::Search(const std::string& search_query,
                                            FileListCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!search_query.empty());

  GetChangeListInternal(
      0,  // start changestamp
      search_query,
      std::string(),  // no directory resource id,
      std::string(),  // empty team drive id.
      0,              // start offset
      default_max_results_, nullptr,
      base::BindOnce(&FileListCallbackAdapter, std::move(callback)));
  return CancelCallbackOnce();
}

CancelCallbackOnce FakeDriveService::SearchByTitle(
    const std::string& title,
    const std::string& directory_resource_id,
    FileListCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!title.empty());

  // Note: the search implementation here doesn't support quotation unescape,
  // so don't escape here.
  GetChangeListInternal(
      0,  // start changestamp
      base::StringPrintf("title:'%s'", title.c_str()), directory_resource_id,
      std::string(),  // empty team drive id.
      0,              // start offset
      default_max_results_, nullptr,
      base::BindOnce(&FileListCallbackAdapter, std::move(callback)));
  return CancelCallbackOnce();
}

CancelCallbackOnce FakeDriveService::GetChangeList(
    int64_t start_changestamp,
    ChangeListCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);

  GetChangeListInternal(start_changestamp,
                        std::string(),  // empty search query
                        std::string(),  // no directory resource id,
                        std::string(),  // empty team drive id.
                        0,              // start offset
                        default_max_results_, &change_list_load_count_,
                        std::move(callback));
  return CancelCallbackOnce();
}

CancelCallbackOnce FakeDriveService::GetChangeListByToken(
    const std::string& team_drive_id,
    const std::string& start_page_token,
    ChangeListCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);

  int64_t changestamp = 0;
  CHECK(base::StringToInt64(start_page_token, &changestamp));

  GetChangeListInternal(changestamp,
                        std::string(),  // empty search query
                        std::string(),  // no directory resource id,
                        team_drive_id,
                        0,  // start offset
                        default_max_results_, &change_list_load_count_,
                        std::move(callback));

  return CancelCallbackOnce();
}

CancelCallbackOnce FakeDriveService::GetRemainingChangeList(
    const GURL& next_link,
    ChangeListCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!next_link.is_empty());
  DCHECK(callback);

  // "changestamp", "q", "parent" and "start-offset" are parameters to
  // implement "paging" of the result on FakeDriveService.
  // The URL should be the one filled in GetChangeListInternal of the
  // previous method invocation, so it should start with "http://localhost/?".
  // See also GetChangeListInternal.
  DCHECK_EQ(next_link.host(), "localhost");
  DCHECK_EQ(next_link.path(), "/");

  int64_t start_changestamp = 0;
  std::string search_query;
  std::string directory_resource_id;
  std::string team_drive_id;
  int start_offset = 0;
  int max_results = default_max_results_;
  base::StringPairs parameters;
  if (base::SplitStringIntoKeyValuePairs(next_link.query(), '=', '&',
                                         &parameters)) {
    for (const auto& param : parameters) {
      if (param.first == "changestamp") {
        base::StringToInt64(param.second, &start_changestamp);
      } else if (param.first == "q") {
        search_query = base::UnescapeBinaryURLComponent(param.second);
      } else if (param.first == "parent") {
        directory_resource_id = base::UnescapeBinaryURLComponent(param.second);
      } else if (param.first == "team-drive-id") {
        team_drive_id = base::UnescapeBinaryURLComponent(param.second);
      } else if (param.first == "start-offset") {
        base::StringToInt(param.second, &start_offset);
      } else if (param.first == "max-results") {
        base::StringToInt(param.second, &max_results);
      }
    }
  }

  GetChangeListInternal(start_changestamp, search_query, directory_resource_id,
                        team_drive_id, start_offset, max_results, nullptr,
                        std::move(callback));
  return CancelCallbackOnce();
}

CancelCallbackOnce FakeDriveService::GetRemainingTeamDriveList(
    const std::string& page_token,
    TeamDriveListCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!page_token.empty());
  DCHECK(callback);

  // Next offset index to page token is embedded in the token.
  size_t start_offset;
  bool parse_success = base::StringToSizeT(page_token, &start_offset);
  DCHECK(parse_success);
  GetTeamDriveListInternal(start_offset, default_max_results_, nullptr,
                           std::move(callback));
  return CancelCallbackOnce();
}

CancelCallbackOnce FakeDriveService::GetRemainingFileList(
    const GURL& next_link,
    FileListCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!next_link.is_empty());

  return GetRemainingChangeList(
      next_link, base::BindOnce(&FileListCallbackAdapter, std::move(callback)));
}

CancelCallbackOnce FakeDriveService::GetFileResource(
    const std::string& resource_id,
    FileResourceCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);

  if (offline_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), NO_CONNECTION,
                                  std::unique_ptr<FileResource>()));
    return CancelCallbackOnce();
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (entry && entry->change_resource.file()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HTTP_SUCCESS,
                                  std::make_unique<FileResource>(
                                      *entry->change_resource.file())));
    return CancelCallbackOnce();
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), HTTP_NOT_FOUND,
                                std::unique_ptr<FileResource>()));
  return CancelCallbackOnce();
}

CancelCallbackOnce FakeDriveService::GetAboutResource(
    AboutResourceCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);

  if (offline_) {
    std::unique_ptr<AboutResource> null;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), NO_CONNECTION, std::move(null)));
    return CancelCallbackOnce();
  }

  ++about_resource_load_count_;
  std::unique_ptr<AboutResource> about_resource(
      new AboutResource(*about_resource_));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), HTTP_SUCCESS,
                                std::move(about_resource)));
  return CancelCallbackOnce();
}

CancelCallbackOnce FakeDriveService::GetStartPageToken(
    const std::string& team_drive_id,
    google_apis::StartPageTokenCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);

  if (offline_) {
    std::unique_ptr<StartPageToken> null;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), NO_CONNECTION, std::move(null)));
    return CancelCallbackOnce();
  }

  std::unique_ptr<StartPageToken> start_page_token;
  if (team_drive_id.empty()) {
    start_page_token = std::make_unique<StartPageToken>(*start_page_token_);
  } else {
    auto it = team_drive_start_page_tokens_.find(team_drive_id);
    CHECK(it != team_drive_start_page_tokens_.end(), base::NotFatalUntil::M130);
    start_page_token = std::make_unique<StartPageToken>(*(it->second));
  }
  ++start_page_token_load_count_;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), HTTP_SUCCESS,
                                std::move(start_page_token)));
  return CancelCallbackOnce();
}

CancelCallbackOnce FakeDriveService::DeleteResource(
    const std::string& resource_id,
    const std::string& etag,
    EntryActionCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);

  if (offline_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), NO_CONNECTION));
    return CancelCallbackOnce();
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (!entry) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HTTP_NOT_FOUND));
    return CancelCallbackOnce();
  }

  ChangeResource* change = &entry->change_resource;
  if (change->is_deleted()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HTTP_NOT_FOUND));
    return CancelCallbackOnce();
  }

  const FileResource* file = change->file();
  if (!etag.empty() && etag != file->etag()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HTTP_PRECONDITION));
    return CancelCallbackOnce();
  }

  if (entry->user_permission != google_apis::drive::PERMISSION_ROLE_OWNER) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HTTP_FORBIDDEN));
    return CancelCallbackOnce();
  }

  change->set_deleted(true);
  AddNewChangestamp(change, file->team_drive_id());
  change->set_file(std::unique_ptr<FileResource>());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), HTTP_NO_CONTENT));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FakeDriveService::NotifyObservers,
                                weak_ptr_factory_.GetWeakPtr()));
  return CancelCallbackOnce();
}

CancelCallbackOnce FakeDriveService::TrashResource(
    const std::string& resource_id,
    EntryActionCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);

  if (offline_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), NO_CONNECTION));
    return CancelCallbackOnce();
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (!entry) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HTTP_NOT_FOUND));
    return CancelCallbackOnce();
  }

  ChangeResource* change = &entry->change_resource;
  FileResource* file = change->mutable_file();
  if (change->is_deleted() || file->labels().is_trashed()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HTTP_NOT_FOUND));
    return CancelCallbackOnce();
  }

  if (entry->user_permission != google_apis::drive::PERMISSION_ROLE_OWNER) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HTTP_FORBIDDEN));
    return CancelCallbackOnce();
  }

  file->mutable_labels()->set_trashed(true);
  AddNewChangestamp(change, file->team_drive_id());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), HTTP_SUCCESS));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FakeDriveService::NotifyObservers,
                                weak_ptr_factory_.GetWeakPtr()));
  return CancelCallbackOnce();
}

CancelCallbackOnce FakeDriveService::DownloadFile(
    const base::FilePath& local_cache_path,
    const std::string& resource_id,
    DownloadActionCallback download_action_callback,
    const GetContentCallback& get_content_callback,
    ProgressCallback progress_callback) {
  base::ScopedAllowBlocking allow_blocking;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!download_action_callback.is_null());

  if (offline_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(download_action_callback),
                                  NO_CONNECTION, base::FilePath()));
    return CancelCallbackOnce();
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (!entry || entry->change_resource.file()->IsHostedDocument()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(download_action_callback),
                                  HTTP_NOT_FOUND, base::FilePath()));
    return CancelCallbackOnce();
  }

  const FileResource* file = entry->change_resource.file();
  const std::string& content_data = entry->content_data;
  int64_t file_size = file->file_size();
  DCHECK_EQ(static_cast<size_t>(file_size), content_data.size());

  if (!get_content_callback.is_null()) {
    const int64_t kBlockSize = 5;
    for (int64_t i = 0; i < file_size; i += kBlockSize) {
      const int64_t size = std::min(kBlockSize, file_size - i);
      std::unique_ptr<std::string> content_for_callback(
          new std::string(content_data.substr(i, size)));
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(get_content_callback, HTTP_SUCCESS,
                                    std::move(content_for_callback), i == 0));
    }
  }

  if (!test_util::WriteStringToFile(local_cache_path, content_data)) {
    // Failed to write the content.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(download_action_callback),
                                  DRIVE_FILE_ERROR, base::FilePath()));
    return CancelCallbackOnce();
  }

  if (!progress_callback.is_null()) {
    // See also the comment in ResumeUpload(). For testing that clients
    // can handle the case progress_callback is called multiple times,
    // here we invoke the callback twice.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(progress_callback, file_size / 2, file_size));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(progress_callback, file_size, file_size));
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(download_action_callback),
                                HTTP_SUCCESS, local_cache_path));
  return google_apis::CancelCallbackOnce();
}

CancelCallbackOnce FakeDriveService::CopyResource(
    const std::string& resource_id,
    const std::string& in_parent_resource_id,
    const std::string& new_title,
    const base::Time& last_modified,
    FileResourceCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);

  if (offline_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), NO_CONNECTION,
                                  std::unique_ptr<FileResource>()));
    return CancelCallbackOnce();
  }

  const std::string& parent_resource_id = in_parent_resource_id.empty()
                                              ? GetRootResourceId()
                                              : in_parent_resource_id;

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (!entry) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HTTP_NOT_FOUND,
                                  std::unique_ptr<FileResource>()));
    return CancelCallbackOnce();
  }

  // Make a copy and set the new resource ID and the new title.
  std::unique_ptr<EntryInfo> copied_entry(new EntryInfo);
  copied_entry->content_data = entry->content_data;
  copied_entry->share_url = entry->share_url;
  copied_entry->change_resource.set_type(ChangeResource::FILE);
  copied_entry->change_resource.set_file(
      std::make_unique<FileResource>(*entry->change_resource.file()));

  ChangeResource* new_change = &copied_entry->change_resource;
  FileResource* new_file = new_change->mutable_file();
  const std::string new_resource_id = GetNewResourceId();
  new_change->set_file_id(new_resource_id);
  new_file->set_file_id(new_resource_id);
  new_file->set_title(new_title);

  ParentReference parent;
  parent.set_file_id(parent_resource_id);
  std::vector<ParentReference> parents;
  parents.push_back(parent);
  *new_file->mutable_parents() = parents;

  // Set the team drive for the new entry to the parent.
  if (entries_.count(parent_resource_id) > 0) {
    const ChangeResource& change =
        entries_[parent_resource_id]->change_resource;
    new_file->set_team_drive_id(GetTeamDriveId(change));
  }

  if (!last_modified.is_null()) {
    new_file->set_modified_date(last_modified);
    new_file->set_modified_by_me_date(last_modified);
  } else {
    auto now = base::Time::Now();
    new_file->set_modified_date(now);
    new_file->set_modified_by_me_date(now);
  }

  AddNewChangestamp(new_change, new_file->team_drive_id());
  UpdateETag(new_file);

  // Add the new entry to the map.
  entries_[new_resource_id] = std::move(copied_entry);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), HTTP_SUCCESS,
                                std::make_unique<FileResource>(*new_file)));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FakeDriveService::NotifyObservers,
                                weak_ptr_factory_.GetWeakPtr()));
  return CancelCallbackOnce();
}

CancelCallbackOnce FakeDriveService::UpdateResource(
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& new_title,
    const base::Time& last_modified,
    const base::Time& last_viewed_by_me,
    const google_apis::drive::Properties& properties,
    FileResourceCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);

  if (offline_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), NO_CONNECTION,
                                  std::unique_ptr<FileResource>()));
    return CancelCallbackOnce();
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (!entry) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HTTP_NOT_FOUND,
                                  std::unique_ptr<FileResource>()));
    return CancelCallbackOnce();
  }

  if (!UserHasWriteAccess(entry->user_permission)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HTTP_FORBIDDEN,
                                  std::unique_ptr<FileResource>()));
    return CancelCallbackOnce();
  }

  ChangeResource* change = &entry->change_resource;
  FileResource* file = change->mutable_file();

  if (!new_title.empty())
    file->set_title(new_title);

  // Set parent if necessary.
  if (!parent_resource_id.empty()) {
    ParentReference parent;
    parent.set_file_id(parent_resource_id);

    std::vector<ParentReference> parents;
    parents.push_back(parent);
    *file->mutable_parents() = parents;
  }

  if (!last_modified.is_null()) {
    file->set_modified_date(last_modified);
    file->set_modified_by_me_date(last_modified);
  }

  if (!last_viewed_by_me.is_null())
    file->set_last_viewed_by_me_date(last_viewed_by_me);

  AddNewChangestamp(change, file->team_drive_id());
  UpdateETag(file);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), HTTP_SUCCESS,
                                std::make_unique<FileResource>(*file)));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FakeDriveService::NotifyObservers,
                                weak_ptr_factory_.GetWeakPtr()));
  return CancelCallbackOnce();
}

CancelCallbackOnce FakeDriveService::AddResourceToDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    EntryActionCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);

  if (offline_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), NO_CONNECTION));
    return CancelCallbackOnce();
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (!entry) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HTTP_NOT_FOUND));
    return CancelCallbackOnce();
  }

  ChangeResource* change = &entry->change_resource;
  // On the real Drive server, resources do not necessary shape a tree
  // structure. That is, each resource can have multiple parent.
  // We mimic the behavior here; AddResourceToDirectoy just adds
  // one more parent, not overwriting old ones.
  ParentReference parent;
  parent.set_file_id(parent_resource_id);
  change->mutable_file()->mutable_parents()->push_back(parent);

  AddNewChangestamp(change, change->file()->team_drive_id());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), HTTP_SUCCESS));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FakeDriveService::NotifyObservers,
                                weak_ptr_factory_.GetWeakPtr()));
  return CancelCallbackOnce();
}

CancelCallbackOnce FakeDriveService::RemoveResourceFromDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    EntryActionCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);

  if (offline_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), NO_CONNECTION));
    return CancelCallbackOnce();
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (!entry) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HTTP_NOT_FOUND));
    return CancelCallbackOnce();
  }

  ChangeResource* change = &entry->change_resource;
  FileResource* file = change->mutable_file();
  std::vector<ParentReference>* parents = file->mutable_parents();
  for (size_t i = 0; i < parents->size(); ++i) {
    if ((*parents)[i].file_id() == parent_resource_id) {
      parents->erase(parents->begin() + i);
      AddNewChangestamp(change, file->team_drive_id());
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), HTTP_NO_CONTENT));
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&FakeDriveService::NotifyObservers,
                                    weak_ptr_factory_.GetWeakPtr()));
      return CancelCallbackOnce();
    }
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), HTTP_NOT_FOUND));
  return CancelCallbackOnce();
}

CancelCallbackOnce FakeDriveService::AddNewDirectory(
    const std::string& parent_resource_id,
    const std::string& directory_title,
    const AddNewDirectoryOptions& options,
    FileResourceCallback callback) {
  return AddNewDirectoryWithResourceId(
      "", parent_resource_id.empty() ? GetRootResourceId() : parent_resource_id,
      directory_title, options, std::move(callback));
}

CancelCallbackOnce FakeDriveService::InitiateUploadNewFile(
    const std::string& content_type,
    int64_t content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    const UploadNewFileOptions& options,
    InitiateUploadCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  if (offline_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), NO_CONNECTION, GURL()));
    return CancelCallbackOnce();
  }

  if (parent_resource_id != GetRootResourceId() &&
      !entries_.count(parent_resource_id)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HTTP_NOT_FOUND, GURL()));
    return CancelCallbackOnce();
  }

  GURL session_url = GetNewUploadSessionUrl();
  upload_sessions_[session_url] =
      UploadSession(content_type, content_length, parent_resource_id,
                    "",  // resource_id
                    "",  // etag
                    title);

  if (title == "never-sync.txt") {
    return CancelCallbackOnce();
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), HTTP_SUCCESS, session_url));
  return CancelCallbackOnce();
}

CancelCallbackOnce FakeDriveService::InitiateUploadExistingFile(
    const std::string& content_type,
    int64_t content_length,
    const std::string& resource_id,
    const UploadExistingFileOptions& options,
    InitiateUploadCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  if (offline_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), NO_CONNECTION, GURL()));
    return CancelCallbackOnce();
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (!entry) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HTTP_NOT_FOUND, GURL()));
    return CancelCallbackOnce();
  }

  if (!UserHasWriteAccess(entry->user_permission)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HTTP_FORBIDDEN, GURL()));
    return CancelCallbackOnce();
  }

  FileResource* file = entry->change_resource.mutable_file();
  if (!options.etag.empty() && options.etag != file->etag()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), HTTP_PRECONDITION, GURL()));
    return CancelCallbackOnce();
  }
  // TODO(hashimoto): Update |file|'s metadata with |options|.

  GURL session_url = GetNewUploadSessionUrl();
  upload_sessions_[session_url] =
      UploadSession(content_type, content_length,
                    "",  // parent_resource_id
                    resource_id, file->etag(), "" /* title */);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), HTTP_SUCCESS, session_url));
  return CancelCallbackOnce();
}

CancelCallbackOnce FakeDriveService::GetUploadStatus(
    const GURL& upload_url,
    int64_t content_length,
    UploadRangeCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);
  return CancelCallbackOnce();
}

CancelCallbackOnce FakeDriveService::ResumeUpload(
    const GURL& upload_url,
    int64_t start_position,
    int64_t end_position,
    int64_t content_length,
    const std::string& content_type,
    const base::FilePath& local_file_path,
    UploadRangeCallback callback,
    ProgressCallback progress_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);

  FileResourceCallback completion_callback =
      base::BindOnce(&ScheduleUploadRangeCallback, std::move(callback),
                     start_position, end_position);

  if (offline_) {
    std::move(completion_callback)
        .Run(NO_CONNECTION, std::unique_ptr<FileResource>());
    return CancelCallbackOnce();
  }

  if (!upload_sessions_.count(upload_url)) {
    std::move(completion_callback)
        .Run(HTTP_NOT_FOUND, std::unique_ptr<FileResource>());
    return CancelCallbackOnce();
  }

  UploadSession* session = &upload_sessions_[upload_url];

  // Chunks are required to be sent in such a ways that they fill from the start
  // of the not-yet-uploaded part with no gaps nor overlaps.
  if (session->uploaded_size != start_position) {
    std::move(completion_callback)
        .Run(HTTP_BAD_REQUEST, std::unique_ptr<FileResource>());
    return CancelCallbackOnce();
  }

  if (!progress_callback.is_null()) {
    // In the real GDataWapi/Drive DriveService, progress is reported in
    // nondeterministic timing. In this fake implementation, we choose to call
    // it twice per one ResumeUpload. This is for making sure that client code
    // works fine even if the callback is invoked more than once; it is the
    // crucial difference of the progress callback from others.
    // Note that progress is notified in the relative offset in each chunk.
    const int64_t chunk_size = end_position - start_position;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(progress_callback, chunk_size / 2, chunk_size));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(progress_callback, chunk_size, chunk_size));
  }

  if (content_length != end_position) {
    session->uploaded_size = end_position;
    std::move(completion_callback)
        .Run(HTTP_RESUME_INCOMPLETE, std::unique_ptr<FileResource>());
    return CancelCallbackOnce();
  }

  std::string content_data;
  {
    base::ScopedAllowBlockingForTesting allow_io;
    if (!base::ReadFileToString(local_file_path, &content_data)) {
      session->uploaded_size = end_position;
      std::move(completion_callback)
          .Run(DRIVE_FILE_ERROR, std::unique_ptr<FileResource>());
      return CancelCallbackOnce();
    }
  }
  session->uploaded_size = end_position;

  // |resource_id| is empty if the upload is for new file.
  if (session->resource_id.empty()) {
    DCHECK(!session->parent_resource_id.empty());
    DCHECK(!session->title.empty());
    const EntryInfo* new_entry =
        AddNewEntry("",  // auto generate resource id.
                    session->content_type, content_data,
                    session->parent_resource_id, session->title,
                    false);  // shared_with_me
    if (!new_entry) {
      std::move(completion_callback)
          .Run(HTTP_NOT_FOUND, std::unique_ptr<FileResource>());
      return CancelCallbackOnce();
    }

    std::move(completion_callback)
        .Run(HTTP_CREATED, std::make_unique<FileResource>(
                               *new_entry->change_resource.file()));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&FakeDriveService::NotifyObservers,
                                  weak_ptr_factory_.GetWeakPtr()));
    return CancelCallbackOnce();
  }

  EntryInfo* entry = FindEntryByResourceId(session->resource_id);
  if (!entry) {
    std::move(completion_callback)
        .Run(HTTP_NOT_FOUND, std::unique_ptr<FileResource>());
    return CancelCallbackOnce();
  }

  ChangeResource* change = &entry->change_resource;
  FileResource* file = change->mutable_file();
  if (file->etag().empty() || session->etag != file->etag()) {
    std::move(completion_callback)
        .Run(HTTP_PRECONDITION, std::unique_ptr<FileResource>());
    return CancelCallbackOnce();
  }

  file->set_md5_checksum(base::MD5String(content_data));
  entry->content_data = content_data;
  file->set_file_size(end_position);
  AddNewChangestamp(change, file->team_drive_id());
  UpdateETag(file);

  std::move(completion_callback)
      .Run(HTTP_SUCCESS, std::make_unique<FileResource>(*file));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FakeDriveService::NotifyObservers,
                                weak_ptr_factory_.GetWeakPtr()));
  return CancelCallbackOnce();
}

CancelCallbackOnce FakeDriveService::MultipartUploadNewFile(
    const std::string& content_type,
    std::optional<std::string_view> converted_mime_type,
    int64_t content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    const base::FilePath& local_file_path,
    const UploadNewFileOptions& options,
    FileResourceCallback callback,
    ProgressCallback progress_callback) {
  std::string destination_mime_type(converted_mime_type.value_or(content_type));

  CallResumeUpload* const call_resume_upload = new CallResumeUpload();
  call_resume_upload->service = weak_ptr_factory_.GetWeakPtr();
  call_resume_upload->content_type = destination_mime_type;
  call_resume_upload->content_length = content_length;
  call_resume_upload->local_file_path = local_file_path;
  call_resume_upload->callback = std::move(callback);
  call_resume_upload->progress_callback = progress_callback;
  InitiateUploadNewFile(
      destination_mime_type, content_length, parent_resource_id, title, options,
      base::BindOnce(&CallResumeUpload::Run, base::Owned(call_resume_upload)));
  return CancelCallbackOnce();
}

CancelCallbackOnce FakeDriveService::MultipartUploadExistingFile(
    const std::string& content_type,
    int64_t content_length,
    const std::string& resource_id,
    const base::FilePath& local_file_path,
    const UploadExistingFileOptions& options,
    FileResourceCallback callback,
    ProgressCallback progress_callback) {
  CallResumeUpload* const call_resume_upload = new CallResumeUpload();
  call_resume_upload->service = weak_ptr_factory_.GetWeakPtr();
  call_resume_upload->content_type = content_type;
  call_resume_upload->content_length = content_length;
  call_resume_upload->local_file_path = local_file_path;
  call_resume_upload->callback = std::move(callback);
  call_resume_upload->progress_callback = progress_callback;
  InitiateUploadExistingFile(
      content_type, content_length, resource_id, options,
      base::BindOnce(&CallResumeUpload::Run, base::Owned(call_resume_upload)));
  return CancelCallbackOnce();
}

void FakeDriveService::AddNewFile(const std::string& content_type,
                                  const std::string& content_data,
                                  const std::string& parent_resource_id,
                                  const std::string& title,
                                  bool shared_with_me,
                                  FileResourceCallback callback) {
  AddNewFileWithResourceId("", content_type, content_data, parent_resource_id,
                           title, shared_with_me, std::move(callback));
}

void FakeDriveService::AddNewFileWithResourceId(
    const std::string& resource_id,
    const std::string& content_type,
    const std::string& content_data,
    const std::string& parent_resource_id,
    const std::string& title,
    bool shared_with_me,
    FileResourceCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);

  if (offline_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), NO_CONNECTION,
                                  std::unique_ptr<FileResource>()));
    return;
  }

  const EntryInfo* new_entry =
      AddNewEntry(resource_id, content_type, content_data, parent_resource_id,
                  title, shared_with_me);
  if (!new_entry) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HTTP_NOT_FOUND,
                                  std::unique_ptr<FileResource>()));
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), HTTP_CREATED,
                                std::make_unique<FileResource>(
                                    *new_entry->change_resource.file())));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FakeDriveService::NotifyObservers,
                                weak_ptr_factory_.GetWeakPtr()));
}

CancelCallbackOnce FakeDriveService::AddNewDirectoryWithResourceId(
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& directory_title,
    const AddNewDirectoryOptions& options,
    FileResourceCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);

  if (offline_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), NO_CONNECTION,
                                  std::unique_ptr<FileResource>()));
    return CancelCallbackOnce();
  }

  const EntryInfo* new_entry =
      AddNewEntry(resource_id, util::kDriveFolderMimeType,
                  "",  // content_data
                  parent_resource_id, directory_title,
                  false);  // shared_with_me
  if (!new_entry) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HTTP_NOT_FOUND,
                                  std::unique_ptr<FileResource>()));
    return CancelCallbackOnce();
  }

  const google_apis::ApiErrorCode result = SetFileVisibility(
      new_entry->change_resource.file_id(), options.visibility);
  DCHECK_EQ(HTTP_SUCCESS, result);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), HTTP_CREATED,
                                std::make_unique<FileResource>(
                                    *new_entry->change_resource.file())));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FakeDriveService::NotifyObservers,
                                weak_ptr_factory_.GetWeakPtr()));
  return CancelCallbackOnce();
}

void FakeDriveService::SetLastModifiedTime(const std::string& resource_id,
                                           const base::Time& last_modified_time,
                                           FileResourceCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);

  if (offline_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), NO_CONNECTION,
                                  std::unique_ptr<FileResource>()));
    return;
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (!entry) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HTTP_NOT_FOUND,
                                  std::unique_ptr<FileResource>()));
    return;
  }

  ChangeResource* change = &entry->change_resource;
  FileResource* file = change->mutable_file();
  file->set_modified_date(last_modified_time);
  file->set_modified_by_me_date(last_modified_time);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), HTTP_SUCCESS,
                                std::make_unique<FileResource>(*file)));
}

void FakeDriveService::SetFileCapabilities(
    const std::string& resource_id,
    const google_apis::FileResourceCapabilities& capabilities,
    FileResourceCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);

  if (offline_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), NO_CONNECTION,
                                  std::unique_ptr<FileResource>()));
    return;
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (!entry) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HTTP_NOT_FOUND,
                                  std::unique_ptr<FileResource>()));
    return;
  }

  ChangeResource& change = entry->change_resource;
  FileResource* file = change.mutable_file();
  file->set_capabilities(capabilities);

  AddNewChangestamp(&change, file->team_drive_id());

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), HTTP_SUCCESS,
                                std::make_unique<FileResource>(*file)));
}

bool FakeDriveService::SetTeamDriveCapabilities(
    const std::string& team_drive_id,
    const google_apis::TeamDriveCapabilities& capabilities) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (offline_) {
    return false;
  }

  EntryInfo* entry = FindEntryByResourceId(team_drive_id);
  if (!entry) {
    return false;
  }

  for (size_t i = 0; i < team_drive_value_.size(); ++i) {
    std::unique_ptr<TeamDriveResource> team_drive(new TeamDriveResource);
    if (team_drive_value_[i]->id() == team_drive_id) {
      team_drive_value_[i]->set_capabilities(capabilities);
    }
  }

  ChangeResource& change = entry->change_resource;
  DCHECK_EQ(ChangeResource::TEAM_DRIVE, change.type());
  TeamDriveResource* team_drive = change.mutable_team_drive();
  team_drive->set_capabilities(capabilities);

  // Changes to Team Drives are added to the default changelist.
  AddNewChangestamp(&change, std::string());
  return true;
}

google_apis::ApiErrorCode FakeDriveService::SetUserPermission(
    const std::string& resource_id,
    google_apis::drive::PermissionRole user_permission) {
  DCHECK(thread_checker_.CalledOnValidThread());

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (!entry)
    return HTTP_NOT_FOUND;

  entry->user_permission = user_permission;
  return HTTP_SUCCESS;
}

google_apis::ApiErrorCode FakeDriveService::SetFileVisibility(
    const std::string& resource_id,
    google_apis::drive::FileVisibility visibility) {
  DCHECK(thread_checker_.CalledOnValidThread());

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (!entry)
    return HTTP_NOT_FOUND;

  entry->visibility = visibility;
  return HTTP_SUCCESS;
}

google_apis::ApiErrorCode FakeDriveService::GetFileVisibility(
    const std::string& resource_id,
    google_apis::drive::FileVisibility* visibility) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(visibility);

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (!entry)
    return HTTP_NOT_FOUND;

  *visibility = entry->visibility;
  return HTTP_SUCCESS;
}

google_apis::ApiErrorCode FakeDriveService::SetFileAsSharedWithMe(
    const std::string& resource_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (!entry)
    return HTTP_NOT_FOUND;

  entry->change_resource.mutable_file()->set_shared_with_me_date(
      base::Time::Now());
  return HTTP_SUCCESS;
}

void FakeDriveService::AddChangeObserver(ChangeObserver* change_observer) {
  change_observers_.AddObserver(change_observer);
}

void FakeDriveService::RemoveChangeObserver(ChangeObserver* change_observer) {
  change_observers_.RemoveObserver(change_observer);
}

FakeDriveService::EntryInfo* FakeDriveService::FindEntryByResourceId(
    const std::string& resource_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto it = entries_.find(resource_id);
  if (it == entries_.end()) {
    return nullptr;
  }

  // Deleted entries don't have FileResource.
  if (it->second->change_resource.type() != ChangeResource::TEAM_DRIVE &&
      !it->second->change_resource.file()) {
    return nullptr;
  }

  return it->second.get();
}

std::string FakeDriveService::GetNewResourceId() {
  DCHECK(thread_checker_.CalledOnValidThread());

  ++resource_id_count_;
  return base::StringPrintf("resource_id_%d", resource_id_count_);
}

void FakeDriveService::UpdateETag(google_apis::FileResource* file) {
  file->set_etag("etag_" + start_page_token_->start_page_token());
}

void FakeDriveService::AddNewChangestamp(google_apis::ChangeResource* change,
                                         const std::string& team_drive_id) {
  ++largest_changestamp_;
  UpdateLatestChangelistId(largest_changestamp_, team_drive_id);
  change->set_change_id(largest_changestamp_);
}

const FakeDriveService::EntryInfo* FakeDriveService::AddNewEntry(
    const std::string& given_resource_id,
    const std::string& content_type,
    const std::string& content_data,
    const std::string& parent_resource_id,
    const std::string& title,
    bool shared_with_me) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!parent_resource_id.empty() &&
      parent_resource_id != GetRootResourceId() &&
      !entries_.count(parent_resource_id)) {
    return nullptr;
  }

  // Extract the team drive id from the parent, if there is one.
  std::string team_drive_id;
  if (!parent_resource_id.empty() && entries_.count(parent_resource_id) > 0) {
    const ChangeResource& resource =
        entries_[parent_resource_id]->change_resource;
    team_drive_id = GetTeamDriveId(resource);
  }

  const std::string resource_id =
      given_resource_id.empty() ? GetNewResourceId() : given_resource_id;
  if (entries_.count(resource_id))
    return nullptr;
  GURL upload_url = GURL("https://xxx/upload/" + resource_id);

  std::unique_ptr<EntryInfo> new_entry = std::make_unique<EntryInfo>();

  ChangeResource* new_change = &new_entry->change_resource;
  new_change->set_type(ChangeResource::FILE);
  new_change->set_team_drive_id(team_drive_id);

  FileResource* new_file = new FileResource;
  new_change->set_file(base::WrapUnique(new_file));

  // Set the resource ID and the title
  new_change->set_file_id(resource_id);
  new_file->set_file_id(resource_id);
  new_file->set_title(title);
  // Set the contents, size and MD5 for a file.
  if (content_type != util::kDriveFolderMimeType &&
      !util::IsKnownHostedDocumentMimeType(content_type)) {
    new_entry->content_data = content_data;
    new_file->set_file_size(content_data.size());
    new_file->set_md5_checksum(base::MD5String(content_data));
  }

  if (shared_with_me) {
    // Set current time to mark the file as shared_with_me.
    new_file->set_shared_with_me_date(base::Time::Now());
  }

  std::string escaped_resource_id = base::EscapePath(resource_id);

  // Set mime type.
  new_file->set_mime_type(content_type);

  // Set the team drive id.
  new_file->set_team_drive_id(team_drive_id);

  // Set alternate link.
  if (content_type == util::kGoogleDocumentMimeType) {
    new_file->set_alternate_link(
        GURL("https://document_alternate_link/" + title));
  } else if (content_type == util::kDriveFolderMimeType) {
    new_file->set_alternate_link(
        GURL("https://folder_alternate_link/" + title));
  } else {
    new_file->set_alternate_link(GURL("https://file_alternate_link/" + title));
  }

  // Set parents.
  if (!parent_resource_id.empty()) {
    ParentReference parent;
    parent.set_file_id(parent_resource_id);
    std::vector<ParentReference> parents;
    parents.push_back(parent);
    *new_file->mutable_parents() = parents;
  }

  AddNewChangestamp(new_change, team_drive_id);
  UpdateETag(new_file);

  new_file->set_created_date(base::Time() + base::Milliseconds(++date_seq_));
  new_file->set_modified_by_me_date(base::Time() +
                                    base::Milliseconds(++date_seq_));
  new_file->set_modified_date(base::Time() + base::Milliseconds(++date_seq_));

  EntryInfo* raw_new_entry = new_entry.get();
  entries_[resource_id] = std::move(new_entry);
  return raw_new_entry;
}

const FakeDriveService::EntryInfo* FakeDriveService::AddNewTeamDriveEntry(
    const std::string& team_drive_id,
    const std::string& team_drive_name) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Check we do not already have this entry.
  if (entries_.count(team_drive_id) > 0) {
    return nullptr;
  }

  std::unique_ptr<EntryInfo> new_entry = std::make_unique<EntryInfo>();

  ChangeResource& change = new_entry->change_resource;
  change.set_type(ChangeResource::TEAM_DRIVE);
  change.set_team_drive_id(team_drive_id);

  std::unique_ptr<TeamDriveResource> team_drive =
      std::make_unique<TeamDriveResource>();
  team_drive->set_id(team_drive_id);
  team_drive->set_name(team_drive_name);
  change.set_team_drive(std::move(team_drive));

  AddNewChangestamp(&change, std::string());

  change.set_modification_date(base::Time() + base::Milliseconds(++date_seq_));

  EntryInfo* raw_new_entry = new_entry.get();
  entries_[team_drive_id] = std::move(new_entry);
  return raw_new_entry;
}

void FakeDriveService::GetChangeListInternal(
    int64_t start_changestamp,
    const std::string& search_query,
    const std::string& directory_resource_id,
    const std::string& team_drive_id,
    int start_offset,
    int max_results,
    int* load_counter,
    ChangeListOnceCallback callback) {
  if (offline_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), NO_CONNECTION,
                                  std::unique_ptr<ChangeList>()));
    return;
  }

  // Filter out entries per parameters like |directory_resource_id| and
  // |search_query|.
  std::vector<std::unique_ptr<ChangeResource>> entries;
  int num_entries_matched = 0;
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    const ChangeResource& entry = it->second->change_resource;
    bool should_exclude = false;

    // If |directory_resource_id| is set, exclude the entry if it's not in
    // the target directory.
    if (!directory_resource_id.empty()) {
      // Get the parent resource ID of the entry.
      std::string parent_resource_id;
      if (entry.type() == ChangeResource::FILE && entry.file() &&
          !entry.file()->parents().empty())
        parent_resource_id = entry.file()->parents()[0].file_id();

      if (directory_resource_id != parent_resource_id)
        should_exclude = true;
    }

    // If |search_query| is set, exclude the entry if it does not contain the
    // search query in the title.
    if (!should_exclude && !search_query.empty() &&
        !EntryMatchWithQuery(entry, search_query)) {
      should_exclude = true;
    }

    // If the team drive does not match, then exclude the entry.
    switch (entry.type()) {
      case ChangeResource::FILE:
        if (entry.file() && entry.file()->team_drive_id() != team_drive_id) {
          should_exclude = true;
        }
        break;
      case ChangeResource::TEAM_DRIVE:
        // Only include TeamDrive change resources in the default change list.
        if (!team_drive_id.empty()) {
          should_exclude = true;
        }
        break;
      case ChangeResource::UNKNOWN:
        NOTREACHED_IN_MIGRATION();
        break;
    }

    // If |start_changestamp| is set, exclude the entry if the
    // changestamp is older than |largest_changestamp|.
    // See https://developers.google.com/google-apps/documents-list/
    // #retrieving_all_changes_since_a_given_changestamp
    if (start_changestamp > 0 && entry.change_id() < start_changestamp)
      should_exclude = true;

    // If the caller requests other list than change list by specifying
    // zero-|start_changestamp|, exclude deleted entry from the result.
    const bool deleted = entry.is_deleted() ||
                         (entry.type() == ChangeResource::FILE &&
                          entry.file() && entry.file()->labels().is_trashed());
    if (!start_changestamp && deleted)
      should_exclude = true;

    // The entry matched the criteria for inclusion.
    if (!should_exclude)
      ++num_entries_matched;

    // If |start_offset| is set, exclude the entry if the entry is before the
    // start index. <= instead of < as |num_entries_matched| was
    // already incremented.
    if (start_offset > 0 && num_entries_matched <= start_offset)
      should_exclude = true;

    if (!should_exclude) {
      std::unique_ptr<ChangeResource> entry_copied(new ChangeResource);
      entry_copied->set_type(entry.type());
      entry_copied->set_change_id(entry.change_id());
      entry_copied->set_deleted(entry.is_deleted());
      if (entry.type() == ChangeResource::FILE) {
        entry_copied->set_file_id(entry.file_id());
        if (entry.file()) {
          entry_copied->set_file(std::make_unique<FileResource>(*entry.file()));
        }
      }
      if (entry.type() == ChangeResource::TEAM_DRIVE && entry.team_drive()) {
        entry_copied->set_team_drive(
            std::make_unique<TeamDriveResource>(*entry.team_drive()));
      }
      entry_copied->set_modification_date(entry.modification_date());
      entries.push_back(std::move(entry_copied));
    }
  }

  std::unique_ptr<ChangeList> change_list(new ChangeList);
  if (start_changestamp > 0 && start_offset == 0) {
    auto largest_change_id = about_resource_->largest_change_id();
    change_list->set_largest_change_id(largest_change_id);
    change_list->set_new_start_page_token(
        drive::util::ConvertChangestampToStartPageToken(largest_change_id));
  }

  // If |max_results| is set, trim the entries if the number exceeded the max
  // results.
  if (max_results > 0 && entries.size() > static_cast<size_t>(max_results)) {
    entries.erase(entries.begin() + max_results, entries.end());
    // Adds the next URL.
    // Here, we embed information which is needed for continuing the
    // GetChangeList request in the next invocation into url query
    // parameters.
    GURL next_url(
        base::StringPrintf("http://localhost/?start-offset=%d&max-results=%d",
                           start_offset + max_results, max_results));
    if (start_changestamp > 0) {
      next_url = net::AppendOrReplaceQueryParameter(
          next_url, "changestamp", base::NumberToString(start_changestamp));
    }
    if (!search_query.empty()) {
      next_url =
          net::AppendOrReplaceQueryParameter(next_url, "q", search_query);
    }
    if (!directory_resource_id.empty()) {
      next_url = net::AppendOrReplaceQueryParameter(next_url, "parent",
                                                    directory_resource_id);
    }
    if (!team_drive_id.empty()) {
      next_url = net::AppendOrReplaceQueryParameter(next_url, "team-drive-id",
                                                    team_drive_id);
    }

    change_list->set_next_link(next_url);
  }
  *change_list->mutable_items() = std::move(entries);

  if (load_counter)
    *load_counter += 1;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), HTTP_SUCCESS,
                                std::move(change_list)));
}

GURL FakeDriveService::GetNewUploadSessionUrl() {
  return GURL("https://upload_session_url/" +
              base::NumberToString(next_upload_sequence_number_++));
}

CancelCallbackOnce FakeDriveService::AddPermission(
    const std::string& resource_id,
    const std::string& email,
    google_apis::drive::PermissionRole role,
    google_apis::EntryActionCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);

  NOTREACHED_IN_MIGRATION();
  return CancelCallbackOnce();
}

std::unique_ptr<BatchRequestConfiguratorInterface>
FakeDriveService::StartBatchRequest() {
  DCHECK(thread_checker_.CalledOnValidThread());

  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void FakeDriveService::NotifyObservers() {
  for (auto& observer : change_observers_)
    observer.OnNewChangeAvailable();
}

void FakeDriveService::UpdateLatestChangelistId(
    int64_t change_list_id,
    const std::string& team_drive_id) {
  if (team_drive_id.empty()) {
    about_resource_->set_largest_change_id(change_list_id);
    start_page_token_->set_start_page_token(
        drive::util::ConvertChangestampToStartPageToken(change_list_id));
  } else {
    DCHECK_GT(team_drive_start_page_tokens_.count(team_drive_id), 0UL);
    team_drive_start_page_tokens_[team_drive_id]->set_start_page_token(
        drive::util::ConvertChangestampToStartPageToken(change_list_id));
  }
}

}  // namespace drive
