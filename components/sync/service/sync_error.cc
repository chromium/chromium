// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_error.h"

#include "base/location.h"
#include "base/notreached.h"

namespace syncer {

SyncError::SyncError(const base::Location& location,
                     ErrorType error_type,
                     const std::string& message)
    : location_(location), message_(message), error_type_(error_type) {}

SyncError::~SyncError() = default;

const base::Location& SyncError::location() const {
  return location_;
}

const std::string& SyncError::message() const {
  return message_;
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
    case PRECONDITION_ERROR_WITH_KEEP_DATA:
      return "failed precondition was encountered with keep data: ";
    case PRECONDITION_ERROR_WITH_CLEAR_DATA:
      return "failed precondition was encountered with clear data: ";
  }
  NOTREACHED();
}

}  // namespace syncer
