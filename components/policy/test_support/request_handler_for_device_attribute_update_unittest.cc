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

}  // namespace

class RequestHandlerForDeviceAttributeUpdateTest
    : public EmbeddedPolicyTestServerTestBase {
 protected:
  RequestHandlerForDeviceAttributeUpdateTest() = default;
  ~RequestHandlerForDeviceAttributeUpdateTest() override = default;

  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();

    SetRequestTypeParam(dm_protocol::kValueRequestDeviceAttributeUpdate);
    SetAppType(dm_protocol::kValueAppType);
    SetDeviceIdParam(kDeviceId);
    SetDeviceType(dm_protocol::kValueDeviceType);
  }
};

TEST_F(RequestHandlerForDeviceAttributeUpdateTest, HandleRequest) {
  em::DeviceManagementRequest device_management_request;
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);

  ASSERT_TRUE(HasResponseBody());
  auto response = GetDeviceManagementResponse();
  EXPECT_EQ(response.device_attribute_update_response().result(),
            em::DeviceAttributeUpdateResponse::ATTRIBUTE_UPDATE_SUCCESS);
}

}  // namespace policy
