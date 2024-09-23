// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AGGREGATION_SERVICE_PARSING_UTILS_H_
#define COMPONENTS_AGGREGATION_SERVICE_PARSING_UTILS_H_

#include <optional>
#include <string_view>

#include "base/component_export.h"

namespace url {
class Origin;
}  // namespace url

namespace aggregation_service {

// Parses aggregation coordinator identifier. Returns `kDefault` if `str` is
// nullptr or is not a pre-defined value.
COMPONENT_EXPORT(AGGREGATION_SERVICE)
std::optional<url::Origin> ParseAggregationCoordinator(std::string_view str);

}  // namespace aggregation_service

#endif  // COMPONENTS_AGGREGATION_SERVICE_PARSING_UTILS_H_
