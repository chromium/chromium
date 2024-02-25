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
#include "components/enterprise/client_certificates/core/mock_dm_server_client.h"
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
constexpr std::string_view kFakeSignature = "signature";
constexpr std::string_view kFakeSpki = "spki";
constexpr std::string_view kFakeDMToken = "dm_token";
constexpr std::string_view kFakeUploadURL = "https://example.com/upload";

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

enterprise_management::DeviceManagementResponse CreateResponse(
    scoped_refptr<net::X509Certificate> certificate) {
  enterprise_management::DeviceManagementResponse response;

  std::vector<std::string> pem_chain;
  if (certificate && certificate->GetPEMEncodedChain(&pem_chain) &&
      !pem_chain.empty()) {
    auto* upload_response =
        response.mutable_browser_public_key_upload_response();
    upload_response->set_pem_encoded_certificate(pem_chain[0]);
  }

  return response;
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
  void CreateUploadClient(
      std::unique_ptr<MockCloudManagementDelegate> mock_management_delegate,
      std::unique_ptr<MockDMServerClient> mock_dm_server_client) {
    upload_client_ = KeyUploadClient::Create(
        std::move(mock_management_delegate), std::move(mock_dm_server_client));
  }

  scoped_refptr<PrivateKey> SetUpSuccessPath(
      scoped_refptr<net::X509Certificate> fake_cert) {
    auto mock_management_delegate =
        std::make_unique<StrictMock<MockCloudManagementDelegate>>();
    EXPECT_CALL(*mock_management_delegate, GetDMToken())
        .WillOnce(Return(std::string(kFakeDMToken)));
    EXPECT_CALL(*mock_management_delegate, GetUploadBrowserPublicKeyUrl())
        .WillOnce(Return(std::string(kFakeUploadURL)));

    auto mock_dm_server_client =
        std::make_unique<StrictMock<MockDMServerClient>>();

    EXPECT_CALL(*mock_dm_server_client,
                SendRequest(GURL(kFakeUploadURL), kFakeDMToken,
                            EqualsProto(CreateExpectedRequest(
                                /*provision_certificate=*/!!fake_cert)),
                            _))
        .WillOnce(RunOnceCallback<3>(kSuccessCode, CreateResponse(fake_cert)));

    CreateUploadClient(std::move(mock_management_delegate),
                       std::move(mock_dm_server_client));

    auto private_key = CreateMockedKey();
    EXPECT_CALL(*private_key, SignSlowly(_));
    EXPECT_CALL(*private_key, GetSubjectPublicKeyInfo());
    EXPECT_CALL(*private_key, GetAlgorithm());
    return private_key;
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<KeyUploadClient> upload_client_;
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
  auto mock_management_delegate =
      std::make_unique<StrictMock<MockCloudManagementDelegate>>();
  EXPECT_CALL(*mock_management_delegate, GetDMToken())
      .WillOnce(Return(std::nullopt));

  auto mock_dm_server_client =
      std::make_unique<StrictMock<MockDMServerClient>>();
  CreateUploadClient(std::move(mock_management_delegate),
                     std::move(mock_dm_server_client));

  base::test::TestFuture<HttpCodeOrClientError,
                         scoped_refptr<net::X509Certificate>>
      test_future;
  upload_client_->CreateCertificate(nullptr, test_future.GetCallback());

  auto [response_code, certificate] = test_future.Take();

  EXPECT_FALSE(certificate);
  EXPECT_EQ(response_code,
            base::unexpected(UploadClientError::kMissingDMToken));
}

TEST_F(KeyUploadClientTest, CreateCertificate_NoUploadURL_Fail) {
  auto mock_management_delegate =
      std::make_unique<StrictMock<MockCloudManagementDelegate>>();
  EXPECT_CALL(*mock_management_delegate, GetDMToken())
      .WillOnce(Return(std::string(kFakeDMToken)));
  EXPECT_CALL(*mock_management_delegate, GetUploadBrowserPublicKeyUrl())
      .WillOnce(Return(std::nullopt));

  auto mock_dm_server_client =
      std::make_unique<StrictMock<MockDMServerClient>>();
  CreateUploadClient(std::move(mock_management_delegate),
                     std::move(mock_dm_server_client));

  base::test::TestFuture<HttpCodeOrClientError,
                         scoped_refptr<net::X509Certificate>>
      test_future;
  upload_client_->CreateCertificate(nullptr, test_future.GetCallback());

  auto [response_code, certificate] = test_future.Take();

  EXPECT_FALSE(certificate);
  EXPECT_EQ(response_code,
            base::unexpected(UploadClientError::kMissingUploadURL));
}

TEST_F(KeyUploadClientTest, CreateCertificate_InvalidURL_Fail) {
  auto mock_management_delegate =
      std::make_unique<StrictMock<MockCloudManagementDelegate>>();
  EXPECT_CALL(*mock_management_delegate, GetDMToken())
      .WillOnce(Return(std::string(kFakeDMToken)));
  EXPECT_CALL(*mock_management_delegate, GetUploadBrowserPublicKeyUrl())
      .WillOnce(Return("h/t/t/p/s"));

  auto mock_dm_server_client =
      std::make_unique<StrictMock<MockDMServerClient>>();
  CreateUploadClient(std::move(mock_management_delegate),
                     std::move(mock_dm_server_client));

  base::test::TestFuture<HttpCodeOrClientError,
                         scoped_refptr<net::X509Certificate>>
      test_future;
  upload_client_->CreateCertificate(nullptr, test_future.GetCallback());

  auto [response_code, certificate] = test_future.Take();

  EXPECT_FALSE(certificate);
  EXPECT_EQ(response_code,
            base::unexpected(UploadClientError::kInvalidUploadURL));
}

TEST_F(KeyUploadClientTest, CreateCertificate_NullKey_Fail) {
  auto mock_management_delegate =
      std::make_unique<StrictMock<MockCloudManagementDelegate>>();
  EXPECT_CALL(*mock_management_delegate, GetDMToken())
      .WillOnce(Return(std::string(kFakeDMToken)));
  EXPECT_CALL(*mock_management_delegate, GetUploadBrowserPublicKeyUrl())
      .WillOnce(Return(std::string(kFakeUploadURL)));

  auto mock_dm_server_client =
      std::make_unique<StrictMock<MockDMServerClient>>();
  CreateUploadClient(std::move(mock_management_delegate),
                     std::move(mock_dm_server_client));

  base::test::TestFuture<HttpCodeOrClientError,
                         scoped_refptr<net::X509Certificate>>
      test_future;
  upload_client_->CreateCertificate(nullptr, test_future.GetCallback());

  auto [response_code, certificate] = test_future.Take();

  EXPECT_FALSE(certificate);
  EXPECT_EQ(response_code,
            base::unexpected(UploadClientError::kInvalidKeyParameter));
}

TEST_F(KeyUploadClientTest, KeySync_Success) {
  auto private_key = SetUpSuccessPath(nullptr);

  base::test::TestFuture<HttpCodeOrClientError> test_future;
  upload_client_->SyncKey(private_key, test_future.GetCallback());

  auto response_code = test_future.Get();

  EXPECT_EQ(response_code, kSuccessCode);
}

TEST_F(KeyUploadClientTest, KeySync_FailSignature_Fail) {
  auto mock_management_delegate =
      std::make_unique<StrictMock<MockCloudManagementDelegate>>();
  EXPECT_CALL(*mock_management_delegate, GetDMToken())
      .WillOnce(Return(std::string(kFakeDMToken)));
  EXPECT_CALL(*mock_management_delegate, GetUploadBrowserPublicKeyUrl())
      .WillOnce(Return(std::string(kFakeUploadURL)));

  auto mock_dm_server_client =
      std::make_unique<StrictMock<MockDMServerClient>>();

  CreateUploadClient(std::move(mock_management_delegate),
                     std::move(mock_dm_server_client));

  auto private_key = CreateMockedKey();
  EXPECT_CALL(*private_key, SignSlowly(_)).WillOnce(Return(std::nullopt));
  EXPECT_CALL(*private_key, GetSubjectPublicKeyInfo());

  base::test::TestFuture<HttpCodeOrClientError> test_future;
  upload_client_->SyncKey(private_key, test_future.GetCallback());

  auto response_code = test_future.Get();

  EXPECT_EQ(response_code,
            base::unexpected(UploadClientError::kSignatureCreationFailed));
}

}  // namespace client_certificates
