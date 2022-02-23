// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback_helpers.h"
#include "base/json/json_reader.h"
#include "components/autofill/core/browser/payments/payments_requests/get_details_for_enrollment_request.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

class GetDetailsForEnrollmentRequestTest : public testing::Test {
 public:
  GetDetailsForEnrollmentRequestTest() = default;
  GetDetailsForEnrollmentRequestTest(
      const GetDetailsForEnrollmentRequestTest&) = delete;
  GetDetailsForEnrollmentRequestTest& operator=(
      const GetDetailsForEnrollmentRequestTest&) = delete;
  ~GetDetailsForEnrollmentRequestTest() override = default;

  void CreateRequest() {
    PaymentsClient::GetDetailsForEnrollmentRequestDetails request_details;
    request_details.instrument_id = 11223344;
    request_details.app_locale = "en";
    request_details.billing_customer_number = 55667788;
    request_details.risk_data = "fake risk data";
    request_details.source = VirtualCardEnrollmentSource::kUpstream;
    request_ = std::make_unique<GetDetailsForEnrollmentRequest>(
        request_details, base::DoNothing());
  }

  GetDetailsForEnrollmentRequest* GetRequest() const { return request_.get(); }

  const PaymentsClient::GetDetailsForEnrollmentResponseDetails&
  GetParsedResponse() const {
    return request_->response_details_;
  }

 private:
  std::unique_ptr<GetDetailsForEnrollmentRequest> request_;
};

TEST_F(GetDetailsForEnrollmentRequestTest, GetRequestContent) {
  CreateRequest();
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
}

TEST_F(GetDetailsForEnrollmentRequestTest, ParseResponse) {
  CreateRequest();
  absl::optional<base::Value> response = base::JSONReader::Read(
      "{ \"google_legal_message\": {}, \"external_legal_message\": {}, "
      "\"context_token\": \"some_token\" }");
  ASSERT_TRUE(response.has_value());
  GetRequest()->ParseResponse(response.value());

  EXPECT_EQ(GetParsedResponse().vcn_context_token, "some_token");
  EXPECT_TRUE(GetParsedResponse().issuer_legal_message.empty());
  EXPECT_TRUE(GetParsedResponse().google_legal_message.empty());
  EXPECT_FALSE(GetRequest()->IsResponseComplete());
}

}  // namespace autofill::payments
