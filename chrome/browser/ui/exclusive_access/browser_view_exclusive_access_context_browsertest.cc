// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/browser_view_exclusive_access_context.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/views/exclusive_access/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/base/base_window.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

class BrowserViewExclusiveAccessContextTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(features::kWebium);
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowserViewExclusiveAccessContextTest,
                       ManagerUsesBrowserViewContext) {
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser()));

  auto* context = BrowserViewExclusiveAccessContext::From(browser());
  ASSERT_TRUE(context);
  auto* manager = ExclusiveAccessManager::From(browser());
  ASSERT_TRUE(manager);
  EXPECT_EQ(context, manager->context());
}

IN_PROC_BROWSER_TEST_F(BrowserViewExclusiveAccessContextTest,
                       ReusesBubbleBeforeDeferredDestruction) {
  auto* context = BrowserViewExclusiveAccessContext::From(browser());
  ASSERT_TRUE(context);
  ExclusiveAccessBubbleParams params{
      .type = EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION};
  context->UpdateExclusiveAccessBubble(params, base::NullCallback());
  auto* bubble = context->exclusive_access_bubble();
  ASSERT_TRUE(bubble);

  // Closing is deferred so that callers on the bubble's stack remain valid.
  context->UpdateExclusiveAccessBubble(
      {.type = EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE}, base::NullCallback());
  ASSERT_EQ(bubble, context->exclusive_access_bubble());

  // A new notification before that task runs must cancel the pending deletion.
  params.has_download = true;
  context->UpdateExclusiveAccessBubble(params, base::NullCallback());

  // The destruction task was posted to this same runner. Wait for a task
  // queued after it before checking that cancellation preserved the bubble.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  ASSERT_EQ(bubble, context->exclusive_access_bubble());
  EXPECT_TRUE(context->exclusive_access_bubble()->params().has_download);
}

class BrowserViewExclusiveAccessContextShutdownTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {};

IN_PROC_BROWSER_TEST_P(BrowserViewExclusiveAccessContextShutdownTest,
                       CloseWindowWithBubble) {
  BrowserWindowInterface* window = CreateBrowserWindowForTesting(
      BrowserWindowCreateParams(GetProfile(), /*from_user_gesture=*/true));
  chrome::AddTabAt(window, GURL("about:blank"), -1, true);
  window->GetWindow()->Show();

  auto* context = BrowserViewExclusiveAccessContext::From(window);
  ASSERT_TRUE(context);
  auto hide_count = std::make_shared<int>(0);
  context->UpdateExclusiveAccessBubble(
      {.type = EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION},
      base::BindLambdaForTesting(
          [hide_count](ExclusiveAccessBubbleHideReason reason) {
            EXPECT_EQ(ExclusiveAccessBubbleHideReason::kInterrupted, reason);
            ++*hide_count;
          }));
  ASSERT_TRUE(context->exclusive_access_bubble());
  base::RunLoop bubble_deleted;
  views::ViewTracker bubble_view(context->exclusive_access_bubble()->GetView());
  // Post the quit task so any previously queued destruction task is processed
  // even if closing the window deletes the view synchronously.
  bubble_view.SetIsDeletingCallback(
      base::BindPostTask(base::SingleThreadTaskRunner::GetCurrentDefault(),
                         bubble_deleted.QuitClosure()));

  if (GetParam()) {
    context->UpdateExclusiveAccessBubble(
        {.type = EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE}, base::NullCallback());
    ASSERT_TRUE(context->exclusive_access_bubble());
  }

  // Destroy the widget synchronously, including its window features, then wait
  // for the bubble view to be deleted and its deletion callback to run.
  ui_test_utils::BrowserDestroyedObserver destroyed(window);
  BrowserView::GetBrowserViewForBrowser(window)->GetWidget()->CloseNow();
  destroyed.Wait();
  bubble_deleted.Run();
  EXPECT_FALSE(bubble_view.view());
  EXPECT_EQ(1, *hide_count);
}

INSTANTIATE_TEST_SUITE_P(All,
                         BrowserViewExclusiveAccessContextShutdownTest,
                         testing::Bool());
