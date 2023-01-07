// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_STATUS_H_
#define COMPONENTS_NTP_SNIPPETS_STATUS_H_

#include <string>

namespace ntp_snippets {

// This enum indicates how an operation was completed. These values are written
// to logs.  New enum values can be added, but existing enums must never be
// renumbered or deleted and reused.
enum class StatusCode {
  // The operation has been completed successfully.
  SUCCESS = 0,
  // The operation failed but retrying might solve the error.
  TEMPORARY_ERROR = 1,
  // The operation failed and would fail again if retried.
  PERMANENT_ERROR = 2,

  STATUS_CODE_COUNT
};

// This struct provides the status code of a request and an optional message
// describing the status (esp. failures) in detail.
struct Status {
  Status(StatusCode status_code, const std::string& message);
  ~Status();

  // Errors always need a message but a success does not.
  static Status Success();

  bool IsSuccess() const { return code == StatusCode::SUCCESS; }

  StatusCode code;
  // The message is not meant to be displayed to the user.
  std::string message;

  // Copy and assignment allowed.
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_STATUS_H_
