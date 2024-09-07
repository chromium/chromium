// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_art_image.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_requests/update_virtual_card_enrollment_request.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "components/autofill/core/browser/payments/test_payments_network_interface.h"
#include "components/autofill/core/browser/payments/test_virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "components/autofill/core/browser/strike_databases/payments/test_strike_database.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/sync/test/test_sync_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/mock_resource_bundle_delegate.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

using testing::_;
using testing::NiceMock;

namespace autofill {
namespace {

const std::string kTestVcnContextToken = "vcn_context_token";
const std::string kTestRiskData = "risk_data";

}  // namespace
// The anonymous namespace needs to end here because of `friend`ships between
// the tests and the production code.

class VirtualCardEnrollmentManagerTest : public testing::Test {
 public:
  void SetUp() override {
    autofill_client_ = std::make_unique<TestAutofillClient>();
    autofill_client_->SetPrefs(test::PrefServiceForTesting());
    personal_data_manager().SetPrefService(autofill_client_->GetPrefs());
    personal_data_manager().SetSyncServiceForTest(&sync_service_);
    autofill_client_->GetPaymentsAutofillClient()
        ->set_test_payments_network_interface(
            std::make_unique<payments::TestPaymentsNetworkInterface>(
                autofill_client_->GetURLLoaderFactory(),
                autofill_client_->GetIdentityManager(),
                &personal_data_manager()));
    autofill_client_->set_test_strike_database(
        std::make_unique<TestStrikeDatabase>());
    virtual_card_enrollment_manager_ =
        std::make_unique<TestVirtualCardEnrollmentManager>(
            &personal_data_manager(), &payments_network_interface(),
            autofill_client_.get());
  }

  void SetUpCard() {
    card_ = std::make_unique<CreditCard>(test::GetMaskedServerCard());
    card_->set_card_art_url(autofill_client_->form_origin());
    card_->set_instrument_id(112233445566);
    card_->set_guid("00000000-0000-0000-0000-000000000001");
    personal_data_manager().test_payments_data_manager().AddServerCreditCard(
        *card_.get());
  }

  void SetValidCardArtImageForCard(const CreditCard& card) {
    personal_data_manager().test_payments_data_manager().AddCardArtImage(
        card.card_art_url(), gfx::test::CreateImage(40, 24));
  }

  void SetNetworkImageInResourceBundle(ui::MockResourceBundleDelegate* delegate,
                                       const std::string& network,
                                       const gfx::Image& network_image) {
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        personal_data_manager().app_locale(), delegate,
        ui::ResourceBundle::LoadResources::DO_NOT_LOAD_COMMON_RESOURCES);
    int resource_id = CreditCard::IconResourceId(network);
    ON_CALL(*delegate, GetImageNamed(resource_id))
        .WillByDefault(testing::Return(network_image));

    // Cache the image so that the ui::ResourceBundle::GetImageSkiaNamed()
    // call in VirtualCardEnrollmentManager can retrieve it.
    ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
  }

  payments::PaymentsNetworkInterface::GetDetailsForEnrollmentResponseDetails
  SetUpOnDidGetDetailsForEnrollResponse(
      const TestLegalMessageLine& google_legal_message,
      const TestLegalMessageLine& issuer_legal_message,
      bool make_image_present) {
    personal_data_manager()
        .test_payments_data_manager()
        .ClearCreditCardArtImages();
    SetUpCard();
    auto* state = virtual_card_enrollment_manager_
                      ->GetVirtualCardEnrollmentProcessState();
    if (make_image_present) {
      SetValidCardArtImageForCard(*card_);
    } else {
      state->virtual_card_enrollment_fields.card_art_image = nullptr;
    }
    state->virtual_card_enrollment_fields.credit_card = *card_;

    payments::PaymentsNetworkInterface::GetDetailsForEnrollmentResponseDetails
        response;
    response.vcn_context_token = kTestVcnContextToken;
    response.google_legal_message = {google_legal_message};
    response.issuer_legal_message = {issuer_legal_message};
    return response;
  }

  // TODO(crbug.com/303715506): This part does not test the desired behavior on
  // iOS as the virtual card enrollment strikedatabase on iOS is not initialized
  // (guarded by the feature flag).
  void SetUpStrikeDatabaseTest() {
    VirtualCardEnrollmentProcessState* state =
        virtual_card_enrollment_manager_
            ->GetVirtualCardEnrollmentProcessState();
    state->vcn_context_token = kTestVcnContextToken;
    SetUpCard();
    state->virtual_card_enrollment_fields.credit_card = *card_;
    personal_data_manager()
        .test_payments_data_manager()
        .SetPaymentsCustomerData(
            std::make_unique<PaymentsCustomerData>("123456"));
    EXPECT_FALSE(
        virtual_card_enrollment_manager_->ShouldBlockVirtualCardEnrollment(
            base::NumberToString(state->virtual_card_enrollment_fields
                                     .credit_card.instrument_id()),
            VirtualCardEnrollmentSource::kUpstream));
    EXPECT_FALSE(
        virtual_card_enrollment_manager_->ShouldBlockVirtualCardEnrollment(
            base::NumberToString(state->virtual_card_enrollment_fields
                                     .credit_card.instrument_id()),
            VirtualCardEnrollmentSource::kDownstream));
  }

