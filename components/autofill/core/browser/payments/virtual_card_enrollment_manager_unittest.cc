// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_art_image.h"
#include "components/autofill/core/browser/payments/payments_requests/update_virtual_card_enrollment_request.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/browser/payments/test_virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace autofill {

namespace {
const std::string kTestVcnContextToken = "vcn_context_token";
const std::string kTestRiskData = "risk_data";
}  // namespace

class VirtualCardEnrollmentManagerTest : public testing::Test {
 public:
  void SetUp() override {
    autofill_client_ = std::make_unique<TestAutofillClient>();
    autofill_client_->SetPrefs(test::PrefServiceForTesting());
    user_prefs_ = autofill_client_->GetPrefs();
    personal_data_manager_ = std::make_unique<TestPersonalDataManager>();
    personal_data_manager_->Init(
        /*profile_database=*/nullptr,
        /*account_database=*/nullptr,
        /*pref_service=*/autofill_client_->GetPrefs(),
        /*local_state=*/autofill_client_->GetPrefs(),
        /*identity_manager=*/nullptr,
        /*history_service=*/nullptr,
        /*strike_database=*/nullptr,
        /*image_fetcher=*/nullptr,
        /*is_off_the_record=*/false);
    autofill_driver_ = std::make_unique<TestAutofillDriver>();
    autofill_client_->set_test_payments_client(
        std::make_unique<payments::TestPaymentsClient>(
            autofill_driver_->GetURLLoaderFactory(),
            autofill_client_->GetIdentityManager(),
            personal_data_manager_.get()));
    payments_client_ = static_cast<payments::TestPaymentsClient*>(
        autofill_client_->GetPaymentsClient());
    virtual_card_enrollment_manager_ =
        std::make_unique<TestVirtualCardEnrollmentManager>(
            personal_data_manager_.get(), payments_client_,
            autofill_client_.get());
  }

  void TearDown() override {
    // Order of destruction is important as AutofillDriver relies on
    // PersonalDataManager to be around when it gets destroyed.
    autofill_driver_.reset();
  }

  void SetUpCard() {
    card_ = std::make_unique<CreditCard>(test::GetMaskedServerCard());
    card_->set_card_art_url(autofill_client_->form_origin());
    card_->set_instrument_id(112233445566);
    personal_data_manager_->AddFullServerCreditCard(*card_.get());
  }

  void SetValidCardArtImageForCard(const CreditCard& card) {
    gfx::Image expected_image = gfx::test::CreateImage(32, 20);
    std::vector<std::unique_ptr<CreditCardArtImage>> images;
    images.emplace_back(std::make_unique<CreditCardArtImage>());
    images.back()->card_art_url = card.card_art_url();
    images.back()->card_art_image = expected_image;
    personal_data_manager_->OnCardArtImagesFetched(std::move(images));
  }

