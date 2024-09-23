// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/update_virtual_card_enrollment_request.h"

#include <memory>
#include <tuple>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

class UpdateVirtualCardEnrollmentRequestTest
    : public testing::Test,
      public testing::WithParamInterface<
          std::tuple<VirtualCardEnrollmentRequestType,
                     VirtualCardEnrollmentSource>> {
 public:
  UpdateVirtualCardEnrollmentRequestTest() = default;
  UpdateVirtualCardEnrollmentRequestTest(
      const UpdateVirtualCardEnrollmentRequestTest&) = delete;
  UpdateVirtualCardEnrollmentRequestTest& operator=(
      const UpdateVirtualCardEnrollmentRequestTest&) = delete;
  ~UpdateVirtualCardEnrollmentRequestTest() override = default;

  void SetUp() override { CreateRequest(); }

  // Creates an UpdateVirtualCardEnrollmentRequest based on the current test
  // parameters, and stores it in `request_`. Can be re-called to recreate the
  // request as needed.
  void CreateRequest() {
    PaymentsNetworkInterface::UpdateVirtualCardEnrollmentRequestDetails
        request_details;
    request_details.virtual_card_enrollment_request_type =
        std::get<0>(GetParam());
    request_details.virtual_card_enrollment_source = std::get<1>(GetParam());
    request_details.billing_customer_number = 55667788;
    request_details.instrument_id = 11223344;
    // Context token is only populated for enroll requests.
    if (std::get<0>(GetParam()) == VirtualCardEnrollmentRequestType::kEnroll)
      request_details.vcn_context_token = "token";

    request_ = std::make_unique<UpdateVirtualCardEnrollmentRequest>(
        request_details, base::DoNothing());
  }

  UpdateVirtualCardEnrollmentRequest* GetRequest() const {
    return request_.get();
  }

  const std::optional<std::string>& GetParsedResponse() const {
    return request_->enroll_result_;
  }

 private:
  std::unique_ptr<UpdateVirtualCardEnrollmentRequest> request_;
};

