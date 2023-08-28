// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_GRAPH_TEST_HARNESS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_GRAPH_TEST_HARNESS_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "components/performance_manager/embedder/graph_features.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/node_base.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/system_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace performance_manager {

template <class NodeClass>
class TestNodeWrapper {
 public:
  struct Factory;

  template <typename... Args>
  static TestNodeWrapper<NodeClass> Create(GraphImpl* graph, Args&&... args);

  TestNodeWrapper() = default;

  explicit TestNodeWrapper(std::unique_ptr<NodeClass> impl)
      : impl_(std::move(impl)) {
    DCHECK(impl_.get());
  }

  TestNodeWrapper(TestNodeWrapper&& other) : impl_(std::move(other.impl_)) {}
  TestNodeWrapper& operator=(TestNodeWrapper&& other) {
    if (this != &other) {
      reset();
      impl_ = std::move(other.impl_);
    }
    return *this;
  }

  TestNodeWrapper(const TestNodeWrapper& other) = delete;
  TestNodeWrapper& operator=(const TestNodeWrapper& other) = delete;

  ~TestNodeWrapper() { reset(); }

  NodeClass* operator->() const { return impl_.get(); }

  NodeClass* get() const { return impl_.get(); }

  void reset() {
    if (impl_) {
      impl_->graph()->RemoveNode(impl_.get());
      impl_.reset();
    }
  }

 private:
  std::unique_ptr<NodeClass> impl_;
};

template <class NodeClass>
struct TestNodeWrapper<NodeClass>::Factory {
  template <typename... Args>
  static std::unique_ptr<NodeClass> Create(Args&&... args) {
    return std::make_unique<NodeClass>(std::forward<Args>(args)...);
  }
};

// A specialized factory function for frame nodes that helps fill out some
// common values.
template <>
struct TestNodeWrapper<FrameNodeImpl>::Factory {
  static std::unique_ptr<FrameNodeImpl> Create(
      ProcessNodeImpl* process_node,
      PageNodeImpl* page_node,
      FrameNodeImpl* parent_frame_node,
      int render_frame_id,
      const blink::LocalFrameToken& frame_token = blink::LocalFrameToken(),
      content::BrowsingInstanceId browsing_instance_id =
          content::BrowsingInstanceId(0),
      content::SiteInstanceId site_instance_id = content::SiteInstanceId(0)) {
    return std::make_unique<FrameNodeImpl>(
        process_node, page_node, parent_frame_node, render_frame_id,
        frame_token, browsing_instance_id, site_instance_id);
  }
};

// A specialized factory function for ProcessNodes which will provide an empty
// proxy when it's not needed.
template <>
struct TestNodeWrapper<ProcessNodeImpl>::Factory {
  static std::unique_ptr<ProcessNodeImpl> Create(BrowserProcessNodeTag tag) {
    return std::make_unique<ProcessNodeImpl>(tag);
  }
  static std::unique_ptr<ProcessNodeImpl> Create(
      RenderProcessHostProxy proxy = RenderProcessHostProxy()) {
    // Provide an empty RenderProcessHostProxy by default.
    return std::make_unique<ProcessNodeImpl>(std::move(proxy));
  }
  static std::unique_ptr<ProcessNodeImpl> Create(
      content::ProcessType process_type,
      BrowserChildProcessHostProxy proxy = BrowserChildProcessHostProxy()) {
    // Provide an empty BrowserChildProcessHostProxy by default.
    return std::make_unique<ProcessNodeImpl>(process_type, std::move(proxy));
  }
};

// A specialized factory function for page nodes that helps fill out some
// common values.
template <>
struct TestNodeWrapper<PageNodeImpl>::Factory {
  static std::unique_ptr<PageNodeImpl> Create(
      const WebContentsProxy& wc_proxy = WebContentsProxy(),
      const std::string& browser_context_id = std::string(),
      const GURL& url = GURL(),
      PagePropertyFlags initial_property_flags = {},
      base::TimeTicks visibility_change_time = base::TimeTicks::Now(),
      PageNode::PageState page_state = PageNode::PageState::kActive) {
    return std::make_unique<PageNodeImpl>(wc_proxy, browser_context_id, url,
                                          initial_property_flags,
                                          visibility_change_time, page_state);
  }
};

// A specialized factory function for worker nodes that helps fill out some
// common values.
template <>
struct TestNodeWrapper<WorkerNodeImpl>::Factory {
  static std::unique_ptr<WorkerNodeImpl> Create(
      WorkerNode::WorkerType worker_type,
      ProcessNodeImpl* process_node,
      const std::string& browser_context_id = std::string(),
      const blink::WorkerToken& token = blink::WorkerToken()) {
    return std::make_unique<WorkerNodeImpl>(browser_context_id, worker_type,
                                            process_node, token);
  }
};

// static
template <typename NodeClass>
template <typename... Args>
TestNodeWrapper<NodeClass> TestNodeWrapper<NodeClass>::Create(GraphImpl* graph,
                                                              Args&&... args) {
  // Dispatch to a helper so that we can use partial specialization.
  std::unique_ptr<NodeClass> node =
      Factory::Create(std::forward<Args>(args)...);
  graph->AddNewNode(node.get());
  return TestNodeWrapper<NodeClass>(std::move(node));
}

