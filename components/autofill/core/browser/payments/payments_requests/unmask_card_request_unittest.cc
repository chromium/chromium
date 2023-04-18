// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/unmask_card_request.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

// TODO(crbug/1372613): Extend tests in this file to all of the possible card
// unmasking test cases. The cases that are not in this file are currently
// tested in Payments Client tests, but they should be tested here as well.
class UnmaskCardRequestTest : public testing::Test {
 public:
  UnmaskCardRequestTest() { SetUpUnmaskCardRequest(); }
  UnmaskCardRequestTest(const UnmaskCardRequestTest&) = delete;
  UnmaskCardRequestTest& operator=(const UnmaskCardRequestTest&) = delete;
  ~UnmaskCardRequestTest() override = default;

  // Returns a pointer to the UnmaskCardRequest that was created for the current
  // test instance.
  UnmaskCardRequest* GetRequest() const { return request_.get(); }

  // Returns true if `field_name_or_value` is included in the `request_`'s
  // request content, false otherwise.
  bool IsIncludedInRequestContent(const std::string& field_name_or_value) {
    return GetRequest()->GetRequestContent().find(field_name_or_value) !=
           std::string::npos;
  }

  // Returns the response details that was created for the current test
  // instance.
  const PaymentsClient::UnmaskResponseDetails& GetParsedResponse() const {
    return request_->GetResponseDetailsForTesting();
  }

 protected:
  // The `request_` that is created for each specific test instance. Set in the
  // initial test set up.
  std::unique_ptr<UnmaskCardRequest> request_;

 private:
  void SetUpUnmaskCardRequest() {
    PaymentsClient::UnmaskRequestDetails request_details;
    request_details.billing_customer_number = 111222333444;
    request_details.card = test::GetMaskedServerCard();
    request_details.card.set_server_id("test server id");
    request_details.user_response.exp_month =
        base::UTF8ToUTF16(test::NextMonth());
    request_details.user_response.exp_year =
        base::UTF8ToUTF16(test::NextYear());
    request_details.user_response.cvc = u"123";
    request_details.risk_data = "some risk data";
    request_details.client_behavior_signals = {
        ClientBehaviorConstants::kShowingCardArtImageAndCardProductName};
    request_ = std::make_unique<UnmaskCardRequest>(
        request_details, /*full_sync_enabled=*/true,
        /*callback=*/base::DoNothing());
  }
};

// Test to ensure that the request content is correctly populated for a regular
// unmask request.
TEST_F(UnmaskCardRequestTest, GetRequestContent) {
  EXPECT_EQ(GetRequest()->GetRequestUrlPath(),
            "payments/apis-secure/creditcardservice/"
            "getrealpan?s7e_suffix=chromewallet");
  ASSERT_TRUE(!GetRequest()->GetRequestContentType().empty());
  EXPECT_TRUE(IsIncludedInRequestContent("customer_context"));
  EXPECT_TRUE(IsIncludedInRequestContent("credit_card_id"));
  EXPECT_TRUE(IsIncludedInRequestContent("risk_data_encoded"));
  EXPECT_TRUE(IsIncludedInRequestContent("billable_service"));
  EXPECT_TRUE(IsIncludedInRequestContent("full_sync_enabled"));
  EXPECT_TRUE(IsIncludedInRequestContent("chrome_user_context"));
  EXPECT_TRUE(IsIncludedInRequestContent("expiration_month"));
  EXPECT_TRUE(IsIncludedInRequestContent("expiration_year"));
  EXPECT_TRUE(IsIncludedInRequestContent("opt_in_fido_auth"));
  EXPECT_TRUE(IsIncludedInRequestContent("encrypted_cvc"));
  EXPECT_TRUE(IsIncludedInRequestContent("&s7e_13_cvc=123"));
  EXPECT_TRUE(IsIncludedInRequestContent("client_behavior_signals"));
  EXPECT_TRUE(IsIncludedInRequestContent(
      "%5B2%5D"));  // '[2]' here stands for the
                    // kShowingCardArtImageAndCardProductName in the
                    // client_behavior_signals.
}

