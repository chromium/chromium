// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/aggregation_service/aggregation_coordinator_utils.h"

#include <string>

#include "base/check.h"
#include "base/containers/enum_set.h"
#include "base/strings/string_piece.h"
#include "components/aggregation_service/features.h"
#include "components/attribution_reporting/is_origin_suitable.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace aggregation_service {

namespace {

// An identifier to specify the deployment option for the aggregation service.
enum class AggregationCoordinator {
  kAwsCloud,
  kGcpCloud,

  kMinValue = kAwsCloud,
  kMaxValue = kGcpCloud,
  kDefault = kAwsCloud,
};

url::Origin GetAggregationCoordinatorOriginFromString(
    base::StringPiece origin_str,
    base::StringPiece default_origin_str) {
  // Uses default origin in case of erroneous Finch params.
  auto origin = url::Origin::Create(GURL(origin_str));
  if (attribution_reporting::IsOriginSuitable(origin)) {
    return origin;
  }
  return url::Origin::Create(GURL(default_origin_str));
}

url::Origin GetAggregationCoordinatorOrigin(
    AggregationCoordinator aggregation_coordinator) {
  url::Origin origin;
  switch (aggregation_coordinator) {
    case AggregationCoordinator::kAwsCloud:
      origin = GetAggregationCoordinatorOriginFromString(
          kAggregationServiceCoordinatorAwsCloud.Get(),
          kDefaultAggregationCoordinatorAwsCloud);
      break;
    case AggregationCoordinator::kGcpCloud:
      origin = GetAggregationCoordinatorOriginFromString(
          kAggregationServiceCoordinatorGcpCloud.Get(),
          kDefaultAggregationCoordinatorGcpCloud);
      break;
  }
  CHECK(attribution_reporting::IsOriginSuitable(origin));
  return origin;
}

}  // namespace

url::Origin GetDefaultAggregationCoordinatorOrigin() {
  return GetAggregationCoordinatorOrigin(AggregationCoordinator::kDefault);
}

bool IsAggregationCoordinatorOriginAllowed(const url::Origin& origin) {
  for (auto coordinator :
       base::EnumSet<AggregationCoordinator, AggregationCoordinator::kMinValue,
                     AggregationCoordinator::kMaxValue>::All()) {
    if (origin == GetAggregationCoordinatorOrigin(coordinator)) {
      return true;
    }
  }
  return false;
}

}  // namespace aggregation_service
