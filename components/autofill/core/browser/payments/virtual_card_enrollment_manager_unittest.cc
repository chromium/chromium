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
            autofill_client_.get(), personal_data_manager_.get());
  }

  void TearDown() override {
    // Order of destruction is important as AutofillDriver relies on
    // PersonalDataManager to be around when it gets destroyed.
    autofill_driver_.reset();
  }

  CreditCard SetUpCard() {
    CreditCard card = test::GetMaskedServerCard();
    card.set_card_art_url(autofill_client_->form_origin());
    card.set_instrument_id(112233445566);
    personal_data_manager_->AddFullServerCreditCard(card);
    return card;
  }

  void SetValidCardArtImageForCard(const CreditCard& card) {
    gfx::Image expected_image = gfx::test::CreateImage(32, 20);
    std::vector<std::unique_ptr<CreditCardArtImage>> images;
    images.emplace_back(std::make_unique<CreditCardArtImage>());
    images.back()->card_art_url = card.card_art_url();
    images.back()->card_art_image = expected_image;
    personal_data_manager_->OnCardArtImagesFetched(std::move(images));
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
      CreditCard card = SetUpCard();
      if (make_image_present)
        SetValidCardArtImageForCard(card);

      virtual_card_enrollment_manager_->OfferVirtualCardEnroll(
          &card, virtual_card_enrollment_source);

      raw_ptr<VirtualCardEnrollmentProcessState> state =
          virtual_card_enrollment_manager_
              ->GetVirtualCardEnrollmentProcessState();

      // CreditCard class overloads equality operator to check that GUIDs,
      // origins, and the contents of the two cards are equal.
      EXPECT_EQ(card, *(state->virtual_card_enrollment_fields.credit_card));
      raw_ptr<gfx::Image> card_art_image =
          state->virtual_card_enrollment_fields.card_art_image;
      EXPECT_EQ(make_image_present, card_art_image != nullptr);
    }
  }
}

TEST_F(VirtualCardEnrollmentManagerTest, OnRiskDataLoadedForVirtualCard) {
  raw_ptr<VirtualCardEnrollmentProcessState> state =
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState();
  state->virtual_card_enrollment_fields.virtual_card_enrollment_source =
      VirtualCardEnrollmentSource::kUpstream;
  CreditCard card = SetUpCard();
  state->virtual_card_enrollment_fields.credit_card = &card;

  virtual_card_enrollment_manager_->OnRiskDataLoadedForVirtualCard(
      kTestRiskData);

  payments::PaymentsClient::GetDetailsForEnrollmentRequestDetails
      request_details =
          payments_client_->get_details_for_enrollment_request_details();

  EXPECT_EQ(request_details.risk_data, state->risk_data.value_or(""));
  EXPECT_EQ(request_details.app_locale, personal_data_manager_->app_locale());
  EXPECT_EQ(request_details.instrument_id,
            state->virtual_card_enrollment_fields.credit_card->instrument_id());
  EXPECT_EQ(request_details.billing_customer_number,
            payments::GetBillingCustomerId(personal_data_manager_.get()));
  EXPECT_EQ(
      request_details.source,
      state->virtual_card_enrollment_fields.virtual_card_enrollment_source);
}

TEST_F(VirtualCardEnrollmentManagerTest, OnDidGetDetailsForEnrollResponse) {
  personal_data_manager_->ClearCreditCardArtImages();
  CreditCard card = SetUpCard();
  SetValidCardArtImageForCard(card);
  raw_ptr<VirtualCardEnrollmentProcessState> state =
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState();
  state->virtual_card_enrollment_fields.credit_card = &card;

  payments::PaymentsClient::GetDetailsForEnrollmentResponseDetails response;
  response.vcn_context_token = kTestVcnContextToken;
  response.google_legal_message = {
      TestLegalMessageLine("google_test_legal_message")};
  response.issuer_legal_message = {
      TestLegalMessageLine("issuer_test_legal_message")};
  virtual_card_enrollment_manager_->OnDidGetDetailsForEnrollResponse(
      AutofillClient::PaymentsRpcResult::kSuccess, response);

  EXPECT_TRUE(state->vcn_context_token.has_value());
  EXPECT_EQ(state->vcn_context_token, response.vcn_context_token);
  VirtualCardEnrollmentFields virtual_card_enrollment_fields =
      state->virtual_card_enrollment_fields;
  EXPECT_EQ(virtual_card_enrollment_fields.google_legal_message[0].text(),
            response.google_legal_message[0].text());
  EXPECT_EQ(virtual_card_enrollment_fields.issuer_legal_message[0].text(),
            response.issuer_legal_message[0].text());
  EXPECT_TRUE(virtual_card_enrollment_fields.card_art_image != nullptr);
}

TEST_F(VirtualCardEnrollmentManagerTest,
       OnDidGetDetailsForEnrollResponse_Reset) {
  autofill_client_->set_virtual_card_error_dialog_shown(false);
  virtual_card_enrollment_manager_->OnDidGetDetailsForEnrollResponse(
      AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure,
      payments::PaymentsClient::GetDetailsForEnrollmentResponseDetails());
  EXPECT_TRUE(autofill_client_->virtual_card_error_dialog_shown());
  EXPECT_TRUE(autofill_client_->virtual_card_error_dialog_is_permanent_error());
  EXPECT_TRUE(virtual_card_enrollment_manager_->GetResetCalled());

  autofill_client_->set_virtual_card_error_dialog_shown(false);
  virtual_card_enrollment_manager_->OnDidGetDetailsForEnrollResponse(
      AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure,
      payments::PaymentsClient::GetDetailsForEnrollmentResponseDetails());
  EXPECT_TRUE(autofill_client_->virtual_card_error_dialog_shown());
  EXPECT_FALSE(
      autofill_client_->virtual_card_error_dialog_is_permanent_error());
  EXPECT_TRUE(virtual_card_enrollment_manager_->GetResetCalled());
}

TEST_F(VirtualCardEnrollmentManagerTest,
       OnVirtualCardEnrollmentBubbleAccepted) {
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

    virtual_card_enrollment_manager_->OnVirtualCardEnrollmentBubbleAccepted();

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

}  // namespace autofill
