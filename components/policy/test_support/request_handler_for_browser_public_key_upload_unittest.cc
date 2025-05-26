// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_browser_public_key_upload.h"

#include <optional>
#include <utility>

#include "base/base64.h"
#include "base/containers/span.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/embedded_policy_test_server_test_base.h"
#include "components/policy/test_support/policy_storage.h"
#include "crypto/keypair.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace policy {

namespace {

constexpr char kDeviceId[] = "fake_device_id";
constexpr char kProfileId[] = "fake_profile_id";
constexpr char kDmToken[] = "dm_token";

// Base64-encoded EC private key PKCS #8 PrivateKeyInfo block.
constexpr char kEncodedPrivateKey[] =
    "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgzsLi0tbAG43E2lR6vwq4t+"
    "mDmBRGXhkbco2G2v5+0gKhRANCAAQ8YD0im5qm9EF6c1R/Yn9b/"
    "vnWWcVnQwc+rk5VziUjnoRGGyBwC52+5w2R7u68SkkN2BTRGZ/RQ/bop4Mpwa/O";

std::optional<crypto::keypair::PrivateKey> LoadPrivateKey() {
  auto decoded_key = base::Base64Decode(kEncodedPrivateKey);
  if (!decoded_key) {
    return std::nullopt;
  }

  return crypto::keypair::PrivateKey::FromPrivateKeyInfo(decoded_key.value());
}

void VerifyIssuerCommonName(const std::string& pem_encoded_certificate,
                            std::string_view expected_issuer_name) {
  net::CertificateList certs =
      net::X509Certificate::CreateCertificateListFromBytes(
          base::as_byte_span(pem_encoded_certificate),
          net::X509Certificate::FORMAT_AUTO);
  ASSERT_FALSE(certs.empty());
  ASSERT_TRUE(certs[0]);
  EXPECT_EQ(certs[0]->issuer().common_name, expected_issuer_name);
}

}  // namespace

class RequestHandlerForBrowserPublicKeyUploadTest
    : public EmbeddedPolicyTestServerTestBase {
 public:
  RequestHandlerForBrowserPublicKeyUploadTest() = default;
  ~RequestHandlerForBrowserPublicKeyUploadTest() override = default;

  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();

    SetRequestTypeParam(dm_protocol::kValueBrowserUploadPublicKey);
    SetAppType(dm_protocol::kValueAppType);
    SetDeviceType(dm_protocol::kValueDeviceType);
    SetDeviceIdParam(kDeviceId);
    SetDeviceTokenHeader(kDmToken);
  }
};

TEST_F(RequestHandlerForBrowserPublicKeyUploadTest, HandleBrowserKeyUpload) {
  // Can set a fake public key when not requiring a cert.
  enterprise_management::DeviceManagementRequest device_management_request;
  auto* request =
      device_management_request.mutable_browser_public_key_upload_request();
  request->set_public_key("test_key");
  request->set_key_trust_level(BPKUR::CHROME_BROWSER_HW_KEY);
  request->set_key_type(BPKUR::RSA_KEY);

  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  ASSERT_TRUE(HasResponseBody());
  auto response = GetDeviceManagementResponse();
  EXPECT_TRUE(response.has_browser_public_key_upload_response());
}

TEST_F(RequestHandlerForBrowserPublicKeyUploadTest, HandleProfileKeyUpload) {
  SetProfileIdParam(kProfileId);

  // Can set a fake public key when not requiring a cert.
  enterprise_management::DeviceManagementRequest device_management_request;
  auto* request =
      device_management_request.mutable_browser_public_key_upload_request();
  request->set_public_key("test_key");
  request->set_key_trust_level(BPKUR::CHROME_BROWSER_HW_KEY);
  request->set_key_type(BPKUR::RSA_KEY);

  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  ASSERT_TRUE(HasResponseBody());
  auto response = GetDeviceManagementResponse();
  EXPECT_TRUE(response.has_browser_public_key_upload_response());
}

TEST_F(RequestHandlerForBrowserPublicKeyUploadTest, HandleBrowserCertRequest) {
  auto ec_private_key = LoadPrivateKey();
  ASSERT_TRUE(ec_private_key);
  auto public_key = ec_private_key->ToSubjectPublicKeyInfo();

  enterprise_management::DeviceManagementRequest device_management_request;
  auto* request =
      device_management_request.mutable_browser_public_key_upload_request();
  request->set_public_key(public_key.data(), public_key.size());
  request->set_key_trust_level(BPKUR::CHROME_BROWSER_OS_KEY);
  request->set_key_type(BPKUR::EC_KEY);
  request->set_provision_certificate(true);

  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  ASSERT_TRUE(HasResponseBody());
  auto response = GetDeviceManagementResponse();
  ASSERT_TRUE(response.has_browser_public_key_upload_response());

  auto& upload_response = response.browser_public_key_upload_response();
  ASSERT_TRUE(upload_response.has_pem_encoded_certificate());
  VerifyIssuerCommonName(upload_response.pem_encoded_certificate(),
                         "Browser Root CA");
}

TEST_F(RequestHandlerForBrowserPublicKeyUploadTest, HandleProfileCertRequest) {
  auto ec_private_key = LoadPrivateKey();
  ASSERT_TRUE(ec_private_key);
  auto public_key = ec_private_key->ToSubjectPublicKeyInfo();

  SetProfileIdParam(kProfileId);

  enterprise_management::DeviceManagementRequest device_management_request;
  auto* request =
      device_management_request.mutable_browser_public_key_upload_request();
  request->set_public_key(public_key.data(), public_key.size());
  request->set_key_trust_level(BPKUR::CHROME_BROWSER_OS_KEY);
  request->set_key_type(BPKUR::EC_KEY);
  request->set_provision_certificate(true);

  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  ASSERT_TRUE(HasResponseBody());
  auto response = GetDeviceManagementResponse();
  ASSERT_TRUE(response.has_browser_public_key_upload_response());

  auto& upload_response = response.browser_public_key_upload_response();
  ASSERT_TRUE(upload_response.has_pem_encoded_certificate());
  VerifyIssuerCommonName(upload_response.pem_encoded_certificate(),
                         "Profile Root CA");
}

TEST_F(RequestHandlerForBrowserPublicKeyUploadTest,
       FailWithMissingPublicKeyData) {
  enterprise_management::DeviceManagementRequest device_management_request;
  auto* request =
      device_management_request.mutable_browser_public_key_upload_request();
  request->set_key_trust_level(BPKUR::CHROME_BROWSER_HW_KEY);
  request->set_key_type(BPKUR::RSA_KEY);

  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_BAD_REQUEST);
}

TEST_F(RequestHandlerForBrowserPublicKeyUploadTest, FailWithMissingTrustLevel) {
  enterprise_management::DeviceManagementRequest device_management_request;
  auto* request =
      device_management_request.mutable_browser_public_key_upload_request();
  request->set_public_key("test_key");
  request->set_key_type(BPKUR::RSA_KEY);

  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_BAD_REQUEST);
}

TEST_F(RequestHandlerForBrowserPublicKeyUploadTest, FailWithMissingKeyType) {
  enterprise_management::DeviceManagementRequest device_management_request;
  auto* request =
      device_management_request.mutable_browser_public_key_upload_request();
  request->set_public_key("test_key");
  request->set_key_trust_level(BPKUR::CHROME_BROWSER_HW_KEY);

  SetPayload(device_management_request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_BAD_REQUEST);
}

}  // namespace policy
