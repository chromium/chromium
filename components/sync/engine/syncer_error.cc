// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/syncer_error.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "components/sync/engine/sync_protocol_error.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"

namespace syncer {

SyncerError::SyncerError(Type type, ValueType value)
    : type_(type), value_(value) {}

// static
SyncerError SyncerError::Success() {
  return SyncerError(Type::kSuccess, SuccessValueType());
}

// static
SyncerError SyncerError::NetworkError(int error_code) {
  return SyncerError(Type::kNetworkError, error_code);
}

// static
SyncerError SyncerError::HttpError(net::HttpStatusCode status_code) {
  return SyncerError(Type::kHttpError, status_code);
}

// static
SyncerError SyncerError::ProtocolError(SyncProtocolErrorType error_type) {
  if (error_type == SyncProtocolErrorType::SYNC_SUCCESS) {
    // Ideally caller should use Success(), but let's fix it here to avoid
    // subtle bugs.
    return Success();
  }
  return SyncerError(Type::kProtocolError, error_type);
}

// static
SyncerError SyncerError::ProtocolViolationError() {
  return SyncerError(Type::kProtocolViolationError,
                     ProtocolViolationValueType());
}

int SyncerError::GetNetworkErrorOrDie() const {
  CHECK_EQ(type_, Type::kNetworkError);
  return absl::get<int>(value_);
}

net::HttpStatusCode SyncerError::GetHttpErrorOrDie() const {
  CHECK_EQ(type_, Type::kHttpError);
  return absl::get<net::HttpStatusCode>(value_);
}

SyncProtocolErrorType SyncerError::GetProtocolErrorOrDie() const {
  CHECK_EQ(type_, Type::kProtocolError);
  return absl::get<SyncProtocolErrorType>(value_);
}

std::string SyncerError::ToString() const {
  switch (type_) {
    case Type::kSuccess:
      return "Success";
    case Type::kNetworkError:
      return "Network error (" +
             net::ErrorToShortString(GetNetworkErrorOrDie()) + ")";
    case Type::kHttpError:
      return "HTTP error (" + base::NumberToString(GetHttpErrorOrDie()) + ")";
    case Type::kProtocolError:
      return std::string("Protocol error (") +
             GetSyncErrorTypeString(GetProtocolErrorOrDie()) + ")";
    case Type::kProtocolViolationError:
      return "Protocol violation error";
  }
  NOTREACHED();
}

}  // namespace syncer
