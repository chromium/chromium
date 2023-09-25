// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNCER_ERROR_H_
#define COMPONENTS_SYNC_ENGINE_SYNCER_ERROR_H_

#include <string>

#include "components/sync/engine/sync_protocol_error.h"
#include "net/http/http_status_code.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace syncer {

// This class describes all the possible results of a sync cycle. It should be
// passed by value.
class SyncerError {
 public:
  enum class Type {
    kSuccess,
    kNetworkError,
    kHttpError,
    kProtocolError,
    kProtocolViolationError,

    kMaxValue = kProtocolViolationError
  };

  static SyncerError Success();
  static SyncerError NetworkError(int error_code);
  static SyncerError HttpError(net::HttpStatusCode status_code);
  static SyncerError ProtocolError(SyncProtocolErrorType error_type);
  static SyncerError ProtocolViolationError();

  ~SyncerError() = default;

  Type type() const { return type_; }

  // It is a caller responsibility to ensure right type(). If it doesn't match
  // Get..OrDie() will cause a crash.
  int GetNetworkErrorOrDie() const;
  net::HttpStatusCode GetHttpErrorOrDie() const;
  SyncProtocolErrorType GetProtocolErrorOrDie() const;

  std::string ToString() const;

 private:
  struct SuccessValueType {};
  struct ProtocolViolationValueType {};

  using ValueType = absl::variant<SuccessValueType,
                                  int /*network error code*/,
                                  net::HttpStatusCode,
                                  SyncProtocolErrorType,
                                  ProtocolViolationValueType>;
  static_assert(absl::variant_size<ValueType>::value ==
                static_cast<int>(Type::kMaxValue) + 1);

  SyncerError(Type type, ValueType value);

  Type type_;
  ValueType value_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNCER_ERROR_H_
