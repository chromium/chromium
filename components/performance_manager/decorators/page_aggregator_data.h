// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_PAGE_AGGREGATOR_DATA_H_
#define COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_PAGE_AGGREGATOR_DATA_H_

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

  // Updates the counter of frames holding a Web Lock, holding an IndexedDB lock
  // or using WebRTC. Sets the corresponding page property.
  void UpdateFrameCountForWebLockUsage(bool frame_is_holding_weblock);
  void UpdateFrameCountForIndexedDBLockUsage(
      bool frame_is_holding_indexeddb_lock);
  void UpdateFrameCountForWebRTCUsage(bool frame_uses_web_rtc);

  // Updates the counter of current frames with form interaction and sets the
  // corresponding page-level property.
  void UpdateCurrentFrameCountForFormInteraction(
      bool frame_had_form_interaction);

  // Updates the counter of current frames with user-initiated edits and sets
  // the corresponding page-level property.
  void UpdateCurrentFrameCountForUserEdits(bool frame_had_user_edits);

  base::Value::Dict Describe();

 private:
  raw_ptr<PageNodeImpl> page_node_;

  // The number of frames holding a Web Lock, holding an IndexedDB lock or using
  // WebRTC. This counts all frames, not just the current ones.
  int num_frames_holding_web_lock_ = 0;
  int num_frames_holding_indexeddb_lock_ = 0;
  int num_frames_using_web_rtc_ = 0;

  // The number of current frames which received a form interaction or a
  // user-initiated edit.
  int num_current_frames_with_form_interaction_ = 0;
  int num_current_frames_with_user_edits_ = 0;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_PAGE_AGGREGATOR_DATA_H_
