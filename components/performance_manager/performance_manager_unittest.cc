// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/performance_manager.h"

#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "components/performance_manager/performance_manager_test_harness.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/web_contents_proxy.h"
#include "content/public/browser/browser_task_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

class PerformanceManagerTest : public PerformanceManagerTestHarness {
 public:
  using Super = PerformanceManagerTestHarness;

  PerformanceManagerTest() {}

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

  ~PerformanceManagerTest() override {}

  DISALLOW_COPY_AND_ASSIGN(PerformanceManagerTest);
};

TEST_F(PerformanceManagerTest, GetPageNodeForWebContents) {
  auto contents = CreateTestWebContents();

  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPageNodeForWebContents(contents.get());

  // Post a task to the Graph and make it call a function on the UI thread that
  // will ensure that |page_node| is really associated with |contents|.

  base::RunLoop run_loop;
  auto check_wc_on_main_thread =
      base::BindLambdaForTesting([&](const WebContentsProxy& wc_proxy) {
        EXPECT_EQ(contents.get(), wc_proxy.Get());
        run_loop.Quit();
      });

  auto call_on_graph_cb = base::BindLambdaForTesting([&](Graph* unused) {
    EXPECT_TRUE(page_node.get());
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   base::BindOnce(std::move(check_wc_on_main_thread),
                                  page_node->GetContentsProxy()));
  });

  PerformanceManager::CallOnGraph(FROM_HERE, call_on_graph_cb);

  // Wait for |check_wc_on_main_thread| to be called.
  run_loop.Run();

  contents.reset();

  // After deleting |contents| the corresponding PageNode WeakPtr should be
  // invalid.
  base::RunLoop run_loop_after_contents_reset;
  auto quit_closure = run_loop_after_contents_reset.QuitClosure();
  auto call_on_graph_cb_2 = base::BindLambdaForTesting([&](Graph* unused) {
    EXPECT_FALSE(page_node.get());
    std::move(quit_closure).Run();
  });

  PerformanceManager::CallOnGraph(FROM_HERE, call_on_graph_cb_2);
  run_loop_after_contents_reset.Run();
}

}  // namespace performance_manager
