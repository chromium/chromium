// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_check_android_management.h"

#include <utility>

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/test_support/embedded_policy_test_server_test_base.h"
#include "components/policy/test_support/policy_storage.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

constexpr char kDeviceId[] = "fake_device_id";

}  // namespace

class RequestHandlerForCheckAndroidManagementTest
    : public EmbeddedPolicyTestServerTestBase {
 public:
  RequestHandlerForCheckAndroidManagementTest() = default;
  ~RequestHandlerForCheckAndroidManagementTest() override = default;

  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();

    SetRequestTypeParam(dm_protocol::kValueRequestCheckAndroidManagement);
    SetAppType(dm_protocol::kValueAppType);
    SetDeviceIdParam(kDeviceId);
    SetDeviceType(dm_protocol::kValueDeviceType);
  }
};

TEST_F(RequestHandlerForCheckAndroidManagementTest, HandleRequest_Success) {
  SetOAuthToken(kUnmanagedAuthToken);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  ASSERT_TRUE(HasResponseBody());
  auto response = GetDeviceManagementResponse();
  EXPECT_TRUE(response.has_check_android_management_response());
}

TEST_F(RequestHandlerForCheckAndroidManagementTest, HandleRequest_Conflict) {
  SetOAuthToken(kManagedAuthToken);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_CONFLICT);
}

TEST_F(RequestHandlerForCheckAndroidManagementTest, HandleRequest_Forbidden) {
  SetOAuthToken("invalid-auth-token");

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_FORBIDDEN);
}

}  // namespace policy
