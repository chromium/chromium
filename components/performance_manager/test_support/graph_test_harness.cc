// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/graph_test_harness.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"

namespace performance_manager {

int NextTestFrameRoutingId() {
  static int next_frame_routing_id_;
  return ++next_frame_routing_id_;
}

RenderProcessHostId NextTestRenderProcessHostId() {
  static RenderProcessHostId::Generator id_generator;
  return id_generator.GenerateNextId();
}

BrowserChildProcessHostId NextTestBrowserChildProcessHostId() {
  static BrowserChildProcessHostId::Generator id_generator;
  return id_generator.GenerateNextId();
}

TestGraphImpl::TestGraphImpl() = default;
TestGraphImpl::~TestGraphImpl() = default;

TestNodeWrapper<FrameNodeImpl> TestGraphImpl::CreateFrameNodeAutoId(
    ProcessNodeImpl* process_node,
    PageNodeImpl* page_node,
    FrameNodeImpl* parent_frame_node,
    content::BrowsingInstanceId browsing_instance_id) {
  return TestNodeWrapper<FrameNodeImpl>::Create(
      this, process_node, page_node, parent_frame_node,
      /*outer_document_for_fenced_frame=*/nullptr, NextTestFrameRoutingId(),
      blink::LocalFrameToken(), browsing_instance_id);
}

TestNodeWrapper<ProcessNodeImpl> TestGraphImpl::CreateBrowserProcessNode() {
  return TestNodeWrapper<ProcessNodeImpl>::Create(this,
                                                  BrowserProcessNodeTag{});
}

TestNodeWrapper<ProcessNodeImpl> TestGraphImpl::CreateRendererProcessNode(
    RenderProcessHostProxy proxy) {
  return TestNodeWrapper<ProcessNodeImpl>::Create(this, std::move(proxy));
}

TestNodeWrapper<ProcessNodeImpl> TestGraphImpl::CreateBrowserChildProcessNode(
    content::ProcessType process_type,
    BrowserChildProcessHostProxy proxy) {
  return TestNodeWrapper<ProcessNodeImpl>::Create(this, process_type,
                                                  std::move(proxy));
}

GraphTestHarness::GraphTestHarness()
    : task_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME,
                base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED),
      graph_(new TestGraphImpl()) {}

GraphTestHarness::~GraphTestHarness() {
  // These will fire if this class is derived from, and SetUp or TearDown are
  // overridden but not called from the derived class.
  static constexpr char kNotCalled[] =
      " was not called. This probably means that the "
      "developer has overridden the method and not called "
      "the superclass version.";
  CHECK(setup_called_ || IsSkipped()) << "SetUp" << kNotCalled;
  CHECK(teardown_called_ || IsSkipped()) << "TearDown" << kNotCalled;

  // Ideally this would be done in TearDown(), but that would require subclasses
  // to destroy all their nodes before invoking TearDown below.
  if (graph_)
    graph_->TearDown();
}

void GraphTestHarness::SetUp() {
  setup_called_ = true;

  graph_->SetUp();
  graph_features_.ConfigureGraph(graph_.get());

  // This can't be done in the constructor because it is a virtual function.
  OnGraphCreated(graph_.get());
}

void GraphTestHarness::TearDown() {
  teardown_called_ = true;
  base::RunLoop().RunUntilIdle();
}

void GraphTestHarness::TearDownAndDestroyGraph() {
  graph_->TearDown();
  graph_.reset();
}

}  // namespace performance_manager
