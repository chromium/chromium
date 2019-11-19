// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/graph_test_harness.h"

#include "base/bind.h"
#include "base/run_loop.h"

namespace performance_manager {

TestGraphImpl::TestGraphImpl() = default;
TestGraphImpl::~TestGraphImpl() = default;

TestNodeWrapper<FrameNodeImpl> TestGraphImpl::CreateFrameNodeAutoId(
    ProcessNodeImpl* process_node,
    PageNodeImpl* page_node,
    FrameNodeImpl* parent_frame_node,
    int frame_tree_node_id) {
  return TestNodeWrapper<FrameNodeImpl>::Create(
      this, process_node, page_node, parent_frame_node, frame_tree_node_id,
      ++next_frame_routing_id_);
}

GraphTestHarness::GraphTestHarness()
    : task_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME,
                base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}

GraphTestHarness::~GraphTestHarness() {
  // Ideally this would be done in TearDown(), but that would require subclasses
  // do destroy all their nodes before invoking TearDown below.
  graph_.TearDown();
}

void GraphTestHarness::TearDown() {
  base::RunLoop().RunUntilIdle();
}

}  // namespace performance_manager
