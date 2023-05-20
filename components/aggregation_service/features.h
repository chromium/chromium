// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AGGREGATION_SERVICE_FEATURES_H_
#define COMPONENTS_AGGREGATION_SERVICE_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace aggregation_service {

COMPONENT_EXPORT(AGGREGATION_SERVICE)
BASE_DECLARE_FEATURE(kAggregationServiceMultipleCloudProviders);

}  // namespace aggregation_service

#endif  // COMPONENTS_AGGREGATION_SERVICE_FEATURES_H_
