// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_register_browser.h"

#include <utility>

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/embedded_policy_test_server_test_base.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr char kDeviceId[] = "fake_device_id";
constexpr char kEnrollmentToken[] = "fake_enrollment_token";
constexpr char kMachineName[] = "fake_machine_name";

}  // namespace

class RequestHandlerForRegisterBrowserTest
    : public EmbeddedPolicyTestServerTestBase {
 protected:
  RequestHandlerForRegisterBrowserTest() = default;
  ~RequestHandlerForRegisterBrowserTest() override = default;

  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();

    SetRequestTypeParam(dm_protocol::kValueRequestRegisterBrowser);
    SetAppType(dm_protocol::kValueAppType);
    SetDeviceIdParam(kDeviceId);
    SetDeviceType(dm_protocol::kValueDeviceType);
  }
};

TEST_F(RequestHandlerForRegisterBrowserTest, HandleRequest_NoEnrollmentToken) {
  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_UNAUTHORIZED);

  EXPECT_EQ(client_storage()->GetNumberOfRegisteredClients(), 0u);
}

TEST_F(RequestHandlerForRegisterBrowserTest,
       HandleRequest_NoDeviceInformation_Desktop) {
  em::DeviceManagementRequest device_management_request;
  device_management_request.mutable_register_browser_request()->set_os_platform(
      "Windows");

  SetEnrollmentTokenHeader(kEnrollmentToken);
  SetPayload(em::DeviceManagementRequest());

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_BAD_REQUEST);

  EXPECT_EQ(client_storage()->GetNumberOfRegisteredClients(), 0u);
}

TEST_F(RequestHandlerForRegisterBrowserTest,
       HandleRequest_InvalidEnrollmentToken) {
  em::DeviceManagementRequest device_management_request;
  em::RegisterBrowserRequest* register_browser_request =
      device_management_request.mutable_register_browser_request();
  register_browser_request->set_os_platform("Windows");
  register_browser_request->set_machine_name(kMachineName);

  SetEnrollmentTokenHeader(kInvalidEnrollmentToken);
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_UNAUTHORIZED);

  EXPECT_EQ(client_storage()->GetNumberOfRegisteredClients(), 0u);
}

TEST_F(RequestHandlerForRegisterBrowserTest, HandleRequest_Success) {
  em::DeviceManagementRequest device_management_request;
  em::RegisterBrowserRequest* register_browser_request =
      device_management_request.mutable_register_browser_request();
  register_browser_request->set_os_platform("Windows");
  register_browser_request->set_machine_name(kMachineName);

  SetEnrollmentTokenHeader(kEnrollmentToken);
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);

  ASSERT_TRUE(HasResponseBody());
  em::DeviceManagementResponse device_management_response =
      GetDeviceManagementResponse();
  EXPECT_EQ(
      device_management_response.register_response().device_management_token(),
      kFakeDeviceToken);

  ASSERT_EQ(client_storage()->GetNumberOfRegisteredClients(), 1u);
  const ClientStorage::ClientInfo* client_info =
      client_storage()->GetClientOrNull(kDeviceId);
  ASSERT_NE(client_info, nullptr);
  EXPECT_EQ(client_info->device_id, kDeviceId);
  EXPECT_EQ(client_info->device_token, kFakeDeviceToken);
  EXPECT_EQ(client_info->machine_name, kMachineName);
}

TEST_F(RequestHandlerForRegisterBrowserTest,
       HandleRequest_Success_NoDeviceInformation_Mobile) {
  em::DeviceManagementRequest device_management_request;
  device_management_request.mutable_register_browser_request()->set_os_platform(
      "Android");

  SetEnrollmentTokenHeader(kEnrollmentToken);
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);

  ASSERT_TRUE(HasResponseBody());
  em::DeviceManagementResponse device_management_response =
      GetDeviceManagementResponse();
  EXPECT_EQ(
      device_management_response.register_response().device_management_token(),
      kFakeDeviceToken);

  ASSERT_EQ(client_storage()->GetNumberOfRegisteredClients(), 1u);
  const ClientStorage::ClientInfo* client_info =
      client_storage()->GetClientOrNull(kDeviceId);
  ASSERT_NE(client_info, nullptr);
  EXPECT_EQ(client_info->device_id, kDeviceId);
  EXPECT_EQ(client_info->device_token, kFakeDeviceToken);
  EXPECT_TRUE(client_info->machine_name.empty());
}

