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

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void VerifySuccessState(scoped_refptr<PrivateKey> expected_private_key,
                          scoped_refptr<net::X509Certificate> expected_cert) {
    ASSERT_TRUE(service_);
    auto managed_identity = service_->GetManagedIdentity();
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

  void VerifyDisabled() {
    ASSERT_TRUE(service_);
    EXPECT_FALSE(service_->GetManagedIdentity().has_value());

    auto status = service_->GetCurrentStatus();
    EXPECT_FALSE(status.is_provisioning);
    EXPECT_FALSE(status.identity.has_value());
    EXPECT_FALSE(status.last_upload_code.has_value());
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

  RunUntilIdle();

  VerifySuccessState(mocked_private_key, fake_cert);

  // Disabling the policy afterwards prevents GetManagedIdentity from returning
  // a value.
  SetPolicyPref(false);
  EXPECT_FALSE(service_->GetManagedIdentity().has_value());
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

  RunUntilIdle();

  VerifySuccessState(mocked_private_key, fake_cert);
}

// When the policy pref is disabled, the service's creation doesn't trigger
// certificate provisioning.
TEST_F(CertificateProvisioningServiceTest,
       Created_PolicyDisabled_NothingHappens) {
  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  CreateService(std::move(mock_client));
  RunUntilIdle();

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

  RunUntilIdle();

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

  RunUntilIdle();

  VerifySuccessState(mocked_private_key, fake_cert);
}

}  // namespace client_certificates
