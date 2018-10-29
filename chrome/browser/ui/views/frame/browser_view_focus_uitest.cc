// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "url/gurl.h"

const char kSimplePage[] = "/focus/page_with_focus.html";

class BrowserViewFocusTest : public InProcessBrowserTest {
 public:
  BrowserViewFocusTest() = default;
  ~BrowserViewFocusTest() override = default;

  bool IsViewFocused(ViewID vid) {
    return ui_test_utils::IsViewFocused(browser(), vid);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserViewFocusTest);
};

IN_PROC_BROWSER_TEST_F(BrowserViewFocusTest, BrowsersRememberFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(embedded_test_server()->Start());

  // First we navigate to our test page.
  GURL url = embedded_test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  gfx::NativeWindow window = browser()->window()->GetNativeWindow();

  // The focus should be on the Tab contents.
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));
  // Now hide the window, show it again, the focus should not have changed.
  ui_test_utils::HideNativeWindow(window);
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(window));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_TAB_CONTAINER));

  chrome::FocusLocationBar(browser());
  ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));
  // Hide the window, show it again, the focus should not have changed.
  ui_test_utils::HideNativeWindow(window);
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(window));
  ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));

  // The rest of this test does not make sense on Linux because the behavior
  // of Activate() is not well defined and can vary by window manager.
#if defined(OS_WIN)
  // Open a new browser window.
  Browser* browser2 =
      new Browser(Browser::CreateParams(browser()->profile(), true));
  ASSERT_TRUE(browser2);
  chrome::AddTabAt(browser2, GURL(), -1, true);
  browser2->window()->Show();
  ui_test_utils::NavigateToURL(browser2, url);

  gfx::NativeWindow window2 = browser2->window()->GetNativeWindow();
  BrowserView* browser_view2 = BrowserView::GetBrowserViewForBrowser(browser2);
  ASSERT_TRUE(browser_view2);
  const views::Widget* widget2 =
      views::Widget::GetWidgetForNativeWindow(window2);
  ASSERT_TRUE(widget2);
  const views::FocusManager* focus_manager2 = widget2->GetFocusManager();
  ASSERT_TRUE(focus_manager2);
  EXPECT_EQ(browser_view2->contents_web_view(),
            focus_manager2->GetFocusedView());

  // Switch to the 1st browser window, focus should still be on the location
  // bar and the second browser should have nothing focused.
  browser()->window()->Activate();
  ASSERT_TRUE(IsViewFocused(VIEW_ID_OMNIBOX));
  EXPECT_EQ(nullptr, focus_manager2->GetFocusedView());

  // Switch back to the second browser, focus should still be on the page.
  browser2->window()->Activate();
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  ASSERT_TRUE(widget);
  EXPECT_EQ(nullptr, widget->GetFocusManager()->GetFocusedView());
  EXPECT_EQ(browser_view2->contents_web_view(),
            focus_manager2->GetFocusedView());

  // Close the 2nd browser to avoid a DCHECK().
  browser_view2->Close();
#endif
}

// Helper class that tracks view classes receiving focus.
class FocusedViewClassRecorder : public views::FocusChangeListener {
 public:
  explicit FocusedViewClassRecorder(views::FocusManager* focus_manager)
      : focus_manager_(focus_manager) {
    focus_manager_->AddFocusChangeListener(this);
  }

  ~FocusedViewClassRecorder() override {
    focus_manager_->RemoveFocusChangeListener(this);
  }

  std::vector<std::string>& GetFocusClasses() { return focus_classes_; }

 private:
  // Inherited from views::FocusChangeListener
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override {}
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override {
    std::string class_name;
    if (focused_now)
      class_name = focused_now->GetClassName();
    focus_classes_.push_back(class_name);
  }

  views::FocusManager* focus_manager_;
  std::vector<std::string> focus_classes_;

  DISALLOW_COPY_AND_ASSIGN(FocusedViewClassRecorder);
};

// Switching tabs does not focus views unexpectedly.
// (bug http://crbug.com/791757, bug http://crbug.com/777051)
IN_PROC_BROWSER_TEST_F(BrowserViewFocusTest, TabChangesAvoidSpuriousFocus) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(embedded_test_server()->Start());

  // First we navigate to our test page.
  GURL url = embedded_test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Create another tab.
  AddTabAtIndex(1, url, ui::PAGE_TRANSITION_TYPED);

  // Begin recording focus changes.
  gfx::NativeWindow window = browser()->window()->GetNativeWindow();
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  views::FocusManager* focus_manager = widget->GetFocusManager();
  FocusedViewClassRecorder focus_change_recorder(focus_manager);

  // Switch tabs using ctrl+tab.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, true,
                                              false, false, false));

  std::vector<std::string>& focused_classes =
      focus_change_recorder.GetFocusClasses();

  // Everything before the last focus must be either "" (nothing) or a WebView.
  for (size_t index = 0; index < focused_classes.size() - 1; index++) {
    EXPECT_THAT(focused_classes[index],
                testing::AnyOf("", views::WebView::kViewClassName));
  }

  // Must end up focused on a WebView.
  ASSERT_FALSE(focused_classes.empty());
  EXPECT_EQ(views::WebView::kViewClassName, focused_classes.back());
}
