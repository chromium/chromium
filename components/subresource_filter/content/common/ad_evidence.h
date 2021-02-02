// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_COMMON_AD_EVIDENCE_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_COMMON_AD_EVIDENCE_H_

#include "base/optional.h"
#include "components/subresource_filter/core/common/load_policy.h"

namespace subresource_filter {

enum class FilterListEvidence {
  // No information is available yet.
  kUnknown,
  // No URL the frame has navigated to has been checked against the filter list.
  // This occurs for initial navigations that are either not handled by the
  // network stack or were not committed.
  kNeverChecked,
  // The last URL checked against the filter list did not match any rules.
  kMatchedNoRules,
  // The last URL checked against the filter list matched a blocking rule.
  kMatchedBlockingRule,
  // The last URL checked against the filter list matched an allowing rule.
  kMatchedAllowingRule,
};

enum class ScriptHeuristicEvidence {
  // No information is available yet.
  kUnknown,
  // At the time the frame was created, no ad script was on the v8 stack.
  kNotCreatedByAdScript,
  // At the time the frame was created, ad script was on the v8 stack.
  kCreatedByAdScript
};

// Returns the appropriate FilterListEvidence, given the result of the most
// recent filter list check. If no LoadPolicy has been computed, i.e. no URL has
// been checked against the filter list for this frame, `load_policy` should be
// `base::nullopt`. Otherwise, `load_policy.value()` should be the result of the
// latest check.
FilterListEvidence InterpretLoadPolicyAsEvidence(
    const base::Optional<LoadPolicy>& load_policy);

// Enumeration of evidence for or against a subframe being an ad. Empty optional
// values indicate unknown or not applicable values.
struct FrameAdEvidence {
  explicit FrameAdEvidence(bool parent_is_ad);
  FrameAdEvidence(const FrameAdEvidence&);
  FrameAdEvidence& operator=(const FrameAdEvidence&);

  ~FrameAdEvidence();

  // Returns whether the fields on the struct have all been set, i.e. that the
  // evidence is complete for calculation.
  bool IsPopulated() const;

  // Returns whether the fields indicate that the corresponding subframe is an
  // ad or not. Should only be called once `IsPopulated()`.
  bool IndicatesAdSubframe() const;

  // Whether the frame's parent is an ad.
  bool parent_is_ad;

  // Whether any URL for this frame has been checked against the filter list
  // and, if so, the result of the latest lookup. This is set once the filter
  // list evaluates a frame url, or it is known a frame will not consult the
  // the filter list (and has never done so yet).
  FilterListEvidence filter_list_result = FilterListEvidence::kUnknown;

  // Whether ad script was on the v8 stack at the time this frame was created.
  ScriptHeuristicEvidence created_by_ad_script =
      ScriptHeuristicEvidence::kUnknown;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_COMMON_AD_EVIDENCE_H_
