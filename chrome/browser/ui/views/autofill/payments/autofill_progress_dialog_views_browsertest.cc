// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/autofill/payments/autofill_progress_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/autofill_progress_dialog_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/autofill/payments/autofill_progress_dialog_views.h"
#include "components/autofill/core/browser/autofill_progress_dialog_type.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "content/public/test/browser_test.h"
#include "ui/views/test/widget_test.h"

namespace autofill {

class AutofillProgressDialogViewsBrowserTest : public DialogBrowserTest {
 public:
  AutofillProgressDialogViewsBrowserTest() = default;
  ~AutofillProgressDialogViewsBrowserTest() override = default;
  AutofillProgressDialogViewsBrowserTest(
      const AutofillProgressDialogViewsBrowserTest&) = delete;
  AutofillProgressDialogViewsBrowserTest& operator=(
      const AutofillProgressDialogViewsBrowserTest&) = delete;

  // DialogBrowserTest:
  void SetUpOnMainThread() override {
    controller_ = std::make_unique<AutofillProgressDialogControllerImpl>(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  void TearDownOnMainThread() override {
    // Reset the controller explicitly to avoid that its raw pointer to the
    // `WebContents` becomes dangling. This mirrors the behavior in production
    // code in which `ChromeAutofillClient` owns the controller and is destroyed
    // prior to the destruction of the respective `WebContents`.
    controller_.reset();

    DialogBrowserTest::TearDownOnMainThread();
  }

  void ShowUi(const std::string& name) override {
    AutofillProgressDialogType autofill_progress_dialog_type_;
    CHECK_EQ(name, "VirtualCardUnmask");
    autofill_progress_dialog_type_ =
        AutofillProgressDialogType::kVirtualCardUnmaskProgressDialog;
    controller()->ShowDialog(autofill_progress_dialog_type_, base::DoNothing());
  }

  AutofillProgressDialogViews* GetDialogViews() {
    DCHECK(controller());

    AutofillProgressDialogView* dialog_view =
        controller()->autofill_progress_dialog_view();
    if (!dialog_view)
      return nullptr;

    return static_cast<AutofillProgressDialogViews*>(dialog_view);
  }

  AutofillProgressDialogControllerImpl* controller() {
    return controller_.get();
  }

 private:
  std::unique_ptr<AutofillProgressDialogControllerImpl> controller_;
};

IN_PROC_BROWSER_TEST_F(AutofillProgressDialogViewsBrowserTest,
                       InvokeUi_VirtualCardUnmask) {
  base::HistogramTester histogram_tester;
  ShowAndVerifyUi();
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProgressDialog.CardUnmask.Shown", true, 1);
}

// Ensures closing current tab while dialog being visible is correctly handle
// and the browser won't crash.
IN_PROC_BROWSER_TEST_F(AutofillProgressDialogViewsBrowserTest,
                       CloseTabWhileDialogShowing) {
  base::HistogramTester histogram_tester;
  ShowUi("VirtualCardUnmask");
  VerifyUi();
  browser()->tab_strip_model()->GetActiveWebContents()->Close();
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProgressDialog.CardUnmask.Shown", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProgressDialog.CardUnmask.Result", true, 1);
}

// Ensures closing browser while dialog being visible is correctly handled and
// the browser won't crash.
IN_PROC_BROWSER_TEST_F(AutofillProgressDialogViewsBrowserTest,
                       CloseBrowserWhileDialogShowing) {
  base::HistogramTester histogram_tester;
  ShowUi("VirtualCardUnmask");
  VerifyUi();
  browser()->window()->Close();
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProgressDialog.CardUnmask.Shown", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProgressDialog.CardUnmask.Result", true, 1);
}

// Ensures clicking on the cancel button is correctly handled.
// TODO(crbug.com/1257990): Flaky.
IN_PROC_BROWSER_TEST_F(AutofillProgressDialogViewsBrowserTest,
                       DISABLED_ClickCancelButton) {
  base::HistogramTester histogram_tester;
  ShowUi("VirtualCardUnmask");
  VerifyUi();
  GetDialogViews()->CancelDialog();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetDialogViews());
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProgressDialog.CardUnmask.Shown", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProgressDialog.CardUnmask.Result", true, 1);
}

// Ensures the dialog closing with confirmation works properly.
IN_PROC_BROWSER_TEST_F(AutofillProgressDialogViewsBrowserTest,
                       CloseDialogWithConfirmation) {
  base::HistogramTester histogram_tester;
  ShowUi("VirtualCardUnmask");
  VerifyUi();
  auto* dialog_views = GetDialogViews();
  EXPECT_TRUE(dialog_views);
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      dialog_views->GetWidget());
  dialog_views->Dismiss(/*show_confirmation_before_closing=*/true,
                        /*is_canceled_by_user=*/false);
  destroyed_waiter.Wait();
  EXPECT_FALSE(GetDialogViews());
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProgressDialog.CardUnmask.Shown", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProgressDialog.CardUnmask.Result", false, 1);
}

}  // namespace autofill
