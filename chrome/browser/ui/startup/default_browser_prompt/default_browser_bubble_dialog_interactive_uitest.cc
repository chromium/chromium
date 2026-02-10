// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_bubble_dialog.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace {

// Baseline Gerrit CL number of the most recent CL that modified the UI.
constexpr char kScreenshotBaselineCL[] = "7475474";

}  // namespace

class DefaultBrowserBubbleDialogInteractiveTest
    : public InteractiveBrowserTest {
 public:
  void ShowUi() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    views::View* anchor_view =
        browser_view->toolbar_button_provider()->GetAppMenuButton();
    dialog_widget_ = default_browser::ShowDefaultBrowserBubbleDialog(
        anchor_view, on_accept_.GetCallback(), on_dismiss_.GetCallback());
  }

  void TearDownOnMainThread() override { dialog_widget_.reset(); }

  base::test::TestFuture<void> on_accept_;
  base::test::TestFuture<void> on_dismiss_;

 private:
  std::unique_ptr<views::Widget> dialog_widget_;
};

IN_PROC_BROWSER_TEST_F(DefaultBrowserBubbleDialogInteractiveTest, ShowDialog) {
  RunTestSequence(
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshots not supported in all testing environments."),
      Do([this]() { ShowUi(); }),
      WaitForShow(default_browser::kBubbleDialogOpenSettingsButtonId),
      WaitForShow(default_browser::kBubbleDialogSetLaterButtonId),
      WaitForShow(default_browser::kBubbleDialogId),
      ScreenshotSurface(default_browser::kBubbleDialogId,
                        /*screenshot_name=*/"DefaultBrowserBubbleDialog",
                        kScreenshotBaselineCL));
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserBubbleDialogInteractiveTest,
                       ClickSetDefault) {
  RunTestSequence(
      Do([this]() { ShowUi(); }),
      WaitForShow(default_browser::kBubbleDialogOpenSettingsButtonId),
      PressButton(default_browser::kBubbleDialogOpenSettingsButtonId),
      Check([this]() { return on_accept_.Wait(); }, "Accept callback run"));
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserBubbleDialogInteractiveTest,
                       ClickSetLater) {
  RunTestSequence(
      Do([this]() { ShowUi(); }),
      WaitForShow(default_browser::kBubbleDialogSetLaterButtonId),
      PressButton(default_browser::kBubbleDialogSetLaterButtonId),
      Check([this]() { return on_dismiss_.Wait(); }, "Dismiss callback run"));
}
