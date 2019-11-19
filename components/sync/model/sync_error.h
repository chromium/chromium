// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_SYNC_ERROR_H_
#define COMPONENTS_SYNC_MODEL_SYNC_ERROR_H_

#include <iosfwd>
#include <memory>
#include <string>

#include "components/sync/base/model_type.h"

namespace base {
class Location;
}  // namespace base

namespace syncer {

// Sync errors are used for debug purposes and handled internally and/or
// exposed through Chrome's "about:sync" internal page.
// This class is copy-friendly and thread-safe.
class SyncError {
 public:
  // Error types are used to distinguish general datatype errors (which result
  // in the datatype being disabled) from actionable sync errors (which might
  // have more complicated results).
  enum ErrorType {
    UNSET,                 // No error.
    UNRECOVERABLE_ERROR,   // An unrecoverable runtime error was encountered,
                           // and sync should be disabled and purged completely.
    DATATYPE_ERROR,        // A datatype error was encountered, and the datatype
                           // should be disabled and purged completely. Note
                           // that datatype errors may be reset, triggering a
                           // re-enable.
    PERSISTENCE_ERROR,     // A persistence error was detected, and the
                           // datataype should be associated after a sync
                           // update.
    CRYPTO_ERROR,          // A cryptographer error was detected, and the
                           // datatype should be associated after it is
                           // resolved.
    UNREADY_ERROR,         // A datatype is not ready to start yet, so should be
                           // neither purged nor enabled until it is ready.
    DATATYPE_POLICY_ERROR  // A datatype should be disabled and purged due to
                           // configuration constraints.
  };

  // Severity is used to indicate how an error should be logged and
  // represented to an end user.
  enum Severity {
    SYNC_ERROR_SEVERITY_ERROR,  // Severe unrecoverable error.
    SYNC_ERROR_SEVERITY_INFO    // Low-severity recoverable error or
                                // configuration policy issue.
  };

  // Default constructor refers to "no error", and IsSet() will return false.
  SyncError();

  // Create a new Sync error of type |error_type| triggered by |model_type|
  // from the specified location. IsSet() will return true afterward. Will
  // create and print an error specific message to LOG(ERROR).
  SyncError(const base::Location& location,
            ErrorType error_type,
            const std::string& message,
            ModelType model_type);

  // Copy and assign via deep copy.
  SyncError(const SyncError& other);
  SyncError& operator=(const SyncError& other);

  ~SyncError();

  // Reset the current error to a new datatype error. May be called
  // irrespective of whether IsSet() is true. After this is called, IsSet()
  // will return true.
  // Will print the new error to LOG(ERROR).
  void Reset(const base::Location& location,
             const std::string& message,
             ModelType type);

  // Whether this is a valid error or not.
  bool IsSet() const;

  // These must only be called if IsSet() is true.
  const base::Location& location() const;
  const std::string& message() const;
  ModelType model_type() const;
  ErrorType error_type() const;

  // Error severity for logging and UI purposes.
  Severity GetSeverity() const;
  // Type specific message prefix for logging and UI purposes.
  std::string GetMessagePrefix() const;

  // Returns empty string is IsSet() is false.
  std::string ToString() const;

 private:
  // Print error information to log.
  void PrintLogError() const;

  // Make a copy of a SyncError. If other.IsSet() == false, this->IsSet() will
  // now return false.
  void Copy(const SyncError& other);

  // Initialize the local error data with the specified error data. After this
  // is called, IsSet() will return true.
  void Init(const base::Location& location,
            const std::string& message,
            ModelType model_type,
            ErrorType error_type);

  // Reset the error to it's default (unset) values.
  void Clear();

  // unique_ptr is necessary because Location objects aren't assignable.
  std::unique_ptr<base::Location> location_;
  std::string message_;
  ModelType model_type_;
  ErrorType error_type_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_SYNC_ERROR_H_
