// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/unmask_card_request.h"

#include <memory>

#include "base/callback_helpers.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

// TODO(crbug/1372613): Extend tests in this file to all of the possible card
// unmasking test cases. The cases that are not in this file are currently
// tested in Payments Client tests, but they should be tested here as well.
class UnmaskCardRequestTest : public testing::Test {
 public:
  UnmaskCardRequestTest() = default;
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

 protected:
  // The `request_` that is created for each specific test instance. Set in the
  // initial test set up.
  std::unique_ptr<UnmaskCardRequest> request_;
};

class VirtualCardUnmaskCardRequestTest
    : public UnmaskCardRequestTest,
      public testing::WithParamInterface<
          autofill::CardUnmaskChallengeOptionType> {
 public:
  VirtualCardUnmaskCardRequestTest() {
    if (GetParam() == autofill::CardUnmaskChallengeOptionType::kCvc) {
      SetUpVirtualCardCvcUnmaskCardRequestTest();
    }
  }
  VirtualCardUnmaskCardRequestTest(const VirtualCardUnmaskCardRequestTest&) =
      delete;
  VirtualCardUnmaskCardRequestTest& operator=(
      const VirtualCardUnmaskCardRequestTest&) = delete;
  ~VirtualCardUnmaskCardRequestTest() override = default;

 private:
  // Sets up `request_` specifically for the Virtual Card CVC Unmask Card
  // Request test case.
  void SetUpVirtualCardCvcUnmaskCardRequestTest() {
    PaymentsClient::UnmaskRequestDetails request_details;
    request_details.billing_customer_number = 111222333444;
    request_details.card = test::GetVirtualCard();
    request_details.card.set_server_id("test server id");
    request_details.user_response.exp_month = u"10";
    request_details.user_response.exp_year = u"2025";
    request_details.user_response.cvc = u"123";
    request_details.risk_data = "some risk data";
    request_details.last_committed_primary_main_frame_origin =
        GURL("https://example.com/");
    request_details.selected_challenge_option =
        CardUnmaskChallengeOption{.id = "1",
                                  .type = CardUnmaskChallengeOptionType::kCvc,
                                  .challenge_input_length = 3,
                                  .cvc_position = CvcPosition::kBackOfCard};
    request_details.context_token = "fake context token";
    request_ = std::make_unique<UnmaskCardRequest>(
        request_details, /*full_sync_enabled=*/true,
        /*callback=*/base::DoNothing());
  }
};

TEST_P(VirtualCardUnmaskCardRequestTest, GetRequestContent) {
  if (GetParam() == autofill::CardUnmaskChallengeOptionType::kCvc) {
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
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    VirtualCardUnmaskCardRequestTest,
    testing::Values(autofill::CardUnmaskChallengeOptionType::kCvc));

}  // namespace autofill::payments
