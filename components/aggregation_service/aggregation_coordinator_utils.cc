// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/aggregation_service/aggregation_coordinator_utils.h"

#include "base/check.h"
#include "base/strings/string_piece.h"
#include "components/aggregation_service/aggregation_service.mojom.h"
#include "components/aggregation_service/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace aggregation_service {

namespace {

url::Origin GetAggregationCoordinatorOriginFromString(
    base::StringPiece origin_str,
    base::StringPiece default_origin_str) {
  // Uses default origin in case of erroneous Finch params.
  auto origin = url::Origin::Create(GURL(origin_str));
  if (network::IsOriginPotentiallyTrustworthy(origin)) {
    return origin;
  }
  return url::Origin::Create(GURL(default_origin_str));
}

}  // namespace

url::Origin GetAggregationCoordinatorOrigin(
    mojom::AggregationCoordinator aggregation_coordinator) {
  switch (aggregation_coordinator) {
    case mojom::AggregationCoordinator::kAwsCloud:
      url::Origin origin = GetAggregationCoordinatorOriginFromString(
          kAggregationServiceCoordinatorAwsCloud.Get(),
          kDefaultAggregationCoordinatorAwsCloud);
      CHECK(network::IsOriginPotentiallyTrustworthy(origin));
      return origin;
  }
}

}  // namespace aggregation_service
