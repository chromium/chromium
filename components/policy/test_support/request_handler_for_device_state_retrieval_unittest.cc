// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/embedded_policy_test_server_test_base.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/policy/test_support/request_handler_for_register_browser.h"
#include "device_management_backend.pb.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr char kDeviceId[] = "fake_device_id";
constexpr char kStateKey1[] = "fake_state_key_1";
constexpr char kStateKey2[] = "fake_state_key_2";
constexpr char kManagementDomain[] = "example.com";

}  // namespace

class RequestHandlerForDeviceStateRetrievalTest
    : public EmbeddedPolicyTestServerTestBase {
 protected:
  RequestHandlerForDeviceStateRetrievalTest() = default;
  ~RequestHandlerForDeviceStateRetrievalTest() override = default;

  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();

    SetRequestTypeParam(dm_protocol::kValueRequestDeviceStateRetrieval);
    SetAppType(dm_protocol::kValueAppType);
    SetDeviceIdParam(kDeviceId);
    SetDeviceType(dm_protocol::kValueDeviceType);
  }
};

TEST_F(RequestHandlerForDeviceStateRetrievalTest, HandleRequest) {
  ClientStorage::ClientInfo client_info;
  client_info.device_id = kDeviceId;
  client_info.state_keys.push_back(kStateKey1);
  client_info.state_keys.push_back(kStateKey2);
  client_storage()->RegisterClient(client_info);
  policy_storage()->set_device_state(PolicyStorage::DeviceState{
      .management_domain = kManagementDomain,
      .restore_mode = enterprise_management::DeviceStateRetrievalResponse::
          RESTORE_MODE_REENROLLMENT_ZERO_TOUCH});

  em::DeviceManagementRequest device_management_request;
  em::DeviceStateRetrievalRequest* request =
      device_management_request.mutable_device_state_retrieval_request();
  request->set_server_backed_state_key(kStateKey2);
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);

  ASSERT_TRUE(HasResponseBody());
  auto response = GetDeviceManagementResponse();
  EXPECT_EQ(
      response.device_state_retrieval_response().restore_mode(),
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ZERO_TOUCH);
  EXPECT_EQ(response.device_state_retrieval_response().management_domain(),
            kManagementDomain);
}

}  // namespace policy