  payments::PaymentsClient::GetDetailsForEnrollmentResponseDetails
  SetUpOnDidGetDetailsForEnrollResponse(
      const TestLegalMessageLine& google_legal_message,
      const TestLegalMessageLine& issuer_legal_message) {
    personal_data_manager_->ClearCreditCardArtImages();
    SetUpCard();
    SetValidCardArtImageForCard(*card_);
    raw_ptr<VirtualCardEnrollmentProcessState> state =
        virtual_card_enrollment_manager_
            ->GetVirtualCardEnrollmentProcessState();
    state->virtual_card_enrollment_fields.credit_card = *card_;

    payments::PaymentsClient::GetDetailsForEnrollmentResponseDetails response;
    response.vcn_context_token = kTestVcnContextToken;
    response.google_legal_message = {google_legal_message};
    response.issuer_legal_message = {issuer_legal_message};
    return response;
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestAutofillClient> autofill_client_;
  std::unique_ptr<TestAutofillDriver> autofill_driver_;
  raw_ptr<payments::TestPaymentsClient> payments_client_;
  std::unique_ptr<TestPersonalDataManager> personal_data_manager_;
  std::unique_ptr<TestVirtualCardEnrollmentManager>
      virtual_card_enrollment_manager_;
  raw_ptr<PrefService> user_prefs_;

  // The global CreditCard used throughout the tests. Each test that needs to
  // use it will set it up for the specific test before testing it.
  std::unique_ptr<CreditCard> card_;
};

TEST_F(VirtualCardEnrollmentManagerTest, OfferVirtualCardEnroll) {
  for (VirtualCardEnrollmentSource virtual_card_enrollment_source :
       {VirtualCardEnrollmentSource::kUpstream,
        VirtualCardEnrollmentSource::kDownstream,
        VirtualCardEnrollmentSource::kSettingsPage}) {
    for (bool make_image_present : {true, false}) {
      SCOPED_TRACE(testing::Message()
                   << " virtual_card_enrollment_source="
                   << static_cast<int>(virtual_card_enrollment_source)
                   << ", make_image_present=" << make_image_present);

      personal_data_manager_->ClearCreditCardArtImages();
      SetUpCard();
      auto state = virtual_card_enrollment_manager_
                       ->GetVirtualCardEnrollmentProcessState();
      state->risk_data.reset();
      if (make_image_present)
        SetValidCardArtImageForCard(*card_);

#if BUILDFLAG(IS_ANDROID)
      virtual_card_enrollment_manager_->SetAutofillClient(nullptr);
#endif

      virtual_card_enrollment_manager_->OfferVirtualCardEnroll(
          *card_, virtual_card_enrollment_source,
          virtual_card_enrollment_manager_->AutofillClientIsPresent()
              ? user_prefs_
              : nullptr);

      // CreditCard class overloads equality operator to check that GUIDs,
      // origins, and the contents of the two cards are equal.
      EXPECT_EQ(*card_, state->virtual_card_enrollment_fields.credit_card);

      raw_ptr<gfx::Image> card_art_image =
          state->virtual_card_enrollment_fields.card_art_image;
      EXPECT_EQ(make_image_present, card_art_image != nullptr);
      EXPECT_TRUE(state->risk_data.has_value());
    }
  }
}

TEST_F(VirtualCardEnrollmentManagerTest, OnRiskDataLoadedForVirtualCard) {
  raw_ptr<VirtualCardEnrollmentProcessState> state =
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState();
  state->virtual_card_enrollment_fields.virtual_card_enrollment_source =
      VirtualCardEnrollmentSource::kUpstream;
  SetUpCard();
  state->virtual_card_enrollment_fields.credit_card = *card_;
  state->risk_data.reset();

  virtual_card_enrollment_manager_->OnRiskDataLoadedForVirtualCard(
      kTestRiskData);

  payments::PaymentsClient::GetDetailsForEnrollmentRequestDetails
      request_details =
          payments_client_->get_details_for_enrollment_request_details();

  EXPECT_EQ(request_details.risk_data, state->risk_data.value_or(""));
  EXPECT_EQ(request_details.app_locale, personal_data_manager_->app_locale());
  EXPECT_EQ(request_details.instrument_id,
            state->virtual_card_enrollment_fields.credit_card.instrument_id());
  EXPECT_EQ(request_details.billing_customer_number,
            payments::GetBillingCustomerId(personal_data_manager_.get()));
  EXPECT_EQ(
      request_details.source,
      state->virtual_card_enrollment_fields.virtual_card_enrollment_source);
}

TEST_F(VirtualCardEnrollmentManagerTest, OnDidGetDetailsForEnrollResponse) {
  const TestLegalMessageLine google_legal_message =
      TestLegalMessageLine("google_test_legal_message");
  const TestLegalMessageLine issuer_legal_message =
      TestLegalMessageLine("issuer_test_legal_message");
  payments::PaymentsClient::GetDetailsForEnrollmentResponseDetails response =
      std::move(SetUpOnDidGetDetailsForEnrollResponse(google_legal_message,
                                                      issuer_legal_message));

  virtual_card_enrollment_manager_->OnDidGetDetailsForEnrollResponse(
      AutofillClient::PaymentsRpcResult::kSuccess, response);

  auto state =
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState();
  EXPECT_TRUE(state->vcn_context_token.has_value());
  EXPECT_EQ(state->vcn_context_token, response.vcn_context_token);
  VirtualCardEnrollmentFields virtual_card_enrollment_fields =
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState()
          ->virtual_card_enrollment_fields;
  EXPECT_TRUE(virtual_card_enrollment_fields.google_legal_message[0].text() ==
              google_legal_message.text());
  EXPECT_TRUE(virtual_card_enrollment_fields.issuer_legal_message[0].text() ==
              issuer_legal_message.text());
  EXPECT_TRUE(virtual_card_enrollment_fields.card_art_image != nullptr);
}

TEST_F(VirtualCardEnrollmentManagerTest,
       OnDidGetDetailsForEnrollResponse_NoAutofillClient) {
  const TestLegalMessageLine google_legal_message =
      TestLegalMessageLine("google_test_legal_message");
  const TestLegalMessageLine issuer_legal_message =
      TestLegalMessageLine("issuer_test_legal_message");
  payments::PaymentsClient::GetDetailsForEnrollmentResponseDetails response =
      std::move(SetUpOnDidGetDetailsForEnrollResponse(google_legal_message,
                                                      issuer_legal_message));
  virtual_card_enrollment_manager_->SetAutofillClient(nullptr);

  virtual_card_enrollment_manager_->OnDidGetDetailsForEnrollResponse(
      AutofillClient::PaymentsRpcResult::kSuccess, response);

  auto state =
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState();
  EXPECT_TRUE(state->vcn_context_token.has_value());
  EXPECT_EQ(state->vcn_context_token, response.vcn_context_token);
  VirtualCardEnrollmentFields virtual_card_enrollment_fields =
      state->virtual_card_enrollment_fields;
  EXPECT_TRUE(virtual_card_enrollment_fields.google_legal_message[0].text() ==
              google_legal_message.text());
  EXPECT_TRUE(virtual_card_enrollment_fields.issuer_legal_message[0].text() ==
              issuer_legal_message.text());
  EXPECT_TRUE(virtual_card_enrollment_fields.card_art_image != nullptr);
}

TEST_F(VirtualCardEnrollmentManagerTest,
       OnDidGetDetailsForEnrollResponse_Reset) {
  for (AutofillClient::PaymentsRpcResult result :
       {AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure,
        AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure}) {
    virtual_card_enrollment_manager_->SetResetCalled(false);

    virtual_card_enrollment_manager_->OnDidGetDetailsForEnrollResponse(
        result,
        payments::PaymentsClient::GetDetailsForEnrollmentResponseDetails());

    EXPECT_TRUE(virtual_card_enrollment_manager_->GetResetCalled());
  }

  // Ensure the clank settings page use-case works as expected.
  virtual_card_enrollment_manager_->SetAutofillClient(nullptr);
  for (AutofillClient::PaymentsRpcResult result :
       {AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure,
        AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure}) {
    virtual_card_enrollment_manager_->SetResetCalled(false);

    virtual_card_enrollment_manager_->OnDidGetDetailsForEnrollResponse(
        result,
        payments::PaymentsClient::GetDetailsForEnrollmentResponseDetails());

    EXPECT_TRUE(virtual_card_enrollment_manager_->GetResetCalled());
  }
}

TEST_F(VirtualCardEnrollmentManagerTest, Enroll) {
  raw_ptr<VirtualCardEnrollmentProcessState> state =
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState();
  state->vcn_context_token = kTestVcnContextToken;
  personal_data_manager_->SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  for (VirtualCardEnrollmentSource virtual_card_enrollment_source :
       {VirtualCardEnrollmentSource::kUpstream,
        VirtualCardEnrollmentSource::kDownstream,
        VirtualCardEnrollmentSource::kSettingsPage}) {
    SCOPED_TRACE(testing::Message()
                 << " virtual_card_enrollment_source="
                 << static_cast<int>(virtual_card_enrollment_source));
    state->virtual_card_enrollment_fields.virtual_card_enrollment_source =
        virtual_card_enrollment_source;
    virtual_card_enrollment_manager_->SetPaymentsRpcResult(
        AutofillClient::PaymentsRpcResult::kNone);

    virtual_card_enrollment_manager_->Enroll();

    payments::PaymentsClient::UpdateVirtualCardEnrollmentRequestDetails
        request_details =
            payments_client_->update_virtual_card_enrollment_request_details();
    EXPECT_TRUE(request_details.vcn_context_token.has_value());
    EXPECT_EQ(request_details.vcn_context_token, kTestVcnContextToken);
    EXPECT_EQ(request_details.virtual_card_enrollment_source,
              virtual_card_enrollment_source);
    EXPECT_EQ(request_details.virtual_card_enrollment_request_type,
              VirtualCardEnrollmentRequestType::kEnroll);
    EXPECT_EQ(request_details.billing_customer_number, 123456);
    EXPECT_EQ(virtual_card_enrollment_manager_->GetPaymentsRpcResult(),
              AutofillClient::PaymentsRpcResult::kSuccess);
  }
}

TEST_F(VirtualCardEnrollmentManagerTest, Unenroll) {
  personal_data_manager_->SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));
  virtual_card_enrollment_manager_->SetPaymentsRpcResult(
      AutofillClient::PaymentsRpcResult::kNone);

  virtual_card_enrollment_manager_->Unenroll(
      /*instrument_id=*/9223372036854775807);

  payments::PaymentsClient::UpdateVirtualCardEnrollmentRequestDetails
      request_details =
          payments_client_->update_virtual_card_enrollment_request_details();
  EXPECT_EQ(request_details.virtual_card_enrollment_source,
            VirtualCardEnrollmentSource::kSettingsPage);
  EXPECT_EQ(request_details.virtual_card_enrollment_request_type,
            VirtualCardEnrollmentRequestType::kUnenroll);
  EXPECT_EQ(request_details.billing_customer_number, 123456);
  EXPECT_EQ(request_details.instrument_id, 9223372036854775807);

  // The request should not include a context token, and should succeed.
  EXPECT_FALSE(request_details.vcn_context_token.has_value());
  EXPECT_EQ(virtual_card_enrollment_manager_->GetPaymentsRpcResult(),
            AutofillClient::PaymentsRpcResult::kSuccess);
}

