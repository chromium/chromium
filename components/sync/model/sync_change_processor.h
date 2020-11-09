// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_SYNC_CHANGE_PROCESSOR_H_
#define COMPONENTS_SYNC_MODEL_SYNC_CHANGE_PROCESSOR_H_

#include <string>
#include <vector>

#include "components/sync/base/model_type.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/sync_error.h"

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
  // Returns: base::nullopt if no error was encountered, otherwise a
  //          base::Optional filled with such error.
  // Inputs:
  //   |from_here|: allows tracking of where sync changes originate.
  //   |change_list|: is the list of sync changes in need of processing.
  virtual base::Optional<ModelError> ProcessSyncChanges(
      const base::Location& from_here,
      const SyncChangeList& change_list) = 0;

  // Fills a list of SyncData. This should create an up to date representation
  // of all the data known to the ChangeProcessor for |datatype|, and
  // should match/be a subset of the server's view of that datatype.
  //
  // WARNING: This can be a potentially slow & memory intensive operation and
  // should only be used when absolutely necessary / sparingly.
  virtual SyncDataList GetAllSyncData(ModelType type) const = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_SYNC_CHANGE_PROCESSOR_H_