 protected:
  payments::TestPaymentsNetworkInterface& payments_network_interface() {
    return *autofill_client_->GetPaymentsAutofillClient()
                ->GetPaymentsNetworkInterface();
  }
  TestPersonalDataManager& personal_data_manager() {
    return *autofill_client_->GetPersonalDataManager();
  }
  PrefService* user_prefs() { return autofill_client_->GetPrefs(); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  syncer::TestSyncService sync_service_;
  std::unique_ptr<TestAutofillClient> autofill_client_;
  std::unique_ptr<TestVirtualCardEnrollmentManager>
      virtual_card_enrollment_manager_;

  // The global CreditCard used throughout the tests. Each test that needs to
  // use it will set it up for the specific test before testing it.
  std::unique_ptr<CreditCard> card_;
};

TEST_F(VirtualCardEnrollmentManagerTest, InitVirtualCardEnroll) {
  for (VirtualCardEnrollmentSource virtual_card_enrollment_source :
       {VirtualCardEnrollmentSource::kUpstream,
        VirtualCardEnrollmentSource::kDownstream,
        VirtualCardEnrollmentSource::kSettingsPage}) {
    for (bool make_image_present : {true, false}) {
      SCOPED_TRACE(testing::Message()
                   << " virtual_card_enrollment_source="
                   << static_cast<int>(virtual_card_enrollment_source)
                   << ", make_image_present=" << make_image_present);
      personal_data_manager()
          .test_payments_data_manager()
          .ClearCreditCardArtImages();
      SetUpCard();
      auto* state = virtual_card_enrollment_manager_
                        ->GetVirtualCardEnrollmentProcessState();
      state->risk_data.reset();
      state->virtual_card_enrollment_fields.card_art_image = nullptr;
      if (make_image_present)
        SetValidCardArtImageForCard(*card_);
#if BUILDFLAG(IS_ANDROID)
      virtual_card_enrollment_manager_->SetAutofillClient(nullptr);
#endif

      virtual_card_enrollment_manager_->InitVirtualCardEnroll(
          *card_, virtual_card_enrollment_source, std::nullopt,
          virtual_card_enrollment_manager_->AutofillClientIsPresent()
              ? user_prefs()
              : nullptr,
          base::DoNothing());

      // CreditCard class overloads equality operator to check that GUIDs,
      // origins, and the contents of the two cards are equal.
      EXPECT_EQ(*card_, state->virtual_card_enrollment_fields.credit_card);
      EXPECT_EQ(
          make_image_present,
          state->virtual_card_enrollment_fields.card_art_image != nullptr);
      EXPECT_TRUE(state->risk_data.has_value());

      // Reset to avoid that state keeps track of card art images that will be
      // invalidated at the start of the next loop.
      virtual_card_enrollment_manager_
          ->ResetVirtualCardEnrollmentProcessState();
    }
  }
}

TEST_F(VirtualCardEnrollmentManagerTest,
       InitVirtualCardEnroll_GetDetailsForEnrollmentResponseReceived) {
  personal_data_manager()
      .test_payments_data_manager()
      .ClearCreditCardArtImages();
  SetUpCard();
  auto* state =
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState();
  state->risk_data.reset();
  SetValidCardArtImageForCard(*card_);
  payments::PaymentsNetworkInterface::GetDetailsForEnrollmentResponseDetails
      get_details_for_enrollment_response_details;
  TestLegalMessageLine google_test_legal_message_line{
      "google_test_legal_message"};
  TestLegalMessageLine issuer_test_legal_message_line{
      "issuer_test_legal_message"};
  get_details_for_enrollment_response_details.google_legal_message = {
      google_test_legal_message_line};
  get_details_for_enrollment_response_details.issuer_legal_message = {
      issuer_test_legal_message_line};
  get_details_for_enrollment_response_details.vcn_context_token =
      "vcn_context_token";
  std::optional<payments::PaymentsNetworkInterface::
                    GetDetailsForEnrollmentResponseDetails>
      get_details_for_enrollment_response_details_optional =
          get_details_for_enrollment_response_details;
  virtual_card_enrollment_manager_->InitVirtualCardEnroll(
      *card_, VirtualCardEnrollmentSource::kUpstream,
      get_details_for_enrollment_response_details_optional);

  // CreditCard class overloads equality operator to check that GUIDs,
  // origins, and the contents of the two cards are equal.
  EXPECT_EQ(*card_, state->virtual_card_enrollment_fields.credit_card);
  EXPECT_TRUE(state->virtual_card_enrollment_fields.card_art_image != nullptr);
  EXPECT_TRUE(state->risk_data.has_value());
  EXPECT_EQ(google_test_legal_message_line.text(),
            get_details_for_enrollment_response_details.google_legal_message[0]
                .text());
  EXPECT_EQ(issuer_test_legal_message_line.text(),
            get_details_for_enrollment_response_details.issuer_legal_message[0]
                .text());
  EXPECT_TRUE(state->vcn_context_token.has_value());
  EXPECT_EQ(state->vcn_context_token.value(),
            get_details_for_enrollment_response_details.vcn_context_token);
}

TEST_F(VirtualCardEnrollmentManagerTest, OnRiskDataLoadedForVirtualCard) {
  base::HistogramTester histogram_tester;
  VirtualCardEnrollmentProcessState* state =
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState();
  state->virtual_card_enrollment_fields.virtual_card_enrollment_source =
      VirtualCardEnrollmentSource::kUpstream;
  SetUpCard();
  state->virtual_card_enrollment_fields.credit_card = *card_;
  state->risk_data.reset();

  virtual_card_enrollment_manager_->OnRiskDataLoadedForVirtualCard(
      kTestRiskData);

  payments::PaymentsNetworkInterface::GetDetailsForEnrollmentRequestDetails
      request_details = payments_network_interface()
                            .get_details_for_enrollment_request_details();

  EXPECT_EQ(request_details.risk_data, state->risk_data.value_or(""));
  EXPECT_EQ(request_details.app_locale, personal_data_manager().app_locale());
  EXPECT_EQ(request_details.instrument_id,
            state->virtual_card_enrollment_fields.credit_card.instrument_id());
  EXPECT_EQ(request_details.billing_customer_number,
            payments::GetBillingCustomerId(
                &personal_data_manager().payments_data_manager()));
  EXPECT_EQ(
      request_details.source,
      state->virtual_card_enrollment_fields.virtual_card_enrollment_source);
  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCard.GetDetailsForEnrollment.Attempt.Upstream",
      /*sample=*/true, 1);
}

