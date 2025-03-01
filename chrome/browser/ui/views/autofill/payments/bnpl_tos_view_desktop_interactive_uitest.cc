// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/payments_view_factory.h"
#include "chrome/browser/ui/views/autofill/payments/bnpl_tos_view_desktop.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_controller_impl.h"
#include "content/public/test/browser_test.h"
#include "ui/views/window/dialog_client_view.h"

namespace autofill {

namespace {
constexpr char kSuppressedScreenshotError[] =
    "Screenshot can only run in pixel_tests.";
}  // namespace

class BnplTosViewDesktopInteractiveUiTest : public InteractiveBrowserTest {
 public:
  BnplTosViewDesktopInteractiveUiTest() = default;
  BnplTosViewDesktopInteractiveUiTest(
      const BnplTosViewDesktopInteractiveUiTest&) = delete;
  BnplTosViewDesktopInteractiveUiTest& operator=(
      const BnplTosViewDesktopInteractiveUiTest&) = delete;
  ~BnplTosViewDesktopInteractiveUiTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    controller_ = std::make_unique<BnplTosControllerImpl>();
  }

  void TearDownOnMainThread() override {
    controller_.reset();
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  InteractiveBrowserTestApi::MultiStep InvokeUiAndWaitForShow() {
    return Steps(
        Do([this]() {
          controller_->Show(base::BindOnce(&CreateAndShowBnplTos,
                                           controller_->GetWeakPtr(),
                                           base::Unretained(web_contents())));
        }),
        InAnyContext(WaitForShow(views::DialogClientView::kTopViewId)));
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  std::unique_ptr<BnplTosControllerImpl> controller_;
};

IN_PROC_BROWSER_TEST_F(BnplTosViewDesktopInteractiveUiTest, InvokeUi) {
  RunTestSequence(InvokeUiAndWaitForShow(),
                  InAnyContext(SetOnIncompatibleAction(
                                   OnIncompatibleAction::kIgnoreAndContinue,
                                   kSuppressedScreenshotError),
                               Screenshot(views::DialogClientView::kTopViewId,
                                          /*screenshot_name=*/"bnpl_tos",
                                          /*baseline_cl=*/"6245071")));
}

IN_PROC_BROWSER_TEST_F(BnplTosViewDesktopInteractiveUiTest, DialogAccepted) {
  RunTestSequence(
      InvokeUiAndWaitForShow(),
      InAnyContext(PressButton(views::DialogClientView::kOkButtonElementId),
                   WaitForHide(views::DialogClientView::kTopViewId)));
}

IN_PROC_BROWSER_TEST_F(BnplTosViewDesktopInteractiveUiTest, DialogDeclined) {
  RunTestSequence(
      InvokeUiAndWaitForShow(),
      InAnyContext(PressButton(views::DialogClientView::kCancelButtonElementId),
                   WaitForHide(views::DialogClientView::kTopViewId)));
}

}  // namespace autofill
