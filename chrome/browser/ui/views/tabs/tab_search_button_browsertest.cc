// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_search_button.h"

#include <vector>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/common/webui_url_constants.h"
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

class TabSearchButtonBrowserTest : public InProcessBrowserTest {
 public:
  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  TabSearchButton* tab_search_button() {
    return browser_view()->GetTabSearchButton();
  }

  WebUIBubbleManager* bubble_manager() {
    return tab_search_button()->webui_bubble_manager_for_testing();
  }

  void RunUntilBubbleWidgetDestroyed() {
    ASSERT_NE(nullptr, bubble_manager()->GetBubbleWidget());
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop.QuitClosure());
    run_loop.Run();
    ASSERT_EQ(nullptr, bubble_manager()->GetBubbleWidget());
  }
};

IN_PROC_BROWSER_TEST_F(TabSearchButtonBrowserTest, ButtonClickCreatesBubble) {
  ASSERT_EQ(nullptr, bubble_manager()->GetBubbleWidget());
  views::test::ButtonTestApi(tab_search_button()).NotifyClick(GetDummyEvent());
  ASSERT_NE(nullptr, bubble_manager()->GetBubbleWidget());

  tab_search_button()->CloseTabSearchBubble();
  ASSERT_TRUE(bubble_manager()->GetBubbleWidget()->IsClosed());

  RunUntilBubbleWidgetDestroyed();
}

IN_PROC_BROWSER_TEST_F(TabSearchButtonBrowserTest,
                       BubbleShowTimerTriggersCorrectly) {
  ASSERT_EQ(nullptr, bubble_manager()->GetBubbleWidget());
  tab_search_button()->ShowTabSearchBubble();

  // |bubble_created_time_| should be set as soon as the bubble widget is
  // created.
  EXPECT_FALSE(bubble_manager()->GetBubbleWidget()->IsVisible());
  EXPECT_TRUE(tab_search_button()->bubble_created_time_for_testing());

  // Showing the bubble should reset the timestamp.
  bubble_manager()->bubble_view_for_testing()->ShowUI();
  EXPECT_TRUE(bubble_manager()->GetBubbleWidget()->IsVisible());
  EXPECT_FALSE(tab_search_button()->bubble_created_time_for_testing());

  tab_search_button()->CloseTabSearchBubble();
  RunUntilBubbleWidgetDestroyed();
}

// On macOS, most accelerators are handled by CommandDispatcher.
#if !defined(OS_MAC)
IN_PROC_BROWSER_TEST_F(TabSearchButtonBrowserTest,
                       KeyboardShortcutTriggersBubble) {
  ASSERT_EQ(nullptr, bubble_manager()->GetBubbleWidget());

  auto accelerator = ui::Accelerator(
      ui::VKEY_A, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR);
  browser_view()->AcceleratorPressed(accelerator);

  // Accelerator keys should have created the tab search bubble.
  ASSERT_NE(nullptr, bubble_manager()->GetBubbleWidget());

  tab_search_button()->CloseTabSearchBubble();
  ASSERT_TRUE(bubble_manager()->GetBubbleWidget()->IsClosed());

  RunUntilBubbleWidgetDestroyed();
}
#endif

class TabSearchButtonBrowserUITest : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    AppendTab(chrome::kChromeUISettingsURL);
    AppendTab(chrome::kChromeUIHistoryURL);
    AppendTab(chrome::kChromeUIBookmarksURL);
    auto* tab_search_button =
        BrowserView::GetBrowserViewForBrowser(browser())->GetTabSearchButton();
    views::test::ButtonTestApi(tab_search_button).NotifyClick(GetDummyEvent());
  }

  void AppendTab(std::string url) {
    chrome::AddTabAt(browser(), GURL(url), -1, true);
  }
};

// Invokes a tab search bubble.
IN_PROC_BROWSER_TEST_F(TabSearchButtonBrowserUITest, InvokeUi_default) {
  ShowAndVerifyUi();
}
