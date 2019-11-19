// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TAB_HELPER_FRAME_NODE_SOURCE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TAB_HELPER_FRAME_NODE_SOURCE_H_

#include "components/performance_manager/frame_node_source.h"

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observer.h"
#include "components/performance_manager/performance_manager_tab_helper.h"

namespace performance_manager {

// An implementation of FrameNodeSource that uses PerformanceManagerTabHelper to
// get frame node information.
class TabHelperFrameNodeSource : public FrameNodeSource,
                                 public PerformanceManagerTabHelper::Observer {
 public:
  TabHelperFrameNodeSource();
  ~TabHelperFrameNodeSource() override;

  // FrameNodeSource:
  FrameNodeImpl* GetFrameNode(int render_process_id, int frame_id) override;
  void SubscribeToFrameNode(int render_process_id,
                            int frame_id,
                            OnbeforeFrameNodeRemovedCallback
                                on_before_frame_node_removed_callback) override;
  void UnsubscribeFromFrameNode(int render_process_id, int frame_id) override;

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
  base::flat_map<PerformanceManagerTabHelper*, base::flat_set<FrameNodeImpl*>>
      observed_frame_nodes_;

  // Observes frame node deletions.
  ScopedObserver<PerformanceManagerTabHelper,
                 PerformanceManagerTabHelper::Observer>
      performance_manager_tab_helper_observers_;

  DISALLOW_COPY_AND_ASSIGN(TabHelperFrameNodeSource);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TAB_HELPER_FRAME_NODE_SOURCE_H_
