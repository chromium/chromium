// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_SYNC_ERROR_H_
#define COMPONENTS_SYNC_SERVICE_SYNC_ERROR_H_

#include <string>

#include "base/location.h"
#include "components/sync/base/data_type.h"

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
    // A datatype is not ready to start yet, so should be neither purged nor
    // enabled until it is ready.
    UNREADY_ERROR,
    // A datatype should be disabled and purged due to configuration
    // constraints.
    DATATYPE_POLICY_ERROR,
  };

  // Create a new Sync error of type `error_type` triggered by `data_type`
  // from the specified location.
  SyncError(const base::Location& location,
            ErrorType error_type,
            const std::string& message,
            DataType data_type);
  SyncError(const SyncError& other) = default;
  SyncError& operator=(const SyncError& other) = default;
  ~SyncError();

  const base::Location& location() const;
  const std::string& message() const;
  DataType data_type() const;
  ErrorType error_type() const;

  // Type specific message prefix for logging and UI purposes.
  std::string GetMessagePrefix() const;

 private:
  base::Location location_;
  std::string message_;
  DataType data_type_;
  ErrorType error_type_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_SYNC_ERROR_H_
