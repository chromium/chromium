// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_COMMON_PUBLIC_STATUS_H_
#define COMPONENTS_CAST_RECEIVER_COMMON_PUBLIC_STATUS_H_

#include <ostream>
#include <string>
#include <string_view>

namespace cast_receiver {

// Enumerated types indicating either no error ("OK") or an error condition.
enum class StatusCode : int {
  kOk = 0,
  kCancelled = 1,
  kUnknown = 2,
  kInvalidArgument = 3,
  kDeadlineExceeded = 4,
  kNotFound = 5,
  kAlreadyExists = 6,
  kPermissionDenied = 7,
  kResourceExhausted = 8,
  kFailedPrecondition = 9,
  kAborted = 10,
  kOutOfRange = 11,
  kUnimplemented = 12,
  kInternal = 13,
  kUnavailable = 14,
  kDataLoss = 15,
  kUnauthenticated = 16,
};

// `cast_receiver::Status` is a simplified implementation of `absl::Status`.
// Functions which can produce a recoverable error should be written to return
// a `cast_receiver::Status`.
class Status final {
 public:
  // Creates an OK status with no message, prefer to use
  // cast_receiver::OkStatus() to create
  Status();

  // Creates a status with the specified StatusCode and blank error message.
  explicit Status(StatusCode code);

  // Creates a status with the specified StatusCode and error message.
  Status(StatusCode code, std::string message);

  Status(const Status&);
  Status(Status&& x);
  Status& operator=(const Status& x);
  Status& operator=(Status&& x);

  ~Status() = default;

  // Updates the existing status with `new_status` if `this->ok()`. If the
  // existing status is already non-OK, then this update will have no effect
  // and instead preserves the existing non-OK code and message.
  void Update(const Status& new_status);
  void Update(Status&& new_status);

  // Returns the `cast_receiver::StatusCode` of this status.
  StatusCode code() const;

  // Returns the error message of this status.
  std::string_view message() const;

  // Returns `true` if `this->code() == cast_receiver::StatusCode::kOk`.
  [[nodiscard]] bool ok() const;
  [[nodiscard]] explicit operator bool() const { return ok(); }

  friend std::ostream& operator<<(std::ostream& os, const Status& x);
  friend bool operator==(const Status& lhs, const Status& rhs);
  friend bool operator!=(const Status& lhs, const Status& rhs);

 private:
  StatusCode code_;
  std::string message_;
};

inline Status::Status() : code_(StatusCode::kOk) {}

inline StatusCode Status::code() const {
  return code_;
}

inline std::string_view Status::message() const {
  return message_;
}

inline bool Status::ok() const {
  return code_ == StatusCode::kOk;
}

Status OkStatus();

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_COMMON_PUBLIC_STATUS_H_
