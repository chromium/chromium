// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_search_button.h"

#include <vector>

#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/views/test/button_test_api.h"

namespace {
ui::MouseEvent GetDummyEvent() {
  return ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::PointF(), gfx::PointF(),
                        base::TimeTicks::Now(), 0, 0);
}
}  // namespace

class TabSearchButtonBrowserTest : public InProcessBrowserTest,
                                   public ::testing::WithParamInterface<bool> {
 public:
  // InProcessBrowserTest:
  void SetUp() override {
    // Run the test with both kTabSearchFixedEntrypoint enabled and disabled.
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kTabSearch,
                                features::kTabSearchFixedEntrypoint},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kTabSearch},
          /*disabled_features=*/{features::kTabSearchFixedEntrypoint});
    }
    InProcessBrowserTest::SetUp();
  }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  TabSearchButton* tab_search_button() {
    return browser_view()->GetTabSearchButton();
  }

  void RunUntilBubbleWidgetDestroyed() {
    ASSERT_NE(nullptr, tab_search_button()->bubble_for_testing());
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop.QuitClosure());
    run_loop.Run();
    ASSERT_EQ(nullptr, tab_search_button()->bubble_for_testing());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(TabSearchButtonBrowserTest, CreateAndClose) {
  ASSERT_EQ(nullptr, tab_search_button()->bubble_for_testing());
  views::test::ButtonTestApi(tab_search_button()).NotifyClick(GetDummyEvent());
  ASSERT_NE(nullptr, tab_search_button()->bubble_for_testing());

  tab_search_button()->bubble_for_testing()->CloseWithReason(
      views::Widget::ClosedReason::kUnspecified);
  ASSERT_TRUE(tab_search_button()->bubble_for_testing()->IsClosed());

  RunUntilBubbleWidgetDestroyed();
}

IN_PROC_BROWSER_TEST_P(TabSearchButtonBrowserTest, TestBubbleVisible) {
  EXPECT_FALSE(tab_search_button()->IsBubbleVisible());

  ASSERT_EQ(nullptr, tab_search_button()->bubble_for_testing());
  views::test::ButtonTestApi(tab_search_button()).NotifyClick(GetDummyEvent());
  ASSERT_NE(nullptr, tab_search_button()->bubble_for_testing());

  // The bubble should not be visible initially since the UI must notify the
  // bubble it is ready before the bubble is shown.
  EXPECT_FALSE(tab_search_button()->IsBubbleVisible());

  // Trigger showing the bubble.
  tab_search_button()->bubble_for_testing()->Show();

  // The bubble should be visible after being shown.
  EXPECT_TRUE(tab_search_button()->IsBubbleVisible());

  tab_search_button()->bubble_for_testing()->CloseWithReason(
      views::Widget::ClosedReason::kUnspecified);
  ASSERT_TRUE(tab_search_button()->bubble_for_testing()->IsClosed());

  RunUntilBubbleWidgetDestroyed();
}

IN_PROC_BROWSER_TEST_P(TabSearchButtonBrowserTest, BubbleNotVisibleIncognito) {
  Browser* incognito_browser = CreateIncognitoBrowser();
  BrowserView* incognito_browser_view =
      BrowserView::GetBrowserViewForBrowser(incognito_browser);

  // The Tab Search button should not be available on incognito browsers.
  EXPECT_EQ(nullptr, incognito_browser_view->GetTabSearchButton());
}

// On macOS, most accelerators are handled by CommandDispatcher.
#if !defined(OS_MAC)
IN_PROC_BROWSER_TEST_P(TabSearchButtonBrowserTest, TestBubbleKeyboardShortcut) {
  ASSERT_EQ(nullptr, tab_search_button()->bubble_for_testing());

  auto accelerator = ui::Accelerator(
      ui::VKEY_A, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR);
  browser_view()->AcceleratorPressed(accelerator);

  // Accelerator keys should have created the tab search bubble.
  ASSERT_NE(nullptr, tab_search_button()->bubble_for_testing());

  tab_search_button()->bubble_for_testing()->CloseWithReason(
      views::Widget::ClosedReason::kUnspecified);
  ASSERT_TRUE(tab_search_button()->bubble_for_testing()->IsClosed());

  RunUntilBubbleWidgetDestroyed();
}
#endif

INSTANTIATE_TEST_SUITE_P(All,
                         TabSearchButtonBrowserTest,
                         ::testing::Values(true, false));
