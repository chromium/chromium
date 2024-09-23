// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/key_upload_client.h"

#include <string_view>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/enterprise/client_certificates/core/cloud_management_delegate.h"
#include "components/enterprise/client_certificates/core/dm_server_client.h"
#include "components/enterprise/client_certificates/core/mock_cloud_management_delegate.h"
#include "components/enterprise/client_certificates/core/mock_private_key.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/upload_client_error.h"
#include "crypto/signature_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace client_certificates {

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;
using base::test::RunOnceCallback;
using testing::_;
using testing::Return;
using testing::StrictMock;

namespace {

constexpr int kSuccessCode = 200;
constexpr int kDeviceIdConflictCode = 409;
constexpr std::string_view kFakeSignature = "signature";
constexpr std::string_view kFakeSpki = "spki";
constexpr std::string kFakeDMToken = "dm_token";

std::vector<uint8_t> ToBytes(std::string_view str) {
  auto bytes = base::as_bytes(base::make_span(str));
  return std::vector<uint8_t>(bytes.begin(), bytes.end());
}

scoped_refptr<MockPrivateKey> CreateMockedKey() {
  auto private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  ON_CALL(*private_key, SignSlowly(_))
      .WillByDefault(Return(ToBytes(kFakeSignature)));
  ON_CALL(*private_key, GetSubjectPublicKeyInfo())
      .WillByDefault(Return(ToBytes(kFakeSpki)));
  ON_CALL(*private_key, GetAlgorithm())
      .WillByDefault(Return(crypto::SignatureVerifier::RSA_PKCS1_SHA1));
  return private_key;
}

enterprise_management::DeviceManagementRequest CreateExpectedRequest(
    bool provision_certificate) {
  enterprise_management::DeviceManagementRequest request;
  auto* upload_request = request.mutable_browser_public_key_upload_request();
  upload_request->set_public_key(std::string(kFakeSpki));
  upload_request->set_signature(std::string(kFakeSignature));
  upload_request->set_key_trust_level(BPKUR::CHROME_BROWSER_HW_KEY);
  upload_request->set_key_type(BPKUR::RSA_KEY);
  upload_request->set_provision_certificate(provision_certificate);
  return request;
}

scoped_refptr<net::X509Certificate> LoadTestCert() {
  static constexpr char kTestCertFileName[] = "client_1.pem";
  return net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                 kTestCertFileName);
}

MATCHER_P(EqualsProto, expected, "") {
  return arg.SerializeAsString() == expected.SerializeAsString();
}

}  // namespace

class KeyUploadClientTest : public testing::Test {
 protected:
  void CreateUploadClient() {
    upload_client_ =
        KeyUploadClient::Create(std::move(mock_management_delegate_));
  }

  scoped_refptr<PrivateKey> SetUpPrivateKey() {
    auto private_key = CreateMockedKey();
    EXPECT_CALL(*private_key, SignSlowly(_));
    EXPECT_CALL(*private_key, GetSubjectPublicKeyInfo());
    EXPECT_CALL(*private_key, GetAlgorithm());
    return private_key;
  }

  void SetUpDMToken(std::optional<std::string> dm_token = kFakeDMToken) {
    EXPECT_CALL(*mock_management_delegate_, GetDMToken())
        .WillOnce(Return(dm_token));
  }

  void SetUpUploadPublicKey(
      policy::DMServerJobResult result,
      scoped_refptr<net::X509Certificate> fake_cert = nullptr) {
    EXPECT_CALL(
        *mock_management_delegate_,
        UploadBrowserPublicKey(EqualsProto(CreateExpectedRequest(
                                   /*provision_certificate=*/!!fake_cert)),
                               _))
        .WillOnce(RunOnceCallback<1>(result));
  }

