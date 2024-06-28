// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/page_aggregator_data.h"

#include "base/check_op.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph_operations.h"

namespace performance_manager {

PageAggregatorData::~PageAggregatorData() {
  DCHECK_EQ(num_frames_holding_web_lock_, 0U);
  DCHECK_EQ(num_frames_holding_indexeddb_lock_, 0U);
  DCHECK_EQ(num_current_frames_with_form_interaction_, 0U);
}

void PageAggregatorData::UpdateFrameCountForWebLockUsage(
    bool frame_is_holding_weblock,
    PageNodeImpl* page_node) {
  if (frame_is_holding_weblock) {
    ++num_frames_holding_web_lock_;
  } else {
    DCHECK_GT(num_frames_holding_web_lock_, 0U);
    --num_frames_holding_web_lock_;
  }

  page_node->SetIsHoldingWebLock(PassKey(), num_frames_holding_web_lock_ > 0);
}

void PageAggregatorData::UpdateFrameCountForIndexedDBLockUsage(
    bool frame_is_holding_indexeddb_lock,
    PageNodeImpl* page_node) {
  if (frame_is_holding_indexeddb_lock) {
    ++num_frames_holding_indexeddb_lock_;
  } else {
    DCHECK_GT(num_frames_holding_indexeddb_lock_, 0U);
    --num_frames_holding_indexeddb_lock_;
  }

  page_node->SetIsHoldingIndexedDBLock(PassKey(),
                                       num_frames_holding_indexeddb_lock_ > 0);
}

void PageAggregatorData::UpdateCurrentFrameCountForFormInteraction(
    bool frame_had_form_interaction,
    PageNodeImpl* page_node,
    const FrameNode* frame_node_being_removed) {
  if (frame_had_form_interaction) {
    ++num_current_frames_with_form_interaction_;
  } else {
    DCHECK_GT(num_current_frames_with_form_interaction_, 0U);
    --num_current_frames_with_form_interaction_;
  }
  // DCHECK that the |num_current_frames_with_form_interaction_| accounting is
  // correct.
  DCHECK_EQ(
      [&]() {
        const auto frame_nodes = GraphOperations::GetFrameNodes(page_node);
        size_t num_current_frames_with_form_interaction = 0;
        for (const auto* node : frame_nodes) {
          if (node != frame_node_being_removed && node->IsCurrent() &&
              node->HadFormInteraction()) {
            ++num_current_frames_with_form_interaction;
          }
        }
        return num_current_frames_with_form_interaction;
      }(),
      num_current_frames_with_form_interaction_);

  page_node->SetHadFormInteraction(
      PassKey(), num_current_frames_with_form_interaction_ > 0);
}

void PageAggregatorData::UpdateCurrentFrameCountForUserEdits(
    bool frame_had_user_edits,
    PageNodeImpl* page_node,
    const FrameNode* frame_node_being_removed) {
  if (frame_had_user_edits) {
    ++num_current_frames_with_user_edits_;
  } else {
    DCHECK_GT(num_current_frames_with_user_edits_, 0U);
    --num_current_frames_with_user_edits_;
  }
  // DCHECK that the |num_current_frames_with_user_edits_| accounting is
  // correct.
  DCHECK_EQ(
      [&]() {
        const auto frame_nodes = GraphOperations::GetFrameNodes(page_node);
        size_t num_current_frames_with_user_edits = 0;
        for (const auto* node : frame_nodes) {
          if (node != frame_node_being_removed && node->IsCurrent() &&
              node->HadUserEdits()) {
            ++num_current_frames_with_user_edits;
          }
        }
        return num_current_frames_with_user_edits;
      }(),
      num_current_frames_with_user_edits_);

  page_node->SetHadUserEdits(PassKey(),
                             num_current_frames_with_user_edits_ > 0);
}

base::Value::Dict PageAggregatorData::Describe() {
  base::Value::Dict ret;
  ret.Set("num_frames_holding_web_lock",
          static_cast<int>(num_frames_holding_web_lock_));
  ret.Set("num_frames_holding_indexeddb_lock",
          static_cast<int>(num_frames_holding_indexeddb_lock_));
  ret.Set("num_current_frames_with_form_interaction",
          static_cast<int>(num_current_frames_with_form_interaction_));
  return ret;
}

}  // namespace performance_manager
