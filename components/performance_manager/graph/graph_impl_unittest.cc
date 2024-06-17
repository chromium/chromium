// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/graph_impl.h"

#include <string_view>

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/system_node_impl.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

using GraphImplTest = GraphTestHarness;

using ::testing::ElementsAreArray;

TEST_F(GraphImplTest, SafeCasting) {
  const Graph* graph_base = graph();
  EXPECT_EQ(graph(), GraphImpl::FromGraph(graph_base));
}

TEST_F(GraphImplTest, GetSystemNodeImpl) {
  // The SystemNode singleton should be created by default.
  EXPECT_NE(nullptr, graph()->GetSystemNodeImpl());
}

TEST_F(GraphImplTest, GetProcessNodeByPid) {
  TestNodeWrapper<ProcessNodeImpl> process = CreateNode<ProcessNodeImpl>();
  EXPECT_EQ(base::kNullProcessId, process->GetProcessId());
  EXPECT_FALSE(process->GetProcess().IsValid());

  const base::Process self = base::Process::Current();

  EXPECT_EQ(nullptr, graph()->GetProcessNodeByPid(self.Pid()));
  process->SetProcess(self.Duplicate(),
                      /* launch_time=*/base::TimeTicks::Now());
  EXPECT_TRUE(process->GetProcess().IsValid());
  EXPECT_EQ(self.Pid(), process->GetProcessId());
  EXPECT_EQ(process.get(), graph()->GetProcessNodeByPid(self.Pid()));

  // Validate that an exited process isn't removed (yet).
  process->SetProcessExitStatus(0xCAFE);
  EXPECT_FALSE(process->GetProcess().IsValid());
  EXPECT_EQ(self.Pid(), process->GetProcessId());
  EXPECT_EQ(process.get(), graph()->GetProcessNodeByPid(self.Pid()));

  process.reset();

  EXPECT_EQ(nullptr, graph()->GetProcessNodeByPid(self.Pid()));
}

TEST_F(GraphImplTest, PIDReuse) {
  // This test emulates what happens on Windows under aggressive PID reuse,
  // where a process termination notification can be delayed until after the
  // PID has been reused for a new process.
  static base::Process self = base::Process::Current();

  TestNodeWrapper<ProcessNodeImpl> process1 = CreateNode<ProcessNodeImpl>();
  TestNodeWrapper<ProcessNodeImpl> process2 = CreateNode<ProcessNodeImpl>();

  process1->SetProcess(self.Duplicate(),
                       /* launch_time=*/base::TimeTicks::Now());
  EXPECT_EQ(process1.get(), graph()->GetProcessNodeByPid(self.Pid()));

  // First process exits, but hasn't been deleted yet.
  process1->SetProcessExitStatus(0xCAFE);
  EXPECT_EQ(process1.get(), graph()->GetProcessNodeByPid(self.Pid()));

  // The second registration for the same PID should override the first one.
  process2->SetProcess(self.Duplicate(),
                       /* launch_time=*/base::TimeTicks::Now());
  EXPECT_EQ(process2.get(), graph()->GetProcessNodeByPid(self.Pid()));

  // The destruction of the first process node shouldn't clear the PID
  // registration.
  process1.reset();
  EXPECT_EQ(process2.get(), graph()->GetProcessNodeByPid(self.Pid()));
}

TEST_F(GraphImplTest, GetAllCUsByType) {
  MockMultiplePagesInSingleProcessGraph mock_graph(graph());

  std::vector<ProcessNodeImpl*> processes =
      graph()->GetAllProcessNodeImpls().AsVector();

  // Graph contains a browser process and 1 renderer process.
  ASSERT_EQ(2u, processes.size());
  EXPECT_NE(nullptr, processes[0]);
  EXPECT_NE(nullptr, processes[1]);

  std::vector<FrameNodeImpl*> frames =
      graph()->GetAllFrameNodeImpls().AsVector();
  ASSERT_EQ(2u, frames.size());
  EXPECT_NE(nullptr, frames[0]);
  EXPECT_NE(nullptr, frames[1]);

  std::vector<PageNodeImpl*> pages = graph()->GetAllPageNodeImpls().AsVector();
  ASSERT_EQ(2u, pages.size());
  EXPECT_NE(nullptr, pages[0]);
  EXPECT_NE(nullptr, pages[1]);
}

