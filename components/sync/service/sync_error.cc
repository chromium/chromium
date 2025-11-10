// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_error.h"

#include "base/location.h"
#include "base/notreached.h"

namespace syncer {

// static
SyncError SyncError::CreateFromModelError(const ModelError& model_error) {
  return SyncError(model_error.location(), MODEL_ERROR, model_error.ToString(),
                   model_error.type());
}

// static
SyncError SyncError::CreateFromErrorType(const base::Location& location,
                                         ErrorType error_type,
                                         const std::string& message) {
  CHECK_NE(error_type, MODEL_ERROR);
  return SyncError(location, error_type, message, std::nullopt);
}

SyncError::SyncError(const base::Location& location,
                     ErrorType error_type,
                     const std::string& message,
                     std::optional<ModelError::Type> model_error_type)
    : location_(location),
      message_(message),
      error_type_(error_type),
      model_error_type_(model_error_type) {
  // `model_error_type` should be passed iff `error_type` is `MODEL_ERROR`.
  CHECK(error_type != MODEL_ERROR || model_error_type.has_value());
}

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

std::optional<ModelError::Type> SyncError::model_error_type() const {
  CHECK_EQ(error_type_, MODEL_ERROR);
  return model_error_type_;
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
