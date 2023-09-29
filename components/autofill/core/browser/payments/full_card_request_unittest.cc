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
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace autofill {
namespace payments {

using testing::_;
using testing::NiceMock;

// The consumer of the full card request API.
class MockResultDelegate : public FullCardRequest::ResultDelegate,
                           public base::SupportsWeakPtr<MockResultDelegate> {
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
};

// The delegate responsible for displaying the unmask prompt UI.
class MockUIDelegate : public FullCardRequest::UIDelegate,
                       public base::SupportsWeakPtr<MockUIDelegate> {
 public:
  MOCK_METHOD(void,
              ShowUnmaskPrompt,
              (const CreditCard&,
               const CardUnmaskPromptOptions&,
               base::WeakPtr<CardUnmaskDelegate>),
              (override));
  MOCK_METHOD(void,
              OnUnmaskVerificationResult,
              (AutofillClient::PaymentsRpcResult),
              (override));
#if BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(bool, ShouldOfferFidoAuth, (), (const override));
  MOCK_METHOD(bool,
              UserOptedInToFidoFromSettingsPageOnMobile,
              (),
              (const override));
#endif
};

// The personal data manager.
class MockPersonalDataManager : public TestPersonalDataManager {
 public:
  MockPersonalDataManager() {}
  ~MockPersonalDataManager() override {}
  MOCK_METHOD(bool,
              IsSyncFeatureEnabledForPaymentsServerMetrics,
              (),
              (const override));
  MOCK_METHOD(void,
              UpdateCreditCard,
              (const CreditCard& credit_card),
              (override));
  MOCK_METHOD(void,
              UpdateServerCreditCard,
              (const CreditCard& credit_card),
              (override));
};

// TODO(crbug.com/881835): Simplify this test setup.
// The test fixture for full card request.
class FullCardRequestTest : public testing::Test {
 public:
  struct FullCardRequestOptions {
    FullCardRequestOptions& with_credit_card(CreditCard cc) {
      credit_card = cc;
      return *this;
    }

    FullCardRequestOptions& with_unmask_card_reason(
        AutofillClient::UnmaskCardReason ucr) {
      unmask_card_reason = ucr;
      return *this;
    }

    FullCardRequestOptions& with_merchant_domain_for_footprints(
        url::Origin mdff) {
      merchant_domain_for_footprints = mdff;
      return *this;
    }

    CreditCard credit_card;
    AutofillClient::UnmaskCardReason unmask_card_reason =
        AutofillClient::UnmaskCardReason::kAutofill;
    url::Origin merchant_domain_for_footprints =
        url::Origin::Create(GURL("https://example.com/"));
  };

  FullCardRequestTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    payments_client_ = std::make_unique<PaymentsClient>(
        test_shared_loader_factory_, autofill_client_.GetIdentityManager(),
        &personal_data_);
    request_ = std::make_unique<FullCardRequest>(
        &autofill_client_, payments_client_.get(), &personal_data_);
    personal_data_.SetAccountInfoForPayments(
        autofill_client_.GetIdentityManager()->GetPrimaryAccountInfo(
            signin::ConsentLevel::kSync));
    // Silence the warning from PaymentsClient about matching sync and Payments
    // server types.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        "sync-url", "https://google.com");
  }

  FullCardRequestTest(const FullCardRequestTest&) = delete;
  FullCardRequestTest& operator=(const FullCardRequestTest&) = delete;

  ~FullCardRequestTest() override = default;

  MockPersonalDataManager* personal_data() { return &personal_data_; }

  FullCardRequest* request() { return request_.get(); }

  CardUnmaskDelegate* card_unmask_delegate() {
    return static_cast<CardUnmaskDelegate*>(request_.get());
  }

  MockResultDelegate* result_delegate() { return &result_delegate_; }

  MockUIDelegate* ui_delegate() { return &ui_delegate_; }

  TestAutofillClient* autofill_client() { return &autofill_client_; }

  void OnDidGetRealPan(AutofillClient::PaymentsRpcResult result,
                       const std::string& real_pan,
                       bool is_virtual_card = false) {
    payments::PaymentsClient::UnmaskResponseDetails response;
    response.card_type = is_virtual_card
                             ? AutofillClient::PaymentsRpcCardType::kVirtualCard
                             : AutofillClient::PaymentsRpcCardType::kServerCard;
    request_->OnDidGetRealPan(result, response.with_real_pan(real_pan));
  }

  void OnDidGetRealPanWithDcvv(AutofillClient::PaymentsRpcResult result,
                               const std::string& real_pan,
                               const std::string& dcvv,
                               bool is_virtual_card = false) {
    payments::PaymentsClient::UnmaskResponseDetails response;
    response.card_type = is_virtual_card
                             ? AutofillClient::PaymentsRpcCardType::kVirtualCard
                             : AutofillClient::PaymentsRpcCardType::kServerCard;
    request_->OnDidGetRealPan(result,
                              response.with_real_pan(real_pan).with_dcvv(dcvv));
  }

  void MakeGetFullCardRequest(FullCardRequestOptions options) {
    request()->GetFullCard(options.credit_card, options.unmask_card_reason,
                           result_delegate()->AsWeakPtr(),
                           ui_delegate()->AsWeakPtr(),
                           options.merchant_domain_for_footprints);
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  NiceMock<MockPersonalDataManager> personal_data_;
  MockResultDelegate result_delegate_;
  MockUIDelegate ui_delegate_;
  TestAutofillClient autofill_client_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<PaymentsClient> payments_client_;
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
  EXPECT_CALL(*ui_delegate(), OnUnmaskVerificationResult(
                                  AutofillClient::PaymentsRpcResult::kSuccess));

  MakeGetFullCardRequest(FullCardRequestOptions().with_credit_card(
      CreditCard(CreditCard::RecordType::kMaskedServerCard, "server_id")));
  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess, "4111");
  card_unmask_delegate()->OnUnmaskPromptClosed();
}

// Verify full PAN and dCVV are both used when returned by the server.
TEST_F(FullCardRequestTest, GetFullCardPanAndDcvvForMaskedServerCardViaDcvv) {
  EXPECT_CALL(*result_delegate(),
              OnFullCardRequestSucceeded(
                  testing::Ref(*request()),
                  CardMatches(CreditCard::RecordType::kFullServerCard, "4111"),
                  testing::Eq(u"321")));
  EXPECT_CALL(*ui_delegate(), ShowUnmaskPrompt(_, _, _));
  EXPECT_CALL(*ui_delegate(), OnUnmaskVerificationResult(
                                  AutofillClient::PaymentsRpcResult::kSuccess));

  MakeGetFullCardRequest(FullCardRequestOptions().with_credit_card(
      CreditCard(CreditCard::RecordType::kMaskedServerCard, "server_id")));
  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  OnDidGetRealPanWithDcvv(AutofillClient::PaymentsRpcResult::kSuccess, "4111",
                          "321");
  card_unmask_delegate()->OnUnmaskPromptClosed();
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
      AutofillClient::UnmaskCardReason::kAutofill,
      result_delegate()->AsWeakPtr(), base::Value::Dict(),
      url::Origin::Create(GURL("https://example.com")),
      GURL("https://example.com"));
  payments::PaymentsClient::UnmaskRequestDetails* request_details =
      request()->GetUnmaskRequestDetailsForTesting();
  EXPECT_EQ(request_details->last_committed_primary_main_frame_origin->spec(),
            GURL("https://example.com/").spec());
  EXPECT_EQ(request_details->merchant_domain_for_footprints->Serialize(),
            "https://example.com");
  OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess, "4111");
}

