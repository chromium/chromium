// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/mock_graphs.h"

#include "base/process/process_handle.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/browser_child_process_host_id.h"
#include "components/performance_manager/public/browser_child_process_host_proxy.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "content/public/common/process_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

using MockGraphsTest = GraphTestHarness;

TEST_F(MockGraphsTest, ProcessNodes) {
  MockUtilityAndMultipleRenderProcessesGraph mock_graph(graph());

  EXPECT_EQ(mock_graph.browser_process->GetProcessType(),
            content::ProcessType::PROCESS_TYPE_BROWSER);
  EXPECT_EQ(mock_graph.process->GetProcessType(),
            content::ProcessType::PROCESS_TYPE_RENDERER);
  EXPECT_EQ(mock_graph.other_process->GetProcessType(),
            content::ProcessType::PROCESS_TYPE_RENDERER);
  EXPECT_EQ(mock_graph.utility_process->GetProcessType(),
            content::ProcessType::PROCESS_TYPE_UTILITY);

  // Make sure the ProcessNodes have the PID's documented in mock_graphs.h.
  EXPECT_EQ(mock_graph.browser_process->GetProcessId(), base::ProcessId(1));
  EXPECT_EQ(mock_graph.process->GetProcessId(), base::ProcessId(2));
  EXPECT_EQ(mock_graph.other_process->GetProcessId(), base::ProcessId(3));
  EXPECT_EQ(mock_graph.utility_process->GetProcessId(), base::ProcessId(4));

  // Make sure the ProcessNodes have valid RenderProcessHostId's or
  // BrowserChildProcessHostId's.
  const RenderProcessHostProxy& process_proxy =
      mock_graph.process->GetRenderProcessHostProxy();
  const RenderProcessHostProxy& other_process_proxy =
      mock_graph.other_process->GetRenderProcessHostProxy();
  const BrowserChildProcessHostProxy& utility_process_proxy =
      mock_graph.utility_process->GetBrowserChildProcessHostProxy();
  EXPECT_FALSE(process_proxy.render_process_host_id().is_null());
  EXPECT_FALSE(other_process_proxy.render_process_host_id().is_null());
  EXPECT_NE(process_proxy.render_process_host_id(),
            other_process_proxy.render_process_host_id());
  EXPECT_FALSE(utility_process_proxy.browser_child_process_host_id().is_null());

  // Add a custom node to the graph.
  const auto custom_process = TestNodeWrapper<TestProcessNodeImpl>::Create(
      graph(), content::ProcessType::PROCESS_TYPE_GPU);
  const BrowserChildProcessHostProxy& custom_process_proxy =
      custom_process->GetBrowserChildProcessHostProxy();
  EXPECT_FALSE(custom_process_proxy.browser_child_process_host_id().is_null());
  EXPECT_NE(utility_process_proxy.browser_child_process_host_id(),
            custom_process_proxy.browser_child_process_host_id());
}

}  // namespace

}  // namespace performance_manager
