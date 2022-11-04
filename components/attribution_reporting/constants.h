// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_CONSTANTS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_CONSTANTS_H_

#include <stddef.h>

namespace attribution_reporting {

constexpr size_t kMaxBytesPerFilterString = 25;
constexpr size_t kMaxValuesPerFilter = 50;
constexpr size_t kMaxFiltersPerSource = 50;

constexpr size_t kMaxBytesPerAggregationKeyId = 25;
constexpr size_t kMaxAggregationKeysPerSourceOrTrigger = 50;

constexpr size_t kMaxAggregatableTriggerDataPerTrigger = 50;

constexpr size_t kMaxEventTriggerData = 10;

constexpr int kMaxAggregatableValue = 65536;

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_CONSTANTS_H_