// Verify getting the CVC for a local card.
TEST_F(FullCardRequestTest, GetFullCardPanAndCvcForLocalCard) {
  EXPECT_CALL(*result_delegate(),
              OnFullCardRequestSucceeded(
                  testing::Ref(*request()),
                  CardMatches(CreditCard::RecordType::kLocalCard, "4111"),
                  testing::Eq(u"123")));
  EXPECT_CALL(*ui_delegate(), ShowUnmaskPrompt(_, _, _));
  EXPECT_CALL(*ui_delegate(), OnUnmaskVerificationResult(
                                  AutofillClient::PaymentsRpcResult::kSuccess));

  CreditCard card;
  test::SetCreditCardInfo(&card, nullptr, "4111", "12", "2050", "1");
  MakeGetFullCardRequest(FullCardRequestOptions().with_credit_card(card));
  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  card_unmask_delegate()->OnUnmaskPromptClosed();
}

// Verify getting the CVC for an unmasked server card.
TEST_F(FullCardRequestTest, GetFullCardPanAndCvcForFullServerCard) {
  EXPECT_CALL(*result_delegate(),
              OnFullCardRequestSucceeded(
                  testing::Ref(*request()),
                  CardMatches(CreditCard::RecordType::kFullServerCard, "4111"),
                  testing::Eq(u"123")));
  EXPECT_CALL(*ui_delegate(), ShowUnmaskPrompt(_, _, _));
  EXPECT_CALL(*ui_delegate(), OnUnmaskVerificationResult(
                                  AutofillClient::PaymentsRpcResult::kSuccess));

  CreditCard full_server_card(CreditCard::RecordType::kFullServerCard,
                              "server_id");
  test::SetCreditCardInfo(&full_server_card, nullptr, "4111", "12", "2050",
                          "1");
  MakeGetFullCardRequest(
      FullCardRequestOptions().with_credit_card(full_server_card));
  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  card_unmask_delegate()->OnUnmaskPromptClosed();
}

