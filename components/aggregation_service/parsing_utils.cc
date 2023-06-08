// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/aggregation_service/parsing_utils.h"

#include <string>

#include "components/aggregation_service/aggregation_coordinator_utils.h"
#include "components/aggregation_service/aggregation_service.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace aggregation_service {

// TODO(crbug.com/1450241): Consider getting rid of the enum and plumbing the
// origin through.
absl::optional<mojom::AggregationCoordinator> ParseAggregationCoordinator(
    const std::string& str) {
  auto origin = url::Origin::Create(GURL(str));
  if (origin == GetAggregationCoordinatorOrigin(
                    mojom::AggregationCoordinator::kAwsCloud)) {
    return mojom::AggregationCoordinator::kAwsCloud;
  }
  return absl::nullopt;
}

std::string SerializeAggregationCoordinator(
    mojom::AggregationCoordinator coordinator) {
  return GetAggregationCoordinatorOrigin(coordinator).Serialize();
}

}  // namespace aggregation_service
