// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/ewallet_account_linking_manager.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/ewallet.h"
#include "components/autofill/core/browser/strike_databases/payments/test_strike_database.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/facilitated_payments/core/browser/ewallet_account_linking_manager_test_api.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/network_api/mock_facilitated_payments_network_interface.h"
#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {
namespace {

using ::testing::_;
using ::testing::Return;

class EwalletAccountLinkingManagerTest : public testing::Test {
 public:
  EwalletAccountLinkingManagerTest() {
    manager_ = std::make_unique<EwalletAccountLinkingManager>(
        &client_, /*api_client_creator=*/
        base::BindRepeating(
            []() -> std::unique_ptr<FacilitatedPaymentsApiClient> {
              return std::make_unique<MockFacilitatedPaymentsApiClient>();
            }),
        autofill::Ewallet(/*instrument_id=*/0, /*nickname=*/u"",
                          /*display_icon_url=*/GURL(),
                          /*ewallet_name=*/u"shopeepay",
                          /*account_display_name=*/u"eWallet",
                          /*supported_payment_link_uris=*/{},
                          /*is_fido_enrolled=*/false));

    payments_network_interface_ =
        std::make_unique<MockFacilitatedPaymentsNetworkInterface>(
            *identity_test_env_.identity_manager(), payments_data_manager_);
  }

  void SetUp() override {
    payments_data_manager_.SetAutofillPaymentMethodsEnabled(true);
    pref_service_ = autofill::test::PrefServiceForTesting();
    payments_data_manager_.SetPrefService(pref_service_.get());
    test_strike_database_ = std::make_unique<autofill::TestStrikeDatabase>();
    ON_CALL(client_, GetStrikeDatabase)
        .WillByDefault(testing::Return(test_strike_database_.get()));
    ON_CALL(client_, GetFacilitatedPaymentsNetworkInterface)
        .WillByDefault(testing::Return(payments_network_interface_.get()));
    ON_CALL(client_, GetPaymentsDataManager)
        .WillByDefault(testing::Return(&payments_data_manager_));
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockFacilitatedPaymentsClient client_;
  signin::IdentityTestEnvironment identity_test_env_;
  autofill::TestPaymentsDataManager payments_data_manager_;
  std::unique_ptr<PrefService> pref_service_;
  std::unique_ptr<autofill::TestStrikeDatabase> test_strike_database_;
  std::unique_ptr<MockFacilitatedPaymentsNetworkInterface>
      payments_network_interface_;
  std::unique_ptr<EwalletAccountLinkingManager> manager_;
  base::HistogramTester histogram_tester_;
};

TEST_F(EwalletAccountLinkingManagerTest, GetHistogramSuffix) {
  EXPECT_EQ("Ewallet", test_api(*manager_).GetHistogramSuffix());
}

TEST_F(EwalletAccountLinkingManagerTest,
       GetPayloadForGetDetailsForCreatePaymentInstrument) {
  base::DictValue payload =
      test_api(*manager_).GetPayloadForGetDetailsForCreatePaymentInstrument();
  const base::DictValue* ewallet_info =
      payload.FindDict("ewallet_account_linking_info");
  ASSERT_TRUE(ewallet_info);
  const std::string* issuer_id = ewallet_info->FindString("issuer_id");
  ASSERT_TRUE(issuer_id);
  EXPECT_EQ(*issuer_id, "shopeepay");
}

TEST_F(EwalletAccountLinkingManagerTest,
       DoOnAccountLinkingResult_InvokesCallback) {
  base::HistogramTester histogram_tester;
  base::MockCallback<base::OnceCallback<void(AccountLinkingResult)>> result_cb;

  AccountLinkingResult expected_result{false, 0,
                                       AccountLinkingResultCode::kResultError};
  EXPECT_CALL(result_cb, Run(expected_result));

  manager_->TriggerAccountLinking(result_cb.Get());

  test_api(*manager_).DoOnAccountLinkingResult(expected_result);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.AccountLinking.Result",
      /*sample=*/false,
      /*expected_bucket_count=*/1);
}

TEST_F(EwalletAccountLinkingManagerTest, DoOnAccountLinkingResult_Canceled) {
  base::HistogramTester histogram_tester;
  test_api(*manager_).DoOnAccountLinkingResult(AccountLinkingResult{
      false, 0, AccountLinkingResultCode::kResultCanceled});
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.AccountLinking.Result",
      /*sample=*/false,
      /*expected_bucket_count=*/1);
}

