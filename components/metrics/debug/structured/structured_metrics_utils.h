// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DEBUG_STRUCTURED_STRUCTURED_METRICS_UTILS_H_
#define COMPONENTS_METRICS_DEBUG_STRUCTURED_STRUCTURED_METRICS_UTILS_H_

#include "base/values.h"
#include "components/metrics/structured/event.h"

namespace metrics::structured {

class StructuredMetricsService;

// Collects summary information for Structured Metrics service. Value format can
// be found in structured_utils.ts.
base::Value GetStructuredMetricsSummary(StructuredMetricsService* service);

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_DEBUG_STRUCTURED_STRUCTURED_METRICS_UTILS_H_
