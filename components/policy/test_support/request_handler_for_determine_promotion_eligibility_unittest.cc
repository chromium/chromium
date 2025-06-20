// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_determine_promotion_eligibility.h"

#include "base/test/protobuf_matchers.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/embedded_policy_test_server_test_base.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::EqualsProto;

namespace em = enterprise_management;

namespace {

constexpr char kDeviceId[] = "fake_device_id";

}  // namespace

namespace policy {

class RequestHandlerForDeterminePromotionEligibilityTest
    : public EmbeddedPolicyTestServerTestBase {
 protected:
  RequestHandlerForDeterminePromotionEligibilityTest() = default;
  ~RequestHandlerForDeterminePromotionEligibilityTest() override = default;

  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();

    SetRequestTypeParam(
        dm_protocol::kValueRequestDeterminePromotionEligibility);
    SetAppType(dm_protocol::kValueAppType);
    SetDeviceIdParam(kDeviceId);
    SetDeviceType(dm_protocol::kValueDeviceType);
  }
};

em::DeviceManagementResponse CreateExpectedResponse(
    em::PromotionType promotion_type) {
  em::DeviceManagementResponse outer_response;
  em::GetUserEligiblePromotionsResponse* fake_response =
      outer_response.mutable_get_user_eligible_promotions_response();
  em::PromotionEligibilityList* promotion_eligibility_list =
      fake_response->mutable_promotions();

  // Set all promotion types to the provided promotion_type
  promotion_eligibility_list->set_policy_page_promotion(promotion_type);
  promotion_eligibility_list->set_management_page_promotion(promotion_type);
  promotion_eligibility_list->set_interstitial_block_promotion(promotion_type);
  promotion_eligibility_list->set_interstitial_warn_promotion(promotion_type);
  promotion_eligibility_list->set_ntp_promotion(promotion_type);
  promotion_eligibility_list->set_cws_extension_details_promotion(
      promotion_type);
  promotion_eligibility_list->set_cws_privacy_details_promotion(promotion_type);

  return outer_response;
}

class RequestHandlerForDeterminePromotionEligibilityParameterizedTest
    : public RequestHandlerForDeterminePromotionEligibilityTest,
      public testing::WithParamInterface<
          std::tuple<std::string, em::PromotionType>> {
 protected:
};

INSTANTIATE_TEST_SUITE_P(
    PromotionEligibilityTests,
    RequestHandlerForDeterminePromotionEligibilityParameterizedTest,
    testing::Values(
        std::make_tuple("unspecified",
                        em::PromotionType::PROMOTION_TYPE_UNSPECIFIED),
        std::make_tuple(kChromeEnterprisePremiumToken,
                        em::PromotionType::CHROME_ENTERPRISE_PREMIUM),
        std::make_tuple(kChromeEnterpriseCoreToken,
                        em::PromotionType::CHROME_ENTERPRISE_CORE)));

TEST_P(RequestHandlerForDeterminePromotionEligibilityParameterizedTest,
       HandleRequest_ValidPromotionType) {
  std::string oauth_token_value = std::get<0>(GetParam());
  em::PromotionType expected_promotion_type = std::get<1>(GetParam());

  SetOAuthToken(oauth_token_value);
  em::DeviceManagementRequest device_management_request;
  SetPayload(device_management_request);

  em::DeviceManagementResponse outer_response =
      CreateExpectedResponse(expected_promotion_type);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  ASSERT_TRUE(HasResponseBody());
  auto response = GetDeviceManagementResponse();
  EXPECT_THAT(response, EqualsProto(outer_response));
}

}  // namespace policy