TEST_F(EwalletAccountLinkingManagerTest, DoOnAccountLinkingResult_Success) {
  base::HistogramTester histogram_tester;
  test_api(*manager_).DoOnGetDetailsForCreatePaymentInstrumentResponse(true);
  test_api(*manager_).ShowAccountLinkingLoadingScreen();
  ASSERT_TRUE(test_api(*manager_).is_progress_screen_showing());

  EXPECT_CALL(client_, DismissPrompt()).Times(0);
  test_api(*manager_).DoOnAccountLinkingResult(
      AccountLinkingResult{true, 12345, AccountLinkingResultCode::kResultOk});
  EXPECT_TRUE(test_api(*manager_).is_hidden());
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.AccountLinking.Result",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

TEST_F(EwalletAccountLinkingManagerTest,
       DoOnAccountLinkingResult_CouldNotInvoke) {
  base::HistogramTester histogram_tester;
  test_api(*manager_).DoOnAccountLinkingResult(AccountLinkingResult{
      false, 0, AccountLinkingResultCode::kCouldNotInvoke});
  // Verify that early exits do not log to the metric.
  histogram_tester.ExpectTotalCount(
      "FacilitatedPayments.Ewallet.AccountLinking.Result", 0);
}

TEST_F(EwalletAccountLinkingManagerTest, DismissAndCancelClearsState) {
  base::MockCallback<base::OnceCallback<void(AccountLinkingResult)>> result_cb;

  EXPECT_CALL(result_cb, Run(_)).Times(0);

  manager_->TriggerAccountLinking(result_cb.Get());

  manager_->DismissAndCancel();

  // Since it was cancelled and the callback was reset, returning a result now
  // should not invoke the callback.
  test_api(*manager_).DoOnAccountLinkingResult(AccountLinkingResult{
      true, /*instrument_id=*/12345, AccountLinkingResultCode::kResultOk});
}

TEST_F(EwalletAccountLinkingManagerTest, DoOnClientTokenReceived) {
  std::vector<uint8_t> client_token = {'t', 'o', 'k', 'e', 'n'};
  base::DictValue expected_payload =
      test_api(*manager_).GetPayloadForGetDetailsForCreatePaymentInstrument();

  EXPECT_CALL(
      *payments_network_interface_,
      GetDetailsForCreatePaymentInstrument(0, client_token, _, _, "en-US"))
      .WillOnce([&expected_payload](long, const std::vector<uint8_t>&,
                                    base::DictValue payload, auto,
                                    const std::string&) {
        EXPECT_EQ(payload, expected_payload);
        return base::StrongAlias<autofill::payments::RequestIdTag,
                                 std::string>();
      });
  test_api(*manager_).DoOnClientTokenReceived(client_token);
}

TEST_F(EwalletAccountLinkingManagerTest,
       DoOnGetDetailsForCreatePaymentInstrumentResponse) {
  base::OnceCallback<void()> on_accepted;
  base::OnceCallback<void()> on_declined;
  base::OnceCallback<void()> on_dismissed;

  EXPECT_CALL(client_,
              ShowAccountLinkingPrompt(
                  testing::AllOf(
                      testing::Field(&AccountLinkingParams::fop_type,
                                     FacilitatedPaymentsType::kEwallet),
                      testing::Field(&AccountLinkingParams::fop_display_name,
                                     u"eWallet"),
                      testing::Field(&AccountLinkingParams::strike_count, 0)),
                  _, _, _))
      .WillOnce([&](const AccountLinkingParams& p,
                    base::OnceCallback<void()> accepted,
                    base::OnceCallback<void()> declined,
                    base::OnceCallback<void()> dismissed) {
        on_accepted = std::move(accepted);
        on_declined = std::move(declined);
        on_dismissed = std::move(dismissed);
      });

  test_api(*manager_).DoOnGetDetailsForCreatePaymentInstrumentResponse(true);

  // Verify that the callbacks are successfully bound to the manager.
  ASSERT_TRUE(on_accepted);
  ASSERT_TRUE(on_declined);
  ASSERT_TRUE(on_dismissed);
}

TEST_F(EwalletAccountLinkingManagerTest,
       DoOnGetDetailsForCreatePaymentInstrumentResponse_Accepted) {
  base::OnceCallback<void()> on_accepted;

  EXPECT_CALL(client_, ShowAccountLinkingPrompt(_, _, _, _))
      .WillOnce([&](const AccountLinkingParams& p,
                    base::OnceCallback<void()> accepted,
                    base::OnceCallback<void()> declined,
                    base::OnceCallback<void()> dismissed) {
        on_accepted = std::move(accepted);
      });

  test_api(*manager_).DoOnGetDetailsForCreatePaymentInstrumentResponse(true);

  ASSERT_TRUE(on_accepted);

  // Since action_token_ is empty after acceptance, kCouldNotInvoke is upgraded
  // to kResultError so PaymentLinkManager can swap in-place to the Error
  // Screen without dismissing the bottom sheet first.
  EXPECT_CALL(client_, DismissPrompt()).Times(0);
  std::move(on_accepted).Run();
  EXPECT_TRUE(test_api(*manager_).is_hidden());
}

TEST_F(EwalletAccountLinkingManagerTest,
       DoOnGetDetailsForCreatePaymentInstrumentResponse_Declined) {
  base::OnceCallback<void()> on_declined;

  EXPECT_CALL(client_, ShowAccountLinkingPrompt(_, _, _, _))
      .WillOnce([&](const AccountLinkingParams& p,
                    base::OnceCallback<void()> accepted,
                    base::OnceCallback<void()> declined,
                    base::OnceCallback<void()> dismissed) {
        on_declined = std::move(declined);
      });

  test_api(*manager_).DoOnGetDetailsForCreatePaymentInstrumentResponse(true);

  ASSERT_TRUE(on_declined);

  EXPECT_CALL(client_, DismissPrompt());
  std::move(on_declined).Run();
}

TEST_F(EwalletAccountLinkingManagerTest,
       DoOnGetDetailsForCreatePaymentInstrumentResponse_Dismissed) {
  base::OnceCallback<void()> on_dismissed;

  EXPECT_CALL(client_, ShowAccountLinkingPrompt(_, _, _, _))
      .WillOnce([&](const AccountLinkingParams& p,
                    base::OnceCallback<void()> accepted,
                    base::OnceCallback<void()> declined,
                    base::OnceCallback<void()> dismissed) {
        on_dismissed = std::move(dismissed);
      });

  test_api(*manager_).DoOnGetDetailsForCreatePaymentInstrumentResponse(true);

  ASSERT_TRUE(on_dismissed);

  EXPECT_CALL(client_, DismissPrompt());
  std::move(on_dismissed).Run();
}

TEST_F(EwalletAccountLinkingManagerTest,
       GetStrikeDatabase_ReturnsValidInstance) {
  auto* strike_db = test_api(*manager_).GetStrikeDatabase();
  ASSERT_NE(strike_db, nullptr);
  EXPECT_EQ(strike_db->GetStrikes(), 0);
}

TEST_F(EwalletAccountLinkingManagerTest,
       GetStrikeDatabase_IncognitoReturnsNullptr) {
  EXPECT_CALL(client_, GetStrikeDatabase()).WillOnce(Return(nullptr));
  EXPECT_EQ(test_api(*manager_).GetStrikeDatabase(), nullptr);
}

TEST_F(EwalletAccountLinkingManagerTest,
       IsUserPrefEnabled_ReadsFromPaymentsDataManager) {
  EXPECT_TRUE(test_api(*manager_).IsUserPrefEnabled());
  autofill::prefs::SetFacilitatedPaymentsEwalletAccountLinking(
      pref_service_.get(), false);
  EXPECT_FALSE(test_api(*manager_).IsUserPrefEnabled());
  autofill::prefs::SetFacilitatedPaymentsEwalletAccountLinking(
      pref_service_.get(), true);
  EXPECT_TRUE(test_api(*manager_).IsUserPrefEnabled());
}

TEST_F(EwalletAccountLinkingManagerTest,
       CreateAccountLinkingParams_PopulatesStrikeCountFromStrikeDatabase) {
  auto* strike_db = test_api(*manager_).GetStrikeDatabase();
  ASSERT_NE(strike_db, nullptr);
  strike_db->AddStrike();
  strike_db->AddStrike();

  auto params = test_api(*manager_).CreateAccountLinkingParams();
  ASSERT_TRUE(params.has_value());
  EXPECT_EQ(params->fop_type, FacilitatedPaymentsType::kEwallet);
  EXPECT_EQ(params->fop_display_name, u"eWallet");
  EXPECT_EQ(params->strike_count, 2);
}

TEST_F(EwalletAccountLinkingManagerTest,
       CreateAccountLinkingParams_NullStrikeDatabase_StrikeCountIsZero) {
  EXPECT_CALL(client_, GetStrikeDatabase()).WillRepeatedly(Return(nullptr));
  auto params = test_api(*manager_).CreateAccountLinkingParams();
  ASSERT_TRUE(params.has_value());
  EXPECT_EQ(params->strike_count, 0);
}

TEST_F(EwalletAccountLinkingManagerTest, DismissAndCancel_InvalidatesWeakPtrs) {
  base::WeakPtr<NativeAccountLinkingHandler> weak_ptr =
      test_api(*manager_).GetWeakPtr();
  EXPECT_TRUE(weak_ptr);
  manager_->DismissAndCancel();
  EXPECT_FALSE(weak_ptr);
}

TEST_F(EwalletAccountLinkingManagerTest,
       DismissAndCancel_DismissesPromptIfShowing) {
  EXPECT_CALL(client_, ShowAccountLinkingPrompt(_, _, _, _));
  test_api(*manager_).DoOnGetDetailsForCreatePaymentInstrumentResponse(true);

  EXPECT_CALL(client_, DismissPrompt());
  manager_->DismissAndCancel();
}

TEST_F(EwalletAccountLinkingManagerTest,
       CanPromptUser_MaxStrikesReached_ReturnsFalseAndLogsHistogram) {
  auto* strike_db = test_api(*manager_).GetStrikeDatabase();
  strike_db->AddStrike();
  strike_db->AddStrike();
  strike_db->AddStrike();
  EXPECT_CALL(client_, HasScreenlockOrBiometricSetup())
      .WillRepeatedly(Return(true));

  EXPECT_FALSE(manager_->CanPromptUser());
  histogram_tester_.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.AccountLinking.FlowExitedReason",
      AccountLinkingFlowExitedReason::kMaxStrikes, 1);
}

