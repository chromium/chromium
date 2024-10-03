// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_error.h"

#include "base/location.h"
#include "base/notreached.h"

namespace syncer {

SyncError::SyncError() = default;

SyncError::SyncError(const base::Location& location,
                     ErrorType error_type,
                     const std::string& message,
                     DataType data_type)
    : location_(location),
      message_(message),
      data_type_(data_type),
      error_type_(error_type) {
  CHECK_NE(error_type, UNSET);
}

SyncError::~SyncError() = default;

bool SyncError::IsSet() const {
  return error_type_ != UNSET;
}

const base::Location& SyncError::location() const {
  DCHECK(IsSet());
  return location_;
}

const std::string& SyncError::message() const {
  DCHECK(IsSet());
  return message_;
}

DataType SyncError::data_type() const {
  DCHECK(IsSet());
  return data_type_;
}

SyncError::ErrorType SyncError::error_type() const {
  DCHECK(IsSet());
  return error_type_;
}

std::string SyncError::GetMessagePrefix() const {
  std::string type_message;
  switch (error_type_) {
    case MODEL_ERROR:
      type_message = "model error was encountered: ";
      break;
    case CONFIGURATION_ERROR:
      type_message = "configuration error was encountered: ";
      break;
    case CRYPTO_ERROR:
      type_message = "cryptographer error was encountered: ";
      break;
    case UNREADY_ERROR:
      type_message = "unready error was encountered: ";
      break;
    case DATATYPE_POLICY_ERROR:
      type_message = "disabled due to configuration constraints: ";
      break;
    case UNSET:
      NOTREACHED_IN_MIGRATION() << "Invalid error type";
      break;
  }
  return type_message;
}

std::string SyncError::ToString() const {
  if (!IsSet()) {
    return std::string();
  }
  return location_.ToString() + ", " + DataTypeToDebugString(data_type_) + " " +
         GetMessagePrefix() + message_;
}

}  // namespace syncer
