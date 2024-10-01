// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/full_card_request.h"

#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/payments/card_unmask_authentication_metrics.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/sync/test/test_sync_service.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace payments {

using testing::_;
using testing::NiceMock;
using PaymentsRpcCardType = PaymentsAutofillClient::PaymentsRpcCardType;
using PaymentsRpcResult = PaymentsAutofillClient::PaymentsRpcResult;

// The consumer of the full card request API.
class MockResultDelegate : public FullCardRequest::ResultDelegate {
 public:
  MOCK_METHOD(void,
              OnFullCardRequestSucceeded,
              (const payments::FullCardRequest&,
               const CreditCard&,
               const std::u16string&),
              (override));
  MOCK_METHOD(void,
              OnFullCardRequestFailed,
              (CreditCard::RecordType, payments::FullCardRequest::FailureType),
              (override));

  base::WeakPtr<MockResultDelegate> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockResultDelegate> weak_ptr_factory_{this};
};

// The delegate responsible for displaying the unmask prompt UI.
class MockUIDelegate : public FullCardRequest::UIDelegate {
 public:
  MOCK_METHOD(void,
              ShowUnmaskPrompt,
              (const CreditCard&,
               const CardUnmaskPromptOptions&,
               base::WeakPtr<CardUnmaskDelegate>),
              (override));
  MOCK_METHOD(void,
              OnUnmaskVerificationResult,
              (PaymentsRpcResult),
              (override));
#if BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(bool, ShouldOfferFidoAuth, (), (const override));
  MOCK_METHOD(bool,
              UserOptedInToFidoFromSettingsPageOnMobile,
              (),
              (const override));
#endif

  base::WeakPtr<MockUIDelegate> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockUIDelegate> weak_ptr_factory_{this};
};

class MockPaymentsDataManager : public TestPaymentsDataManager {
 public:
  using TestPaymentsDataManager::TestPaymentsDataManager;
  MOCK_METHOD(void,
              UpdateCreditCard,
              (const CreditCard& credit_card),
              (override));
};

// TODO(crbug.com/41412501): Simplify this test setup.
// The test fixture for full card request.
class FullCardRequestTest : public testing::Test {
 public:
  struct FullCardRequestOptions {
    FullCardRequestOptions& with_credit_card(CreditCard cc) {
      credit_card = cc;
      return *this;
    }

    FullCardRequestOptions& with_unmask_card_reason(
        payments::PaymentsAutofillClient::UnmaskCardReason ucr) {
      unmask_card_reason = ucr;
      return *this;
    }

    CreditCard credit_card;
    payments::PaymentsAutofillClient::UnmaskCardReason unmask_card_reason =
        payments::PaymentsAutofillClient::UnmaskCardReason::kAutofill;
  };

  FullCardRequestTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    personal_data_.set_payments_data_manager(
        std::make_unique<MockPaymentsDataManager>());
    personal_data_.SetPrefService(autofill_client_.GetPrefs());
    personal_data_.SetSyncServiceForTest(&sync_service_);
    payments_network_interface_ = std::make_unique<PaymentsNetworkInterface>(
        test_shared_loader_factory_, autofill_client_.GetIdentityManager(),
        &personal_data_.payments_data_manager());
    request_ = std::make_unique<FullCardRequest>(
        &autofill_client_, payments_network_interface_.get(), &personal_data_);
    personal_data_.test_payments_data_manager().SetAccountInfoForPayments(
        autofill_client_.GetIdentityManager()->GetPrimaryAccountInfo(
            signin::ConsentLevel::kSync));
    // Silence the warning from PaymentsNetworkInterface about matching sync and
    // Payments server types.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        "sync-url", "https://google.com");
  }

  void TearDown() override {
    // Order of destruction is important as AutofillDriver relies on
    // PersonalDataManager to be around when it gets destroyed.
    personal_data_.SetPrefService(nullptr);
  }

  FullCardRequestTest(const FullCardRequestTest&) = delete;
  FullCardRequestTest& operator=(const FullCardRequestTest&) = delete;

  ~FullCardRequestTest() override = default;

  TestPersonalDataManager* personal_data() { return &personal_data_; }

  FullCardRequest* request() { return request_.get(); }

  CardUnmaskDelegate* card_unmask_delegate() {
    return static_cast<CardUnmaskDelegate*>(request_.get());
  }

  MockResultDelegate* result_delegate() { return &result_delegate_; }

  MockUIDelegate* ui_delegate() { return &ui_delegate_; }

  TestAutofillClient* autofill_client() { return &autofill_client_; }

  void OnDidGetRealPan(PaymentsRpcResult result,
                       const std::string& real_pan,
                       bool is_virtual_card = false) {
    payments::PaymentsNetworkInterface::UnmaskResponseDetails response;
    response.card_type = is_virtual_card ? PaymentsRpcCardType::kVirtualCard
                                         : PaymentsRpcCardType::kServerCard;
    request_->OnDidGetRealPan(result, response.with_real_pan(real_pan));
  }

  void OnDidGetRealPanWithDcvv(PaymentsRpcResult result,
                               const std::string& real_pan,
                               const std::string& dcvv,
                               bool is_virtual_card = false) {
    payments::PaymentsNetworkInterface::UnmaskResponseDetails response;
    response.card_type = is_virtual_card ? PaymentsRpcCardType::kVirtualCard
                                         : PaymentsRpcCardType::kServerCard;
    request_->OnDidGetRealPan(result,
                              response.with_real_pan(real_pan).with_dcvv(dcvv));
  }

  void MakeGetFullCardRequest(FullCardRequestOptions options) {
    request()->GetFullCard(options.credit_card, options.unmask_card_reason,
                           result_delegate()->AsWeakPtr(),
                           ui_delegate()->AsWeakPtr());
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  syncer::TestSyncService sync_service_;
  TestPersonalDataManager personal_data_;
  MockResultDelegate result_delegate_;
  MockUIDelegate ui_delegate_;
  TestAutofillClient autofill_client_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<PaymentsNetworkInterface> payments_network_interface_;
  std::unique_ptr<FullCardRequest> request_;
};

