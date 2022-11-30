// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_selection_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_selection_dialog_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/autofill/payments/virtual_card_selection_dialog_view_impl.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/views/controls/button/label_button.h"

namespace autofill {

namespace {
constexpr char kOneCardTest[] = "OneCard";
constexpr char kTwoCardsTest[] = "TwoCards";
}  // namespace

class VirtualCardSelectionDialogBrowserTest : public DialogBrowserTest {
 public:
  VirtualCardSelectionDialogBrowserTest() = default;

  VirtualCardSelectionDialogBrowserTest(
      const VirtualCardSelectionDialogBrowserTest&) = delete;
  VirtualCardSelectionDialogBrowserTest& operator=(
      const VirtualCardSelectionDialogBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Do lazy initialization of VirtualCardSelectionDialogControllerImpl.
    VirtualCardSelectionDialogControllerImpl::CreateForWebContents(
        web_contents);

    CreditCard card1 = test::GetFullServerCard();
    if (name == kOneCardTest) {
      controller()->ShowDialog({&card1}, base::DoNothing());
    } else if (name == kTwoCardsTest) {
      CreditCard card2 = test::GetFullServerCard();
      controller()->ShowDialog({&card1, &card2}, base::DoNothing());
    }
  }

  VirtualCardSelectionDialogViewImpl* GetDialog() {
    if (!controller())
      return nullptr;

    VirtualCardSelectionDialogView* dialog_view = controller()->dialog_view();
    if (!dialog_view)
      return nullptr;

    return static_cast<VirtualCardSelectionDialogViewImpl*>(dialog_view);
  }

  VirtualCardSelectionDialogControllerImpl* controller() {
    if (!browser() || !browser()->tab_strip_model() ||
        !browser()->tab_strip_model()->GetActiveWebContents())
      return nullptr;

    return VirtualCardSelectionDialogControllerImpl::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }
};

IN_PROC_BROWSER_TEST_F(VirtualCardSelectionDialogBrowserTest,
                       InvokeUi_OneCard) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(VirtualCardSelectionDialogBrowserTest,
                       CanCloseTabWhileDialogShowing) {
  ShowUi(kOneCardTest);
  VerifyUi();
  browser()->tab_strip_model()->GetActiveWebContents()->Close();
  base::RunLoop().RunUntilIdle();
}

// Ensures closing browser while dialog being visible is correctly handled.
IN_PROC_BROWSER_TEST_F(VirtualCardSelectionDialogBrowserTest,
                       CanCloseBrowserWhileDialogShowing) {
  ShowUi(kOneCardTest);
  VerifyUi();
  browser()->window()->Close();
  base::RunLoop().RunUntilIdle();
}

// Ensures dialog is closed when ok button is clicked when there is one card
// available.
IN_PROC_BROWSER_TEST_F(VirtualCardSelectionDialogBrowserTest,
                       ClickOkButton_OneCard) {
  ShowUi(kOneCardTest);
  VerifyUi();
  ASSERT_TRUE(GetDialog()->GetOkButton()->GetEnabled());
  GetDialog()->AcceptDialog();
  base::RunLoop().RunUntilIdle();
}

// TODO(crbug.com/1020740): Add browser test for OK button when there are two
// cards. The logic to update button state will be implemented in the CL adding
// card list in the dialog.

// Ensures dialog is closed when cancel button is clicked.
IN_PROC_BROWSER_TEST_F(VirtualCardSelectionDialogBrowserTest,
                       ClickCancelButton) {
  ShowUi(kOneCardTest);
  VerifyUi();
  GetDialog()->CancelDialog();
  base::RunLoop().RunUntilIdle();
}

// TODO(crbug.com/1020740): Add more browsertests for interactions.

}  // namespace autofill