TEST_F(EwalletAccountLinkingManagerTest,
       CanPromptUser_RequiredDelayNotPassed_ReturnsFalseAndLogsHistogram) {
  auto* strike_db = test_api(*manager_).GetStrikeDatabase();
  strike_db->AddStrike();
  EXPECT_CALL(client_, HasScreenlockOrBiometricSetup())
      .WillRepeatedly(Return(true));

  EXPECT_FALSE(manager_->CanPromptUser());
  histogram_tester_.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.AccountLinking.FlowExitedReason",
      AccountLinkingFlowExitedReason::kRequiredDelayNotPassed, 1);
}

TEST_F(EwalletAccountLinkingManagerTest,
       CanPromptUser_UserPrefDisabled_ReturnsFalseAndLogsHistogram) {
  autofill::prefs::SetFacilitatedPaymentsEwalletAccountLinking(
      pref_service_.get(), false);
  EXPECT_FALSE(manager_->CanPromptUser());
  histogram_tester_.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.AccountLinking.FlowExitedReason",
      AccountLinkingFlowExitedReason::kUserOptedOut, 1);
}

TEST_F(EwalletAccountLinkingManagerTest,
       CanPromptUser_NoScreenlockOrBiometrics_ReturnsFalseAndLogsHistogram) {
  EXPECT_CALL(client_, HasScreenlockOrBiometricSetup()).WillOnce(Return(false));
  EXPECT_FALSE(manager_->CanPromptUser());
  histogram_tester_.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.AccountLinking.FlowExitedReason",
      AccountLinkingFlowExitedReason::kNoScreenlockOrBiometricSetup, 1);
}

