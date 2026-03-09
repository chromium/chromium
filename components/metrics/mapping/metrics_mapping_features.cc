// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/mapping/metrics_mapping_features.h"

namespace metrics::features {

BASE_FEATURE(kWebiumMetricsMapping,
             "WebiumMetricsMapping",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(std::string,
                   kWebiumMetricsMappingConfig,
                   &kWebiumMetricsMapping,
                   "config",
                   "");

}  // namespace metrics::features
