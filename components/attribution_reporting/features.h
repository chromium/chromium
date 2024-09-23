// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_FEATURES_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace attribution_reporting::features {

// Note: "Conversion Measurement" is a legacy name for "Attribution Reporting"
COMPONENT_EXPORT(ATTRIBUTION_REPORTING_FEATURES)
BASE_DECLARE_FEATURE(kConversionMeasurement);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING_FEATURES)
BASE_DECLARE_FEATURE(kAttributionAggregatableDebugReporting);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING_FEATURES)
BASE_DECLARE_FEATURE(kAttributionSourceDestinationLimit);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING_FEATURES)
BASE_DECLARE_FEATURE(kAttributionReportingAggregatableFilteringIds);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING_FEATURES)
BASE_DECLARE_FEATURE(kAttributionScopes);

}  // namespace attribution_reporting::features

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_FEATURES_H_
