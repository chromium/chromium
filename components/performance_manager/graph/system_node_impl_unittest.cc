// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/system_node_impl.h"

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/system_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

// Observer used to make sure that signals are dispatched correctly.
class SystemObserver : public SystemNodeImpl::ObserverDefaultImpl {
 public:
  size_t system_event_seen_count() const { return system_event_seen_count_; }

 private:
  size_t system_event_seen_count_ = 0;
};

using SystemNodeImplTest = GraphTestHarness;

}  // namespace

TEST_F(SystemNodeImplTest, SafeDowncast) {
  MockMultiplePagesWithMultipleProcessesGraph mock_graph(graph());
  auto& sys = mock_graph.system;
  SystemNode* node = sys.get();
  EXPECT_EQ(sys.get(), SystemNodeImpl::FromNode(node));
  NodeBase* base = sys.get();
  EXPECT_EQ(base, NodeBase::FromNode(node));
  EXPECT_EQ(static_cast<Node*>(node), base->ToNode());
}

using SystemNodeImplDeathTest = SystemNodeImplTest;

TEST_F(SystemNodeImplDeathTest, SafeDowncast) {
  const NodeBase* system =
      NodeBase::FromNode(graph()->FindOrCreateSystemNode());
  ASSERT_DEATH_IF_SUPPORTED(PageNodeImpl::FromNodeBase(system), "");
}

namespace {

class LenientMockObserver : public SystemNodeImpl::Observer {
 public:
  LenientMockObserver() {}
  ~LenientMockObserver() override {}

  MOCK_METHOD1(OnSystemNodeAdded, void(const SystemNode*));
  MOCK_METHOD1(OnBeforeSystemNodeRemoved, void(const SystemNode*));
  MOCK_METHOD1(OnProcessMemoryMetricsAvailable, void(const SystemNode*));

  void SetNotifiedSystemNode(const SystemNode* system_node) {
    notified_system_node_ = system_node;
  }

  const SystemNode* TakeNotifiedSystemNode() {
    const SystemNode* node = notified_system_node_;
    notified_system_node_ = nullptr;
    return node;
  }

 private:
  const SystemNode* notified_system_node_ = nullptr;
};

using MockObserver = ::testing::StrictMock<LenientMockObserver>;

using testing::_;
using testing::Invoke;

}  // namespace

TEST_F(SystemNodeImplTest, ObserverWorks) {
  MockObserver obs;
  graph()->AddSystemNodeObserver(&obs);

  // Fetch the system node and expect a matching call to "OnSystemNodeAdded".
  EXPECT_CALL(obs, OnSystemNodeAdded(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedSystemNode));
  const SystemNode* system_node = graph()->FindOrCreateSystemNode();
  EXPECT_EQ(system_node, obs.TakeNotifiedSystemNode());

  EXPECT_CALL(obs, OnProcessMemoryMetricsAvailable(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedSystemNode));
  SystemNodeImpl::FromNode(system_node)->OnProcessMemoryMetricsAvailable();
  EXPECT_EQ(system_node, obs.TakeNotifiedSystemNode());

  // Release the system node and expect a call to "OnBeforeSystemNodeRemoved".
  EXPECT_CALL(obs, OnBeforeSystemNodeRemoved(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedSystemNode));
  graph()->ReleaseSystemNodeForTesting();
  EXPECT_EQ(system_node, obs.TakeNotifiedSystemNode());

  graph()->RemoveSystemNodeObserver(&obs);
}

}  // namespace performance_manager
