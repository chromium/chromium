// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context/execution_context_registry_impl.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/gtest_util.h"
#include "components/performance_manager/execution_context/execution_context_impl.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace performance_manager {
namespace execution_context {

namespace {

class LenientMockExecutionContextObserver : public ExecutionContextObserver {
 public:
  LenientMockExecutionContextObserver() = default;
  LenientMockExecutionContextObserver(
      const LenientMockExecutionContextObserver&) = delete;
  LenientMockExecutionContextObserver& operator=(
      const LenientMockExecutionContextObserver&) = delete;
  ~LenientMockExecutionContextObserver() override = default;

  // ExecutionContextObserver implementation:
  MOCK_METHOD(void, OnExecutionContextAdded, (const ExecutionContext*), ());
  MOCK_METHOD(void,
              OnBeforeExecutionContextRemoved,
              (const ExecutionContext*),
              ());
  MOCK_METHOD(void,
              OnPriorityAndReasonChanged,
              (const ExecutionContext*,
               const PriorityAndReason& previous_value),
              ());
};
using MockExecutionContextObserver =
    testing::StrictMock<LenientMockExecutionContextObserver>;

class ExecutionContextRegistryImplTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  ExecutionContextRegistryImplTest() = default;
  ExecutionContextRegistryImplTest(const ExecutionContextRegistryImplTest&) =
      delete;
  ExecutionContextRegistryImplTest& operator=(
      const ExecutionContextRegistryImplTest&) = delete;
  ~ExecutionContextRegistryImplTest() override = default;

  void SetUp() override {
    Super::SetUp();
    registry_ = GraphRegisteredImpl<ExecutionContextRegistryImpl>::GetFromGraph(
        graph());
    ASSERT_TRUE(registry_);
  }

 protected:
  raw_ptr<ExecutionContextRegistryImpl> registry_ = nullptr;
};

using ExecutionContextRegistryImplDeathTest = ExecutionContextRegistryImplTest;

}  // namespace

TEST_F(ExecutionContextRegistryImplTest, RegistryWorks) {
  // Ensure that the public getter works.
  EXPECT_EQ(registry_, ExecutionContextRegistry::GetFromGraph(graph()));

  // Create some mock nodes. This creates a graph with 1 page containing 2
  // frames in 1 process.
  MockMultiplePagesInSingleProcessGraph mock_graph(graph());

  // Only the frames are in the map at this point.
  EXPECT_EQ(2u, registry_->GetExecutionContextCountForTesting());

  // Creating a worker should create another entry in the map.
  auto worker_node = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kDedicated, mock_graph.process.get());
  EXPECT_EQ(3u, registry_->GetExecutionContextCountForTesting());

  auto* frame1 = mock_graph.frame.get();
  auto* frame2 = mock_graph.other_frame.get();
  auto* worker = worker_node.get();

  // Get the execution contexts for each node directly.
  auto* frame1_ec = &FrameExecutionContext::Get(frame1);
  auto* frame2_ec = &FrameExecutionContext::Get(frame2);
  auto* worker_ec = &WorkerExecutionContext::Get(worker);

  // Expect the FrameExecutionContext implementation to work.
  EXPECT_EQ(ExecutionContextType::kFrameNode, frame1_ec->GetType());
  EXPECT_EQ(frame1->GetFrameToken().value(), frame1_ec->GetToken().value());
  EXPECT_EQ(frame1->GetURL(), frame1_ec->GetUrl());
  EXPECT_EQ(frame1->process_node(), frame1_ec->GetProcessNode());
  EXPECT_EQ(frame1, frame1_ec->GetFrameNode());
  EXPECT_FALSE(frame1_ec->GetWorkerNode());

  // Expect the WorkerExecutionContext implementation to work.
  EXPECT_EQ(ExecutionContextType::kWorkerNode, worker_ec->GetType());
  EXPECT_EQ(worker->GetWorkerToken().value(), worker_ec->GetToken().value());
  EXPECT_EQ(worker->GetURL(), worker_ec->GetUrl());
  EXPECT_EQ(worker->process_node(), worker_ec->GetProcessNode());
  EXPECT_FALSE(worker_ec->GetFrameNode());
  EXPECT_EQ(worker, worker_ec->GetWorkerNode());

  // Getting ExecutionContexts for a node should work.
  EXPECT_EQ(frame1_ec, registry_->GetExecutionContextForFrameNode(frame1));
  EXPECT_EQ(frame2_ec, registry_->GetExecutionContextForFrameNode(frame2));
  EXPECT_EQ(worker_ec, registry_->GetExecutionContextForWorkerNode(worker));

  // Lookup by ExecutionContextToken should work.
  EXPECT_EQ(frame1_ec,
            registry_->GetExecutionContextByToken(frame1_ec->GetToken()));
  EXPECT_EQ(frame2_ec,
            registry_->GetExecutionContextByToken(frame2_ec->GetToken()));
  EXPECT_EQ(worker_ec,
            registry_->GetExecutionContextByToken(worker_ec->GetToken()));

  // Lookup by typed tokens should work.
  EXPECT_EQ(frame1,
            registry_->GetFrameNodeByFrameToken(frame1->GetFrameToken()));
  EXPECT_EQ(frame2,
            registry_->GetFrameNodeByFrameToken(frame2->GetFrameToken()));
  EXPECT_EQ(worker,
            registry_->GetWorkerNodeByWorkerToken(worker->GetWorkerToken()));

  // Querying a random token should fail.
  EXPECT_FALSE(
      registry_->GetExecutionContextByToken(blink::ExecutionContextToken()));
  EXPECT_FALSE(registry_->GetFrameNodeByFrameToken(blink::LocalFrameToken()));
  EXPECT_FALSE(registry_->GetWorkerNodeByWorkerToken(blink::WorkerToken()));
}