  policy::DMServerJobResult CreateResult(
      scoped_refptr<net::X509Certificate> certificate = nullptr) {
    policy::DMServerJobResult result;
    result.net_error = net::OK;
    result.dm_status = policy::DM_STATUS_SUCCESS;
    result.response_code = kSuccessCode;
    std::vector<std::string> pem_chain;
    if (certificate && certificate->GetPEMEncodedChain(&pem_chain) &&
        !pem_chain.empty()) {
      result.response.mutable_browser_public_key_upload_response()
          ->set_pem_encoded_certificate(pem_chain[0]);
    }

    return result;
  }

  scoped_refptr<PrivateKey> SetUpSuccessPath(
      scoped_refptr<net::X509Certificate> fake_cert = nullptr) {
    SetUpDMToken();
    SetUpUploadPublicKey(CreateResult(fake_cert), fake_cert);
    CreateUploadClient();
    return SetUpPrivateKey();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<KeyUploadClient> upload_client_;
  std::unique_ptr<enterprise_attestation::MockCloudManagementDelegate>
      mock_management_delegate_ = std::make_unique<
          StrictMock<enterprise_attestation::MockCloudManagementDelegate>>();
};

TEST_F(KeyUploadClientTest, CreateCertificate_Success) {
  auto test_cert = LoadTestCert();
  ASSERT_TRUE(test_cert);
  auto private_key = SetUpSuccessPath(test_cert);

  base::test::TestFuture<HttpCodeOrClientError,
                         scoped_refptr<net::X509Certificate>>
      test_future;
  upload_client_->CreateCertificate(private_key, test_future.GetCallback());

  auto [response_code, certificate] = test_future.Take();

  EXPECT_EQ(response_code, kSuccessCode);
  ASSERT_TRUE(certificate);
  EXPECT_TRUE(test_cert->EqualsIncludingChain(certificate.get()));
}

TEST_F(KeyUploadClientTest, CreateCertificate_NoDMToken_Fail) {
  SetUpDMToken(std::nullopt);
  CreateUploadClient();

  base::test::TestFuture<HttpCodeOrClientError,
                         scoped_refptr<net::X509Certificate>>
      test_future;
  upload_client_->CreateCertificate(nullptr, test_future.GetCallback());

  auto [response_code, certificate] = test_future.Take();

  EXPECT_FALSE(certificate);
  EXPECT_EQ(response_code,
            base::unexpected(UploadClientError::kMissingDMToken));
}

TEST_F(KeyUploadClientTest, CreateCertificate_NullKey_Fail) {
  SetUpDMToken();
  CreateUploadClient();

  base::test::TestFuture<HttpCodeOrClientError,
                         scoped_refptr<net::X509Certificate>>
      test_future;
  upload_client_->CreateCertificate(nullptr, test_future.GetCallback());

  auto [response_code, certificate] = test_future.Take();

  EXPECT_FALSE(certificate);
  EXPECT_EQ(response_code,
            base::unexpected(UploadClientError::kInvalidKeyParameter));
}

TEST_F(KeyUploadClientTest, CreateCertificate_DMServerFailed) {
  auto test_cert = LoadTestCert();
  ASSERT_TRUE(test_cert);

  SetUpDMToken();
  SetUpUploadPublicKey(
      policy::DMServerJobResult{
          nullptr, net::OK, policy::DM_STATUS_SERVICE_DEVICE_ID_CONFLICT,
          kDeviceIdConflictCode,
          enterprise_management::DeviceManagementResponse()},
      test_cert);

  CreateUploadClient();

  auto private_key = SetUpPrivateKey();

  base::test::TestFuture<HttpCodeOrClientError,
                         scoped_refptr<net::X509Certificate>>
      test_future;
  upload_client_->CreateCertificate(private_key, test_future.GetCallback());

  auto [response_code, certificate] = test_future.Take();

  EXPECT_FALSE(certificate);  // Certificate will be empty.
  EXPECT_EQ(response_code, kDeviceIdConflictCode);
}

TEST_F(KeyUploadClientTest, CreateCertificate_NetFailed) {
  auto test_cert = LoadTestCert();
  ASSERT_TRUE(test_cert);

  SetUpDMToken();
  SetUpUploadPublicKey(
      policy::DMServerJobResult{
          nullptr, net::ERR_FAILED, policy::DM_STATUS_REQUEST_FAILED, 0,
          enterprise_management::DeviceManagementResponse()},
      test_cert);

  CreateUploadClient();

  auto private_key = SetUpPrivateKey();

  base::test::TestFuture<HttpCodeOrClientError,
                         scoped_refptr<net::X509Certificate>>
      test_future;
  upload_client_->CreateCertificate(private_key, test_future.GetCallback());

  auto [response_code, certificate] = test_future.Take();

  EXPECT_FALSE(certificate);  // Certificate will be empty.
  EXPECT_EQ(response_code, 0);
}

TEST_F(KeyUploadClientTest, CreateCertificate_MalformedResponse) {
  auto test_cert = LoadTestCert();
  ASSERT_TRUE(test_cert);

  SetUpDMToken();
  SetUpUploadPublicKey(
      policy::DMServerJobResult{
          nullptr, net::OK, policy::DM_STATUS_SUCCESS, kSuccessCode,
          enterprise_management::DeviceManagementResponse()},
      test_cert);

  CreateUploadClient();

  auto private_key = SetUpPrivateKey();

  base::test::TestFuture<HttpCodeOrClientError,
                         scoped_refptr<net::X509Certificate>>
      test_future;
  upload_client_->CreateCertificate(private_key, test_future.GetCallback());

  auto [response_code, certificate] = test_future.Take();

  EXPECT_FALSE(certificate);  // Certificate will be empty.
  EXPECT_EQ(response_code, kSuccessCode);
}

TEST_F(KeyUploadClientTest, KeySync_Success) {
  auto private_key = SetUpSuccessPath();

  base::test::TestFuture<HttpCodeOrClientError> test_future;
  upload_client_->SyncKey(private_key, test_future.GetCallback());

  auto response_code = test_future.Get();

  EXPECT_EQ(response_code, kSuccessCode);
}

TEST_F(KeyUploadClientTest, KeySync_NoDMToken) {
  SetUpDMToken(std::nullopt);
  CreateUploadClient();

  base::test::TestFuture<HttpCodeOrClientError> test_future;
  upload_client_->SyncKey(nullptr, test_future.GetCallback());

  auto response_code = test_future.Take();

  EXPECT_EQ(response_code,
            base::unexpected(UploadClientError::kMissingDMToken));
}

TEST_F(KeyUploadClientTest, KeySync_FailSignature_Fail) {
  SetUpDMToken();
  CreateUploadClient();

  auto private_key = CreateMockedKey();
  EXPECT_CALL(*private_key, SignSlowly(_)).WillOnce(Return(std::nullopt));
  EXPECT_CALL(*private_key, GetSubjectPublicKeyInfo());

  base::test::TestFuture<HttpCodeOrClientError> test_future;
  upload_client_->SyncKey(private_key, test_future.GetCallback());

  auto response_code = test_future.Get();

  EXPECT_EQ(response_code,
            base::unexpected(UploadClientError::kSignatureCreationFailed));
}

TEST_F(KeyUploadClientTest, KeySync_DMServerFailed) {
  SetUpDMToken();
  auto private_key = SetUpPrivateKey();
  SetUpUploadPublicKey(policy::DMServerJobResult{
      nullptr, net::OK,
      policy::DeviceManagementStatus::DM_STATUS_SERVICE_DEVICE_ID_CONFLICT,
      kDeviceIdConflictCode,
      enterprise_management::DeviceManagementResponse()});

  CreateUploadClient();

  base::test::TestFuture<HttpCodeOrClientError> test_future;
  upload_client_->SyncKey(private_key, test_future.GetCallback());
  auto response_code = test_future.Take();

  EXPECT_EQ(response_code, 409);
}

}  // namespace client_certificates