// Matches the |arg| credit card to the given |record_type| and |card_number|.
MATCHER_P2(CardMatches, record_type, card_number, "") {
  return arg.record_type() == record_type &&
         arg.GetRawInfo(CREDIT_CARD_NUMBER) == base::ASCIIToUTF16(card_number);
}

// Matches the |arg| credit card to the given `record_type`, `card_number`, and
// `cvc`.
MATCHER_P3(CardMatches, record_type, card_number, cvc, "") {
  return arg.record_type() == record_type &&
         arg.GetRawInfo(CREDIT_CARD_NUMBER) ==
             base::ASCIIToUTF16(card_number) &&
         arg.cvc() == cvc;
}

// Matches the |arg| credit card to the given |record_type|, card |number|,
// expiration |month|, and expiration |year|.
MATCHER_P4(CardMatches, record_type, number, month, year, "") {
  return arg.record_type() == record_type &&
         arg.GetRawInfo(CREDIT_CARD_NUMBER) == base::ASCIIToUTF16(number) &&
         arg.GetRawInfo(CREDIT_CARD_EXP_MONTH) == base::ASCIIToUTF16(month) &&
         arg.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR) ==
             base::ASCIIToUTF16(year);
}

// Verify getting the full PAN and the CVC for a masked server card.
TEST_F(FullCardRequestTest, GetFullCardPanAndCvcForMaskedServerCardViaCvc) {
  EXPECT_CALL(*result_delegate(),
              OnFullCardRequestSucceeded(
                  testing::Ref(*request()),
                  CardMatches(CreditCard::RecordType::kFullServerCard, "4111"),
                  testing::Eq(u"123")));
  EXPECT_CALL(*ui_delegate(), ShowUnmaskPrompt(_, _, _));
  EXPECT_CALL(*ui_delegate(),
              OnUnmaskVerificationResult(PaymentsRpcResult::kSuccess));

  MakeGetFullCardRequest(FullCardRequestOptions().with_credit_card(
      CreditCard(CreditCard::RecordType::kMaskedServerCard, "server_id")));
  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  OnDidGetRealPan(PaymentsRpcResult::kSuccess, "4111");
}

// Verify full PAN and dCVV are both used when returned by the server.
TEST_F(FullCardRequestTest, GetFullCardPanAndDcvvForMaskedServerCardViaDcvv) {
  EXPECT_CALL(*result_delegate(),
              OnFullCardRequestSucceeded(
                  testing::Ref(*request()),
                  CardMatches(CreditCard::RecordType::kFullServerCard, "4111"),
                  testing::Eq(u"321")));
  EXPECT_CALL(*ui_delegate(), ShowUnmaskPrompt(_, _, _));
  EXPECT_CALL(*ui_delegate(),
              OnUnmaskVerificationResult(PaymentsRpcResult::kSuccess));

  MakeGetFullCardRequest(FullCardRequestOptions().with_credit_card(
      CreditCard(CreditCard::RecordType::kMaskedServerCard, "server_id")));
  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  OnDidGetRealPanWithDcvv(PaymentsRpcResult::kSuccess, "4111", "321");
}

