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

  // Helper for dereferencing a RenderProcessHostProxy on the main thread,
  // and saving its value in |proxy_host|.
  content::RenderProcessHost* proxy_host = nullptr;
  auto deref_proxy = base::BindLambdaForTesting(
      [&proxy_host](const RenderProcessHostProxy& proxy,
                    base::OnceClosure quit_loop) {
        proxy_host = proxy.Get();
        std::move(quit_loop).Run();
      });

  // Get the RPH associated with the main frame.
  content::RenderProcessHost* host =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();

  // And its associated ProcessNode.
  auto* render_process_user_data =
      RenderProcessUserData::GetForRenderProcessHost(host);
  ASSERT_NE(render_process_user_data, nullptr);
  ProcessNode* process_node = render_process_user_data->process_node();
  ASSERT_NE(process_node, nullptr);

  // Bounce over to the PM sequence, retrieve the proxy, bounce back to the UI
  // thread, dereference it if possible, and save the returned host. To be
  // fair, it's entirely valid to grab the weak pointer directly on the UI
  // thread, as the lifetime of the process node is managed there and the
  // property being accessed is thread safe. However, this test aims to simulate
  // what would happen with a policy message being posted from the graph.
  {
    base::RunLoop run_loop;
    PerformanceManager::CallOnGraph(
        FROM_HERE,
        base::BindLambdaForTesting(
            [&deref_proxy, process_node, quit_loop = run_loop.QuitClosure()]() {
              content::GetUIThreadTaskRunner({})->PostTask(
                  FROM_HERE,
                  base::BindOnce(deref_proxy,
                                 process_node->GetRenderProcessHostProxy(),
                                 std::move(quit_loop)));
            }));
    run_loop.Run();

    // We should see the RPH via the proxy.
    EXPECT_EQ(host, proxy_host);
  }

  // Run the same test but make sure the RPH is gone first.
  {
    base::RunLoop run_loop;
    PerformanceManagerImpl::CallOnGraphImpl(
        FROM_HERE,
        base::BindLambdaForTesting([&deref_proxy, process_node,
                                    shell = this->shell(),
                                    quit_loop = run_loop.QuitClosure()]() {
          content::GetUIThreadTaskRunner({})->PostTask(
              FROM_HERE,
              base::BindLambdaForTesting([shell]() { shell->Close(); }));
          content::GetUIThreadTaskRunner({})->PostTask(
              FROM_HERE,
              base::BindOnce(deref_proxy,
                             process_node->GetRenderProcessHostProxy(),
                             std::move(quit_loop)));
        }));
    run_loop.Run();

    // The process was destroyed on the UI thread prior to dereferencing the
    // proxy, so it should return nullptr.
    EXPECT_EQ(proxy_host, nullptr);
  }
}

}  // namespace performance_manager
