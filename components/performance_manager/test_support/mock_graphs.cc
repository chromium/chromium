// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/mock_graphs.h"

#include <string>
#include <utility>

#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/node_base.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/system_node_impl.h"
#include "components/performance_manager/public/browser_child_process_host_id.h"
#include "components/performance_manager/public/browser_child_process_host_proxy.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "content/public/common/process_type.h"

namespace performance_manager {

const content::BrowsingInstanceId kBrowsingInstanceForPage =
    content::BrowsingInstanceId::FromUnsafeValue(1);
const content::BrowsingInstanceId kBrowsingInstanceForOtherPage =
    content::BrowsingInstanceId::FromUnsafeValue(2);

TestProcessNodeImpl::TestProcessNodeImpl()
    : ProcessNodeImpl(RenderProcessHostProxy::CreateForTesting(
                          NextTestRenderProcessHostId()),
                      base::TaskPriority::HIGHEST) {}

TestProcessNodeImpl::TestProcessNodeImpl(content::ProcessType process_type)
    : ProcessNodeImpl(process_type,
                      BrowserChildProcessHostProxy::CreateForTesting(
                          NextTestBrowserChildProcessHostId())) {}

void TestProcessNodeImpl::SetProcessWithPid(base::ProcessId pid,
                                            base::Process process,
                                            base::TimeTicks launch_time) {
  SetProcessImpl(std::move(process), pid, launch_time);
}

MockSinglePageInSingleProcessGraph::MockSinglePageInSingleProcessGraph(
    TestGraphImpl* graph)
    : system(TestNodeWrapper<SystemNodeImpl>::Create(graph)),
      browser_process(TestNodeWrapper<TestProcessNodeImpl>::Create(
          graph,
          BrowserProcessNodeTag{})),
      process(TestNodeWrapper<TestProcessNodeImpl>::Create(graph)),
      page(TestNodeWrapper<PageNodeImpl>::Create(graph)),
      frame(graph->CreateFrameNodeAutoId(process.get(),
                                         page.get(),
                                         /*parent_frame_node=*/nullptr,
                                         kBrowsingInstanceForPage)) {
  browser_process->SetProcessWithPid(1);
  process->SetProcessWithPid(2);
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
                                               /*parent_frame_node=*/nullptr,
                                               kBrowsingInstanceForOtherPage)) {
}

MockMultiplePagesInSingleProcessGraph::
    ~MockMultiplePagesInSingleProcessGraph() {
  other_frame.reset();
  other_page.reset();
}

MockManyPagesInSingleProcessGraph::MockManyPagesInSingleProcessGraph(
    TestGraphImpl* graph,
    size_t num_other_pages)
    : MockSinglePageInSingleProcessGraph(graph) {
  other_pages.reserve(num_other_pages);
  other_frames.reserve(num_other_pages);
  for (size_t i = 0; i < num_other_pages; ++i) {
    other_pages.push_back(TestNodeWrapper<PageNodeImpl>::Create(graph));
    other_frames.push_back(graph->CreateFrameNodeAutoId(
        process.get(), other_pages[i].get(), /*parent_frame_node=*/nullptr));
  }
}

MockManyPagesInSingleProcessGraph::~MockManyPagesInSingleProcessGraph() {
  for (auto& f : other_frames) {
    f.reset();
  }

  for (auto& p : other_pages) {
    p.reset();
  }
}

MockSinglePageWithMultipleProcessesGraph::
    MockSinglePageWithMultipleProcessesGraph(TestGraphImpl* graph)
    : MockSinglePageInSingleProcessGraph(graph),
      other_process(TestNodeWrapper<TestProcessNodeImpl>::Create(graph)),
      child_frame(graph->CreateFrameNodeAutoId(other_process.get(),
                                               page.get(),
                                               frame.get(),
                                               kBrowsingInstanceForPage)) {
  other_process->SetProcessWithPid(3);
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
                                               kBrowsingInstanceForOtherPage)) {
  other_process->SetProcessWithPid(3);
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

MockUtilityAndMultipleRenderProcessesGraph::
    MockUtilityAndMultipleRenderProcessesGraph(TestGraphImpl* graph)
    : MockMultiplePagesAndWorkersWithMultipleProcessesGraph(graph),
      utility_process(TestNodeWrapper<TestProcessNodeImpl>::Create(
          graph,
          content::ProcessType::PROCESS_TYPE_UTILITY)) {
  utility_process->SetProcessWithPid(4);
}

MockUtilityAndMultipleRenderProcessesGraph::
    ~MockUtilityAndMultipleRenderProcessesGraph() = default;

}  // namespace performance_manager
