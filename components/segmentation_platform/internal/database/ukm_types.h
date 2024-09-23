// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_TYPES_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_TYPES_H_

#include <cstdint>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/types/id_type.h"
#include "components/segmentation_platform/public/proto/types.pb.h"
#include "components/segmentation_platform/public/types/processed_value.h"

namespace segmentation_platform {

// Max number of days to keep UKM metrics stored in database.
constexpr base::TimeDelta kNumDaysToKeepUkm = base::Days(30);

using UkmEventHash = base::IdTypeU64<class UkmEventHashTag>;
using UkmMetricHash = base::IdTypeU64<class UkmMetricHashTag>;
using UrlId = base::IdType64<class UrlIdTag>;

using UkmEventsToMetricsMap =
    base::flat_map<UkmEventHash, base::flat_set<UkmMetricHash>>;

// Represents an UMA metric or user action entry from the signal database.
struct UmaMetricEntry {
  proto::SignalType type = proto::SignalType::UNKNOWN_SIGNAL_TYPE;
  // Hash of the histogram or user action.
  uint64_t name_hash = 0;
  // Sample recorded time.
  base::Time time;
  // Sample value, always 0 for user actions.
  int32_t value = 0;
};

// CleanupItem is used to store signals for cleanup.
struct CleanupItem {
 public:
  CleanupItem();
  CleanupItem(uint64_t name_hash,
              uint64_t event_hash,
              proto::SignalType signal_type,
              base::Time timestamp);
  ~CleanupItem();

  bool operator==(const CleanupItem& other) const;

  bool IsUma() const;

  // Name of the signal to be cleaned up.
  uint64_t name_hash;
  // Event hash for the signal.
  uint64_t event_hash;
  // Type of signal.
  proto::SignalType signal_type;
  // Indicates the time when the signal was last cleaned up.
  base::Time timestamp;
  // Event hash for non UKM signals.
  static const uint64_t kNonUkmEventHash = 0;
};

namespace processing {

// Intermediate representation of processed features from the metadata queries.
using FeatureIndex = int;
using IndexedTensors = base::flat_map<FeatureIndex, Tensor>;

}  // namespace processing

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_TYPES_H_