// Verify getting the CVC for an unmasked server card with expiration date in
// the past.
TEST_F(FullCardRequestTest, GetFullCardPanAndCvcForExpiredFullServerCard) {
  EXPECT_CALL(*result_delegate(),
              OnFullCardRequestSucceeded(
                  testing::Ref(*request()),
                  CardMatches(CreditCard::RecordType::kFullServerCard, "4111",
                              "12", "2051"),
                  testing::Eq(u"123")));
  EXPECT_CALL(*ui_delegate(), ShowUnmaskPrompt(_, _, _));
  EXPECT_CALL(*personal_data(), UpdateServerCreditCard(_)).Times(0);
  EXPECT_CALL(*ui_delegate(), OnUnmaskVerificationResult(
                                  AutofillClient::PaymentsRpcResult::kSuccess));

  base::Time::Exploded today;
  AutofillClock::Now().LocalExplode(&today);
  CreditCard full_server_card(CreditCard::RecordType::kFullServerCard,
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
  OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess, "4111");
  card_unmask_delegate()->OnUnmaskPromptClosed();
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
  EXPECT_CALL(*personal_data(), UpdateServerCreditCard(_)).Times(0);
  EXPECT_CALL(*ui_delegate(), OnUnmaskVerificationResult(
                                  AutofillClient::PaymentsRpcResult::kSuccess));

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
  OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess, "4111");
  card_unmask_delegate()->OnUnmaskPromptClosed();
}

// Verify getting the full PAN, the expiration and the dCVV for a virtual card
// using CVC authentication.
// TODO(crbug/1373232): Add a FullCardRequest test case for Virtual Card
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
  EXPECT_CALL(*ui_delegate(), OnUnmaskVerificationResult(
                                  AutofillClient::PaymentsRpcResult::kSuccess))
      .Times(1);

  CreditCard card;
  card.set_record_type(CreditCard::RecordType::kVirtualCard);
  card.set_server_id("server_id");
  CardUnmaskChallengeOption challenge_option =
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kCvc})[0];
  request()->GetFullVirtualCardViaCVC(
      card, AutofillClient::UnmaskCardReason::kAutofill,
      result_delegate()->AsWeakPtr(), ui_delegate()->AsWeakPtr(),
      GURL("https://example.com/"), "test_context_token", challenge_option,
      url::Origin::Create(GURL("https://example.com")));
  ASSERT_TRUE(request()->GetShouldUnmaskCardForTesting());
  payments::PaymentsClient::UnmaskRequestDetails* request_details =
      request()->GetUnmaskRequestDetailsForTesting();
  EXPECT_EQ(request_details->selected_challenge_option->type,
            CardUnmaskChallengeOptionType::kCvc);
  EXPECT_EQ(request_details->selected_challenge_option->id.value(),
            challenge_option.id.value());
  EXPECT_EQ(request_details->context_token, "test_context_token");
  EXPECT_EQ(request_details->last_committed_primary_main_frame_origin->spec(),
            GURL("https://example.com/").spec());
  EXPECT_EQ(request_details->merchant_domain_for_footprints->Serialize(),
            "https://example.com");

  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  details.exp_month = u"12";
  details.exp_year = base::UTF8ToUTF16(test::NextYear());
  details.enable_fido_auth = false;
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  payments::PaymentsClient::UnmaskResponseDetails response;
  response.real_pan = "4111";
  response.dcvv = "123";
  response.expiration_month = "12";
  response.expiration_year = test::NextYear();
  response.card_type = AutofillClient::PaymentsRpcCardType::kVirtualCard;
  request()->OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess,
                             response);
  card_unmask_delegate()->OnUnmaskPromptClosed();
}