TEST_F(VirtualCardEnrollmentManagerTest, OnDidGetDetailsForEnrollResponse) {
  base::HistogramTester histogram_tester;
  const TestLegalMessageLine google_legal_message =
      TestLegalMessageLine("google_test_legal_message");
  const TestLegalMessageLine issuer_legal_message =
      TestLegalMessageLine("issuer_test_legal_message");
  for (VirtualCardEnrollmentSource source :
       {VirtualCardEnrollmentSource::kUpstream,
        VirtualCardEnrollmentSource::kDownstream,
        VirtualCardEnrollmentSource::kSettingsPage}) {
// TODO(crbug.com/40223706): Makes the following test
// PersonalDataManagerTest.AddUpdateRemoveCreditCards fail on iOS.
// That other test fails when SetNetworkImageInResourceBundle is called here.
#if BUILDFLAG(IS_IOS)
    for (bool make_image_present : {true}) {
#else
    for (bool make_image_present : {true, false}) {
#endif  // BUILDFLAG(IS_IOS)
      virtual_card_enrollment_manager_
          ->get_details_for_enrollment_request_sent_timestamp_ =
          base::Time::Now();
      payments::PaymentsNetworkInterface::GetDetailsForEnrollmentResponseDetails
          response = std::move(SetUpOnDidGetDetailsForEnrollResponse(
              google_legal_message, issuer_legal_message, make_image_present));
      auto* state = virtual_card_enrollment_manager_
                        ->GetVirtualCardEnrollmentProcessState();
      state->virtual_card_enrollment_fields.virtual_card_enrollment_source =
          source;

      NiceMock<ui::MockResourceBundleDelegate> delegate;
      ui::ResourceBundle* orig_resource_bundle = nullptr;

      gfx::Image network_image;
      if (!make_image_present) {
        network_image = gfx::test::CreateImage(32, 30);
        orig_resource_bundle =
            ui::ResourceBundle::SwapSharedInstanceForTesting(nullptr);
        SetNetworkImageInResourceBundle(
            &delegate,
            state->virtual_card_enrollment_fields.credit_card.network(),
            network_image);
      }

      task_environment_.FastForwardBy(base::Milliseconds(5));

      virtual_card_enrollment_manager_->OnDidGetDetailsForEnrollResponse(
          payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
          response);

      EXPECT_TRUE(state->vcn_context_token.has_value());
      EXPECT_EQ(state->vcn_context_token, response.vcn_context_token);
      VirtualCardEnrollmentFields virtual_card_enrollment_fields =
          virtual_card_enrollment_manager_
              ->GetVirtualCardEnrollmentProcessState()
              ->virtual_card_enrollment_fields;
      EXPECT_TRUE(
          virtual_card_enrollment_fields.google_legal_message[0].text() ==
          google_legal_message.text());
      EXPECT_TRUE(
          virtual_card_enrollment_fields.issuer_legal_message[0].text() ==
          issuer_legal_message.text());

      // The |card_art_image| should always be present. If there is no card art
      // image available, it should be set to the network image.
      EXPECT_TRUE(virtual_card_enrollment_fields.card_art_image != nullptr);
      if (!make_image_present) {
        EXPECT_TRUE(
            virtual_card_enrollment_fields.card_art_image->BackedBySameObjectAs(
                network_image.AsImageSkia()));
      }
      histogram_tester.ExpectUniqueSample(
          "Autofill.VirtualCard.GetDetailsForEnrollment.Result." +
              VirtualCardEnrollmentSourceToMetricSuffix(source),
          /*sample=*/true, make_image_present ? 1 : 2);
      histogram_tester.ExpectBucketCount(
          "Autofill.VirtualCard.GetDetailsForEnrollment.Latency." +
              VirtualCardEnrollmentSourceToMetricSuffix(source) +
              PaymentsRpcResultToMetricsSuffix(
                  payments::PaymentsAutofillClient::PaymentsRpcResult::
                      kSuccess),
          /*sample=*/5, make_image_present ? 1 : 2);

      // Avoid dangling pointers to artwork.
      virtual_card_enrollment_manager_
          ->ResetVirtualCardEnrollmentProcessState();
      if (!make_image_present) {
        ui::ResourceBundle::CleanupSharedInstance();
        ui::ResourceBundle::SwapSharedInstanceForTesting(orig_resource_bundle);
      }
    }
  }
}

TEST_F(VirtualCardEnrollmentManagerTest,
       OnDidGetDetailsForEnrollResponse_NoAutofillClient) {
  base::HistogramTester histogram_tester;
  const TestLegalMessageLine google_legal_message =
      TestLegalMessageLine("google_test_legal_message");
  const TestLegalMessageLine issuer_legal_message =
      TestLegalMessageLine("issuer_test_legal_message");
  payments::PaymentsNetworkInterface::GetDetailsForEnrollmentResponseDetails
      response = std::move(SetUpOnDidGetDetailsForEnrollResponse(
          google_legal_message, issuer_legal_message,
          /*make_image_present=*/true));
  auto* state =
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState();
  state->virtual_card_enrollment_fields.virtual_card_enrollment_source =
      VirtualCardEnrollmentSource::kSettingsPage;

  virtual_card_enrollment_manager_->SetAutofillClient(nullptr);
  base::MockCallback<TestVirtualCardEnrollmentManager::
                         VirtualCardEnrollmentFieldsLoadedCallback>
      virtual_card_enrollment_fields_loaded_callback;
  virtual_card_enrollment_manager_
      ->SetVirtualCardEnrollmentFieldsLoadedCallback(
          virtual_card_enrollment_fields_loaded_callback.Get());
  EXPECT_CALL(virtual_card_enrollment_fields_loaded_callback, Run(_));
  virtual_card_enrollment_manager_->OnDidGetDetailsForEnrollResponse(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess, response);

  EXPECT_TRUE(state->vcn_context_token.has_value());
  EXPECT_EQ(state->vcn_context_token, response.vcn_context_token);
  VirtualCardEnrollmentFields virtual_card_enrollment_fields =
      state->virtual_card_enrollment_fields;
  EXPECT_TRUE(virtual_card_enrollment_fields.google_legal_message[0].text() ==
              google_legal_message.text());
  EXPECT_TRUE(virtual_card_enrollment_fields.issuer_legal_message[0].text() ==
              issuer_legal_message.text());
  EXPECT_TRUE(virtual_card_enrollment_fields.card_art_image != nullptr);

  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCard.GetDetailsForEnrollment.Result.SettingsPage",
      /*sample=*/true, 1);
}

