// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_MODEL_ERROR_H_
#define COMPONENTS_SYNC_MODEL_MODEL_ERROR_H_

#include <string>

#include "base/functional/callback.h"
#include "base/location.h"

namespace syncer {

// A minimal error object that individual datatypes can report.
class ModelError {
 public:
  // This enum should be in sync with ModelErrorType in enums.xml. These
  // values are persisted to logs. Entries should not be renumbered and numeric
  // values should never be reused.

  // LINT.IfChange(Type)
  enum class Type {
    kUnspecified = 0,  // Default value if the error type is not set.
                       // TODO(crbug.com/425629291): Remove this value once we
                       // have implemented proper error handling for all data
                       // types.

    // Password error types.
    kPasswordDbInitFailed = 1,
    kPasswordMergeDecryptionFailed = 2,
    kPasswordMergeUpdateFailed = 3,
    kPasswordIncrementalAddFailed = 4,
    kPasswordCleanupDbFailed = 5,
    kPasswordMergeReadFromDbFailed = 6,
    kPasswordMergeReadAfterCleanupFailed = 7,
    kPasswordCommitReadFailed = 8,
    kPasswordDebugReadFailed = 9,
    kPasswordMergeAddFailed = 10,
    kPasswordCleanupDecryptionFailed = 11,
    kPasswordIncrementalUpdateFailed = 12,
    kMaxValue = kPasswordIncrementalUpdateFailed,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/sync/enums.xml:SyncModelError)

  // Creates a set error object with the given location and message.

  // DEPRECATED. Use the constructor with ModelError::Type instead. See
  // crbug.com/40886237.
  ModelError(const base::Location& location, const std::string& message);

  // Creates a set error object with the given location and error
  // type. Do not use this with the default ModelErrorType::kUnspecified value.
  ModelError(const base::Location& location, Type model_error_type);

  ~ModelError();

  // The location of the error this object represents. Can only be called if the
  // error is set.
  const base::Location& location() const;

  // The message explaining the error this object represents. Can only be called
  // if the error is set.
  const std::string& message() const;

  // The type of the error this object represents. Only set if the error type is
  // known. Otherwise, returns ModelErrorType::kUnspecified.
  ModelError::Type type() const;

  // Returns string representation of this object, appropriate for logging.
  std::string ToString() const;

 private:
  base::Location location_;
  std::string message_;
  // The type of the error. This is optional to ensure backwards compatibility.
  // It is used for metrics collection.
  Type type_ = Type::kUnspecified;
};

// Typedef for a simple error handler callback.
using ModelErrorHandler = base::RepeatingCallback<void(const ModelError&)>;

using OnceModelErrorHandler = base::OnceCallback<void(const ModelError&)>;

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_MODEL_ERROR_H_
