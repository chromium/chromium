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
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/enterprise/client_certificates/core/certificate_store.h"
#include "components/enterprise/client_certificates/core/client_identity.h"
#include "components/enterprise/client_certificates/core/constants.h"
#include "components/enterprise/client_certificates/core/context_delegate.h"
#include "components/enterprise/client_certificates/core/key_upload_client.h"
#include "components/enterprise/client_certificates/core/metrics_util.h"
#include "components/enterprise/client_certificates/core/mock_certificate_store.h"
#include "components/enterprise/client_certificates/core/mock_context_delegate.h"
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
using testing::Return;
using testing::StrictMock;

namespace client_certificates {

namespace {

constexpr int kSuccessUploadCode = 200;
constexpr char kIdentityName[] = "IdentityName";
constexpr char kTempIdentityName[] = "TempIdentityName";
constexpr char kLoggingContext[] = "Profile";

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
    RegisterLocalStatePrefs(pref_service_.registry());
  }

  const std::string pref() {
    return prefs::kProvisionManagedClientCertificateForUserPrefs;
  }

  void SetPolicyPref(bool enabled) {
    pref_service_.SetManagedPref(pref(), base::Value(enabled ? 1 : 0));
  }

  void CreateProvisioningService(
      std::unique_ptr<ContextDelegate> context_delegate,
      std::unique_ptr<KeyUploadClient> upload_client) {
    service_ = CertificateProvisioningService::Create(
        &pref_service_, &mock_store_, std::move(context_delegate),
        std::move(upload_client));
  }

