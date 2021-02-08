// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_COMMON_AD_EVIDENCE_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_COMMON_AD_EVIDENCE_H_

#include "base/optional.h"
#include "components/subresource_filter/core/common/load_policy.h"

namespace subresource_filter {

enum class FilterListEvidence {
  // No URL the frame has navigated to has been checked against the filter list.
  // This occurs for initial navigations that are either not handled by the
  // network stack or were not committed.
  kNotChecked,
  // The last URL checked against the filter list did not match any rules.
  kMatchedNoRules,
  // The last URL checked against the filter list matched a blocking rule.
  kMatchedBlockingRule,
  // The last URL checked against the filter list matched an allowing rule.
  kMatchedAllowingRule,
};

enum class ScriptHeuristicEvidence {
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

// Enumeration of evidence for or against a subframe being an ad.
class FrameAdEvidence {
 public:
  explicit FrameAdEvidence(bool parent_is_ad);
  FrameAdEvidence(const FrameAdEvidence&);

  ~FrameAdEvidence();

  // Returns whether the fields indicate that the corresponding subframe is an
  // ad or not. Should only be called once `is_complete()`.
  bool IndicatesAdSubframe() const;

  // Indicates whether the fields on the class are ready to be used for
  // calculation. If false, some fields might represent defaults rather than the
  // truth. Once set (as true), this will not change further. For example, this
  // bit should not be set during an initial navigation while waiting on an IPC
  // message that might change one of the fields from its default value. Once it
  // we know that no more updates will occur for the navigation,
  // `set_is_complete()` should be called.
  bool is_complete() const { return is_complete_; }
  void set_is_complete() { is_complete_ = true; }

  bool parent_is_ad() const { return parent_is_ad_; }

  FilterListEvidence filter_list_result() const { return filter_list_result_; }
  void set_filter_list_result(FilterListEvidence value) {
    filter_list_result_ = value;
  }

  ScriptHeuristicEvidence created_by_ad_script() const {
    return created_by_ad_script_;
  }
  void set_created_by_ad_script(ScriptHeuristicEvidence value) {
    created_by_ad_script_ = value;
  }

 private:
  // See `is_complete()`.
  bool is_complete_ = false;

  // Whether the frame's parent is an ad.
  const bool parent_is_ad_;

  // Whether any URL for this frame has been checked against the filter list
  // and, if so, the result of the latest lookup. This is set once the filter
  // list evaluates a frame url, or it is known a frame will not consult the
  // the filter list (and has never done so yet).
  FilterListEvidence filter_list_result_ = FilterListEvidence::kNotChecked;

  // Whether ad script was on the v8 stack at the time this frame was created.
  ScriptHeuristicEvidence created_by_ad_script_ =
      ScriptHeuristicEvidence::kNotCreatedByAdScript;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_COMMON_AD_EVIDENCE_H_
