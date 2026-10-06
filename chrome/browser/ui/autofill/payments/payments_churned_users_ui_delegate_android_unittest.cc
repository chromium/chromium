// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/payments_churned_users_ui_delegate_android.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/android/autofill/autofill_payments_churned_users_bottom_sheet_bridge.h"
#include "chrome/browser/ui/android/autofill/autofill_payments_churned_users_bottom_sheet_bridge_test_api.h"
#include "chrome/browser/ui/autofill/autofill_message_model.h"
#include "chrome/browser/ui/autofill/autofill_snackbar_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_snackbar_type.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/mock_autofill_message_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/payments/payments_churned_users_metrics.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/messages/android/message_enums.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {
namespace {

using ::base::test::RunOnceCallback;
using ::base::test::RunOnceClosure;
using ::testing::_;

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
      (AutofillEnableResurrectingPaymentsUsersTreatmentArm treatment_arm,
       base::OnceCallback<void(PaymentsUiClosedReason)> callback),
      (override));
};

class MockAutofillSnackbarControllerImpl
    : public AutofillSnackbarControllerImpl {
 public:
  explicit MockAutofillSnackbarControllerImpl(
      content::WebContents* web_contents)
      : AutofillSnackbarControllerImpl(web_contents) {}

  MOCK_METHOD(void,
              Show,
              (AutofillSnackbarType, base::OnceClosure),
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
    autofill_client()->SetAutofillSnackbarControllerImplForTesting(
        std::make_unique<MockAutofillSnackbarControllerImpl>(web_contents()));
    delegate_ = std::make_unique<PaymentsChurnedUsersUiDelegateAndroid>(
        autofill_client());
    auto mock_message_controller =
        std::make_unique<MockAutofillMessageController>();
    mock_message_controller_ = mock_message_controller.get();
    delegate_->SetAutofillMessageControllerForTesting(
        std::move(mock_message_controller));
  }

  void TearDown() override {
    mock_message_controller_ = nullptr;
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

  MockAutofillMessageController* mock_message_controller() {
    return mock_message_controller_;
  }

  MockAutofillSnackbarControllerImpl* mock_snackbar_controller() {
    return static_cast<MockAutofillSnackbarControllerImpl*>(
        autofill_client()->GetAutofillSnackbarController());
  }

  PaymentsChurnedUsersUiDelegateAndroid* delegate() { return delegate_.get(); }

  void ResetDelegate() {
    mock_message_controller_ = nullptr;
    delegate_.reset();
  }

  void ExpectShowResultRecorded(
      autofill_metrics::PaymentsChurnedUsersBubbleShowResult show_result,
      base::HistogramBase::Count32 expected_count = 1) {
    histogram_tester_.ExpectUniqueSample(
        "Autofill.PaymentsChurnedUsersBubble.ShowResult", show_result,
        expected_count);
  }

  void ExpectResultRecorded(PaymentsUiClosedReason closed_reason,
                            base::HistogramBase::Count32 expected_count = 1) {
    histogram_tester_.ExpectUniqueSample(
        "Autofill.PaymentsChurnedUsersBubble.Result", closed_reason,
        expected_count);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
  raw_ptr<MockAutofillMessageController> mock_message_controller_ = nullptr;
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
          AutofillEnableResurrectingPaymentsUsersTreatmentArm::kSecurity, _));

  delegate()->SetAutofillPaymentsChurnedUsersBottomSheetBridgeForTesting(
      std::move(mock_bridge));

  delegate()->ShowPaymentsChurnedUsersUI(
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
          AutofillEnableResurrectingPaymentsUsersTreatmentArm::kConvenience,
          _));

  delegate()->SetAutofillPaymentsChurnedUsersBottomSheetBridgeForTesting(
      std::move(mock_bridge));

  delegate()->ShowPaymentsChurnedUsersUI(
      /*closed_callback=*/base::DoNothing());
}

TEST_F(PaymentsChurnedUsersUiDelegateAndroidTest,
       ShowPaymentsChurnedUsersUI_NullBridgeRunsCallbackWithUnknown) {
  InitFeatureWithTreatmentArm(
      AutofillEnableResurrectingPaymentsUsersTreatmentArm::kSecurity);

  base::MockCallback<base::OnceCallback<void(PaymentsUiClosedReason)>>
      closed_callback;
  EXPECT_CALL(closed_callback, Run(PaymentsUiClosedReason::kUnknown));

  delegate()->ShowPaymentsChurnedUsersUI(closed_callback.Get());

  ExpectResultRecorded(PaymentsUiClosedReason::kUnknown, 0);
}