// This specialization is necessary because the graph has ownership of the
// system node as it's a singleton. For the other node types the test wrapper
// manages the node lifetime.
template <>
class TestNodeWrapper<SystemNodeImpl> {
 public:
  static TestNodeWrapper<SystemNodeImpl> Create(GraphImpl* graph) {
    return TestNodeWrapper<SystemNodeImpl>(graph->GetSystemNodeImpl());
  }

  explicit TestNodeWrapper(SystemNodeImpl* impl) : impl_(impl) {}
  TestNodeWrapper(TestNodeWrapper&& other) : impl_(other.impl_) {}

  TestNodeWrapper(const TestNodeWrapper&) = delete;
  TestNodeWrapper& operator=(const TestNodeWrapper&) = delete;

  ~TestNodeWrapper() { reset(); }

  SystemNodeImpl* operator->() const { return impl_; }
  SystemNodeImpl* get() const { return impl_; }

  void reset() { impl_ = nullptr; }

 private:
  raw_ptr<SystemNodeImpl> impl_;
};

class TestGraphImpl : public GraphImpl {
 public:
  TestGraphImpl();
  ~TestGraphImpl() override;

  // Creates a frame node with an automatically generated routing id, different
  // from previously generated routing ids. Useful for tests that don't care
  // about the frame routing id but need to avoid collisions in
  // |GraphImpl::frames_by_id_|. Caveat: The generated routing id is not
  // guaranteed to be different from routing ids set explicitly by the test.
  TestNodeWrapper<FrameNodeImpl> CreateFrameNodeAutoId(
      ProcessNodeImpl* process_node,
      PageNodeImpl* page_node,
      FrameNodeImpl* parent_frame_node = nullptr);

 private:
  int next_frame_routing_id_ = 0;
};

// A test harness that initializes the graph without the rest of
// PerformanceManager. Allows for creating individual nodes without going
// through an embedder. The structs in mock_graphs.h are useful for this.
//
// This is intended for testing code that is entirely bound to the
// PerformanceManager sequence. Since the PerformanceManager itself is not
// initialized messages posted using CallOnGraph or
// PerformanceManager::GetTaskRunner will go into the void. To test code that
// posts to and from the PerformanceManager sequence use
// PerformanceManagerTestHarness.
//
// If you need to write tests that manipulate graph nodes and also use
// CallOnGraph, you probably want to split the code under test into a
// sequence-bound portion that deals with the graph (tested using
// GraphTestHarness) and an interface that marshals to the PerformanceManager
// sequence (tested using PerformanceManagerTestHarness).
class GraphTestHarness : public ::testing::Test {
 public:
  GraphTestHarness();
  ~GraphTestHarness() override;

  // Optional constructor for directly configuring the BrowserTaskEnvironment.
  template <class... ArgTypes>
  explicit GraphTestHarness(ArgTypes... args)
      : task_env_(args...), graph_(new TestGraphImpl()) {}

  template <class NodeClass, typename... Args>
  TestNodeWrapper<NodeClass> CreateNode(Args&&... args) {
    return TestNodeWrapper<NodeClass>::Create(graph(),
                                              std::forward<Args>(args)...);
  }

  TestNodeWrapper<FrameNodeImpl> CreateFrameNodeAutoId(
      ProcessNodeImpl* process_node,
      PageNodeImpl* page_node,
      FrameNodeImpl* parent_frame_node = nullptr) {
    return graph()->CreateFrameNodeAutoId(process_node, page_node,
                                          parent_frame_node);
  }

  TestNodeWrapper<SystemNodeImpl> GetSystemNode() {
    return TestNodeWrapper<SystemNodeImpl>(graph()->GetSystemNodeImpl());
  }

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // Allows configuring which Graph features are initialized during "SetUp".
  // This defaults to initializing no features. Features will be initialized
  // before "OnGraphCreated" is called.
  GraphFeatures& GetGraphFeatures() { return graph_features_; }

  // A callback that will be invoked as part of the graph initialization
  // during "SetUp". The same effect can be had by overriding "SetUp" in this
  // case, because the graph lives on the same sequence as this fixture.
  // However, to keep the various PM and Graph test fixtures similar in usage,
  // this seam has been exposed.
  virtual void OnGraphCreated(GraphImpl* graph) {}

 protected:
  void AdvanceClock(base::TimeDelta delta) { task_env_.FastForwardBy(delta); }

  content::BrowserTaskEnvironment& task_env() { return task_env_; }
  TestGraphImpl* graph() {
    DCHECK(graph_.get());
    return graph_.get();
  }

  // Manually tears down the graph. Useful for DEATH tests that deliberately
  // violate graph invariants.
  void TearDownAndDestroyGraph();

 private:
  GraphFeatures graph_features_;
  content::BrowserTaskEnvironment task_env_;
  std::unique_ptr<TestGraphImpl> graph_;

  // Detects when the test fixture is being misused.
  bool setup_called_ = false;
  bool teardown_called_ = false;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_GRAPH_TEST_HARNESS_H_
