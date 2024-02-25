// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_STATUS_H_
#define COMPONENTS_INVALIDATION_IMPL_STATUS_H_

#include <string>

namespace invalidation {

// Status of the message arrived from FCM.
// Used by UMA histogram, so entries shouldn't be reordered or removed.
enum class InvalidationParsingStatus {
  kSuccess = 0,
  kPublicTopicEmpty = 1,
  kPrivateTopicEmpty = 2,
  kVersionEmpty = 3,
  kVersionInvalid = 4,
  kMaxValue = kVersionInvalid,
};

// This enum indicates how an operation was completed. These values are written
// to logs.  New enum values can be added, but existing enums must never be
// renumbered or deleted and reused.
enum class StatusCode {
  // The operation has been completed successfully.
  SUCCESS = 0,
  // Failed with HTTP 401.
  AUTH_FAILURE = 1,
  // The operation failed.
  FAILED = 2,
  // Something is terribly wrong and we shouldn't retry the requests until
  // next startup.
  FAILED_NON_RETRIABLE = 3,
};

// This struct provides the status code of a request and an optional message
// describing the status (esp. failures) in detail.
struct Status {
  Status(StatusCode status_code, const std::string& message);
  ~Status();

  friend bool operator==(const Status& lhs, const Status& rhs) = default;
  friend auto operator<=>(const Status& lhs, const Status& rhs) = default;

  // Errors always need a message but a success does not.
  static Status Success();

  bool IsSuccess() const { return code == StatusCode::SUCCESS; }
  bool IsAuthFailure() const { return code == StatusCode::AUTH_FAILURE; }
  bool ShouldRetry() const { return code == StatusCode::FAILED; }

  StatusCode code;
  // The message is not meant to be displayed to the user.
  std::string message;

  // Copy and assignment allowed.
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_STATUS_H_