TEST_F(VirtualCardEnrollmentManagerTest, UpstreamAnimationSync_AnimationFirst) {
  personal_data_manager_->ClearCreditCardArtImages();
  SetUpCard();
  SetValidCardArtImageForCard(*card_);

  raw_ptr<VirtualCardEnrollmentProcessState> state =
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState();
  state->virtual_card_enrollment_fields.credit_card = *card_;
  state->vcn_context_token = kTestVcnContextToken;
  state->virtual_card_enrollment_fields.virtual_card_enrollment_source =
      VirtualCardEnrollmentSource::kUpstream;

  payments::PaymentsClient::GetDetailsForEnrollmentResponseDetails response;
  response.vcn_context_token = kTestVcnContextToken;
  response.issuer_legal_message = {
      TestLegalMessageLine("issuer_test_legal_message_line")};
  response.google_legal_message = {
      TestLegalMessageLine("google_test_legal_message_line")};

  // Update avatar animation complete boolean.
  virtual_card_enrollment_manager_->OnCardSavedAnimationComplete();
  EXPECT_TRUE(virtual_card_enrollment_manager_->GetAvatarAnimationComplete());

  // Ensure bubble was not shown yet.
  EXPECT_FALSE(virtual_card_enrollment_manager_->GetBubbleShown());

  // Update enrollment response complete boolean.
  virtual_card_enrollment_manager_->OnDidGetDetailsForEnrollResponse(
      AutofillClient::PaymentsRpcResult::kSuccess, response);
  EXPECT_TRUE(
      virtual_card_enrollment_manager_->GetEnrollResponseDetailsReceived());

  // Ensure bubble was shown.
  EXPECT_TRUE(virtual_card_enrollment_manager_->GetBubbleShown());
}

