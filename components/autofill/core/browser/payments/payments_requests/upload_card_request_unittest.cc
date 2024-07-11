// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/upload_card_request.h"

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::HasSubstr;

namespace autofill::payments {

namespace {

struct UploadCardOptions {
  UploadCardOptions& with_cvc_in_request(bool b) {
    include_cvc = b;
    return *this;
  }

  UploadCardOptions& with_nickname_in_request(bool b) {
    include_nickname = b;
    return *this;
  }

  UploadCardOptions& with_billing_customer_number(int64_t i) {
    billing_customer_number = i;
    return *this;
  }

  UploadCardOptions& with_client_behavior_signals(
      std::vector<ClientBehaviorConstants> v) {
    client_behavior_signals = std::move(v);
    return *this;
  }

  bool include_cvc = false;
  bool include_nickname = false;
  int64_t billing_customer_number = 111222333444L;
  std::vector<ClientBehaviorConstants> client_behavior_signals;
};

AutofillProfile BuildProfile(std::string_view first_name,
                             std::string_view last_name,
                             std::string_view address_line,
                             std::string_view city,
                             std::string_view state,
                             std::string_view zip,
                             std::string_view phone_number) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);

  profile.SetInfo(NAME_FIRST, base::ASCIIToUTF16(first_name), "en-US");
  profile.SetInfo(NAME_LAST, base::ASCIIToUTF16(last_name), "en-US");
  profile.SetInfo(ADDRESS_HOME_LINE1, base::ASCIIToUTF16(address_line),
                  "en-US");
  profile.SetInfo(ADDRESS_HOME_CITY, base::ASCIIToUTF16(city), "en-US");
  profile.SetInfo(ADDRESS_HOME_STATE, base::ASCIIToUTF16(state), "en-US");
  profile.SetInfo(ADDRESS_HOME_ZIP, base::ASCIIToUTF16(zip), "en-US");
  profile.SetInfo(PHONE_HOME_WHOLE_NUMBER, base::ASCIIToUTF16(phone_number),
                  "en-US");
  profile.FinalizeAfterImport();
  return profile;
}

std::vector<AutofillProfile> BuildTestProfiles() {
  std::vector<AutofillProfile> profiles;
  profiles.push_back(BuildProfile("John", "Smith", "1234 Main St.", "Miami",
                                  "FL", "32006", "212-555-0162"));
  profiles.push_back(BuildProfile("Pat", "Jones", "432 Oak Lane", "Lincoln",
                                  "OH", "43005", "(834)555-0090"));
  return profiles;
}

std::unique_ptr<UploadCardRequest> CreateUploadCardRequest(
    UploadCardOptions upload_card_options) {
  PaymentsNetworkInterface::UploadCardRequestDetails request_details;
  request_details.billing_customer_number =
      upload_card_options.billing_customer_number;
  request_details.card = test::GetCreditCard();
  if (upload_card_options.include_cvc) {
    request_details.cvc = u"123";
  }
  if (upload_card_options.include_nickname) {
    request_details.card.SetNickname(u"grocery");
  }
  request_details.client_behavior_signals =
      upload_card_options.client_behavior_signals;

  request_details.context_token = u"context token";
  request_details.risk_data = "some risk data";
  request_details.app_locale = "language-LOCALE";
  request_details.profiles = BuildTestProfiles();

  return std::make_unique<UploadCardRequest>(
      request_details, /*full_sync_enabled=*/true, base::DoNothing());
}

}  // namespace

TEST(UploadCardRequestTest, UploadIncludesNonLocationData) {
  std::unique_ptr<UploadCardRequest> request =
      CreateUploadCardRequest(UploadCardOptions());

  // Verify that the recipient name field and test names do appear in the upload
  // data.
  EXPECT_TRUE(request->GetRequestContent().find(
                  PaymentsNetworkInterface::kRecipientName) !=
              std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("John") != std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("Smith") != std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("Pat") != std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("Jones") != std::string::npos);

  // Verify that the phone number field and test numbers do appear in the upload
  // data.
  EXPECT_TRUE(request->GetRequestContent().find(
                  PaymentsNetworkInterface::kPhoneNumber) != std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("212") != std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("555") != std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("0162") != std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("834") != std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("0090") != std::string::npos);
}

