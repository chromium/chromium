// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/graph_impl.h"

#include "base/memory/ptr_util.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/system_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

using GraphImplTest = GraphTestHarness;

TEST_F(GraphImplTest, SafeCasting) {
  const Graph* graph_base = graph();
  EXPECT_EQ(graph(), GraphImpl::FromGraph(graph_base));
}

TEST_F(GraphImplTest, FindOrCreateSystemNode) {
  EXPECT_TRUE(graph()->IsEmpty());
  SystemNodeImpl* system_node = graph()->FindOrCreateSystemNodeImpl();
  EXPECT_FALSE(graph()->IsEmpty());

  // A second request should return the same instance.
  EXPECT_EQ(system_node, graph()->FindOrCreateSystemNodeImpl());
}

TEST_F(GraphImplTest, GetProcessNodeByPid) {
  TestNodeWrapper<ProcessNodeImpl> process =
      TestNodeWrapper<ProcessNodeImpl>::Create(graph());
  EXPECT_EQ(base::kNullProcessId, process->process_id());
  EXPECT_FALSE(process->process().IsValid());

  const base::Process self = base::Process::Current();

  EXPECT_EQ(nullptr, graph()->GetProcessNodeByPid(self.Pid()));
  process->SetProcess(self.Duplicate(), base::Time::Now());
  EXPECT_TRUE(process->process().IsValid());
  EXPECT_EQ(self.Pid(), process->process_id());
  EXPECT_EQ(process.get(), graph()->GetProcessNodeByPid(self.Pid()));

  // Validate that an exited process isn't removed (yet).
  process->SetProcessExitStatus(0xCAFE);
  EXPECT_FALSE(process->process().IsValid());
  EXPECT_EQ(self.Pid(), process->process_id());
  EXPECT_EQ(process.get(), graph()->GetProcessNodeByPid(self.Pid()));

  process.reset();

  EXPECT_EQ(nullptr, graph()->GetProcessNodeByPid(self.Pid()));
}

TEST_F(GraphImplTest, PIDReuse) {
  // This test emulates what happens on Windows under aggressive PID reuse,
  // where a process termination notification can be delayed until after the
  // PID has been reused for a new process.
  static base::Process self = base::Process::Current();

  TestNodeWrapper<ProcessNodeImpl> process1 =
      TestNodeWrapper<ProcessNodeImpl>::Create(graph());
  TestNodeWrapper<ProcessNodeImpl> process2 =
      TestNodeWrapper<ProcessNodeImpl>::Create(graph());

  process1->SetProcess(self.Duplicate(), base::Time::Now());
  EXPECT_EQ(process1.get(), graph()->GetProcessNodeByPid(self.Pid()));

  // First process exits, but hasn't been deleted yet.
  process1->SetProcessExitStatus(0xCAFE);
  EXPECT_EQ(process1.get(), graph()->GetProcessNodeByPid(self.Pid()));

  // The second registration for the same PID should override the first one.
  process2->SetProcess(self.Duplicate(), base::Time::Now());
  EXPECT_EQ(process2.get(), graph()->GetProcessNodeByPid(self.Pid()));

  // The destruction of the first process node shouldn't clear the PID
  // registration.
  process1.reset();
  EXPECT_EQ(process2.get(), graph()->GetProcessNodeByPid(self.Pid()));
}

TEST_F(GraphImplTest, GetAllCUsByType) {
  MockMultiplePagesInSingleProcessGraph mock_graph(graph());

  std::vector<ProcessNodeImpl*> processes = graph()->GetAllProcessNodeImpls();
  ASSERT_EQ(1u, processes.size());
  EXPECT_NE(nullptr, processes[0]);

  std::vector<FrameNodeImpl*> frames = graph()->GetAllFrameNodeImpls();
  ASSERT_EQ(2u, frames.size());
  EXPECT_NE(nullptr, frames[0]);
  EXPECT_NE(nullptr, frames[1]);

  std::vector<PageNodeImpl*> pages = graph()->GetAllPageNodeImpls();
  ASSERT_EQ(2u, pages.size());
  EXPECT_NE(nullptr, pages[0]);
  EXPECT_NE(nullptr, pages[1]);
}