TEST_F(EwalletAccountLinkingManagerTest,
       CanPromptUser_IncognitoNullStrikeDatabase_ReturnsTrue) {
  EXPECT_CALL(client_, GetStrikeDatabase()).WillRepeatedly(Return(nullptr));
  EXPECT_CALL(client_, HasScreenlockOrBiometricSetup()).WillOnce(Return(true));
  EXPECT_TRUE(manager_->CanPromptUser());
  histogram_tester_.ExpectTotalCount(
      "FacilitatedPayments.Ewallet.AccountLinking.FlowExitedReason", 0);
}

TEST_F(EwalletAccountLinkingManagerTest, OnAccepted_ClearsStrikesInDatabase) {
  auto* strike_db = test_api(*manager_).GetStrikeDatabase();
  strike_db->AddStrike();
  strike_db->AddStrike();
  ASSERT_EQ(strike_db->GetStrikes(), 2);

  manager_->OnAccepted();
  EXPECT_EQ(strike_db->GetStrikes(), 0);
}

TEST_F(EwalletAccountLinkingManagerTest, OnDeclined_RecordsStrikeInDatabase) {
  auto* strike_db = test_api(*manager_).GetStrikeDatabase();
  ASSERT_EQ(strike_db->GetStrikes(), 0);

  manager_->OnDeclined();
  EXPECT_EQ(strike_db->GetStrikes(), 1);
  histogram_tester_.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.AccountLinking.FlowExitedReason",
      AccountLinkingFlowExitedReason::kUserDeclined, 1);
}

