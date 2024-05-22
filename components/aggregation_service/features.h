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

// This feature is no longer checked, and only the feature param is used.
COMPONENT_EXPORT(AGGREGATION_SERVICE)
BASE_DECLARE_FEATURE(kAggregationServiceMultipleCloudProviders);

// Comma-separated origins. The first origin will be used as default. When
// empty, the default coordinator origins will be used.
//
// TODO(linnan): Consider replacing this with a command-line switch since it
// isn't needed in production.
COMPONENT_EXPORT(AGGREGATION_SERVICE)
extern const base::FeatureParam<std::string>
    kAggregationServiceCoordinatorAllowlist;

}  // namespace aggregation_service

#endif  // COMPONENTS_AGGREGATION_SERVICE_FEATURES_H_
