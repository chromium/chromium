// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_GRAPH_MOCK_WORKER_NODE_OBSERVER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_GRAPH_MOCK_WORKER_NODE_OBSERVER_H_

#include "components/performance_manager/public/graph/worker_node.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace performance_manager {

class LenientMockWorkerNodeObserver : public WorkerNodeObserver {
 public:
  LenientMockWorkerNodeObserver();
  ~LenientMockWorkerNodeObserver() override;

  MOCK_METHOD(void,
              OnBeforeWorkerNodeAdded,
              (const WorkerNode*, const ProcessNode*),
              (override));
  MOCK_METHOD(void, OnWorkerNodeAdded, (const WorkerNode*), (override));
  MOCK_METHOD(void, OnBeforeWorkerNodeRemoved, (const WorkerNode*), (override));
  MOCK_METHOD(void,
              OnWorkerNodeRemoved,
              (const WorkerNode*, const ProcessNode*),
              (override));
  MOCK_METHOD(void,
              OnFinalResponseURLDetermined,
              (const WorkerNode*),
              (override));
  MOCK_METHOD(void,
              OnBeforeClientFrameAdded,
              (const WorkerNode*, const FrameNode*),
              (override));
  MOCK_METHOD(void,
              OnClientFrameAdded,
              (const WorkerNode*, const FrameNode*),
              (override));
  MOCK_METHOD(void,
              OnBeforeClientFrameRemoved,
              (const WorkerNode*, const FrameNode*),
              (override));
  MOCK_METHOD(void,
              OnBeforeClientWorkerAdded,
              (const WorkerNode*, const WorkerNode*),
              (override));
  MOCK_METHOD(void,
              OnClientWorkerAdded,
              (const WorkerNode*, const WorkerNode*),
              (override));
  MOCK_METHOD(void,
              OnBeforeClientWorkerRemoved,
              (const WorkerNode*, const WorkerNode*),
              (override));
  MOCK_METHOD(void,
              OnPriorityAndReasonChanged,
              (const WorkerNode*, const PriorityAndReason&),
              (override));
};

using MockWorkerNodeObserver =
    ::testing::StrictMock<LenientMockWorkerNodeObserver>;

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_GRAPH_MOCK_WORKER_NODE_OBSERVER_H_