  void VerifySuccessState(scoped_refptr<PrivateKey> expected_private_key,
                          scoped_refptr<net::X509Certificate> expected_cert,
                          bool was_uploaded = true) {
    ASSERT_TRUE(service_);

    base::test::TestFuture<std::optional<ClientIdentity>> test_future;
    service_->GetManagedIdentity(test_future.GetCallback());
    auto managed_identity = test_future.Get();

    ASSERT_TRUE(managed_identity.has_value());
    EXPECT_EQ(managed_identity->certificate, expected_cert);
    EXPECT_EQ(managed_identity->private_key, expected_private_key);
    EXPECT_EQ(managed_identity->name, kIdentityName);

    auto status = service_->GetCurrentStatus();
    EXPECT_FALSE(status.is_provisioning);
    ASSERT_TRUE(status.identity.has_value());
    EXPECT_EQ(status.identity.value(), managed_identity.value());

    ASSERT_EQ(status.last_upload_code.has_value(), was_uploaded);
    if (was_uploaded) {
      EXPECT_EQ(status.last_upload_code.value(), kSuccessUploadCode);
    }
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

  std::unique_ptr<MockContextDelegate> CreateContextDelegate() {
    auto context_delegate = std::make_unique<MockContextDelegate>();
    EXPECT_CALL(*context_delegate, GetPolicyPref())
        .WillRepeatedly(Return(pref()));
    EXPECT_CALL(*context_delegate, GetIdentityName())
        .WillRepeatedly(Return(kIdentityName));
    EXPECT_CALL(*context_delegate, GetTemporaryIdentityName())
        .WillRepeatedly(Return(kTempIdentityName));
    EXPECT_CALL(*context_delegate, GetLoggingContext())
        .WillRepeatedly(Return(kLoggingContext));
    return context_delegate;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::HistogramTester histogram_tester_;

  StrictMock<MockCertificateStore> mock_store_;
  TestingPrefServiceSimple pref_service_;

  std::unique_ptr<CertificateProvisioningService> service_;
};

// Tests that the service will properly provision the identity when the policy
// pref is already enabled and there is no pre-existing identity in the store.
TEST_F(CertificateProvisioningServiceTest,
       CreatedWithPref_Empty_ProvisionsIdentity) {
  SetPolicyPref(true);
  EXPECT_CALL(mock_store_, GetIdentity(kIdentityName, _))
      .WillOnce(RunOnceCallback<1>(std::nullopt));

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  auto fake_cert = LoadTestCert();
  EXPECT_CALL(mock_store_, CreatePrivateKey(kTempIdentityName, _))
      .WillOnce(RunOnceCallback<1>(mocked_private_key));

  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  EXPECT_CALL(*mock_client,
              CreateCertificate(testing::Eq(mocked_private_key), _))
      .WillOnce(RunOnceCallback<1>(kSuccessUploadCode, fake_cert));

  EXPECT_CALL(mock_store_,
              CommitIdentity(kTempIdentityName, kIdentityName, fake_cert, _))
      .WillOnce(RunOnceCallback<3>(std::nullopt));

  CreateProvisioningService(CreateContextDelegate(), std::move(mock_client));

  VerifySuccessState(mocked_private_key, fake_cert);

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.ClientCertificate.Profile.CreateCertificate.UploadCode",
      kSuccessUploadCode, 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.ClientCertificate.Profile.CreatePrivateKey.Source",
      PrivateKeySource::kUnexportableKey, 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.ClientCertificate.Profile.CreateCertificate.Success.HasCert",
      true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.ClientCertificate.Profile.Provisioning.CertificateCreation."
      "Outcome",
      true, 1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.ClientCertificate.Profile.Provisioning.CertificateCreation."
      "Success.Latency",
      1);
  EXPECT_EQ(
      histogram_tester_.GetTotalCountsForPrefix("Enterprise.ClientCertificate")
          .size(),
      5U);

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
  EXPECT_CALL(mock_store_, GetIdentity(kIdentityName, _))
      .WillOnce(RunOnceCallback<1>(std::nullopt));

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  auto fake_cert = LoadTestCert();
  EXPECT_CALL(mock_store_, CreatePrivateKey(kTempIdentityName, _))
      .WillOnce(RunOnceCallback<1>(mocked_private_key));

  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  EXPECT_CALL(*mock_client,
              CreateCertificate(testing::Eq(mocked_private_key), _))
      .WillOnce(RunOnceCallback<1>(kSuccessUploadCode, fake_cert));

  EXPECT_CALL(mock_store_,
              CommitIdentity(kTempIdentityName, kIdentityName, fake_cert, _))
      .WillOnce(RunOnceCallback<3>(std::nullopt));

  CreateProvisioningService(CreateContextDelegate(), std::move(mock_client));

  SetPolicyPref(true);

  VerifySuccessState(mocked_private_key, fake_cert);
}

// When the policy pref is disabled, the service's creation doesn't trigger
// certificate provisioning.
TEST_F(CertificateProvisioningServiceTest,
       Created_PolicyDisabled_NothingHappens) {
  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  auto mock_context_delegate =
      std::make_unique<StrictMock<MockContextDelegate>>();
  EXPECT_CALL(*mock_context_delegate, GetPolicyPref())
      .Times(3)
      .WillRepeatedly(Return(pref()));
  CreateProvisioningService(std::move(mock_context_delegate),
                            std::move(mock_client));

  VerifyDisabled();
}

// When the service is created, the policy is enabled and the store has an
// existing identity, the service will simply load it up.
TEST_F(CertificateProvisioningServiceTest,
       CreatedWithPref_ExistingIdentityLoaded) {
  SetPolicyPref(true);

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  auto fake_cert = LoadTestCert();
  ClientIdentity existing_permanent_identity(kIdentityName, mocked_private_key,
                                             fake_cert);

  EXPECT_CALL(mock_store_, GetIdentity(kIdentityName, _))
      .WillOnce(RunOnceCallback<1>(existing_permanent_identity));

  CreateProvisioningService(
      CreateContextDelegate(),
      std::make_unique<StrictMock<MockKeyUploadClient>>());

  VerifySuccessState(mocked_private_key, fake_cert, /*was_uploaded=*/false);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.ClientCertificate.Profile.Provisioning.ExistingIdentity."
      "Outcome",
      true, 1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.ClientCertificate.Profile.Provisioning.ExistingIdentity."
      "Success.Latency",
      1);
}

// When the service is created, the policy is enabled and the store has an
// existing identity that only has a private key, the service will get a new
// certificate for that private key and then commit it.
TEST_F(CertificateProvisioningServiceTest,
       CreatedWithPref_ExistingIdentity_NoCertificate) {
  SetPolicyPref(true);

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  ClientIdentity existing_permanent_identity(kIdentityName, mocked_private_key,
                                             /*certificate=*/nullptr);

  EXPECT_CALL(mock_store_, GetIdentity(kIdentityName, _))
      .WillOnce(RunOnceCallback<1>(existing_permanent_identity));

  auto fake_cert = LoadTestCert();
  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  EXPECT_CALL(*mock_client,
              CreateCertificate(testing::Eq(mocked_private_key), _))
      .WillOnce(RunOnceCallback<1>(kSuccessUploadCode, fake_cert));

  EXPECT_CALL(mock_store_, CommitCertificate(kIdentityName, fake_cert, _))
      .WillOnce(RunOnceCallback<2>(std::nullopt));
  CreateProvisioningService(CreateContextDelegate(), std::move(mock_client));

  VerifySuccessState(mocked_private_key, fake_cert);

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.ClientCertificate.Profile.CreateCertificate.UploadCode",
      kSuccessUploadCode, 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.ClientCertificate.Profile.CreateCertificate.Success.HasCert",
      true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.ClientCertificate.Profile.Provisioning.CertificateCreation."
      "Outcome",
      true, 1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.ClientCertificate.Profile.Provisioning.CertificateCreation."
      "Success.Latency",
      1);
  EXPECT_EQ(
      histogram_tester_.GetTotalCountsForPrefix("Enterprise.ClientCertificate")
          .size(),
      4U);
}

