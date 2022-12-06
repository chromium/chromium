// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/aggregation_service/parsing_utils.h"

#include "components/aggregation_service/aggregation_service.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace aggregation_service {

absl::optional<mojom::AggregationCoordinator> ParseAggregationCoordinator(
    const std::string& str) {
  if (str == "aws-cloud")
    return mojom::AggregationCoordinator::kAwsCloud;

  return absl::nullopt;
}

}  // namespace aggregation_service