TEST_F(GraphImplTest, SerializationId) {
  EXPECT_EQ(0u, NodeBase::GetSerializationId(nullptr));

  TestNodeWrapper<ProcessNodeImpl> process =
      TestNodeWrapper<ProcessNodeImpl>::Create(graph());

  // The serialization ID should be non-zero, and should be stable for a given
  // node.
  auto id = NodeBase::GetSerializationId(process.get());
  EXPECT_NE(0u, id);
  EXPECT_EQ(id, NodeBase::GetSerializationId(process.get()));

  SystemNodeImpl* system = graph()->FindOrCreateSystemNodeImpl();

  // Different nodes should be assigned different IDs.
  EXPECT_NE(id, NodeBase::GetSerializationId(system));
  EXPECT_NE(0, NodeBase::GetSerializationId(system));
  EXPECT_EQ(NodeBase::GetSerializationId(system),
            NodeBase::GetSerializationId(system));
}

namespace {

class LenientMockObserver : public GraphObserver {
 public:
  LenientMockObserver() {}
  ~LenientMockObserver() override {}

  MOCK_METHOD1(OnBeforeGraphDestroyed, void(Graph*));
};

using MockObserver = ::testing::StrictMock<LenientMockObserver>;

using testing::_;
using testing::Invoke;

}  // namespace

TEST_F(GraphImplTest, ObserverWorks) {
  std::unique_ptr<GraphImpl> graph = base::WrapUnique(new GraphImpl());
  Graph* raw_graph = graph.get();

  MockObserver obs;
  graph->AddGraphObserver(&obs);
  graph->RemoveGraphObserver(&obs);
  graph->AddGraphObserver(&obs);

  // Expect the graph teardown callback to be invoked. We have to unregister our
  // observer in order to maintain graph invariants.
  EXPECT_CALL(obs, OnBeforeGraphDestroyed(raw_graph))
      .WillOnce(testing::Invoke(
          [&obs](Graph* graph) { graph->RemoveGraphObserver(&obs); }));
  graph->TearDown();
  graph.reset();
}

namespace {

class Foo : public GraphOwned {
 public:
  explicit Foo(int* destructor_count) : destructor_count_(destructor_count) {}

  ~Foo() override { (*destructor_count_)++; }

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override { passed_to_called_ = true; }
  void OnTakenFromGraph(Graph* graph) override { taken_from_called_ = true; }

  bool passed_to_called() const { return passed_to_called_; }
  bool taken_from_called() const { return taken_from_called_; }

 private:
  bool passed_to_called_ = false;
  bool taken_from_called_ = false;
  int* destructor_count_ = nullptr;
};

}  // namespace

TEST_F(GraphImplTest, GraphOwned) {
  int destructor_count = 0;

  std::unique_ptr<Foo> foo1 = base::WrapUnique(new Foo(&destructor_count));
  std::unique_ptr<Foo> foo2 = base::WrapUnique(new Foo(&destructor_count));
  auto* raw1 = foo1.get();
  auto* raw2 = foo2.get();

  // Pass both objects to the graph.
  std::unique_ptr<GraphImpl> graph = base::WrapUnique(new GraphImpl());
  EXPECT_EQ(0u, graph->GraphOwnedCountForTesting());
  EXPECT_FALSE(raw1->passed_to_called());
  graph->PassToGraph(std::move(foo1));
  EXPECT_TRUE(raw1->passed_to_called());
  EXPECT_EQ(1u, graph->GraphOwnedCountForTesting());
  EXPECT_FALSE(raw2->passed_to_called());
  graph->PassToGraph(std::move(foo2));
  EXPECT_TRUE(raw2->passed_to_called());
  EXPECT_EQ(2u, graph->GraphOwnedCountForTesting());

  // Take one back.
  EXPECT_FALSE(raw1->taken_from_called());
  foo1 = graph->TakeFromGraphAs<Foo>(raw1);
  EXPECT_TRUE(raw1->taken_from_called());
  EXPECT_EQ(1u, graph->GraphOwnedCountForTesting());

  // Destroy that object and expect its destructor to have been invoked.
  EXPECT_EQ(0, destructor_count);
  foo1.reset();
  EXPECT_EQ(1, destructor_count);

  // Now destroy the graph and expect the other object to have been torn down
  // too.
  graph->TearDown();
  graph.reset();
  EXPECT_EQ(2, destructor_count);
}

}  // namespace performance_manager
