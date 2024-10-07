// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_SYNC_ERROR_H_
#define COMPONENTS_SYNC_SERVICE_SYNC_ERROR_H_

#include <string>

#include "base/location.h"

namespace syncer {

// Sync errors are used for debug purposes and handled internally and/or
// exposed through Chrome's "chrome://sync-internals" internal page.
// This class is copy-friendly and thread-safe.
class SyncError {
 public:
  // Error types are used to distinguish general datatype errors (which result
  // in the datatype being disabled) from actionable sync errors (which might
  // have more complicated results).
  enum ErrorType {
    // A datatype model reported an error.
    MODEL_ERROR,
    // The configuration procedure (usually the initial download) failed.
    CONFIGURATION_ERROR,
    // A cryptographer error was detected (i.e. the datatype is encrypted and
    // encryption keys are missing).
    CRYPTO_ERROR,
    // A datatype cannot start because its controller determined that it doesn't
    // meet all preconditions via `DataTypeController::GetPreconditionState()`.
    // Specifically, it returned `kMustStopAndKeepData`.
    PRECONDITION_ERROR_WITH_KEEP_DATA,
    // Same as above, but the controller returned `kMustStopAndClearData`.
    PRECONDITION_ERROR_WITH_CLEAR_DATA,
  };

  // Create a new Sync error of type `error_type` triggered from the specified
  // location.
  SyncError(const base::Location& location,
            ErrorType error_type,
            const std::string& message);
  SyncError(const SyncError& other) = default;
  SyncError& operator=(const SyncError& other) = default;
  ~SyncError();

  const base::Location& location() const;
  const std::string& message() const;
  ErrorType error_type() const;

  // Type specific message prefix for logging and UI purposes.
  std::string GetMessagePrefix() const;

 private:
  base::Location location_;
  std::string message_;
  ErrorType error_type_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_SYNC_ERROR_H_
