// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_types.h"

namespace offline_pages {

namespace {
std::string PrefetchEnumToString(PrefetchBackgroundTaskRescheduleType value) {
  switch (value) {
    case PrefetchBackgroundTaskRescheduleType::NO_RESCHEDULE:
      return "NO_RESCHEDULE";
    case PrefetchBackgroundTaskRescheduleType::RESCHEDULE_WITHOUT_BACKOFF:
      return "RESCHEDULE_WITHOUT_BACKOFF";
    case PrefetchBackgroundTaskRescheduleType::RESCHEDULE_WITH_BACKOFF:
      return "RESCHEDULE_WITH_BACKOFF";
    case PrefetchBackgroundTaskRescheduleType::RESCHEDULE_DUE_TO_SYSTEM:
      return "RESCHEDULE_DUE_TO_SYSTEM";
    case PrefetchBackgroundTaskRescheduleType::SUSPEND:
      return "SUSPEND";
  }
  DCHECK(false) << static_cast<int>(value) << " not valid enum value";
}

std::string PrefetchEnumToString(PrefetchRequestStatus value) {
  switch (value) {
    case PrefetchRequestStatus::kSuccess:
      return "SUCCESS";
    case PrefetchRequestStatus::kShouldRetryWithoutBackoff:
      return "SHOULD_RETRY_WITHOUT_BACKOFF";
    case PrefetchRequestStatus::kShouldRetryWithBackoff:
      return "SHOULD_RETRY_WITH_BACKOFF";
    case PrefetchRequestStatus::kShouldSuspendNotImplemented:
      return "SHOULD_SUSPEND_NOT_IMPLEMENTED";
    case PrefetchRequestStatus::kShouldSuspendForbidden:
      return "SHOULD_SUSPEND_FORBIDDEN";
    case PrefetchRequestStatus::kShouldSuspendBlockedByAdministrator:
      return "SHOULD_SUSPEND_BLOCKED_BY_ADMINISTRATOR";
    case PrefetchRequestStatus::kShouldSuspendForbiddenByOPS:
      return "SHOULD_SUSPEND_FORBIDDEN_BY_OPS";
    case PrefetchRequestStatus::kShouldSuspendNewlyForbiddenByOPS:
      return "SHOULD_SUSPEND_NEWLY_FORBIDDEN_BY_OPS";
    case PrefetchRequestStatus::kEmptyRequestSuccess:
      return "EMPTY_REQUEST_SUCCESS";
  }
  DCHECK(false) << static_cast<int>(value) << " not valid enum value";
}

std::string PrefetchEnumToString(RenderStatus value) {
  switch (value) {
    case RenderStatus::RENDERED:
      return "RENDERED";
    case RenderStatus::PENDING:
      return "PENDING";
    case RenderStatus::FAILED:
      return "FAILED";
    case RenderStatus::EXCEEDED_LIMIT:
      return "EXCEEDED_LIMIT";
  }
  DCHECK(false) << static_cast<int>(value) << " not valid enum value";
}

std::string PrefetchEnumToString(PrefetchItemState value) {
  switch (value) {
    case PrefetchItemState::NEW_REQUEST:
      return "NEW_REQUEST";
    case PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE:
      return "SENT_GENERATE_PAGE_BUNDLE";
    case PrefetchItemState::AWAITING_GCM:
      return "AWAITING_GCM";
    case PrefetchItemState::RECEIVED_GCM:
      return "RECEIVED_GCM";
    case PrefetchItemState::SENT_GET_OPERATION:
      return "SENT_GET_OPERATION";
    case PrefetchItemState::RECEIVED_BUNDLE:
      return "RECEIVED_BUNDLE";
    case PrefetchItemState::DOWNLOADING:
      return "DOWNLOADING";
    case PrefetchItemState::DOWNLOADED:
      return "DOWNLOADED";
    case PrefetchItemState::IMPORTING:
      return "IMPORTING";
    case PrefetchItemState::FINISHED:
      return "FINISHED";
    case PrefetchItemState::ZOMBIE:
      return "ZOMBIE";
  }
  DCHECK(false) << static_cast<int>(value) << " not valid enum value";
}

std::string PrefetchEnumToString(PrefetchItemErrorCode value) {
  switch (value) {
    case PrefetchItemErrorCode::SUCCESS:
      return "SUCCESS";
    case PrefetchItemErrorCode::TOO_MANY_NEW_URLS:
      return "TOO_MANY_NEW_URLS";
    case PrefetchItemErrorCode::DOWNLOAD_ERROR:
      return "DOWNLOAD_ERROR";
    case PrefetchItemErrorCode::IMPORT_ERROR:
      return "IMPORT_ERROR";
    case PrefetchItemErrorCode::ARCHIVING_FAILED:
      return "ARCHIVING_FAILED";
    case PrefetchItemErrorCode::ARCHIVING_LIMIT_EXCEEDED:
      return "ARCHIVING_LIMIT_EXCEEDED";
    case PrefetchItemErrorCode::STALE_AT_NEW_REQUEST:
      return "STALE_AT_NEW_REQUEST";
    case PrefetchItemErrorCode::STALE_AT_AWAITING_GCM:
      return "STALE_AT_AWAITING_GCM";
    case PrefetchItemErrorCode::STALE_AT_RECEIVED_GCM:
      return "STALE_AT_RECEIVED_GCM";
    case PrefetchItemErrorCode::STALE_AT_RECEIVED_BUNDLE:
      return "STALE_AT_RECEIVED_BUNDLE";
    case PrefetchItemErrorCode::STALE_AT_DOWNLOADING:
      return "STALE_AT_DOWNLOADING";
    case PrefetchItemErrorCode::STALE_AT_IMPORTING:
      return "STALE_AT_IMPORTING";
    case PrefetchItemErrorCode::STALE_AT_UNKNOWN:
      return "STALE_AT_UNKNOWN";
    case PrefetchItemErrorCode::STUCK:
      return "STUCK";
    case PrefetchItemErrorCode::INVALID_ITEM:
      return "INVALID_ITEM";
    case PrefetchItemErrorCode::GET_OPERATION_MAX_ATTEMPTS_REACHED:
      return "GET_OPERATION_MAX_ATTEMPTS_REACHED";
    case PrefetchItemErrorCode::
        GENERATE_PAGE_BUNDLE_REQUEST_MAX_ATTEMPTS_REACHED:
      return "GENERATE_PAGE_BUNDLE_REQUEST_MAX_ATTEMPTS_REACHED";
    case PrefetchItemErrorCode::DOWNLOAD_MAX_ATTEMPTS_REACHED:
      return "DOWNLOAD_MAX_ATTEMPTS_REACHED";
    case PrefetchItemErrorCode::MAXIMUM_CLOCK_BACKWARD_SKEW_EXCEEDED:
      return "MAXIMUM_CLOCK_BACKWARD_SKEW_EXCEEDED";
    case PrefetchItemErrorCode::IMPORT_LOST:
      return "IMPORT_LOST";
    case PrefetchItemErrorCode::SUGGESTION_INVALIDATED:
      return "SUGGESTION_INVALIDATED";
  }
  DCHECK(false) << static_cast<int>(value) << " not valid enum value";
}
}  // namespace

RenderPageInfo::RenderPageInfo() = default;

RenderPageInfo::RenderPageInfo(const RenderPageInfo& other) = default;

PrefetchURL::PrefetchURL(const std::string& id,
                         const GURL& url,
                         const base::string16& title)
    : id(id), url(url), title(title) {}

PrefetchURL::PrefetchURL(const std::string& id,
                         const GURL& url,
                         const base::string16& title,
                         const GURL& thumbnail_url,
                         const GURL& favicon_url,
                         const std::string& snippet,
                         const std::string& attribution)
    : id(id),
      url(url),
      title(title),
      thumbnail_url(thumbnail_url),
      favicon_url(favicon_url),
      snippet(snippet),
      attribution(attribution) {}

PrefetchURL::~PrefetchURL() = default;

PrefetchURL::PrefetchURL(const PrefetchURL& other) = default;

PrefetchDownloadResult::PrefetchDownloadResult() = default;

PrefetchDownloadResult::PrefetchDownloadResult(const std::string& download_id,
                                               const base::FilePath& file_path,
                                               int64_t file_size)
    : download_id(download_id),
      success(true),
      file_path(file_path),
      file_size(file_size) {}

PrefetchDownloadResult::PrefetchDownloadResult(
    const PrefetchDownloadResult& other) = default;

bool PrefetchDownloadResult::operator==(
    const PrefetchDownloadResult& other) const {
  return download_id == other.download_id && success == other.success &&
         file_path == other.file_path && file_size == other.file_size;
}

PrefetchArchiveInfo::PrefetchArchiveInfo() = default;

PrefetchArchiveInfo::PrefetchArchiveInfo(const PrefetchArchiveInfo& other) =
    default;

PrefetchArchiveInfo::~PrefetchArchiveInfo() = default;

bool PrefetchArchiveInfo::empty() const {
  return offline_id == 0;
}

base::Optional<PrefetchItemState> ToPrefetchItemState(int value) {
  switch (static_cast<PrefetchItemState>(value)) {
    case PrefetchItemState::NEW_REQUEST:
    case PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE:
    case PrefetchItemState::AWAITING_GCM:
    case PrefetchItemState::RECEIVED_GCM:
    case PrefetchItemState::SENT_GET_OPERATION:
    case PrefetchItemState::RECEIVED_BUNDLE:
    case PrefetchItemState::DOWNLOADING:
    case PrefetchItemState::DOWNLOADED:
    case PrefetchItemState::IMPORTING:
    case PrefetchItemState::FINISHED:
    case PrefetchItemState::ZOMBIE:
      return static_cast<PrefetchItemState>(value);
  }
  return base::nullopt;
}

base::Optional<PrefetchItemErrorCode> ToPrefetchItemErrorCode(int value) {
  switch (static_cast<PrefetchItemErrorCode>(value)) {
    case PrefetchItemErrorCode::SUCCESS:
    case PrefetchItemErrorCode::TOO_MANY_NEW_URLS:
    case PrefetchItemErrorCode::DOWNLOAD_ERROR:
    case PrefetchItemErrorCode::IMPORT_ERROR:
    case PrefetchItemErrorCode::ARCHIVING_FAILED:
    case PrefetchItemErrorCode::ARCHIVING_LIMIT_EXCEEDED:
    case PrefetchItemErrorCode::STALE_AT_NEW_REQUEST:
    case PrefetchItemErrorCode::STALE_AT_AWAITING_GCM:
    case PrefetchItemErrorCode::STALE_AT_RECEIVED_GCM:
    case PrefetchItemErrorCode::STALE_AT_RECEIVED_BUNDLE:
    case PrefetchItemErrorCode::STALE_AT_DOWNLOADING:
    case PrefetchItemErrorCode::STALE_AT_IMPORTING:
    case PrefetchItemErrorCode::STALE_AT_UNKNOWN:
    case PrefetchItemErrorCode::STUCK:
    case PrefetchItemErrorCode::INVALID_ITEM:
    case PrefetchItemErrorCode::GET_OPERATION_MAX_ATTEMPTS_REACHED:
    case PrefetchItemErrorCode::
        GENERATE_PAGE_BUNDLE_REQUEST_MAX_ATTEMPTS_REACHED:
    case PrefetchItemErrorCode::DOWNLOAD_MAX_ATTEMPTS_REACHED:
    case PrefetchItemErrorCode::MAXIMUM_CLOCK_BACKWARD_SKEW_EXCEEDED:
    case PrefetchItemErrorCode::IMPORT_LOST:
    case PrefetchItemErrorCode::SUGGESTION_INVALIDATED:
      return static_cast<PrefetchItemErrorCode>(value);
  }
  return base::nullopt;
}

std::ostream& operator<<(std::ostream& out,
                         PrefetchBackgroundTaskRescheduleType value) {
  return out << PrefetchEnumToString(value);
}
std::ostream& operator<<(std::ostream& out, PrefetchRequestStatus value) {
  return out << PrefetchEnumToString(value);
}
std::ostream& operator<<(std::ostream& out, RenderStatus value) {
  return out << PrefetchEnumToString(value);
}
std::ostream& operator<<(std::ostream& out, const PrefetchItemState& value) {
  return out << PrefetchEnumToString(value);
}
std::ostream& operator<<(std::ostream& out, PrefetchItemErrorCode value) {
  return out << PrefetchEnumToString(value);
}

}  // namespace offline_pages
