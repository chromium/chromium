// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/status.h"

#include <ostream>
#include <string>
#include <utility>

#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_number_conversions_internal.h"
#include "components/reporting/proto/synced/status.pb.h"

namespace reporting {
namespace error {
inline std::string CodeEnumToString(error::Code code) {
  switch (code) {
    case OK:
      return "OK";
    case CANCELLED:
      return "CANCELLED";
    case UNKNOWN:
      return "UNKNOWN";
    case INVALID_ARGUMENT:
      return "INVALID_ARGUMENT";
    case DEADLINE_EXCEEDED:
      return "DEADLINE_EXCEEDED";
    case NOT_FOUND:
      return "NOT_FOUND";
    case ALREADY_EXISTS:
      return "ALREADY_EXISTS";
    case PERMISSION_DENIED:
      return "PERMISSION_DENIED";
    case RESOURCE_EXHAUSTED:
      return "RESOURCE_EXHAUSTED";
    case FAILED_PRECONDITION:
      return "FAILED_PRECONDITION";
    case ABORTED:
      return "ABORTED";
    case OUT_OF_RANGE:
      return "OUT_OF_RANGE";
    case UNIMPLEMENTED:
      return "UNIMPLEMENTED";
    case INTERNAL:
      return "INTERNAL";
    case UNAVAILABLE:
      return "UNAVAILABLE";
    case DATA_LOSS:
      return "DATA_LOSS";
    case UNAUTHENTICATED:
      return "UNAUTHENTICATED";
    default:
      return base::StrCat({"ILLEGAL[", base::NumberToString(code), "]"});
  }
}
}  // namespace error.

const Status& Status::StatusOK() {
  static base::NoDestructor<Status> status_ok;
  return *status_ok;
}

Status::Status() : error_code_(error::OK) {}

Status::Status(const Status&) = default;
Status& Status::operator=(const Status&) = default;
Status::Status(Status&&) = default;
Status& Status::operator=(Status&&) = default;
Status::~Status() = default;

Status::Status(error::Code error_code, std::string error_message)
    : error_code_(error_code),
      error_message_{error_code != error::OK ? std::move(error_message)
                                             : std::string()} {}

bool Status::operator==(const Status& x) const {
  return error_code_ == x.error_code_ && error_message_ == x.error_message_;
}

std::string Status::ToString() const {
  if (error_code_ == error::OK) {
    return "OK";
  }
  auto output = error::CodeEnumToString(error_code_);
  if (!error_message_.empty()) {
    base::StrAppend(&output, {":", error_message_});
  }
  return output;
}

void Status::SaveTo(StatusProto* status_proto) const {
  status_proto->set_code(error_code_);
  if (error_code_ != error::OK) {
    status_proto->set_error_message(error_message_);
  } else {
    status_proto->clear_error_message();
  }
}

void Status::RestoreFrom(const StatusProto& status_proto) {
  error_code_ = static_cast<error::Code>(status_proto.code());
  if (error_code_ != error::OK) {
    error_message_ = status_proto.error_message();
  } else {
    error_message_.clear();
  }
}

std::ostream& operator<<(std::ostream& os, const Status& x) {
  os << x.ToString();
  return os;
}

}  // namespace reporting
