// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/render_process_host_proxy.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/test/bind.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/render_process_user_data.h"
#include "components/performance_manager/test_support/performance_manager_browsertest_harness.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

using RenderProcessHostProxyTest = PerformanceManagerBrowserTestHarness;

IN_PROC_BROWSER_TEST_F(RenderProcessHostProxyTest,
                       RPHDeletionInvalidatesProxy) {
  // Navigate, and block until the navigation has completed. RPH, RFH, etc, will
  // all exist when this is finished.
  ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  // Get the RPH associated with the main frame.
  content::RenderProcessHost* host =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();

  // And its associated ProcessNode.
  auto* render_process_user_data =
      RenderProcessUserData::GetForRenderProcessHost(host);
  ASSERT_NE(render_process_user_data, nullptr);
  ProcessNode* process_node = render_process_user_data->process_node();
  ASSERT_NE(process_node, nullptr);

  RenderProcessHostProxy proxy = process_node->GetRenderProcessHostProxy();
  EXPECT_EQ(proxy.Get(), host);

  shell()->Close();
  EXPECT_EQ(proxy.Get(), nullptr);
}

}  // namespace performance_manager