TEST_F(GraphImplTest, GetAllNodes) {
  // This mock graphs contains 2 pages with 2 main frames in a single process.
  // There is a total of 2 process nodes because of the browser process node.
  MockMultiplePagesInSingleProcessGraph mock_graph(graph());

  // 1 renderer and 1 browser process.
  auto process_nodes = graph()->GetAllProcessNodes().AsVector();
  EXPECT_EQ(process_nodes.size(), 2u);
  EXPECT_TRUE(base::Contains(process_nodes, mock_graph.process.get()));

  // 2 pages.
  EXPECT_THAT(graph()->GetAllPageNodes().AsVector(),
              ::testing::UnorderedElementsAre(mock_graph.page.get(),
                                              mock_graph.other_page.get()));

  // 2 frames.
  EXPECT_THAT(graph()->GetAllFrameNodes().AsVector(),
              ::testing::UnorderedElementsAre(mock_graph.frame.get(),
                                              mock_graph.other_frame.get()));

  // No workers.
  EXPECT_THAT(graph()->GetAllWorkerNodes().AsVector(),
              ::testing::UnorderedElementsAre());
}

TEST_F(GraphImplTest, GetAllNodeImpls) {
  // This mock graphs contains 2 pages with 2 main frames in a single process.
  // There is a total of 2 process nodes because of the browser process node.
  MockMultiplePagesInSingleProcessGraph mock_graph(graph());

  // 1 renderer and 1 browser process.
  auto process_nodes = graph()->GetAllProcessNodeImpls().AsVector();
  EXPECT_EQ(process_nodes.size(), 2u);
  EXPECT_TRUE(base::Contains(process_nodes, mock_graph.process.get()));

  // 2 pages.
  EXPECT_THAT(graph()->GetAllPageNodeImpls().AsVector(),
              ::testing::UnorderedElementsAre(mock_graph.page.get(),
                                              mock_graph.other_page.get()));

  // 2 frames.
  EXPECT_THAT(graph()->GetAllFrameNodeImpls().AsVector(),
              ::testing::UnorderedElementsAre(mock_graph.frame.get(),
                                              mock_graph.other_frame.get()));

  // No workers.
  EXPECT_THAT(graph()->GetAllWorkerNodeImpls().AsVector(),
              ::testing::UnorderedElementsAre());
}

namespace {

class Foo : public GraphOwned {
 public:
  explicit Foo(int* destructor_count) : destructor_count_(destructor_count) {}

  ~Foo() override { (*destructor_count_)++; }

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override {
    EXPECT_EQ(GetOwningGraph(), graph);
    passed_to_called_ = true;
  }
  void OnTakenFromGraph(Graph* graph) override {
    EXPECT_EQ(GetOwningGraph(), graph);
    taken_from_called_ = true;
  }

  bool passed_to_called() const { return passed_to_called_; }
  bool taken_from_called() const { return taken_from_called_; }

 private:
  bool passed_to_called_ = false;
  bool taken_from_called_ = false;
  raw_ptr<int> destructor_count_ = nullptr;
};

}  // namespace

