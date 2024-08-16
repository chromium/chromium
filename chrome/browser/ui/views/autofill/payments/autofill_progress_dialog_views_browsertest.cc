// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client.h"
#include "chrome/browser/ui/autofill/payments/view_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/autofill/payments/autofill_progress_dialog_views.h"
#include "components/autofill/core/browser/autofill_progress_dialog_type.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/payments/autofill_progress_dialog_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/autofill_progress_dialog_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/views/test/widget_test.h"

namespace autofill {

class AutofillProgressDialogViewsBrowserTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<std::string> {
 public:
  AutofillProgressDialogViewsBrowserTest() = default;
  ~AutofillProgressDialogViewsBrowserTest() override = default;
  AutofillProgressDialogViewsBrowserTest(
      const AutofillProgressDialogViewsBrowserTest&) = delete;
  AutofillProgressDialogViewsBrowserTest& operator=(
      const AutofillProgressDialogViewsBrowserTest&) = delete;

  AutofillProgressDialogType GetDialogType() const {
    if (GetParam() == "VirtualCardUnmask") {
      return AutofillProgressDialogType::kVirtualCardUnmaskProgressDialog;
    } else if (GetParam() == "ServerCardUnmask") {
      return AutofillProgressDialogType::kServerCardUnmaskProgressDialog;
    } else if (GetParam() == "3dsFetchVirtualCard") {
      return AutofillProgressDialogType::k3dsFetchVcnProgressDialog;
    }
    NOTREACHED();
  }

  std::string GetDialogTypeStringForLogging() const {
    return std::string(
        AutofillMetrics::GetDialogTypeStringForLogging(GetDialogType()));
  }

  void ShowUi(const std::string& name) override {
    client()->ShowAutofillProgressDialog(GetDialogType(), base::DoNothing());
  }

  AutofillProgressDialogViews* GetDialogViews() {
    DCHECK(controller());

    base::WeakPtr<AutofillProgressDialogView> dialog_view =
        controller()->autofill_progress_dialog_view();
    if (!dialog_view)
      return nullptr;

    return static_cast<AutofillProgressDialogViews*>(dialog_view.get());
  }

  AutofillProgressDialogControllerImpl* controller() const {
    return client()->AutofillProgressDialogControllerForTesting();
  }

  payments::ChromePaymentsAutofillClient* client() const {
    auto* client =
        ChromeAutofillClient::FromWebContentsForTesting(web_contents());
    // On Desktop and Clank, the PaymentsAutofillClient can only be a
    // ChromePaymentsAutofillClient.
    return static_cast<payments::ChromePaymentsAutofillClient*>(
        client->GetPaymentsAutofillClient());
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

IN_PROC_BROWSER_TEST_P(AutofillProgressDialogViewsBrowserTest,
                       InvokeUi_VirtualCardUnmask) {
  base::HistogramTester histogram_tester;
  ShowAndVerifyUi();
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.ProgressDialog.", GetDialogTypeStringForLogging(),
                    ".Shown"}),
      true, 1);
}

// Ensures closing current tab while dialog being visible is correctly handle
// and the browser won't crash.
IN_PROC_BROWSER_TEST_P(AutofillProgressDialogViewsBrowserTest,
                       CloseTabWhileDialogShowing) {
  base::HistogramTester histogram_tester;
  ShowUi(GetDialogTypeStringForLogging());
  VerifyUi();
  web_contents()->Close();
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.ProgressDialog.", GetDialogTypeStringForLogging(),
                    ".Shown"}),
      true, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.ProgressDialog.", GetDialogTypeStringForLogging(),
                    ".Result"}),
      true, 1);
}

// Ensures closing browser while dialog being visible is correctly handled and
// the browser won't crash.
IN_PROC_BROWSER_TEST_P(AutofillProgressDialogViewsBrowserTest,
                       CloseBrowserWhileDialogShowing) {
  base::HistogramTester histogram_tester;
  ShowUi(GetDialogTypeStringForLogging());
  VerifyUi();
  browser()->window()->Close();
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.ProgressDialog.", GetDialogTypeStringForLogging(),
                    ".Shown"}),
      true, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.ProgressDialog.", GetDialogTypeStringForLogging(),
                    ".Result"}),
      true, 1);
}

// Ensures clicking on the cancel button is correctly handled.
IN_PROC_BROWSER_TEST_P(AutofillProgressDialogViewsBrowserTest,
                       ClickCancelButton) {
  base::HistogramTester histogram_tester;
  ShowUi(GetDialogTypeStringForLogging());
  VerifyUi();
  auto* dialog_views = GetDialogViews();
  ASSERT_TRUE(dialog_views);
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      dialog_views->GetWidget());
  GetDialogViews()->CancelDialog();
  destroyed_waiter.Wait();
  EXPECT_FALSE(GetDialogViews());
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.ProgressDialog.", GetDialogTypeStringForLogging(),
                    ".Shown"}),
      true, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.ProgressDialog.", GetDialogTypeStringForLogging(),
                    ".Result"}),
      true, 1);
}

// Ensures the dialog closing with confirmation works properly.
IN_PROC_BROWSER_TEST_P(AutofillProgressDialogViewsBrowserTest,
                       CloseDialogWithConfirmation) {
  base::HistogramTester histogram_tester;
  ShowUi(GetDialogTypeStringForLogging());
  VerifyUi();
  auto* dialog_views = GetDialogViews();
  ASSERT_TRUE(dialog_views);
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      dialog_views->GetWidget());
  base::MockOnceClosure no_interactive_authentication_callback;
  EXPECT_CALL(no_interactive_authentication_callback, Run).Times(1);
  controller()->DismissDialog(
      /*show_confirmation_before_closing=*/true,
      /*no_interactive_authentication_callback=*/
      no_interactive_authentication_callback.Get());
  destroyed_waiter.Wait();
  EXPECT_FALSE(GetDialogViews());
  testing::Mock::VerifyAndClearExpectations(
      &no_interactive_authentication_callback);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.ProgressDialog.", GetDialogTypeStringForLogging(),
                    ".Shown"}),
      true, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.ProgressDialog.", GetDialogTypeStringForLogging(),
                    ".Result"}),
      false, 1);
}

INSTANTIATE_TEST_SUITE_P(,
                         AutofillProgressDialogViewsBrowserTest,
                         testing::Values("VirtualCardUnmask",
                                         "ServerCardUnmask",
                                         "3dsFetchVirtualCard"));

}  // namespace autofill
