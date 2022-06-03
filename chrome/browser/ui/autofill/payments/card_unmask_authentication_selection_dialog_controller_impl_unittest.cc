// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/card_unmask_authentication_selection_dialog_controller_impl.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill {

namespace {

class TestCardUnmaskAuthenticationSelectionDialogControllerImpl
    : public CardUnmaskAuthenticationSelectionDialogControllerImpl {
 public:
  static void CreateForTesting(content::WebContents* web_contents) {
    web_contents->SetUserData(
        UserDataKey(),
        std::make_unique<
            TestCardUnmaskAuthenticationSelectionDialogControllerImpl>(
            web_contents));
  }

  explicit TestCardUnmaskAuthenticationSelectionDialogControllerImpl(
      content::WebContents* web_contents)
      : CardUnmaskAuthenticationSelectionDialogControllerImpl(web_contents) {}
};

}  // namespace

class CardUnmaskAuthenticationSelectionDialogControllerImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  CardUnmaskAuthenticationSelectionDialogControllerImplTest() = default;
  CardUnmaskAuthenticationSelectionDialogControllerImplTest(
      const CardUnmaskAuthenticationSelectionDialogControllerImplTest&) =
      delete;
  CardUnmaskAuthenticationSelectionDialogControllerImplTest& operator=(
      const CardUnmaskAuthenticationSelectionDialogControllerImplTest&) =
      delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    TestCardUnmaskAuthenticationSelectionDialogControllerImpl::CreateForTesting(
        web_contents());
  }

  TestCardUnmaskAuthenticationSelectionDialogControllerImpl* controller() {
    return static_cast<
        TestCardUnmaskAuthenticationSelectionDialogControllerImpl*>(
        TestCardUnmaskAuthenticationSelectionDialogControllerImpl::
            FromWebContents(web_contents()));
  }
};

TEST_F(CardUnmaskAuthenticationSelectionDialogControllerImplTest,
       DialogCanceledByUserBeforeConfirmation) {
  base::HistogramTester histogram_tester;

  DCHECK(controller());
  controller()->OnDialogClosed(/*user_closed_dialog=*/true,
                               /*server_success=*/false);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUnmaskAuthenticationSelectionDialog.Result",
      AutofillMetrics::CardUnmaskAuthenticationSelectionDialogResultMetric::
          kCanceledByUserBeforeSelection,
      1);
}

TEST_F(CardUnmaskAuthenticationSelectionDialogControllerImplTest,
       DialogCanceledByUserAfterConfirmation) {
  base::HistogramTester histogram_tester;

  DCHECK(controller());
  controller()->OnOkButtonClicked(std::string());
  controller()->OnDialogClosed(/*user_closed_dialog=*/true,
                               /*server_success=*/false);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUnmaskAuthenticationSelectionDialog.Result",
      AutofillMetrics::CardUnmaskAuthenticationSelectionDialogResultMetric::
          kCanceledByUserAfterSelection,
      1);
}

TEST_F(CardUnmaskAuthenticationSelectionDialogControllerImplTest,
       ServerRequestSucceeded) {
  base::HistogramTester histogram_tester;

  DCHECK(controller());
  controller()->OnOkButtonClicked(std::string());
  controller()->OnDialogClosed(/*user_closed_dialog=*/false,
                               /*server_success=*/true);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUnmaskAuthenticationSelectionDialog.Result",
      AutofillMetrics::CardUnmaskAuthenticationSelectionDialogResultMetric::
          kDismissedByServerRequestSuccess,
      1);
}

TEST_F(CardUnmaskAuthenticationSelectionDialogControllerImplTest,
       ServerRequestFailed) {
  base::HistogramTester histogram_tester;

  DCHECK(controller());
  controller()->OnOkButtonClicked(std::string());
  controller()->OnDialogClosed(/*user_closed_dialog=*/false,
                               /*server_success=*/false);

  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUnmaskAuthenticationSelectionDialog.Result",
      AutofillMetrics::CardUnmaskAuthenticationSelectionDialogResultMetric::
          kDismissedByServerRequestFailure,
      1);
}

}  // namespace autofill