TEST_P(UpdateVirtualCardEnrollmentRequestTest, GetRequestContent) {
  ASSERT_FALSE(GetRequest()->GetRequestContent().empty());

  if (std::get<0>(GetParam()) == VirtualCardEnrollmentRequestType::kEnroll) {
    EXPECT_EQ(GetRequest()->GetRequestUrlPath(),
              "payments/apis/virtualcardservice/enroll");

    EXPECT_TRUE(GetRequest()->GetRequestContent().find("billable_service") !=
                std::string::npos);
    EXPECT_TRUE(GetRequest()->GetRequestContent().find("channel_type") !=
                std::string::npos);
    EXPECT_TRUE(GetRequest()->GetRequestContent().find(
                    "external_customer_id") != std::string::npos);
    EXPECT_TRUE(GetRequest()->GetRequestContent().find(
                    "virtual_card_enrollment_flow") != std::string::npos);
    EXPECT_TRUE(GetRequest()->GetRequestContent().find("instrument_id") !=
                std::string::npos);
    EXPECT_TRUE(GetRequest()->GetRequestContent().find("context_token") !=
                std::string::npos);

    std::string billable_service_number;
    std::string channel_type;
    switch (std::get<1>(GetParam())) {
      case VirtualCardEnrollmentSource::kDownstream:
      case VirtualCardEnrollmentSource::kSettingsPage:
        billable_service_number =
            base::NumberToString(kUnmaskPaymentMethodBillableServiceNumber);
        channel_type = "CHROME_DOWNSTREAM";
        break;
      case VirtualCardEnrollmentSource::kUpstream:
        billable_service_number =
            base::NumberToString(kUploadPaymentMethodBillableServiceNumber);
        channel_type = "CHROME_UPSTREAM";
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
    EXPECT_TRUE(GetRequest()->GetRequestContent().find(
                    billable_service_number) != std::string::npos);
    EXPECT_TRUE(GetRequest()->GetRequestContent().find(channel_type) !=
                std::string::npos);

    return;
  }

  ASSERT_EQ(std::get<0>(GetParam()),
            VirtualCardEnrollmentRequestType::kUnenroll);
  // Unenroll is only available from the settings page.
  if (std::get<1>(GetParam()) == VirtualCardEnrollmentSource::kSettingsPage) {
    ASSERT_EQ(std::get<0>(GetParam()),
              VirtualCardEnrollmentRequestType::kUnenroll);
    EXPECT_EQ(GetRequest()->GetRequestUrlPath(),
              "payments/apis/virtualcardservice/unenroll");
    EXPECT_TRUE(GetRequest()->GetRequestContent().find("billable_service") !=
                std::string::npos);
    EXPECT_TRUE(GetRequest()->GetRequestContent().find(
                    "external_customer_id") != std::string::npos);
    EXPECT_TRUE(GetRequest()->GetRequestContent().find("instrument_id") !=
                std::string::npos);
    EXPECT_TRUE(GetRequest()->GetRequestContent().find(base::NumberToString(
                    kUnmaskPaymentMethodBillableServiceNumber)) !=
                std::string::npos);
  }
}

TEST_P(UpdateVirtualCardEnrollmentRequestTest, ParseResponse) {
  if (std::get<0>(GetParam()) == VirtualCardEnrollmentRequestType::kEnroll) {
    std::optional<base::Value> response =
        base::JSONReader::Read("{ \"enroll_result\": \"ENROLL_SUCCESS\" }");
    ASSERT_TRUE(response.has_value());
    GetRequest()->ParseResponse(response->GetDict());

    EXPECT_TRUE(GetRequest()->IsResponseComplete());
    return;
  }

  ASSERT_EQ(std::get<0>(GetParam()),
            VirtualCardEnrollmentRequestType::kUnenroll);
  // Unenroll is only available from the settings page.
  if (std::get<1>(GetParam()) == VirtualCardEnrollmentSource::kSettingsPage) {
    std::optional<base::Value> response = base::JSONReader::Read("{}");
    ASSERT_TRUE(response.has_value());
    GetRequest()->ParseResponse(response->GetDict());

    EXPECT_TRUE(GetRequest()->IsResponseComplete());
  }
}

TEST_P(UpdateVirtualCardEnrollmentRequestTest,
       EnrollDoesNotHaveTimeoutWithoutFlag) {
  if (std::get<0>(GetParam()) != VirtualCardEnrollmentRequestType::kEnroll) {
    // TODO(crbug.com/40605207): Use GTEST_SKIP once supported.
    return;
  }

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillVcnEnrollRequestTimeout);

  // Re-create the request with the flag now set.
  CreateRequest();
  EXPECT_FALSE(GetRequest()->GetTimeout().has_value());
}

TEST_P(UpdateVirtualCardEnrollmentRequestTest, EnrollHasTimeoutWhenFlagSet) {
  base::FieldTrialParams params;
  params["autofill_vcn_enroll_request_timeout_milliseconds"] = "6000";
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillVcnEnrollRequestTimeout, params);

  // Re-create the request with the flag now set.
  CreateRequest();

  if (std::get<0>(GetParam()) == VirtualCardEnrollmentRequestType::kEnroll) {
    EXPECT_EQ(*GetRequest()->GetTimeout(), base::Milliseconds(6000));
  } else {
    // Currently only Enroll has a client-side timeout set.
    EXPECT_FALSE(GetRequest()->GetTimeout().has_value());
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    UpdateVirtualCardEnrollmentRequestTest,
    testing::Combine(
        testing::Values(VirtualCardEnrollmentRequestType::kEnroll,
                        VirtualCardEnrollmentRequestType::kUnenroll),
        testing::Values(VirtualCardEnrollmentSource::kUpstream,
                        VirtualCardEnrollmentSource::kDownstream,
                        VirtualCardEnrollmentSource::kSettingsPage)));

}  // namespace autofill::payments
