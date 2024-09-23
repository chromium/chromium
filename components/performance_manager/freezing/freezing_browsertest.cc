// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/run_loop.h"
#include "components/performance_manager/freezing/freezer.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/performance_manager_browsertest_harness.h"
#include "components/performance_manager/test_support/run_in_graph.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"

namespace performance_manager {

namespace {

using PerformanceManagerFreezingBrowserTest =
    PerformanceManagerBrowserTestHarness;

class PageLifecycleWaiter : public PageNode::ObserverDefaultImpl {
 public:
  PageLifecycleWaiter() = default;

  void SetExpectedLifecycleState(PageNode::LifecycleState lifecycle_state) {
    run_loop_ = std::make_unique<base::RunLoop>();
    expected_lifecycle_state_ = lifecycle_state;
  }
  void WaitForExpectedLifecycleState() {
    CHECK(run_loop_);
    run_loop_->Run();
    run_loop_.reset();
  }

  // PageNodeObserver:
  void OnPageLifecycleStateChanged(const PageNode* page_node) override {
    CHECK(run_loop_);
    EXPECT_EQ(page_node->GetLifecycleState(),
              expected_lifecycle_state_.value());
    run_loop_->Quit();
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  std::optional<PageNode::LifecycleState> expected_lifecycle_state_;
};

}  // namespace

// Verifies that a page can be frozen a second time after being unfrozen by a
// visibility change (regression test for issue fixed by crrev.com/c/5827022).
IN_PROC_BROWSER_TEST_F(PerformanceManagerFreezingBrowserTest, FreezeTwice) {
  Freezer freezer;
  ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  content::WebContents* contents = shell()->web_contents();
  contents->WasHidden();
  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(contents);

  PageLifecycleWaiter waiter;
  RunInGraph([&](Graph* graph) { graph->AddPageNodeObserver(&waiter); });

  waiter.SetExpectedLifecycleState(PageNode::LifecycleState::kFrozen);
  freezer.MaybeFreezePageNode(page_node.get());
  waiter.WaitForExpectedLifecycleState();

  waiter.SetExpectedLifecycleState(PageNode::LifecycleState::kRunning);
  contents->WasShown();
  waiter.WaitForExpectedLifecycleState();

  contents->WasHidden();
  waiter.SetExpectedLifecycleState(PageNode::LifecycleState::kFrozen);
  freezer.MaybeFreezePageNode(page_node.get());
  waiter.WaitForExpectedLifecycleState();

  RunInGraph([&](Graph* graph) { graph->RemovePageNodeObserver(&waiter); });
}

}  // namespace performance_manager
