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

using testing::IsEmpty;
using testing::UnorderedElementsAreArray;

constexpr char kDeviceId[] = "fake_device_id";
constexpr char kBrandSerial[] = "ABC21345748";
constexpr char kTrimmedSHA256HashForBrandSerial[] =
    "\x8C\xB0\x9C\xC2\x11\x70\x9B\xB1";
constexpr char kStateKey1[] = "fake_state_key_1";
constexpr char kStateKey2[] = "fake_state_key_2";
constexpr char kSHA256HashForStateKey1[] =
    "\xB0\x58\x21\x15\x1E\xF5\xEE\x95\x50\xE7\x7D\xB5\x62\x8F\x44\x5A\xE2\x83"
    "\xB1\xD1\x2C\x87\x22\x85\x50\xF9\x9C\x33\x34\x0F\x42\x13";
constexpr char kSHA256HashForStateKey2[] =
    "\xBC\x11\x2A\x4D\x1A\x7F\xA8\x66\xCA\x4F\xF4\xD8\xC3\x0B\xC3\x5B\x83\x0A"
    "\x82\xF1\x2C\x0C\x1A\xBE\x34\xA7\xAD\xF6\x29\x88\x18\x6D";

}  // namespace

class RequestHandlerForAutoEnrollmentTest
    : public EmbeddedPolicyTestServerTestBase {
 protected:
  RequestHandlerForAutoEnrollmentTest() = default;
  ~RequestHandlerForAutoEnrollmentTest() override = default;

  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();

    SetRequestTypeParam(dm_protocol::kValueRequestAutoEnrollment);
    SetAppType(dm_protocol::kValueAppType);
    SetDeviceIdParam(kDeviceId);
    SetDeviceType(dm_protocol::kValueDeviceType);
  }
};

TEST_F(RequestHandlerForAutoEnrollmentTest, HandleRequest_ForcedEnrollment) {
  policy_storage()->SetInitialEnrollmentState(
      kBrandSerial, PolicyStorage::InitialEnrollmentState{});

  em::DeviceManagementRequest device_management_request;
  em::DeviceAutoEnrollmentRequest* enrollment_request =
      device_management_request.mutable_auto_enrollment_request();
  enrollment_request->set_enrollment_check_type(
      em::DeviceAutoEnrollmentRequest::ENROLLMENT_CHECK_TYPE_FORCED_ENROLLMENT);
  // This matches any serial hash since dividing by 1 always gives remainder 0.
  enrollment_request->set_modulus(1);
  enrollment_request->set_remainder(0);
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);

  ASSERT_TRUE(HasResponseBody());
  auto response = GetDeviceManagementResponse();
  EXPECT_THAT(response.auto_enrollment_response().hashes(),
              UnorderedElementsAreArray({kTrimmedSHA256HashForBrandSerial}));
}

TEST_F(RequestHandlerForAutoEnrollmentTest,
       HandleRequest_ForcedEnrollmentMismatchingRemainder) {
  policy_storage()->SetInitialEnrollmentState(
      kBrandSerial, PolicyStorage::InitialEnrollmentState{});

  em::DeviceManagementRequest device_management_request;
  em::DeviceAutoEnrollmentRequest* enrollment_request =
      device_management_request.mutable_auto_enrollment_request();
  enrollment_request->set_enrollment_check_type(
      em::DeviceAutoEnrollmentRequest::ENROLLMENT_CHECK_TYPE_FORCED_ENROLLMENT);
  // Set impossible remainder to ensure than no serial hash matches it.
  enrollment_request->set_modulus(1);
  enrollment_request->set_remainder(1);
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);

  ASSERT_TRUE(HasResponseBody());
  auto response = GetDeviceManagementResponse();
  EXPECT_THAT(response.auto_enrollment_response().hashes(), IsEmpty());
}

TEST_F(RequestHandlerForAutoEnrollmentTest, HandleRequest_ForcedReEnrollment) {
  ClientStorage::ClientInfo client_info;
  client_info.device_id = kDeviceId;
  client_info.state_keys.push_back(kStateKey1);
  client_info.state_keys.push_back(kStateKey2);
  client_storage()->RegisterClient(client_info);

  em::DeviceManagementRequest device_management_request;
  em::DeviceAutoEnrollmentRequest* enrollment_request =
      device_management_request.mutable_auto_enrollment_request();
  enrollment_request->set_enrollment_check_type(
      em::DeviceAutoEnrollmentRequest::ENROLLMENT_CHECK_TYPE_FRE);
  // This matches any serial hash since dividing by 1 always gives remainder 0.
  enrollment_request->set_modulus(1);
  enrollment_request->set_remainder(0);
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);

  ASSERT_TRUE(HasResponseBody());
  auto response = GetDeviceManagementResponse();
  EXPECT_THAT(response.auto_enrollment_response().hashes(),
              UnorderedElementsAreArray(
                  {kSHA256HashForStateKey1, kSHA256HashForStateKey2}));
}

TEST_F(RequestHandlerForAutoEnrollmentTest, HandleRequest_Modulus32) {
  em::DeviceManagementRequest device_management_request;
  em::DeviceAutoEnrollmentRequest* enrollment_request =
      device_management_request.mutable_auto_enrollment_request();
  enrollment_request->set_modulus(32);
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);

  ASSERT_TRUE(HasResponseBody());
  auto response = GetDeviceManagementResponse();
  EXPECT_EQ(response.auto_enrollment_response().expected_modulus(), 1);
}

}  // namespace policy
