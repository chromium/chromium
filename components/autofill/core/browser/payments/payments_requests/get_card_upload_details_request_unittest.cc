// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/get_card_upload_details_request.h"

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/test/autofill_payments_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::HasSubstr;

namespace autofill::payments {
namespace {

int kAllDetectableValues =
    CreditCardSaveManager::DetectedValue::CVC |
    CreditCardSaveManager::DetectedValue::CARDHOLDER_NAME |
    CreditCardSaveManager::DetectedValue::ADDRESS_NAME |
    CreditCardSaveManager::DetectedValue::ADDRESS_LINE |
    CreditCardSaveManager::DetectedValue::LOCALITY |
    CreditCardSaveManager::DetectedValue::ADMINISTRATIVE_AREA |
    CreditCardSaveManager::DetectedValue::POSTAL_CODE |
    CreditCardSaveManager::DetectedValue::COUNTRY_CODE |
    CreditCardSaveManager::DetectedValue::HAS_GOOGLE_PAYMENTS_ACCOUNT;

struct GetCardUploadDetailsOptions {
  GetCardUploadDetailsOptions& with_upload_card_source(
      PaymentsNetworkInterface::UploadCardSource u) {
    upload_card_source = u;
    return *this;
  }

  GetCardUploadDetailsOptions& with_billing_customer_number(int64_t i) {
    billing_customer_number = i;
    return *this;
  }

  GetCardUploadDetailsOptions& with_client_behavior_signals(
      std::vector<ClientBehaviorConstants> v) {
    client_behavior_signals = std::move(v);
    return *this;
  }

  PaymentsNetworkInterface::UploadCardSource upload_card_source =
      PaymentsNetworkInterface::UploadCardSource::UNKNOWN_UPLOAD_CARD_SOURCE;
  int64_t billing_customer_number = 111222333444L;
  std::vector<ClientBehaviorConstants> client_behavior_signals;
};

std::unique_ptr<GetCardUploadDetailsRequest> CreateGetCardUploadDetailsRequest(
    GetCardUploadDetailsOptions get_card_upload_details_options) {
  return std::make_unique<GetCardUploadDetailsRequest>(
      BuildTestProfiles(), kAllDetectableValues,
      get_card_upload_details_options.client_behavior_signals,
      /*full_sync_enabled=*/true, "language-LOCALE",
      /*callback=*/base::DoNothing(),
      /*billable_service_number=*/12345,
      get_card_upload_details_options.billing_customer_number,
      get_card_upload_details_options.upload_card_source);
}

TEST(GetCardUploadDetailsRequestTest, GetDetailsRemovesNonLocationData) {
  std::unique_ptr<GetCardUploadDetailsRequest> request =
      CreateGetCardUploadDetailsRequest(GetCardUploadDetailsOptions());

  // Verify that the recipient name field and test names appear nowhere in the
  // upload data.
  EXPECT_TRUE(request->GetRequestContent().find(
                  PaymentsNetworkInterface::kRecipientName) ==
              std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("John") == std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("Smith") == std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("Pat") == std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("Jones") == std::string::npos);

  // Verify that the phone number field and test numbers appear nowhere in the
  // upload data.
  EXPECT_TRUE(request->GetRequestContent().find(
                  PaymentsNetworkInterface::kPhoneNumber) == std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("212") == std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("555") == std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("0162") == std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("834") == std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("0090") == std::string::npos);
}

TEST(GetCardUploadDetailsRequestTest,
     GetDetailsIncludesDetectedValuesInRequest) {
  std::unique_ptr<GetCardUploadDetailsRequest> request =
      CreateGetCardUploadDetailsRequest(GetCardUploadDetailsOptions());

  // Verify that the detected values were included in the request.
  std::string detected_values_string =
      "\"detected_values\":" + base::NumberToString(kAllDetectableValues);
  EXPECT_TRUE(request->GetRequestContent().find(detected_values_string) !=
              std::string::npos);
}

TEST(GetCardUploadDetailsRequestTest,
     GetDetailsIncludesClientBehaviorSignalsInChromeUserContext) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillEnableCvcStorageAndFilling);

  std::unique_ptr<GetCardUploadDetailsRequest> request =
      CreateGetCardUploadDetailsRequest(
          GetCardUploadDetailsOptions().with_client_behavior_signals(
              std::vector<ClientBehaviorConstants>{
                  ClientBehaviorConstants::kOfferingToSaveCvc}));

  // Verify ChromeUserContext was set.
  EXPECT_THAT(request->GetRequestContent(), HasSubstr("chrome_user_context"));
  // Verify Client_behavior_signals was set.
  EXPECT_THAT(request->GetRequestContent(),
              HasSubstr("client_behavior_signals"));
  // Verify fake_client_behavior_signal was set.
  // ClientBehaviorConstants::kOfferingToSaveCvc has the numeric value
  // set to 3.
  EXPECT_THAT(request->GetRequestContent(),
              HasSubstr("\"client_behavior_signals\":[3]"));
}

TEST(GetCardUploadDetailsRequestTest, GetDetailsIncludesChromeUserContext) {
  std::unique_ptr<GetCardUploadDetailsRequest> request =
      CreateGetCardUploadDetailsRequest(GetCardUploadDetailsOptions());

  // ChromeUserContext was set.
  EXPECT_TRUE(request->GetRequestContent().find("chrome_user_context") !=
              std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("full_sync_enabled") !=
              std::string::npos);
}

