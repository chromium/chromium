// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_save_card_bottom_sheet_bridge.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/android/autofill/autofill_save_card_delegate_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/grit/components_scaled_resources.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"

namespace autofill {

using CardSaveType = payments::PaymentsAutofillClient::CardSaveType;
using SaveCreditCardOptions =
    payments::PaymentsAutofillClient::SaveCreditCardOptions;

class AutofillSaveCardBottomSheetBridgeTest
    : public ChromeRenderViewHostTestHarness,
      public ::testing::WithParamInterface</*is_upload_save*/ bool> {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    window_ = ui::WindowAndroid::CreateForTesting();
    window_->get()->AddChild(web_contents()->GetNativeView());
    tab_model_ = std::make_unique<TestTabModel>(profile());
    autofill_save_card_bottom_sheet_bridge_ =
        std::make_unique<AutofillSaveCardBottomSheetBridge>(
            window_.get()->get(), tab_model_.get());
  }

  std::unique_ptr<AutofillSaveCardDelegateAndroid> GetDelegate(
      SaveCreditCardOptions options) {
    if (IsUploadSave()) {
      return std::make_unique<AutofillSaveCardDelegateAndroid>(
          CreateUploadSaveCardCallback(), options, web_contents());
    } else {
      return std::make_unique<AutofillSaveCardDelegateAndroid>(
          CreateLocalSaveCardCallback(), options, web_contents());
    }
  }

  std::string GetBaseHistogramName() {
    return base::StrCat({"Autofill.SaveCreditCardPromptOffer.Android",
                         IsUploadSave() ? ".Server" : ".Local"});
  }

  bool IsUploadSave() { return GetParam(); }

  std::unique_ptr<AutofillSaveCardBottomSheetBridge>
      autofill_save_card_bottom_sheet_bridge_;

 private:
  payments::PaymentsAutofillClient::LocalSaveCardPromptCallback
  CreateLocalSaveCardCallback() {
    return base::BindOnce(
        &AutofillSaveCardBottomSheetBridgeTest::LocalSaveCardCallback,
        base::Unretained(this));
  }

  void LocalSaveCardCallback(
      payments::PaymentsAutofillClient::SaveCardOfferUserDecision decision) {}

  payments::PaymentsAutofillClient::UploadSaveCardPromptCallback
  CreateUploadSaveCardCallback() {
    return base::BindOnce(
        &AutofillSaveCardBottomSheetBridgeTest::UploadSaveCardCallback,
        base::Unretained(this));
  }

  void UploadSaveCardCallback(
      payments::PaymentsAutofillClient::SaveCardOfferUserDecision user_decision,
      const payments::PaymentsAutofillClient::UserProvidedCardDetails&
          user_provided_card_details) {}

  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window_;
  std::unique_ptr<TestTabModel> tab_model_;
};

INSTANTIATE_TEST_SUITE_P(,
                         AutofillSaveCardBottomSheetBridgeTest,
                         testing::Bool());

TEST_P(AutofillSaveCardBottomSheetBridgeTest, LogsPromptShown) {
  base::HistogramTester histogram_tester;
  autofill_save_card_bottom_sheet_bridge_->SetSaveCardDelegateForTesting(
      GetDelegate(
          SaveCreditCardOptions().with_show_prompt(true).with_card_save_type(
              CardSaveType::kCardSaveOnly)));
  autofill_save_card_bottom_sheet_bridge_->OnUiShown(/*env=*/nullptr);
  histogram_tester.ExpectUniqueSample(
      GetBaseHistogramName(), autofill_metrics::SaveCardPromptOffer::kShown, 1);
}

TEST_P(AutofillSaveCardBottomSheetBridgeTest,
       LogsPromptShown_WhenRequestingCardHolderName) {
  base::HistogramTester histogram_tester;
  autofill_save_card_bottom_sheet_bridge_->SetSaveCardDelegateForTesting(
      GetDelegate(SaveCreditCardOptions()
                      .with_should_request_name_from_user(true)
                      .with_show_prompt(true)
                      .with_card_save_type(CardSaveType::kCardSaveOnly)));
  autofill_save_card_bottom_sheet_bridge_->OnUiShown(/*env=*/nullptr);
  histogram_tester.ExpectUniqueSample(
      GetBaseHistogramName(), autofill_metrics::SaveCardPromptOffer::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({GetBaseHistogramName(), ".RequestingCardholderName"}),
      autofill_metrics::SaveCardPromptOffer::kShown, 1);
}