// Verify getting the full PAN for a masked server card.
TEST_F(FullCardRequestTest, GetFullCardPanAndCvcForMaskedServerCardViaFido) {
  EXPECT_CALL(*result_delegate(),
              OnFullCardRequestSucceeded(
                  testing::Ref(*request()),
                  CardMatches(CreditCard::RecordType::kFullServerCard, "4111"),
                  testing::Eq(u"")));

  request()->GetFullCardViaFIDO(
      CreditCard(CreditCard::RecordType::kMaskedServerCard, "server_id"),
      payments::PaymentsAutofillClient::UnmaskCardReason::kAutofill,
      result_delegate()->AsWeakPtr(), base::Value::Dict());
  OnDidGetRealPan(PaymentsRpcResult::kSuccess, "4111");
}

// Verify getting the CVC for a local card.
TEST_F(FullCardRequestTest, GetFullCardPanAndCvcForLocalCard) {
  EXPECT_CALL(*result_delegate(),
              OnFullCardRequestSucceeded(
                  testing::Ref(*request()),
                  CardMatches(CreditCard::RecordType::kLocalCard, "4111"),
                  testing::Eq(u"123")));
  EXPECT_CALL(*ui_delegate(), ShowUnmaskPrompt(_, _, _));
  EXPECT_CALL(*ui_delegate(),
              OnUnmaskVerificationResult(PaymentsRpcResult::kSuccess));

  CreditCard card;
  test::SetCreditCardInfo(&card, nullptr, "4111", "12", "2050", "1");
  MakeGetFullCardRequest(FullCardRequestOptions().with_credit_card(card));
  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
}

// Verify getting the CVC for a masked server card with expiration date in the past.
TEST_F(FullCardRequestTest, GetFullCardPanAndCvcForExpiredMaskedServerCard) {
  EXPECT_CALL(*result_delegate(),
              OnFullCardRequestSucceeded(
                  testing::Ref(*request()),
                  CardMatches(CreditCard::RecordType::kFullServerCard, "4111",
                              "12", "2051"),
                  testing::Eq(u"123")));
  EXPECT_CALL(*ui_delegate(), ShowUnmaskPrompt(_, _, _));
  EXPECT_CALL(*ui_delegate(),
              OnUnmaskVerificationResult(PaymentsRpcResult::kSuccess));

  base::Time::Exploded today;
  AutofillClock::Now().LocalExplode(&today);
  CreditCard full_server_card(CreditCard::RecordType::kMaskedServerCard,
                              "server_id");
  test::SetCreditCardInfo(&full_server_card, nullptr, "4111", "12",
                          base::StringPrintf("%d", today.year - 1).c_str(),
                          "1");
  MakeGetFullCardRequest(
      FullCardRequestOptions().with_credit_card(full_server_card));
  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  details.exp_year = u"2051";
  details.exp_month = u"12";
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  OnDidGetRealPan(PaymentsRpcResult::kSuccess, "4111");
}

// Verify getting the full PAN, the expiration and the dCVV for a virtual card
// using CVC authentication.
// TODO(crbug.com/40241969): Add a FullCardRequest test case for Virtual Card
// retrieval via FIDO as well.
TEST_F(FullCardRequestTest,
       GetFullCardPanAndExpirationAndDcvvForVirtualCardViaCvc) {
  EXPECT_CALL(
      *result_delegate(),
      OnFullCardRequestSucceeded(
          testing::Ref(*request()),
          CardMatches(CreditCard::RecordType::kVirtualCard, "4111", u"123"),
          testing::Eq(u"123")))
      .Times(1);
  EXPECT_CALL(*ui_delegate(), ShowUnmaskPrompt(_, _, _)).Times(1);
  EXPECT_CALL(*ui_delegate(),
              OnUnmaskVerificationResult(PaymentsRpcResult::kSuccess))
      .Times(1);

  CreditCard card;
  card.set_record_type(CreditCard::RecordType::kVirtualCard);
  card.set_server_id("server_id");
  CardUnmaskChallengeOption challenge_option =
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kCvc})[0];
  request()->GetFullVirtualCardViaCVC(
      card, payments::PaymentsAutofillClient::UnmaskCardReason::kAutofill,
      result_delegate()->AsWeakPtr(), ui_delegate()->AsWeakPtr(),
      GURL("https://example.com/"), "test_context_token", challenge_option);
  ASSERT_TRUE(request()->GetShouldUnmaskCardForTesting());
  payments::PaymentsNetworkInterface::UnmaskRequestDetails* request_details =
      request()->GetUnmaskRequestDetailsForTesting();
  EXPECT_EQ(request_details->selected_challenge_option->type,
            CardUnmaskChallengeOptionType::kCvc);
  EXPECT_EQ(request_details->selected_challenge_option->id.value(),
            challenge_option.id.value());
  EXPECT_EQ(request_details->context_token, "test_context_token");
  EXPECT_EQ(request_details->last_committed_primary_main_frame_origin->spec(),
            GURL("https://example.com/").spec());

  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  details.exp_month = u"12";
  details.exp_year = base::UTF8ToUTF16(test::NextYear());
  details.enable_fido_auth = false;
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  payments::PaymentsNetworkInterface::UnmaskResponseDetails response;
  response.real_pan = "4111";
  response.dcvv = "123";
  response.expiration_month = "12";
  response.expiration_year = test::NextYear();
  response.card_type = PaymentsRpcCardType::kVirtualCard;
  request()->OnDidGetRealPan(PaymentsRpcResult::kSuccess, response);
}

