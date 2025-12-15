// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"
#include "components/autofill/core/browser/payments/multiple_request_payments_network_interface_base.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_requests/update_virtual_card_enrollment_request.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/payments/test/mock_multiple_request_payments_network_interface.h"
#include "components/autofill/core/browser/payments/test/mock_virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "components/autofill/core/browser/payments/test_payments_network_interface.h"
#include "components/autofill/core/browser/payments/test_virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "components/autofill/core/browser/strike_databases/payments/test_strike_database.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
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
    personal_data_manager().SetSyncServiceForTest(&sync_service_);
    autofill_client_->GetPaymentsAutofillClient()
        ->set_payments_network_interface(
            std::make_unique<payments::TestPaymentsNetworkInterface>(
                autofill_client_->GetURLLoaderFactory(),
                autofill_client_->GetIdentityManager(),
                &personal_data_manager()));
    autofill_client_->GetPaymentsAutofillClient()
        ->set_multiple_request_payments_network_interface(
            std::make_unique<
                payments::MockMultipleRequestPaymentsNetworkInterface>(
                autofill_client_->GetURLLoaderFactory(),
                *autofill_client_->GetIdentityManager()));
    autofill_client_->set_test_strike_database(
        std::make_unique<TestStrikeDatabase>());

    if (base::FeatureList::IsEnabled(
            features::
                kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollment)) {
      virtual_card_enrollment_manager_ =
          std::make_unique<TestVirtualCardEnrollmentManager>(
              &payments_data_manager(),
              &multiple_request_payments_network_interface(),
              autofill_client_.get());
    } else {
      virtual_card_enrollment_manager_ =
          std::make_unique<TestVirtualCardEnrollmentManager>(
              &payments_data_manager(), &payments_network_interface(),
              autofill_client_.get());
    }

    SetUpCard();
  }

  void SetUpCard() {
    card_ = std::make_unique<CreditCard>(test::GetMaskedServerCard());
    card_->set_card_art_url(GURL("http://www.example.com/image.png"));
    card_->set_instrument_id(112233445566);
    card_->set_guid("00000000-0000-0000-0000-000000000001");
    payments_data_manager().AddServerCreditCard(*card_.get());
  }

  void SetValidCardArtImageForCard(const CreditCard& card) {
    payments_data_manager().CacheImage(card.card_art_url(),
                                       gfx::test::CreateImage(40, 24));
  }

  void SetNetworkImageInResourceBundle(ui::MockResourceBundleDelegate* delegate,
                                       const std::string& network,
                                       const gfx::Image& network_image) {
    int resource_id = CreditCard::IconResourceId(network);
    ON_CALL(*delegate, GetImageNamed(resource_id))
        .WillByDefault(testing::Return(network_image));

    // Cache the image so that the ui::ResourceBundle::GetImageSkiaNamed()
    // call in VirtualCardEnrollmentManager can retrieve it.
    ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
  }

  payments::GetDetailsForEnrollmentResponseDetails
  SetUpOnDidGetDetailsForEnrollResponse(
      const TestLegalMessageLine& google_legal_message,
      const TestLegalMessageLine& issuer_legal_message,
      bool make_image_present) {
    payments_data_manager().ClearCachedImages();
    if (make_image_present) {
      CHECK(card_);
      SetValidCardArtImageForCard(*card_);
    }

    payments::GetDetailsForEnrollmentResponseDetails response;
    response.vcn_context_token = kTestVcnContextToken;
    response.google_legal_message = {google_legal_message};
    response.issuer_legal_message = {issuer_legal_message};
    return response;
  }

  // TODO(crbug.com/303715506): This part does not test the desired behavior on
  // iOS as the virtual card enrollment strikedatabase on iOS is not initialized
  // (guarded by the feature flag).
  // Strike database tests bypass the InitVirtualCardEnroll call. Hence set the
  // VirtualCardEnrollmentProcessState specifically.
  void SetUpStrikeDatabaseTest() {
    VirtualCardEnrollmentProcessState* state =
        virtual_card_enrollment_manager_
            ->GetVirtualCardEnrollmentProcessState();
    state->vcn_context_token = kTestVcnContextToken;
    state->virtual_card_enrollment_fields.credit_card = *card_;
    state->virtual_card_enrollment_fields.virtual_card_enrollment_source =
        VirtualCardEnrollmentSource::kDownstream;
    payments_data_manager().SetPaymentsCustomerData(
        std::make_unique<PaymentsCustomerData>("123456"));
  }

 protected:
  TestPaymentsDataManager& payments_data_manager() {
    return personal_data_manager().test_payments_data_manager();
  }
  payments::TestPaymentsNetworkInterface& payments_network_interface() {
    return static_cast<payments::TestPaymentsNetworkInterface&>(
        *autofill_client_->GetPaymentsAutofillClient()
             ->GetPaymentsNetworkInterface());
  }
  payments::MockMultipleRequestPaymentsNetworkInterface&
  multiple_request_payments_network_interface() {
    return *autofill_client_->GetPaymentsAutofillClient()
                ->GetMultipleRequestPaymentsNetworkInterface();
  }
  TestPersonalDataManager& personal_data_manager() {
    return autofill_client_->GetPersonalDataManager();
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

TEST_F(VirtualCardEnrollmentManagerTest,
       InitVirtualCardEnroll_GetDetailsForEnrollmentResponseReceived) {
  payments_data_manager().ClearCachedImages();
  auto* state =
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState();
  state->risk_data.reset();
  SetValidCardArtImageForCard(*card_);
  payments::GetDetailsForEnrollmentResponseDetails
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
  std::optional<payments::GetDetailsForEnrollmentResponseDetails>
      get_details_for_enrollment_response_details_optional =
          get_details_for_enrollment_response_details;
  virtual_card_enrollment_manager_->InitVirtualCardEnroll(
      *card_, VirtualCardEnrollmentSource::kUpstream, base::DoNothing(),
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
  state->virtual_card_enrollment_fields.credit_card = *card_;
  state->risk_data.reset();
  payments::GetDetailsForEnrollmentRequestDetails request_details;
  if (base::FeatureList::IsEnabled(
          features::
              kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollment)) {
    EXPECT_CALL(multiple_request_payments_network_interface(),
                GetVirtualCardEnrollmentDetails)
        .WillOnce(
            testing::DoAll(testing::SaveArg<0>(&request_details),
                           testing::Return(payments::RequestId("11223344"))));
  }

  virtual_card_enrollment_manager_->OnRiskDataLoadedForVirtualCard(
      kTestRiskData);
  if (!base::FeatureList::IsEnabled(
          features::
              kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollment)) {
    request_details = payments_network_interface()
                          .get_details_for_enrollment_request_details();
  }

  EXPECT_EQ(request_details.risk_data, state->risk_data.value_or(""));
  EXPECT_EQ(request_details.app_locale, payments_data_manager().app_locale());
  EXPECT_EQ(request_details.instrument_id,
            state->virtual_card_enrollment_fields.credit_card.instrument_id());
  EXPECT_EQ(request_details.billing_customer_number,
            payments::GetBillingCustomerId(payments_data_manager()));
  EXPECT_EQ(
      request_details.source,
      state->virtual_card_enrollment_fields.virtual_card_enrollment_source);
  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCard.GetDetailsForEnrollment.Attempt.Upstream",
      /*sample=*/true, 1);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(VirtualCardEnrollmentManagerTest,
       OnDidGetDetailsForEnrollResponse_NoAutofillClient) {
  base::HistogramTester histogram_tester;
  const TestLegalMessageLine google_legal_message =
      TestLegalMessageLine("google_test_legal_message");
  const TestLegalMessageLine issuer_legal_message =
      TestLegalMessageLine("issuer_test_legal_message");
  payments::GetDetailsForEnrollmentResponseDetails response =
      std::move(SetUpOnDidGetDetailsForEnrollResponse(
          google_legal_message, issuer_legal_message,
          /*make_image_present=*/true));
  virtual_card_enrollment_manager_->SetAutofillClient(nullptr);
  base::MockCallback<TestVirtualCardEnrollmentManager::
                         VirtualCardEnrollmentFieldsLoadedCallback>
      virtual_card_enrollment_fields_loaded_callback;

  EXPECT_CALL(virtual_card_enrollment_fields_loaded_callback, Run(_));
  virtual_card_enrollment_manager_->InitVirtualCardEnroll(
      *card_, VirtualCardEnrollmentSource::kSettingsPage,
      virtual_card_enrollment_fields_loaded_callback.Get());
  virtual_card_enrollment_manager_->OnDidGetDetailsForEnrollResponse(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess, response);

  auto* state =
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

  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCard.GetDetailsForEnrollment.Result.SettingsPage",
      /*sample=*/true, 1);
}
#endif

TEST_F(VirtualCardEnrollmentManagerTest,
       OnDidGetDetailsForEnrollResponse_Reset) {
  base::HistogramTester histogram_tester;
  // Ignore strike database to avoid its required delay cooldown.
  virtual_card_enrollment_manager_->set_ignore_strike_database(true);

  for (payments::PaymentsAutofillClient::PaymentsRpcResult result :
       {payments::PaymentsAutofillClient::PaymentsRpcResult::
            kVcnRetrievalTryAgainFailure,
        payments::PaymentsAutofillClient::PaymentsRpcResult::
            kVcnRetrievalPermanentFailure}) {
    virtual_card_enrollment_manager_->InitVirtualCardEnroll(
        *card_, VirtualCardEnrollmentSource::kDownstream, base::DoNothing());
    virtual_card_enrollment_manager_->SetResetCalled(false);
    virtual_card_enrollment_manager_->OnDidGetDetailsForEnrollResponse(
        result, payments::GetDetailsForEnrollmentResponseDetails());
    EXPECT_TRUE(virtual_card_enrollment_manager_->GetResetCalled());
  }
  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCard.GetDetailsForEnrollment.Result.Downstream",
      /*sample=*/false, 2);

#if BUILDFLAG(IS_ANDROID)
  // Ensure the clank settings page use-case works as expected.
  virtual_card_enrollment_manager_->SetAutofillClient(nullptr);
  for (payments::PaymentsAutofillClient::PaymentsRpcResult result :
       {payments::PaymentsAutofillClient::PaymentsRpcResult::
            kVcnRetrievalTryAgainFailure,
        payments::PaymentsAutofillClient::PaymentsRpcResult::
            kVcnRetrievalPermanentFailure}) {
    virtual_card_enrollment_manager_->InitVirtualCardEnroll(
        *card_, VirtualCardEnrollmentSource::kSettingsPage, base::DoNothing());
    virtual_card_enrollment_manager_->SetResetCalled(false);
    virtual_card_enrollment_manager_->OnDidGetDetailsForEnrollResponse(
        result, payments::GetDetailsForEnrollmentResponseDetails());
    EXPECT_TRUE(virtual_card_enrollment_manager_->GetResetCalled());
  }
  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCard.GetDetailsForEnrollment.Result.SettingsPage",
      /*sample=*/false, 2);
