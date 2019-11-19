// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/sync_error.h"

#include <ostream>

#include "base/location.h"
#include "base/logging.h"

namespace syncer {

SyncError::SyncError() {
  Clear();
}

SyncError::SyncError(const base::Location& location,
                     ErrorType error_type,
                     const std::string& message,
                     ModelType model_type) {
  DCHECK(error_type != UNSET);
  Init(location, message, model_type, error_type);
  PrintLogError();
}

SyncError::SyncError(const SyncError& other) {
  Copy(other);
}

SyncError::~SyncError() {}

SyncError& SyncError::operator=(const SyncError& other) {
  if (this == &other) {
    return *this;
  }
  Copy(other);
  return *this;
}

void SyncError::Copy(const SyncError& other) {
  if (other.IsSet()) {
    Init(other.location(), other.message(), other.model_type(),
         other.error_type());
  } else {
    Clear();
  }
}

void SyncError::Clear() {
  location_.reset();
  message_ = std::string();
  model_type_ = UNSPECIFIED;
  error_type_ = UNSET;
}

void SyncError::Reset(const base::Location& location,
                      const std::string& message,
                      ModelType model_type) {
  Init(location, message, model_type, DATATYPE_ERROR);
  PrintLogError();
}

void SyncError::Init(const base::Location& location,
                     const std::string& message,
                     ModelType model_type,
                     ErrorType error_type) {
  location_ = std::make_unique<base::Location>(location);
  message_ = message;
  model_type_ = model_type;
  error_type_ = error_type;
}

bool SyncError::IsSet() const {
  return error_type_ != UNSET;
}

const base::Location& SyncError::location() const {
  DCHECK(IsSet());
  return *location_;
}

const std::string& SyncError::message() const {
  DCHECK(IsSet());
  return message_;
}

ModelType SyncError::model_type() const {
  DCHECK(IsSet());
  return model_type_;
}

SyncError::ErrorType SyncError::error_type() const {
  DCHECK(IsSet());
  return error_type_;
}

SyncError::Severity SyncError::GetSeverity() const {
  switch (error_type_) {
    case UNREADY_ERROR:
    case DATATYPE_POLICY_ERROR:
      return SYNC_ERROR_SEVERITY_INFO;
    default:
      return SYNC_ERROR_SEVERITY_ERROR;
  }
}

std::string SyncError::GetMessagePrefix() const {
  std::string type_message;
  switch (error_type_) {
    case UNRECOVERABLE_ERROR:
      type_message = "unrecoverable error was encountered: ";
      break;
    case DATATYPE_ERROR:
      type_message = "datatype error was encountered: ";
      break;
    case PERSISTENCE_ERROR:
      type_message = "persistence error was encountered: ";
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
      NOTREACHED() << "Invalid error type";
      break;
  }
  return type_message;
}

std::string SyncError::ToString() const {
  if (!IsSet()) {
    return std::string();
  }
  return location_->ToString() + ", " + ModelTypeToString(model_type_) + " " +
         GetMessagePrefix() + message_;
}

void SyncError::PrintLogError() const {
  logging::LogSeverity logSeverity = (GetSeverity() == SYNC_ERROR_SEVERITY_INFO)
                                         ? logging::LOG_VERBOSE
                                         : logging::LOG_ERROR;

  LAZY_STREAM(logging::LogMessage(location_->file_name(),
                                  location_->line_number(), logSeverity)
                  .stream(),
              logSeverity >= ::logging::GetMinLogLevel())
      << ModelTypeToString(model_type_) << " " << GetMessagePrefix()
      << message_;
}

}  // namespace syncer