TEST_F(PaymentsChurnedUsersUiDelegateAndroidTest,
       ShowPaymentsChurnedUsersUI_MessageArm_AcceptedShowsSnackbar) {
  InitFeatureWithTreatmentArm(
      AutofillEnableResurrectingPaymentsUsersTreatmentArm::kMessage);

  auto mock_bridge =
      std::make_unique<MockAutofillPaymentsChurnedUsersBottomSheetBridge>();
  EXPECT_CALL(*mock_bridge, RequestShowContent).Times(0);
  delegate()->SetAutofillPaymentsChurnedUsersBottomSheetBridgeForTesting(
      std::move(mock_bridge));

  std::unique_ptr<AutofillMessageModel> shown_message_model;
  EXPECT_CALL(*mock_message_controller(), Show)
      .WillOnce(MoveArg<0>(&shown_message_model));

  base::MockCallback<base::OnceCallback<void(PaymentsUiClosedReason)>>
      closed_callback;

  delegate()->ShowPaymentsChurnedUsersUI(closed_callback.Get());

  ExpectShowResultRecorded(
      autofill_metrics::PaymentsChurnedUsersBubbleShowResult::kShown);

  ASSERT_TRUE(shown_message_model);
  EXPECT_EQ(shown_message_model->GetType(),
            AutofillMessageModel::Type::kResurrectChurnedUsers);

  EXPECT_CALL(closed_callback, Run(PaymentsUiClosedReason::kAccepted));
  EXPECT_CALL(*mock_snackbar_controller(),
              Show(AutofillSnackbarType::kResurrectChurnedUsers, _))
      .WillOnce(RunOnceClosure<1>());

  shown_message_model->OnActionClicked();
  shown_message_model->OnDismissed(messages::DismissReason::PRIMARY_ACTION);

  ExpectResultRecorded(PaymentsUiClosedReason::kAccepted);
}

TEST_F(PaymentsChurnedUsersUiDelegateAndroidTest,
       ShowPaymentsChurnedUsersUI_MessageArm_GestureDismissed) {
  InitFeatureWithTreatmentArm(
      AutofillEnableResurrectingPaymentsUsersTreatmentArm::kMessage);

  std::unique_ptr<AutofillMessageModel> shown_message_model;
  EXPECT_CALL(*mock_message_controller(), Show)
      .WillOnce(MoveArg<0>(&shown_message_model));

  base::MockCallback<base::OnceCallback<void(PaymentsUiClosedReason)>>
      closed_callback;

  delegate()->ShowPaymentsChurnedUsersUI(closed_callback.Get());

  ASSERT_TRUE(shown_message_model);
  EXPECT_CALL(closed_callback, Run(PaymentsUiClosedReason::kCancelled));
  EXPECT_CALL(*mock_snackbar_controller(), Show).Times(0);

  shown_message_model->OnDismissed(messages::DismissReason::GESTURE);

  ExpectResultRecorded(PaymentsUiClosedReason::kCancelled);
}

TEST_F(PaymentsChurnedUsersUiDelegateAndroidTest,
       ShowPaymentsChurnedUsersUI_MessageArm_NotInteracted) {
  InitFeatureWithTreatmentArm(
      AutofillEnableResurrectingPaymentsUsersTreatmentArm::kMessage);

  std::unique_ptr<AutofillMessageModel> shown_message_model;
  EXPECT_CALL(*mock_message_controller(), Show)
      .Times(2)
      .WillRepeatedly(MoveArg<0>(&shown_message_model));

  for (messages::DismissReason dismiss_reason :
       {messages::DismissReason::TIMER,
        messages::DismissReason::SCOPE_DESTROYED}) {
    base::MockCallback<base::OnceCallback<void(PaymentsUiClosedReason)>>
        closed_callback;
    delegate()->ShowPaymentsChurnedUsersUI(closed_callback.Get());
    ASSERT_TRUE(shown_message_model);
    EXPECT_CALL(closed_callback, Run(PaymentsUiClosedReason::kNotInteracted));
    EXPECT_CALL(*mock_snackbar_controller(), Show).Times(0);
    shown_message_model->OnDismissed(dismiss_reason);
  }

  ExpectResultRecorded(PaymentsUiClosedReason::kNotInteracted, 2);
}

