// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TAB_HELPER_FRAME_NODE_SOURCE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TAB_HELPER_FRAME_NODE_SOURCE_H_

#include "base/memory/raw_ptr.h"
#include "components/performance_manager/frame_node_source.h"

#include "base/observer_list_types.h"
#include "base/scoped_multi_source_observation.h"
#include "components/performance_manager/performance_manager_tab_helper.h"

namespace performance_manager {

// An implementation of FrameNodeSource that uses PerformanceManagerTabHelper to
// get frame node information.
class TabHelperFrameNodeSource : public FrameNodeSource,
                                 public PerformanceManagerTabHelper::Observer {
 public:
  TabHelperFrameNodeSource();

  TabHelperFrameNodeSource(const TabHelperFrameNodeSource&) = delete;
  TabHelperFrameNodeSource& operator=(const TabHelperFrameNodeSource&) = delete;

  ~TabHelperFrameNodeSource() override;

  // FrameNodeSource:
  FrameNodeImpl* GetFrameNode(
      content::GlobalRenderFrameHostId render_process_host_id) override;
  void SubscribeToFrameNode(
      content::GlobalRenderFrameHostId render_process_host_id,
      OnbeforeFrameNodeRemovedCallback on_before_frame_node_removed_callback)
      override;
  void UnsubscribeFromFrameNode(
      content::GlobalRenderFrameHostId render_process_host_id) override;

  // PerformanceManagerTabHelper::Observer:
  void OnBeforeFrameNodeRemoved(
      PerformanceManagerTabHelper* performance_manager_tab_helper,
      FrameNodeImpl* frame_node) override;

 private:
  // Adds |frame_node| to the set of observed frame nodes associated with
  // |performance_manager_tab_helper|. Returns true if |frame_node| was the
  // first frame added to that set.
  bool AddObservedFrameNode(
      PerformanceManagerTabHelper* performance_manager_tab_helper,
      FrameNodeImpl* frame_node);

  // Removes |frame_node| to the set of observed frame nodes associated with
  // |performance_manager_tab_helper|. Returns true if |frame_node| was the
  // last frame removed from that set.
  bool RemoveObservedFrameNode(
      PerformanceManagerTabHelper* performance_manager_tab_helper,
      FrameNodeImpl* frame_node);

  // Maps each observed frame node to their callback.
  base::flat_map<FrameNodeImpl*, OnbeforeFrameNodeRemovedCallback>
      frame_node_callbacks_;

  // Maps each tab helper to the set of observed frame nodes that belongs to
  // that tab helper.
  base::flat_map<PerformanceManagerTabHelper*,
                 base::flat_set<raw_ptr<FrameNodeImpl, CtnExperimental>>>
      observed_frame_nodes_;

  // Observes frame node deletions.
  base::ScopedMultiSourceObservation<PerformanceManagerTabHelper,
                                     PerformanceManagerTabHelper::Observer>
      performance_manager_tab_helper_observations_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TAB_HELPER_FRAME_NODE_SOURCE_H_
