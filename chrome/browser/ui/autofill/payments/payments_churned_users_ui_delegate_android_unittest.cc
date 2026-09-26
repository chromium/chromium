// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/payments_churned_users_ui_delegate_android.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/android/autofill/autofill_payments_churned_users_bottom_sheet_bridge.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {
namespace {

class MockAutofillPaymentsChurnedUsersBottomSheetBridge
    : public AutofillPaymentsChurnedUsersBottomSheetBridge {
 public:
  MockAutofillPaymentsChurnedUsersBottomSheetBridge()
      : AutofillPaymentsChurnedUsersBottomSheetBridge(
            /*window_android=*/nullptr) {}
  ~MockAutofillPaymentsChurnedUsersBottomSheetBridge() override = default;

  MOCK_METHOD(
      void,
      RequestShowContent,
      (AutofillEnableResurrectingPaymentsUsersTreatmentArm treatment_arm),
      (override));
};

class PaymentsChurnedUsersUiDelegateAndroidTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PaymentsChurnedUsersUiDelegateAndroidTest() = default;
  ~PaymentsChurnedUsersUiDelegateAndroidTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ChromeAutofillClient::CreateForWebContents(web_contents());
    delegate_ = std::make_unique<PaymentsChurnedUsersUiDelegateAndroid>(
        autofill_client());
  }

  void TearDown() override {
    delegate_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void InitFeatureWithTreatmentArm(
      AutofillEnableResurrectingPaymentsUsersTreatmentArm treatment_arm) {
    std::string treatment_param;
    switch (treatment_arm) {
      case AutofillEnableResurrectingPaymentsUsersTreatmentArm::kSecurity:
        treatment_param = "1";
        break;
      case AutofillEnableResurrectingPaymentsUsersTreatmentArm::kConvenience:
        treatment_param = "2";
        break;
      case AutofillEnableResurrectingPaymentsUsersTreatmentArm::kMessage:
        treatment_param = "3";
        break;
    }
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kAutofillEnableResurrectingPaymentsUsers,
        {{features::kAutofillEnableResurrectingPaymentsUsersTreatment.name,
          treatment_param}});
  }

  ChromeAutofillClient* autofill_client() {
    return ChromeAutofillClient::FromWebContentsForTesting(web_contents());
  }

  PaymentsChurnedUsersUiDelegateAndroid* delegate() { return delegate_.get(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<PaymentsChurnedUsersUiDelegateAndroid> delegate_;
};

TEST_F(PaymentsChurnedUsersUiDelegateAndroidTest,
       ShowPaymentsChurnedUsersUI_SecurityArm) {
  InitFeatureWithTreatmentArm(
      AutofillEnableResurrectingPaymentsUsersTreatmentArm::kSecurity);

  auto mock_bridge =
      std::make_unique<MockAutofillPaymentsChurnedUsersBottomSheetBridge>();
  EXPECT_CALL(
      *mock_bridge,
      RequestShowContent(
          AutofillEnableResurrectingPaymentsUsersTreatmentArm::kSecurity));

  delegate()->SetAutofillPaymentsChurnedUsersBottomSheetBridgeForTesting(
      std::move(mock_bridge));

  delegate()->ShowPaymentsChurnedUsersUI(
      /*accept_callback=*/base::DoNothing(),
      /*cancel_callback=*/base::DoNothing(),
      /*closed_callback=*/base::DoNothing());
}

TEST_F(PaymentsChurnedUsersUiDelegateAndroidTest,
       ShowPaymentsChurnedUsersUI_ConvenienceArm) {
  InitFeatureWithTreatmentArm(
      AutofillEnableResurrectingPaymentsUsersTreatmentArm::kConvenience);

  auto mock_bridge =
      std::make_unique<MockAutofillPaymentsChurnedUsersBottomSheetBridge>();
  EXPECT_CALL(
      *mock_bridge,
      RequestShowContent(
          AutofillEnableResurrectingPaymentsUsersTreatmentArm::kConvenience));

  delegate()->SetAutofillPaymentsChurnedUsersBottomSheetBridgeForTesting(
      std::move(mock_bridge));

  delegate()->ShowPaymentsChurnedUsersUI(
      /*accept_callback=*/base::DoNothing(),
      /*cancel_callback=*/base::DoNothing(),
      /*closed_callback=*/base::DoNothing());
}

TEST_F(PaymentsChurnedUsersUiDelegateAndroidTest,
       ShowPaymentsChurnedUsersUI_MessageArm) {
  InitFeatureWithTreatmentArm(
      AutofillEnableResurrectingPaymentsUsersTreatmentArm::kMessage);

  auto mock_bridge =
      std::make_unique<MockAutofillPaymentsChurnedUsersBottomSheetBridge>();
  EXPECT_CALL(*mock_bridge, RequestShowContent).Times(0);

  delegate()->SetAutofillPaymentsChurnedUsersBottomSheetBridgeForTesting(
      std::move(mock_bridge));

  delegate()->ShowPaymentsChurnedUsersUI(
      /*accept_callback=*/base::DoNothing(),
      /*cancel_callback=*/base::DoNothing(),
      /*closed_callback=*/base::DoNothing());
}

TEST_F(PaymentsChurnedUsersUiDelegateAndroidTest,
       ShowPaymentsChurnedUsersUI_DefaultArmFallsBackToSecurity) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillEnableResurrectingPaymentsUsers,
      {{features::kAutofillEnableResurrectingPaymentsUsersTreatment.name,
        "0"}});

  auto mock_bridge =
      std::make_unique<MockAutofillPaymentsChurnedUsersBottomSheetBridge>();
  EXPECT_CALL(
      *mock_bridge,
      RequestShowContent(
          AutofillEnableResurrectingPaymentsUsersTreatmentArm::kSecurity));

  delegate()->SetAutofillPaymentsChurnedUsersBottomSheetBridgeForTesting(
      std::move(mock_bridge));

  delegate()->ShowPaymentsChurnedUsersUI(
      /*accept_callback=*/base::DoNothing(),
      /*cancel_callback=*/base::DoNothing(),
      /*closed_callback=*/base::DoNothing());
}

}  // namespace
}  // namespace autofill::payments