// Params of the VirtualCardUnmaskCardRequestTest:
// -- autofill::CardUnmaskChallengeOptionType challenge_option_type
// -- bool autofill_enable_email_otp_for_vcn_yellow_path
// TODO(crbug.com/1430297): Extend this texting fixture to test the OTP cases as
// well.
class VirtualCardUnmaskCardRequestTest
    : public UnmaskCardRequestTest,
      public testing::WithParamInterface<
          std::tuple<autofill::CardUnmaskChallengeOptionType, bool>> {
 public:
  VirtualCardUnmaskCardRequestTest() {
    if (IsCvcChallengeOption()) {
      SetUpVirtualCardCvcUnmaskCardRequestTest();
    }
  }
  VirtualCardUnmaskCardRequestTest(const VirtualCardUnmaskCardRequestTest&) =
      delete;
  VirtualCardUnmaskCardRequestTest& operator=(
      const VirtualCardUnmaskCardRequestTest&) = delete;
  ~VirtualCardUnmaskCardRequestTest() override = default;

  bool IsAutofillEnableEmailOtpForVcnYellowPathTurnedOn() {
    return std::get<1>(GetParam());
  }

  bool IsCvcChallengeOption() {
    return std::get<0>(GetParam()) ==
           autofill::CardUnmaskChallengeOptionType::kCvc;
  }

 private:
  // Sets up `request_` specifically for the Virtual Card CVC Unmask Card
  // Request test case.
  void SetUpVirtualCardCvcUnmaskCardRequestTest() {
    PaymentsClient::UnmaskRequestDetails request_details;
    request_details.billing_customer_number = 111222333444;
    request_details.card = test::GetVirtualCard();
    request_details.card.set_server_id("test server id");
    request_details.user_response.exp_month =
        base::UTF8ToUTF16(test::NextMonth());
    request_details.user_response.exp_year =
        base::UTF8ToUTF16(test::NextYear());
    request_details.user_response.cvc = u"123";
    request_details.risk_data = "some risk data";
    request_details.last_committed_primary_main_frame_origin =
        GURL("https://example.com/");
    request_details.selected_challenge_option =
        test::GetCardUnmaskChallengeOptions(
            {CardUnmaskChallengeOptionType::kCvc})[0];
    request_details.context_token = "fake context token";
    request_ = std::make_unique<UnmaskCardRequest>(
        request_details, /*full_sync_enabled=*/true,
        /*callback=*/base::DoNothing());
  }
};

TEST_P(VirtualCardUnmaskCardRequestTest, GetRequestContent) {
  if (IsCvcChallengeOption()) {
    EXPECT_EQ(GetRequest()->GetRequestUrlPath(),
              "payments/apis-secure/creditcardservice/"
              "getrealpan?s7e_suffix=chromewallet");
    ASSERT_TRUE(!GetRequest()->GetRequestContentType().empty());
    EXPECT_TRUE(IsIncludedInRequestContent("customer_context"));
    EXPECT_TRUE(IsIncludedInRequestContent("credit_card_id"));
    EXPECT_TRUE(IsIncludedInRequestContent("risk_data_encoded"));
    EXPECT_TRUE(IsIncludedInRequestContent("billable_service"));
    EXPECT_TRUE(IsIncludedInRequestContent("full_sync_enabled"));
    EXPECT_TRUE(IsIncludedInRequestContent("chrome_user_context"));
    EXPECT_TRUE(IsIncludedInRequestContent("context_token"));
    EXPECT_TRUE(IsIncludedInRequestContent("expiration_month"));
    EXPECT_TRUE(IsIncludedInRequestContent("expiration_year"));
    EXPECT_TRUE(IsIncludedInRequestContent("opt_in_fido_auth"));
    EXPECT_TRUE(IsIncludedInRequestContent("merchant_domain"));
    EXPECT_TRUE(IsIncludedInRequestContent("encrypted_cvc"));
    EXPECT_TRUE(IsIncludedInRequestContent("&s7e_13_cvc=123"));
    EXPECT_TRUE(IsIncludedInRequestContent("cvc_challenge_option"));
    EXPECT_TRUE(IsIncludedInRequestContent("challenge_id"));
    EXPECT_TRUE(IsIncludedInRequestContent("cvc_length"));
    EXPECT_TRUE(IsIncludedInRequestContent("cvc_position"));
    EXPECT_FALSE(IsIncludedInRequestContent("client_behavior_signals"));
  }
}

