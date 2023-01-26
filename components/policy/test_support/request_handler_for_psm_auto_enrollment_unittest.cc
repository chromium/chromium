// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_psm_auto_enrollment.h"

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/test_support/embedded_policy_test_server_test_base.h"
#include "device_management_backend.pb.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/private_membership/src/internal/testing/regression_test_data/regression_test_data.pb.h"

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr char kDeviceId[] = "fake_device_id";

}  // namespace

class RequestHandlerForPsmAutoEnrollmentTest
    : public EmbeddedPolicyTestServerTestBase {
 protected:
  RequestHandlerForPsmAutoEnrollmentTest() = default;
  ~RequestHandlerForPsmAutoEnrollmentTest() override = default;

  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();

    SetRequestTypeParam(dm_protocol::kValueRequestPsmHasDeviceState);
    SetAppType(dm_protocol::kValueAppType);
    SetDeviceIdParam(kDeviceId);
    SetDeviceType(dm_protocol::kValueDeviceType);
  }
};

class RequestHandlerForPsmAutoEnrollmentTestWithCase
    : public RequestHandlerForPsmAutoEnrollmentTest,
      public testing::WithParamInterface</*test_index=*/int> {};

TEST_P(RequestHandlerForPsmAutoEnrollmentTestWithCase, GoodOprfRequest) {
  const auto test_data = RequestHandlerForPsmAutoEnrollment::LoadTestData();
  ASSERT_TRUE(test_data);
  const auto& test_case = test_data->test_cases(GetParam());
  em::DeviceManagementRequest device_management_request;
  *device_management_request.mutable_private_set_membership_request()
       ->mutable_rlwe_request()
       ->mutable_oprf_request() = test_case.expected_oprf_request();
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  ASSERT_TRUE(HasResponseBody());
  const auto response = GetDeviceManagementResponse();
  EXPECT_EQ(response.private_set_membership_response()
                .rlwe_response()
                .oprf_response()
                .SerializeAsString(),
            test_case.oprf_response().SerializeAsString());
}

TEST_P(RequestHandlerForPsmAutoEnrollmentTestWithCase, GoodQueryRequest) {
  const auto test_data = RequestHandlerForPsmAutoEnrollment::LoadTestData();
  ASSERT_TRUE(test_data);
  const auto& test_case = test_data->test_cases(GetParam());
  em::DeviceManagementRequest device_management_request;
  *device_management_request.mutable_private_set_membership_request()
       ->mutable_rlwe_request()
       ->mutable_query_request() = test_case.expected_query_request();
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  ASSERT_TRUE(HasResponseBody());
  const auto response = GetDeviceManagementResponse();
  EXPECT_EQ(response.private_set_membership_response()
                .rlwe_response()
                .query_response()
                .SerializeAsString(),
            test_case.query_response().SerializeAsString());
}

INSTANTIATE_TEST_SUITE_P(
    Each,
    RequestHandlerForPsmAutoEnrollmentTestWithCase,
    testing::Range(
        0,
        RequestHandlerForPsmAutoEnrollment::LoadTestData()->test_cases_size()));

TEST_F(RequestHandlerForPsmAutoEnrollmentTest, BadOprfRequest) {
  em::DeviceManagementRequest device_management_request;
  *device_management_request.mutable_private_set_membership_request()
       ->mutable_rlwe_request()
       ->mutable_oprf_request() = {};
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_BAD_REQUEST);
}

TEST_F(RequestHandlerForPsmAutoEnrollmentTest, BadQueryRequest) {
  em::DeviceManagementRequest device_management_request;
  *device_management_request.mutable_private_set_membership_request()
       ->mutable_rlwe_request()
       ->mutable_oprf_request() = {};
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_BAD_REQUEST);
}

TEST_F(RequestHandlerForPsmAutoEnrollmentTest, BadRequest) {
  em::DeviceManagementRequest device_management_request;
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_BAD_REQUEST);
}

}  // namespace policy
