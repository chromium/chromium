// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_determine_promotion_eligibility.h"

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace em = enterprise_management;

namespace policy {

const char kChromeEnterpriseCoreToken[] = "CHROME_ENTERPRISE_CORE";
const char kChromeEnterprisePremiumToken[] = "CHROME_ENTERPRISE_PREMIUM";

RequestHandlerForDeterminePromotionEligibility::
    RequestHandlerForDeterminePromotionEligibility(
        EmbeddedPolicyTestServer* parent)
    : EmbeddedPolicyTestServer::RequestHandler(parent) {}

RequestHandlerForDeterminePromotionEligibility::
    ~RequestHandlerForDeterminePromotionEligibility() = default;

std::string RequestHandlerForDeterminePromotionEligibility::RequestType() {
  return dm_protocol::kValueRequestDeterminePromotionEligibility;
}

std::unique_ptr<HttpResponse>
RequestHandlerForDeterminePromotionEligibility::HandleRequest(
    const HttpRequest& request) {
  em::DeviceManagementResponse outer_response;
  em::GetUserEligiblePromotionsResponse* fake_response =
      outer_response.mutable_get_user_eligible_promotions_response();
  em::PromotionEligibilityList* promotion_eligibility_list =
      fake_response->mutable_promotions();
  // OAuth token will represent the promotions available for all pages, refer to
  // device_management_backend.proto for source of truth
  em::PromotionType promotion_type;
  std::string oauth_token =
      KeyValueFromUrl(request.GetURL(), dm_protocol::kParamOAuthToken);

  if (oauth_token == kChromeEnterpriseCoreToken) {
    promotion_type = em::PromotionType::CHROME_ENTERPRISE_CORE;
  } else if (oauth_token == kChromeEnterprisePremiumToken) {
    promotion_type = em::PromotionType::CHROME_ENTERPRISE_PREMIUM;
  } else {
    promotion_type = em::PromotionType::PROMOTION_TYPE_UNSPECIFIED;
  }

  promotion_eligibility_list->set_policy_page_promotion(promotion_type);
  promotion_eligibility_list->set_management_page_promotion(promotion_type);
  promotion_eligibility_list->set_interstitial_block_promotion(promotion_type);
  promotion_eligibility_list->set_interstitial_warn_promotion(promotion_type);
  promotion_eligibility_list->set_ntp_promotion(promotion_type);
  promotion_eligibility_list->set_cws_extension_details_promotion(
      promotion_type);
  promotion_eligibility_list->set_cws_privacy_details_promotion(promotion_type);

  return CreateHttpResponse(net::HTTP_OK, outer_response);
}

}  // namespace policy
