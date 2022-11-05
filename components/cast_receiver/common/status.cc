// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/common/public/status.h"

namespace cast_receiver {
std::string StatusCodeToString(StatusCode code) {
  switch (code) {
    case StatusCode::kOk:
      return "OK";
    case StatusCode::kCancelled:
      return "CANCELLED";
    case StatusCode::kUnknown:
      return "UNKNOWN";
    case StatusCode::kInvalidArgument:
      return "INVALID_ARGUMENT";
    case StatusCode::kDeadlineExceeded:
      return "DEADLINE_EXCEEDED";
    case StatusCode::kNotFound:
      return "NOT_FOUND";
    case StatusCode::kAlreadyExists:
      return "ALREADY_EXISTS";
    case StatusCode::kPermissionDenied:
      return "PERMISSION_DENIED";
    case StatusCode::kUnauthenticated:
      return "UNAUTHENTICATED";
    case StatusCode::kResourceExhausted:
      return "RESOURCE_EXHAUSTED";
    case StatusCode::kFailedPrecondition:
      return "FAILED_PRECONDITION";
    case StatusCode::kAborted:
      return "ABORTED";
    case StatusCode::kOutOfRange:
      return "OUT_OF_RANGE";
    case StatusCode::kUnimplemented:
      return "UNIMPLEMENTED";
    case StatusCode::kInternal:
      return "INTERNAL";
    case StatusCode::kUnavailable:
      return "UNAVAILABLE";
    case StatusCode::kDataLoss:
      return "DATA_LOSS";
  }
}

Status::Status(StatusCode code) : code_(code) {}

Status::Status(StatusCode code, std::string message)
    : code_(code), message_(message) {}

Status::Status(const Status& x) : code_(x.code_), message_(x.message_) {}

Status::Status(Status&& x) {
  code_ = x.code_;
  message_ = x.message_;
}

Status& Status::operator=(const Status& x) {
  code_ = x.code_;
  message_ = x.message_;
  return *this;
}

Status& Status::operator=(Status&& x) {
  code_ = x.code_;
  message_ = x.message_;
  return *this;
}

void Status::Update(const Status& new_status) {
  if (ok()) {
    *this = new_status;
  }
}

void Status::Update(Status&& new_status) {
  if (ok()) {
    *this = std::move(new_status);
  }
}

std::ostream& operator<<(std::ostream& os, const Status& x) {
  os << StatusCodeToString(x.code_) + ": " + x.message_;
  return os;
}

bool operator==(const Status& lhs, const Status& rhs) {
  return lhs.code_ == rhs.code_;
}

bool operator!=(const Status& lhs, const Status& rhs) {
  return lhs.code_ != rhs.code_;
}

Status OkStatus() {
  return Status(StatusCode::kOk);
}

}  // namespace cast_receiver
