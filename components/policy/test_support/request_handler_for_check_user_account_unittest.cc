// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/test_support/embedded_policy_test_server_test_base.h"
#include "components/policy/test_support/policy_storage.h"
#include "device_management_backend.pb.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr char kDeviceId[] = "fake_device_id";
constexpr char kManagedUserEmail[] = "managed@example.com";
constexpr char kConsumerUserEmail[] = "consumer@example.com";

}  // namespace

class RequestHandlerForCheckUserAccountTest
    : public EmbeddedPolicyTestServerTestBase {
 protected:
  RequestHandlerForCheckUserAccountTest() = default;
  ~RequestHandlerForCheckUserAccountTest() override = default;

  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();

    SetRequestTypeParam(dm_protocol::kValueCheckUserAccount);
    SetAppType(dm_protocol::kValueAppType);
    SetDeviceIdParam(kDeviceId);
    SetDeviceType(dm_protocol::kValueDeviceType);
  }
};

TEST_F(RequestHandlerForCheckUserAccountTest, DasherEnrollmentRequired) {
  policy_storage()->add_managed_user(kManagedUserEmail);
  policy_storage()->set_enrollment_required(true);

  em::DeviceManagementRequest device_management_request;
  em::CheckUserAccountRequest& check_user_account_request =
      *device_management_request.mutable_check_user_account_request();
  check_user_account_request.set_user_email(kManagedUserEmail);
  check_user_account_request.set_enrollment_nudge_request(true);

  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  ASSERT_TRUE(HasResponseBody());

  auto response = GetDeviceManagementResponse();

  EXPECT_EQ(response.check_user_account_response().user_account_type(),
            em::CheckUserAccountResponse::DASHER);
  EXPECT_TRUE(
      response.check_user_account_response().has_enrollment_nudge_type());
  EXPECT_EQ(response.check_user_account_response().enrollment_nudge_type(),
            em::CheckUserAccountResponse::ENROLLMENT_REQUIRED);
}

TEST_F(RequestHandlerForCheckUserAccountTest, Consumer) {
  em::DeviceManagementRequest device_management_request;
  em::CheckUserAccountRequest& check_user_account_request =
      *device_management_request.mutable_check_user_account_request();
  check_user_account_request.set_user_email(kConsumerUserEmail);
  check_user_account_request.set_enrollment_nudge_request(true);

  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  ASSERT_TRUE(HasResponseBody());

  auto response = GetDeviceManagementResponse();

  EXPECT_EQ(response.check_user_account_response().user_account_type(),
            em::CheckUserAccountResponse::CONSUMER);
  EXPECT_TRUE(
      response.check_user_account_response().has_enrollment_nudge_type());
  EXPECT_EQ(response.check_user_account_response().enrollment_nudge_type(),
            em::CheckUserAccountResponse::NONE);
}

TEST_F(RequestHandlerForCheckUserAccountTest, UnknownEnrollmentNudgeValue) {
  policy_storage()->add_managed_user(kManagedUserEmail);

  em::DeviceManagementRequest device_management_request;
  em::CheckUserAccountRequest& check_user_account_request =
      *device_management_request.mutable_check_user_account_request();
  check_user_account_request.set_user_email(kManagedUserEmail);
  // Indicate that we are not interested in enrollment nudge policy.
  check_user_account_request.set_enrollment_nudge_request(false);

  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  ASSERT_TRUE(HasResponseBody());

  auto response = GetDeviceManagementResponse();

  EXPECT_EQ(response.check_user_account_response().user_account_type(),
            em::CheckUserAccountResponse::DASHER);
  EXPECT_TRUE(
      response.check_user_account_response().has_enrollment_nudge_type());
  EXPECT_EQ(response.check_user_account_response().enrollment_nudge_type(),
            em::CheckUserAccountResponse::UNKNOWN_ENROLLMENT_NUDGE_TYPE);
}

}  // namespace policy
