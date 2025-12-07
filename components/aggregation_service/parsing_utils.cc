// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/aggregation_service/parsing_utils.h"

#include <optional>
#include <string_view>

#include "components/aggregation_service/aggregation_coordinator_utils.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace aggregation_service {

std::optional<url::Origin> ParseAggregationCoordinator(std::string_view str) {
  auto origin = url::Origin::Create(GURL(str));
  if (IsAggregationCoordinatorOriginAllowed(origin)) {
    return origin;
  }
  return std::nullopt;
}

}  // namespace aggregation_service
