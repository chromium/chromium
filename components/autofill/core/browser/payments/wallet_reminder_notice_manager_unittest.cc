// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/wallet_reminder_notice_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/metrics/payments/wallet_reminder_notice_metrics.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#include "components/autofill/core/browser/ui/payments/wallet_reminder_notice_ui_delegate.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::FieldsAre;
using ::testing::Property;

constexpr char kAppLocale[] = "en-US";
constexpr int64_t kBillingCustomerNumber = 123456789;
constexpr char kAcknowledgementToken[] = "test_acknowledgement_token";

class MockWalletReminderNoticeUiDelegate
    : public WalletReminderNoticeUiDelegate {
 public:
  MockWalletReminderNoticeUiDelegate() = default;
  ~MockWalletReminderNoticeUiDelegate() override = default;

  MOCK_METHOD(void,
              ShowWalletReminderNotice,
              (LegalMessageLines legal_message_lines),
              (override));
};

class PaymentsNetworkInterfaceMock : public PaymentsNetworkInterface {
 public:
  PaymentsNetworkInterfaceMock()
      : PaymentsNetworkInterface(/*url_loader_factory=*/nullptr,
                                 /*identity_manager=*/nullptr,
                                 /*account_info_getter=*/nullptr) {}
  ~PaymentsNetworkInterfaceMock() override = default;

  MOCK_METHOD(
      void,
      GetWalletReminderNotice,
      (const GetWalletReminderNoticeRequestDetails& request_details,
       base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                               const GetWalletReminderNoticeResponseDetails&)>
           callback),
      (override));
  MOCK_METHOD(
      void,
      RecordLegalReminderAcknowledgment,
      (const RecordLegalReminderAcknowledgmentRequestDetails& request_details,
       base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult)>
           callback),
      (override));
};

class WalletReminderNoticeManagerTest : public testing::Test {
 public:
  WalletReminderNoticeManagerTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {autofill::features::kAutofillEnableWalletReminderNotice,
         autofill::features::kAutofillEnableWalletReminderNoticePublicPass},
        /*disabled_features=*/{});

    autofill_client_.set_app_locale(kAppLocale);
    autofill_client_.GetPersonalDataManager()
        .payments_data_manager()
        .SetSyncingForTest(true);
    autofill_client_.GetPersonalDataManager()
        .test_payments_data_manager()
        .SetPaymentsCustomerData(std::make_unique<PaymentsCustomerData>(
            base::NumberToString(kBillingCustomerNumber)));

    auto payments_network_interface =
        std::make_unique<PaymentsNetworkInterfaceMock>();
    payments_network_interface_ = payments_network_interface.get();
    autofill_client_.GetPaymentsAutofillClient()
        ->set_payments_network_interface(std::move(payments_network_interface));

    auto ui_delegate = std::make_unique<MockWalletReminderNoticeUiDelegate>();
    ui_delegate_ = ui_delegate.get();
    autofill_client_.GetPaymentsAutofillClient()
        ->set_wallet_reminder_notice_ui_delegate(std::move(ui_delegate));

    manager_ = std::make_unique<WalletReminderNoticeManager>(&autofill_client_);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
  TestAutofillClient autofill_client_;
  raw_ptr<PaymentsNetworkInterfaceMock> payments_network_interface_;
  raw_ptr<MockWalletReminderNoticeUiDelegate> ui_delegate_;
  std::unique_ptr<WalletReminderNoticeManager> manager_;
};

TEST_F(WalletReminderNoticeManagerTest,
       IsWalletReminderNoticeEligible_CreditCard_FlagOff_NotEligible) {
  feature_list_.Reset();
  feature_list_.InitAndDisableFeature(
      autofill::features::kAutofillEnableWalletReminderNotice);
  autofill_client_.identity_test_environment().MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  EXPECT_FALSE(
      manager_->IsWalletReminderNoticeEligible(test::GetMaskedServerCard()));
}

TEST_F(WalletReminderNoticeManagerTest,
       IsWalletReminderNoticeEligible_CreditCard_LocalCard_NotEligible) {
  autofill_client_.identity_test_environment().MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  EXPECT_FALSE(manager_->IsWalletReminderNoticeEligible(test::GetCreditCard()));
}

TEST_F(WalletReminderNoticeManagerTest,
       IsWalletReminderNoticeEligible_CreditCard_AlreadyShown_NotEligible) {
  base::HistogramTester histogram_tester;
  autofill_client_.identity_test_environment().MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  prefs::SetHasShownWalletReminderNotice(autofill_client_.GetPrefs());
  EXPECT_FALSE(
      manager_->IsWalletReminderNoticeEligible(test::GetMaskedServerCard()));
  histogram_tester.ExpectUniqueSample(
      "Autofill.WalletReminderNotice.ShowResult",
      autofill_metrics::WalletReminderNoticeShowResult::
          kNotShownAlreadyAcknowledgedAccordingToPref,
      1);
}

