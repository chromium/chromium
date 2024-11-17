// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_FRAME_VISIBILITY_DECORATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_FRAME_VISIBILITY_DECORATOR_H_

#include "components/performance_manager/graph/initializing_frame_node_observer.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace performance_manager {

// Decorates each frame with their visibility, which is based on its viewport
// intersection and the visibility of the page containing the frame.
//
// A page is considered "user visible" if its `IsVisible()` property is true, or
// if it is being mirrored (PageLiveStateDecorator::Data::IsBeingMirrored).
//
// When the visibility of the frame cannot be determined, it is assigned a value
// of FrameNode::Visibility::kUnknown. This can happen early in the lifetime of
// a frame, where it hasn't been assigned its viewport intersection yet.
class FrameVisibilityDecorator : public GraphOwnedDefaultImpl,
                                 public PageNode::ObserverDefaultImpl,
                                 public PageLiveStateObserver,
                                 public InitializingFrameNodeObserver {
 public:
  FrameVisibilityDecorator();
  ~FrameVisibilityDecorator() override;

  FrameVisibilityDecorator(const FrameVisibilityDecorator&) = delete;
  FrameVisibilityDecorator& operator=(const FrameVisibilityDecorator&) = delete;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // PageNodeObserver:
  void OnPageNodeAdded(const PageNode* page_node) override;
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;
  void OnIsVisibleChanged(const PageNode* page_node) override;

  // PageLiveStateObserver:
  void OnIsBeingMirroredChanged(const PageNode* page_node) override;

  // Unused from PageLiveStateObserver:
  void OnIsConnectedToUSBDeviceChanged(const PageNode* page_node) override {}
  void OnIsConnectedToBluetoothDeviceChanged(
      const PageNode* page_node) override {}
  void OnIsConnectedToHidDeviceChanged(const PageNode* page_node) override {}
  void OnIsConnectedToSerialPortChanged(const PageNode* page_node) override {}
  void OnIsCapturingVideoChanged(const PageNode* page_node) override {}
  void OnIsCapturingAudioChanged(const PageNode* page_node) override {}
  void OnIsCapturingWindowChanged(const PageNode* page_node) override {}
  void OnIsCapturingDisplayChanged(const PageNode* page_node) override {}
  void OnIsAutoDiscardableChanged(const PageNode* page_node) override {}
  void OnWasDiscardedChanged(const PageNode* page_node) override {}
  void OnIsActiveTabChanged(const PageNode* page_node) override {}
  void OnIsPinnedTabChanged(const PageNode* page_node) override {}
  void OnIsDevToolsOpenChanged(const PageNode* page_node) override {}
  void OnAccessibilityModeChanged(const PageNode* page_node) override {}

  // FrameNodeObserver:
  void OnFrameNodeInitializing(const FrameNode* frame_node) override;
  void OnCurrentFrameChanged(const FrameNode* previous_frame_node,
                             const FrameNode* current_frame_node) override;
  void OnViewportIntersectionChanged(const FrameNode* frame_node) override;

 private:
  // Handles changes in the user visibility of pages.
  void OnPageUserVisibilityChanged(const PageNode* page_node,
                                   bool page_is_user_visible);

  // Handles changes to a frame's property.
  void OnFramePropertyChanged(const FrameNode* frame_node);

  // Returns true if the page node is visible to the user in some capacity,
  // taking into account if the page is being mirrored.
  static bool IsPageUserVisible(const PageNode* page_node);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_FRAME_VISIBILITY_DECORATOR_H_