#endif
}

TEST_F(VirtualCardEnrollmentManagerTest, Unenroll) {
  base::HistogramTester histogram_tester;
  payments_data_manager().SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));
  virtual_card_enrollment_manager_->SetPaymentsRpcResult(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kNone);
  payments::UpdateVirtualCardEnrollmentRequestDetails request_details;
  if (base::FeatureList::IsEnabled(
          features::
              kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollment)) {
    EXPECT_CALL(multiple_request_payments_network_interface(),
                UpdateVirtualCardEnrollment)
        .WillOnce([&](const payments::UpdateVirtualCardEnrollmentRequestDetails&
                          req,
                      base::OnceCallback<void(
                          payments::PaymentsAutofillClient::PaymentsRpcResult)>
                          callback) {
          // Action 1: Save the argument
          request_details = req;

          // Action 2: Run the callback
          std::move(callback).Run(
              payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess);

          // Action 3: Return the required RequestId
          return payments::RequestId("11223344");
        });
  }

  virtual_card_enrollment_manager_->Unenroll(
      /*instrument_id=*/9223372036854775807,
      /*virtual_card_enrollment_update_response_callback=*/std::nullopt);
  if (!base::FeatureList::IsEnabled(
          features::
              kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollment)) {
    request_details = payments_network_interface()
                          .update_virtual_card_enrollment_request_details();
  }

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
  if (base::FeatureList::IsEnabled(
          features::
              kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollment)) {
    EXPECT_CALL(multiple_request_payments_network_interface(),
                UpdateVirtualCardEnrollment)
        .WillOnce(
            [&](const payments::UpdateVirtualCardEnrollmentRequestDetails& req,
                base::OnceCallback<void(
                    payments::PaymentsAutofillClient::PaymentsRpcResult)>
                    callback) {
              // Action 1: Run the callback
              std::move(callback).Run(payments::PaymentsAutofillClient::
                                          PaymentsRpcResult::kPermanentFailure);

              // Action 2: Return the required RequestId
              return payments::RequestId("11223344");
            });
  } else {
    payments_network_interface().set_update_virtual_card_enrollment_result(
        payments::PaymentsAutofillClient::PaymentsRpcResult::
            kVcnRetrievalPermanentFailure);
  }
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

  virtual_card_enrollment_manager_
      ->AddStrikeToBlockOfferingVirtualCardEnrollment(
          base::NumberToString(card_->instrument_id()));
  EXPECT_EQ(
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentStrikeDatabase()
          ->GetStrikes(base::NumberToString(card_->instrument_id())),
      1);

  // Ensure a strike has been removed after enrollment accepted.
  virtual_card_enrollment_manager_->Enroll(
      /*virtual_card_enrollment_update_response_callback=*/std::nullopt);
  EXPECT_EQ(
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentStrikeDatabase()
          ->GetStrikes(base::NumberToString(card_->instrument_id())),
      0);

  histogram_tester.ExpectBucketCount(
      "Autofill.StrikeDatabase.StrikesPresentWhenVirtualCardEnrolled", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardEnrollmentStrikeDatabase." +
          VirtualCardEnrollmentSourceToMetricSuffix(
              VirtualCardEnrollmentSource::kDownstream),
      VirtualCardEnrollmentStrikeDatabaseEvent::
          VIRTUAL_CARD_ENROLLMENT_STRIKE_DATABASE_STRIKES_CLEARED,
      1);
}

