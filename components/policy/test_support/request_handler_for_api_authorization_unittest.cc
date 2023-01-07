// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_api_authorization.h"

#include <utility>

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/test_support/embedded_policy_test_server_test_base.h"
#include "components/policy/test_support/policy_storage.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

constexpr char kDeviceId[] = "fake_device_id";
constexpr char kRobotApiAuthCode[] = "fake_auth_code";

}  // namespace

class RequestHandlerForApiAuthorizationTest
    : public EmbeddedPolicyTestServerTestBase {
 public:
  RequestHandlerForApiAuthorizationTest() = default;
  ~RequestHandlerForApiAuthorizationTest() override = default;

  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();

    SetRequestTypeParam(dm_protocol::kValueRequestApiAuthorization);
    SetAppType(dm_protocol::kValueAppType);
    SetDeviceIdParam(kDeviceId);
    SetDeviceType(dm_protocol::kValueDeviceType);
  }
};

TEST_F(RequestHandlerForApiAuthorizationTest, HandleRequest) {
  policy_storage()->set_robot_api_auth_code(kRobotApiAuthCode);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  ASSERT_TRUE(HasResponseBody());
  auto response = GetDeviceManagementResponse();
  EXPECT_EQ(response.service_api_access_response().auth_code(),
            kRobotApiAuthCode);
}

}  // namespace policy