TEST(GetCardUploadDetailsRequestTest,
     GetDetailsIncludesUpstreamCheckoutFlowUploadCardSourceInRequest) {
  std::unique_ptr<GetCardUploadDetailsRequest> request =
      CreateGetCardUploadDetailsRequest(
          GetCardUploadDetailsOptions().with_upload_card_source(
              PaymentsNetworkInterface::UploadCardSource::
                  UPSTREAM_CHECKOUT_FLOW));

  // Verify that the correct upload card source was included in the request.
  EXPECT_TRUE(request->GetRequestContent().find("UPSTREAM_CHECKOUT_FLOW") !=
              std::string::npos);
}

TEST(GetCardUploadDetailsRequestTest,
     GetDetailsIncludesUpstreamSettingsPageUploadCardSourceInRequest) {
  std::unique_ptr<GetCardUploadDetailsRequest> request =
      CreateGetCardUploadDetailsRequest(
          GetCardUploadDetailsOptions().with_upload_card_source(
              PaymentsNetworkInterface::UploadCardSource::
                  UPSTREAM_SETTINGS_PAGE));

  // Verify that the correct upload card source was included in the request.
  EXPECT_TRUE(request->GetRequestContent().find("UPSTREAM_SETTINGS_PAGE") !=
              std::string::npos);
}

TEST(GetCardUploadDetailsRequestTest,
     GetDetailsIncludesUpstreamCardOcrUploadCardSourceInRequest) {
  std::unique_ptr<GetCardUploadDetailsRequest> request =
      CreateGetCardUploadDetailsRequest(
          GetCardUploadDetailsOptions().with_upload_card_source(
              PaymentsNetworkInterface::UploadCardSource::UPSTREAM_CARD_OCR));

  // Verify that the correct upload card source was included in the request.
  EXPECT_TRUE(request->GetRequestContent().find("UPSTREAM_CARD_OCR") !=
              std::string::npos);
}

TEST(
    GetCardUploadDetailsRequestTest,
    GetDetailsIncludesLocalCardMigrationCheckoutFlowUploadCardSourceInRequest) {
  std::unique_ptr<GetCardUploadDetailsRequest> request =
      CreateGetCardUploadDetailsRequest(
          GetCardUploadDetailsOptions().with_upload_card_source(
              PaymentsNetworkInterface::UploadCardSource::
                  LOCAL_CARD_MIGRATION_CHECKOUT_FLOW));

  // Verify that the correct upload card source was included in the request.
  EXPECT_TRUE(request->GetRequestContent().find(
                  "LOCAL_CARD_MIGRATION_CHECKOUT_FLOW") != std::string::npos);
}

TEST(
    GetCardUploadDetailsRequestTest,
    GetDetailsIncludesLocalCardMigrationSettingsPageUploadCardSourceInRequest) {
  std::unique_ptr<GetCardUploadDetailsRequest> request =
      CreateGetCardUploadDetailsRequest(
          GetCardUploadDetailsOptions().with_upload_card_source(
              PaymentsNetworkInterface::UploadCardSource::
                  LOCAL_CARD_MIGRATION_SETTINGS_PAGE));

  // Verify that the correct upload card source was included in the request.
  EXPECT_TRUE(request->GetRequestContent().find(
                  "LOCAL_CARD_MIGRATION_SETTINGS_PAGE") != std::string::npos);
}

TEST(GetCardUploadDetailsRequestTest,
     GetDetailsIncludesUnknownUploadCardSourceInRequest) {
  std::unique_ptr<GetCardUploadDetailsRequest> request =
      CreateGetCardUploadDetailsRequest(GetCardUploadDetailsOptions());

  // Verify that the absence of an upload card source results in UNKNOWN.
  EXPECT_TRUE(request->GetRequestContent().find("UNKNOWN_UPLOAD_CARD_SOURCE") !=
              std::string::npos);
}

TEST(GetCardUploadDetailsRequestTest, GetDetailsIncludeBillableServiceNumber) {
  std::unique_ptr<GetCardUploadDetailsRequest> request =
      CreateGetCardUploadDetailsRequest(GetCardUploadDetailsOptions());

  // Verify that billable service number was included in the request.
  EXPECT_TRUE(request->GetRequestContent().find("\"billable_service\":12345") !=
              std::string::npos);
}

TEST(GetCardUploadDetailsRequestTest, GetDetailsIncludeBillingCustomerNumber) {
  std::unique_ptr<GetCardUploadDetailsRequest> request =
      CreateGetCardUploadDetailsRequest(GetCardUploadDetailsOptions());

  // Verify that the billing customer number is included in the request if flag
  // is enabled.
  EXPECT_TRUE(request->GetRequestContent().find(
                  "\"external_customer_id\":\"111222333444\"") !=
              std::string::npos);
}

TEST(GetCardUploadDetailsRequestTest,
     GetDetailsExcludesBillingCustomerNumberIfNoBcnExists) {
  // A value of zero is treated as a non-existent BCN.
  std::unique_ptr<GetCardUploadDetailsRequest> request =
      CreateGetCardUploadDetailsRequest(
          GetCardUploadDetailsOptions().with_billing_customer_number(0L));

  // Verify that the billing customer number is not included in the request if
  // billing customer number is 0.
  EXPECT_TRUE(request->GetRequestContent().find("\"external_customer_id\"") ==
              std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("\"customer_context\"") ==
              std::string::npos);
}

}  // namespace
}  // namespace autofill::payments
