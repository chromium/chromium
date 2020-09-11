// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context/execution_context_registry_impl.h"

#include <memory>

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
    graph()->PassToGraph(std::make_unique<ExecutionContextRegistryImpl>());
    registry_ = GraphRegisteredImpl<ExecutionContextRegistryImpl>::GetFromGraph(
        graph());
    ASSERT_TRUE(registry_);
  }

 protected:
  ExecutionContextRegistryImpl* registry_ = nullptr;
};

using ExecutionContextRegistryImplDeathTest = ExecutionContextRegistryImplTest;

}  // namespace

TEST_F(ExecutionContextRegistryImplTest, RegistryWorks) {
  // Ensure that the public getter works.
  EXPECT_EQ(registry_, ExecutionContextRegistry::GetFromGraph(graph()));

  // Create an observer.
  MockExecutionContextObserver obs;
  registry_->AddObserver(&obs);

  // Create some mock nodes. This creates a graph with 1 page containing 2
  // frames in 1 process.
  std::vector<const ExecutionContext*> ecs;
  EXPECT_CALL(obs, OnExecutionContextAdded(testing::_))
      .Times(2)
      .WillRepeatedly(
          [&ecs](const ExecutionContext* ec) { ecs.push_back(ec); });
  MockMultiplePagesInSingleProcessGraph mock_graph(graph());

  // Only the frames are in the map at this point.
  EXPECT_EQ(2u, ecs.size());
  EXPECT_EQ(2u, registry_->GetExecutionContextCountForTesting());

  // Creating a worker should create another entry in the map.
  EXPECT_CALL(obs, OnExecutionContextAdded(testing::_))
      .WillOnce([&ecs](const ExecutionContext* ec) { ecs.push_back(ec); });
  auto worker_node = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kDedicated, mock_graph.process.get());
  EXPECT_EQ(3u, ecs.size());
  EXPECT_EQ(3u, registry_->GetExecutionContextCountForTesting());

  auto* frame1 = mock_graph.frame.get();
  auto* frame2 = mock_graph.other_frame.get();
  auto* worker = worker_node.get();

  // Get the execution contexts for each node directly.
  auto* frame1_ec = GetOrCreateExecutionContextForFrameNode(frame1);
  auto* frame2_ec = GetOrCreateExecutionContextForFrameNode(frame2);
  auto* worker_ec = GetOrCreateExecutionContextForWorkerNode(worker);

  // Expect them to match those that were seen by the observer.
  EXPECT_EQ(ecs[0], frame1_ec);
  EXPECT_EQ(ecs[1], frame2_ec);
  EXPECT_EQ(ecs[2], worker_ec);

  // Expect the FrameExecutionContext implementation to work.
  EXPECT_EQ(ExecutionContextType::kFrameNode, frame1_ec->GetType());
  EXPECT_EQ(frame1->frame_token().value(), frame1_ec->GetToken().value());
  EXPECT_EQ(frame1->url(), frame1_ec->GetUrl());
  EXPECT_EQ(frame1->process_node(), frame1_ec->GetProcessNode());
  EXPECT_EQ(frame1, frame1_ec->GetFrameNode());
  EXPECT_FALSE(frame1_ec->GetWorkerNode());

  // Expect the WorkerExecutionContext implementation to work.
  EXPECT_EQ(ExecutionContextType::kWorkerNode, worker_ec->GetType());
  EXPECT_EQ(worker->worker_token().value(), worker_ec->GetToken().value());
  EXPECT_EQ(worker->url(), worker_ec->GetUrl());
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
  EXPECT_EQ(frame1, registry_->GetFrameNodeByFrameToken(frame1->frame_token()));
  EXPECT_EQ(frame2, registry_->GetFrameNodeByFrameToken(frame2->frame_token()));
  EXPECT_EQ(worker,
            registry_->GetWorkerNodeByWorkerToken(worker->worker_token()));

  // Querying an invalid token or a random token should fail.
  EXPECT_FALSE(
      registry_->GetExecutionContextByToken(blink::ExecutionContextToken(
          blink::LocalFrameToken(base::UnguessableToken::Null()))));
  EXPECT_FALSE(
      registry_->GetExecutionContextByToken(blink::ExecutionContextToken()));
  EXPECT_FALSE(registry_->GetFrameNodeByFrameToken(blink::LocalFrameToken()));
  EXPECT_FALSE(registry_->GetWorkerNodeByWorkerToken(blink::WorkerToken()));

  // Destroy nodes one by one and expect observer notifications.
  EXPECT_CALL(obs, OnBeforeExecutionContextRemoved(worker_ec));
  worker_node.reset();
  EXPECT_EQ(2u, registry_->GetExecutionContextCountForTesting());
  EXPECT_CALL(obs, OnBeforeExecutionContextRemoved(frame2_ec));
  mock_graph.other_frame.reset();
  EXPECT_EQ(1u, registry_->GetExecutionContextCountForTesting());
  EXPECT_CALL(obs, OnBeforeExecutionContextRemoved(frame1_ec));
  mock_graph.frame.reset();
  EXPECT_EQ(0u, registry_->GetExecutionContextCountForTesting());

  // Unregister the observer so that the registry doesn't explode when it is
  // torn down.
  registry_->RemoveObserver(&obs);
}

TEST_F(ExecutionContextRegistryImplDeathTest, EnforceObserversRemoved) {
  // Create an observer.
  MockExecutionContextObserver obs;
  registry_->AddObserver(&obs);

  // The registry should explode if we kill it without unregistering observers.
  EXPECT_DCHECK_DEATH(graph()->TakeFromGraph(registry_));

  // Unregister the observer so that the registry doesn't explode when it is
  // torn down.
  registry_->RemoveObserver(&obs);
}

}  // namespace execution_context
}  // namespace performance_manager
