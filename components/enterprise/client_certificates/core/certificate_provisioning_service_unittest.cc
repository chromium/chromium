// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/certificate_provisioning_service.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/enterprise/client_certificates/core/certificate_store.h"
#include "components/enterprise/client_certificates/core/client_identity.h"
#include "components/enterprise/client_certificates/core/constants.h"
#include "components/enterprise/client_certificates/core/key_upload_client.h"
#include "components/enterprise/client_certificates/core/mock_certificate_store.h"
#include "components/enterprise/client_certificates/core/mock_key_upload_client.h"
#include "components/enterprise/client_certificates/core/mock_private_key.h"
#include "components/enterprise/client_certificates/core/prefs.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/store_error.h"
#include "components/prefs/testing_pref_service.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceCallback;
using testing::_;
using testing::StrictMock;

namespace client_certificates {

namespace {

constexpr int kSuccessUploadCode = 200;

scoped_refptr<net::X509Certificate> LoadTestCert() {
  static constexpr char kTestCertFileName[] = "client_1.pem";
  return net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                 kTestCertFileName);
}

scoped_refptr<net::X509Certificate> LoadOtherTestCert() {
  static constexpr char kTestCertFileName[] = "client_2.pem";
  return net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                 kTestCertFileName);
}

}  // namespace

class CertificateProvisioningServiceTest : public testing::Test {
 protected:
  CertificateProvisioningServiceTest() {
    RegisterProfilePrefs(pref_service_.registry());
  }

  void SetPolicyPref(bool enabled) {
    pref_service_.SetManagedPref(
        prefs::kProvisionManagedClientCertificateForUserPrefs,
        base::Value(enabled ? 1 : 0));
  }

  void CreateService(std::unique_ptr<KeyUploadClient> upload_client) {
    service_ = CertificateProvisioningService::Create(
        &pref_service_, &mock_store_, std::move(upload_client));
  }

  void VerifySuccessState(scoped_refptr<PrivateKey> expected_private_key,
                          scoped_refptr<net::X509Certificate> expected_cert) {
    ASSERT_TRUE(service_);

    base::test::TestFuture<std::optional<ClientIdentity>> test_future;
    service_->GetManagedIdentity(test_future.GetCallback());
    auto managed_identity = test_future.Get();

    ASSERT_TRUE(managed_identity.has_value());
    EXPECT_EQ(managed_identity->certificate, expected_cert);
    EXPECT_EQ(managed_identity->private_key, expected_private_key);
    EXPECT_EQ(managed_identity->name, kManagedProfileIdentityName);

    auto status = service_->GetCurrentStatus();
    EXPECT_FALSE(status.is_provisioning);
    ASSERT_TRUE(status.identity.has_value());
    EXPECT_EQ(status.identity.value(), managed_identity.value());
    ASSERT_TRUE(status.last_upload_code.has_value());
    EXPECT_EQ(status.last_upload_code.value(), kSuccessUploadCode);
  }

  void VerifyIdleWithCache(scoped_refptr<PrivateKey> expected_private_key,
                           scoped_refptr<net::X509Certificate> expected_cert,
                           std::optional<HttpCodeOrClientError>
                               expected_upload_code = std::nullopt) {
    auto status = service_->GetCurrentStatus();
    EXPECT_FALSE(status.is_provisioning);
    EXPECT_TRUE(status.identity.has_value());
    EXPECT_EQ(status.identity->private_key, expected_private_key);
    EXPECT_EQ(status.identity->certificate, expected_cert);
    EXPECT_EQ(status.last_upload_code, expected_upload_code);
  }

  void VerifyDisabled() {
    ASSERT_TRUE(service_);
    base::test::TestFuture<std::optional<ClientIdentity>> test_future;
    service_->GetManagedIdentity(test_future.GetCallback());
    EXPECT_FALSE(test_future.Get().has_value());
  }

  void VerifyIdledWithoutCache(std::optional<HttpCodeOrClientError>
                                   expected_upload_code = std::nullopt) {
    auto status = service_->GetCurrentStatus();
    EXPECT_FALSE(status.is_provisioning);
    EXPECT_FALSE(status.identity.has_value());
    EXPECT_EQ(status.last_upload_code, expected_upload_code);
  }

