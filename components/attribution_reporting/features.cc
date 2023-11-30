// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/features.h"

#include "base/feature_list.h"

namespace attribution_reporting::features {

// Controls whether the Conversion Measurement API infrastructure is enabled.
BASE_FEATURE(kConversionMeasurement,
             "ConversionMeasurement",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAttributionReportingTriggerConfig,
             "AttributionReportingTriggerConfig",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAttributionReportingTriggerContextId,
             "AttributionReportingTriggerContextId",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace attribution_reporting::features
