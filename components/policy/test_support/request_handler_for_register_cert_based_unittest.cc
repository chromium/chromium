// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "components/policy/test_support/request_handler_for_register_browser.h"

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/embedded_policy_test_server_test_base.h"
#include "components/policy/test_support/policy_storage.h"
#include "device_management_backend.pb.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

using testing::IsEmpty;

constexpr char kDeviceId[] = "fake_device_id";
constexpr char kMachineModel[] = "iPhone 10";
constexpr char kBrandCode[] = "iPhone";
constexpr char kMachineId[] = "11123";
constexpr char kExtraData[] = "fake_extra_data";

}  // namespace

class RequestHandlerForRegisterCertBasedTest
    : public EmbeddedPolicyTestServerTestBase {
 protected:
  RequestHandlerForRegisterCertBasedTest() = default;
  ~RequestHandlerForRegisterCertBasedTest() override = default;

  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();

    SetRequestTypeParam(dm_protocol::kValueRequestCertBasedRegister);
    SetAppType(dm_protocol::kValueAppType);
    SetDeviceIdParam(kDeviceId);
    SetDeviceType(dm_protocol::kValueDeviceType);
  }
};

TEST_F(RequestHandlerForRegisterCertBasedTest, HandleRequest_Success) {
  em::CertificateBasedDeviceRegistrationData register_data;
  register_data.set_certificate_type(
      em::CertificateBasedDeviceRegistrationData::
          ENTERPRISE_ENROLLMENT_CERTIFICATE);
  em::DeviceRegisterRequest* register_request =
      register_data.mutable_device_register_request();
  register_request->set_machine_model(kMachineModel);
  register_request->set_type(em::DeviceRegisterRequest::USER);
  register_request->set_brand_code(kBrandCode);
  register_request->set_machine_id(kMachineId);

  em::DeviceManagementRequest device_management_request;
  em::CertificateBasedDeviceRegisterRequest* cert_request =
      device_management_request.mutable_certificate_based_register_request();
  cert_request->mutable_signed_request()->set_data(
      register_data.SerializeAsString() + kExtraData);
  cert_request->mutable_signed_request()->set_extra_data_bytes(
      std::string(kExtraData).length());
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);

  ASSERT_TRUE(HasResponseBody());
  auto response = GetDeviceManagementResponse();
  EXPECT_FALSE(response.register_response().device_management_token().empty());
  EXPECT_FALSE(response.register_response().machine_name().empty());
  EXPECT_EQ(response.register_response().enrollment_type(),
            em::DeviceRegisterResponse::ENTERPRISE);

  ASSERT_EQ(client_storage()->GetNumberOfRegisteredClients(), 1u);
  const ClientStorage::ClientInfo* client_info =
      client_storage()->GetClientOrNull(kDeviceId);
  ASSERT_NE(client_info, nullptr);
  EXPECT_EQ(client_info->device_id, kDeviceId);
  EXPECT_EQ(client_info->device_token,
            response.register_response().device_management_token());
  EXPECT_EQ(client_info->machine_name,
            response.register_response().machine_name());
  EXPECT_FALSE(client_info->username.has_value());
  EXPECT_FALSE(client_info->allowed_policy_types.empty());
}

TEST_F(RequestHandlerForRegisterCertBasedTest, HandleRequest_InvalidData) {
  em::DeviceManagementRequest device_management_request;
  em::CertificateBasedDeviceRegisterRequest* cert_request =
      device_management_request.mutable_certificate_based_register_request();
  cert_request->mutable_signed_request()->set_data(kExtraData);
  cert_request->mutable_signed_request()->set_extra_data_bytes(0);
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_BAD_REQUEST);
}

TEST_F(RequestHandlerForRegisterCertBasedTest,
       HandleRequest_MissingCertificateType) {
  em::CertificateBasedDeviceRegistrationData register_data;
  em::DeviceManagementRequest device_management_request;
  em::CertificateBasedDeviceRegisterRequest* cert_request =
      device_management_request.mutable_certificate_based_register_request();
  cert_request->mutable_signed_request()->set_data(
      register_data.SerializeAsString());
  cert_request->mutable_signed_request()->set_extra_data_bytes(0);
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_FORBIDDEN);
}

}  // namespace policy