TEST_F(VirtualCardEnrollmentManagerTest, StrikeDatabase_BubbleCanceled) {
  base::HistogramTester histogram_tester;
  SetUpStrikeDatabaseTest();

  // Log one strike for the card.
  virtual_card_enrollment_manager_
      ->AddStrikeToBlockOfferingVirtualCardEnrollment(
          base::NumberToString(card_->instrument_id()));

  histogram_tester.ExpectUniqueSample(
      "Autofill.StrikeDatabase.NthStrikeAdded.VirtualCardEnrollment",
      /*sample=*/1, /*count=*/1);

  // Ensure a strike has been logged.
  EXPECT_EQ(
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentStrikeDatabase()
          ->GetStrikes(base::NumberToString(card_->instrument_id())),
      1);

  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardEnrollmentStrikeDatabase." +
          VirtualCardEnrollmentSourceToMetricSuffix(
              VirtualCardEnrollmentSource::kDownstream),
      VirtualCardEnrollmentStrikeDatabaseEvent::
          VIRTUAL_CARD_ENROLLMENT_STRIKE_DATABASE_STRIKE_LOGGED,
      1);
}

TEST_F(VirtualCardEnrollmentManagerTest, StrikeDatabase_BubbleBlocked) {
  base::HistogramTester histogram_tester;
  SetUpStrikeDatabaseTest();
  EXPECT_FALSE(
      virtual_card_enrollment_manager_->ShouldBlockVirtualCardEnrollment(
          base::NumberToString(card_->instrument_id()),
          VirtualCardEnrollmentSource::kUpstream));
  EXPECT_FALSE(
      virtual_card_enrollment_manager_->ShouldBlockVirtualCardEnrollment(
          base::NumberToString(card_->instrument_id()),
          VirtualCardEnrollmentSource::kDownstream));

  for (int i = 0; i < virtual_card_enrollment_manager_
                          ->GetVirtualCardEnrollmentStrikeDatabase()
                          ->GetMaxStrikesLimit();
       i++) {
    virtual_card_enrollment_manager_
        ->AddStrikeToBlockOfferingVirtualCardEnrollment(
            base::NumberToString(card_->instrument_id()));

    histogram_tester.ExpectBucketCount(
        "Autofill.StrikeDatabase.NthStrikeAdded.VirtualCardEnrollment",
        /*sample=*/i + 1, /*expected_count=*/1);

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
        *card_, source, base::DoNothing(), std::nullopt,
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

  EXPECT_TRUE(
      virtual_card_enrollment_manager_->ShouldBlockVirtualCardEnrollment(
          base::NumberToString(card_->instrument_id()),
          VirtualCardEnrollmentSource::kUpstream));
  EXPECT_TRUE(
      virtual_card_enrollment_manager_->ShouldBlockVirtualCardEnrollment(
          base::NumberToString(card_->instrument_id()),
          VirtualCardEnrollmentSource::kDownstream));
}

TEST_F(VirtualCardEnrollmentManagerTest,
       StrikeDatabase_EnrollmentAttemptFailed) {
  base::HistogramTester histogram_tester;

  std::vector<payments::PaymentsAutofillClient::PaymentsRpcResult>
      failure_results = {
          payments::PaymentsAutofillClient::PaymentsRpcResult::kTryAgainFailure,
          payments::PaymentsAutofillClient::PaymentsRpcResult::
              kPermanentFailure,
          payments::PaymentsAutofillClient::PaymentsRpcResult::
              kClientSideTimeout,
      };

  for (int i = 0; i < static_cast<int>(failure_results.size()); i++) {
    SetUpStrikeDatabaseTest();
    virtual_card_enrollment_manager_
        ->OnDidGetUpdateVirtualCardEnrollmentResponse(
            VirtualCardEnrollmentRequestType::kEnroll, failure_results[i]);
    histogram_tester.ExpectBucketCount(
        "Autofill.StrikeDatabase.NthStrikeAdded.VirtualCardEnrollment",
        /*sample=*/i + 1, /*expected_count=*/1);

    EXPECT_EQ(virtual_card_enrollment_manager_
                  ->GetVirtualCardEnrollmentStrikeDatabase()
                  ->GetStrikes(base::NumberToString(card_->instrument_id())),
              i + 1);

    histogram_tester.ExpectBucketCount(
        "Autofill.VirtualCardEnrollmentStrikeDatabase." +
            VirtualCardEnrollmentSourceToMetricSuffix(
                VirtualCardEnrollmentSource::kDownstream),
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
    virtual_card_enrollment_manager_
        ->AddStrikeToBlockOfferingVirtualCardEnrollment(
            base::NumberToString(card_->instrument_id()));

    histogram_tester.ExpectBucketCount(
        "Autofill.StrikeDatabase.NthStrikeAdded.VirtualCardEnrollment",
        /*sample=*/i + 1, /*count=*/1);
  }

  // Make sure enrollment is not blocked through settings page.
  EXPECT_FALSE(
      virtual_card_enrollment_manager_->ShouldBlockVirtualCardEnrollment(
          base::NumberToString(card_->instrument_id()),
          VirtualCardEnrollmentSource::kSettingsPage));
}

// Test to ensure that the |last_show| inside a VirtualCardEnrollmentFields is
// set correctly.
TEST_F(VirtualCardEnrollmentManagerTest, VirtualCardEnrollmentFields_LastShow) {
  base::HistogramTester histogram_tester;
  VirtualCardEnrollmentProcessState* state =
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState();
  state->vcn_context_token = kTestVcnContextToken;
  state->virtual_card_enrollment_fields.credit_card = *card_;
  payments_data_manager().SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>("123456"));
  // Ignore strike database to avoid its required delay cooldown.
  virtual_card_enrollment_manager_->set_ignore_strike_database(true);

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
    // Start enrollment and ensures VirtualCardEnrollmentFields is set
    // correctly.
    virtual_card_enrollment_manager_->InitVirtualCardEnroll(
        *card_, VirtualCardEnrollmentSource::kUpstream, base::DoNothing());
    EXPECT_FALSE(state->virtual_card_enrollment_fields.last_show);
    // Log one strike for the card.
    virtual_card_enrollment_manager_
        ->AddStrikeToBlockOfferingVirtualCardEnrollment(
            base::NumberToString(card_->instrument_id()));

    histogram_tester.ExpectBucketCount(
        "Autofill.StrikeDatabase.NthStrikeAdded.VirtualCardEnrollment",
        /*sample=*/i + 1, /*expected_count=*/1);
  }

  // Start enrollment and ensures VirtualCardEnrollmentFields is set
  // correctly.
  virtual_card_enrollment_manager_->InitVirtualCardEnroll(
      *card_, VirtualCardEnrollmentSource::kUpstream, base::DoNothing());
  EXPECT_TRUE(state->virtual_card_enrollment_fields.last_show);
}

