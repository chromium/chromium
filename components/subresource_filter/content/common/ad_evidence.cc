// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/common/ad_evidence.h"

namespace subresource_filter {

FilterListEvidence InterpretLoadPolicyAsEvidence(
    const base::Optional<LoadPolicy>& load_policy) {
  if (!load_policy.has_value()) {
    return FilterListEvidence::kNeverChecked;
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

FrameAdEvidence::FrameAdEvidence(bool parent_is_ad)
    : parent_is_ad(parent_is_ad) {}

FrameAdEvidence::FrameAdEvidence(const FrameAdEvidence&) = default;

FrameAdEvidence& FrameAdEvidence::operator=(const FrameAdEvidence&) = default;

FrameAdEvidence::~FrameAdEvidence() = default;

bool FrameAdEvidence::IsPopulated() const {
  return created_by_ad_script != ScriptHeuristicEvidence::kUnknown &&
         filter_list_result != FilterListEvidence::kUnknown;
}

bool FrameAdEvidence::IndicatesAdSubframe() const {
  DCHECK(IsPopulated());

  return parent_is_ad ||
         filter_list_result == FilterListEvidence::kMatchedBlockingRule ||
         created_by_ad_script == ScriptHeuristicEvidence::kCreatedByAdScript;
}

}  // namespace subresource_filter