// Only one request at a time should be allowed.
TEST_F(FullCardRequestTest, OneRequestAtATime) {
  EXPECT_CALL(
      *result_delegate(),
      OnFullCardRequestFailed(CreditCard::RecordType::kMaskedServerCard,
                              FullCardRequest::FailureType::GENERIC_FAILURE));
  EXPECT_CALL(*ui_delegate(), ShowUnmaskPrompt(_, _, _));
  EXPECT_CALL(*ui_delegate(), OnUnmaskVerificationResult(_)).Times(0);

  MakeGetFullCardRequest(
      FullCardRequestOptions()
          .with_credit_card(CreditCard(
              CreditCard::RecordType::kMaskedServerCard, "server_id_1"))
          .with_unmask_card_reason(
              payments::PaymentsAutofillClient::UnmaskCardReason::kAutofill));
  MakeGetFullCardRequest(
      FullCardRequestOptions()
          .with_credit_card(CreditCard(
              CreditCard::RecordType::kMaskedServerCard, "server_id_2"))
          .with_unmask_card_reason(payments::PaymentsAutofillClient::
                                       UnmaskCardReason::kPaymentRequest));
}

// After the first request completes, it's OK to start the second request.
TEST_F(FullCardRequestTest, SecondRequestOkAfterFirstFinished) {
  EXPECT_CALL(*result_delegate(), OnFullCardRequestFailed(_, _)).Times(0);
  EXPECT_CALL(*result_delegate(),
              OnFullCardRequestSucceeded(
                  testing::Ref(*request()),
                  CardMatches(CreditCard::RecordType::kLocalCard, "4111"),
                  testing::Eq(u"123")))
      .Times(2);
  EXPECT_CALL(*ui_delegate(), ShowUnmaskPrompt(_, _, _)).Times(2);
  EXPECT_CALL(*ui_delegate(),
              OnUnmaskVerificationResult(PaymentsRpcResult::kSuccess))
      .Times(2);

  CreditCard card;
  test::SetCreditCardInfo(&card, nullptr, "4111", "12", "2050", "1");
  MakeGetFullCardRequest(FullCardRequestOptions().with_credit_card(card));
  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);

  MakeGetFullCardRequest(FullCardRequestOptions().with_credit_card(card));
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
}

// If the user cancels the CVC prompt,
// FullCardRequest::Delegate::OnFullCardRequestFailed() should be invoked.
TEST_F(FullCardRequestTest, ClosePromptWithoutUserInput) {
  EXPECT_CALL(
      *result_delegate(),
      OnFullCardRequestFailed(CreditCard::RecordType::kMaskedServerCard,
                              FullCardRequest::FailureType::PROMPT_CLOSED));
  EXPECT_CALL(*ui_delegate(), ShowUnmaskPrompt(_, _, _));
  EXPECT_CALL(*ui_delegate(), OnUnmaskVerificationResult(_)).Times(0);

  MakeGetFullCardRequest(FullCardRequestOptions().with_credit_card(
      CreditCard(CreditCard::RecordType::kMaskedServerCard, "server_id")));
  card_unmask_delegate()->OnUnmaskPromptCancelled();
}

// If the server provides an empty PAN with PERMANENT_FAILURE error,
// FullCardRequest::Delegate::OnFullCardRequestFailed() should be invoked.
TEST_F(FullCardRequestTest, PermanentFailure) {
  EXPECT_CALL(*result_delegate(),
              OnFullCardRequestFailed(
                  CreditCard::RecordType::kMaskedServerCard,
                  FullCardRequest::FailureType::VERIFICATION_DECLINED));
  EXPECT_CALL(*ui_delegate(), ShowUnmaskPrompt(_, _, _));
  EXPECT_CALL(*ui_delegate(),
              OnUnmaskVerificationResult(PaymentsRpcResult::kPermanentFailure));

  MakeGetFullCardRequest(FullCardRequestOptions().with_credit_card(
      CreditCard(CreditCard::RecordType::kMaskedServerCard, "server_id")));
  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  OnDidGetRealPan(PaymentsRpcResult::kPermanentFailure, "");
}