// Test to ensure that the required delay since the last strike is respected
// before Chrome offers another virtual card enrollment for the card.
TEST_F(VirtualCardEnrollmentManagerTest, RequiredDelaySinceLastStrike) {
  base::HistogramTester histogram_tester;
  SetUpStrikeDatabaseTest();
  VirtualCardEnrollmentProcessState* state =
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState();
  card_->set_instrument_id(11223344);
  state->virtual_card_enrollment_fields.credit_card = *card_;

  virtual_card_enrollment_manager_->InitVirtualCardEnroll(
      *card_, VirtualCardEnrollmentSource::kDownstream, base::DoNothing(),
      std::nullopt,
      virtual_card_enrollment_manager_->AutofillClientIsPresent() ? user_prefs()
                                                                  : nullptr,
      base::DoNothing());

  // Logs one strike for the card and makes sure that the enrollment offer is
  // blocked.
  virtual_card_enrollment_manager_
      ->AddStrikeToBlockOfferingVirtualCardEnrollment(
          base::NumberToString(card_->instrument_id()));

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
  virtual_card_enrollment_manager_->ShowVirtualCardEnrollBubble(
      &virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState()
           ->virtual_card_enrollment_fields);
  histogram_tester.ExpectTimeBucketCount(
      "Autofill.VirtualCardEnrollBubble.LatencySinceUpstream", base::Minutes(1),
      1);
}