TEST_F(EwalletAccountLinkingManagerTest, OnDismissed_DoesNotRecordStrike) {
  auto* strike_db = test_api(*manager_).GetStrikeDatabase();
  ASSERT_EQ(strike_db->GetStrikes(), 0);

  EXPECT_CALL(client_, ShowAccountLinkingPrompt(_, _, _, _));
  test_api(*manager_).DoOnGetDetailsForCreatePaymentInstrumentResponse(true);

  manager_->OnDismissed();
  EXPECT_EQ(strike_db->GetStrikes(), 0);
  histogram_tester_.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.AccountLinking.FlowExitedReason",
      AccountLinkingFlowExitedReason::kScreenClosedByUser, 1);
}

TEST_F(EwalletAccountLinkingManagerTest, OnDeclined_PrefRemainsEnabled) {
  EXPECT_TRUE(test_api(*manager_).IsUserPrefEnabled());
  manager_->OnDeclined();
  EXPECT_TRUE(test_api(*manager_).IsUserPrefEnabled());
}

TEST_F(EwalletAccountLinkingManagerTest,
       OnAccepted_UserLoggedOut_ExitedReasonLogged) {
  test_api(*manager_).set_action_token({'a', 'c', 't', 'i', 'o', 'n'});
  EXPECT_CALL(client_, GetCoreAccountInfo()).WillOnce(Return(std::nullopt));
  manager_->OnAccepted();
  histogram_tester_.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.AccountLinking.FlowExitedReason",
      AccountLinkingFlowExitedReason::kUserLoggedOut, 1);
}