// If the server provides an empty PAN with VCN_RETRIEVAL_TRY_AGAIN_FAILURE
// error, FullCardRequest::Delegate::OnFullCardRequestFailed() should be
// invoked.
TEST_F(FullCardRequestTest, VcnRetrievalTemporaryFailure) {
  EXPECT_CALL(
      *result_delegate(),
      OnFullCardRequestFailed(CreditCard::RecordType::kVirtualCard,
                              FullCardRequest::FailureType::
                                  VIRTUAL_CARD_RETRIEVAL_TRANSIENT_FAILURE));
  EXPECT_CALL(*ui_delegate(), ShowUnmaskPrompt(_, _, _));
  EXPECT_CALL(*ui_delegate(),
              OnUnmaskVerificationResult(
                  PaymentsRpcResult::kVcnRetrievalTryAgainFailure));

  CreditCard card;
  card.set_record_type(CreditCard::RecordType::kVirtualCard);
  request()->GetFullVirtualCardViaCVC(
      card, payments::PaymentsAutofillClient::UnmaskCardReason::kAutofill,
      result_delegate()->AsWeakPtr(), ui_delegate()->AsWeakPtr(),
      GURL("https://example.com/"), "test_context_token",
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kCvc})[0]);
  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  OnDidGetRealPan(PaymentsRpcResult::kVcnRetrievalTryAgainFailure, "",
                  /*is_virtual_card=*/true);
}

// If the server provides an empty PAN with VCN_RETRIEVAL_PERMANENT_FAILURE
// error, FullCardRequest::Delegate::OnFullCardRequestFailed() should be
// invoked.
TEST_F(FullCardRequestTest, VcnRetrievalPermanentFailure) {
  EXPECT_CALL(
      *result_delegate(),
      OnFullCardRequestFailed(CreditCard::RecordType::kVirtualCard,
                              FullCardRequest::FailureType::
                                  VIRTUAL_CARD_RETRIEVAL_PERMANENT_FAILURE));
  EXPECT_CALL(*ui_delegate(), ShowUnmaskPrompt(_, _, _));
  EXPECT_CALL(*ui_delegate(),
              OnUnmaskVerificationResult(
                  PaymentsRpcResult::kVcnRetrievalPermanentFailure));

  CreditCard card;
  card.set_record_type(CreditCard::RecordType::kVirtualCard);
  request()->GetFullVirtualCardViaCVC(
      card, payments::PaymentsAutofillClient::UnmaskCardReason::kAutofill,
      result_delegate()->AsWeakPtr(), ui_delegate()->AsWeakPtr(),
      GURL("https://example.com/"), "test_context_token",
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kCvc})[0]);
  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  OnDidGetRealPan(PaymentsRpcResult::kVcnRetrievalPermanentFailure, "",
                  /*is_virtual_card=*/true);
}

// If the server provides an empty PAN with NETWORK_ERROR error,
// FullCardRequest::Delegate::OnFullCardRequestFailed() should be invoked.
TEST_F(FullCardRequestTest, NetworkError) {
  EXPECT_CALL(
      *result_delegate(),
      OnFullCardRequestFailed(CreditCard::RecordType::kMaskedServerCard,
                              FullCardRequest::FailureType::GENERIC_FAILURE));
  EXPECT_CALL(*ui_delegate(), ShowUnmaskPrompt(_, _, _));
  EXPECT_CALL(*ui_delegate(),
              OnUnmaskVerificationResult(PaymentsRpcResult::kNetworkError));

  MakeGetFullCardRequest(FullCardRequestOptions().with_credit_card(
      CreditCard(CreditCard::RecordType::kMaskedServerCard, "server_id")));
  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  OnDidGetRealPan(PaymentsRpcResult::kNetworkError, "");
}