class VirtualCardEnrollmentManagerParamTest
    : public VirtualCardEnrollmentManagerTest,
      public ::testing::WithParamInterface<VirtualCardEnrollmentSource> {
 public:
  VirtualCardEnrollmentSource source() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    VirtualCardEnrollmentManagerTest,
    VirtualCardEnrollmentManagerParamTest,
    ::testing::Values(VirtualCardEnrollmentSource::kUpstream,
                      VirtualCardEnrollmentSource::kDownstream,
                      VirtualCardEnrollmentSource::kSettingsPage));

TEST_P(VirtualCardEnrollmentManagerParamTest, InitVirtualCardEnroll) {
  for (bool make_image_present : {true, false}) {
    SCOPED_TRACE(testing::Message()
                 << ", make_image_present=" << make_image_present);
    payments_data_manager().ClearCachedImages();
    auto* state = virtual_card_enrollment_manager_
                      ->GetVirtualCardEnrollmentProcessState();
    state->risk_data.reset();
    state->virtual_card_enrollment_fields.card_art_image = nullptr;
    if (make_image_present) {
      SetValidCardArtImageForCard(*card_);
    }
#if BUILDFLAG(IS_ANDROID)
    virtual_card_enrollment_manager_->SetAutofillClient(nullptr);
#endif

    virtual_card_enrollment_manager_->InitVirtualCardEnroll(
        *card_, source(), base::DoNothing(), std::nullopt,
        virtual_card_enrollment_manager_->AutofillClientIsPresent()
            ? user_prefs()
            : nullptr,
        base::DoNothing());

    // CreditCard class overloads equality operator to check that GUIDs,
    // origins, and the contents of the two cards are equal.
    EXPECT_EQ(*card_, state->virtual_card_enrollment_fields.credit_card);
    EXPECT_EQ(make_image_present,
              state->virtual_card_enrollment_fields.card_art_image != nullptr);
    EXPECT_TRUE(state->risk_data.has_value());

    // Reset to avoid that state keeps track of card art images that will be
    // invalidated at the start of the next loop.
    virtual_card_enrollment_manager_->ResetVirtualCardEnrollmentProcessState();
  }
}

