// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_INITIALIZING_FRAME_NODE_OBSERVER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_INITIALIZING_FRAME_NODE_OBSERVER_H_

#include "base/observer_list.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "url/origin.h"

namespace performance_manager {

// This class is for FrameNodeObservers that need to initialize values on the
// actual frame nodes before other observers are notified of their existence.
// This is needed because modifying frames nodes during the `OnFrameNodeAdded()`
// notification is disallowed. This class should be used sparingly.
class InitializingFrameNodeObserver {
 public:
  virtual ~InitializingFrameNodeObserver() = default;

  // Called when a `frame_node` that is being added to the graph is
  // initializing. This should be used to set the initial value for a property.
  virtual void OnFrameNodeInitializing(const FrameNode* frame_node) {}

  // Called when a `frame_node` that is being removed from the graph is tearing
  // down. Used to clean up any state related to this node.
  virtual void OnFrameNodeTearingDown(const FrameNode* frame_node) {}

  // Same interface as FrameNodeObserver. Look up frame_node.h for their
  // descriptions.
  virtual void OnCurrentFrameChanged(const FrameNode* previous_frame_node,
                                     const FrameNode* current_frame_node) {}
  virtual void OnNetworkAlmostIdleChanged(const FrameNode* frame_node) {}
  virtual void OnFrameLifecycleStateChanged(const FrameNode* frame_node) {}
  virtual void OnURLChanged(const FrameNode* frame_node,
                            const GURL& previous_value) {}
  virtual void OnOriginChanged(
      const FrameNode* frame_node,
      const std::optional<url::Origin>& previous_value) {}
  virtual void OnIsAdFrameChanged(const FrameNode* frame_node) {}
  virtual void OnFrameIsHoldingWebLockChanged(const FrameNode* frame_node) {}
  virtual void OnFrameIsHoldingIndexedDBLockChanged(
      const FrameNode* frame_node) {}
  virtual void OnPriorityAndReasonChanged(
      const FrameNode* frame_node,
      const PriorityAndReason& previous_value) {}
  virtual void OnHadUserActivationChanged(const FrameNode* frame_node) {}
  virtual void OnHadFormInteractionChanged(const FrameNode* frame_node) {}
  virtual void OnHadUserEditsChanged(const FrameNode* frame_node) {}
  virtual void OnFrameUsesWebRTCChanged(const FrameNode* frame_node) {}
  virtual void OnIsAudibleChanged(const FrameNode* frame_node) {}
  virtual void OnIsCapturingMediaStreamChanged(const FrameNode* frame_node) {}
  virtual void OnViewportIntersectionChanged(const FrameNode* frame_node) {}
  virtual void OnFrameVisibilityChanged(const FrameNode* frame_node,
                                        FrameNode::Visibility previous_value) {}
  virtual void OnIsImportantChanged(const FrameNode* frame_node) {}
  virtual void OnNonPersistentNotificationCreated(const FrameNode* frame_node) {
  }
  virtual void OnFirstContentfulPaint(
      const FrameNode* frame_node,
      base::TimeDelta time_since_navigation_start) {}
};

class InitializingFrameNodeObserverManager final : public FrameNodeObserver {
 public:
  InitializingFrameNodeObserverManager();
  ~InitializingFrameNodeObserverManager() override;

  // Adds/removes observers.
  void AddObserver(InitializingFrameNodeObserver* observer);
  void RemoveObserver(InitializingFrameNodeObserver* observer);

  // Used to notify all observers when the frame node is added, while the node
  // is still modifiable.
  void NotifyFrameNodeInitializing(const FrameNode* frame_node);
  // Used to notify all observers when the frame node is about to be removed,
  // while the node is modifiable.
  void NotifyFrameNodeTearingDown(const FrameNode* frame_node);

 private:
  // FrameNodeObserver:
  void OnFrameNodeAdded(const FrameNode* frame_node) override;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override;
  void OnCurrentFrameChanged(const FrameNode* previous_frame_node,
                             const FrameNode* current_frame_node) override;
  void OnNetworkAlmostIdleChanged(const FrameNode* frame_node) override;
  void OnFrameLifecycleStateChanged(const FrameNode* frame_node) override;
  void OnURLChanged(const FrameNode* frame_node,
                    const GURL& previous_value) override;
  void OnOriginChanged(
      const FrameNode* frame_node,
      const std::optional<url::Origin>& previous_value) override;
  void OnIsAdFrameChanged(const FrameNode* frame_node) override;
  void OnFrameIsHoldingWebLockChanged(const FrameNode* frame_node) override;
  void OnFrameIsHoldingIndexedDBLockChanged(
      const FrameNode* frame_node) override;
  void OnPriorityAndReasonChanged(
      const FrameNode* frame_node,
      const PriorityAndReason& previous_value) override;
  void OnHadUserActivationChanged(const FrameNode* frame_node) override;
  void OnHadFormInteractionChanged(const FrameNode* frame_node) override;
  void OnHadUserEditsChanged(const FrameNode* frame_node) override;
  void OnFrameUsesWebRTCChanged(const FrameNode* frame_node) override;
  void OnIsAudibleChanged(const FrameNode* frame_node) override;
  void OnIsCapturingMediaStreamChanged(const FrameNode* frame_node) override;
  void OnViewportIntersectionChanged(const FrameNode* frame_node) override;
  void OnFrameVisibilityChanged(const FrameNode* frame_node,
                                FrameNode::Visibility previous_value) override;
  void OnIsImportantChanged(const FrameNode* frame_node) override;
  void OnNonPersistentNotificationCreated(const FrameNode* frame_node) override;
  void OnFirstContentfulPaint(
      const FrameNode* frame_node,
      base::TimeDelta time_since_navigation_start) override;

  base::ObserverList<InitializingFrameNodeObserver>::Unchecked observer_list_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_INITIALIZING_FRAME_NODE_OBSERVER_H_