// If the server provides an empty PAN with TRY_AGAIN_FAILURE, the user can
// manually cancel out of the dialog.
TEST_F(FullCardRequestTest, TryAgainFailureGiveUp) {
  // We test all possible cases of a temporary error.
  for (autofill_metrics::CvcAuthEvent test_event :
       {autofill_metrics::CvcAuthEvent::kTemporaryErrorCvcMismatch,
        autofill_metrics::CvcAuthEvent::kTemporaryErrorExpiredCard}) {
    SCOPED_TRACE(::testing::Message()
                 << "Iteration " << static_cast<int>(test_event));
    base::HistogramTester histogram_tester;
    EXPECT_CALL(
        *result_delegate(),
        OnFullCardRequestFailed(CreditCard::RecordType::kMaskedServerCard,
                                FullCardRequest::FailureType::PROMPT_CLOSED));
    EXPECT_CALL(*ui_delegate(), ShowUnmaskPrompt(_, _, _));
    EXPECT_CALL(*ui_delegate(), OnUnmaskVerificationResult(
                                    PaymentsRpcResult::kTryAgainFailure));
    CreditCard card = test::GetMaskedServerCard();
    if (test_event ==
        autofill_metrics::CvcAuthEvent::kTemporaryErrorExpiredCard) {
      card.SetExpirationMonth(01);
      card.SetExpirationYear(2016);
    }
    MakeGetFullCardRequest(FullCardRequestOptions().with_credit_card(card));
    CardUnmaskDelegate::UserProvidedUnmaskDetails details;
    details.cvc = u"123";
    card_unmask_delegate()->OnUnmaskPromptAccepted(details);
    OnDidGetRealPan(PaymentsRpcResult::kTryAgainFailure, "");
    card_unmask_delegate()->OnUnmaskPromptCancelled();
    histogram_tester.ExpectUniqueSample(
        "Autofill.CvcAuth.ServerCard.RetryableError", test_event, 1);
  }
}

// If the server provides an empty PAN with TRY_AGAIN_FAILURE, the user can
// correct their mistake and resubmit.
TEST_F(FullCardRequestTest, ServerCardTryAgainFailure) {
  // We test all possible cases of a temporary error.
  for (autofill_metrics::CvcAuthEvent test_event :
       {autofill_metrics::CvcAuthEvent::kTemporaryErrorCvcMismatch,
        autofill_metrics::CvcAuthEvent::kTemporaryErrorExpiredCard}) {
    SCOPED_TRACE(::testing::Message()
                 << "Iteration " << static_cast<int>(test_event));
    base::HistogramTester histogram_tester;
    EXPECT_CALL(*result_delegate(), OnFullCardRequestFailed(_, _)).Times(0);
    EXPECT_CALL(
        *result_delegate(),
        OnFullCardRequestSucceeded(
            testing::Ref(*request()),
            CardMatches(CreditCard::RecordType::kFullServerCard, "4111"),
            testing::Eq(u"123")));
    EXPECT_CALL(*ui_delegate(), ShowUnmaskPrompt(_, _, _));
    EXPECT_CALL(*ui_delegate(), OnUnmaskVerificationResult(
                                    PaymentsRpcResult::kTryAgainFailure));
    EXPECT_CALL(*ui_delegate(),
                OnUnmaskVerificationResult(PaymentsRpcResult::kSuccess));

    CreditCard card = test::GetMaskedServerCard();
    if (test_event ==
        autofill_metrics::CvcAuthEvent::kTemporaryErrorExpiredCard) {
      card.SetExpirationMonth(01);
      card.SetExpirationYear(2016);
    }
    MakeGetFullCardRequest(FullCardRequestOptions().with_credit_card(card));
    CardUnmaskDelegate::UserProvidedUnmaskDetails details;
    details.cvc = u"789";
    card_unmask_delegate()->OnUnmaskPromptAccepted(details);
    OnDidGetRealPan(PaymentsRpcResult::kTryAgainFailure, "");
    details.cvc = u"123";
    card_unmask_delegate()->OnUnmaskPromptAccepted(details);
    OnDidGetRealPan(PaymentsRpcResult::kSuccess, "4111");
    histogram_tester.ExpectUniqueSample(
        "Autofill.CvcAuth.ServerCard.RetryableError", test_event, 1);
  }
}

