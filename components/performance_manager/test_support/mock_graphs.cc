// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/mock_graphs.h"

#include <string>

#include "base/process/process.h"
#include "base/time/time.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/node_base.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/system_node_impl.h"

namespace performance_manager {

TestProcessNodeImpl::TestProcessNodeImpl(GraphImpl* graph)
    : ProcessNodeImpl(graph, RenderProcessHostProxy()) {}

void TestProcessNodeImpl::SetProcessWithPid(base::ProcessId pid,
                                            base::Process process,
                                            base::Time launch_time) {
  SetProcessImpl(std::move(process), pid, launch_time);
}

MockSinglePageInSingleProcessGraph::MockSinglePageInSingleProcessGraph(
    TestGraphImpl* graph)
    : system(TestNodeWrapper<SystemNodeImpl>::Create(graph)),
      process(TestNodeWrapper<TestProcessNodeImpl>::Create(graph)),
      page(TestNodeWrapper<PageNodeImpl>::Create(graph)),
      frame(graph->CreateFrameNodeAutoId(process.get(), page.get())) {
  process->SetProcessWithPid(1, base::Process::Current(), base::Time::Now());
}

MockSinglePageInSingleProcessGraph::~MockSinglePageInSingleProcessGraph() {
  // Make sure frame nodes are torn down before pages.
  frame.reset();
  page.reset();
}

MockMultiplePagesInSingleProcessGraph::MockMultiplePagesInSingleProcessGraph(
    TestGraphImpl* graph)
    : MockSinglePageInSingleProcessGraph(graph),
      other_page(TestNodeWrapper<PageNodeImpl>::Create(graph)),
      other_frame(graph->CreateFrameNodeAutoId(process.get(),
                                               other_page.get(),
                                               nullptr,
                                               1)) {}

MockMultiplePagesInSingleProcessGraph::
    ~MockMultiplePagesInSingleProcessGraph() {
  other_frame.reset();
  other_page.reset();
}

MockSinglePageWithMultipleProcessesGraph::
    MockSinglePageWithMultipleProcessesGraph(TestGraphImpl* graph)
    : MockSinglePageInSingleProcessGraph(graph),
      other_process(TestNodeWrapper<TestProcessNodeImpl>::Create(graph)),
      child_frame(graph->CreateFrameNodeAutoId(other_process.get(),
                                               page.get(),
                                               frame.get(),
                                               2)) {
  other_process->SetProcessWithPid(2, base::Process::Current(),
                                   base::Time::Now());
}

MockSinglePageWithMultipleProcessesGraph::
    ~MockSinglePageWithMultipleProcessesGraph() = default;

MockMultiplePagesWithMultipleProcessesGraph::
    MockMultiplePagesWithMultipleProcessesGraph(TestGraphImpl* graph)
    : MockMultiplePagesInSingleProcessGraph(graph),
      other_process(TestNodeWrapper<TestProcessNodeImpl>::Create(graph)),
      child_frame(graph->CreateFrameNodeAutoId(other_process.get(),
                                               other_page.get(),
                                               other_frame.get(),
                                               3)) {
  other_process->SetProcessWithPid(2, base::Process::Current(),
                                   base::Time::Now());
}

MockMultiplePagesWithMultipleProcessesGraph::
    ~MockMultiplePagesWithMultipleProcessesGraph() = default;

}  // namespace performance_manager
