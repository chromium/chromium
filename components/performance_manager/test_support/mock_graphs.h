// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_MOCK_GRAPHS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_MOCK_GRAPHS_H_

#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"

namespace performance_manager {

class GraphImpl;
class FrameNodeImpl;
class PageNodeImpl;
class SystemNodeImpl;

// A for-testing subclass of the process node that allows mocking the
// process' PID.
class TestProcessNodeImpl : public ProcessNodeImpl {
 public:
  explicit TestProcessNodeImpl(GraphImpl* graph);

  void SetProcessWithPid(base::ProcessId pid,
                         base::Process process,
                         base::Time launch_time);
};

// The following graph topology is created to emulate a scenario when a single
// page executes in a single process:
//
// Pr  Pg
//  \ /
//   F
//
// Where:
// F: frame(frame_tree_id:0)
// Pr: process(pid:1)
// Pg: page
struct MockSinglePageInSingleProcessGraph {
  explicit MockSinglePageInSingleProcessGraph(TestGraphImpl* graph);
  ~MockSinglePageInSingleProcessGraph();
  TestNodeWrapper<SystemNodeImpl> system;
  TestNodeWrapper<TestProcessNodeImpl> process;
  TestNodeWrapper<PageNodeImpl> page;
  TestNodeWrapper<FrameNodeImpl> frame;
};

// The following graph topology is created to emulate a scenario where multiple
// pages are executing in a single process:
//
// Pg  Pr OPg
//  \ / \ /
//   F  OF
//
// Where:
// F: frame(frame_tree_id:0)
// OF: other_frame(frame_tree_id:1)
// Pg: page
// OPg: other_page
// Pr: process(pid:1)
struct MockMultiplePagesInSingleProcessGraph
    : public MockSinglePageInSingleProcessGraph {
  explicit MockMultiplePagesInSingleProcessGraph(TestGraphImpl* graph);
  ~MockMultiplePagesInSingleProcessGraph();
  TestNodeWrapper<PageNodeImpl> other_page;
  TestNodeWrapper<FrameNodeImpl> other_frame;
};

// The following graph topology is created to emulate a scenario where a single
// page that has frames is executing in different processes (e.g. out-of-process
// iFrames):
//
// Pg  Pr
// |\ /
// | F  OPr
// |  \ /
// |__CF
//
// Where:
// F: frame(frame_tree_id:0)
// CF: child_frame(frame_tree_id:2)
// Pg: page
// Pr: process(pid:1)
// OPr: other_process(pid:2)
struct MockSinglePageWithMultipleProcessesGraph
    : public MockSinglePageInSingleProcessGraph {
  explicit MockSinglePageWithMultipleProcessesGraph(TestGraphImpl* graph);
  ~MockSinglePageWithMultipleProcessesGraph();
  TestNodeWrapper<TestProcessNodeImpl> other_process;
  TestNodeWrapper<FrameNodeImpl> child_frame;
};

// The following graph topology is created to emulate a scenario where multiple
// pages are utilizing multiple processes (e.g. out-of-process iFrames and
// multiple pages in a process):
//
// Pg  Pr OPg___
//  \ / \ /     |
//   F   OF OPr |
//        \ /   |
//         CF___|
//
// Where:
// F: frame(frame_tree_id:0)
// OF: other_frame(frame_tree_id:1)
// CF: child_frame(frame_tree_id:3)
// Pg: page
// OPg: other_page
// Pr: process(pid:1)
// OPr: other_process(pid:2)
struct MockMultiplePagesWithMultipleProcessesGraph
    : public MockMultiplePagesInSingleProcessGraph {
  explicit MockMultiplePagesWithMultipleProcessesGraph(TestGraphImpl* graph);
  ~MockMultiplePagesWithMultipleProcessesGraph();
  TestNodeWrapper<TestProcessNodeImpl> other_process;
  TestNodeWrapper<FrameNodeImpl> child_frame;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_MOCK_GRAPHS_H_