TEST_F(ExecutionContextRegistryImplTest, Observers) {
  // Create an observer.
  MockExecutionContextObserver obs;
  EXPECT_FALSE(registry_->HasObserver(&obs));
  registry_->AddObserver(&obs);
  EXPECT_TRUE(registry_->HasObserver(&obs));

  // Create some mock nodes. This creates a graph with 1 page containing 1 frame
  // and 1 worker in a single process.
  EXPECT_CALL(obs, OnExecutionContextAdded(testing::_)).Times(2);
  MockSinglePageWithFrameAndWorkerInSingleProcessGraph mock_graph(graph());

  // The registry has 2 entries: the frame and the worker.
  EXPECT_EQ(2u, registry_->GetExecutionContextCountForTesting());

  auto* frame = mock_graph.frame.get();
  auto* worker = mock_graph.worker.get();

  // Get the execution contexts for each node directly.
  auto* frame_ec = &FrameExecutionContext::Get(frame);
  auto* worker_ec = &WorkerExecutionContext::Get(worker);

  // Set the priority and reason of the frame and expect a notification.
  EXPECT_CALL(obs, OnPriorityAndReasonChanged(frame_ec, testing::_));
  frame->SetPriorityAndReason(
      PriorityAndReason(base::TaskPriority::HIGHEST, "frame reason"));

  // Set the priority and reason of the worker and expect a notification.
  EXPECT_CALL(obs, OnPriorityAndReasonChanged(worker_ec, testing::_));
  worker->SetPriorityAndReason(
      PriorityAndReason(base::TaskPriority::HIGHEST, "worker reason"));

  // Destroy nodes one by one and expect observer notifications.
  EXPECT_CALL(obs, OnBeforeExecutionContextRemoved(worker_ec));
  mock_graph.DeleteWorker();
  EXPECT_EQ(1u, registry_->GetExecutionContextCountForTesting());
  EXPECT_CALL(obs, OnBeforeExecutionContextRemoved(frame_ec));
  mock_graph.frame.reset();
  EXPECT_EQ(0u, registry_->GetExecutionContextCountForTesting());

  // Unregister the observer so that the registry doesn't explode when it is
  // torn down.
  registry_->RemoveObserver(&obs);
}

}  // namespace execution_context
}  // namespace performance_manager