TEST_F(VirtualCardEnrollmentManagerTest,
       OnDidGetDetailsForEnrollResponse_Reset) {
  base::HistogramTester histogram_tester;
  auto* state =
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState();
  state->virtual_card_enrollment_fields.virtual_card_enrollment_source =
      VirtualCardEnrollmentSource::kSettingsPage;
  for (payments::PaymentsAutofillClient::PaymentsRpcResult result :
       {payments::PaymentsAutofillClient::PaymentsRpcResult::
            kVcnRetrievalTryAgainFailure,
        payments::PaymentsAutofillClient::PaymentsRpcResult::
            kVcnRetrievalPermanentFailure}) {
    virtual_card_enrollment_manager_->SetResetCalled(false);

    virtual_card_enrollment_manager_->OnDidGetDetailsForEnrollResponse(
        result, payments::PaymentsNetworkInterface::
                    GetDetailsForEnrollmentResponseDetails());

    EXPECT_TRUE(virtual_card_enrollment_manager_->GetResetCalled());
  }

  // Ensure the clank settings page use-case works as expected.
  virtual_card_enrollment_manager_->SetAutofillClient(nullptr);
  for (payments::PaymentsAutofillClient::PaymentsRpcResult result :
       {payments::PaymentsAutofillClient::PaymentsRpcResult::
            kVcnRetrievalTryAgainFailure,
        payments::PaymentsAutofillClient::PaymentsRpcResult::
            kVcnRetrievalPermanentFailure}) {
    virtual_card_enrollment_manager_->SetResetCalled(false);

    virtual_card_enrollment_manager_->OnDidGetDetailsForEnrollResponse(
        result, payments::PaymentsNetworkInterface::
                    GetDetailsForEnrollmentResponseDetails());

    EXPECT_TRUE(virtual_card_enrollment_manager_->GetResetCalled());
  }
  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCard.GetDetailsForEnrollment.Result.SettingsPage",
      /*sample=*/false, 4);
}