TEST_F(VirtualCardEnrollmentManagerTest, UpstreamAnimationSync_ResponseFirst) {
  personal_data_manager_->ClearCreditCardArtImages();
  SetUpCard();
  SetValidCardArtImageForCard(*card_);

  raw_ptr<VirtualCardEnrollmentProcessState> state =
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState();
  state->virtual_card_enrollment_fields.credit_card = *card_;
  state->vcn_context_token = kTestVcnContextToken;
  state->virtual_card_enrollment_fields.virtual_card_enrollment_source =
      VirtualCardEnrollmentSource::kUpstream;

  payments::PaymentsClient::GetDetailsForEnrollmentResponseDetails response;
  response.vcn_context_token = kTestVcnContextToken;
  response.issuer_legal_message = {
      TestLegalMessageLine("issuer_test_legal_message_line")};
  response.google_legal_message = {
      TestLegalMessageLine("google_test_legal_message_line")};

  // Update enrollment response complete boolean.
  virtual_card_enrollment_manager_->OnDidGetDetailsForEnrollResponse(
      AutofillClient::PaymentsRpcResult::kSuccess, response);
  EXPECT_TRUE(
      virtual_card_enrollment_manager_->GetEnrollResponseDetailsReceived());

  // Ensure bubble was not shown yet.
  EXPECT_FALSE(virtual_card_enrollment_manager_->GetBubbleShown());

  // Update avatar animation complete boolean.
  virtual_card_enrollment_manager_->OnCardSavedAnimationComplete();
  EXPECT_TRUE(virtual_card_enrollment_manager_->GetAvatarAnimationComplete());
}

}  // namespace autofill