TEST_F(WalletReminderNoticeManagerTest,
       IsWalletReminderNoticeEligible_CreditCard_Eligible) {
  autofill_client_.identity_test_environment().MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  EXPECT_TRUE(
      manager_->IsWalletReminderNoticeEligible(test::GetMaskedServerCard()));
}

TEST_F(WalletReminderNoticeManagerTest,
       IsWalletReminderNoticeEligible_PublicPass_FlagOff_NotEligible) {
  feature_list_.Reset();
  feature_list_.InitAndDisableFeature(
      autofill::features::kAutofillEnableWalletReminderNoticePublicPass);
  EXPECT_FALSE(manager_->IsWalletReminderNoticeEligible(
      test::GetVehicleEntityInstance(
          {.record_type = EntityInstance::RecordType::kServerWallet})));
}

TEST_F(WalletReminderNoticeManagerTest,
       IsWalletReminderNoticeEligible_PublicPass_PrivatePass_NotEligible) {
  EXPECT_FALSE(manager_->IsWalletReminderNoticeEligible(
      test::GetPassportEntityInstance(
          {.record_type = EntityInstance::RecordType::kServerWallet})));
}

TEST_F(WalletReminderNoticeManagerTest,
       IsWalletReminderNoticeEligible_PublicPass_LocalRecord_NotEligible) {
  EXPECT_FALSE(manager_->IsWalletReminderNoticeEligible(
      test::GetVehicleEntityInstance(
          {.record_type = EntityInstance::RecordType::kLocal})));
}

TEST_F(WalletReminderNoticeManagerTest,
       IsWalletReminderNoticeEligible_PublicPass_ReadOnly_NotEligible) {
  EXPECT_FALSE(manager_->IsWalletReminderNoticeEligible(
      test::GetFlightReservationEntityInstance(
          {.record_type = EntityInstance::RecordType::kServerWallet,
           .are_attributes_read_only =
               EntityInstance::AreAttributesReadOnly(true)})));
}

TEST_F(WalletReminderNoticeManagerTest,
       IsWalletReminderNoticeEligible_PublicPass_AlreadyShown_NotEligible) {
  base::HistogramTester histogram_tester;
  prefs::SetHasShownWalletReminderNotice(autofill_client_.GetPrefs());
  EXPECT_FALSE(manager_->IsWalletReminderNoticeEligible(
      test::GetVehicleEntityInstance(
          {.record_type = EntityInstance::RecordType::kServerWallet})));
  histogram_tester.ExpectUniqueSample(
      "Autofill.WalletReminderNotice.ShowResult",
      autofill_metrics::WalletReminderNoticeShowResult::
          kNotShownAlreadyAcknowledgedAccordingToPref,
      1);
}

TEST_F(WalletReminderNoticeManagerTest,
       IsWalletReminderNoticeEligible_PublicPass_Eligible) {
  EXPECT_TRUE(manager_->IsWalletReminderNoticeEligible(
      test::GetVehicleEntityInstance(
          {.record_type = EntityInstance::RecordType::kServerWallet})));
}

TEST_F(WalletReminderNoticeManagerTest, ShowWalletReminderNotice_CreditCard) {
  EXPECT_CALL(*payments_network_interface_,
              GetWalletReminderNotice(
                  FieldsAre(kAppLocale, kBillingCustomerNumber,
                            kUnmaskPaymentMethodBillableServiceNumber),
                  _));

  manager_->ShowWalletReminderNotice(
      RecordLegalReminderAcknowledgmentRequestDetails::FlowType::
          kChromeDownstream);
}

TEST_F(WalletReminderNoticeManagerTest, ShowWalletReminderNotice_PublicPass) {
  EXPECT_CALL(*payments_network_interface_,
              GetWalletReminderNotice(
                  FieldsAre(kAppLocale, kBillingCustomerNumber,
                            kWalletPassBillableServiceNumber),
                  _));

  manager_->ShowWalletReminderNotice(
      RecordLegalReminderAcknowledgmentRequestDetails::FlowType::kWalletPass);
}

TEST_F(WalletReminderNoticeManagerTest,
       OnGetWalletReminderNoticeResponse_RpcFailure) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*ui_delegate_, ShowWalletReminderNotice(_)).Times(0);
  EXPECT_CALL(*payments_network_interface_,
              RecordLegalReminderAcknowledgment(_, _))
      .Times(0);

  GetWalletReminderNoticeResponseDetails response_details;
  response_details.legal_message_lines.push_back(
      TestLegalMessageLine("Legal message line"));
  response_details.acknowledgement_token = kAcknowledgementToken;
  response_details.has_user_been_shown_reminder = false;

  manager_->OnGetWalletReminderNoticeResponse(
      RecordLegalReminderAcknowledgmentRequestDetails::FlowType::
          kChromeDownstream,
      PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure,
      response_details);

  EXPECT_FALSE(
      prefs::HasShownWalletReminderNotice(autofill_client_.GetPrefs()));
  histogram_tester.ExpectUniqueSample(
      "Autofill.WalletReminderNotice.ShowResult",
      autofill_metrics::WalletReminderNoticeShowResult::
          kNotShownNetworkOrServerError,
      1);
}

