// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_MODEL_ERROR_H_
#define COMPONENTS_SYNC_MODEL_MODEL_ERROR_H_

#include <string>

#include "base/functional/callback.h"
#include "base/location.h"

namespace syncer {

// A minimal error object for use by USS data type code.
class ModelError {
 public:
  // Creates a set error object with the given location and message.
  ModelError(const base::Location& location, const std::string& message);

  ~ModelError();

  // The location of the error this object represents. Can only be called if the
  // error is set.
  const base::Location& location() const;

  // The message explaining the error this object represents. Can only be called
  // if the error is set.
  const std::string& message() const;

  // Returns string representation of this object, appropriate for logging.
  std::string ToString() const;

 private:
  base::Location location_;
  std::string message_;
};

// Typedef for a simple error handler callback.
using ModelErrorHandler = base::RepeatingCallback<void(const ModelError&)>;

using OnceModelErrorHandler = base::OnceCallback<void(const ModelError&)>;

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_MODEL_ERROR_H_
