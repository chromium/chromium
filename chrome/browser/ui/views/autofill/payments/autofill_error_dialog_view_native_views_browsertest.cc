// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/autofill/payments/autofill_error_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/autofill_error_dialog_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/autofill/payments/autofill_error_dialog_view_native_views.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "content/public/test/browser_test.h"

namespace autofill {

// Param of the AutofillErrorDialogViewNativeViewsBrowserTest:
// -- bool server_did_return_title;
// -- bool server_did_return_description;
class AutofillErrorDialogViewNativeViewsBrowserTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  AutofillErrorDialogViewNativeViewsBrowserTest() = default;
  ~AutofillErrorDialogViewNativeViewsBrowserTest() override = default;
  AutofillErrorDialogViewNativeViewsBrowserTest(
      const AutofillErrorDialogViewNativeViewsBrowserTest&) = delete;
  AutofillErrorDialogViewNativeViewsBrowserTest& operator=(
      const AutofillErrorDialogViewNativeViewsBrowserTest&) = delete;

  // DialogBrowserTest:
  void SetUpOnMainThread() override {
    controller_ = std::make_unique<AutofillErrorDialogControllerImpl>(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  void ShowUi(const std::string& name) override {
    AutofillErrorDialogContext autofill_error_dialog_context;
    if (server_did_return_title()) {
      autofill_error_dialog_context.server_returned_title =
          "test_server_returned_title";
    }
    if (server_did_return_description()) {
      autofill_error_dialog_context.server_returned_description =
          "test_server_returned_description";
    }

    if (name.find("temporary") != std::string::npos) {
      autofill_error_dialog_context.type =
          AutofillErrorDialogType::kVirtualCardTemporaryError;
    } else if (name.find("permanent") != std::string::npos) {
      autofill_error_dialog_context.type =
          AutofillErrorDialogType::kVirtualCardPermanentError;
    } else if (name.find("eligibility") != std::string::npos) {
      autofill_error_dialog_context.type =
          AutofillErrorDialogType::kVirtualCardNotEligibleError;
    } else {
      NOTREACHED();
      return;
    }

    controller()->Show(autofill_error_dialog_context);
  }

  AutofillErrorDialogViewNativeViews* GetDialogViews() {
    if (!controller())
      return nullptr;

    AutofillErrorDialogView* dialog_view =
        controller()->autofill_error_dialog_view();
    if (!dialog_view)
      return nullptr;

    return static_cast<AutofillErrorDialogViewNativeViews*>(dialog_view);
  }

  AutofillErrorDialogControllerImpl* controller() { return controller_.get(); }

  bool server_did_return_title() { return std::get<0>(GetParam()); }

  bool server_did_return_description() { return std::get<1>(GetParam()); }

 private:
  std::unique_ptr<AutofillErrorDialogControllerImpl> controller_;
};

INSTANTIATE_TEST_SUITE_P(,
                         AutofillErrorDialogViewNativeViewsBrowserTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

// Verify that the dialog is shown, and the metrics for shown are incremented
// correctly for virtual card temporary error.
IN_PROC_BROWSER_TEST_P(AutofillErrorDialogViewNativeViewsBrowserTest,
                       InvokeUi_temporary) {
  base::HistogramTester histogram_tester;

  ShowAndVerifyUi();

  // Verify that the metric for shown is incremented.
  EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.ErrorDialogShown"),
              BucketsAre(base::Bucket(
                  AutofillErrorDialogType::kVirtualCardTemporaryError, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.ErrorDialogShown.WithServerText"),
      BucketsAre(base::Bucket(
          AutofillErrorDialogType::kVirtualCardTemporaryError,
          /*count=*/server_did_return_title() && server_did_return_description()
              ? 1
              : 0)));
}

// Verify that the dialog is shown, and the metrics for shown are incremented
// correctly for virtual card permanent error.
IN_PROC_BROWSER_TEST_P(AutofillErrorDialogViewNativeViewsBrowserTest,
                       InvokeUi_permanent) {
  base::HistogramTester histogram_tester;

  ShowAndVerifyUi();

  EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.ErrorDialogShown"),
              BucketsAre(base::Bucket(
                  AutofillErrorDialogType::kVirtualCardPermanentError, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.ErrorDialogShown.WithServerText"),
      BucketsAre(base::Bucket(
          AutofillErrorDialogType::kVirtualCardPermanentError,
          /*count=*/server_did_return_title() && server_did_return_description()
              ? 1
              : 0)));
}

// Verify that the dialog is shown, and the metrics for shown are incremented
// correctly for virtual card not eligible error.
IN_PROC_BROWSER_TEST_P(AutofillErrorDialogViewNativeViewsBrowserTest,
                       InvokeUi_eligibility) {
  base::HistogramTester histogram_tester;

  ShowAndVerifyUi();

  // Verify that the metric for shown is incremented.
  EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.ErrorDialogShown"),
              BucketsAre(base::Bucket(
                  AutofillErrorDialogType::kVirtualCardNotEligibleError, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.ErrorDialogShown.WithServerText"),
      BucketsAre(base::Bucket(
          AutofillErrorDialogType::kVirtualCardNotEligibleError,
          /*count=*/server_did_return_title() && server_did_return_description()
              ? 1
              : 0)));
}

// Ensures closing current tab while dialog being visible is correctly handled,
// and the metrics for shown are incremented correctly.
IN_PROC_BROWSER_TEST_P(AutofillErrorDialogViewNativeViewsBrowserTest,
                       CloseTabWhileDialogShowing) {
  base::HistogramTester histogram_tester;

  ShowUi("temporary");
  VerifyUi();
  browser()->tab_strip_model()->GetActiveWebContents()->Close();
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.ErrorDialogShown"),
              BucketsAre(base::Bucket(
                  AutofillErrorDialogType::kVirtualCardTemporaryError, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.ErrorDialogShown.WithServerText"),
      BucketsAre(base::Bucket(
          AutofillErrorDialogType::kVirtualCardTemporaryError,
          /*count=*/server_did_return_title() && server_did_return_description()
              ? 1
              : 0)));
}

// Ensures closing browser while dialog being visible is correctly handled, and
// the metrics for shown are incremented correctly.
IN_PROC_BROWSER_TEST_P(AutofillErrorDialogViewNativeViewsBrowserTest,
                       CanCloseBrowserWhileDialogShowing) {
  base::HistogramTester histogram_tester;

  ShowUi("temporary");
  VerifyUi();
  browser()->window()->Close();
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.ErrorDialogShown"),
              BucketsAre(base::Bucket(
                  AutofillErrorDialogType::kVirtualCardTemporaryError, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.ErrorDialogShown.WithServerText"),
      BucketsAre(base::Bucket(
          AutofillErrorDialogType::kVirtualCardTemporaryError,
          /*count=*/server_did_return_title() && server_did_return_description()
              ? 1
              : 0)));
}

// Ensures clicking on the cancel button is correctly handled, and the metrics
// for shown are incremented correctly.
IN_PROC_BROWSER_TEST_P(AutofillErrorDialogViewNativeViewsBrowserTest,
                       ClickCancelButton) {
  base::HistogramTester histogram_tester;

  ShowUi("temporary");
  VerifyUi();
  GetDialogViews()->CancelDialog();
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.ErrorDialogShown"),
              BucketsAre(base::Bucket(
                  AutofillErrorDialogType::kVirtualCardTemporaryError, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.ErrorDialogShown.WithServerText"),
      BucketsAre(base::Bucket(
          AutofillErrorDialogType::kVirtualCardTemporaryError,
          /*count=*/server_did_return_title() && server_did_return_description()
              ? 1
              : 0)));
}

}  // namespace autofill
