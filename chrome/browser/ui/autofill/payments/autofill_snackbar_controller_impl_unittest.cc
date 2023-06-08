// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/autofill_snackbar_controller_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/autofill/manual_filling_controller_impl.h"
#include "chrome/browser/autofill/mock_address_accessory_controller.h"
#include "chrome/browser/autofill/mock_credit_card_accessory_controller.h"
#include "chrome/browser/autofill/mock_manual_filling_view.h"
#include "chrome/browser/autofill/mock_password_accessory_controller.h"
#include "chrome/browser/ui/autofill/payments/autofill_snackbar_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;

namespace autofill {

class AutofillSnackbarControllerImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  AutofillSnackbarControllerImplTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ManualFillingControllerImpl::CreateForWebContentsForTesting(
        web_contents(), mock_pwd_controller_.AsWeakPtr(),
        mock_address_controller_.AsWeakPtr(), mock_cc_controller_.AsWeakPtr(),
        std::make_unique<NiceMock<MockManualFillingView>>());
  }

  AutofillSnackbarControllerImpl* controller() {
    if (!controller_)
      controller_ = new AutofillSnackbarControllerImpl(web_contents());
    return controller_;
  }

 private:
  raw_ptr<AutofillSnackbarControllerImpl> controller_ = nullptr;
  NiceMock<MockPasswordAccessoryController> mock_pwd_controller_;
  NiceMock<MockAddressAccessoryController> mock_address_controller_;
  NiceMock<MockCreditCardAccessoryController> mock_cc_controller_;
};

TEST_F(AutofillSnackbarControllerImplTest, MetricsTest) {
  base::HistogramTester histogram_tester;
  controller()->Show();
  // Verify that the count for Shown is incremented and ActionClicked hasn't
  // changed.
  histogram_tester.ExpectUniqueSample("Autofill.Snackbar.VirtualCard.Shown", 1,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Snackbar.VirtualCard.ActionClicked", 1, 0);
  controller()->OnDismissed();

  controller()->Show();
  controller()->OnActionClicked();
  // Verify that the count for both Shown and ActionClicked is incremented.
  histogram_tester.ExpectUniqueSample("Autofill.Snackbar.VirtualCard.Shown", 1,
                                      2);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Snackbar.VirtualCard.ActionClicked", 1, 1);
}

TEST_F(AutofillSnackbarControllerImplTest,
       AttemptToShowDialogWhileAlreadyShowing) {
  base::HistogramTester histogram_tester;
  controller()->Show();
  // Verify that the count for Shown is incremented and ActionClicked hasn't
  // changed.
  histogram_tester.ExpectUniqueSample("Autofill.Snackbar.VirtualCard.Shown", 1,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Snackbar.VirtualCard.ActionClicked", 1, 0);

  // Attempt to show another dialog without dismissing the previous one.
  controller()->Show();

  // Verify that the count for Shown is not incremented.
  histogram_tester.ExpectUniqueSample("Autofill.Snackbar.VirtualCard.Shown", 1,
                                      1);
}
}  // namespace autofill
