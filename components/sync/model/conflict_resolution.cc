// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/conflict_resolution.h"

#include <string>

#include "base/metrics/histogram_functions.h"

namespace syncer {

void RecordDataTypeEntityConflictResolution(
    DataType data_type,
    ConflictResolution resolution_type) {
  base::UmaHistogramEnumeration(
      std::string("Sync.DataTypeEntityConflictResolution.") +
          DataTypeToHistogramSuffix(data_type),
      resolution_type);
}

}  // namespace syncer
