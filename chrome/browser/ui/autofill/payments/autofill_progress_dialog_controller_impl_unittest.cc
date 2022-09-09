// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/autofill_progress_dialog_controller_impl.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/autofill/payments/autofill_progress_dialog_controller.h"
#include "chrome/browser/ui/autofill/payments/autofill_progress_dialog_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class AutofillProgressDialogControllerImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  AutofillProgressDialogControllerImplTest() = default;
};

TEST_F(AutofillProgressDialogControllerImplTest,
       ShowDialogWithConfirmationTest) {
  base::HistogramTester histogram_tester;
  AutofillProgressDialogControllerImpl controller(web_contents());

  controller.ShowDialog(AutofillProgressDialogType::kAndroidFIDOProgressDialog,
                        base::DoNothing());
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProgressDialog.AndroidFIDO.Shown", true, 1);

  controller.DismissDialog(/*show_confirmation_before_closing=*/true);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProgressDialog.AndroidFIDO.Result", false, 1);
}

TEST_F(AutofillProgressDialogControllerImplTest,
       ShowDialogWithoutConfirmationTest) {
  base::HistogramTester histogram_tester;
  AutofillProgressDialogControllerImpl controller(web_contents());

  controller.ShowDialog(AutofillProgressDialogType::kAndroidFIDOProgressDialog,
                        base::DoNothing());
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProgressDialog.AndroidFIDO.Shown", true, 1);

  controller.DismissDialog(/*show_confirmation_before_closing=*/false);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProgressDialog.AndroidFIDO.Result", false, 1);
}

TEST_F(AutofillProgressDialogControllerImplTest,
       ShowDialogAndCancelledByUserTest) {
  base::HistogramTester histogram_tester;
  AutofillProgressDialogControllerImpl controller(web_contents());

  controller.ShowDialog(AutofillProgressDialogType::kAndroidFIDOProgressDialog,
                        base::DoNothing());
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProgressDialog.AndroidFIDO.Shown", true, 1);

  controller.OnDismissed(/*is_canceled_by_user=*/true);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProgressDialog.AndroidFIDO.Result", true, 1);
}

}  // namespace autofill
