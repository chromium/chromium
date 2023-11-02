// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_COMMON_SUBRESOURCE_FILTER_UTILS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_COMMON_SUBRESOURCE_FILTER_UTILS_H_

#include "components/subresource_filter/core/common/load_policy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/ad_tagging/ad_evidence.mojom-shared.h"

class GURL;

namespace subresource_filter {

// Child frame navigations and initial root frame navigations matching these
// URLs/ schemes will not trigger ReadyToCommitNavigation in the browser
// process, so they must be treated specially to maintain activation. Each
// should inherit the activation of its parent in the case of a child frame and
// its opener in the case of a root frame. This also accounts for the ability of
// the parent/opener to affect the frame's content more directly, e.g. through
// document.write(), even though these URLs won't match a filter list rule by
// themselves.
bool ShouldInheritActivation(const GURL& url);

// Returns the appropriate FilterListResult, given the result of the most recent
// filter list check. If no LoadPolicy has been computed, i.e. no URL has been
// checked against the filter list for this frame, `load_policy` should be
// `absl::nullopt`. Otherwise, `load_policy.value()` should be the result of the
// latest check.
blink::mojom::FilterListResult InterpretLoadPolicyAsEvidence(
    const absl::optional<LoadPolicy>& load_policy);

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_COMMON_SUBRESOURCE_FILTER_UTILS_H_