TEST_F(VirtualCardEnrollmentManagerTest, Enroll) {
  VirtualCardEnrollmentProcessState* state =
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState();
  state->vcn_context_token = kTestVcnContextToken;
  SetUpCard();
  SetValidCardArtImageForCard(*card_);
  personal_data_manager().test_payments_data_manager().SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  for (VirtualCardEnrollmentSource virtual_card_enrollment_source :
       {VirtualCardEnrollmentSource::kUpstream,
        VirtualCardEnrollmentSource::kDownstream,
        VirtualCardEnrollmentSource::kSettingsPage}) {
    base::HistogramTester histogram_tester;
    SCOPED_TRACE(testing::Message()
                 << " virtual_card_enrollment_source="
                 << static_cast<int>(virtual_card_enrollment_source));
    state->virtual_card_enrollment_fields.virtual_card_enrollment_source =
        virtual_card_enrollment_source;
    virtual_card_enrollment_manager_->SetPaymentsRpcResult(
        payments::PaymentsAutofillClient::PaymentsRpcResult::kNone);

    payments_network_interface().set_update_virtual_card_enrollment_result(
        payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess);
    virtual_card_enrollment_manager_->Enroll(
        /*virtual_card_enrollment_update_response_callback=*/std::nullopt);

    payments::PaymentsNetworkInterface::
        UpdateVirtualCardEnrollmentRequestDetails request_details =
            payments_network_interface()
                .update_virtual_card_enrollment_request_details();
    EXPECT_TRUE(request_details.vcn_context_token.has_value());
    EXPECT_EQ(request_details.vcn_context_token, kTestVcnContextToken);
    EXPECT_EQ(request_details.virtual_card_enrollment_source,
              virtual_card_enrollment_source);
    EXPECT_EQ(request_details.virtual_card_enrollment_request_type,
              VirtualCardEnrollmentRequestType::kEnroll);
    EXPECT_EQ(request_details.billing_customer_number, 123456);
    EXPECT_EQ(virtual_card_enrollment_manager_->GetPaymentsRpcResult(),
              payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess);

    std::string suffix;
    switch (virtual_card_enrollment_source) {
      case VirtualCardEnrollmentSource::kUpstream:
        suffix = "Upstream";
        break;
      case VirtualCardEnrollmentSource::kDownstream:
        suffix = "Downstream";
        break;
      case VirtualCardEnrollmentSource::kSettingsPage:
        suffix = "SettingsPage";
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }

    // Verifies the logging.
    histogram_tester.ExpectUniqueSample(
        "Autofill.VirtualCard.Enroll.Attempt." + suffix,
        /*sample=*/true, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.VirtualCard.Enroll.Result." + suffix,
        /*sample=*/true, 1);

    // Starts another request and makes sure it fails.
    payments_network_interface().set_update_virtual_card_enrollment_result(
        payments::PaymentsAutofillClient::PaymentsRpcResult::
            kVcnRetrievalPermanentFailure);
    virtual_card_enrollment_manager_->Enroll(
        /*virtual_card_enrollment_update_response_callback=*/std::nullopt);

    // Verifies the logging.
    histogram_tester.ExpectUniqueSample(
        "Autofill.VirtualCard.Enroll.Attempt." + suffix,
        /*sample=*/true, 2);
    histogram_tester.ExpectBucketCount(
        "Autofill.VirtualCard.Enroll.Result." + suffix,
        /*sample=*/false, 1);
  }
}

TEST_F(VirtualCardEnrollmentManagerTest, Unenroll) {
  base::HistogramTester histogram_tester;
  personal_data_manager().test_payments_data_manager().SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));
  virtual_card_enrollment_manager_->SetPaymentsRpcResult(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kNone);

  virtual_card_enrollment_manager_->Unenroll(
      /*instrument_id=*/9223372036854775807,
      /*virtual_card_enrollment_update_response_callback=*/std::nullopt);

  payments::PaymentsNetworkInterface::UpdateVirtualCardEnrollmentRequestDetails
      request_details = payments_network_interface()
                            .update_virtual_card_enrollment_request_details();
  EXPECT_EQ(request_details.virtual_card_enrollment_source,
            VirtualCardEnrollmentSource::kSettingsPage);
  EXPECT_EQ(request_details.virtual_card_enrollment_request_type,
            VirtualCardEnrollmentRequestType::kUnenroll);
  EXPECT_EQ(request_details.billing_customer_number, 123456);
  EXPECT_EQ(request_details.instrument_id, 9223372036854775807);

  // The request should not include a context token, and should succeed.
  EXPECT_FALSE(request_details.vcn_context_token.has_value());
  EXPECT_EQ(virtual_card_enrollment_manager_->GetPaymentsRpcResult(),
            payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess);

  // Verifies the logging.
  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCard.Unenroll.Attempt.SettingsPage",
      /*sample=*/true, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCard.Unenroll.Result.SettingsPage",
      /*sample=*/true, 1);

  // Starts another request and make sure it fails.
  payments_network_interface().set_update_virtual_card_enrollment_result(
      payments::PaymentsAutofillClient::PaymentsRpcResult::
          kVcnRetrievalPermanentFailure);
  virtual_card_enrollment_manager_->Unenroll(
      /*instrument_id=*/9223372036854775807,
      /*virtual_card_enrollment_update_response_callback=*/std::nullopt);

  // Verifies the logging.
  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCard.Unenroll.Attempt.SettingsPage",
      /*sample=*/true, 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCard.Unenroll.Result.SettingsPage",
      /*sample=*/false, 1);
}

