// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AGGREGATION_SERVICE_PARSING_UTILS_H_
#define COMPONENTS_AGGREGATION_SERVICE_PARSING_UTILS_H_

#include <string>

#include "base/component_export.h"
#include "components/aggregation_service/aggregation_service.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace aggregation_service {

// Parses aggregation coordinator identifier. Returns `kDefault` if `str` is
// nullptr or is not a pre-defined value.
COMPONENT_EXPORT(AGGREGATION_SERVICE)
absl::optional<mojom::AggregationCoordinator> ParseAggregationCoordinator(
    const std::string& str);

COMPONENT_EXPORT(AGGREGATION_SERVICE)
std::string SerializeAggregationCoordinator(mojom::AggregationCoordinator);

}  // namespace aggregation_service

#endif  // COMPONENTS_AGGREGATION_SERVICE_PARSING_UTILS_H_