TEST_F(EwalletAccountLinkingManagerTest,
       CanPromptUser_StrikeLimitAndPrefDisabled_LogsStrikeLimitReasonFirst) {
  auto* strike_db = test_api(*manager_).GetStrikeDatabase();
  strike_db->AddStrike();
  strike_db->AddStrike();
  strike_db->AddStrike();
  autofill::prefs::SetFacilitatedPaymentsEwalletAccountLinking(
      pref_service_.get(), false);

  EXPECT_FALSE(manager_->CanPromptUser());
  histogram_tester_.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.AccountLinking.FlowExitedReason",
      AccountLinkingFlowExitedReason::kMaxStrikes, 1);
}

TEST_F(EwalletAccountLinkingManagerTest,
       TriggerAccountLinking_CanPromptUserReturnsFalse_DoesNotFetchToken) {
  bool get_api_client_called = false;
  manager_ = std::make_unique<EwalletAccountLinkingManager>(
      &client_,
      base::BindLambdaForTesting(
          [&get_api_client_called]()
              -> std::unique_ptr<FacilitatedPaymentsApiClient> {
            get_api_client_called = true;
            return nullptr;
          }),
      autofill::Ewallet(/*instrument_id=*/0, /*nickname=*/u"",
                        /*display_icon_url=*/GURL(),
                        /*ewallet_name=*/u"shopeepay",
                        /*account_display_name=*/u"eWallet",
                        /*supported_payment_link_uris=*/{},
                        /*is_fido_enrolled=*/false));

  autofill::prefs::SetFacilitatedPaymentsEwalletAccountLinking(
      pref_service_.get(), false);

  base::MockCallback<base::OnceCallback<void(AccountLinkingResult)>> result_callback;
  manager_->TriggerAccountLinking(result_callback.Get());

  EXPECT_FALSE(get_api_client_called);
  histogram_tester_.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.AccountLinking.FlowExitedReason",
      AccountLinkingFlowExitedReason::kUserOptedOut, 1);
}

TEST_F(EwalletAccountLinkingManagerTest,
       TriggerAccountLinking_CanPromptUserReturnsTrue_FetchesToken) {
  bool get_api_client_called = false;
  manager_ = std::make_unique<EwalletAccountLinkingManager>(
      &client_,
      base::BindLambdaForTesting(
          [&get_api_client_called]()
              -> std::unique_ptr<FacilitatedPaymentsApiClient> {
            get_api_client_called = true;
            return nullptr;
          }),
      autofill::Ewallet(/*instrument_id=*/0, /*nickname=*/u"",
                        /*display_icon_url=*/GURL(),
                        /*ewallet_name=*/u"shopeepay",
                        /*account_display_name=*/u"eWallet",
                        /*supported_payment_link_uris=*/{},
                        /*is_fido_enrolled=*/false));

  EXPECT_CALL(client_, HasScreenlockOrBiometricSetup())
      .WillRepeatedly(Return(true));

  base::MockCallback<base::OnceCallback<void(AccountLinkingResult)>> result_callback;
  manager_->TriggerAccountLinking(result_callback.Get());

  EXPECT_TRUE(get_api_client_called);
  histogram_tester_.ExpectTotalCount(
      "FacilitatedPayments.Ewallet.AccountLinking.FlowExitedReason", 0);
}

TEST_F(EwalletAccountLinkingManagerTest,
       DismissAndCancel_WhenPromptHidden_DoesNotCrash) {
  // Client is already hidden.
  EXPECT_CALL(client_, DismissPrompt()).Times(0);

  // Calling it should safely ignore the call (returns early in base class)
  manager_->DismissAndCancel();
}

