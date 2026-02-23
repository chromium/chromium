// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/default_browser/test_support/fake_default_browser_setter.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_bubble_dialog.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_bubble_dialog_manager.h"
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
constexpr char kScreenshotNoPinningBaselineCL[] = "7475474";
constexpr char kScreenshotWithPinningBaselineCL[] = "7597208";

}  // namespace

class DefaultBrowserBubbleDialogInteractiveTest
    : public InteractiveBrowserTest {
 public:
  void ShowUi(bool can_pin_to_taskbar = false) {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    views::View* anchor_view =
        browser_view->toolbar_button_provider()->GetAppMenuButton();
    dialog_widget_ = default_browser::ShowDefaultBrowserBubbleDialog(
        anchor_view, can_pin_to_taskbar, on_accept_.GetCallback(),
        on_dismiss_.GetCallback());
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
      Do([this]() { ShowUi(/*can_pin_to_taskbar=*/false); }),
      WaitForShow(default_browser::kBubbleDialogOpenSettingsButtonId),
      WaitForShow(default_browser::kBubbleDialogSetLaterButtonId),
      WaitForShow(default_browser::kBubbleDialogId),
      ScreenshotSurface(default_browser::kBubbleDialogId,
                        /*screenshot_name=*/"DefaultBrowserBubbleDialog",
                        kScreenshotNoPinningBaselineCL));
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserBubbleDialogInteractiveTest,
                       ShowDialogWithPinning) {
  RunTestSequence(
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshots not supported in all testing environments."),
      Do([this]() { ShowUi(/*can_pin_to_taskbar=*/true); }),
      WaitForShow(default_browser::kBubbleDialogOpenSettingsButtonId),
      WaitForShow(default_browser::kBubbleDialogSetLaterButtonId),
      WaitForShow(default_browser::kBubbleDialogId),
      ScreenshotSurface(default_browser::kBubbleDialogId,
                        /*screenshot_name=*/"DefaultBrowserBubbleDialog",
                        kScreenshotWithPinningBaselineCL));
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

class DefaultBrowserDialogManagerInteractiveTest
    : public InteractiveBrowserTest {
 protected:
  void ShowDialogManager() {
    manager_ = std::make_unique<DefaultBrowserBubbleDialogManager>();
    manager_->Show(
        std::make_unique<default_browser::DefaultBrowserController>(
            std::make_unique<default_browser::FakeDefaultBrowserSetter>(),
            default_browser::DefaultBrowserEntrypointType::kBubbleDialog),
        /*can_pin_to_taskbar=*/false);
  }

  void CloseDialogs() { manager_->CloseAll(); }

  void TearDownOnMainThread() override {
    manager_.reset();
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  MultiStep VerifyHistogram(std::string histogram_name, int bucket, int count) {
    MultiStep steps;

    steps += Do([this, histogram_name, bucket, count]() {
      histogram_tester_.ExpectBucketCount(histogram_name, bucket, count);
    });
    return steps;
  }

  std::unique_ptr<DefaultBrowserBubbleDialogManager> manager_;
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(DefaultBrowserDialogManagerInteractiveTest,
                       ShowAndDismiss) {
  RunTestSequence(
      Do([this]() { ShowDialogManager(); }),
      WaitForShow(default_browser::kBubbleDialogSetLaterButtonId),
      VerifyHistogram("DefaultBrowser.BubbleDialog.ShellIntegration.Shown", 1,
                      1),
      PressButton(default_browser::kBubbleDialogSetLaterButtonId),
      VerifyHistogram(
          "DefaultBrowser.BubbleDialog.ShellIntegration.Interaction",
          std::to_underlying(
              default_browser::DefaultBrowserInteractionType::kDismissed),
          1),
      Do([this]() { CloseDialogs(); }),
      WaitForHide(default_browser::kBubbleDialogId));
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserDialogManagerInteractiveTest,
                       ShowAndAccept) {
  RunTestSequence(
      Do([this]() { ShowDialogManager(); }),
      WaitForShow(default_browser::kBubbleDialogOpenSettingsButtonId),
      VerifyHistogram("DefaultBrowser.BubbleDialog.ShellIntegration.Shown", 1,
                      1),
      PressButton(default_browser::kBubbleDialogOpenSettingsButtonId),
      VerifyHistogram(
          "DefaultBrowser.BubbleDialog.ShellIntegration.Interaction",
          std::to_underlying(
              default_browser::DefaultBrowserInteractionType::kAccepted),
          1),
      Do([this]() { CloseDialogs(); }),
      WaitForHide(default_browser::kBubbleDialogId));
}
