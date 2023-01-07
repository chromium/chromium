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
constexpr char kBrandCode[] = "Google Pixel";
constexpr char kSerialNumber[] = "AXD123145";
constexpr char kManagementDomain[] = "example.com";

}  // namespace

class RequestHandlerForDeviceInitialEnrollmentStateTest
    : public EmbeddedPolicyTestServerTestBase {
 protected:
  RequestHandlerForDeviceInitialEnrollmentStateTest() = default;
  ~RequestHandlerForDeviceInitialEnrollmentStateTest() override = default;

  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();

    SetRequestTypeParam(
        dm_protocol::kValueRequestInitialEnrollmentStateRetrieval);
    SetAppType(dm_protocol::kValueAppType);
    SetDeviceIdParam(kDeviceId);
    SetDeviceType(dm_protocol::kValueDeviceType);
  }
};

TEST_F(RequestHandlerForDeviceInitialEnrollmentStateTest, HandleRequest) {
  policy_storage()->SetInitialEnrollmentState(
      base::StrCat({kBrandCode, "_", kSerialNumber}),
      PolicyStorage::InitialEnrollmentState{
          .initial_enrollment_mode = em::DeviceInitialEnrollmentStateResponse::
              INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED,
          .management_domain = kManagementDomain});

  em::DeviceManagementRequest device_management_request;
  em::DeviceInitialEnrollmentStateRequest* enrollment_request =
      device_management_request
          .mutable_device_initial_enrollment_state_request();
  enrollment_request->set_brand_code(kBrandCode);
  enrollment_request->set_serial_number(kSerialNumber);
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);

  ASSERT_TRUE(HasResponseBody());
  auto response = GetDeviceManagementResponse();
  EXPECT_EQ(response.device_initial_enrollment_state_response()
                .initial_enrollment_mode(),
            em::DeviceInitialEnrollmentStateResponse::
                INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED);
  EXPECT_EQ(
      response.device_initial_enrollment_state_response().management_domain(),
      kManagementDomain);
}

}  // namespace policy
