// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_MODEL_STATS_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_MODEL_STATS_H_

#include "components/download/internal/background_service/entry.h"

#include <map>

namespace download {
namespace stats {

// Metrics logger for background download service storage.

// Enum used by UMA metrics to tie to specific actions taken on a Model.  This
// can be used to track failure events.
enum class ModelAction {
  // Represents an attempt to initialize the Model.
  kInitialize = 0,

  // Represents an attempt to add an Entry to the Model.
  kAdd = 1,

  // Represents an attempt to update an Entry in the Model.
  kUpdate = 2,

  // Represents an attempt to remove an Entry from the Model.
  kRemove = 3,

  kMaxValue = kRemove,
};

// Logs statistics about the result of a model operation.  Used to track failure
// cases.
void LogModelOperationResult(ModelAction action, bool success);

// Logs the total number of all entries, and the number of entries in each
// state after the model is initialized.
void LogEntries(std::map<Entry::State, uint32_t>& entries_count);

}  // namespace stats
}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_MODEL_STATS_H_
