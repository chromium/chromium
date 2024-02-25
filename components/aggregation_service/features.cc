// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/aggregation_service/features.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/aggregation_service/aggregation_coordinator_utils.h"

namespace aggregation_service {

BASE_FEATURE(kAggregationServiceMultipleCloudProviders,
             "AggregationServiceMultipleCloudProviders",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kAggregationServiceCoordinatorAwsCloud{
    &kAggregationServiceMultipleCloudProviders, "aws_cloud",
    kDefaultAggregationCoordinatorAwsCloud};

const base::FeatureParam<std::string> kAggregationServiceCoordinatorGcpCloud{
    &kAggregationServiceMultipleCloudProviders, "gcp_cloud",
    kDefaultAggregationCoordinatorGcpCloud};

}  // namespace aggregation_service