TEST_F(
    EwalletAccountLinkingManagerTest,
    DoOnGetDetailsForCreatePaymentInstrumentResponse_Eligible_SetsAndRoutesUiEventListener) {
  base::HistogramTester histogram_tester;
  base::RepeatingCallback<void(UiEvent)> captured_listener;
  EXPECT_CALL(client_, SetUiEventListener(testing::_))
      .WillOnce(testing::SaveArg<0>(&captured_listener));
  EXPECT_CALL(client_, ShowAccountLinkingPrompt(testing::_, testing::_,
                                                testing::_, testing::_))
      .Times(1);

  test_api(*manager_).DoOnGetDetailsForCreatePaymentInstrumentResponse(
      /*is_eligible=*/true);
  EXPECT_TRUE(test_api(*manager_).is_prompt_showing());
  ASSERT_TRUE(captured_listener);

  // Firing kNewScreenShown through the captured listener should succeed
  // without crashing and keep ui_state_ in kPrompt.
  captured_listener.Run(UiEvent::kNewScreenShown);
  EXPECT_TRUE(test_api(*manager_).is_prompt_showing());
  histogram_tester.ExpectTotalCount(
      "FacilitatedPayments.Ewallet.AccountLinking.FlowExitedReason", 0);
}

TEST_F(EwalletAccountLinkingManagerTest,
       OnUiScreenEvent_PromptState_LogsExitReasonAndNotifiesResult) {
  struct TestCase {
    UiEvent event;
    AccountLinkingFlowExitedReason expected_reason;
  };
  const TestCase kTestCases[] = {
      {UiEvent::kScreenCouldNotBeShown,
       AccountLinkingFlowExitedReason::kScreenNotShown},
      {UiEvent::kScreenClosedNotByUser,
       AccountLinkingFlowExitedReason::kScreenClosedNotByUser},
      {UiEvent::kScreenClosedByUser,
       AccountLinkingFlowExitedReason::kScreenClosedByUser},
  };

  for (const auto& test_case : kTestCases) {
    base::HistogramTester histogram_tester;
    base::MockCallback<base::OnceCallback<void(AccountLinkingResult)>>
        result_callback;
    EXPECT_CALL(client_, HasScreenlockOrBiometricSetup())
        .WillRepeatedly(Return(true));
    manager_->TriggerAccountLinking(result_callback.Get());

    test_api(*manager_).DoOnGetDetailsForCreatePaymentInstrumentResponse(
        /*is_eligible=*/true);
    ASSERT_TRUE(test_api(*manager_).is_prompt_showing());

    EXPECT_CALL(client_, DismissPrompt()).Times(0);
    EXPECT_CALL(
        result_callback,
        Run(AccountLinkingResult{/*is_successful=*/false, /*instrument_id=*/0,
                                 AccountLinkingResultCode::kCouldNotInvoke}))
        .Times(1);

    test_api(*manager_).OnUiScreenEvent(test_case.event);

    EXPECT_TRUE(test_api(*manager_).is_hidden());
    histogram_tester.ExpectUniqueSample(
        "FacilitatedPayments.Ewallet.AccountLinking.FlowExitedReason",
        test_case.expected_reason, 1);
  }
}

