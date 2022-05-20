// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_TYPES_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_TYPES_H_

#include <cstdint>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "base/types/id_type.h"

namespace segmentation_platform {

// Max number of days to keep UKM metrics stored in database.
constexpr base::TimeDelta kNumDaysToKeepUkm = base::Days(30);

using UkmEventHash = base::IdTypeU64<class UkmEventHashTag>;
using UkmMetricHash = base::IdTypeU64<class UkmMetricHashTag>;
using UrlId = base::IdType64<class UrlIdTag>;

using UkmEventsToMetricsMap =
    base::flat_map<UkmEventHash, base::flat_set<UkmMetricHash>>;

namespace processing {

// A struct that can accommodate multiple output types needed for Segmentation
// metadata's feature processing. It can only hold one value at a time with the
// corresponding type.
struct ProcessedValue {
  explicit ProcessedValue(bool val);
  explicit ProcessedValue(int val);
  explicit ProcessedValue(float val);
  explicit ProcessedValue(double val);
  explicit ProcessedValue(const std::string& val);
  explicit ProcessedValue(base::Time val);
  explicit ProcessedValue(int64_t val);

  ProcessedValue(const ProcessedValue& other);
  ProcessedValue& operator=(const ProcessedValue& other);

  bool operator==(const ProcessedValue& rhs) const;

  enum Type {
    UNKNOWN = 0,
    BOOL = 1,
    INT = 2,
    FLOAT = 3,
    DOUBLE = 4,
    STRING = 5,
    TIME = 6,
    INT64 = 7,
  };
  Type type{UNKNOWN};
  bool bool_val{false};
  int int_val{0};
  float float_val{0};
  double double_val{0};
  std::string str_val;
  base::Time time_val;
  int64_t int64_val{0};
};

// Represents a set of values that can represent inputs or outputs for a model.
using Tensor = std::vector<ProcessedValue>;

// Intermediate representation of processed features from the metadata queries.
using FeatureIndex = int;
using IndexedTensors = base::flat_map<FeatureIndex, Tensor>;

}  // namespace processing

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_TYPES_H_
