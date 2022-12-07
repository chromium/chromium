// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/aggregation_service/parsing_utils.h"

#include <string>

#include "components/aggregation_service/aggregation_service.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace aggregation_service {

namespace {
constexpr char kAwsCloud[] = "aws-cloud";
}  // namespace

absl::optional<mojom::AggregationCoordinator> ParseAggregationCoordinator(
    const std::string& str) {
  if (str == kAwsCloud)
    return mojom::AggregationCoordinator::kAwsCloud;

  return absl::nullopt;
}

std::string SerializeAggregationCoordinator(
    mojom::AggregationCoordinator coordinator) {
  switch (coordinator) {
    case mojom::AggregationCoordinator::kAwsCloud:
      return kAwsCloud;
  }
}

}  // namespace aggregation_service
