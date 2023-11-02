// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/cycle/entity_change_metric_recording.h"

#include "base/metrics/histogram_functions.h"

namespace syncer {

namespace {

const char kEntityChangeHistogramPrefix[] = "Sync.ModelTypeEntityChange3.";

}  // namespace

void RecordEntityChangeMetrics(ModelType type, ModelTypeEntityChange change) {
  std::string histogram_name = std::string(kEntityChangeHistogramPrefix) +
                               ModelTypeToHistogramSuffix(type);
  base::UmaHistogramEnumeration(histogram_name, change);
}

std::string GetEntityChangeHistogramNameForTest(ModelType type) {
  return std::string(kEntityChangeHistogramPrefix) +
         ModelTypeToHistogramSuffix(type);
}

}  // namespace syncer
