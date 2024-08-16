// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_CYCLE_ENTITY_CHANGE_METRIC_RECORDING_H_
#define COMPONENTS_SYNC_ENGINE_CYCLE_ENTITY_CHANGE_METRIC_RECORDING_H_

#include <string>

#include "components/sync/base/data_type.h"

namespace syncer {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SyncEntityChange)
enum class DataTypeEntityChange {
  kLocalDeletion = 0,
  kLocalCreation = 1,
  kLocalUpdate = 2,
  kRemoteDeletion = 3,
  kRemoteNonInitialUpdate = 4,
  kRemoteInitialUpdate = 5,
  kMaxValue = kRemoteInitialUpdate,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:SyncEntityChange)

void RecordEntityChangeMetrics(DataType type, DataTypeEntityChange change);

std::string GetEntityChangeHistogramNameForTest(DataType type);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_CYCLE_ENTITY_CHANGE_METRIC_RECORDING_H_