TEST_P(VirtualCardEnrollmentManagerParamTest, Enroll) {
  SetValidCardArtImageForCard(*card_);
  payments_data_manager().SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));
  for (payments::PaymentsRpcResult result :
       {payments::PaymentsRpcResult::kSuccess,
        payments::PaymentsRpcResult::kPermanentFailure}) {
    base::HistogramTester histogram_tester;
    SCOPED_TRACE(testing::Message()
                 << " Payments Rpc result= " << static_cast<int>(result));
    VirtualCardEnrollmentProcessState* state =
        virtual_card_enrollment_manager_
            ->GetVirtualCardEnrollmentProcessState();
    state->vcn_context_token = kTestVcnContextToken;
    state->virtual_card_enrollment_fields.credit_card = *card_;
    state->virtual_card_enrollment_fields.virtual_card_enrollment_source =
        source();
    virtual_card_enrollment_manager_->SetPaymentsRpcResult(
        payments::PaymentsAutofillClient::PaymentsRpcResult::kNone);
    payments::UpdateVirtualCardEnrollmentRequestDetails request_details;
    if (base::FeatureList::IsEnabled(
            features::
                kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollment)) {
      EXPECT_CALL(multiple_request_payments_network_interface(),
                  UpdateVirtualCardEnrollment)
          .WillOnce(
              [&](const payments::UpdateVirtualCardEnrollmentRequestDetails&
                      req,
                  base::OnceCallback<void(
                      payments::PaymentsAutofillClient::PaymentsRpcResult)>
                      callback) {
                // Action 1: Save the argument
                request_details = req;

                // Action 2: Run the callback
                std::move(callback).Run(result);

                // Action 3: Return the required RequestId
                return payments::RequestId("11223344");
              });
    } else {
      payments_network_interface().set_update_virtual_card_enrollment_result(
          result);
    }

    virtual_card_enrollment_manager_->Enroll(
        /*virtual_card_enrollment_update_response_callback=*/std::nullopt);
    if (!base::FeatureList::IsEnabled(
            features::
                kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollment)) {
      request_details = payments_network_interface()
                            .update_virtual_card_enrollment_request_details();
    }

    EXPECT_TRUE(request_details.vcn_context_token.has_value());
    EXPECT_EQ(request_details.vcn_context_token, kTestVcnContextToken);
    EXPECT_EQ(request_details.virtual_card_enrollment_source, source());
    EXPECT_EQ(request_details.virtual_card_enrollment_request_type,
              VirtualCardEnrollmentRequestType::kEnroll);
    EXPECT_EQ(request_details.billing_customer_number, 123456);
    EXPECT_EQ(virtual_card_enrollment_manager_->GetPaymentsRpcResult(), result);

    std::string suffix;
    switch (source()) {
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
        NOTREACHED();
    }
    // Verifies the logging.
    histogram_tester.ExpectUniqueSample(
        "Autofill.VirtualCard.Enroll.Attempt." + suffix,
        /*sample=*/true, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.VirtualCard.Enroll.Result." + suffix,
        /*sample=*/result ==
            payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
        1);
  }
}