class RequestHandlerForRegisterPolicyAgentTest
    : public EmbeddedPolicyTestServerTestBase {
 protected:
  RequestHandlerForRegisterPolicyAgentTest() = default;
  ~RequestHandlerForRegisterPolicyAgentTest() override = default;

  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();

    SetRequestTypeParam(dm_protocol::kValueRequestRegisterPolicyAgent);
    SetAppType(dm_protocol::kValueAppType);
    SetDeviceIdParam(kDeviceId);
    SetDeviceType(dm_protocol::kValueDeviceType);
  }
};

TEST_F(RequestHandlerForRegisterPolicyAgentTest,
       HandleRequest_NoEnrollmentToken) {
  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_UNAUTHORIZED);

  EXPECT_EQ(client_storage()->GetNumberOfRegisteredClients(), 0u);
}

TEST_F(RequestHandlerForRegisterPolicyAgentTest,
       HandleRequest_NoDeviceInformation) {
  em::DeviceManagementRequest device_management_request;
  device_management_request.mutable_register_browser_request()->set_os_platform(
      "Windows");

  SetEnrollmentTokenHeader(kEnrollmentToken);
  SetPayload(em::DeviceManagementRequest());

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_BAD_REQUEST);

  EXPECT_EQ(client_storage()->GetNumberOfRegisteredClients(), 0u);
}

TEST_F(RequestHandlerForRegisterPolicyAgentTest,
       HandleRequest_InvalidEnrollmentToken) {
  em::DeviceManagementRequest device_management_request;
  em::RegisterBrowserRequest* register_browser_request =
      device_management_request.mutable_register_browser_request();
  register_browser_request->set_os_platform("Linux");
  register_browser_request->set_machine_name(kMachineName);

  SetEnrollmentTokenHeader(kInvalidEnrollmentToken);
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_UNAUTHORIZED);

  EXPECT_EQ(client_storage()->GetNumberOfRegisteredClients(), 0u);
}

TEST_F(RequestHandlerForRegisterPolicyAgentTest, HandleRequest_Success) {
  em::DeviceManagementRequest device_management_request;
  em::RegisterBrowserRequest* register_browser_request =
      device_management_request.mutable_register_browser_request();
  register_browser_request->set_os_platform("Mac OS X");
  register_browser_request->set_machine_name(kMachineName);

  SetEnrollmentTokenHeader(kEnrollmentToken);
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);

  ASSERT_TRUE(HasResponseBody());
  em::DeviceManagementResponse device_management_response =
      GetDeviceManagementResponse();
  EXPECT_EQ(
      device_management_response.register_response().device_management_token(),
      kFakeDeviceToken);

  ASSERT_EQ(client_storage()->GetNumberOfRegisteredClients(), 1u);
  const ClientStorage::ClientInfo* client_info =
      client_storage()->GetClientOrNull(kDeviceId);
  ASSERT_NE(client_info, nullptr);
  EXPECT_EQ(client_info->device_id, kDeviceId);
  EXPECT_EQ(client_info->device_token, kFakeDeviceToken);
  EXPECT_EQ(client_info->machine_name, kMachineName);
}

TEST_F(RequestHandlerForRegisterPolicyAgentTest, HandleRequest_Mobile) {
  em::DeviceManagementRequest device_management_request;
  device_management_request.mutable_register_browser_request()->set_os_platform(
      "Android");

  SetEnrollmentTokenHeader(kEnrollmentToken);
  SetPayload(em::DeviceManagementRequest());

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_BAD_REQUEST);

  EXPECT_EQ(client_storage()->GetNumberOfRegisteredClients(), 0u);
}

}  // namespace policy
