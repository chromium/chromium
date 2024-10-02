// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_SYNC_CHANGE_PROCESSOR_H_
#define COMPONENTS_SYNC_MODEL_SYNC_CHANGE_PROCESSOR_H_

#include "components/sync/base/data_type.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_data.h"

namespace base {
class Location;
}  // namespace base

namespace syncer {

// An interface for services that handle receiving SyncChanges.
class SyncChangeProcessor {
 public:
  SyncChangeProcessor() = default;
  virtual ~SyncChangeProcessor() = default;

  // Process a list of SyncChanges.
  // Returns: std::nullopt if no error was encountered, otherwise a
  //          std::optional filled with such error.
  // Inputs:
  //   |from_here|: allows tracking of where sync changes originate.
  //   |change_list|: is the list of sync changes in need of processing.
  virtual std::optional<ModelError> ProcessSyncChanges(
      const base::Location& from_here,
      const SyncChangeList& change_list) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_SYNC_CHANGE_PROCESSOR_H_
