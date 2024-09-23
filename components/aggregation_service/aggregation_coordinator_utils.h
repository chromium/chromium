// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AGGREGATION_SERVICE_AGGREGATION_COORDINATOR_UTILS_H_
#define COMPONENTS_AGGREGATION_SERVICE_AGGREGATION_COORDINATOR_UTILS_H_

#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "url/origin.h"

namespace aggregation_service {

inline constexpr std::string_view kDefaultAggregationCoordinatorAwsCloud =
    "https://publickeyservice.msmt.aws.privacysandboxservices.com";

inline constexpr std::string_view kDefaultAggregationCoordinatorGcpCloud =
    "https://publickeyservice.msmt.gcp.privacysandboxservices.com";

COMPONENT_EXPORT(AGGREGATION_SERVICE)
url::Origin GetDefaultAggregationCoordinatorOrigin();

COMPONENT_EXPORT(AGGREGATION_SERVICE)
bool IsAggregationCoordinatorOriginAllowed(const url::Origin&);

class COMPONENT_EXPORT(AGGREGATION_SERVICE)
    ScopedAggregationCoordinatorAllowlistForTesting {
 public:
  explicit ScopedAggregationCoordinatorAllowlistForTesting(
      std::vector<url::Origin> origins = {});
  ~ScopedAggregationCoordinatorAllowlistForTesting();
  ScopedAggregationCoordinatorAllowlistForTesting(
      const ScopedAggregationCoordinatorAllowlistForTesting&) = delete;
  ScopedAggregationCoordinatorAllowlistForTesting& operator=(
      const ScopedAggregationCoordinatorAllowlistForTesting&) = delete;

  ScopedAggregationCoordinatorAllowlistForTesting(
      ScopedAggregationCoordinatorAllowlistForTesting&&) = delete;
  ScopedAggregationCoordinatorAllowlistForTesting& operator=(
      ScopedAggregationCoordinatorAllowlistForTesting&&) = delete;

 private:
  std::vector<url::Origin> previous_;
};

}  // namespace aggregation_service

#endif  // COMPONENTS_AGGREGATION_SERVICE_AGGREGATION_COORDINATOR_UTILS_H_