// Tests what happens when the GetIdentity provisioning step fails.
TEST_F(CertificateProvisioningServiceTest,
       CreatedWithPref_Empty_GetIdentityFails) {
  SetPolicyPref(true);

  base::OnceCallback<void(StoreErrorOr<std::optional<ClientIdentity>>)>
      get_identity_callback;
  EXPECT_CALL(mock_store_, GetIdentity(kIdentityName, _))
      .WillOnce(MoveArg<1>(&get_identity_callback));

  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  CreateProvisioningService(CreateContextDelegate(), std::move(mock_client));
  ASSERT_TRUE(service_);

  base::test::TestFuture<std::optional<ClientIdentity>> test_future;
  service_->GetManagedIdentity(test_future.GetCallback());

  ASSERT_TRUE(get_identity_callback);
  std::move(get_identity_callback)
      .Run(base::unexpected(StoreError::kGetDatabaseEntryFailed));

  EXPECT_FALSE(test_future.Get().has_value());
  VerifyIdledWithoutCache();

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.ClientCertificate.Profile.Provisioning.Unknown.Outcome",
      false, 1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.ClientCertificate.Profile.Provisioning.Unknown.Failure."
      "Latency",
      1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.ClientCertificate.Profile.Provisioning.Error",
      ProvisioningError::kIdentityLoadingFailed, 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.ClientCertificate.Profile.Provisioning.Store.Error",
      StoreError::kGetDatabaseEntryFailed, 1);
  EXPECT_EQ(
      histogram_tester_.GetTotalCountsForPrefix("Enterprise.ClientCertificate")
          .size(),
      4U);
}

// Tests what happens when the GetIdentity provisioning step fails with
// a key loading store error. The provisioning service should then provision
// a new identity.
TEST_F(CertificateProvisioningServiceTest,
       CreatedWithPref_Empty_GetIdentityFailsDueToKeyLoad_Fallback) {
  EXPECT_CALL(mock_store_, GetIdentity(kIdentityName, _))
      .WillOnce(
          RunOnceCallback<1>(base::unexpected(StoreError::kLoadKeyFailed)));

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  auto fake_cert = LoadTestCert();
  EXPECT_CALL(mock_store_, CreatePrivateKey(kTempIdentityName, _))
      .WillOnce(RunOnceCallback<1>(mocked_private_key));

  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  EXPECT_CALL(*mock_client,
              CreateCertificate(testing::Eq(mocked_private_key), _))
      .WillOnce(RunOnceCallback<1>(kSuccessUploadCode, fake_cert));

  EXPECT_CALL(mock_store_,
              CommitIdentity(kTempIdentityName, kIdentityName, fake_cert, _))
      .WillOnce(RunOnceCallback<3>(std::nullopt));

  CreateProvisioningService(CreateContextDelegate(), std::move(mock_client));
  ASSERT_TRUE(service_);
  SetPolicyPref(true);

  VerifySuccessState(mocked_private_key, fake_cert);
}