TEST(UploadCardRequestTest,
     UploadRequestIncludesBillingCustomerNumberInRequest) {
  std::unique_ptr<UploadCardRequest> request = CreateUploadCardRequest(
      UploadCardOptions().with_billing_customer_number(1234L));

  // Verify that the billing customer number is included in the request.
  EXPECT_TRUE(request->GetRequestContent().find(
                  "%22external_customer_id%22:%221234%22") !=
              std::string::npos);
}

TEST(UploadCardRequestTest,
     UploadRequestExcludesBillingCustomerNumberIfNoBcnExists) {
  // A value of zero is treated as a non-existent BCN.
  std::unique_ptr<UploadCardRequest> request = CreateUploadCardRequest(
      UploadCardOptions().with_billing_customer_number(0L));

  // Verify that the billing customer number is not included in the request if
  // billing customer number is 0.
  EXPECT_TRUE(request->GetRequestContent().find("\"external_customer_id\"") ==
              std::string::npos);
}

TEST(UploadCardRequestTest, UploadRequestIncludesClientBehaviorSignals) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillEnableCvcStorageAndFilling);

  std::unique_ptr<UploadCardRequest> request =
      CreateUploadCardRequest(UploadCardOptions().with_client_behavior_signals(
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
              HasSubstr("%22client_behavior_signals%22:%5B3%5D"));
}

TEST(UploadCardRequestTest, UploadRequestIncludesPan) {
  std::unique_ptr<UploadCardRequest> request =
      CreateUploadCardRequest(UploadCardOptions());

  // Verify that the `pan` and s7e_21_pan parameters were included in the
  // request, and the legacy field `encrypted_pan` was not.
  EXPECT_TRUE(request->GetRequestContent().find("encrypted_pan") ==
              std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("pan") != std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("__param:s7e_21_pan") !=
              std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find(
                  "&s7e_21_pan=4111111111111111") != std::string::npos);
}

TEST(UploadCardRequestTest, UploadIncludesCvcInRequestIfProvided) {
  std::unique_ptr<UploadCardRequest> request =
      CreateUploadCardRequest(UploadCardOptions().with_cvc_in_request(true));

  // Verify that the encrypted_cvc and s7e_13_cvc parameters were included in
  // the request.
  EXPECT_TRUE(request->GetRequestContent().find("encrypted_cvc") !=
              std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("__param:s7e_13_cvc") !=
              std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("&s7e_13_cvc=") !=
              std::string::npos);
}

TEST(UploadCardRequestTest, UploadDoesNotIncludeCvcInRequestIfNotProvided) {
  std::unique_ptr<UploadCardRequest> request =
      CreateUploadCardRequest(UploadCardOptions().with_cvc_in_request(false));

  EXPECT_TRUE(!request->GetRequestContent().empty());
  // Verify that the encrypted_cvc and s7e_13_cvc parameters were not included
  // in the request.
  EXPECT_TRUE(request->GetRequestContent().find("encrypted_cvc") ==
              std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("__param:s7e_13_cvc") ==
              std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("&s7e_13_cvc=") ==
              std::string::npos);
}

TEST(UploadCardRequestTest, UploadIncludesChromeUserContext) {
  std::unique_ptr<UploadCardRequest> request =
      CreateUploadCardRequest(UploadCardOptions());

  // ChromeUserContext was set.
  EXPECT_TRUE(request->GetRequestContent().find("chrome_user_context") !=
              std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("full_sync_enabled") !=
              std::string::npos);
}

TEST(UploadCardRequestTest, UploadIncludesCardNickname) {
  std::unique_ptr<UploadCardRequest> request = CreateUploadCardRequest(
      UploadCardOptions().with_nickname_in_request(true));

  // Card nickname was set.
  EXPECT_TRUE(request->GetRequestContent().find("nickname") !=
              std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find(
                  base::UTF16ToUTF8(u"grocery")) != std::string::npos);
}

TEST(UploadCardRequestTest, UploadDoesNotIncludeCardNicknameEmptyNickname) {
  std::unique_ptr<UploadCardRequest> request = CreateUploadCardRequest(
      UploadCardOptions().with_nickname_in_request(false));

  // Card nickname was not set.
  EXPECT_FALSE(request->GetRequestContent().find("nickname") !=
               std::string::npos);
}

}  // namespace autofill::payments