#if !BUILDFLAG(IS_IOS)
TEST_F(VirtualCardEnrollmentManagerTest, StrikeDatabase_BubbleAccepted) {
  base::HistogramTester histogram_tester;
  SetUpStrikeDatabaseTest();

  VirtualCardEnrollmentProcessState* state =
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState();
  // Reject the bubble and log strike.
  virtual_card_enrollment_manager_->OnVirtualCardEnrollmentBubbleCancelled();

  histogram_tester.ExpectUniqueSample(
      "Autofill.StrikeDatabase.NthStrikeAdded.VirtualCardEnrollment",
      /*sample=*/1, /*count=*/1);
  EXPECT_EQ(
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentStrikeDatabase()
          ->GetStrikes(
              base::NumberToString(state->virtual_card_enrollment_fields
                                       .credit_card.instrument_id())),
      1);

  // Ensure a strike has been removed after enrollment accepted.
  virtual_card_enrollment_manager_->Enroll(
      /*virtual_card_enrollment_update_response_callback=*/std::nullopt);
  EXPECT_EQ(
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentStrikeDatabase()
          ->GetStrikes(
              base::NumberToString(state->virtual_card_enrollment_fields
                                       .credit_card.instrument_id())),
      0);

  histogram_tester.ExpectBucketCount(
      "Autofill.StrikeDatabase.StrikesPresentWhenVirtualCardEnrolled", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardEnrollmentStrikeDatabase." +
          VirtualCardEnrollmentSourceToMetricSuffix(
              state->virtual_card_enrollment_fields
                  .virtual_card_enrollment_source),
      VirtualCardEnrollmentStrikeDatabaseEvent::
          VIRTUAL_CARD_ENROLLMENT_STRIKE_DATABASE_STRIKES_CLEARED,
      1);
}

TEST_F(VirtualCardEnrollmentManagerTest, StrikeDatabase_BubbleCanceled) {
  base::HistogramTester histogram_tester;
  SetUpStrikeDatabaseTest();

  // Reject the bubble and log strike.
  virtual_card_enrollment_manager_->OnVirtualCardEnrollmentBubbleCancelled();

  histogram_tester.ExpectUniqueSample(
      "Autofill.StrikeDatabase.NthStrikeAdded.VirtualCardEnrollment",
      /*sample=*/1, /*count=*/1);

  VirtualCardEnrollmentProcessState* state =
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState();
  // Ensure a strike has been logged.
  EXPECT_EQ(
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentStrikeDatabase()
          ->GetStrikes(
              base::NumberToString(state->virtual_card_enrollment_fields
                                       .credit_card.instrument_id())),
      1);

  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardEnrollmentStrikeDatabase." +
          VirtualCardEnrollmentSourceToMetricSuffix(
              state->virtual_card_enrollment_fields
                  .virtual_card_enrollment_source),
      VirtualCardEnrollmentStrikeDatabaseEvent::
          VIRTUAL_CARD_ENROLLMENT_STRIKE_DATABASE_STRIKE_LOGGED,
      1);
}

