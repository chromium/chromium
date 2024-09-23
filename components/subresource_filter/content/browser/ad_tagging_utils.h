// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_AD_TAGGING_UTILS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_AD_TAGGING_UTILS_H_

#include <optional>

#include "components/subresource_filter/core/common/load_policy.h"
#include "third_party/blink/public/mojom/ad_tagging/ad_evidence.mojom-shared.h"

namespace subresource_filter {

// Returns the appropriate FilterListResult, given the result of the most recent
// filter list check. If no LoadPolicy has been computed, i.e. no URL has been
// checked against the filter list for this frame, `load_policy` should be
// `std::nullopt`. Otherwise, `load_policy.value()` should be the result of the
// latest check.
blink::mojom::FilterListResult InterpretLoadPolicyAsEvidence(
    const std::optional<LoadPolicy>& load_policy);

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_AD_TAGGING_UTILS_H_
