// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_register_browser.h"

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/embedded_policy_test_server_test_base.h"
#include "components/policy/test_support/policy_storage.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr char kDeviceId[] = "fake_device_id";
constexpr char kAllowedUserEmail[] = "user@example.com";
constexpr char kDisallowedUserEmail[] = "invalid-user@example.com";
constexpr char kAllowedUserOAuthToken[] = "oauth-token-for-user";
constexpr char kDisallowedUserOAuthToken[] = "oauth-token-for-invalid-user";
constexpr char kMachineModel[] = "iPhone 10";

}  // namespace

class RequestHandlerForRegisterDeviceAndUserTest
    : public EmbeddedPolicyTestServerTestBase {
 protected:
  RequestHandlerForRegisterDeviceAndUserTest() = default;
  ~RequestHandlerForRegisterDeviceAndUserTest() override = default;

  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();

    SetRequestTypeParam(dm_protocol::kValueRequestRegister);
    SetAppType(dm_protocol::kValueAppType);
    SetDeviceIdParam(kDeviceId);
    SetDeviceType(dm_protocol::kValueDeviceType);
  }
};

TEST_F(RequestHandlerForRegisterDeviceAndUserTest,
       HandleRequest_NoEnrollmentToken) {
  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_UNAUTHORIZED);

  EXPECT_EQ(client_storage()->GetNumberOfRegisteredClients(), 0u);
}

TEST_F(RequestHandlerForRegisterDeviceAndUserTest,
       HandleRequest_NoManagedUsers) {
  SetGoogleLoginTokenHeader(kAllowedUserOAuthToken);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_INTERNAL_SERVER_ERROR);

  EXPECT_EQ(client_storage()->GetNumberOfRegisteredClients(), 0u);
}

TEST_F(RequestHandlerForRegisterDeviceAndUserTest,
       HandleRequest_UserNotAllowed) {
  policy_storage()->add_managed_user(kAllowedUserEmail);
  SetGoogleLoginTokenHeader(kDisallowedUserOAuthToken);
  policy_storage()->set_policy_user(kDisallowedUserEmail);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_FORBIDDEN);

  EXPECT_EQ(client_storage()->GetNumberOfRegisteredClients(), 0u);
}

TEST_F(RequestHandlerForRegisterDeviceAndUserTest, HandleRequest_Success) {
  policy_storage()->add_managed_user(kAllowedUserEmail);
  SetGoogleLoginTokenHeader(kAllowedUserOAuthToken);
  policy_storage()->set_policy_user(kAllowedUserEmail);

  em::DeviceManagementRequest device_management_request;
  em::DeviceRegisterRequest* register_request =
      device_management_request.mutable_register_request();
  register_request->set_machine_model(kMachineModel);
  register_request->set_type(em::DeviceRegisterRequest::USER);
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);

  ASSERT_TRUE(HasResponseBody());
  em::DeviceManagementResponse device_management_response =
      GetDeviceManagementResponse();
  const em::DeviceRegisterResponse& register_response =
      device_management_response.register_response();
  EXPECT_FALSE(register_response.device_management_token().empty());
  EXPECT_FALSE(register_response.machine_name().empty());
  EXPECT_EQ(register_response.enrollment_type(),
            em::DeviceRegisterResponse::ENTERPRISE);

  ASSERT_EQ(client_storage()->GetNumberOfRegisteredClients(), 1u);
  const ClientStorage::ClientInfo* client_info =
      client_storage()->GetClientOrNull(kDeviceId);
  ASSERT_NE(client_info, nullptr);
  EXPECT_EQ(client_info->device_id, kDeviceId);
  EXPECT_EQ(client_info->device_token,
            register_response.device_management_token());
  EXPECT_EQ(client_info->machine_name, register_response.machine_name());
  EXPECT_EQ(client_info->username, kAllowedUserEmail);
  EXPECT_FALSE(client_info->allowed_policy_types.empty());
}

}  // namespace policy
