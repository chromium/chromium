// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AGGREGATION_SERVICE_FEATURES_H_
#define COMPONENTS_AGGREGATION_SERVICE_FEATURES_H_

#include <string>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace aggregation_service {

COMPONENT_EXPORT(AGGREGATION_SERVICE)
BASE_DECLARE_FEATURE(kAggregationServiceMultipleCloudProviders);

COMPONENT_EXPORT(AGGREGATION_SERVICE)
extern const base::FeatureParam<std::string>
    kAggregationServiceCoordinatorAwsCloud;

COMPONENT_EXPORT(AGGREGATION_SERVICE)
extern const base::FeatureParam<std::string>
    kAggregationServiceCoordinatorGcpCloud;

}  // namespace aggregation_service

#endif  // COMPONENTS_AGGREGATION_SERVICE_FEATURES_H_
