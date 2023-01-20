// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/mock_graphs.h"

#include <string>
#include <utility>

#include "base/process/process.h"
#include "base/time/time.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/node_base.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/system_node_impl.h"

namespace performance_manager {

TestProcessNodeImpl::TestProcessNodeImpl()
    : ProcessNodeImpl(RenderProcessHostProxy()) {}

void TestProcessNodeImpl::SetProcessWithPid(base::ProcessId pid,
                                            base::Process process,
                                            base::TimeTicks launch_time) {
  SetProcessImpl(std::move(process), pid, launch_time);
}

MockSinglePageInSingleProcessGraph::MockSinglePageInSingleProcessGraph(
    TestGraphImpl* graph)
    : system(TestNodeWrapper<SystemNodeImpl>::Create(graph)),
      process(TestNodeWrapper<TestProcessNodeImpl>::Create(graph)),
      page(TestNodeWrapper<PageNodeImpl>::Create(graph)),
      frame(graph->CreateFrameNodeAutoId(process.get(), page.get())) {
  process->SetProcessWithPid(1, base::Process::Current(),
                             /* launch_time=*/base::TimeTicks::Now());
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
                                               nullptr)) {}

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
                                               frame.get())) {
  other_process->SetProcessWithPid(2, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
}

MockSinglePageWithMultipleProcessesGraph::
    ~MockSinglePageWithMultipleProcessesGraph() = default;

MockMultiplePagesWithMultipleProcessesGraph::
    MockMultiplePagesWithMultipleProcessesGraph(TestGraphImpl* graph)
    : MockMultiplePagesInSingleProcessGraph(graph),
      other_process(TestNodeWrapper<TestProcessNodeImpl>::Create(graph)),
      child_frame(graph->CreateFrameNodeAutoId(other_process.get(),
                                               other_page.get(),
                                               other_frame.get())) {
  other_process->SetProcessWithPid(2, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
}

MockMultiplePagesWithMultipleProcessesGraph::
    ~MockMultiplePagesWithMultipleProcessesGraph() = default;

MockSinglePageWithFrameAndWorkerInSingleProcessGraph::
    MockSinglePageWithFrameAndWorkerInSingleProcessGraph(TestGraphImpl* graph)
    : MockSinglePageInSingleProcessGraph(graph),
      worker(TestNodeWrapper<WorkerNodeImpl>::Create(
          graph,
          WorkerNode::WorkerType::kDedicated,
          process.get())) {
  worker->AddClientFrame(frame.get());
}

MockSinglePageWithFrameAndWorkerInSingleProcessGraph::
    ~MockSinglePageWithFrameAndWorkerInSingleProcessGraph() {
  if (worker.get())
    worker->RemoveClientFrame(frame.get());
}

void MockSinglePageWithFrameAndWorkerInSingleProcessGraph::DeleteWorker() {
  DCHECK(worker.get());
  worker->RemoveClientFrame(frame.get());
  worker.reset();
}

MockMultiplePagesAndWorkersWithMultipleProcessesGraph::
    MockMultiplePagesAndWorkersWithMultipleProcessesGraph(TestGraphImpl* graph)
    : MockMultiplePagesWithMultipleProcessesGraph(graph),
      worker(TestNodeWrapper<WorkerNodeImpl>::Create(
          graph,
          WorkerNode::WorkerType::kDedicated,
          process.get())),
      other_worker(TestNodeWrapper<WorkerNodeImpl>::Create(
          graph,
          WorkerNode::WorkerType::kDedicated,
          other_process.get())) {
  worker->AddClientFrame(frame.get());
  other_worker->AddClientFrame(child_frame.get());
}

MockMultiplePagesAndWorkersWithMultipleProcessesGraph::
    ~MockMultiplePagesAndWorkersWithMultipleProcessesGraph() {
  other_worker->RemoveClientFrame(child_frame.get());
  worker->RemoveClientFrame(frame.get());
}

}  // namespace performance_manager