TEST_P(VirtualCardUnmaskCardRequestTest,
       ChallengeOptionsReturned_ParseResponse) {
  base::test::ScopedFeatureList feature_list_email_otp;
  feature_list_email_otp.InitWithFeatureState(
      features::kAutofillEnableEmailOtpForVcnYellowPath,
      /*enabled=*/IsAutofillEnableEmailOtpForVcnYellowPathTurnedOn());
  absl::optional<base::Value> response = base::JSONReader::Read(
      "{\"fido_request_options\": {\"challenge\": \"fake_fido_challenge\"}, "
      "\"context_token\": \"fake_context_token\", \"idv_challenge_options\": "
      "[{\"sms_otp_challenge_option\": {\"challenge_id\": "
      "\"fake_challenge_id_1\", \"masked_phone_number\": \"(***)-***-1234\"}}, "
      "{\"sms_otp_challenge_option\": {\"challenge_id\": "
      "\"fake_challenge_id_2\", \"masked_phone_number\": \"(***)-***-5678\", "
      "\"otp_length\": 5}}, {\"cvc_challenge_option\": {\"challenge_id\": "
      "\"fake_challenge_id_3\", \"cvc_length\": 3, \"cvc_position\": "
      "\"CVC_POSITION_BACK\"}}, {\"cvc_challenge_option\": {\"challenge_id\": "
      "\"fake_challenge_id_4\", \"cvc_length\": 4, \"cvc_position\": "
      "\"CVC_POSITION_FRONT\"}}, {\"email_otp_challenge_option\": "
      "{\"challenge_id\": \"fake_challenge_id_5\", \"masked_email_address\": "
      "\"a******b@google.com\"}}, {\"email_otp_challenge_option\": "
      "{\"challenge_id\": \"fake_challenge_id_6\", \"masked_email_address\": "
      "\"c******d@google.com\", \"otp_length\": 4}}]}");
  ASSERT_TRUE(response.has_value());
  GetRequest()->ParseResponse(response->GetDict());

  const PaymentsClient::UnmaskResponseDetails& response_details =
      GetParsedResponse();
  EXPECT_EQ("fake_context_token", response_details.context_token);
  // Verify the FIDO request challenge is correctly parsed.
  EXPECT_EQ("fake_fido_challenge",
            *response_details.fido_request_options->FindString("challenge"));
  // Verify the six challenge options are two SMS OTP challenge options, two
  // email OTP challenge options and two CVC challenge option, and fields can be
  // correctly parsed.
  if (IsAutofillEnableEmailOtpForVcnYellowPathTurnedOn()) {
    ASSERT_EQ(6u, response_details.card_unmask_challenge_options.size());
  } else {
    ASSERT_EQ(4u, response_details.card_unmask_challenge_options.size());
  }

  const CardUnmaskChallengeOption& challenge_option_1 =
      response_details.card_unmask_challenge_options[0];
  EXPECT_EQ(CardUnmaskChallengeOptionType::kSmsOtp, challenge_option_1.type);
  EXPECT_EQ("fake_challenge_id_1", challenge_option_1.id.value());
  EXPECT_EQ(u"(***)-***-1234", challenge_option_1.challenge_info);
  EXPECT_EQ(6u, challenge_option_1.challenge_input_length);

  const CardUnmaskChallengeOption& challenge_option_2 =
      response_details.card_unmask_challenge_options[1];
  EXPECT_EQ(CardUnmaskChallengeOptionType::kSmsOtp, challenge_option_2.type);
  EXPECT_EQ("fake_challenge_id_2", challenge_option_2.id.value());
  EXPECT_EQ(u"(***)-***-5678", challenge_option_2.challenge_info);
  EXPECT_EQ(5u, challenge_option_2.challenge_input_length);

  const CardUnmaskChallengeOption& challenge_option_3 =
      response_details.card_unmask_challenge_options[2];
  EXPECT_EQ(CardUnmaskChallengeOptionType::kCvc, challenge_option_3.type);
  EXPECT_EQ("fake_challenge_id_3", challenge_option_3.id.value());
  EXPECT_EQ(challenge_option_3.challenge_info,
            u"This is the 3-digit code on the back of your card");
  EXPECT_EQ(3u, challenge_option_3.challenge_input_length);
  EXPECT_EQ(CvcPosition::kBackOfCard, challenge_option_3.cvc_position);

  const CardUnmaskChallengeOption& challenge_option_4 =
      response_details.card_unmask_challenge_options[3];
  EXPECT_EQ(CardUnmaskChallengeOptionType::kCvc, challenge_option_4.type);
  EXPECT_EQ("fake_challenge_id_4", challenge_option_4.id.value());
  EXPECT_EQ(challenge_option_4.challenge_info,
            u"This is the 4-digit code on the front of your card");
  EXPECT_EQ(4u, challenge_option_4.challenge_input_length);
  EXPECT_EQ(CvcPosition::kFrontOfCard, challenge_option_4.cvc_position);

  if (IsAutofillEnableEmailOtpForVcnYellowPathTurnedOn()) {
    const CardUnmaskChallengeOption& challenge_option_5 =
        response_details.card_unmask_challenge_options[4];
    EXPECT_EQ(CardUnmaskChallengeOptionType::kEmailOtp,
              challenge_option_5.type);
    EXPECT_EQ("fake_challenge_id_5", challenge_option_5.id.value());
    EXPECT_EQ(u"a******b@google.com", challenge_option_5.challenge_info);
    EXPECT_EQ(6u, challenge_option_5.challenge_input_length);

    const CardUnmaskChallengeOption& challenge_option_6 =
        response_details.card_unmask_challenge_options[5];
    EXPECT_EQ(CardUnmaskChallengeOptionType::kEmailOtp,
              challenge_option_6.type);
    EXPECT_EQ("fake_challenge_id_6", challenge_option_6.id.value());
    EXPECT_EQ(u"c******d@google.com", challenge_option_6.challenge_info);
    EXPECT_EQ(4u, challenge_option_6.challenge_input_length);
  }
}