TEST_F(FullCardRequestTest,
       DoesNotIncludeMerchantDomainForFootprintsWhenOffTheRecord) {
  autofill_client()->set_is_off_the_record(true);

  MakeGetFullCardRequest(
      FullCardRequestOptions()
          .with_credit_card(CreditCard(
              CreditCard::RecordType::kMaskedServerCard, "server_id"))
          .with_merchant_domain_for_footprints(
              url::Origin::Create(GURL("http://example.com"))));
  payments::PaymentsClient::UnmaskRequestDetails* request_details =
      request()->GetUnmaskRequestDetailsForTesting();
  ASSERT_EQ(request_details->merchant_domain_for_footprints, absl::nullopt);

  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess, "4111");
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
              AutofillClient::UnmaskCardReason::kAutofill));
  MakeGetFullCardRequest(
      FullCardRequestOptions()
          .with_credit_card(CreditCard(
              CreditCard::RecordType::kMaskedServerCard, "server_id_2"))
          .with_unmask_card_reason(
              AutofillClient::UnmaskCardReason::kPaymentRequest));
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
  EXPECT_CALL(*ui_delegate(), OnUnmaskVerificationResult(
                                  AutofillClient::PaymentsRpcResult::kSuccess))
      .Times(2);

  CreditCard card;
  test::SetCreditCardInfo(&card, nullptr, "4111", "12", "2050", "1");
  MakeGetFullCardRequest(FullCardRequestOptions().with_credit_card(card));
  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  card_unmask_delegate()->OnUnmaskPromptClosed();

  MakeGetFullCardRequest(FullCardRequestOptions().with_credit_card(card));
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  card_unmask_delegate()->OnUnmaskPromptClosed();
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
  card_unmask_delegate()->OnUnmaskPromptClosed();
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
              OnUnmaskVerificationResult(
                  AutofillClient::PaymentsRpcResult::kPermanentFailure));

  MakeGetFullCardRequest(FullCardRequestOptions().with_credit_card(
      CreditCard(CreditCard::RecordType::kMaskedServerCard, "server_id")));
  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kPermanentFailure, "");
  card_unmask_delegate()->OnUnmaskPromptClosed();
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
  EXPECT_CALL(
      *ui_delegate(),
      OnUnmaskVerificationResult(
          AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure));

  CreditCard card;
  card.set_record_type(CreditCard::RecordType::kVirtualCard);
  request()->GetFullVirtualCardViaCVC(
      card, AutofillClient::UnmaskCardReason::kAutofill,
      result_delegate()->AsWeakPtr(), ui_delegate()->AsWeakPtr(),
      GURL("https://example.com/"), "test_context_token",
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kCvc})[0],
      url::Origin::Create(GURL("https://example.com")));
  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  OnDidGetRealPan(
      AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure, "",
      /*is_virtual_card=*/true);
  card_unmask_delegate()->OnUnmaskPromptClosed();
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
  EXPECT_CALL(
      *ui_delegate(),
      OnUnmaskVerificationResult(
          AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure));

  CreditCard card;
  card.set_record_type(CreditCard::RecordType::kVirtualCard);
  request()->GetFullVirtualCardViaCVC(
      card, AutofillClient::UnmaskCardReason::kAutofill,
      result_delegate()->AsWeakPtr(), ui_delegate()->AsWeakPtr(),
      GURL("https://example.com/"), "test_context_token",
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kCvc})[0],
      url::Origin::Create(GURL("https://example.com")));
  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  OnDidGetRealPan(
      AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure, "",
      /*is_virtual_card=*/true);
  card_unmask_delegate()->OnUnmaskPromptClosed();
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
              OnUnmaskVerificationResult(
                  AutofillClient::PaymentsRpcResult::kNetworkError));

  MakeGetFullCardRequest(FullCardRequestOptions().with_credit_card(
      CreditCard(CreditCard::RecordType::kMaskedServerCard, "server_id")));
  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kNetworkError, "");
  card_unmask_delegate()->OnUnmaskPromptClosed();
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
    EXPECT_CALL(*ui_delegate(),
                OnUnmaskVerificationResult(
                    AutofillClient::PaymentsRpcResult::kTryAgainFailure));
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
    OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kTryAgainFailure, "");
    card_unmask_delegate()->OnUnmaskPromptClosed();
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
    EXPECT_CALL(*ui_delegate(),
                OnUnmaskVerificationResult(
                    AutofillClient::PaymentsRpcResult::kTryAgainFailure));
    EXPECT_CALL(*ui_delegate(),
                OnUnmaskVerificationResult(
                    AutofillClient::PaymentsRpcResult::kSuccess));

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
    OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kTryAgainFailure, "");
    details.cvc = u"123";
    card_unmask_delegate()->OnUnmaskPromptAccepted(details);
    OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess, "4111");
    card_unmask_delegate()->OnUnmaskPromptClosed();
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
              OnUnmaskVerificationResult(
                  AutofillClient::PaymentsRpcResult::kTryAgainFailure))
      .Times(1);

  CardUnmaskChallengeOption challenge_option =
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kCvc})[0];
  request()->GetFullVirtualCardViaCVC(
      test::GetVirtualCard(), AutofillClient::UnmaskCardReason::kAutofill,
      result_delegate()->AsWeakPtr(), ui_delegate()->AsWeakPtr(),
      GURL("https://example.com/"), "test_context_token", challenge_option,
      url::Origin::Create(GURL("https://example.com")));
  CardUnmaskDelegate::UserProvidedUnmaskDetails user_provided_details;
  user_provided_details.cvc = u"321";
  card_unmask_delegate()->OnUnmaskPromptAccepted(user_provided_details);
  PaymentsClient::UnmaskResponseDetails response_details;
  response_details.card_type =
      AutofillClient::PaymentsRpcCardType::kVirtualCard;
  response_details.context_token = "test_context_token";
  request()->OnDidGetRealPan(
      AutofillClient::PaymentsRpcResult::kTryAgainFailure, response_details);
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
  EXPECT_EQ(request()
                ->GetUnmaskRequestDetailsForTesting()
                ->merchant_domain_for_footprints->Serialize(),
            "https://example.com");
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
  EXPECT_CALL(*ui_delegate(), OnUnmaskVerificationResult(
                                  AutofillClient::PaymentsRpcResult::kSuccess));

  MakeGetFullCardRequest(FullCardRequestOptions().with_credit_card(
      CreditCard(CreditCard::RecordType::kMaskedServerCard, "server_id")));
  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  details.exp_month = u"12";
  details.exp_year = u"2050";
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess, "4111");
  card_unmask_delegate()->OnUnmaskPromptClosed();
}