// If the server provides an empty PAN with TRY_AGAIN_FAILURE for virtual card,
// ensure it is handled the same way as a regular try again case.
TEST_F(FullCardRequestTest, VirtualCardTryAgainFailure) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*ui_delegate(), ShowUnmaskPrompt(_, _, _)).Times(1);
  EXPECT_CALL(*ui_delegate(),
              OnUnmaskVerificationResult(PaymentsRpcResult::kTryAgainFailure))
      .Times(1);

  CardUnmaskChallengeOption challenge_option =
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kCvc})[0];
  request()->GetFullVirtualCardViaCVC(
      test::GetVirtualCard(),
      payments::PaymentsAutofillClient::UnmaskCardReason::kAutofill,
      result_delegate()->AsWeakPtr(), ui_delegate()->AsWeakPtr(),
      GURL("https://example.com/"), "test_context_token", challenge_option);
  CardUnmaskDelegate::UserProvidedUnmaskDetails user_provided_details;
  user_provided_details.cvc = u"321";
  card_unmask_delegate()->OnUnmaskPromptAccepted(user_provided_details);
  PaymentsNetworkInterface::UnmaskResponseDetails response_details;
  response_details.card_type = PaymentsRpcCardType::kVirtualCard;
  response_details.context_token = "test_context_token";
  request()->OnDidGetRealPan(PaymentsRpcResult::kTryAgainFailure,
                             response_details);
  EXPECT_EQ(request()->GetUnmaskRequestDetailsForTesting()->context_token,
            "test_context_token");
  EXPECT_EQ(request()
                ->GetUnmaskRequestDetailsForTesting()
                ->selected_challenge_option->id.value(),
            challenge_option.id.value());
  EXPECT_EQ(request()
                ->GetUnmaskRequestDetailsForTesting()
                ->selected_challenge_option->type,
            CardUnmaskChallengeOptionType::kCvc);
  EXPECT_EQ(request()
                ->GetUnmaskRequestDetailsForTesting()
                ->last_committed_primary_main_frame_origin->spec(),
            "https://example.com/");
  histogram_tester.ExpectUniqueSample(
      "Autofill.CvcAuth.VirtualCard.RetryableError",
      autofill_metrics::CvcAuthEvent::kTemporaryErrorCvcMismatch, 1);
}

// Verify updating expiration date for a masked server card.
TEST_F(FullCardRequestTest, UpdateExpDateForMaskedServerCard) {
  EXPECT_CALL(*result_delegate(),
              OnFullCardRequestSucceeded(
                  testing::Ref(*request()),
                  CardMatches(CreditCard::RecordType::kFullServerCard, "4111",
                              "12", "2050"),
                  testing::Eq(u"123")));
  EXPECT_CALL(*ui_delegate(), ShowUnmaskPrompt(_, _, _));
  EXPECT_CALL(*ui_delegate(),
              OnUnmaskVerificationResult(PaymentsRpcResult::kSuccess));

  MakeGetFullCardRequest(FullCardRequestOptions().with_credit_card(
      CreditCard(CreditCard::RecordType::kMaskedServerCard, "server_id")));
  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  details.exp_month = u"12";
  details.exp_year = u"2050";
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  OnDidGetRealPan(PaymentsRpcResult::kSuccess, "4111");
}

// Verify updating expiration date for a local card.
TEST_F(FullCardRequestTest, UpdateExpDateForLocalCard) {
  EXPECT_CALL(
      *result_delegate(),
      OnFullCardRequestSucceeded(
          testing::Ref(*request()),
          CardMatches(CreditCard::RecordType::kLocalCard, "4111", "12", "2051"),
          testing::Eq(u"123")));
  EXPECT_CALL(*ui_delegate(), ShowUnmaskPrompt(_, _, _));
  EXPECT_CALL(static_cast<MockPaymentsDataManager&>(
                  personal_data()->payments_data_manager()),
              UpdateCreditCard(CardMatches(CreditCard::RecordType::kLocalCard,
                                           "4111", "12", "2051")));
  EXPECT_CALL(*ui_delegate(),
              OnUnmaskVerificationResult(PaymentsRpcResult::kSuccess));

  base::Time::Exploded today;
  AutofillClock::Now().LocalExplode(&today);
  CreditCard card;
  test::SetCreditCardInfo(&card, nullptr, "4111", "10",
                          base::StringPrintf("%d", today.year - 1).c_str(),
                          "1");
  MakeGetFullCardRequest(FullCardRequestOptions().with_credit_card(card));
  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  details.exp_month = u"12";
  details.exp_year = u"2051";
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
}

// Verify getting full PAN and CVC for PaymentRequest.
TEST_F(FullCardRequestTest, UnmaskForPaymentRequest) {
  EXPECT_CALL(*result_delegate(),
              OnFullCardRequestSucceeded(
                  testing::Ref(*request()),
                  CardMatches(CreditCard::RecordType::kFullServerCard, "4111"),
                  testing::Eq(u"123")));
  EXPECT_CALL(*ui_delegate(), ShowUnmaskPrompt(_, _, _));
  EXPECT_CALL(*ui_delegate(),
              OnUnmaskVerificationResult(PaymentsRpcResult::kSuccess));

  MakeGetFullCardRequest(
      FullCardRequestOptions()
          .with_credit_card(CreditCard(
              CreditCard::RecordType::kMaskedServerCard, "server_id"))
          .with_unmask_card_reason(payments::PaymentsAutofillClient::
                                       UnmaskCardReason::kPaymentRequest));
  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  OnDidGetRealPan(PaymentsRpcResult::kSuccess, "4111");
}

