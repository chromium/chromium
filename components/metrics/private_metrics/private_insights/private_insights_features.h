// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_PRIVATE_INSIGHTS_FEATURES_H_
#define COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_PRIVATE_INSIGHTS_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace private_insights {

// Enables Private Insights.
COMPONENT_EXPORT(PRIVATE_INSIGHTS)
BASE_DECLARE_FEATURE(kPrivateInsightsFeature);

// Enables Private AI Compute error reporting over Private Insights.
COMPONENT_EXPORT(PRIVATE_INSIGHTS)
BASE_DECLARE_FEATURE(kPrivateInsightsPaicErrorReporting);

}  // namespace private_insights

#endif  // COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_PRIVATE_INSIGHTS_FEATURES_H_