TEST_P(VirtualCardUnmaskCardRequestTest, IsRetryableFailure) {
  if (IsCvcChallengeOption()) {
    // Test that `IsRetryableFailure()` returns true if the error code denotes
    // that it is a retryable failure.
    EXPECT_TRUE(GetRequest()->IsRetryableFailure("internal"));

    // Test that `IsRetryableFailure()` returns true if a flow status is
    // present.
    absl::optional<base::Value> response = base::JSONReader::Read(
        "{\"flow_status\": \"FLOW_STATUS_INCORRECT_ACCOUNT_SECURITY_CODE\"}");
    ASSERT_TRUE(response);
    GetRequest()->ParseResponse(response->GetDict());
    EXPECT_TRUE(GetRequest()->IsRetryableFailure(""));

    // The next several tests ensure that `IsRetryableFailure()` returns false
    // if no flow status is present.
    response = base::JSONReader::Read(
        "{\"error\": {\"code\": \"ANYTHING_ELSE\", "
        "\"api_error_reason\": \"virtual_card_temporary_error\"}, "
        "\"decline_details\": {\"user_message_title\": "
        "\"\", \"user_message_description\": "
        "\"\"}}");
    ASSERT_TRUE(response);
    GetRequest()->ParseResponse(response->GetDict());
    EXPECT_FALSE(GetRequest()->IsRetryableFailure(""));

    response = base::JSONReader::Read(
        "{\"error\": {\"code\": \"ANYTHING_ELSE\", "
        "\"api_error_reason\": \"virtual_card_permanent_error\"}, "
        "\"decline_details\": {\"user_message_title\": "
        "\"\", \"user_message_description\": "
        "\"\"}}");
    ASSERT_TRUE(response);
    GetRequest()->ParseResponse(response->GetDict());
    EXPECT_FALSE(GetRequest()->IsRetryableFailure(""));

    response = base::JSONReader::Read("{ \"pan\": \"1234\" }");
    ASSERT_TRUE(response);
    GetRequest()->ParseResponse(response->GetDict());
    EXPECT_FALSE(GetRequest()->IsRetryableFailure(""));
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    VirtualCardUnmaskCardRequestTest,
    testing::Combine(
        testing::Values(autofill::CardUnmaskChallengeOptionType::kCvc),
        testing::Bool()));

}  // namespace autofill::payments
