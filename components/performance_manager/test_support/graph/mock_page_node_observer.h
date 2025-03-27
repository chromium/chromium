// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_GRAPH_MOCK_PAGE_NODE_OBSERVER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_GRAPH_MOCK_PAGE_NODE_OBSERVER_H_

#include "components/performance_manager/public/graph/page_node.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace performance_manager {

class LenientMockPageNodeObserver : public PageNodeObserver {
 public:
  LenientMockPageNodeObserver();
  ~LenientMockPageNodeObserver() override;

  MOCK_METHOD(void, OnBeforePageNodeAdded, (const PageNode*), (override));
  MOCK_METHOD(void, OnPageNodeAdded, (const PageNode*), (override));
  MOCK_METHOD(void, OnBeforePageNodeRemoved, (const PageNode*), (override));
  MOCK_METHOD(void, OnPageNodeRemoved, (const PageNode*), (override));
  MOCK_METHOD(void,
              OnOpenerFrameNodeChanged,
              (const PageNode*, const FrameNode*),
              (override));
  MOCK_METHOD(void,
              OnEmbedderFrameNodeChanged,
              (const PageNode*, const FrameNode*),
              (override));
  MOCK_METHOD(void, OnTypeChanged, (const PageNode*, PageType), (override));
  MOCK_METHOD(void, OnIsFocusedChanged, (const PageNode*), (override));
  MOCK_METHOD(void, OnIsVisibleChanged, (const PageNode*), (override));
  MOCK_METHOD(void, OnIsAudibleChanged, (const PageNode*), (override));
  MOCK_METHOD(void,
              OnPageHasFreezingOriginTrialOptOutChanged,
              (const PageNode*),
              (override));
  MOCK_METHOD(void,
              OnHasPictureInPictureChanged,
              (const PageNode*),
              (override));
  MOCK_METHOD(void,
              OnLoadingStateChanged,
              (const PageNode*, PageNode::LoadingState),
              (override));
  MOCK_METHOD(void, OnUkmSourceIdChanged, (const PageNode*), (override));
  MOCK_METHOD(void, OnPageLifecycleStateChanged, (const PageNode*), (override));
  MOCK_METHOD(void,
              OnPageIsHoldingWebLockChanged,
              (const PageNode*),
              (override));
  MOCK_METHOD(void,
              OnPageIsHoldingBlockingIndexedDBLockChanged,
              (const PageNode*),
              (override));
  MOCK_METHOD(void, OnPageUsesWebRTCChanged, (const PageNode*), (override));
  MOCK_METHOD(void,
              OnPageNotificationPermissionStatusChange,
              (const PageNode*, std::optional<blink::mojom::PermissionStatus>),
              (override));
  MOCK_METHOD(void, OnMainFrameUrlChanged, (const PageNode*), (override));
  MOCK_METHOD(void, OnMainFrameDocumentChanged, (const PageNode*), (override));
  MOCK_METHOD(void, OnTitleUpdated, (const PageNode*), (override));
  MOCK_METHOD(void, OnFaviconUpdated, (const PageNode*), (override));
  MOCK_METHOD(void, OnHadFormInteractionChanged, (const PageNode*), (override));
  MOCK_METHOD(void, OnHadUserEditsChanged, (const PageNode*), (override));
  MOCK_METHOD(void,
              OnAboutToBeDiscarded,
              (const PageNode*, const PageNode*),
              (override));
};

using MockPageNodeObserver = ::testing::StrictMock<LenientMockPageNodeObserver>;

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_GRAPH_MOCK_PAGE_NODE_OBSERVER_H_