TEST_F(VirtualCardEnrollmentManagerTest, StrikeDatabase_BubbleBlocked) {
  base::HistogramTester histogram_tester;
  SetUpStrikeDatabaseTest();

  for (int i = 0; i < virtual_card_enrollment_manager_
                          ->GetVirtualCardEnrollmentStrikeDatabase()
                          ->GetMaxStrikesLimit();
       i++) {
    // Reject the bubble and log strike.
    virtual_card_enrollment_manager_->OnVirtualCardEnrollmentBubbleCancelled();

    histogram_tester.ExpectBucketCount(
        "Autofill.StrikeDatabase.NthStrikeAdded.VirtualCardEnrollment",
        /*sample=*/i + 1, /*count=*/1);

    for (VirtualCardEnrollmentSource source :
         {VirtualCardEnrollmentSource::kUpstream,
          VirtualCardEnrollmentSource::kDownstream}) {
      histogram_tester.ExpectBucketCount(
          "Autofill.VirtualCardEnrollBubble.MaxStrikesLimitReached", source, 0);
      histogram_tester.ExpectBucketCount(
          "Autofill.StrikeDatabase."
          "VirtualCardEnrollmentNotOfferedDueToMaxStrikes",
          source, 0);
    }
  }

  for (VirtualCardEnrollmentSource source :
       {VirtualCardEnrollmentSource::kUpstream,
        VirtualCardEnrollmentSource::kDownstream}) {
    virtual_card_enrollment_manager_->InitVirtualCardEnroll(
        *card_, source, std::nullopt,
        virtual_card_enrollment_manager_->AutofillClientIsPresent()
            ? user_prefs()
            : nullptr,
        base::DoNothing());
    histogram_tester.ExpectBucketCount(
        "Autofill.VirtualCardEnrollBubble.MaxStrikesLimitReached", source, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.StrikeDatabase."
        "VirtualCardEnrollmentNotOfferedDueToMaxStrikes",
        source, 1);
  }

  VirtualCardEnrollmentProcessState* state =
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState();
  EXPECT_TRUE(
      virtual_card_enrollment_manager_->ShouldBlockVirtualCardEnrollment(
          base::NumberToString(state->virtual_card_enrollment_fields.credit_card
                                   .instrument_id()),
          VirtualCardEnrollmentSource::kUpstream));
  EXPECT_TRUE(
      virtual_card_enrollment_manager_->ShouldBlockVirtualCardEnrollment(
          base::NumberToString(state->virtual_card_enrollment_fields.credit_card
                                   .instrument_id()),
          VirtualCardEnrollmentSource::kDownstream));
}

TEST_F(VirtualCardEnrollmentManagerTest,
       StrikeDatabase_EnrollmentAttemptFailed) {
  base::HistogramTester histogram_tester;
  SetUpStrikeDatabaseTest();

  std::vector<payments::PaymentsAutofillClient::PaymentsRpcResult>
      failure_results = {
          payments::PaymentsAutofillClient::PaymentsRpcResult::kTryAgainFailure,
          payments::PaymentsAutofillClient::PaymentsRpcResult::
              kPermanentFailure,
          payments::PaymentsAutofillClient::PaymentsRpcResult::
              kClientSideTimeout,
      };

  VirtualCardEnrollmentProcessState* state =
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState();

  for (int i = 0; i < static_cast<int>(failure_results.size()); i++) {
    virtual_card_enrollment_manager_
        ->OnDidGetUpdateVirtualCardEnrollmentResponse(
            VirtualCardEnrollmentRequestType::kEnroll, failure_results[i]);
    histogram_tester.ExpectBucketCount(
        "Autofill.StrikeDatabase.NthStrikeAdded.VirtualCardEnrollment",
        /*sample=*/i + 1, /*count=*/1);

    EXPECT_EQ(virtual_card_enrollment_manager_
                  ->GetVirtualCardEnrollmentStrikeDatabase()
                  ->GetStrikes(
                      base::NumberToString(state->virtual_card_enrollment_fields
                                               .credit_card.instrument_id())),
              i + 1);

    histogram_tester.ExpectBucketCount(
        "Autofill.VirtualCardEnrollmentStrikeDatabase." +
            VirtualCardEnrollmentSourceToMetricSuffix(
                state->virtual_card_enrollment_fields
                    .virtual_card_enrollment_source),
        VirtualCardEnrollmentStrikeDatabaseEvent::
            VIRTUAL_CARD_ENROLLMENT_STRIKE_DATABASE_STRIKE_LOGGED,
        i + 1);
  }
}

TEST_F(VirtualCardEnrollmentManagerTest,
       StrikeDatabase_SettingsPageNotBlocked) {
  SetUpStrikeDatabaseTest();
  base::HistogramTester histogram_tester;

  for (int i = 0; i < virtual_card_enrollment_manager_
                          ->GetVirtualCardEnrollmentStrikeDatabase()
                          ->GetMaxStrikesLimit();
       i++) {
    // Reject the bubble and log strike.
    virtual_card_enrollment_manager_->OnVirtualCardEnrollmentBubbleCancelled();

    histogram_tester.ExpectBucketCount(
        "Autofill.StrikeDatabase.NthStrikeAdded.VirtualCardEnrollment",
        /*sample=*/i + 1, /*count=*/1);
  }

  // Make sure enrollment is not blocked through settings page.
  EXPECT_FALSE(
      virtual_card_enrollment_manager_->ShouldBlockVirtualCardEnrollment(
          base::NumberToString(
              virtual_card_enrollment_manager_
                  ->GetVirtualCardEnrollmentProcessState()
                  ->virtual_card_enrollment_fields.credit_card.instrument_id()),
          VirtualCardEnrollmentSource::kSettingsPage));
}

