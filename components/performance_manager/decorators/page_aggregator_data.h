// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_PAGE_AGGREGATOR_DATA_H_
#define COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_PAGE_AGGREGATOR_DATA_H_

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "components/performance_manager/graph/node_inline_data.h"

namespace performance_manager {

class PageNodeImpl;

class PageAggregatorData : public SparseNodeInlineData<PageAggregatorData> {
 public:
  using PassKey = base::PassKey<PageAggregatorData>;

  explicit PageAggregatorData(PageNodeImpl* page_node);
  ~PageAggregatorData();

  PageAggregatorData(const PageAggregatorData&) = delete;
  PageAggregatorData& operator=(const PageAggregatorData&) = delete;
  PageAggregatorData(PageAggregatorData&&) = delete;
  PageAggregatorData& operator=(PageAggregatorData&&) = delete;

  // Updates the counter of frames using various web features. Sets the
  // corresponding page property.
  void UpdateFrameCountForWebLockUsage(bool frame_is_holding_weblock);
  void UpdateFrameCountForBlockingIndexedDBLockUsage(
      bool frame_is_holding_blocking_indexeddb_lock);
  void UpdateFrameCountForWebRTCUsage(bool frame_uses_web_rtc);

  // Updates the counter of *current* frames with form interaction,
  // user-initiated edits or freezing origin trial opt-out. Sets the
  // corresponding page-level property.
  void UpdateCurrentFrameCountForFormInteraction(
      bool frame_had_form_interaction);
  void UpdateCurrentFrameCountForUserEdits(bool frame_had_user_edits);
  void UpdateCurrentFrameCountForFreezingOriginTrialOptOut(
      bool frame_has_freezing_origin_trial_opt_out);

  base::Value::Dict Describe();

 private:
  raw_ptr<PageNodeImpl> page_node_;

  // The number of frames using various web features.
  int num_frames_holding_web_lock_ = 0;
  int num_frames_holding_blocking_indexeddb_lock_ = 0;
  int num_frames_using_web_rtc_ = 0;

  // The number of *current* frames which with form interaction, user-initiated
  // edit or freezing origin trial opt-out.
  int num_current_frames_with_form_interaction_ = 0;
  int num_current_frames_with_user_edits_ = 0;
  int num_current_frames_with_freezing_origin_trial_opt_out_ = 0;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_PAGE_AGGREGATOR_DATA_H_