TEST_F(PaymentsChurnedUsersUiDelegateAndroidTest,
       ShowPaymentsChurnedUsersUI_MessageArm_IgnoresDuplicateWhileShowing) {
  InitFeatureWithTreatmentArm(
      AutofillEnableResurrectingPaymentsUsersTreatmentArm::kMessage);

  std::unique_ptr<AutofillMessageModel> shown_message_model;
  EXPECT_CALL(*mock_message_controller(), Show)
      .WillOnce(MoveArg<0>(&shown_message_model));

  base::MockCallback<base::OnceCallback<void(PaymentsUiClosedReason)>>
      initial_closed_callback;
  delegate()->ShowPaymentsChurnedUsersUI(initial_closed_callback.Get());
  ASSERT_TRUE(shown_message_model);

  // A second request while the first message is still showing must not trigger
  // a duplicate `Show` call on the message controller, overwrite the initial
  // `closed_callback_`, or log a duplicate metric, and returns kUnknown.
  base::MockCallback<base::OnceCallback<void(PaymentsUiClosedReason)>>
      second_closed_callback;
  EXPECT_CALL(second_closed_callback, Run(PaymentsUiClosedReason::kUnknown));
  delegate()->ShowPaymentsChurnedUsersUI(second_closed_callback.Get());

  ExpectShowResultRecorded(
      autofill_metrics::PaymentsChurnedUsersBubbleShowResult::kShown);

  EXPECT_CALL(initial_closed_callback, Run(PaymentsUiClosedReason::kAccepted));
  EXPECT_CALL(*mock_snackbar_controller(),
              Show(AutofillSnackbarType::kResurrectChurnedUsers, _));
  shown_message_model->OnActionClicked();
  shown_message_model->OnDismissed(messages::DismissReason::PRIMARY_ACTION);
}

TEST_F(
    PaymentsChurnedUsersUiDelegateAndroidTest,
    ShowPaymentsChurnedUsersUI_MessageArm_SnackbarCallbackSafeAfterDestruction) {
  InitFeatureWithTreatmentArm(
      AutofillEnableResurrectingPaymentsUsersTreatmentArm::kMessage);

  std::unique_ptr<AutofillMessageModel> shown_message_model;
  EXPECT_CALL(*mock_message_controller(), Show)
      .WillOnce(MoveArg<0>(&shown_message_model));

  delegate()->ShowPaymentsChurnedUsersUI(
      /*closed_callback=*/base::DoNothing());
  ASSERT_TRUE(shown_message_model);

  base::OnceClosure snackbar_action_callback;
  EXPECT_CALL(*mock_snackbar_controller(),
              Show(AutofillSnackbarType::kResurrectChurnedUsers, _))
      .WillOnce(MoveArg<1>(&snackbar_action_callback));

  shown_message_model->OnActionClicked();
  shown_message_model->OnDismissed(messages::DismissReason::PRIMARY_ACTION);
  ASSERT_TRUE(snackbar_action_callback);

  // Destroy the delegate and `WebContents` before the snackbar action callback
  // runs; the `WebContents` `WeakPtr` must safely no-op without crashing.
  ResetDelegate();
  DeleteContents();
  std::move(snackbar_action_callback).Run();
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
          AutofillEnableResurrectingPaymentsUsersTreatmentArm::kSecurity, _));

  delegate()->SetAutofillPaymentsChurnedUsersBottomSheetBridgeForTesting(
      std::move(mock_bridge));

  delegate()->ShowPaymentsChurnedUsersUI(
      /*closed_callback=*/base::DoNothing());
}

TEST_F(PaymentsChurnedUsersUiDelegateAndroidTest,
       ShowPaymentsChurnedUsersUI_PassesCallbackToBridge) {
  InitFeatureWithTreatmentArm(
      AutofillEnableResurrectingPaymentsUsersTreatmentArm::kSecurity);

  auto mock_bridge =
      std::make_unique<MockAutofillPaymentsChurnedUsersBottomSheetBridge>();
  EXPECT_CALL(
      *mock_bridge,
      RequestShowContent(
          AutofillEnableResurrectingPaymentsUsersTreatmentArm::kSecurity, _))
      .WillOnce(RunOnceCallback<1>(PaymentsUiClosedReason::kAccepted));

  delegate()->SetAutofillPaymentsChurnedUsersBottomSheetBridgeForTesting(
      std::move(mock_bridge));

  base::MockCallback<base::OnceCallback<void(PaymentsUiClosedReason)>>
      closed_callback;
  EXPECT_CALL(closed_callback, Run(PaymentsUiClosedReason::kAccepted));

  delegate()->ShowPaymentsChurnedUsersUI(closed_callback.Get());
}