TEST_F(
    EwalletAccountLinkingManagerTest,
    OnUiScreenEvent_ProgressScreenState_TransitionsToHiddenWithoutLoggingPromptExitReason) {
  base::HistogramTester histogram_tester;
  const UiEvent kCloseEvents[] = {
      UiEvent::kScreenCouldNotBeShown,
      UiEvent::kScreenClosedNotByUser,
      UiEvent::kScreenClosedByUser,
  };

  for (UiEvent close_event : kCloseEvents) {
    test_api(*manager_).DoOnGetDetailsForCreatePaymentInstrumentResponse(
        /*is_eligible=*/true);
    test_api(*manager_).ShowAccountLinkingLoadingScreen();
    ASSERT_TRUE(test_api(*manager_).is_progress_screen_showing());

    // kNewScreenShown in progress screen state should keep progress screen
    // state.
    test_api(*manager_).OnUiScreenEvent(UiEvent::kNewScreenShown);
    EXPECT_TRUE(test_api(*manager_).is_progress_screen_showing());

    // Close events while in progress screen state should unconditionally reset
    // ui_state_ to kHidden without calling DismissPrompt() or logging prompt
    // exit reasons.
    EXPECT_CALL(client_, DismissPrompt()).Times(0);
    test_api(*manager_).OnUiScreenEvent(close_event);
    EXPECT_TRUE(test_api(*manager_).is_hidden());
    histogram_tester.ExpectTotalCount(
        "FacilitatedPayments.Ewallet.AccountLinking.FlowExitedReason", 0);
  }
}

TEST_F(EwalletAccountLinkingManagerTest,
       OnAccepted_FailureUpgradesCouldNotInvokeToErrorAndLogsMetrics) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(client_, HasScreenlockOrBiometricSetup())
      .WillRepeatedly(Return(true));

  base::MockCallback<base::OnceCallback<void(AccountLinkingResult)>>
      result_callback;
  manager_->TriggerAccountLinking(result_callback.Get());

  EXPECT_CALL(result_callback, Run(AccountLinkingResult{
                                   /*is_successful=*/false, /*instrument_id=*/0,
                                   AccountLinkingResultCode::kResultError}))
      .Times(1);

  // Calling OnAccepted with empty action_token fails post-acceptance and
  // upgrades kCouldNotInvoke to kResultError.
  manager_->OnAccepted();

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.AccountLinking.FlowExitedReason",
      AccountLinkingFlowExitedReason::kActionTokenNotAvailable, 1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.AccountLinking.Result", false, 1);
}

TEST_F(EwalletAccountLinkingManagerTest,
       OnAccepted_GmsCoreCanceled_PreservesResultCanceled) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(client_, HasScreenlockOrBiometricSetup())
      .WillRepeatedly(Return(true));

  base::MockCallback<base::OnceCallback<void(AccountLinkingResult)>>
      result_callback;
  manager_->TriggerAccountLinking(result_callback.Get());

  // Simulate user accepting prompt, then cancelling inside GMSCore.
  test_api(*manager_).DoOnGetDetailsForCreatePaymentInstrumentResponse(true);
  test_api(*manager_).ShowAccountLinkingLoadingScreen();
  test_api(*manager_).DoOnAccepted();

  EXPECT_CALL(client_, DismissPrompt()).Times(1);
  EXPECT_CALL(result_callback, Run(AccountLinkingResult{
                                   /*is_successful=*/false, /*instrument_id=*/0,
                                   AccountLinkingResultCode::kResultCanceled}))
      .Times(1);

  test_api(*manager_).DoOnAccountLinkingResult(
      AccountLinkingResult{/*is_successful=*/false, /*instrument_id=*/0,
                           AccountLinkingResultCode::kResultCanceled});

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Ewallet.AccountLinking.Result", false, 1);
}

TEST_F(EwalletAccountLinkingManagerTest,
       PreAcceptanceExits_Declined_KeepCouldNotInvoke) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(client_, HasScreenlockOrBiometricSetup())
      .WillRepeatedly(Return(true));

  base::MockCallback<base::OnceCallback<void(AccountLinkingResult)>>
      result_callback;
  manager_->TriggerAccountLinking(result_callback.Get());

  EXPECT_CALL(result_callback, Run(AccountLinkingResult{
                                   /*is_successful=*/false, /*instrument_id=*/0,
                                   AccountLinkingResultCode::kCouldNotInvoke}))
      .Times(1);

  manager_->OnDeclined();
  histogram_tester.ExpectTotalCount(
      "FacilitatedPayments.Ewallet.AccountLinking.Result", 0);
}

}  // namespace
}  // namespace payments::facilitated
