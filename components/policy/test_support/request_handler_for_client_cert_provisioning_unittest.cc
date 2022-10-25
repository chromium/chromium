// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_client_cert_provisioning.h"

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/test_support/embedded_policy_test_server_test_base.h"
#include "crypto/rsa_private_key.h"
#include "crypto/signature_creator.h"
#include "device_management_backend.pb.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr char kDeviceId[] = "fake_device_id";

}  // namespace

class RequestHandlerForClientCertProvisioningTest
    : public EmbeddedPolicyTestServerTestBase {
 protected:
  RequestHandlerForClientCertProvisioningTest() = default;
  ~RequestHandlerForClientCertProvisioningTest() override = default;

  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();

    SetRequestTypeParam(dm_protocol::kValueRequestCertProvisioningRequest);
    SetAppType(dm_protocol::kValueAppType);
    SetDeviceIdParam(kDeviceId);
    SetDeviceType(dm_protocol::kValueDeviceType);
  }
};

TEST_F(RequestHandlerForClientCertProvisioningTest,
       HandleStartCsrRequest_Succeeds) {
  em::DeviceManagementRequest device_management_request;
  em::ClientCertificateProvisioningRequest*
      client_certificate_provisioning_request =
          device_management_request
              .mutable_client_certificate_provisioning_request();
  client_certificate_provisioning_request->mutable_start_csr_request();
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  em::DeviceManagementResponse device_management_response =
      GetDeviceManagementResponse();
  em::ClientCertificateProvisioningResponse provisioning_response =
      device_management_response.client_certificate_provisioning_response();
  EXPECT_TRUE(provisioning_response.has_start_csr_response());
  em::StartCsrResponse start_csr_response =
      provisioning_response.start_csr_response();
  EXPECT_EQ(start_csr_response.invalidation_topic(), "invalidation_topic_123");
  EXPECT_EQ(start_csr_response.hashing_algorithm(),
            em::HashingAlgorithm::SHA256);
  EXPECT_EQ(start_csr_response.signing_algorithm(),
            em::SigningAlgorithm::RSA_PKCS1_V1_5);
  EXPECT_EQ(start_csr_response.data_to_sign(), "data_to_sign_123");
}

TEST_F(RequestHandlerForClientCertProvisioningTest,
       HandleFinishCsrRequest_Succeeds) {
  em::DeviceManagementRequest device_management_request;
  em::ClientCertificateProvisioningRequest*
      client_certificate_provisioning_request =
          device_management_request
              .mutable_client_certificate_provisioning_request();
  client_certificate_provisioning_request->mutable_finish_csr_request();
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  em::DeviceManagementResponse device_management_response =
      GetDeviceManagementResponse();
  EXPECT_TRUE(
      device_management_response.client_certificate_provisioning_response()
          .has_finish_csr_response());
}

TEST_F(RequestHandlerForClientCertProvisioningTest,
       HandleDownloadCsrRequest_Succeeds) {
  em::DeviceManagementRequest device_management_request;
  em::ClientCertificateProvisioningRequest*
      client_certificate_provisioning_request =
          device_management_request
              .mutable_client_certificate_provisioning_request();
  auto private_key = crypto::RSAPrivateKey::Create(2048);
  std::vector<uint8_t> pk;
  ASSERT_TRUE(private_key->ExportPublicKey(&pk));
  client_certificate_provisioning_request->set_public_key(
      std::string(pk.begin(), pk.end()));
  client_certificate_provisioning_request->mutable_download_cert_request();
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  em::DeviceManagementResponse device_management_response =
      GetDeviceManagementResponse();
  em::ClientCertificateProvisioningResponse provisioning_response =
      device_management_response.client_certificate_provisioning_response();
  EXPECT_TRUE(provisioning_response.has_download_cert_response());
  EXPECT_TRUE(provisioning_response.download_cert_response()
                  .has_pem_encoded_certificate());
}

TEST_F(RequestHandlerForClientCertProvisioningTest,
       HandleNoSpecificRequest_Fails) {
  em::DeviceManagementRequest device_management_request;
  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_BAD_REQUEST);
}

}  // namespace policy
