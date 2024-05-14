// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "components/autofill/core/browser/payments/payments_requests/get_details_for_enrollment_request.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

class GetDetailsForEnrollmentRequestTest
    : public testing::Test,
      public testing::WithParamInterface<VirtualCardEnrollmentSource> {
 public:
  GetDetailsForEnrollmentRequestTest() = default;
  GetDetailsForEnrollmentRequestTest(
      const GetDetailsForEnrollmentRequestTest&) = delete;
  GetDetailsForEnrollmentRequestTest& operator=(
      const GetDetailsForEnrollmentRequestTest&) = delete;
  ~GetDetailsForEnrollmentRequestTest() override = default;

  void SetUp() override {
    PaymentsNetworkInterface::GetDetailsForEnrollmentRequestDetails
        request_details;
    request_details.instrument_id = 11223344;
    request_details.app_locale = "en";
    request_details.billing_customer_number = 55667788;
    request_details.risk_data = "fake risk data";
    request_details.source = GetParam();
    request_ = std::make_unique<GetDetailsForEnrollmentRequest>(
        request_details, base::DoNothing());
  }

  GetDetailsForEnrollmentRequest* GetRequest() const { return request_.get(); }

  const PaymentsNetworkInterface::GetDetailsForEnrollmentResponseDetails&
  GetParsedResponse() const {
    return request_->response_details_;
  }

 private:
  std::unique_ptr<GetDetailsForEnrollmentRequest> request_;
};

TEST_P(GetDetailsForEnrollmentRequestTest, GetRequestContent) {
  EXPECT_EQ(GetRequest()->GetRequestUrlPath(),
            "payments/apis/virtualcardservice/getdetailsforenroll");
  EXPECT_TRUE(!GetRequest()->GetRequestContent().empty());
  EXPECT_TRUE(GetRequest()->GetRequestContent().find("language_code") !=
              std::string::npos);
  EXPECT_TRUE(GetRequest()->GetRequestContent().find("billable_service") !=
              std::string::npos);
  EXPECT_TRUE(GetRequest()->GetRequestContent().find("external_customer_id") !=
              std::string::npos);
  EXPECT_TRUE(GetRequest()->GetRequestContent().find("instrument_id") !=
              std::string::npos);
  EXPECT_TRUE(GetRequest()->GetRequestContent().find("risk_data_encoded") !=
              std::string::npos);
  EXPECT_TRUE(GetRequest()->GetRequestContent().find("channel_type") !=
              std::string::npos);

  std::string channel_type;
  switch (GetParam()) {
    case VirtualCardEnrollmentSource::kUpstream:
      channel_type = "CHROME_UPSTREAM";
      break;
    case VirtualCardEnrollmentSource::kDownstream:
    case VirtualCardEnrollmentSource::kSettingsPage:
      channel_type = "CHROME_DOWNSTREAM";
      break;
    case VirtualCardEnrollmentSource::kNone:
      NOTREACHED_IN_MIGRATION();
      ASSERT_TRUE(false);
      break;
  }
  EXPECT_TRUE(GetRequest()->GetRequestContent().find(channel_type) !=
              std::string::npos);
}

TEST_P(GetDetailsForEnrollmentRequestTest, ParseResponse) {
  std::optional<base::Value> response = base::JSONReader::Read(
      "{ \"google_legal_message\": {}, \"external_legal_message\": {}, "
      "\"context_token\": \"some_token\" }");
  ASSERT_TRUE(response.has_value());
  GetRequest()->ParseResponse(response->GetDict());

  EXPECT_EQ(GetParsedResponse().vcn_context_token, "some_token");
  EXPECT_TRUE(GetParsedResponse().issuer_legal_message.empty());
  EXPECT_TRUE(GetParsedResponse().google_legal_message.empty());
  EXPECT_FALSE(GetRequest()->IsResponseComplete());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    GetDetailsForEnrollmentRequestTest,
    testing::Values(VirtualCardEnrollmentSource::kUpstream,
                    VirtualCardEnrollmentSource::kDownstream,
                    VirtualCardEnrollmentSource::kSettingsPage));

}  // namespace autofill::payments
