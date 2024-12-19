// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/page_aggregator_data.h"

#include "base/check_op.h"
#include "components/performance_manager/graph/page_node_impl.h"

namespace performance_manager {

PageAggregatorData::PageAggregatorData(PageNodeImpl* page_node)
    : page_node_(page_node) {}

PageAggregatorData::~PageAggregatorData() {
  DCHECK_EQ(num_frames_holding_web_lock_, 0);
  DCHECK_EQ(num_frames_holding_blocking_indexeddb_lock_, 0);
  DCHECK_EQ(num_frames_using_web_rtc_, 0);
  DCHECK_EQ(num_current_frames_with_form_interaction_, 0);
  DCHECK_EQ(num_current_frames_with_user_edits_, 0);
}

void PageAggregatorData::UpdateFrameCountForWebLockUsage(
    bool frame_is_holding_weblock) {
  if (frame_is_holding_weblock) {
    ++num_frames_holding_web_lock_;
  } else {
    DCHECK_GT(num_frames_holding_web_lock_, 0);
    --num_frames_holding_web_lock_;
  }

  page_node_->SetIsHoldingWebLock(PassKey(), num_frames_holding_web_lock_ > 0);
}

void PageAggregatorData::UpdateFrameCountForBlockingIndexedDBLockUsage(
    bool frame_is_holding_blocking_indexeddb_lock) {
  if (frame_is_holding_blocking_indexeddb_lock) {
    ++num_frames_holding_blocking_indexeddb_lock_;
  } else {
    DCHECK_GT(num_frames_holding_blocking_indexeddb_lock_, 0);
    --num_frames_holding_blocking_indexeddb_lock_;
  }

  page_node_->SetIsHoldingBlockingIndexedDBLock(
      PassKey(), num_frames_holding_blocking_indexeddb_lock_ > 0);
}

void PageAggregatorData::UpdateFrameCountForWebRTCUsage(
    bool frame_uses_web_rtc) {
  if (frame_uses_web_rtc) {
    ++num_frames_using_web_rtc_;
  } else {
    DCHECK_GT(num_frames_using_web_rtc_, 0);
    --num_frames_using_web_rtc_;
  }

  page_node_->SetUsesWebRTC(PassKey(), num_frames_using_web_rtc_ > 0);
}

void PageAggregatorData::UpdateCurrentFrameCountForFormInteraction(
    bool frame_had_form_interaction) {
  if (frame_had_form_interaction) {
    ++num_current_frames_with_form_interaction_;
  } else {
    DCHECK_GT(num_current_frames_with_form_interaction_, 0);
    --num_current_frames_with_form_interaction_;
  }

  page_node_->SetHadFormInteraction(
      PassKey(), num_current_frames_with_form_interaction_ > 0);
}

void PageAggregatorData::UpdateCurrentFrameCountForUserEdits(
    bool frame_had_user_edits) {
  if (frame_had_user_edits) {
    ++num_current_frames_with_user_edits_;
  } else {
    DCHECK_GT(num_current_frames_with_user_edits_, 0);
    --num_current_frames_with_user_edits_;
  }

  page_node_->SetHadUserEdits(PassKey(),
                              num_current_frames_with_user_edits_ > 0);
}

void PageAggregatorData::UpdateCurrentFrameCountForFreezingOriginTrialOptOut(
    bool frame_has_freezing_origin_trial_opt_out) {
  if (frame_has_freezing_origin_trial_opt_out) {
    ++num_current_frames_with_freezing_origin_trial_opt_out_;
  } else {
    DCHECK_GT(num_current_frames_with_freezing_origin_trial_opt_out_, 0);
    --num_current_frames_with_freezing_origin_trial_opt_out_;
  }

  page_node_->SetHasFreezingOriginTrialOptOut(
      PassKey(), num_current_frames_with_freezing_origin_trial_opt_out_ > 0);
}

base::Value::Dict PageAggregatorData::Describe() {
  base::Value::Dict ret;
  ret.Set("num_frames_holding_web_lock", num_frames_holding_web_lock_);
  ret.Set("num_frames_holding_blocking_indexeddb_lock",
          num_frames_holding_blocking_indexeddb_lock_);
  ret.Set("num_frames_using_web_rtc", num_frames_using_web_rtc_);
  ret.Set("num_current_frames_with_form_interaction",
          num_current_frames_with_form_interaction_);
  ret.Set("num_current_frames_with_user_edits",
          num_current_frames_with_user_edits_);
  return ret;
}

}  // namespace performance_manager
