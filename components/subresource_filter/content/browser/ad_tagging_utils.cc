// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/ad_tagging_utils.h"

#include <optional>

#include "components/subresource_filter/core/common/load_policy.h"
#include "third_party/blink/public/mojom/ad_tagging/ad_evidence.mojom-shared.h"

namespace subresource_filter {

blink::mojom::FilterListResult InterpretLoadPolicyAsEvidence(
    const std::optional<LoadPolicy>& load_policy) {
  if (!load_policy.has_value()) {
    return blink::mojom::FilterListResult::kNotChecked;
  }
  switch (load_policy.value()) {
    case LoadPolicy::EXPLICITLY_ALLOW:
      return blink::mojom::FilterListResult::kMatchedAllowingRule;
    case LoadPolicy::ALLOW:
      return blink::mojom::FilterListResult::kMatchedNoRules;
    case LoadPolicy::WOULD_DISALLOW:
    case LoadPolicy::DISALLOW:
      return blink::mojom::FilterListResult::kMatchedBlockingRule;
  }
}

}  // namespace subresource_filter
