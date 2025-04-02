// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_GRAPH_MOCK_FRAME_NODE_OBSERVER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_GRAPH_MOCK_FRAME_NODE_OBSERVER_H_

#include "components/performance_manager/public/graph/frame_node.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace performance_manager {

class LenientMockFrameNodeObserver : public FrameNodeObserver {
 public:
  LenientMockFrameNodeObserver();
  ~LenientMockFrameNodeObserver() override;

  MOCK_METHOD(void,
              OnBeforeFrameNodeAdded,
              (const FrameNode*,
               const FrameNode*,
               const PageNode*,
               const ProcessNode*,
               const FrameNode*),
              (override));
  MOCK_METHOD(void, OnFrameNodeAdded, (const FrameNode*), (override));
  MOCK_METHOD(void, OnBeforeFrameNodeRemoved, (const FrameNode*), (override));
  MOCK_METHOD(void,
              OnFrameNodeRemoved,
              (const FrameNode*,
               const FrameNode*,
               const PageNode*,
               const ProcessNode*,
               const FrameNode*),
              (override));
  MOCK_METHOD(void,
              OnCurrentFrameChanged,
              (const FrameNode*, const FrameNode*),
              (override));
  MOCK_METHOD(void, OnNetworkAlmostIdleChanged, (const FrameNode*), (override));
  MOCK_METHOD(void,
              OnFrameLifecycleStateChanged,
              (const FrameNode*),
              (override));
  MOCK_METHOD(void, OnURLChanged, (const FrameNode*, const GURL&), (override));
  MOCK_METHOD(void,
              OnOriginChanged,
              (const FrameNode*, const std::optional<url::Origin>&),
              (override));
  MOCK_METHOD(void, OnIsAdFrameChanged, (const FrameNode*), (override));
  MOCK_METHOD(void,
              OnFrameIsHoldingWebLockChanged,
              (const FrameNode*),
              (override));
  MOCK_METHOD(void,
              OnFrameIsHoldingBlockingIndexedDBLockChanged,
              (const FrameNode*),
              (override));
  MOCK_METHOD(void,
              OnPriorityAndReasonChanged,
              (const FrameNode*, const PriorityAndReason& previous_value),
              (override));
  MOCK_METHOD(void, OnFrameUsesWebRTCChanged, (const FrameNode*), (override));
  MOCK_METHOD(void, OnHadUserActivationChanged, (const FrameNode*), (override));
  MOCK_METHOD(void,
              OnHadFormInteractionChanged,
              (const FrameNode*),
              (override));
  MOCK_METHOD(void, OnHadUserEditsChanged, (const FrameNode*), (override));
  MOCK_METHOD(void, OnIsAudibleChanged, (const FrameNode*), (override));
  MOCK_METHOD(void,
              OnIsCapturingMediaStreamChanged,
              (const FrameNode*),
              (override));
  MOCK_METHOD(void,
              OnFrameHasFreezingOriginTrialOptOutChanged,
              (const FrameNode*),
              (override));
  MOCK_METHOD(void,
              OnViewportIntersectionChanged,
              (const FrameNode*),
              (override));
  MOCK_METHOD(void,
              OnFrameVisibilityChanged,
              (const FrameNode*, FrameNode::Visibility),
              (override));
  MOCK_METHOD(void,
              OnIsIntersectingLargeAreaChanged,
              (const FrameNode*),
              (override));
  MOCK_METHOD(void, OnIsImportantChanged, (const FrameNode*), (override));
  MOCK_METHOD(void,
              OnNonPersistentNotificationCreated,
              (const FrameNode*),
              (override));
  MOCK_METHOD(void,
              OnFirstContentfulPaint,
              (const FrameNode*, base::TimeDelta),
              (override));
};

using MockFrameNodeObserver =
    ::testing::StrictMock<LenientMockFrameNodeObserver>;

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_GRAPH_MOCK_FRAME_NODE_OBSERVER_H_
