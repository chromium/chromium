// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_PARSING_UTILS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_PARSING_UTILS_H_

#include <string>

#include "base/component_export.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
absl::optional<absl::uint128> StringToAggregationKeyPiece(const std::string& s);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
bool AggregationKeyIdHasValidLength(const std::string& key);

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_PARSING_UTILS_H_