  // To really make sure the expired cert is considered expired, advance
  // the test clock by a few hundred years.
  void AdvanceClockToDistantFuture() {
    task_environment_.AdvanceClock(base::Days(500 * 365));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  StrictMock<MockCertificateStore> mock_store_;
  TestingPrefServiceSimple pref_service_;

  std::unique_ptr<CertificateProvisioningService> service_;
};

// Tests that the service will properly provision the identity when the policy
// pref is already enabled and there is no pre-existing identity in the store.
TEST_F(CertificateProvisioningServiceTest,
       CreatedWithPref_Empty_ProvisionsIdentity) {
  SetPolicyPref(true);
  EXPECT_CALL(mock_store_, GetIdentity(kManagedProfileIdentityName, _))
      .WillOnce(RunOnceCallback<1>(std::nullopt));

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  auto fake_cert = LoadTestCert();
  EXPECT_CALL(mock_store_,
              CreatePrivateKey(kTemporaryManagedProfileIdentityName, _))
      .WillOnce(RunOnceCallback<1>(mocked_private_key));

  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  EXPECT_CALL(*mock_client,
              CreateCertificate(testing::Eq(mocked_private_key), _))
      .WillOnce(RunOnceCallback<1>(kSuccessUploadCode, fake_cert));

  EXPECT_CALL(mock_store_,
              CommitIdentity(kTemporaryManagedProfileIdentityName,
                             kManagedProfileIdentityName, fake_cert, _))
      .WillOnce(RunOnceCallback<3>(std::nullopt));

  CreateService(std::move(mock_client));

  VerifySuccessState(mocked_private_key, fake_cert);

  // Disabling the policy afterwards prevents GetManagedIdentity from returning
  // a value.
  SetPolicyPref(false);
  VerifyDisabled();
}

// Tests that the service will properly provision the identity when the policy
// pref becomes enabled post-creation and there is no pre-existing identity in
// the store.
TEST_F(CertificateProvisioningServiceTest,
       CreatedWithoutPref_Empty_ProvisionsIdentity) {
  EXPECT_CALL(mock_store_, GetIdentity(kManagedProfileIdentityName, _))
      .WillOnce(RunOnceCallback<1>(std::nullopt));

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  auto fake_cert = LoadTestCert();
  EXPECT_CALL(mock_store_,
              CreatePrivateKey(kTemporaryManagedProfileIdentityName, _))
      .WillOnce(RunOnceCallback<1>(mocked_private_key));

  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  EXPECT_CALL(*mock_client,
              CreateCertificate(testing::Eq(mocked_private_key), _))
      .WillOnce(RunOnceCallback<1>(kSuccessUploadCode, fake_cert));

  EXPECT_CALL(mock_store_,
              CommitIdentity(kTemporaryManagedProfileIdentityName,
                             kManagedProfileIdentityName, fake_cert, _))
      .WillOnce(RunOnceCallback<3>(std::nullopt));

  CreateService(std::move(mock_client));

  SetPolicyPref(true);

  VerifySuccessState(mocked_private_key, fake_cert);
}

// When the policy pref is disabled, the service's creation doesn't trigger
// certificate provisioning.
TEST_F(CertificateProvisioningServiceTest,
       Created_PolicyDisabled_NothingHappens) {
  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  CreateService(std::move(mock_client));

  VerifyDisabled();
}

// When the service is created, the policy is enabled and the store has an
// existing identity, the service will simply load it up and sync the key.
TEST_F(CertificateProvisioningServiceTest,
       CreatedWithPref_ExistingIdentityLoaded) {
  SetPolicyPref(true);

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  auto fake_cert = LoadTestCert();
  ClientIdentity existing_permanent_identity(kManagedProfileIdentityName,
                                             mocked_private_key, fake_cert);

  EXPECT_CALL(mock_store_, GetIdentity(kManagedProfileIdentityName, _))
      .WillOnce(RunOnceCallback<1>(existing_permanent_identity));

  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  EXPECT_CALL(*mock_client, SyncKey(testing::Eq(mocked_private_key), _))
      .WillOnce(RunOnceCallback<1>(kSuccessUploadCode));

  CreateService(std::move(mock_client));

  VerifySuccessState(mocked_private_key, fake_cert);
}

// When the service is created, the policy is enabled and the store has an
// existing identity that only has a private key, the service will get a new
// certificate for that private key and then commit it.
TEST_F(CertificateProvisioningServiceTest,
       CreatedWithPref_ExistingIdentity_NoCertificate) {
  SetPolicyPref(true);

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  ClientIdentity existing_permanent_identity(
      kManagedProfileIdentityName, mocked_private_key, /*certificate=*/nullptr);

  EXPECT_CALL(mock_store_, GetIdentity(kManagedProfileIdentityName, _))
      .WillOnce(RunOnceCallback<1>(existing_permanent_identity));

  auto fake_cert = LoadTestCert();
  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  EXPECT_CALL(*mock_client,
              CreateCertificate(testing::Eq(mocked_private_key), _))
      .WillOnce(RunOnceCallback<1>(kSuccessUploadCode, fake_cert));

  EXPECT_CALL(mock_store_,
              CommitCertificate(kManagedProfileIdentityName, fake_cert, _))
      .WillOnce(RunOnceCallback<2>(std::nullopt));
  CreateService(std::move(mock_client));

  VerifySuccessState(mocked_private_key, fake_cert);
}

// Tests what happens when the GetIdentity provisioning step fails.
TEST_F(CertificateProvisioningServiceTest,
       CreatedWithPref_Empty_GetIdentityFails) {
  SetPolicyPref(true);

  base::OnceCallback<void(StoreErrorOr<std::optional<ClientIdentity>>)>
      get_identity_callback;
  EXPECT_CALL(mock_store_, GetIdentity(kManagedProfileIdentityName, _))
      .WillOnce(MoveArg<1>(&get_identity_callback));

  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  CreateService(std::move(mock_client));
  ASSERT_TRUE(service_);

  base::test::TestFuture<std::optional<ClientIdentity>> test_future;
  service_->GetManagedIdentity(test_future.GetCallback());

  ASSERT_TRUE(get_identity_callback);
  std::move(get_identity_callback)
      .Run(base::unexpected(StoreError::kGetDatabaseEntryFailed));

  EXPECT_FALSE(test_future.Get().has_value());
  VerifyIdledWithoutCache();
}

// Tests what happens when the CreateKey provisioning step fails.
TEST_F(CertificateProvisioningServiceTest,
       CreatedWithPref_Empty_CreateKeyFails) {
  SetPolicyPref(true);
  EXPECT_CALL(mock_store_, GetIdentity(kManagedProfileIdentityName, _))
      .WillOnce(RunOnceCallback<1>(std::nullopt));

  base::OnceCallback<void(StoreErrorOr<scoped_refptr<PrivateKey>>)>
      create_key_callback;
  EXPECT_CALL(mock_store_,
              CreatePrivateKey(kTemporaryManagedProfileIdentityName, _))
      .WillOnce(MoveArg<1>(&create_key_callback));

  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  CreateService(std::move(mock_client));
  ASSERT_TRUE(service_);

  base::test::TestFuture<std::optional<ClientIdentity>> test_future;
  service_->GetManagedIdentity(test_future.GetCallback());

  ASSERT_TRUE(create_key_callback);
  std::move(create_key_callback)
      .Run(base::unexpected(StoreError::kCreateKeyFailed));

  EXPECT_FALSE(test_future.Get().has_value());
  VerifyIdledWithoutCache();
}

// Tests what happens when the CreateCertificate provisioning step fails.
TEST_F(CertificateProvisioningServiceTest,
       CreatedWithPref_Empty_CreateCertificateFails) {
  SetPolicyPref(true);
  EXPECT_CALL(mock_store_, GetIdentity(kManagedProfileIdentityName, _))
      .WillOnce(RunOnceCallback<1>(std::nullopt));

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  EXPECT_CALL(mock_store_,
              CreatePrivateKey(kTemporaryManagedProfileIdentityName, _))
      .WillOnce(RunOnceCallback<1>(mocked_private_key));

  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  KeyUploadClient::CreateCertificateCallback create_certificate_callback;
  EXPECT_CALL(*mock_client,
              CreateCertificate(testing::Eq(mocked_private_key), _))
      .WillOnce(MoveArg<1>(&create_certificate_callback));

  CreateService(std::move(mock_client));
  ASSERT_TRUE(service_);

  base::test::TestFuture<std::optional<ClientIdentity>> test_future;
  service_->GetManagedIdentity(test_future.GetCallback());

  ASSERT_TRUE(create_certificate_callback);
  HttpCodeOrClientError create_certificate_error =
      base::unexpected(UploadClientError::kSignatureCreationFailed);
  std::move(create_certificate_callback).Run(create_certificate_error, nullptr);

  EXPECT_FALSE(test_future.Get().has_value());
  VerifyIdledWithoutCache(create_certificate_error);
}

// Tests what happens when the CreateCertificate provisioning step succeeds, but
// doesn't return a certificate.
TEST_F(CertificateProvisioningServiceTest,
       CreatedWithPref_Empty_CreateCertificateSucceeds_NoCert) {
  SetPolicyPref(true);
  EXPECT_CALL(mock_store_, GetIdentity(kManagedProfileIdentityName, _))
      .WillOnce(RunOnceCallback<1>(std::nullopt));

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  EXPECT_CALL(mock_store_,
              CreatePrivateKey(kTemporaryManagedProfileIdentityName, _))
      .WillOnce(RunOnceCallback<1>(mocked_private_key));

  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  KeyUploadClient::CreateCertificateCallback create_certificate_callback;
  EXPECT_CALL(*mock_client,
              CreateCertificate(testing::Eq(mocked_private_key), _))
      .WillOnce(MoveArg<1>(&create_certificate_callback));

  CreateService(std::move(mock_client));
  ASSERT_TRUE(service_);

  base::test::TestFuture<std::optional<ClientIdentity>> test_future;
  service_->GetManagedIdentity(test_future.GetCallback());

  ASSERT_TRUE(create_certificate_callback);
  std::move(create_certificate_callback).Run(kSuccessUploadCode, nullptr);

  EXPECT_FALSE(test_future.Get().has_value());
  VerifyIdledWithoutCache(kSuccessUploadCode);
}

// Tests what happens when the CommitIdentity provisioning step fails.
TEST_F(CertificateProvisioningServiceTest,
       CreatedWithPref_Empty_CommitIdentityFails) {
  SetPolicyPref(true);
  EXPECT_CALL(mock_store_, GetIdentity(kManagedProfileIdentityName, _))
      .WillOnce(RunOnceCallback<1>(std::nullopt));

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  EXPECT_CALL(mock_store_,
              CreatePrivateKey(kTemporaryManagedProfileIdentityName, _))
      .WillOnce(RunOnceCallback<1>(mocked_private_key));

  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  auto fake_cert = LoadTestCert();
  EXPECT_CALL(*mock_client,
              CreateCertificate(testing::Eq(mocked_private_key), _))
      .WillOnce(RunOnceCallback<1>(kSuccessUploadCode, fake_cert));

  base::OnceCallback<void(std::optional<StoreError>)> commit_identity_callback;
  EXPECT_CALL(mock_store_,
              CommitIdentity(kTemporaryManagedProfileIdentityName,
                             kManagedProfileIdentityName, fake_cert, _))
      .WillOnce(MoveArg<3>(&commit_identity_callback));

  CreateService(std::move(mock_client));
  ASSERT_TRUE(service_);

  base::test::TestFuture<std::optional<ClientIdentity>> test_future;
  service_->GetManagedIdentity(test_future.GetCallback());

  ASSERT_TRUE(commit_identity_callback);
  std::move(commit_identity_callback).Run(StoreError::kInvalidCertificateInput);

  EXPECT_FALSE(test_future.Get().has_value());
  VerifyIdledWithoutCache(kSuccessUploadCode);
}

// Tests what happens when a private key already exists, but the
// CommitCertificate provisioning step fails.
TEST_F(CertificateProvisioningServiceTest,
       CreatedWithPref_ExistingIdentity_CommitCertificateFails) {
  SetPolicyPref(true);

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  ClientIdentity existing_permanent_identity(
      kManagedProfileIdentityName, mocked_private_key, /*certificate=*/nullptr);

  EXPECT_CALL(mock_store_, GetIdentity(kManagedProfileIdentityName, _))
      .WillOnce(RunOnceCallback<1>(existing_permanent_identity));

  auto fake_cert = LoadTestCert();
  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  EXPECT_CALL(*mock_client,
              CreateCertificate(testing::Eq(mocked_private_key), _))
      .WillOnce(RunOnceCallback<1>(kSuccessUploadCode, fake_cert));

  base::OnceCallback<void(std::optional<StoreError>)> commit_cert_callback;
  EXPECT_CALL(mock_store_,
              CommitCertificate(kManagedProfileIdentityName, fake_cert, _))
      .WillOnce(MoveArg<2>(&commit_cert_callback));

  CreateService(std::move(mock_client));
  ASSERT_TRUE(service_);

  base::test::TestFuture<std::optional<ClientIdentity>> test_future;
  service_->GetManagedIdentity(test_future.GetCallback());

  ASSERT_TRUE(commit_cert_callback);
  std::move(commit_cert_callback).Run(StoreError::kInvalidCertificateInput);

  EXPECT_FALSE(test_future.Get().has_value());
  VerifyIdledWithoutCache(kSuccessUploadCode);
}

// Tests that the provisioning flow will renew an expired certificate when
// loading it, and only invoke the pending callback when the new certificate
// is available.
TEST_F(CertificateProvisioningServiceTest,
       CreatedWithPref_Empty_ProvisionsIdentity_RenewsExpiredCert) {
  AdvanceClockToDistantFuture();
  SetPolicyPref(true);

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  auto test_cert = LoadTestCert();
  ClientIdentity existing_permanent_identity(kManagedProfileIdentityName,
                                             mocked_private_key, test_cert);

  EXPECT_CALL(mock_store_, GetIdentity(kManagedProfileIdentityName, _))
      .WillOnce(RunOnceCallback<1>(existing_permanent_identity));

  auto fake_cert = LoadTestCert();
  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  EXPECT_CALL(*mock_client,
              CreateCertificate(testing::Eq(mocked_private_key), _))
      .WillOnce(RunOnceCallback<1>(kSuccessUploadCode, fake_cert));

  EXPECT_CALL(mock_store_,
              CommitCertificate(kManagedProfileIdentityName, fake_cert, _))
      .WillOnce(RunOnceCallback<2>(std::nullopt));
  CreateService(std::move(mock_client));

  VerifyIdleWithCache(mocked_private_key, fake_cert, kSuccessUploadCode);
}

// Tests that the provisioning flow will attempt to renew an expired certificate
// when loading it, but fail to create a new certificate. It will then invoke
// pending callbacks with the expired certificate, but will re-attempt to renew
// the certificate on subsequent calls.
TEST_F(
    CertificateProvisioningServiceTest,
    CreatedWithPref_Empty_ProvisionsIdentity_RenewsExpiredCert_FailsDownload) {
  SetPolicyPref(true);
  AdvanceClockToDistantFuture();

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  auto test_cert = LoadTestCert();
  ClientIdentity existing_permanent_identity(kManagedProfileIdentityName,
                                             mocked_private_key, test_cert);

  EXPECT_CALL(mock_store_, GetIdentity(kManagedProfileIdentityName, _))
      .WillOnce(RunOnceCallback<1>(existing_permanent_identity));

  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  auto* mock_client_ptr = mock_client.get();
  EXPECT_CALL(*mock_client_ptr,
              CreateCertificate(testing::Eq(mocked_private_key), _))
      .WillOnce(RunOnceCallback<1>(kSuccessUploadCode, nullptr));

  CreateService(std::move(mock_client));
  ASSERT_TRUE(service_);

  VerifyIdleWithCache(mocked_private_key, test_cert, kSuccessUploadCode);

  EXPECT_CALL(mock_store_, GetIdentity(kManagedProfileIdentityName, _))
      .WillOnce(RunOnceCallback<1>(existing_permanent_identity));

  auto other_test_cert = LoadOtherTestCert();
  EXPECT_CALL(*mock_client_ptr,
              CreateCertificate(testing::Eq(mocked_private_key), _))
      .WillOnce(RunOnceCallback<1>(kSuccessUploadCode, other_test_cert));

  EXPECT_CALL(mock_store_, CommitCertificate(kManagedProfileIdentityName,
                                             other_test_cert, _))
      .WillOnce(RunOnceCallback<2>(std::nullopt));

  base::test::TestFuture<std::optional<ClientIdentity>> test_future;
  service_->GetManagedIdentity(test_future.GetCallback());

  auto renewed_identity = test_future.Get();
  VerifyIdleWithCache(mocked_private_key, other_test_cert, kSuccessUploadCode);
  ASSERT_TRUE(renewed_identity.has_value());
  ASSERT_TRUE(renewed_identity->is_valid());
  EXPECT_EQ(renewed_identity->private_key, mocked_private_key);
  EXPECT_EQ(renewed_identity->certificate, other_test_cert);
}

// Tests that concurrent GetManagedIdentity calls don't provision multiple
// managed identities, and will wait for existing provisioning processes.
TEST_F(CertificateProvisioningServiceTest, ConcurrentGetManagedIdentityCalls) {
  SetPolicyPref(true);
  EXPECT_CALL(mock_store_, GetIdentity(kManagedProfileIdentityName, _))
      .WillOnce(RunOnceCallback<1>(std::nullopt));

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  auto fake_cert = LoadTestCert();
  EXPECT_CALL(mock_store_,
              CreatePrivateKey(kTemporaryManagedProfileIdentityName, _))
      .WillOnce(RunOnceCallback<1>(mocked_private_key));

  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  KeyUploadClient::CreateCertificateCallback create_certificate_callback;
  EXPECT_CALL(*mock_client,
              CreateCertificate(testing::Eq(mocked_private_key), _))
      .WillOnce(MoveArg<1>(&create_certificate_callback));

  EXPECT_CALL(mock_store_,
              CommitIdentity(kTemporaryManagedProfileIdentityName,
                             kManagedProfileIdentityName, fake_cert, _))
      .WillOnce(RunOnceCallback<3>(std::nullopt));

  CreateService(std::move(mock_client));

  ASSERT_TRUE(service_);

  base::test::TestFuture<std::optional<ClientIdentity>> test_future_1;
  service_->GetManagedIdentity(test_future_1.GetCallback());

  base::test::TestFuture<std::optional<ClientIdentity>> test_future_2;
  service_->GetManagedIdentity(test_future_2.GetCallback());

  // Mimic that the certificate is returned by the network call.
  ASSERT_TRUE(create_certificate_callback);
  std::move(create_certificate_callback).Run(kSuccessUploadCode, fake_cert);

  auto managed_identity_1 = test_future_1.Get();
  ASSERT_TRUE(managed_identity_1.has_value());
  EXPECT_EQ(managed_identity_1->certificate, fake_cert);
  EXPECT_EQ(managed_identity_1->private_key, mocked_private_key);
  EXPECT_EQ(managed_identity_1->name, kManagedProfileIdentityName);

  auto managed_identity_2 = test_future_2.Get();
  ASSERT_TRUE(managed_identity_2.has_value());
  EXPECT_EQ(managed_identity_2->certificate, fake_cert);
  EXPECT_EQ(managed_identity_2->private_key, mocked_private_key);
  EXPECT_EQ(managed_identity_2->name, kManagedProfileIdentityName);

  VerifyIdleWithCache(mocked_private_key, fake_cert, kSuccessUploadCode);
}

}  // namespace client_certificates
