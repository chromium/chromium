// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_upload_euicc_info.h"

#include <utility>

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/test_support/embedded_policy_test_server_test_base.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {
constexpr char kDeviceId[] = "fake_device_id";
}  // namespace

class RequestHandlerForUploadEuiccInfoTest
    : public EmbeddedPolicyTestServerTestBase {
 public:
  RequestHandlerForUploadEuiccInfoTest() = default;
  ~RequestHandlerForUploadEuiccInfoTest() override = default;

  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();

    SetRequestTypeParam(dm_protocol::kValueRequestUploadEuiccInfo);
    SetAppType(dm_protocol::kValueAppType);
    SetDeviceIdParam(kDeviceId);
    SetDeviceType(dm_protocol::kValueDeviceType);
  }
};

TEST_F(RequestHandlerForUploadEuiccInfoTest, HandleRequest) {
  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  ASSERT_TRUE(HasResponseBody());
  auto response = GetDeviceManagementResponse();
  EXPECT_TRUE(response.has_device_status_report_response());
  EXPECT_TRUE(response.has_session_status_report_response());
}

}  // namespace policy
