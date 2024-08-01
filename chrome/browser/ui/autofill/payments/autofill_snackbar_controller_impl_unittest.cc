// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/autofill_snackbar_controller_impl.h"

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/autofill/mock_manual_filling_view.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_controller_impl.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_address_accessory_controller.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_password_accessory_controller.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_payment_method_accessory_controller.h"
#include "chrome/browser/ui/autofill/payments/autofill_snackbar_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

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
        mock_address_controller_.AsWeakPtr(),
        mock_payment_method_controller_.AsWeakPtr(),
        std::make_unique<NiceMock<MockManualFillingView>>());
  }

  AutofillSnackbarControllerImpl* controller() {
    if (!controller_)
      controller_ = new AutofillSnackbarControllerImpl(web_contents());
    return controller_;
  }

  void ShowWithOnDismissCallback(AutofillSnackbarType autofill_snackbar_type) {
    controller()->ShowWithDurationAndCallback(
        autofill_snackbar_type,
        AutofillSnackbarControllerImpl::kDefaultSnackbarDuration,
        base::BindOnce(&AutofillSnackbarControllerImplTest::OnDismissCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 protected:
  int on_dismiss_callback_call_count_ = 0;

 private:
  void OnDismissCallback() { ++on_dismiss_callback_call_count_; }

  raw_ptr<AutofillSnackbarControllerImpl> controller_ = nullptr;
  NiceMock<MockPasswordAccessoryController> mock_pwd_controller_;
  NiceMock<MockAddressAccessoryController> mock_address_controller_;
  NiceMock<MockPaymentMethodAccessoryController>
      mock_payment_method_controller_;

  base::WeakPtrFactory<AutofillSnackbarControllerImplTest> weak_ptr_factory_{
      this};
};

TEST_F(AutofillSnackbarControllerImplTest, Metrics_VirtualCard) {
  base::HistogramTester histogram_tester;
  controller()->Show(AutofillSnackbarType::kVirtualCard);
  // Verify that the count for Shown is incremented and ActionClicked hasn't
  // changed.
  histogram_tester.ExpectUniqueSample("Autofill.Snackbar.VirtualCard.Shown", 1,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Snackbar.VirtualCard.ActionClicked", 1, 0);
  controller()->OnDismissed();

  controller()->Show(AutofillSnackbarType::kVirtualCard);
  controller()->OnActionClicked();
  // Verify that the count for both Shown and ActionClicked is incremented.
  histogram_tester.ExpectUniqueSample("Autofill.Snackbar.VirtualCard.Shown", 1,
                                      2);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Snackbar.VirtualCard.ActionClicked", 1, 1);
}

TEST_F(AutofillSnackbarControllerImplTest,
       Metrics_ShowVirtualCardWhenAlreadyShowing) {
  base::HistogramTester histogram_tester;
  controller()->Show(AutofillSnackbarType::kVirtualCard);
  // Verify that the count for Shown is incremented and ActionClicked hasn't
  // changed.
  histogram_tester.ExpectUniqueSample("Autofill.Snackbar.VirtualCard.Shown", 1,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Snackbar.VirtualCard.ActionClicked", 1, 0);

  // Attempt to show another dialog without dismissing the previous one.
  controller()->Show(AutofillSnackbarType::kVirtualCard);

  // Verify that the count for both Shown is not incremented.
  histogram_tester.ExpectUniqueSample("Autofill.Snackbar.VirtualCard.Shown", 1,
                                      1);
}

TEST_F(AutofillSnackbarControllerImplTest, Metrics_ShowMandatoryReauth) {
  base::HistogramTester histogram_tester;
  controller()->Show(AutofillSnackbarType::kMandatoryReauth);
  // Verify that the count for Shown is incremented and ActionClicked hasn't
  // changed.
  histogram_tester.ExpectUniqueSample("Autofill.Snackbar.MandatoryReauth.Shown",
                                      1, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Snackbar.MandatoryReauth.ActionClicked", 1, 0);
  controller()->OnDismissed();

  // TODO(crbug.com/40570965): Figure out how to mock
  // ShowAutofillCreditCardSettings to test ActionClicked metric.
}

TEST_F(AutofillSnackbarControllerImplTest, Metrics_SaveCardSuccess) {
  base::HistogramTester histogram_tester;

  controller()->Show(AutofillSnackbarType::kSaveCardSuccess);

  histogram_tester.ExpectUniqueSample("Autofill.Snackbar.SaveCardSuccess.Shown",
                                      true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Snackbar.SaveCardSuccess.ActionClicked", true, 0);

  controller()->OnActionClicked();

  histogram_tester.ExpectUniqueSample(
      "Autofill.Snackbar.SaveCardSuccess.ActionClicked", true, 1);
}

TEST_F(AutofillSnackbarControllerImplTest, Metrics_VirtualCardEnrollSuccess) {
  base::HistogramTester histogram_tester;

  controller()->Show(AutofillSnackbarType::kVirtualCardEnrollSuccess);

  histogram_tester.ExpectUniqueSample(
      "Autofill.Snackbar.VirtualCardEnrollSuccess.Shown", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Snackbar.VirtualCardEnrollSuccess.ActionClicked", true, 0);

  controller()->OnActionClicked();

  histogram_tester.ExpectUniqueSample(
      "Autofill.Snackbar.VirtualCardEnrollSuccess.ActionClicked", true, 1);
}

TEST_F(AutofillSnackbarControllerImplTest,
       SaveCardSuccessMessageAndActionButtonText) {
  controller()->Show(AutofillSnackbarType::kSaveCardSuccess);

  EXPECT_EQ(controller()->GetMessageText(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_DESCRIPTION_TEXT));
  EXPECT_EQ(
      controller()->GetActionButtonText(),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_BUTTON_TEXT));
}

TEST_F(AutofillSnackbarControllerImplTest,
       VirtualCardEnrollSuccessMessageAndActionButtonText) {
  controller()->Show(AutofillSnackbarType::kVirtualCardEnrollSuccess);

  EXPECT_EQ(
      controller()->GetMessageText(),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_SUCCESS_DESCRIPTION_TEXT));
  EXPECT_EQ(
      controller()->GetActionButtonText(),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_BUTTON_TEXT));
}

TEST_F(AutofillSnackbarControllerImplTest, Metrics_SaveServerIbanSuccess) {
  base::HistogramTester histogram_tester;
  controller()->Show(AutofillSnackbarType::kSaveServerIbanSuccess);
  // Verify that the count for Shown is incremented and ActionClicked hasn't
  // changed.
  histogram_tester.ExpectUniqueSample(
      "Autofill.Snackbar.SaveServerIbanSuccess.Shown", 1, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.Snackbar.SaveServerIbanSuccess.ActionClicked", 0);
  controller()->OnDismissed();

  controller()->Show(AutofillSnackbarType::kSaveServerIbanSuccess);
  controller()->OnActionClicked();

  // Verify that the count for both Shown and ActionClicked is incremented.
  histogram_tester.ExpectUniqueSample(
      "Autofill.Snackbar.SaveServerIbanSuccess.Shown", 1, 2);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Snackbar.SaveServerIbanSuccess.ActionClicked", true, 1);
}

TEST_F(AutofillSnackbarControllerImplTest,
       SaveServerIbanSuccessMessageAndActionButtonText) {
  controller()->Show(AutofillSnackbarType::kSaveServerIbanSuccess);

  EXPECT_EQ(controller()->GetMessageText(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_SERVER_IBAN_SUCCESS_SNACKBAR_MESSAGE_TEXT));
  EXPECT_EQ(controller()->GetActionButtonText(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_SERVER_IBAN_SUCCESS_SNACKBAR_BUTTON_TEXT));
}

TEST_F(AutofillSnackbarControllerImplTest, OnShowDefaultDurationSet) {
  controller()->Show(AutofillSnackbarType::kSaveCardSuccess);
  EXPECT_EQ(controller()->GetDuration(),
            AutofillSnackbarControllerImpl::kDefaultSnackbarDuration);
}

TEST_F(AutofillSnackbarControllerImplTest,
       OnShowWithDurationCustomDurationSet) {
  base::TimeDelta duration = base::Seconds(3);
  controller()->ShowWithDurationAndCallback(
      AutofillSnackbarType::kSaveCardSuccess, duration, std::nullopt);
  EXPECT_EQ(controller()->GetDuration(), duration);
}

TEST_F(AutofillSnackbarControllerImplTest, OnDismissCallbackCalled) {
  ShowWithOnDismissCallback(AutofillSnackbarType::kSaveCardSuccess);
  EXPECT_EQ(on_dismiss_callback_call_count_, 0);

  controller()->OnDismissed();
  EXPECT_EQ(on_dismiss_callback_call_count_, 1);
}

TEST_F(AutofillSnackbarControllerImplTest, OnDismissTwiceCallbackCalledOnce) {
  ShowWithOnDismissCallback(AutofillSnackbarType::kSaveCardSuccess);
  controller()->OnDismissed();
  controller()->Show(AutofillSnackbarType::kSaveCardSuccess);
  controller()->OnDismissed();

  EXPECT_EQ(on_dismiss_callback_call_count_, 1);
}

}  // namespace autofill
