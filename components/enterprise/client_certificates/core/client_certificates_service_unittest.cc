// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/client_certificates_service.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/enterprise/client_certificates/core/certificate_provisioning_service.h"
#include "components/enterprise/client_certificates/core/client_identity.h"
#include "components/enterprise/client_certificates/core/mock_certificate_provisioning_service.h"
#include "components/enterprise/client_certificates/core/mock_private_key.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/test_ssl_private_key.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace client_certificates {

using base::test::RunOnceCallback;
using testing::_;
using testing::Return;
using testing::StrictMock;

namespace {

class MockClientCertStore : public net::ClientCertStore {
 public:
  MockClientCertStore() = default;
  ~MockClientCertStore() override = default;

  MOCK_METHOD(void,
              GetClientCerts,
              (scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
               net::ClientCertStore::ClientCertListCallback),
              (override));
};

scoped_refptr<net::X509Certificate> LoadTestPlatformCert() {
  static constexpr char kTestCertFileName[] = "client_1.pem";
  return net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                 kTestCertFileName);
}

scoped_refptr<net::X509Certificate> LoadTestManagedCert() {
  static constexpr char kTestCertFileName[] = "client_2.pem";
  return net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                 kTestCertFileName);
}

}  // namespace

class ClientCertificatesServiceTest : public testing::Test {
 protected:
  std::unique_ptr<MockClientCertStore> CreateMockedStore(
      net::ClientCertIdentityList identity_list) {
    auto store = std::make_unique<MockClientCertStore>();
    EXPECT_CALL(*store, GetClientCerts(_, _))
        .WillOnce(RunOnceCallback<1>(std::move(identity_list)));
    return store;
  }

  base::test::TaskEnvironment task_environment_;

  MockCertificateProvisioningService mock_provisioning_service_;
};

// Tests that only the platform identity is returned when there are no managed
// identity.
TEST_F(ClientCertificatesServiceTest, GetClientCertsNoManagedIdentity) {
  auto platform_cert = LoadTestPlatformCert();
  auto platform_identities =
      net::FakeClientCertIdentityListFromCertificateList({platform_cert});

  auto mocked_store = CreateMockedStore(std::move(platform_identities));

  EXPECT_CALL(mock_provisioning_service_, GetManagedIdentity(_))
      .WillOnce(RunOnceCallback<0>(std::nullopt));

  auto service = ClientCertificatesService::Create(&mock_provisioning_service_,
                                                   std::move(mocked_store));

  auto request_info = base::MakeRefCounted<net::SSLCertRequestInfo>();
  base::test::TestFuture<net::ClientCertIdentityList> test_future;
  service->GetClientCerts(request_info, test_future.GetCallback());

  const auto& certs = test_future.Get();
  ASSERT_EQ(certs.size(), 1U);
  EXPECT_EQ(certs[0]->certificate(), platform_cert.get());
}

// Tests that both the platform identity and managed identity are returned.
TEST_F(ClientCertificatesServiceTest,
       GetClientCerts_WithManagedIdentity_NoSSLKey) {
  auto platform_cert = LoadTestPlatformCert();
  auto platform_identities =
      net::FakeClientCertIdentityListFromCertificateList({platform_cert});

  auto mocked_store = CreateMockedStore(std::move(platform_identities));

  auto managed_cert = LoadTestManagedCert();
  ClientIdentity managed_identity(
      "managed", base::MakeRefCounted<StrictMock<MockPrivateKey>>(),
      managed_cert);
  EXPECT_CALL(mock_provisioning_service_, GetManagedIdentity(_))
      .WillOnce(RunOnceCallback<0>(managed_identity));

  auto service = ClientCertificatesService::Create(&mock_provisioning_service_,
                                                   std::move(mocked_store));

  auto request_info = base::MakeRefCounted<net::SSLCertRequestInfo>();
  base::test::TestFuture<net::ClientCertIdentityList> test_future;
  service->GetClientCerts(request_info, test_future.GetCallback());

  const auto& certs = test_future.Get();
  ASSERT_EQ(certs.size(), 1U);
  EXPECT_EQ(certs[0]->certificate(), platform_cert.get());
}

// Tests that both the platform identity and managed identity are returned when
// the managed identity has a valid key to be used with TLS.
TEST_F(ClientCertificatesServiceTest, GetClientCerts_WithManagedIdentity) {
  auto platform_cert = LoadTestPlatformCert();
  auto platform_identities =
      net::FakeClientCertIdentityListFromCertificateList({platform_cert});

  auto mocked_store = CreateMockedStore(std::move(platform_identities));

  auto managed_cert = LoadTestManagedCert();

  // Make sure the mocked PrivateKey has a SSLPrivateKey.
  // net::CreateFailSigningSSLPrivateKey is being used as signing won't be
  // tested here.
  auto mock_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>(
      PrivateKeySource::kUnexportableKey,
      net::CreateFailSigningSSLPrivateKey());
  ClientIdentity managed_identity("managed", std::move(mock_private_key),
                                  managed_cert);
  EXPECT_CALL(mock_provisioning_service_, GetManagedIdentity(_))
      .WillOnce(RunOnceCallback<0>(managed_identity));

  auto service = ClientCertificatesService::Create(&mock_provisioning_service_,
                                                   std::move(mocked_store));

  auto request_info = base::MakeRefCounted<net::SSLCertRequestInfo>();
  base::test::TestFuture<net::ClientCertIdentityList> test_future;
  service->GetClientCerts(request_info, test_future.GetCallback());

  const auto& certs = test_future.Get();
  ASSERT_EQ(certs.size(), 2U);
  EXPECT_EQ(certs[0]->certificate(), platform_cert.get());
  EXPECT_EQ(certs[1]->certificate(), managed_cert.get());
}

}  // namespace client_certificates