// Test to ensure that the |last_show| inside a VirtualCardEnrollmentFields is
// set correctly.
TEST_F(VirtualCardEnrollmentManagerTest, VirtualCardEnrollmentFields_LastShow) {
  base::HistogramTester histogram_tester;
  VirtualCardEnrollmentProcessState* state =
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState();
  state->vcn_context_token = kTestVcnContextToken;
  SetUpCard();
  state->virtual_card_enrollment_fields.credit_card = *card_;
  personal_data_manager().test_payments_data_manager().SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>("123456"));

  // Making sure there is no existing strike for the card.
  ASSERT_EQ(
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentStrikeDatabase()
          ->GetStrikes(
              base::NumberToString(state->virtual_card_enrollment_fields
                                       .credit_card.instrument_id())),
      0);

  for (int i = 0; i < virtual_card_enrollment_manager_
                              ->GetVirtualCardEnrollmentStrikeDatabase()
                              ->GetMaxStrikesLimit() -
                          1;
       i++) {
    // Show the bubble and ensures VirtualCardEnrollmentFields is set correctly.
    virtual_card_enrollment_manager_->ShowVirtualCardEnrollBubble();
    EXPECT_FALSE(state->virtual_card_enrollment_fields.last_show);
    // Reject the bubble and log strike.
    virtual_card_enrollment_manager_->OnVirtualCardEnrollmentBubbleCancelled();

    histogram_tester.ExpectBucketCount(
        "Autofill.StrikeDatabase.NthStrikeAdded.VirtualCardEnrollment",
        /*sample=*/i + 1, /*count=*/1);
  }

  // Show the bubble for the last time and ensures VirtualCardEnrollmentFields
  // is set correctly.
  virtual_card_enrollment_manager_->ShowVirtualCardEnrollBubble();
  EXPECT_TRUE(state->virtual_card_enrollment_fields.last_show);
}

// Test to ensure that the required delay since the last strike is respected
// before Chrome offers another virtual card enrollment for the card.
TEST_F(VirtualCardEnrollmentManagerTest, RequiredDelaySinceLastStrike) {
  base::HistogramTester histogram_tester;
  SetUpStrikeDatabaseTest();
  VirtualCardEnrollmentProcessState* state =
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState();
  SetUpCard();
  card_->set_instrument_id(11223344);
  state->virtual_card_enrollment_fields.credit_card = *card_;

  virtual_card_enrollment_manager_->InitVirtualCardEnroll(
      *card_, VirtualCardEnrollmentSource::kDownstream, std::nullopt,
      virtual_card_enrollment_manager_->AutofillClientIsPresent() ? user_prefs()
                                                                  : nullptr,
      base::DoNothing());

  // Logs one strike for the card and makes sure that the enrollment offer is
  // blocked.
  virtual_card_enrollment_manager_->OnVirtualCardEnrollmentBubbleCancelled();

  histogram_tester.ExpectUniqueSample(
      "Autofill.StrikeDatabase.NthStrikeAdded.VirtualCardEnrollment",
      /*sample=*/1, /*count=*/1);
  EXPECT_TRUE(
      virtual_card_enrollment_manager_->ShouldBlockVirtualCardEnrollment(
          base::NumberToString(card_->instrument_id()),
          VirtualCardEnrollmentSource::kDownstream));

  // Advances the clock for `kEnrollmentEnforcedDelayInDays` - 1 days. Verifies
  // that enrollment should still be blocked.
  task_environment_.FastForwardBy(
      base::Days(kEnrollmentEnforcedDelayInDays - 1));
  EXPECT_TRUE(
      virtual_card_enrollment_manager_->ShouldBlockVirtualCardEnrollment(
          base::NumberToString(card_->instrument_id()),
          VirtualCardEnrollmentSource::kDownstream));

  // Makes sure that enrollment offer for another card is not blocked.
  EXPECT_FALSE(
      virtual_card_enrollment_manager_->ShouldBlockVirtualCardEnrollment(
          base::NumberToString(55667788),
          VirtualCardEnrollmentSource::kDownstream));

  // Advances the clock for another days. Verifies that enrollment should not
  // be blocked.
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_FALSE(
      virtual_card_enrollment_manager_->ShouldBlockVirtualCardEnrollment(
          base::NumberToString(card_->instrument_id()),
          VirtualCardEnrollmentSource::kDownstream));
  histogram_tester.ExpectBucketCount(
      "Autofill.StrikeDatabase."
      "VirtualCardEnrollmentNotOfferedDueToRequiredDelay",
      VirtualCardEnrollmentSource::kDownstream, 2);
}

#endif  // !BUILDFLAG(IS_IOS)

TEST_F(VirtualCardEnrollmentManagerTest, Metrics_LatencySinceUpstream) {
  base::HistogramTester histogram_tester;
  virtual_card_enrollment_manager_->SetSaveCardBubbleAcceptedTimestamp(
      base::Time::Now());
  virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState()
      ->virtual_card_enrollment_fields.virtual_card_enrollment_source =
      VirtualCardEnrollmentSource::kUpstream;
  task_environment_.FastForwardBy(base::Minutes(1));
  virtual_card_enrollment_manager_->ShowVirtualCardEnrollBubble();
  histogram_tester.ExpectTimeBucketCount(
      "Autofill.VirtualCardEnrollBubble.LatencySinceUpstream", base::Minutes(1),
      1);
}

}  // namespace autofill
