// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_FEATURES_H_
#define COMPONENTS_METRICS_METRICS_FEATURES_H_

#include "base/feature_list.h"

namespace metrics::features {
// Determines whether the initial log should use the same logic as subsequent
// logs when building it.
BASE_DECLARE_FEATURE(kConsolidateMetricsServiceInitialLogLogic);
}  // namespace metrics::features

#endif  // COMPONENTS_METRICS_METRICS_FEATURES_H_