// Tests what happens when the CreateKey provisioning step fails.
TEST_F(CertificateProvisioningServiceTest,
       CreatedWithPref_Empty_CreateKeyFails) {
  SetPolicyPref(true);
  EXPECT_CALL(mock_store_, GetIdentity(kIdentityName, _))
      .WillOnce(RunOnceCallback<1>(std::nullopt));

  base::OnceCallback<void(StoreErrorOr<scoped_refptr<PrivateKey>>)>
      create_key_callback;
  EXPECT_CALL(mock_store_, CreatePrivateKey(kTempIdentityName, _))
      .WillOnce(MoveArg<1>(&create_key_callback));

  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();

  CreateProvisioningService(CreateContextDelegate(), std::move(mock_client));
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
  EXPECT_CALL(mock_store_, GetIdentity(kIdentityName, _))
      .WillOnce(RunOnceCallback<1>(std::nullopt));

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  EXPECT_CALL(mock_store_, CreatePrivateKey(kTempIdentityName, _))
      .WillOnce(RunOnceCallback<1>(mocked_private_key));

  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  KeyUploadClient::CreateCertificateCallback create_certificate_callback;
  EXPECT_CALL(*mock_client,
              CreateCertificate(testing::Eq(mocked_private_key), _))
      .WillOnce(MoveArg<1>(&create_certificate_callback));

  CreateProvisioningService(CreateContextDelegate(), std::move(mock_client));
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
  EXPECT_CALL(mock_store_, GetIdentity(kIdentityName, _))
      .WillOnce(RunOnceCallback<1>(std::nullopt));

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  EXPECT_CALL(mock_store_, CreatePrivateKey(kTempIdentityName, _))
      .WillOnce(RunOnceCallback<1>(mocked_private_key));

  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  KeyUploadClient::CreateCertificateCallback create_certificate_callback;
  EXPECT_CALL(*mock_client,
              CreateCertificate(testing::Eq(mocked_private_key), _))
      .WillOnce(MoveArg<1>(&create_certificate_callback));

  CreateProvisioningService(CreateContextDelegate(), std::move(mock_client));
  ASSERT_TRUE(service_);

  base::test::TestFuture<std::optional<ClientIdentity>> test_future;
  service_->GetManagedIdentity(test_future.GetCallback());

  ASSERT_TRUE(create_certificate_callback);
  std::move(create_certificate_callback).Run(kSuccessUploadCode, nullptr);

  EXPECT_FALSE(test_future.Get().has_value());
  VerifyIdledWithoutCache(kSuccessUploadCode);
}

// Tests that the store returns "conflict" for attempting to create a temporary
// key, so the code will just reuse that key and continue with the flow.
TEST_F(CertificateProvisioningServiceTest, ConflictTemporaryKey_Resolves) {
  SetPolicyPref(true);
  EXPECT_CALL(mock_store_, GetIdentity(kIdentityName, _))
      .WillOnce(RunOnceCallback<1>(std::nullopt));
  EXPECT_CALL(mock_store_, CreatePrivateKey(kTempIdentityName, _))
      .WillOnce(RunOnceCallback<1>(
          base::unexpected(StoreError::kConflictingIdentity)));

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  ClientIdentity existing_temporary_identity(kTempIdentityName,
                                             mocked_private_key,
                                             /*certificate=*/nullptr);
  EXPECT_CALL(mock_store_, GetIdentity(kTempIdentityName, _))
      .WillOnce(RunOnceCallback<1>(existing_temporary_identity));

  auto fake_cert = LoadTestCert();
  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  EXPECT_CALL(*mock_client,
              CreateCertificate(testing::Eq(mocked_private_key), _))
      .WillOnce(RunOnceCallback<1>(kSuccessUploadCode, fake_cert));

  EXPECT_CALL(mock_store_,
              CommitIdentity(kTempIdentityName, kIdentityName, fake_cert, _))
      .WillOnce(RunOnceCallback<3>(std::nullopt));

  CreateProvisioningService(CreateContextDelegate(), std::move(mock_client));

  VerifySuccessState(mocked_private_key, fake_cert);
}

// Tests that the store returns "conflict" for attempting to create a temporary
// key, and then the store fails to load the temporary identity.
TEST_F(CertificateProvisioningServiceTest, ConflictTemporaryKey_FailsLoad) {
  SetPolicyPref(true);
  EXPECT_CALL(mock_store_, GetIdentity(kIdentityName, _))
      .WillOnce(RunOnceCallback<1>(std::nullopt));
  EXPECT_CALL(mock_store_, CreatePrivateKey(kTempIdentityName, _))
      .WillOnce(RunOnceCallback<1>(
          base::unexpected(StoreError::kConflictingIdentity)));

  EXPECT_CALL(mock_store_, GetIdentity(kTempIdentityName, _))
      .WillOnce(RunOnceCallback<1>(
          base::unexpected(StoreError::kInvalidDatabaseState)));

  CreateProvisioningService(
      CreateContextDelegate(),
      std::make_unique<StrictMock<MockKeyUploadClient>>());

  VerifyIdledWithoutCache();
}

