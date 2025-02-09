// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/system_node_impl.h"

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "components/memory_pressure/fake_memory_pressure_monitor.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/test_support/graph/mock_system_node_observer.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

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
  const NodeBase* system = NodeBase::FromNode(graph()->GetSystemNodeImpl());
  ASSERT_DEATH_IF_SUPPORTED(PageNodeImpl::FromNodeBase(system), "");
}

namespace {

using MemoryPressureLevel = base::MemoryPressureListener::MemoryPressureLevel;
using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;

class MockObserver : public MockSystemNodeObserver {
 public:
  explicit MockObserver(Graph* graph = nullptr) {
    // If a `graph` is passed, automatically start observing it.
    if (graph) {
      scoped_observation_.Observe(graph);
    }
  }

  void SetNotifiedSystemNode(const SystemNode* system_node) {
    notified_system_node_ = system_node;
  }

  const SystemNode* TakeNotifiedSystemNode() {
    const SystemNode* node = notified_system_node_;
    notified_system_node_ = nullptr;
    return node;
  }

 private:
  base::ScopedObservation<Graph, SystemNodeObserver> scoped_observation_{this};
  raw_ptr<const SystemNode> notified_system_node_ = nullptr;
};

}  // namespace

TEST_F(SystemNodeImplTest, ObserverWorks) {
  MockObserver head_obs;
  MockObserver obs;
  MockObserver tail_obs;
  graph()->AddSystemNodeObserver(&head_obs);
  graph()->AddSystemNodeObserver(&obs);
  graph()->AddSystemNodeObserver(&tail_obs);

  const SystemNode* system_node = graph()->GetSystemNode();

  // Remove observers at the head and tail of the list inside a callback, and
  // expect that `obs` is still notified correctly.
  EXPECT_CALL(head_obs, OnProcessMemoryMetricsAvailable(_))
      .WillOnce(InvokeWithoutArgs([&] {
        graph()->RemoveSystemNodeObserver(&head_obs);
        graph()->RemoveSystemNodeObserver(&tail_obs);
      }));
  // `tail_obs` should not be notified as it was removed.
  EXPECT_CALL(tail_obs, OnProcessMemoryMetricsAvailable(_)).Times(0);

  EXPECT_CALL(obs, OnProcessMemoryMetricsAvailable(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedSystemNode));
  SystemNodeImpl::FromNode(system_node)->OnProcessMemoryMetricsAvailable();
  EXPECT_EQ(system_node, obs.TakeNotifiedSystemNode());

  EXPECT_CALL(obs, OnBeforeMemoryPressure(
                       MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_CRITICAL));
  EXPECT_CALL(obs, OnMemoryPressure(
                       MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_CRITICAL));
  SystemNodeImpl::FromNode(system_node)
      ->OnMemoryPressureForTesting(
          MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_CRITICAL);

  // Re-entrant iteration should work.
  EXPECT_CALL(obs, OnProcessMemoryMetricsAvailable(system_node))
      .WillOnce(InvokeWithoutArgs([&] {
        SystemNodeImpl::FromNode(system_node)
            ->OnMemoryPressureForTesting(
                MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_MODERATE);
      }));
  EXPECT_CALL(obs, OnBeforeMemoryPressure(
                       MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_MODERATE));
  EXPECT_CALL(obs, OnMemoryPressure(
                       MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_MODERATE));
  SystemNodeImpl::FromNode(system_node)->OnProcessMemoryMetricsAvailable();

  graph()->RemoveSystemNodeObserver(&obs);
}

TEST_F(SystemNodeImplTest, MemoryPressureNotification) {
  MockObserver obs(graph());
  memory_pressure::test::FakeMemoryPressureMonitor mem_pressure_monitor;

  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(obs, OnBeforeMemoryPressure(
                         base::MemoryPressureListener::MemoryPressureLevel::
                             MEMORY_PRESSURE_LEVEL_CRITICAL))
        .WillOnce(InvokeWithoutArgs([&]() { std::move(quit_closure).Run(); }));
    EXPECT_CALL(obs, OnMemoryPressure(
                         base::MemoryPressureListener::MemoryPressureLevel::
                             MEMORY_PRESSURE_LEVEL_CRITICAL));
    mem_pressure_monitor.SetAndNotifyMemoryPressure(
        base::MemoryPressureListener::MemoryPressureLevel::
            MEMORY_PRESSURE_LEVEL_CRITICAL);
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(obs, OnBeforeMemoryPressure(
                         base::MemoryPressureListener::MemoryPressureLevel::
                             MEMORY_PRESSURE_LEVEL_MODERATE))
        .WillOnce(InvokeWithoutArgs([&]() { std::move(quit_closure).Run(); }));
    EXPECT_CALL(obs, OnMemoryPressure(
                         base::MemoryPressureListener::MemoryPressureLevel::
                             MEMORY_PRESSURE_LEVEL_MODERATE));
    mem_pressure_monitor.SetAndNotifyMemoryPressure(
        base::MemoryPressureListener::MemoryPressureLevel::
            MEMORY_PRESSURE_LEVEL_MODERATE);
    run_loop.Run();
  }
}

}  // namespace performance_manager
