// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/cycle/entity_change_metric_recording.h"

#include "base/metrics/histogram_functions.h"

namespace syncer {

namespace {

const char kEntityChangeHistogramPrefix[] = "Sync.DataTypeEntityChange.";
const char kLegacyEntityChangeHistogramPrefix[] =
    "Sync.ModelTypeEntityChange3.";

}  // namespace

void RecordEntityChangeMetrics(DataType type, DataTypeEntityChange change) {
  std::string histogram_name = std::string(kEntityChangeHistogramPrefix) +
                               DataTypeToHistogramSuffix(type);
  base::UmaHistogramEnumeration(histogram_name, change);

  // Legacy equivalent, before the metric was renamed.
  // TODO(crbug.com/358120886): Stop recording once alerts are switched to use
  // Sync.DataTypeEntityChange.
  std::string legacy_histogram_name =
      std::string(kLegacyEntityChangeHistogramPrefix) +
      DataTypeToHistogramSuffix(type);
  base::UmaHistogramEnumeration(legacy_histogram_name, change);
}

std::string GetEntityChangeHistogramNameForTest(DataType type) {
  return std::string(kEntityChangeHistogramPrefix) +
         DataTypeToHistogramSuffix(type);
}

}  // namespace syncer