TEST_P(AutofillSaveCardBottomSheetBridgeTest,
       LogsPromptShown_WhenRequestingExpirationDate) {
  base::HistogramTester histogram_tester;
  autofill_save_card_bottom_sheet_bridge_->SetSaveCardDelegateForTesting(
      GetDelegate(SaveCreditCardOptions()
                      .with_should_request_expiration_date_from_user(true)
                      .with_show_prompt(true)
                      .with_card_save_type(CardSaveType::kCardSaveOnly)));
  autofill_save_card_bottom_sheet_bridge_->OnUiShown(/*env=*/nullptr);
  histogram_tester.ExpectUniqueSample(
      GetBaseHistogramName(), autofill_metrics::SaveCardPromptOffer::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({GetBaseHistogramName(), ".RequestingExpirationDate"}),
      autofill_metrics::SaveCardPromptOffer::kShown, 1);
}

TEST_P(AutofillSaveCardBottomSheetBridgeTest,
       LogsPromptShown_WhenSavingWithCvc) {
  base::HistogramTester histogram_tester;
  autofill_save_card_bottom_sheet_bridge_->SetSaveCardDelegateForTesting(
      GetDelegate(
          SaveCreditCardOptions().with_show_prompt(true).with_card_save_type(
              CardSaveType::kCardSaveWithCvc)));
  autofill_save_card_bottom_sheet_bridge_->OnUiShown(/*env=*/nullptr);
  histogram_tester.ExpectUniqueSample(
      GetBaseHistogramName(), autofill_metrics::SaveCardPromptOffer::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({GetBaseHistogramName(), ".SavingWithCvc"}),
      autofill_metrics::SaveCardPromptOffer::kShown, 1);
}

TEST_P(AutofillSaveCardBottomSheetBridgeTest,
       LogsPromptShown_WithMultipleLegalLines) {
  if (!GetParam()) {
    GTEST_SKIP() << "Not applicable for local save, as legal lines are "
                    "present only in server save scenarios";
  }

  base::HistogramTester histogram_tester;
  autofill_save_card_bottom_sheet_bridge_->SetSaveCardDelegateForTesting(
      GetDelegate(SaveCreditCardOptions()
                      .with_has_multiple_legal_lines(true)
                      .with_show_prompt(true)
                      .with_card_save_type(CardSaveType::kCardSaveOnly)));
  autofill_save_card_bottom_sheet_bridge_->OnUiShown(/*env=*/nullptr);
  histogram_tester.ExpectUniqueSample(
      GetBaseHistogramName(), autofill_metrics::SaveCardPromptOffer::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({GetBaseHistogramName(), ".WithMultipleLegalLines"}),
      autofill_metrics::SaveCardPromptOffer::kShown, 1);
}

TEST_P(AutofillSaveCardBottomSheetBridgeTest,
       LogsPromptShown_ForCardWithSameLastFourButDifferentExpiration) {
  if (!IsUploadSave()) {
    GTEST_SKIP() << "Not applicable for local save, as the condition (same "
                    "last four digits, different expiration date) is only "
                    "possible for server save scenarios.";
  }

  base::HistogramTester histogram_tester;
  autofill_save_card_bottom_sheet_bridge_->SetSaveCardDelegateForTesting(
      GetDelegate(
          SaveCreditCardOptions()
              .with_same_last_four_as_server_card_but_different_expiration_date(
                  true)
              .with_show_prompt(true)
              .with_card_save_type(CardSaveType::kCardSaveOnly)));
  autofill_save_card_bottom_sheet_bridge_->OnUiShown(/*env=*/nullptr);
  histogram_tester.ExpectUniqueSample(
      GetBaseHistogramName(), autofill_metrics::SaveCardPromptOffer::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {GetBaseHistogramName(), ".WithSameLastFourButDifferentExpiration"}),
      autofill_metrics::SaveCardPromptOffer::kShown, 1);
}

TEST_P(AutofillSaveCardBottomSheetBridgeTest,
       LogsPromptShown_ForAllRelevantSubHistograms) {
  if (!IsUploadSave()) {
    GTEST_SKIP() << "Not applicable for local save, as legal lines are "
                    "present only in server save scenarios.";
  }

  base::HistogramTester histogram_tester;
  autofill_save_card_bottom_sheet_bridge_->SetSaveCardDelegateForTesting(
      GetDelegate(SaveCreditCardOptions()
                      .with_should_request_name_from_user(true)
                      .with_has_multiple_legal_lines(true)
                      .with_show_prompt(true)
                      .with_card_save_type(CardSaveType::kCardSaveOnly)));
  autofill_save_card_bottom_sheet_bridge_->OnUiShown(/*env=*/nullptr);
  histogram_tester.ExpectUniqueSample(
      GetBaseHistogramName(), autofill_metrics::SaveCardPromptOffer::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({GetBaseHistogramName(), ".RequestingCardholderName"}),
      autofill_metrics::SaveCardPromptOffer::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({GetBaseHistogramName(), ".WithMultipleLegalLines"}),
      autofill_metrics::SaveCardPromptOffer::kShown, 1);
}

}  // namespace autofill