// Params of the FullCardRequestCardMetadataTest:
// -- bool card_name_available;
// -- bool card_art_available;
// -- bool metadata_enabled;
class FullCardRequestCardMetadataTest
    : public FullCardRequestTest,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  FullCardRequestCardMetadataTest() = default;
  ~FullCardRequestCardMetadataTest() override = default;

  bool CardNameAvailable() { return std::get<0>(GetParam()); }
  bool CardArtAvailable() { return std::get<1>(GetParam()); }
  bool MetadataEnabled() { return std::get<2>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(,
                         FullCardRequestCardMetadataTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

// Verify the metadata signal is correctly set in the unmask request.
TEST_P(FullCardRequestCardMetadataTest, MetadataSignal) {
  base::test::ScopedFeatureList metadata_feature_list;
  CreditCard card = test::GetMaskedServerCard();
  if (MetadataEnabled()) {
    metadata_feature_list.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillEnableCardProductName,
                              features::kAutofillEnableCardArtImage},
        /*disabled_features=*/{});
  } else {
    metadata_feature_list.InitWithFeaturesAndParameters(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kAutofillEnableCardProductName,
                               features::kAutofillEnableCardArtImage});
  }
  if (CardNameAvailable()) {
    card.set_product_description(u"fake product description");
  }
  if (CardArtAvailable()) {
    card.set_card_art_url(GURL("https://www.example.com"));
  }

  MakeGetFullCardRequest(FullCardRequestOptions().with_credit_card(card));

  EXPECT_TRUE(request()->GetShouldUnmaskCardForTesting());
  std::vector<ClientBehaviorConstants> signals =
      request()->GetUnmaskRequestDetailsForTesting()->client_behavior_signals;
  if (MetadataEnabled() && CardNameAvailable() && CardArtAvailable()) {
    EXPECT_NE(
        signals.end(),
        base::ranges::find(
            signals,
            ClientBehaviorConstants::kShowingCardArtImageAndCardProductName));
  } else {
    EXPECT_TRUE(signals.empty());
  }
}

// Params:
// 1. Function reference to call which creates the appropriate credit card
// benefit for the unittest.
// 2. Whether the flag to render benefits is enabled.
// 3. Issuer ID which is set for the credit card with benefits.
class FullCardRequestCardBenefitsTest
    : public FullCardRequestTest,
      public ::testing::WithParamInterface<
          std::tuple<base::FunctionRef<CreditCardBenefit()>,
                     bool,
                     std::string>> {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureStates(
        {{features::kAutofillEnableCardBenefitsForAmericanExpress,
          IsCreditCardBenefitsEnabled()},
         {features::kAutofillEnableCardBenefitsForCapitalOne,
          IsCreditCardBenefitsEnabled()}});

    card_ = test::GetMaskedServerCard();
    autofill_client()->set_last_committed_primary_main_frame_url(
        test::GetOriginsForMerchantBenefit().begin()->GetURL());
    test::SetUpCreditCardAndBenefitData(
        card_, GetBenefit(), GetIssuerId(), *personal_data(),
        autofill_client()->GetAutofillOptimizationGuide());
  }

  CreditCardBenefit GetBenefit() const { return std::get<0>(GetParam())(); }

  bool IsCreditCardBenefitsEnabled() const { return std::get<1>(GetParam()); }

  const std::string& GetIssuerId() const { return std::get<2>(GetParam()); }

  const CreditCard& card() { return card_; }

 private:
  CreditCard card_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    FullCardRequestTest,
    FullCardRequestCardBenefitsTest,
    testing::Combine(
        ::testing::Values(&test::GetActiveCreditCardFlatRateBenefit,
                          &test::GetActiveCreditCardCategoryBenefit,
                          &test::GetActiveCreditCardMerchantBenefit),
        ::testing::Bool(),
        ::testing::Values("amex", "capitalone")));

// Checks that ClientBehaviorConstants::kShowingCardBenefits is populated as a
// signal if a card benefit was shown when unmasking a credit card suggestion
// through the FullCardRequest.
TEST_P(FullCardRequestCardBenefitsTest, Benefits_ClientBehaviorConstants) {
  MakeGetFullCardRequest(FullCardRequestOptions().with_credit_card(card()));
  ASSERT_TRUE(request()->GetShouldUnmaskCardForTesting());
  std::vector<ClientBehaviorConstants> signals =
      request()->GetUnmaskRequestDetailsForTesting()->client_behavior_signals;
  EXPECT_EQ(base::ranges::find(signals,
                               ClientBehaviorConstants::kShowingCardBenefits) !=
                signals.end(),
            IsCreditCardBenefitsEnabled());
}

}  // namespace payments
}  // namespace autofill