// Tests that the store returns "conflict" for attempting to create a temporary
// key, and then the store loads the temporary identity but it is empty.
TEST_F(CertificateProvisioningServiceTest, ConflictTemporaryKey_LoadEmpty) {
  SetPolicyPref(true);
  EXPECT_CALL(mock_store_, GetIdentity(kIdentityName, _))
      .WillOnce(RunOnceCallback<1>(std::nullopt));
  EXPECT_CALL(mock_store_, CreatePrivateKey(kTempIdentityName, _))
      .WillOnce(RunOnceCallback<1>(
          base::unexpected(StoreError::kConflictingIdentity)));

  EXPECT_CALL(mock_store_, GetIdentity(kTempIdentityName, _))
      .WillOnce(RunOnceCallback<1>(std::nullopt));

  CreateProvisioningService(
      CreateContextDelegate(),
      std::make_unique<StrictMock<MockKeyUploadClient>>());

  VerifyIdledWithoutCache();
}

// Tests that the store returns "conflict" for attempting to create a temporary
// key, and then the store loads the temporary identity but it has no private
// key.
TEST_F(CertificateProvisioningServiceTest,
       ConflictTemporaryKey_LoadNoPrivateKey) {
  SetPolicyPref(true);
  EXPECT_CALL(mock_store_, GetIdentity(kIdentityName, _))
      .WillOnce(RunOnceCallback<1>(std::nullopt));
  EXPECT_CALL(mock_store_, CreatePrivateKey(kTempIdentityName, _))
      .WillOnce(RunOnceCallback<1>(
          base::unexpected(StoreError::kConflictingIdentity)));

  ClientIdentity existing_temporary_identity(kTempIdentityName,
                                             /*private_key=*/nullptr,
                                             /*certificate=*/nullptr);
  EXPECT_CALL(mock_store_, GetIdentity(kTempIdentityName, _))
      .WillOnce(RunOnceCallback<1>(existing_temporary_identity));

  CreateProvisioningService(
      CreateContextDelegate(),
      std::make_unique<StrictMock<MockKeyUploadClient>>());

  VerifyIdledWithoutCache();
}