TEST_P(VirtualCardEnrollmentManagerParamTest,
       OnDidGetDetailsForEnrollResponse) {
  base::HistogramTester histogram_tester;
  const TestLegalMessageLine google_legal_message =
      TestLegalMessageLine("google_test_legal_message");
  const TestLegalMessageLine issuer_legal_message =
      TestLegalMessageLine("issuer_test_legal_message");
  // Ignore strike database to avoid its required delay cooldown.
  virtual_card_enrollment_manager_->set_ignore_strike_database(true);
// TODO(crbug.com/40223706): Makes the following test
// PersonalDataManagerTest.AddUpdateRemoveCreditCards fail on iOS.
// That other test fails when SetNetworkImageInResourceBundle is called here.
#if BUILDFLAG(IS_IOS)
  for (bool make_image_present : {true}) {
#else
  for (bool make_image_present : {true, false}) {
#endif  // BUILDFLAG(IS_IOS)
    SCOPED_TRACE(testing::Message()
                 << ", make_image_present=" << make_image_present);
    payments::GetDetailsForEnrollmentResponseDetails response =
        std::move(SetUpOnDidGetDetailsForEnrollResponse(
            google_legal_message, issuer_legal_message, make_image_present));
    NiceMock<ui::MockResourceBundleDelegate> delegate;

    // A ResourceBundle that uses the test's mock delegate.
    ui::ResourceBundle resource_bundle_with_mock_delegate{&delegate};
    std::unique_ptr<ui::ResourceBundle::SharedInstanceSwapperForTesting>
        resource_bundle_swapper;

    gfx::Image network_image;
    if (!make_image_present) {
      network_image = gfx::test::CreateImage(32, 30);
      // Swap in the test ResourceBundle for the lifetime of the test.
      resource_bundle_swapper =
          std::make_unique<ui::ResourceBundle::SharedInstanceSwapperForTesting>(
              &resource_bundle_with_mock_delegate);
      SetNetworkImageInResourceBundle(&delegate, card_->network(),
                                      network_image);
    }

    virtual_card_enrollment_manager_->InitVirtualCardEnroll(*card_, source(),
                                                            base::DoNothing());
    task_environment_.FastForwardBy(base::Milliseconds(5));
    virtual_card_enrollment_manager_->OnDidGetDetailsForEnrollResponse(
        payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
        response);

    auto* state = virtual_card_enrollment_manager_
                      ->GetVirtualCardEnrollmentProcessState();
    EXPECT_TRUE(state->vcn_context_token.has_value());
    EXPECT_EQ(state->vcn_context_token, response.vcn_context_token);
    VirtualCardEnrollmentFields virtual_card_enrollment_fields =
        virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState()
            ->virtual_card_enrollment_fields;
    EXPECT_TRUE(virtual_card_enrollment_fields.google_legal_message[0].text() ==
                google_legal_message.text());
    EXPECT_TRUE(virtual_card_enrollment_fields.issuer_legal_message[0].text() ==
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
            VirtualCardEnrollmentSourceToMetricSuffix(source()),
        /*sample=*/true, make_image_present ? 1 : 2);
    histogram_tester.ExpectBucketCount(
        "Autofill.VirtualCard.GetDetailsForEnrollment.Latency." +
            VirtualCardEnrollmentSourceToMetricSuffix(source()) +
            PaymentsRpcResultToMetricsSuffix(
                payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess),
        /*sample=*/5, make_image_present ? 1 : 2);

    // Avoid dangling pointers to artwork.
    virtual_card_enrollment_manager_->ResetVirtualCardEnrollmentProcessState();
  }
}

class DownstreamLatencyMetricsTest
    : public VirtualCardEnrollmentManagerTest,
      public ::testing::WithParamInterface<bool> {
 public:
  bool card_unmasked_from_cache() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(VirtualCardEnrollmentManagerTest,
                         DownstreamLatencyMetricsTest,
                         ::testing::Bool());

TEST_P(DownstreamLatencyMetricsTest, LatencySinceDownstream) {
  base::HistogramTester histogram_tester;
  CreditCard card = test::GetMaskedServerCard();
  card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kUnenrolledAndEligible);
  virtual_card_enrollment_manager_->ShouldOfferVirtualCardEnrollment(
      card, card.instrument_id(), card_unmasked_from_cache());
  virtual_card_enrollment_manager_->InitVirtualCardEnroll(
      card, VirtualCardEnrollmentSource::kDownstream,
      /*virtual_card_enrollment_fields_loaded_callback=*/base::DoNothing());

  task_environment_.FastForwardBy(base::Minutes(1));
  virtual_card_enrollment_manager_->ShowVirtualCardEnrollBubble(
      &virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState()
           ->virtual_card_enrollment_fields);

  histogram_tester.ExpectTimeBucketCount(
      "Autofill.VirtualCardEnrollBubble.LatencySinceDownstream",
      base::Minutes(1), card_unmasked_from_cache() ? 0 : 1);
}

class DownstreamEnrollmentEarlyPreflightCallParamTest
    : public VirtualCardEnrollmentManagerTest,
      public ::testing::WithParamInterface<
          std::tuple<bool, VirtualCardEnrollmentSource, bool>> {
 public:
  DownstreamEnrollmentEarlyPreflightCallParamTest() {
    feature_list_.InitWithFeatureState(
        features::
            kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollment,
        experiment_enabled());
  }

  // Whether the experiment to support multiple requests in downstream
  // enrollment is enabled.
  bool experiment_enabled() const { return std::get<0>(GetParam()); }

  // The source of the enrollment.
  VirtualCardEnrollmentSource source() const { return std::get<1>(GetParam()); }

  // Whether downstream enrollment for this VirtualCardEnrollmentManager
  // instance has started or not.
  bool downstream_enrollment_has_started() const {
    return std::get<2>(GetParam());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    VirtualCardEnrollmentManagerTest,
    DownstreamEnrollmentEarlyPreflightCallParamTest,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Values(VirtualCardEnrollmentSource::kUpstream,
                          VirtualCardEnrollmentSource::kDownstream,
                          VirtualCardEnrollmentSource::kSettingsPage),
        ::testing::Bool()));

// Tests that ShouldContinueExistingDownstreamEnrollment should return correct
// values.
TEST_P(DownstreamEnrollmentEarlyPreflightCallParamTest,
       ShouldContinueExistingDownstreamEnrollment) {
  CreditCard card = test::GetMaskedServerCard();
  card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kUnenrolledAndEligible);
  auto* virtual_card_enrollment_process_state =
      virtual_card_enrollment_manager_->GetVirtualCardEnrollmentProcessState();
  if (downstream_enrollment_has_started()) {
    virtual_card_enrollment_process_state->virtual_card_enrollment_fields
        .credit_card = card;
    virtual_card_enrollment_process_state->virtual_card_enrollment_fields
        .virtual_card_enrollment_source =
        VirtualCardEnrollmentSource::kDownstream;
  }

  bool expected = experiment_enabled() &&
                  source() == VirtualCardEnrollmentSource::kDownstream &&
                  downstream_enrollment_has_started();
  EXPECT_EQ(expected,
            virtual_card_enrollment_manager_
                ->ShouldContinueExistingDownstreamEnrollment(card, source()));
}

