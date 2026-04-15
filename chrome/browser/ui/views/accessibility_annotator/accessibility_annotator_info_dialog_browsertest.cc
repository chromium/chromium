// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/accessibility_annotator/accessibility_annotator_info_dialog_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace accessibility_annotator::info {

class AccessibilityAnnotatorInfoDialogBrowserTest
    : public InProcessBrowserTest {
 public:
  AccessibilityAnnotatorInfoDialogBrowserTest() = default;
  ~AccessibilityAnnotatorInfoDialogBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(AccessibilityAnnotatorInfoDialogBrowserTest,
                       InvokeUi_default) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = browser()->profile();

  auto controller =
      std::make_unique<AccessibilityAnnotatorInfoDialogController>(profile);

  content::TestNavigationObserver navigation_observer(
      GURL("chrome://accessibility-annotator-info/"));
  navigation_observer.StartWatchingNewWebContents();

  controller->ShowDialog(web_contents, base::DoNothing());

  views::Widget* widget = controller->GetWidgetForTesting();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  navigation_observer.Wait();

  controller->CloseDialog();
}

IN_PROC_BROWSER_TEST_F(AccessibilityAnnotatorInfoDialogBrowserTest,
                       ClickOutsideDismissesDialog) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = browser()->profile();

  auto controller =
      std::make_unique<AccessibilityAnnotatorInfoDialogController>(profile);

  content::TestNavigationObserver navigation_observer(
      GURL("chrome://accessibility-annotator-info/"));
  navigation_observer.StartWatchingNewWebContents();

  controller->ShowDialog(web_contents, base::DoNothing());

  views::Widget* widget = controller->GetWidgetForTesting();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  navigation_observer.Wait();

  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);

  gfx::NativeWindow root_window = views::GetRootWindow(
      BrowserView::GetBrowserViewForBrowser(browser())->GetWidget());
  ui::test::EventGenerator event_generator(root_window);
  gfx::Point click_point =
      widget->GetWindowBoundsInScreen().origin() - gfx::Vector2d(1, 1);
  event_generator.MoveMouseTo(click_point);
  event_generator.ClickLeftButton();

  destroyed_waiter.Wait();

  EXPECT_FALSE(controller->GetWidgetForTesting());
}

}  // namespace accessibility_annotator::info
