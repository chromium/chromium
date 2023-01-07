// Copyright 2022 The Chromium Authors
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
#include "components/segmentation_platform/public/types/processed_value.h"

namespace segmentation_platform {

// Max number of days to keep UKM metrics stored in database.
constexpr base::TimeDelta kNumDaysToKeepUkm = base::Days(30);

using UkmEventHash = base::IdTypeU64<class UkmEventHashTag>;
using UkmMetricHash = base::IdTypeU64<class UkmMetricHashTag>;
using UrlId = base::IdType64<class UrlIdTag>;

using UkmEventsToMetricsMap =
    base::flat_map<UkmEventHash, base::flat_set<UkmMetricHash>>;

namespace processing {

// Intermediate representation of processed features from the metadata queries.
using FeatureIndex = int;
using IndexedTensors = base::flat_map<FeatureIndex, Tensor>;

}  // namespace processing

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_TYPES_H_
