// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/cycle/entity_change_metric_recording.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace syncer {

namespace {

constexpr char kEntityChangeHistogramPrefix[] = "Sync.DataTypeEntityChange.";

}  // namespace

void RecordEntityChangeMetrics(DataType type, DataTypeEntityChange change) {
  base::UmaHistogramEnumeration(base::StrCat({kEntityChangeHistogramPrefix,
                                              DataTypeToHistogramSuffix(type)}),
                                change);
}

std::string GetEntityChangeHistogramNameForTest(DataType type) {
  return base::StrCat(
      {kEntityChangeHistogramPrefix, DataTypeToHistogramSuffix(type)});
}

}  // namespace syncer