class DownstreamEnrollmentEarlyPreflightCallCallbackParamTest
    : public VirtualCardEnrollmentManagerTest,
      public ::testing::WithParamInterface<bool> {
 public:
  // Whether enroll details have been received from the server, when the card
  // extraction from the form happens.
  bool enroll_details_received() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    VirtualCardEnrollmentManagerTest,
    DownstreamEnrollmentEarlyPreflightCallCallbackParamTest,
    ::testing::Bool());

// Tests that callback should be invoked at the right time.
TEST_P(DownstreamEnrollmentEarlyPreflightCallCallbackParamTest,
       InvokedAfterEnrollDetailsReceived) {
  auto mock_manager = NiceMock<MockVirtualCardEnrollmentManager>(
      &payments_data_manager(), &multiple_request_payments_network_interface(),
      autofill_client_.get());

  // SetUpOnDidGetDetailsForEnrollResponse call configures the card art image,
  // so it should be called before the first InitVirtualCardEnroll to set up the
  // VirtualCardEnrollmentProcessState properly.
  const TestLegalMessageLine google_legal_message =
      TestLegalMessageLine("google_test_legal_message");
  const TestLegalMessageLine issuer_legal_message =
      TestLegalMessageLine("issuer_test_legal_message");
  payments::GetDetailsForEnrollmentResponseDetails response =
      std::move(SetUpOnDidGetDetailsForEnrollResponse(
          google_legal_message, issuer_legal_message,
          /*make_image_present=*/true));

  // Simulate InitVirtualCardEnroll call during card unmask.
  ON_CALL(mock_manager, ShouldContinueExistingDownstreamEnrollment)
      .WillByDefault(testing::Return(false));
  mock_manager.VirtualCardEnrollmentManager::InitVirtualCardEnroll(
      *card_, VirtualCardEnrollmentSource::kDownstream, base::DoNothing());

  // Simulate InitVirtualCardEnroll call after form extraction.
  ON_CALL(mock_manager, ShouldContinueExistingDownstreamEnrollment)
      .WillByDefault(testing::Return(true));
  base::MockCallback<MockVirtualCardEnrollmentManager::
                         VirtualCardEnrollmentFieldsLoadedCallback>
      callback;
  mock_manager.SetEnrollResponseDetailsReceived(enroll_details_received());

  // If enroll details have been received, the callback should be invoked
  // immediately.
  EXPECT_CALL(callback, Run).Times(enroll_details_received() ? 1 : 0);
  mock_manager.VirtualCardEnrollmentManager::InitVirtualCardEnroll(
      *card_, VirtualCardEnrollmentSource::kDownstream, callback.Get());

  // Otherwise, the callback should be invoked when the details are received.
  if (!enroll_details_received()) {
    EXPECT_CALL(callback, Run);
    mock_manager.OnDidGetDetailsForEnrollResponse(
        payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
        response);
  }
}

}  // namespace autofill
