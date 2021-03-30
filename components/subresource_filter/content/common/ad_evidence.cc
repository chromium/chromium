// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "components/subresource_filter/content/common/ad_evidence.h"

namespace subresource_filter {

FilterListEvidence InterpretLoadPolicyAsEvidence(
    const base::Optional<LoadPolicy>& load_policy) {
  if (!load_policy.has_value()) {
    return FilterListEvidence::kNotChecked;
  }
  switch (load_policy.value()) {
    case LoadPolicy::EXPLICITLY_ALLOW:
      return FilterListEvidence::kMatchedAllowingRule;
    case LoadPolicy::ALLOW:
      return FilterListEvidence::kMatchedNoRules;
    case LoadPolicy::WOULD_DISALLOW:
    case LoadPolicy::DISALLOW:
      return FilterListEvidence::kMatchedBlockingRule;
  }
}

FilterListEvidence MoreRestrictiveFilterListEvidence(FilterListEvidence a,
                                                     FilterListEvidence b) {
  return std::max(a, b);
}

FrameAdEvidence::FrameAdEvidence(bool parent_is_ad)
    : parent_is_ad_(parent_is_ad) {}

FrameAdEvidence::FrameAdEvidence(const FrameAdEvidence&) = default;

FrameAdEvidence::~FrameAdEvidence() = default;

bool FrameAdEvidence::IndicatesAdSubframe() const {
  DCHECK(is_complete_);

  // We tag a frame as an ad if its parent is one, it was created by ad script
  // or the frame has ever navigated to an URL matching a blocking rule.
  return parent_is_ad_ ||
         created_by_ad_script_ == ScriptHeuristicEvidence::kCreatedByAdScript ||
         most_restrictive_filter_list_result_ ==
             FilterListEvidence::kMatchedBlockingRule;
}

void FrameAdEvidence::UpdateFilterListResult(FilterListEvidence value) {
  latest_filter_list_result_ = value;
  most_restrictive_filter_list_result_ = MoreRestrictiveFilterListEvidence(
      most_restrictive_filter_list_result_, value);
}

}  // namespace subresource_filter