// Tests what happens when the CommitIdentity provisioning step fails.
TEST_F(CertificateProvisioningServiceTest,
       CreatedWithPref_Empty_CommitIdentityFails) {
  SetPolicyPref(true);
  EXPECT_CALL(mock_store_, GetIdentity(kIdentityName, _))
      .WillOnce(RunOnceCallback<1>(std::nullopt));

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  EXPECT_CALL(mock_store_, CreatePrivateKey(kTempIdentityName, _))
      .WillOnce(RunOnceCallback<1>(mocked_private_key));

  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  auto fake_cert = LoadTestCert();
  EXPECT_CALL(*mock_client,
              CreateCertificate(testing::Eq(mocked_private_key), _))
      .WillOnce(RunOnceCallback<1>(kSuccessUploadCode, fake_cert));

  base::OnceCallback<void(std::optional<StoreError>)> commit_identity_callback;
  EXPECT_CALL(mock_store_,
              CommitIdentity(kTempIdentityName, kIdentityName, fake_cert, _))
      .WillOnce(MoveArg<3>(&commit_identity_callback));

  CreateProvisioningService(CreateContextDelegate(), std::move(mock_client));
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
  ClientIdentity existing_permanent_identity(kIdentityName, mocked_private_key,
                                             /*certificate=*/nullptr);

  EXPECT_CALL(mock_store_, GetIdentity(kIdentityName, _))
      .WillOnce(RunOnceCallback<1>(existing_permanent_identity));

  auto fake_cert = LoadTestCert();
  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  EXPECT_CALL(*mock_client,
              CreateCertificate(testing::Eq(mocked_private_key), _))
      .WillOnce(RunOnceCallback<1>(kSuccessUploadCode, fake_cert));

  base::OnceCallback<void(std::optional<StoreError>)> commit_cert_callback;
  EXPECT_CALL(mock_store_, CommitCertificate(kIdentityName, fake_cert, _))
      .WillOnce(MoveArg<2>(&commit_cert_callback));

  CreateProvisioningService(CreateContextDelegate(), std::move(mock_client));
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
  auto expired_test_cert = LoadTestCert();
  ClientIdentity existing_permanent_identity(kIdentityName, mocked_private_key,
                                             expired_test_cert);

  EXPECT_CALL(mock_store_, GetIdentity(kIdentityName, _))
      .WillOnce(RunOnceCallback<1>(existing_permanent_identity));

  auto fake_cert = LoadTestCert();
  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  EXPECT_CALL(*mock_client,
              CreateCertificate(testing::Eq(mocked_private_key), _))
      .WillOnce(RunOnceCallback<1>(kSuccessUploadCode, fake_cert));

  auto mock_context_delegate = CreateContextDelegate();
  EXPECT_CALL(*mock_context_delegate,
              OnClientCertificateDeleted(expired_test_cert));

  EXPECT_CALL(mock_store_, CommitCertificate(kIdentityName, fake_cert, _))
      .WillOnce(RunOnceCallback<2>(std::nullopt));
  CreateProvisioningService(std::move(mock_context_delegate),
                            std::move(mock_client));

  VerifyIdleWithCache(mocked_private_key, fake_cert, kSuccessUploadCode);

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.ClientCertificate.Profile.CreateCertificate.UploadCode",
      kSuccessUploadCode, 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.ClientCertificate.Profile.CreateCertificate.Success.HasCert",
      true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.ClientCertificate.Profile.Provisioning.CertificateRenewal."
      "Outcome",
      true, 1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.ClientCertificate.Profile.Provisioning.CertificateRenewal."
      "Success.Latency",
      1);
  EXPECT_EQ(
      histogram_tester_.GetTotalCountsForPrefix("Enterprise.ClientCertificate")
          .size(),
      4U);
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
  ClientIdentity existing_permanent_identity(kIdentityName, mocked_private_key,
                                             test_cert);

  EXPECT_CALL(mock_store_, GetIdentity(kIdentityName, _))
      .WillOnce(RunOnceCallback<1>(existing_permanent_identity));

  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  auto* mock_client_ptr = mock_client.get();
  EXPECT_CALL(*mock_client_ptr,
              CreateCertificate(testing::Eq(mocked_private_key), _))
      .WillOnce(RunOnceCallback<1>(kSuccessUploadCode, nullptr));

  auto mock_context_delegate = CreateContextDelegate();
  auto* mock_context_delegate_ptr = mock_context_delegate.get();

  CreateProvisioningService(std::move(mock_context_delegate),
                            std::move(mock_client));
  ASSERT_TRUE(service_);

  VerifyIdleWithCache(mocked_private_key, test_cert, kSuccessUploadCode);

  EXPECT_CALL(mock_store_, GetIdentity(kIdentityName, _))
      .WillOnce(RunOnceCallback<1>(existing_permanent_identity));

  auto other_test_cert = LoadOtherTestCert();
  EXPECT_CALL(*mock_client_ptr,
              CreateCertificate(testing::Eq(mocked_private_key), _))
      .WillOnce(RunOnceCallback<1>(kSuccessUploadCode, other_test_cert));

  EXPECT_CALL(*mock_context_delegate_ptr,
              OnClientCertificateDeleted(test_cert));

  EXPECT_CALL(mock_store_, CommitCertificate(kIdentityName, other_test_cert, _))
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
  EXPECT_CALL(mock_store_, GetIdentity(kIdentityName, _))
      .WillOnce(RunOnceCallback<1>(std::nullopt));

  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  auto fake_cert = LoadTestCert();
  EXPECT_CALL(mock_store_, CreatePrivateKey(kTempIdentityName, _))
      .WillOnce(RunOnceCallback<1>(mocked_private_key));

  auto mock_client = std::make_unique<StrictMock<MockKeyUploadClient>>();
  KeyUploadClient::CreateCertificateCallback create_certificate_callback;
  EXPECT_CALL(*mock_client,
              CreateCertificate(testing::Eq(mocked_private_key), _))
      .WillOnce(MoveArg<1>(&create_certificate_callback));

  EXPECT_CALL(mock_store_,
              CommitIdentity(kTempIdentityName, kIdentityName, fake_cert, _))
      .WillOnce(RunOnceCallback<3>(std::nullopt));

  CreateProvisioningService(CreateContextDelegate(), std::move(mock_client));

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
  EXPECT_EQ(managed_identity_1->name, kIdentityName);

  auto managed_identity_2 = test_future_2.Get();
  ASSERT_TRUE(managed_identity_2.has_value());
  EXPECT_EQ(managed_identity_2->certificate, fake_cert);
  EXPECT_EQ(managed_identity_2->private_key, mocked_private_key);
  EXPECT_EQ(managed_identity_2->name, kIdentityName);

  VerifyIdleWithCache(mocked_private_key, fake_cert, kSuccessUploadCode);
}

}  // namespace client_certificates
