// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_METRICS_METRICS_UTILS_H_
#define COMPONENTS_COMMERCE_CORE_METRICS_METRICS_UTILS_H_

#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/prefs/pref_service.h"

namespace commerce::metrics {

extern const char kPDPStateHistogramName[];

// Possible options for the stat of a product details page (PDP). These must be
// kept in sync with the values in enums.xml.
enum class ShoppingPDPState {
  kNotPDP = 0,

  // The cluster ID is used to identify a product that is not specific to a
  // particular merchant (i.e. many merchants sell this). This is the
  // counterpart to offer ID which identifies a product for a specific merchant.
  kIsPDPWithoutClusterId = 1,
  kIsPDPWithClusterId = 2,

  // This enum must be last and is only used for histograms.
  kMaxValue = kIsPDPWithClusterId
};

// Record the state of a PDP for a navigation.
void RecordPDPStateForNavigation(
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata,
    PrefService* pref_service,
    bool is_off_the_record);

}  // namespace commerce::metrics

#endif  // COMPONENTS_COMMERCE_CORE_METRICS_METRICS_UTILS_H_
