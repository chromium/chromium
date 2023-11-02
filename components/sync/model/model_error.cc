// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/model_error.h"
#include "components/sync/model/sync_error.h"

namespace syncer {

ModelError::ModelError(const base::Location& location,
                       const std::string& message)
    : location_(location), message_(message) {}

ModelError::~ModelError() = default;

const base::Location& ModelError::location() const {
  return location_;
}

const std::string& ModelError::message() const {
  return message_;
}

std::string ModelError::ToString() const {
  return location_.ToString() + std::string(": ") + message_;
}

// TODO(https://crbug.com/1057577): Remove this once ProcessSyncChanges in
// SyncableService has been refactored.
absl::optional<ModelError> ConvertToModelError(const SyncError& sync_error) {
  if (sync_error.IsSet()) {
    return ModelError(sync_error.location(), sync_error.message());
  }
  return absl::nullopt;
}

}  // namespace syncer
