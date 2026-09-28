// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/syncer_error.h"

#include <variant>

#include "base/strings/string_number_conversions.h"
#include "components/sync/engine/sync_protocol_error.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace syncer {

SyncerError::SyncerError(ValueType value) : value_(value) {}

// static
SyncerError SyncerError::Success() {
  return SyncerError(SuccessValueType());
}

// static
SyncerError SyncerError::NetworkError(int error_code) {
  return SyncerError(error_code);
}

// static
SyncerError SyncerError::HttpError(net::HttpStatusCode status_code) {
  return SyncerError(status_code);
}

// static
SyncerError SyncerError::ProtocolError(SyncProtocolErrorType error_type) {
  if (error_type == SyncProtocolErrorType::SYNC_SUCCESS) {
    // Ideally caller should use Success(), but let's fix it here to avoid
    // subtle bugs.
    return Success();
  }
  return SyncerError(error_type);
}

// static
SyncerError SyncerError::ProtocolViolationError() {
  return SyncerError(ProtocolViolationValueType());
}

int SyncerError::GetNetworkErrorOrDie() const {
  return std::get<int>(value_);
}

net::HttpStatusCode SyncerError::GetHttpErrorOrDie() const {
  return std::get<net::HttpStatusCode>(value_);
}

SyncProtocolErrorType SyncerError::GetProtocolErrorOrDie() const {
  return std::get<SyncProtocolErrorType>(value_);
}

std::string SyncerError::ToString() const {
  return std::visit(absl::Overload{
                        [](SuccessValueType) { return std::string("Success"); },
                        [](int error_code) {
                          return "Network error (" +
                                 net::ErrorToShortString(error_code) + ")";
                        },
                        [](net::HttpStatusCode status_code) {
                          return "HTTP error (" +
                                 base::NumberToString(status_code) + ")";
                        },
                        [](SyncProtocolErrorType error_type) {
                          return std::string("Protocol error (") +
                                 GetSyncErrorTypeString(error_type) + ")";
                        },
                        [](ProtocolViolationValueType) {
                          return std::string("Protocol violation error");
                        },
                    },
                    value_);
}

}  // namespace syncer
