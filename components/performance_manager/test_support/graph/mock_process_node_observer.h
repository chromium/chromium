// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_GRAPH_MOCK_PROCESS_NODE_OBSERVER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_GRAPH_MOCK_PROCESS_NODE_OBSERVER_H_

#include "components/performance_manager/public/graph/process_node.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace performance_manager {

class LenientMockProcessNodeObserver : public ProcessNodeObserver {
 public:
  LenientMockProcessNodeObserver();
  ~LenientMockProcessNodeObserver() override;

  MOCK_METHOD(void, OnBeforeProcessNodeAdded, (const ProcessNode*), (override));
  MOCK_METHOD(void, OnProcessNodeAdded, (const ProcessNode*), (override));
  MOCK_METHOD(void, OnProcessLifetimeChange, (const ProcessNode*), (override));
  MOCK_METHOD(void,
              OnBeforeProcessNodeRemoved,
              (const ProcessNode*),
              (override));
  MOCK_METHOD(void, OnProcessNodeRemoved, (const ProcessNode*), (override));
  MOCK_METHOD(void,
              OnMainThreadTaskLoadIsLow,
              (const ProcessNode*),
              (override));
  MOCK_METHOD(void,
              OnPriorityChanged,
              (const ProcessNode*, base::TaskPriority),
              (override));
  MOCK_METHOD(void,
              OnAllFramesInProcessFrozen,
              (const ProcessNode*),
              (override));
};

using MockProcessNodeObserver =
    ::testing::StrictMock<LenientMockProcessNodeObserver>;

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_GRAPH_MOCK_PROCESS_NODE_OBSERVER_H_