TEST_F(PaymentsChurnedUsersUiDelegateAndroidTest,
       ShowPaymentsChurnedUsersUI_SecurityArm_IgnoresDuplicateWhileShowing) {
  InitFeatureWithTreatmentArm(
      AutofillEnableResurrectingPaymentsUsersTreatmentArm::kSecurity);

  base::OnceCallback<void(PaymentsUiClosedReason)> bridge_callback;
  auto mock_bridge =
      std::make_unique<MockAutofillPaymentsChurnedUsersBottomSheetBridge>();
  EXPECT_CALL(
      *mock_bridge,
      RequestShowContent(
          AutofillEnableResurrectingPaymentsUsersTreatmentArm::kSecurity, _))
      .WillOnce(MoveArg<1>(&bridge_callback));

  delegate()->SetAutofillPaymentsChurnedUsersBottomSheetBridgeForTesting(
      std::move(mock_bridge));

  base::MockCallback<base::OnceCallback<void(PaymentsUiClosedReason)>>
      first_callback;
  delegate()->ShowPaymentsChurnedUsersUI(first_callback.Get());

  // Second request while first is showing is ignored, and returns kUnknown.
  base::MockCallback<base::OnceCallback<void(PaymentsUiClosedReason)>>
      second_callback;
  EXPECT_CALL(second_callback, Run(PaymentsUiClosedReason::kUnknown));
  delegate()->ShowPaymentsChurnedUsersUI(second_callback.Get());

  EXPECT_CALL(first_callback, Run(PaymentsUiClosedReason::kAccepted));
  std::move(bridge_callback).Run(PaymentsUiClosedReason::kAccepted);
}

}  // namespace
}  // namespace autofill::payments

namespace autofill {

TEST(AutofillPaymentsChurnedUsersBottomSheetBridgeTest,
     OnUiAccepted_InvokesCallbackWithAccepted) {
  AutofillPaymentsChurnedUsersBottomSheetBridge bridge(
      /*window_android=*/nullptr);
  base::MockCallback<base::OnceCallback<void(PaymentsUiClosedReason)>> callback;
  EXPECT_CALL(callback, Run(PaymentsUiClosedReason::kAccepted));

  test_api(bridge).SetCallback(callback.Get());
  bridge.OnUiAccepted(/*env=*/nullptr);
}

TEST(AutofillPaymentsChurnedUsersBottomSheetBridgeTest,
     OnUiCanceled_InvokesCallbackWithCancelled) {
  AutofillPaymentsChurnedUsersBottomSheetBridge bridge(
      /*window_android=*/nullptr);
  base::MockCallback<base::OnceCallback<void(PaymentsUiClosedReason)>> callback;
  EXPECT_CALL(callback, Run(PaymentsUiClosedReason::kCancelled));

  test_api(bridge).SetCallback(callback.Get());
  bridge.OnUiCanceled(/*env=*/nullptr);
}

TEST(AutofillPaymentsChurnedUsersBottomSheetBridgeTest,
     OnUiDismissed_InvokesCallbackWithNotInteracted) {
  AutofillPaymentsChurnedUsersBottomSheetBridge bridge(
      /*window_android=*/nullptr);
  base::MockCallback<base::OnceCallback<void(PaymentsUiClosedReason)>> callback;
  EXPECT_CALL(callback, Run(PaymentsUiClosedReason::kNotInteracted));

  test_api(bridge).SetCallback(callback.Get());
  bridge.OnUiDismissed(/*env=*/nullptr);
}

TEST(AutofillPaymentsChurnedUsersBottomSheetBridgeTest,
     OnUiNotShown_InvokesCallbackWithUnknown) {
  AutofillPaymentsChurnedUsersBottomSheetBridge bridge(
      /*window_android=*/nullptr);
  base::MockCallback<base::OnceCallback<void(PaymentsUiClosedReason)>> callback;
  EXPECT_CALL(callback, Run(PaymentsUiClosedReason::kUnknown));

  test_api(bridge).SetCallback(callback.Get());
  bridge.OnUiNotShown(/*env=*/nullptr);
}

TEST(AutofillPaymentsChurnedUsersBottomSheetBridgeTest,
     RequestShowContent_WhenNullJavaObject_InvokesOnUiNotShown) {
  AutofillPaymentsChurnedUsersBottomSheetBridge bridge(
      /*window_android=*/nullptr);
  base::MockCallback<base::OnceCallback<void(PaymentsUiClosedReason)>> callback;
  EXPECT_CALL(callback, Run(PaymentsUiClosedReason::kUnknown));

  bridge.RequestShowContent(
      AutofillEnableResurrectingPaymentsUsersTreatmentArm::kSecurity,
      callback.Get());
}

}  // namespace autofill