// Verify updating expiration date for an unmasked server card.
TEST_F(FullCardRequestTest, UpdateExpDateForFullServerCard) {
  EXPECT_CALL(*result_delegate(),
              OnFullCardRequestSucceeded(
                  testing::Ref(*request()),
                  CardMatches(CreditCard::RecordType::kFullServerCard, "4111",
                              "12", "2050"),
                  testing::Eq(u"123")));
  EXPECT_CALL(*ui_delegate(), ShowUnmaskPrompt(_, _, _));
  EXPECT_CALL(*ui_delegate(), OnUnmaskVerificationResult(
                                  AutofillClient::PaymentsRpcResult::kSuccess));

  CreditCard full_server_card(CreditCard::RecordType::kFullServerCard,
                              "server_id");
  test::SetCreditCardInfo(&full_server_card, nullptr, "4111", "10", "2000",
                          "1");
  MakeGetFullCardRequest(
      FullCardRequestOptions().with_credit_card(full_server_card));
  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  details.exp_month = u"12";
  details.exp_year = u"2050";
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess, "4111");
  card_unmask_delegate()->OnUnmaskPromptClosed();
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
  EXPECT_CALL(*personal_data(),
              UpdateCreditCard(CardMatches(CreditCard::RecordType::kLocalCard,
                                           "4111", "12", "2051")));
  EXPECT_CALL(*ui_delegate(), OnUnmaskVerificationResult(
                                  AutofillClient::PaymentsRpcResult::kSuccess));

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
  card_unmask_delegate()->OnUnmaskPromptClosed();
}

// Verify getting full PAN and CVC for PaymentRequest.
TEST_F(FullCardRequestTest, UnmaskForPaymentRequest) {
  EXPECT_CALL(*result_delegate(),
              OnFullCardRequestSucceeded(
                  testing::Ref(*request()),
                  CardMatches(CreditCard::RecordType::kFullServerCard, "4111"),
                  testing::Eq(u"123")));
  EXPECT_CALL(*ui_delegate(), ShowUnmaskPrompt(_, _, _));
  EXPECT_CALL(*ui_delegate(), OnUnmaskVerificationResult(
                                  AutofillClient::PaymentsRpcResult::kSuccess));

  MakeGetFullCardRequest(
      FullCardRequestOptions()
          .with_credit_card(CreditCard(
              CreditCard::RecordType::kMaskedServerCard, "server_id"))
          .with_unmask_card_reason(
              AutofillClient::UnmaskCardReason::kPaymentRequest));
  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess, "4111");
  card_unmask_delegate()->OnUnmaskPromptClosed();
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

}  // namespace payments
}  // namespace autofill