TEST_F(GraphImplTest, GraphOwned) {
  int destructor_count = 0;

  std::unique_ptr<Foo> foo1 = std::make_unique<Foo>(&destructor_count);
  std::unique_ptr<Foo> foo2 = std::make_unique<Foo>(&destructor_count);
  auto* raw1 = foo1.get();
  auto* raw2 = foo2.get();

  // Pass both objects to the graph.
  std::unique_ptr<GraphImpl> graph = std::make_unique<GraphImpl>();
  graph->SetUp();
  EXPECT_EQ(0u, graph->GraphOwnedCountForTesting());

  EXPECT_FALSE(raw1->passed_to_called());
  EXPECT_EQ(raw1->GetOwningGraph(), nullptr);
  graph->PassToGraph(std::move(foo1));
  EXPECT_TRUE(raw1->passed_to_called());
  EXPECT_EQ(raw1->GetOwningGraph(), graph.get());
  EXPECT_EQ(1u, graph->GraphOwnedCountForTesting());

  EXPECT_FALSE(raw2->passed_to_called());
  EXPECT_EQ(raw2->GetOwningGraph(), nullptr);
  graph->PassToGraph(std::move(foo2));
  EXPECT_TRUE(raw2->passed_to_called());
  EXPECT_EQ(raw2->GetOwningGraph(), graph.get());
  EXPECT_EQ(2u, graph->GraphOwnedCountForTesting());

  // Take one back.
  EXPECT_FALSE(raw1->taken_from_called());
  foo1 = graph->TakeFromGraphAs<Foo>(raw1);
  EXPECT_TRUE(raw1->taken_from_called());
  EXPECT_EQ(raw1->GetOwningGraph(), nullptr);
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

namespace {

class TestNodeDataDescriber : public NodeDataDescriber {
 public:
  explicit TestNodeDataDescriber(std::string_view name) : name_(name) {}

  base::Value::Dict DescribeFrameNodeData(
      const FrameNode* node) const override {
    base::Value::Dict dict;
    dict.Set("name", name_);
    dict.Set("type", "FrameNode");
    return dict;
  }

  base::Value::Dict DescribePageNodeData(const PageNode* node) const override {
    base::Value::Dict dict;
    dict.Set("name", name_);
    dict.Set("type", "PageNode");
    return dict;
  }

  base::Value::Dict DescribeProcessNodeData(
      const ProcessNode* node) const override {
    base::Value::Dict dict;
    dict.Set("name", name_);
    dict.Set("type", "ProcessNode");
    return dict;
  }

  base::Value::Dict DescribeSystemNodeData(
      const SystemNode* node) const override {
    base::Value::Dict dict;
    dict.Set("name", name_);
    dict.Set("type", "SystemNode");
    return dict;
  }

  base::Value::Dict DescribeWorkerNodeData(
      const WorkerNode* node) const override {
    base::Value::Dict dict;
    dict.Set("name", name_);
    dict.Set("type", "WorkerNode");
    return dict;
  }

 private:
  const std::string name_;
};

void AssertDictValueContainsListKey(const base::Value::Dict& descr,
                                    const char* key,
                                    const std::string s1,
                                    const std::string s2) {
  const base::Value::Dict* dict = descr.FindDict(key);
  ASSERT_NE(nullptr, dict);

  ASSERT_EQ(2u, dict->size());
  ASSERT_EQ(*(dict->FindString("name")), s1);
  ASSERT_EQ(*(dict->FindString("type")), s2);
}

}  // namespace

TEST_F(GraphImplTest, NodeDataDescribers) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  NodeDataDescriberRegistry* registry = graph()->GetNodeDataDescriberRegistry();

  // No describers->no description.
  base::Value::Dict descr = registry->DescribeNodeData(mock_graph.frame.get());
  EXPECT_EQ(0u, descr.size());

  // Test that the default impl does nothing.
  NodeDataDescriberDefaultImpl default_impl;
  registry->RegisterDescriber(&default_impl, "default_impl");

  // Test a single non-default describer for each node type.
  TestNodeDataDescriber d1("d1");
  registry->RegisterDescriber(&d1, "d1");

  descr = registry->DescribeNodeData(mock_graph.frame.get());
  AssertDictValueContainsListKey(descr, "d1", "d1", "FrameNode");
  EXPECT_EQ(1u, descr.size());

  descr = registry->DescribeNodeData(mock_graph.page.get());
  AssertDictValueContainsListKey(descr, "d1", "d1", "PageNode");
  EXPECT_EQ(1u, descr.size());

  descr = registry->DescribeNodeData(mock_graph.process.get());
  AssertDictValueContainsListKey(descr, "d1", "d1", "ProcessNode");
  EXPECT_EQ(1u, descr.size());

  descr = registry->DescribeNodeData(graph()->GetSystemNode());
  AssertDictValueContainsListKey(descr, "d1", "d1", "SystemNode");
  EXPECT_EQ(1u, descr.size());

  auto worker = CreateNode<WorkerNodeImpl>(WorkerNode::WorkerType::kDedicated,
                                           mock_graph.process.get());
  descr = registry->DescribeNodeData(worker.get());
  AssertDictValueContainsListKey(descr, "d1", "d1", "WorkerNode");
  EXPECT_EQ(1u, descr.size());

  // Unregister the default impl now that it's been verified to say nothing
  // about all node types.
  registry->UnregisterDescriber(&default_impl);

  // Register a second describer and test one node type.
  TestNodeDataDescriber d2("d2");
  registry->RegisterDescriber(&d2, "d2");

  descr = registry->DescribeNodeData(mock_graph.frame.get());
  EXPECT_EQ(2u, descr.size());
  AssertDictValueContainsListKey(descr, "d1", "d1", "FrameNode");
  AssertDictValueContainsListKey(descr, "d2", "d2", "FrameNode");

  registry->UnregisterDescriber(&d2);
  registry->UnregisterDescriber(&d1);

  // No describers after unregistration->no description.
  descr = registry->DescribeNodeData(mock_graph.frame.get());
  EXPECT_EQ(0u, descr.size());
}

TEST_F(GraphImplTest, OpenersAndEmbeddersClearedOnTeardown) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto pageA = CreateNode<PageNodeImpl>();
  auto frameA1 = CreateFrameNodeAutoId(process.get(), pageA.get());
  auto frameA2 =
      CreateFrameNodeAutoId(process.get(), pageA.get(), frameA1.get());
  auto pageB = CreateNode<PageNodeImpl>();
  auto frameB1 = CreateFrameNodeAutoId(process.get(), pageB.get());
  auto pageC = CreateNode<PageNodeImpl>();
  auto frameC1 = CreateFrameNodeAutoId(process.get(), pageC.get());

  // Set up some embedder relationships. These should be gracefully torn down as
  // the graph cleans up nodes, otherwise the frame and page node destructors
  // will explode.
  pageB->SetEmbedderFrameNodeAndEmbeddingType(
      frameA1.get(), PageNode::EmbeddingType::kGuestView);
  pageC->SetOpenerFrameNode(frameA2.get());
}

}  // namespace performance_manager
