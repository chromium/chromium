// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/initializing_frame_node_observer.h"

namespace performance_manager {

InitializingFrameNodeObserverManager::InitializingFrameNodeObserverManager() =
    default;

InitializingFrameNodeObserverManager::~InitializingFrameNodeObserverManager() =
    default;

void InitializingFrameNodeObserverManager::AddObserver(
    InitializingFrameNodeObserver* observer) {
  observer_list_.AddObserver(observer);
}

void InitializingFrameNodeObserverManager::RemoveObserver(
    InitializingFrameNodeObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void InitializingFrameNodeObserverManager::NotifyFrameNodeInitializing(
    const FrameNode* frame_node) {
  for (InitializingFrameNodeObserver& observer : observer_list_) {
    observer.OnFrameNodeInitializing(frame_node);
  }
}

void InitializingFrameNodeObserverManager::NotifyFrameNodeTearingDown(
    const FrameNode* frame_node) {
  for (InitializingFrameNodeObserver& observer : observer_list_) {
    observer.OnFrameNodeTearingDown(frame_node);
  }
}

void InitializingFrameNodeObserverManager::OnFrameNodeAdded(
    const FrameNode* frame_node) {
  // Ignore this as this class manually notifies observes of new frame nodes
  // using `OnFrameNodeInitializing()`.
}

void InitializingFrameNodeObserverManager::OnBeforeFrameNodeRemoved(
    const FrameNode* frame_node) {
  // Ignore this as this class manually notifies observes of the removal of
  // frame nodes using `NotifyFrameNodeTearingDown()`.
}

void InitializingFrameNodeObserverManager::OnIsCurrentChanged(
    const FrameNode* frame_node) {
  for (InitializingFrameNodeObserver& observer : observer_list_) {
    observer.OnIsCurrentChanged(frame_node);
  }
}

void InitializingFrameNodeObserverManager::OnNetworkAlmostIdleChanged(
    const FrameNode* frame_node) {
  for (InitializingFrameNodeObserver& observer : observer_list_) {
    observer.OnNetworkAlmostIdleChanged(frame_node);
  }
}

void InitializingFrameNodeObserverManager::OnFrameLifecycleStateChanged(
    const FrameNode* frame_node) {
  for (InitializingFrameNodeObserver& observer : observer_list_) {
    observer.OnFrameLifecycleStateChanged(frame_node);
  }
}

void InitializingFrameNodeObserverManager::OnURLChanged(
    const FrameNode* frame_node,
    const GURL& previous_value) {
  for (InitializingFrameNodeObserver& observer : observer_list_) {
    observer.OnURLChanged(frame_node, previous_value);
  }
}

void InitializingFrameNodeObserverManager::OnIsAdFrameChanged(
    const FrameNode* frame_node) {
  for (InitializingFrameNodeObserver& observer : observer_list_) {
    observer.OnIsAdFrameChanged(frame_node);
  }
}

void InitializingFrameNodeObserverManager::OnFrameIsHoldingWebLockChanged(
    const FrameNode* frame_node) {
  for (InitializingFrameNodeObserver& observer : observer_list_) {
    observer.OnFrameIsHoldingWebLockChanged(frame_node);
  }
}

void InitializingFrameNodeObserverManager::OnFrameIsHoldingIndexedDBLockChanged(
    const FrameNode* frame_node) {
  for (InitializingFrameNodeObserver& observer : observer_list_) {
    observer.OnFrameIsHoldingIndexedDBLockChanged(frame_node);
  }
}

void InitializingFrameNodeObserverManager::OnPriorityAndReasonChanged(
    const FrameNode* frame_node,
    const PriorityAndReason& previous_value) {
  for (InitializingFrameNodeObserver& observer : observer_list_) {
    observer.OnPriorityAndReasonChanged(frame_node, previous_value);
  }
}

void InitializingFrameNodeObserverManager::OnHadFormInteractionChanged(
    const FrameNode* frame_node) {
  for (InitializingFrameNodeObserver& observer : observer_list_) {
    observer.OnHadFormInteractionChanged(frame_node);
  }
}

void InitializingFrameNodeObserverManager::OnHadUserEditsChanged(
    const FrameNode* frame_node) {
  for (InitializingFrameNodeObserver& observer : observer_list_) {
    observer.OnHadUserEditsChanged(frame_node);
  }
}

void InitializingFrameNodeObserverManager::OnIsAudibleChanged(
    const FrameNode* frame_node) {
  for (InitializingFrameNodeObserver& observer : observer_list_) {
    observer.OnIsAudibleChanged(frame_node);
  }
}

void InitializingFrameNodeObserverManager::OnIsCapturingMediaStreamChanged(
    const FrameNode* frame_node) {
  for (InitializingFrameNodeObserver& observer : observer_list_) {
    observer.OnIsCapturingMediaStreamChanged(frame_node);
  }
}

void InitializingFrameNodeObserverManager::OnIntersectsViewportChanged(
    const FrameNode* frame_node) {
  for (InitializingFrameNodeObserver& observer : observer_list_) {
    observer.OnIntersectsViewportChanged(frame_node);
  }
}

void InitializingFrameNodeObserverManager::OnFrameVisibilityChanged(
    const FrameNode* frame_node,
    FrameNode::Visibility previous_value) {
  for (InitializingFrameNodeObserver& observer : observer_list_) {
    observer.OnFrameVisibilityChanged(frame_node, previous_value);
  }
}

void InitializingFrameNodeObserverManager::OnNonPersistentNotificationCreated(
    const FrameNode* frame_node) {
  for (InitializingFrameNodeObserver& observer : observer_list_) {
    observer.OnNonPersistentNotificationCreated(frame_node);
  }
}

void InitializingFrameNodeObserverManager::OnFirstContentfulPaint(
    const FrameNode* frame_node,
    base::TimeDelta time_since_navigation_start) {
  for (InitializingFrameNodeObserver& observer : observer_list_) {
    observer.OnFirstContentfulPaint(frame_node, time_since_navigation_start);
  }
}

}  // namespace performance_manager
