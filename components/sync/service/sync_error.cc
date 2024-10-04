// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_error.h"

#include "base/location.h"
#include "base/notreached.h"

namespace syncer {

SyncError::SyncError(const base::Location& location,
                     ErrorType error_type,
                     const std::string& message,
                     DataType data_type)
    : location_(location),
      message_(message),
      data_type_(data_type),
      error_type_(error_type) {}

SyncError::~SyncError() = default;

const base::Location& SyncError::location() const {
  return location_;
}

const std::string& SyncError::message() const {
  return message_;
}

DataType SyncError::data_type() const {
  return data_type_;
}

SyncError::ErrorType SyncError::error_type() const {
  return error_type_;
}

std::string SyncError::GetMessagePrefix() const {
  switch (error_type_) {
    case MODEL_ERROR:
      return "model error was encountered: ";
    case CONFIGURATION_ERROR:
      return "configuration error was encountered: ";
    case CRYPTO_ERROR:
      return "cryptographer error was encountered: ";
    case UNREADY_ERROR:
      return "unready error was encountered: ";
    case DATATYPE_POLICY_ERROR:
      return "disabled due to configuration constraints: ";
  }
  NOTREACHED();
}

}  // namespace syncer
