// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/save_page_request.h"

#include <string>

namespace offline_pages {

SavePageRequest::SavePageRequest(int64_t request_id,
                                 const GURL& url,
                                 const ClientId& client_id,
                                 const base::Time& creation_time,
                                 const bool user_requested)
    : request_id_(request_id),
      url_(url),
      client_id_(client_id),
      creation_time_(creation_time),
      started_attempt_count_(0),
      completed_attempt_count_(0),
      user_requested_(user_requested),
      state_(RequestState::AVAILABLE),
      fail_state_(FailState::NO_FAILURE),
      pending_state_(PendingState::NOT_PENDING) {}

SavePageRequest::SavePageRequest(const SavePageRequest& other) = default;

SavePageRequest::~SavePageRequest() = default;

bool SavePageRequest::operator==(const SavePageRequest& other) const {
  return request_id_ == other.request_id_ && url_ == other.url_ &&
         client_id_ == other.client_id_ &&
         creation_time_ == other.creation_time_ &&
         started_attempt_count_ == other.started_attempt_count_ &&
         completed_attempt_count_ == other.completed_attempt_count_ &&
         last_attempt_time_ == other.last_attempt_time_ &&
         state_ == other.state_ && original_url_ == other.original_url_ &&
         request_origin_ == other.request_origin_ &&
         auto_fetch_notification_state_ == other.auto_fetch_notification_state_;
}

void SavePageRequest::MarkAttemptStarted(const base::Time& start_time) {
  last_attempt_time_ = start_time;
  ++started_attempt_count_;
  state_ = RequestState::OFFLINING;
}

void SavePageRequest::MarkAttemptCompleted(FailState fail_state) {
  ++completed_attempt_count_;
  state_ = RequestState::AVAILABLE;
  UpdateFailState(fail_state);
}

void SavePageRequest::MarkAttemptAborted() {
  // We intentinally do not increment the completed_attempt_count_, since this
  // was killed before it completed, so we could use the phone or browser for
  // other things.
  if (state_ == RequestState::OFFLINING) {
    DCHECK_GT(started_attempt_count_, 0);
    state_ = RequestState::AVAILABLE;
  }
}

void SavePageRequest::MarkAttemptPaused() {
  state_ = RequestState::PAUSED;
}

void SavePageRequest::MarkAttemptDeferred(const base::Time& attempt_time) {
  ++started_attempt_count_;
  ++completed_attempt_count_;
  last_attempt_time_ = attempt_time;
  state_ = RequestState::AVAILABLE;
}

void SavePageRequest::UpdateFailState(FailState fail_state) {
  // The order of precedence for failure errors related to offline page
  // downloads is as follows: NO_FAILURE, Failures that are not recoverable and
  // recoverable failures.
  switch (fail_state) {
    case FailState::NO_FAILURE:  // Intentional fallthrough.
    case FailState::CANNOT_DOWNLOAD:
    case FailState::FILE_ACCESS_DENIED:
    case FailState::FILE_NO_SPACE:
    case FailState::FILE_NAME_TOO_LONG:
    case FailState::FILE_TOO_LARGE:
    case FailState::FILE_VIRUS_INFECTED:
    case FailState::FILE_BLOCKED:
    case FailState::FILE_SECURITY_CHECK_FAILED:
    case FailState::FILE_TOO_SHORT:
    case FailState::FILE_SAME_AS_SOURCE:
    case FailState::NETWORK_INVALID_REQUEST:
    case FailState::NETWORK_SERVER_DOWN:
    case FailState::SERVER_FAILED:
    case FailState::SERVER_BAD_CONTENT:
    case FailState::USER_CANCELED:
    case FailState::USER_SHUTDOWN:
    case FailState::CRASH:
    case FailState::SERVER_UNAUTHORIZED:
    case FailState::SERVER_CERT_PROBLEM:
    case FailState::SERVER_FORBIDDEN:
    case FailState::SERVER_UNREACHABLE:
    case FailState::SERVER_CONTENT_LENGTH_MISMATCH:
    case FailState::SERVER_NO_RANGE:
    case FailState::SERVER_CROSS_ORIGIN_REDIRECT:
    case FailState::FILE_FAILED:
    case FailState::FILE_HASH_MISMATCH:
      fail_state_ = fail_state;
      break;
    case FailState::FILE_TRANSIENT_ERROR:  // Intentional fallthrough.
    case FailState::NETWORK_INSTABILITY:
    case FailState::NETWORK_FAILED:
    case FailState::NETWORK_TIMEOUT:
    case FailState::NETWORK_DISCONNECTED:
      if (fail_state_ != FailState::CANNOT_DOWNLOAD) {
        fail_state_ = fail_state;
      }
      break;
  }
}

}  // namespace offline_pages
