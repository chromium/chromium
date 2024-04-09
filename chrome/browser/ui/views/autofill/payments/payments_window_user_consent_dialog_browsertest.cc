// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/autofill/payments/view_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/autofill/payments/payments_window_user_consent_dialog_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/autofill/core/browser/ui/payments/payments_window_user_consent_dialog_controller_impl.h"
#include "content/public/test/browser_test.h"
#include "ui/views/window/dialog_client_view.h"

namespace autofill::payments {

constexpr char kSuppressedScreenshotError[] =
    "Screenshot can only run in pixel_tests.";

class PaymentsWindowUserConsentDialogBrowserTest
    : public InteractiveBrowserTest {
 public:
  PaymentsWindowUserConsentDialogBrowserTest() {
    controller_ =
        std::make_unique<PaymentsWindowUserConsentDialogControllerImpl>(
            accept_callback_.Get(), cancel_callback_.Get());
  }
  PaymentsWindowUserConsentDialogBrowserTest(
      const PaymentsWindowUserConsentDialogBrowserTest&) = delete;
  PaymentsWindowUserConsentDialogBrowserTest& operator=(
      const PaymentsWindowUserConsentDialogBrowserTest&) = delete;
  ~PaymentsWindowUserConsentDialogBrowserTest() override = default;

 protected:
  InteractiveBrowserTestApi::MultiStep TriggerDialogAndWaitForShow(
      ElementSpecifier element_specifier) {
    return Steps(
        TriggerDialog(),
        // b/332603033: Tab-modal dialogs on MacOS run in a different context.
        InAnyContext(WaitForShow(element_specifier)));
  }

  base::MockOnceClosure accept_callback_;
  base::MockOnceClosure cancel_callback_;
  std::unique_ptr<PaymentsWindowUserConsentDialogControllerImpl> controller_;

 private:
  InteractiveTestApi::StepBuilder TriggerDialog() {
    return Do([this]() {
      controller_->ShowDialog(base::BindOnce(
          &CreateAndShowPaymentsWindowUserConsentDialog,
          controller_->GetWeakPtr(),
          // The callback is run instantly, so `base::Unretained()` is safe here
          // as `browser()->tab_strip_model()->GetActiveWebContents()` will
          // always be present when the callback is run.
          base::Unretained(
              browser()->tab_strip_model()->GetActiveWebContents())));
    });
  }
};

// Ensures the UI can be shown, and verifies that it looks as expected.
IN_PROC_BROWSER_TEST_F(PaymentsWindowUserConsentDialogBrowserTest,
                       InvokeUi_PaymentsWindowUserConsentDialogDisplays) {
  const std::string payments_user_consent_dialog_root_view =
      "Payments User Consent Dialog Root View";
  RunTestSequence(
      TriggerDialogAndWaitForShow(
          PaymentsWindowUserConsentDialogView::kTopViewId),
      // TriggerDialogAndWaitForShow() changes the context, so the same context
      // must be used.
      InSameContext(Steps(
          NameViewRelative(PaymentsWindowUserConsentDialogView::kTopViewId,
                           payments_user_consent_dialog_root_view,
                           [](views::View* dialog_view) {
                             return dialog_view->GetWidget()->GetRootView();
                           }),
          SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                                  kSuppressedScreenshotError),
          Screenshot(payments_user_consent_dialog_root_view,
                     /*screenshot_name=*/"consent_popup",
                     /*baseline_cl=*/"5338589"))));
}

// Ensures the UI can be shown, and verifies that accepting the dialog runs the
// accept callback and hides the view.
IN_PROC_BROWSER_TEST_F(PaymentsWindowUserConsentDialogBrowserTest,
                       InvokeUi_DialogAcceptance) {
  EXPECT_CALL(accept_callback_, Run);

  RunTestSequence(
      TriggerDialogAndWaitForShow(views::DialogClientView::kOkButtonElementId),
      // TriggerDialogAndWaitForShow() changes the context, so the same context
      // must be used.
      InSameContext(
          Steps(PressButton(views::DialogClientView::kOkButtonElementId),
                WaitForHide(PaymentsWindowUserConsentDialogView::kTopViewId))));
}

// Ensures the UI can be shown, and verifies that cancelling the dialog runs the
// cancel callback and hides the view.
IN_PROC_BROWSER_TEST_F(PaymentsWindowUserConsentDialogBrowserTest,
                       InvokeUi_DialogCancelled) {
  EXPECT_CALL(cancel_callback_, Run);

  RunTestSequence(
      TriggerDialogAndWaitForShow(
          views::DialogClientView::kCancelButtonElementId),
      // TriggerDialogAndWaitForShow() changes the context, so the same context
      // must be used.
      InSameContext(
          Steps(PressButton(views::DialogClientView::kCancelButtonElementId),
                WaitForHide(PaymentsWindowUserConsentDialogView::kTopViewId))));
}

}  // namespace autofill::payments
