// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AGGREGATION_SERVICE_AGGREGATION_COORDINATOR_UTILS_H_
#define COMPONENTS_AGGREGATION_SERVICE_AGGREGATION_COORDINATOR_UTILS_H_

#include "base/component_export.h"

namespace url {
class Origin;
}  // namespace url

namespace aggregation_service {

constexpr char kDefaultAggregationCoordinatorAwsCloud[] =
    "https://publickeyservice.aws.privacysandboxservices.com";

constexpr char kDefaultAggregationCoordinatorGcpCloud[] =
    "https://gcp-server.example";

COMPONENT_EXPORT(AGGREGATION_SERVICE)
url::Origin GetDefaultAggregationCoordinatorOrigin();

COMPONENT_EXPORT(AGGREGATION_SERVICE)
bool IsAggregationCoordinatorOriginAllowed(const url::Origin&);

}  // namespace aggregation_service

#endif  // COMPONENTS_AGGREGATION_SERVICE_AGGREGATION_COORDINATOR_UTILS_H_
