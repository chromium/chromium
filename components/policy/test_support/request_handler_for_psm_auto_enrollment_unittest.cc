// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_psm_auto_enrollment.h"
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
constexpr char kEncryptedId1[] = "fake/ecrypted-id";
constexpr char kEncryptedId2[] = "54455354/111111";

}  // namespace

class RequestHandlerForPsmAutoEnrollmentTest
    : public EmbeddedPolicyTestServerTestBase {
 protected:
  RequestHandlerForPsmAutoEnrollmentTest() = default;
  ~RequestHandlerForPsmAutoEnrollmentTest() override = default;

  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();

    SetRequestTypeParam(dm_protocol::kValueRequestPsmHasDeviceState);
    SetAppType(dm_protocol::kValueAppType);
    SetDeviceIdParam(kDeviceId);
    SetDeviceType(dm_protocol::kValueDeviceType);
  }
};

TEST_F(RequestHandlerForPsmAutoEnrollmentTest, HandleRequest_OprfRequest) {
  em::DeviceManagementRequest device_management_request;
  em::PrivateSetMembershipRequest* request =
      device_management_request.mutable_private_set_membership_request();
  request->mutable_rlwe_request()->mutable_oprf_request()->add_encrypted_ids(
      kEncryptedId1);
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  ASSERT_TRUE(HasResponseBody());
  auto response = GetDeviceManagementResponse();
  ASSERT_EQ(response.private_set_membership_response()
                .rlwe_response()
                .oprf_response()
                .doubly_encrypted_ids_size(),
            1);
  EXPECT_EQ(response.private_set_membership_response()
                .rlwe_response()
                .oprf_response()
                .doubly_encrypted_ids(0)
                .queried_encrypted_id(),
            kEncryptedId1);
}

TEST_F(RequestHandlerForPsmAutoEnrollmentTest,
       HandleRequest_QueryRequestNoMembership) {
  em::DeviceManagementRequest device_management_request;
  em::PrivateSetMembershipRequest* request =
      device_management_request.mutable_private_set_membership_request();
  request->mutable_rlwe_request()
      ->mutable_query_request()
      ->add_queries()
      ->set_queried_encrypted_id(kEncryptedId1);
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  ASSERT_TRUE(HasResponseBody());
  auto response = GetDeviceManagementResponse();
  ASSERT_EQ(response.private_set_membership_response()
                .rlwe_response()
                .query_response()
                .pir_responses_size(),
            1);
  EXPECT_EQ(response.private_set_membership_response()
                .rlwe_response()
                .query_response()
                .pir_responses(0)
                .queried_encrypted_id(),
            kEncryptedId1);
  EXPECT_EQ(response.private_set_membership_response()
                .rlwe_response()
                .query_response()
                .pir_responses(0)
                .pir_response()
                .plaintext_entry_size(),
            RequestHandlerForPsmAutoEnrollment::kPirResponseHasNoMembership);
}

TEST_F(RequestHandlerForPsmAutoEnrollmentTest,
       HandleRequest_QueryRequestHasMembership) {
  em::DeviceManagementRequest device_management_request;
  em::PrivateSetMembershipRequest* request =
      device_management_request.mutable_private_set_membership_request();
  request->mutable_rlwe_request()
      ->mutable_query_request()
      ->add_queries()
      ->set_queried_encrypted_id(kEncryptedId2);
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  ASSERT_TRUE(HasResponseBody());
  auto response = GetDeviceManagementResponse();
  ASSERT_EQ(response.private_set_membership_response()
                .rlwe_response()
                .query_response()
                .pir_responses_size(),
            1);
  EXPECT_EQ(response.private_set_membership_response()
                .rlwe_response()
                .query_response()
                .pir_responses(0)
                .queried_encrypted_id(),
            kEncryptedId2);
  EXPECT_EQ(response.private_set_membership_response()
                .rlwe_response()
                .query_response()
                .pir_responses(0)
                .pir_response()
                .plaintext_entry_size(),
            RequestHandlerForPsmAutoEnrollment::kPirResponseHasMembership);
}

TEST_F(RequestHandlerForPsmAutoEnrollmentTest,
       HandleRequest_MissingRequestFields) {
  em::DeviceManagementRequest device_management_request;
  device_management_request.mutable_private_set_membership_request();
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_BAD_REQUEST);
}

}  // namespace policy
