// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/performance_manager.h"

#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/render_frame_host_proxy.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace performance_manager {

class PerformanceManagerTest : public PerformanceManagerTestHarness {
 public:
  using Super = PerformanceManagerTestHarness;

  PerformanceManagerTest() = default;

  void SetUp() override {
    EXPECT_FALSE(PerformanceManager::IsAvailable());
    Super::SetUp();
    EXPECT_TRUE(PerformanceManager::IsAvailable());
  }

  void TearDown() override {
    EXPECT_TRUE(PerformanceManager::IsAvailable());
    Super::TearDown();
    EXPECT_FALSE(PerformanceManager::IsAvailable());
  }

  PerformanceManagerTest(const PerformanceManagerTest&) = delete;
  PerformanceManagerTest& operator=(const PerformanceManagerTest&) = delete;

  ~PerformanceManagerTest() override = default;
};

TEST_F(PerformanceManagerTest, NodeAccessors) {
  auto contents = CreateTestWebContents();
  content::RenderFrameHost* rfh = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(rfh);
  content::RenderProcessHost* rph = rfh->GetProcess();
  ASSERT_TRUE(rph);

  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(contents.get());

  // FrameNode's and ProcessNode's don't exist until an observer fires on
  // navigation. Verify that looking them up before that returns null instead
  // of crashing.
  EXPECT_FALSE(PerformanceManager::GetFrameNodeForRenderFrameHost(rfh));
  EXPECT_FALSE(PerformanceManager::GetProcessNodeForRenderProcessHost(rph));

  // Simulate a committed navigation to create the nodes.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      contents.get(), GURL("https://www.example.com/"));
  base::WeakPtr<FrameNode> frame_node =
      PerformanceManager::GetFrameNodeForRenderFrameHost(rfh);
  base::WeakPtr<ProcessNode> process_node =
      PerformanceManager::GetProcessNodeForRenderProcessHost(rph);

  // Post a task to the Graph and make it call a function on the UI thread that
  // will ensure that the nodes are really associated with the content objects.

  base::RunLoop run_loop;
  auto check_proxies_on_main_thread = base::BindLambdaForTesting(
      [&](base::WeakPtr<content::WebContents> weak_contents,
          const RenderFrameHostProxy& rfh_proxy,
          const RenderProcessHostProxy& rph_proxy) {
        EXPECT_EQ(contents.get(), weak_contents.get());
        EXPECT_EQ(rfh, rfh_proxy.Get());
        EXPECT_EQ(rph, rph_proxy.Get());
        run_loop.Quit();
      });

  auto call_on_graph_cb = base::BindLambdaForTesting([&]() {
    EXPECT_TRUE(page_node.get());
    EXPECT_TRUE(frame_node.get());
    EXPECT_TRUE(process_node.get());
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(check_proxies_on_main_thread),
                                  page_node->GetWebContents(),
                                  frame_node->GetRenderFrameHostProxy(),
                                  process_node->GetRenderProcessHostProxy()));
  });

  PerformanceManager::CallOnGraph(FROM_HERE, call_on_graph_cb);

  // Wait for |check_proxies_on_main_thread| to be called.
  run_loop.Run();

  contents.reset();

  // After deleting |contents| the corresponding WeakPtr's should be
  // invalid.
  base::RunLoop run_loop_after_contents_reset;
  auto quit_closure = run_loop_after_contents_reset.QuitClosure();
  auto call_on_graph_cb_2 = base::BindLambdaForTesting([&]() {
    EXPECT_FALSE(page_node.get());
    EXPECT_FALSE(frame_node.get());
    EXPECT_FALSE(process_node.get());
    std::move(quit_closure).Run();
  });

  PerformanceManager::CallOnGraph(FROM_HERE, call_on_graph_cb_2);
  run_loop_after_contents_reset.Run();
}

}  // namespace performance_manager