TEST_F(
    WalletReminderNoticeManagerTest,
    OnGetWalletReminderNoticeResponse_ServerDenotesUserHasBeenShownReminder) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*ui_delegate_, ShowWalletReminderNotice(_)).Times(0);
  EXPECT_CALL(*payments_network_interface_,
              RecordLegalReminderAcknowledgment(_, _))
      .Times(0);

  GetWalletReminderNoticeResponseDetails response_details;
  response_details.legal_message_lines.push_back(
      TestLegalMessageLine("Legal message line"));
  response_details.acknowledgement_token = kAcknowledgementToken;
  response_details.has_user_been_shown_reminder = true;

  manager_->OnGetWalletReminderNoticeResponse(
      RecordLegalReminderAcknowledgmentRequestDetails::FlowType::
          kChromeDownstream,
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess, response_details);
  histogram_tester.ExpectUniqueSample(
      "Autofill.WalletReminderNotice.ShowResult",
      autofill_metrics::WalletReminderNoticeShowResult::
          kNotShownAlreadyAcknowledgedAccordingToServer,
      1);
}

TEST_F(WalletReminderNoticeManagerTest,
       OnGetWalletReminderNoticeResponse_CreditCard_RpcSuccess) {
  base::HistogramTester histogram_tester;
  LegalMessageLines legal_message_lines;
  legal_message_lines.push_back(TestLegalMessageLine("Legal message line"));

  EXPECT_CALL(*ui_delegate_,
              ShowWalletReminderNotice(ElementsAre(Property(
                  &LegalMessageLine::text, Eq(u"Legal message line")))));
  EXPECT_CALL(*payments_network_interface_,
              RecordLegalReminderAcknowledgment(
                  FieldsAre(kAppLocale, kBillingCustomerNumber,
                            kUnmaskPaymentMethodBillableServiceNumber,
                            kAcknowledgementToken,
                            RecordLegalReminderAcknowledgmentRequestDetails::
                                FlowType::kChromeDownstream),
                  _));

  GetWalletReminderNoticeResponseDetails response_details;
  response_details.legal_message_lines = std::move(legal_message_lines);
  response_details.acknowledgement_token = kAcknowledgementToken;
  response_details.has_user_been_shown_reminder = false;

  manager_->OnGetWalletReminderNoticeResponse(
      RecordLegalReminderAcknowledgmentRequestDetails::FlowType::
          kChromeDownstream,
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess, response_details);
  histogram_tester.ExpectUniqueSample(
      "Autofill.WalletReminderNotice.ShowResult",
      autofill_metrics::WalletReminderNoticeShowResult::kShown, 1);
}

TEST_F(WalletReminderNoticeManagerTest,
       OnGetWalletReminderNoticeResponse_PublicPass_RpcSuccess) {
  base::HistogramTester histogram_tester;
  LegalMessageLines legal_message_lines;
  legal_message_lines.push_back(TestLegalMessageLine("Legal message line"));

  EXPECT_CALL(*ui_delegate_,
              ShowWalletReminderNotice(ElementsAre(Property(
                  &LegalMessageLine::text, u"Legal message line"))));
  EXPECT_CALL(*payments_network_interface_,
              RecordLegalReminderAcknowledgment(
                  FieldsAre(kAppLocale, kBillingCustomerNumber,
                            kWalletPassBillableServiceNumber,
                            kAcknowledgementToken,
                            RecordLegalReminderAcknowledgmentRequestDetails::
                                FlowType::kWalletPass),
                  _));

  GetWalletReminderNoticeResponseDetails response_details;
  response_details.legal_message_lines = std::move(legal_message_lines);
  response_details.acknowledgement_token = kAcknowledgementToken;
  response_details.has_user_been_shown_reminder = false;

  manager_->OnGetWalletReminderNoticeResponse(
      RecordLegalReminderAcknowledgmentRequestDetails::FlowType::kWalletPass,
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess, response_details);
  histogram_tester.ExpectUniqueSample(
      "Autofill.WalletReminderNotice.ShowResult",
      autofill_metrics::WalletReminderNoticeShowResult::kShown, 1);
}

TEST_F(WalletReminderNoticeManagerTest,
       OnRecordLegalReminderAcknowledgmentResponse_RpcFailure) {
  manager_->OnRecordLegalReminderAcknowledgmentResponse(
      PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure);

  EXPECT_FALSE(
      prefs::HasShownWalletReminderNotice(autofill_client_.GetPrefs()));
}

TEST_F(WalletReminderNoticeManagerTest,
       OnRecordLegalReminderAcknowledgmentResponse_RpcSuccess) {
  manager_->OnRecordLegalReminderAcknowledgmentResponse(
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess);

  EXPECT_TRUE(prefs::HasShownWalletReminderNotice(autofill_client_.GetPrefs()));
}

}  // namespace

}  // namespace autofill::payments
