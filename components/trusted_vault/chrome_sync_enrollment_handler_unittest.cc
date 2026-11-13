// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/chrome_sync_enrollment_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/local_domains_storage.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/standalone_trusted_vault_server_constants.h"
#include "components/trusted_vault/test/fake_local_domains_storage_file_access.h"
#include "components/trusted_vault/test/mock_trusted_vault_throttling_connection.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

namespace {

using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::NiceMock;
using testing::SizeIs;

MATCHER_P(MatchTrustedVaultKeyAndVersions, expected, "") {
  const auto* trusted_vault_keys =
      std::get_if<std::vector<TrustedVaultKeyAndVersion>>(&arg);
  if (!trusted_vault_keys) {
    *result_listener << "does not hold a vector of TrustedVaultKeyAndVersion";
    return false;
  }
  return testing::ExplainMatchResult(expected, *trusted_vault_keys,
                                     result_listener);
}

MATCHER_P(PublicKeyWhenExportedEq, expected, "") {
  return arg.ExportToBytes() == expected;
}

class ChromeSyncEnrollmentHandlerTest : public testing::Test {
 public:
  ChromeSyncEnrollmentHandlerTest() {
    account_info_.gaia = GaiaId("user");
    account_info_.email = "user@gmail.com";

    storage_ = LocalDomainsStorage::CreateForTesting(
        std::make_unique<FakeLocalDomainsStorageFileAccess>());
    storage_->ReadDataFromDisk();

    connection_ =
        std::make_unique<NiceMock<MockTrustedVaultThrottlingConnection>>();

    handler_ = std::make_unique<ChromeSyncEnrollmentHandler>(
        account_info_, storage_.get(), connection_.get());
  }

  void StoreConstantKeys(int last_key_version = 1) {
    storage_->SetVaultKeys(account_info_.gaia, SecurityDomainId::kChromeSync,
                           {GetConstantTrustedVaultKey()}, last_key_version);
  }

  const CoreAccountInfo& account_info() const { return account_info_; }
  LocalDomainsStorage* storage() { return storage_.get(); }
  MockTrustedVaultThrottlingConnection* connection() {
    return connection_.get();
  }
  ChromeSyncEnrollmentHandler* handler() { return handler_.get(); }

  void ResetHandler() { handler_.reset(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  CoreAccountInfo account_info_;
  std::unique_ptr<LocalDomainsStorage> storage_;
  std::unique_ptr<NiceMock<MockTrustedVaultThrottlingConnection>> connection_;
  std::unique_ptr<ChromeSyncEnrollmentHandler> handler_;
};

TEST_F(ChromeSyncEnrollmentHandlerTest,
       ShouldReturnTrueIfConditionsAreSatisfied) {
  const std::vector<uint8_t> kValidPublicKey =
      SecureBoxKeyPair::GenerateRandom()->public_key().ExportToBytes();
  StoreConstantKeys();

  TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
      download_callback;
  EXPECT_CALL(*connection(),
              DownloadGaiaPasswordPublicKey(Eq(account_info()), _))
      .WillOnce(
          [&download_callback](
              const CoreAccountInfo&,
              TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
                  callback) {
            download_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  base::test::TestFuture<bool, TrustedVaultCanEnrollOutcomeForUMA> future;
  handler()->CanEnroll(future.GetCallback());
  ASSERT_FALSE(download_callback.is_null());
  std::move(download_callback)
      .Run(TrustedVaultDownloadPasswordPublicKeyStatus::kSuccess,
           kValidPublicKey);
  EXPECT_TRUE(future.Get<0>());
  EXPECT_EQ(future.Get<1>(), TrustedVaultCanEnrollOutcomeForUMA::kEligible);
}

TEST_F(ChromeSyncEnrollmentHandlerTest,
       ShouldReturnFalseIfPreconditionsNotMet) {
  EXPECT_CALL(*connection(), DownloadGaiaPasswordPublicKey).Times(0);

  // No keys in storage.
  {
    base::test::TestFuture<bool, TrustedVaultCanEnrollOutcomeForUMA> future;
    handler()->CanEnroll(future.GetCallback());
    EXPECT_FALSE(future.Get<0>());
    EXPECT_EQ(future.Get<1>(),
              TrustedVaultCanEnrollOutcomeForUMA::kNoLocalKeys);
  }

  // Non-constant key in storage.
  storage()->SetVaultKeys(account_info().gaia, SecurityDomainId::kChromeSync,
                          {{1, 2, 3}}, /*last_key_version=*/1);
  {
    base::test::TestFuture<bool, TrustedVaultCanEnrollOutcomeForUMA> future;
    handler()->CanEnroll(future.GetCallback());
    EXPECT_FALSE(future.Get<0>());
    EXPECT_EQ(future.Get<1>(),
              TrustedVaultCanEnrollOutcomeForUMA::kAlreadyEnrolled);
  }
}

TEST_F(ChromeSyncEnrollmentHandlerTest,
       ShouldCoalesceConcurrentCanEnrollRequests) {
  const std::vector<uint8_t> kValidPublicKey =
      SecureBoxKeyPair::GenerateRandom()->public_key().ExportToBytes();
  StoreConstantKeys();

  TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
      download_callback;
  EXPECT_CALL(*connection(),
              DownloadGaiaPasswordPublicKey(Eq(account_info()), _))
      .WillOnce(
          [&download_callback](
              const CoreAccountInfo&,
              TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
                  callback) {
            download_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  base::test::TestFuture<bool, TrustedVaultCanEnrollOutcomeForUMA> future1;
  base::test::TestFuture<bool, TrustedVaultCanEnrollOutcomeForUMA> future2;
  handler()->CanEnroll(future1.GetCallback());
  handler()->CanEnroll(future2.GetCallback());

  ASSERT_FALSE(download_callback.is_null());
  std::move(download_callback)
      .Run(TrustedVaultDownloadPasswordPublicKeyStatus::kSuccess,
           kValidPublicKey);

  EXPECT_TRUE(future1.Get<0>());
  EXPECT_EQ(future1.Get<1>(), TrustedVaultCanEnrollOutcomeForUMA::kEligible);
  EXPECT_TRUE(future2.Get<0>());
  EXPECT_EQ(future2.Get<1>(), TrustedVaultCanEnrollOutcomeForUMA::kEligible);
}

TEST_F(ChromeSyncEnrollmentHandlerTest,
       ShouldReturnFalseIfDownloadFailsOrInvalid) {
  StoreConstantKeys();

  // No Gaia password public key on the server.
  {
    TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
        download_callback;
    EXPECT_CALL(*connection(),
                DownloadGaiaPasswordPublicKey(Eq(account_info()), _))
        .WillOnce(
            [&download_callback](
                const CoreAccountInfo&,
                TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
                    callback) {
              download_callback = std::move(callback);
              return std::make_unique<TrustedVaultConnection::Request>();
            });

    base::test::TestFuture<bool, TrustedVaultCanEnrollOutcomeForUMA> future;
    handler()->CanEnroll(future.GetCallback());
    ASSERT_FALSE(download_callback.is_null());
    std::move(download_callback)
        .Run(TrustedVaultDownloadPasswordPublicKeyStatus::kNoPublicKey,
             std::vector<uint8_t>());
    EXPECT_FALSE(future.Get<0>());
    EXPECT_EQ(future.Get<1>(),
              TrustedVaultCanEnrollOutcomeForUMA::kNoEligibleGaiaPassword);
  }

  // Network error.
  {
    TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
        download_callback;
    EXPECT_CALL(*connection(),
                DownloadGaiaPasswordPublicKey(Eq(account_info()), _))
        .WillOnce(
            [&download_callback](
                const CoreAccountInfo&,
                TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
                    callback) {
              download_callback = std::move(callback);
              return std::make_unique<TrustedVaultConnection::Request>();
            });

    base::test::TestFuture<bool, TrustedVaultCanEnrollOutcomeForUMA> future;
    handler()->CanEnroll(future.GetCallback());
    ASSERT_FALSE(download_callback.is_null());
    std::move(download_callback)
        .Run(TrustedVaultDownloadPasswordPublicKeyStatus::kNetworkError,
             std::vector<uint8_t>());
    EXPECT_FALSE(future.Get<0>());
    EXPECT_EQ(future.Get<1>(),
              TrustedVaultCanEnrollOutcomeForUMA::kGaiaPasswordFetchFailed);
  }

  // Invalid public key bytes.
  {
    TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
        download_callback;
    EXPECT_CALL(*connection(),
                DownloadGaiaPasswordPublicKey(Eq(account_info()), _))
        .WillOnce(
            [&download_callback](
                const CoreAccountInfo&,
                TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
                    callback) {
              download_callback = std::move(callback);
              return std::make_unique<TrustedVaultConnection::Request>();
            });

    base::test::TestFuture<bool, TrustedVaultCanEnrollOutcomeForUMA> future;
    handler()->CanEnroll(future.GetCallback());
    ASSERT_FALSE(download_callback.is_null());
    std::move(download_callback)
        .Run(TrustedVaultDownloadPasswordPublicKeyStatus::kSuccess,
             /*invalid_public_key=*/{1, 2, 3});
    EXPECT_FALSE(future.Get<0>());
    EXPECT_EQ(future.Get<1>(),
              TrustedVaultCanEnrollOutcomeForUMA::kGaiaPasswordFetchFailed);
  }
}

TEST_F(ChromeSyncEnrollmentHandlerTest, ShouldAbortCanEnrollOnDestruction) {
  StoreConstantKeys();

  EXPECT_CALL(*connection(),
              DownloadGaiaPasswordPublicKey(Eq(account_info()), _))
      .WillOnce([](const CoreAccountInfo&,
                   TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
                       callback) {
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  base::MockCallback<ChromeSyncEnrollmentHandler::CanEnrollCallback> cb;
  EXPECT_CALL(cb, Run).Times(0);
  handler()->CanEnroll(cb.Get());

  ResetHandler();
}

TEST_F(ChromeSyncEnrollmentHandlerTest, ShouldEnroll) {
  const int kConstantKeyVersion = 5;
  const int kNewKeyVersion = 6;
  const std::vector<uint8_t> kValidPublicKey =
      SecureBoxKeyPair::GenerateRandom()->public_key().ExportToBytes();

  StoreConstantKeys(kConstantKeyVersion);

  TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
      download_callback;
  EXPECT_CALL(*connection(),
              DownloadGaiaPasswordPublicKey(Eq(account_info()), _))
      .WillOnce(
          [&](const CoreAccountInfo&,
              TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
                  callback) {
            download_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      register_callback;
  EXPECT_CALL(
      *connection(),
      RegisterAuthenticationFactor(
          Eq(account_info()), Eq(SecurityDomainId::kChromeSync),
          MatchTrustedVaultKeyAndVersions(ElementsAre(TrustedVaultKeyAndVersion(
              GetConstantTrustedVaultKey(), kConstantKeyVersion))),
          PublicKeyWhenExportedEq(kValidPublicKey),
          Eq(AuthenticationFactorTypeAndRegistrationParams(GaiaPassword())), _))
      .WillOnce([&](const CoreAccountInfo&, SecurityDomainId,
                    const MemberKeysSource&, const SecureBoxPublicKey&,
                    AuthenticationFactorTypeAndRegistrationParams,
                    TrustedVaultConnection::RegisterAuthenticationFactorCallback
                        callback) {
        register_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  std::vector<uint8_t> rotated_key;
  TrustedVaultConnection::RotateSharedKeyCallback rotate_callback;
  EXPECT_CALL(
      *connection(),
      RotateSharedKey(Eq(account_info()), Eq(SecurityDomainId::kChromeSync),
                      Eq(TrustedVaultKeyAndVersion(GetConstantTrustedVaultKey(),
                                                   kConstantKeyVersion)),
                      SizeIs(32), _))
      .WillOnce([&](const CoreAccountInfo&, SecurityDomainId,
                    const TrustedVaultKeyAndVersion&,
                    const std::vector<uint8_t>& new_shared_key,
                    TrustedVaultConnection::RotateSharedKeyCallback callback) {
        rotated_key = new_shared_key;
        rotate_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  base::test::TestFuture<const std::vector<uint8_t>&, int,
                         TrustedVaultEnrollOutcomeForUMA>
      future;
  handler()->Enroll(future.GetCallback());

  ASSERT_FALSE(download_callback.is_null());
  std::move(download_callback)
      .Run(TrustedVaultDownloadPasswordPublicKeyStatus::kSuccess,
           kValidPublicKey);

  ASSERT_FALSE(register_callback.is_null());
  std::move(register_callback)
      .Run(TrustedVaultRegistrationStatus::kSuccess, kConstantKeyVersion);

  ASSERT_FALSE(rotate_callback.is_null());
  std::move(rotate_callback)
      .Run(TrustedVaultKeyRotationStatus::kSuccess, kNewKeyVersion);

  EXPECT_EQ(future.Get<0>(), rotated_key);
  EXPECT_EQ(future.Get<1>(), kNewKeyVersion);
  EXPECT_EQ(future.Get<2>(), TrustedVaultEnrollOutcomeForUMA::kSuccess);
}

TEST_F(ChromeSyncEnrollmentHandlerTest,
       ShouldRejectConcurrentEnrollWhileInFlight) {
  const int kConstantKeyVersion = 5;
  const int kNewKeyVersion = 6;
  const std::vector<uint8_t> kValidPublicKey =
      SecureBoxKeyPair::GenerateRandom()->public_key().ExportToBytes();

  StoreConstantKeys(kConstantKeyVersion);

  TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
      download_callback;
  EXPECT_CALL(*connection(),
              DownloadGaiaPasswordPublicKey(Eq(account_info()), _))
      .WillOnce(
          [&](const CoreAccountInfo&,
              TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
                  callback) {
            download_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      register_callback;
  EXPECT_CALL(*connection(), RegisterAuthenticationFactor)
      .WillOnce([&](const CoreAccountInfo&, SecurityDomainId,
                    const MemberKeysSource&, const SecureBoxPublicKey&,
                    AuthenticationFactorTypeAndRegistrationParams,
                    TrustedVaultConnection::RegisterAuthenticationFactorCallback
                        callback) {
        register_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  TrustedVaultConnection::RotateSharedKeyCallback rotate_callback;
  EXPECT_CALL(*connection(), RotateSharedKey)
      .WillOnce([&](const CoreAccountInfo&, SecurityDomainId,
                    const TrustedVaultKeyAndVersion&,
                    const std::vector<uint8_t>&,
                    TrustedVaultConnection::RotateSharedKeyCallback callback) {
        rotate_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  base::test::TestFuture<const std::vector<uint8_t>&, int,
                         TrustedVaultEnrollOutcomeForUMA>
      future1;
  base::test::TestFuture<const std::vector<uint8_t>&, int,
                         TrustedVaultEnrollOutcomeForUMA>
      future2;
  handler()->Enroll(future1.GetCallback());
  handler()->Enroll(future2.GetCallback());

  // The second call should fail immediately without disrupting the first.
  EXPECT_THAT(future2.Get<0>(), IsEmpty());
  EXPECT_EQ(future2.Get<1>(), -1);
  EXPECT_EQ(future2.Get<2>(),
            TrustedVaultEnrollOutcomeForUMA::kAlreadyInProgress);

  ASSERT_FALSE(download_callback.is_null());
  std::move(download_callback)
      .Run(TrustedVaultDownloadPasswordPublicKeyStatus::kSuccess,
           kValidPublicKey);
  ASSERT_FALSE(register_callback.is_null());
  std::move(register_callback)
      .Run(TrustedVaultRegistrationStatus::kSuccess, kConstantKeyVersion);
  ASSERT_FALSE(rotate_callback.is_null());
  std::move(rotate_callback)
      .Run(TrustedVaultKeyRotationStatus::kSuccess, kNewKeyVersion);

  EXPECT_THAT(future1.Get<0>(), SizeIs(32));
  EXPECT_EQ(future1.Get<1>(), kNewKeyVersion);
  EXPECT_EQ(future1.Get<2>(), TrustedVaultEnrollOutcomeForUMA::kSuccess);
}

TEST_F(ChromeSyncEnrollmentHandlerTest,
       ShouldFailEnrollIfLocalPrerequisitesNotMetOrLostMidFlight) {
  const int kConstantKeyVersion = 5;
  const std::vector<uint8_t> kValidPublicKey =
      SecureBoxKeyPair::GenerateRandom()->public_key().ExportToBytes();

  // No keys stored initially -> kNoLocalKeys.
  {
    base::test::TestFuture<const std::vector<uint8_t>&, int,
                           TrustedVaultEnrollOutcomeForUMA>
        future;
    handler()->Enroll(future.GetCallback());
    EXPECT_THAT(future.Get<0>(), IsEmpty());
    EXPECT_EQ(future.Get<1>(), -1);
    EXPECT_EQ(future.Get<2>(), TrustedVaultEnrollOutcomeForUMA::kNoLocalKeys);
  }

  StoreConstantKeys(kConstantKeyVersion);

  TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
      download_callback;
  EXPECT_CALL(*connection(),
              DownloadGaiaPasswordPublicKey(Eq(account_info()), _))
      .WillOnce(
          [&](const CoreAccountInfo&,
              TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
                  callback) {
            download_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  base::test::TestFuture<const std::vector<uint8_t>&, int,
                         TrustedVaultEnrollOutcomeForUMA>
      future;
  handler()->Enroll(future.GetCallback());
  ASSERT_FALSE(download_callback.is_null());

  // Simulate a non-constant key being stored while the download is in flight.
  storage()->SetVaultKeys(account_info().gaia, SecurityDomainId::kChromeSync,
                          {{1, 2, 3}}, /*last_key_version=*/2);

  EXPECT_CALL(*connection(), RegisterAuthenticationFactor).Times(0);
  std::move(download_callback)
      .Run(TrustedVaultDownloadPasswordPublicKeyStatus::kSuccess,
           kValidPublicKey);

  EXPECT_THAT(future.Get<0>(), IsEmpty());
  EXPECT_EQ(future.Get<1>(), -1);
  EXPECT_EQ(future.Get<2>(), TrustedVaultEnrollOutcomeForUMA::kAlreadyEnrolled);
}

TEST_F(ChromeSyncEnrollmentHandlerTest,
       ShouldFailEnrollOnNoEligibleGaiaPasswordOrLocalDataObsolete) {
  const int kConstantKeyVersion = 5;
  const std::vector<uint8_t> kValidPublicKey =
      SecureBoxKeyPair::GenerateRandom()->public_key().ExportToBytes();
  StoreConstantKeys(kConstantKeyVersion);

  // Server responds with kNoPublicKey -> kNoEligibleGaiaPassword.
  {
    TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
        download_callback;
    EXPECT_CALL(*connection(),
                DownloadGaiaPasswordPublicKey(Eq(account_info()), _))
        .WillOnce(
            [&](const CoreAccountInfo&,
                TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
                    callback) {
              download_callback = std::move(callback);
              return std::make_unique<TrustedVaultConnection::Request>();
            });

    base::test::TestFuture<const std::vector<uint8_t>&, int,
                           TrustedVaultEnrollOutcomeForUMA>
        future;
    handler()->Enroll(future.GetCallback());
    ASSERT_FALSE(download_callback.is_null());
    std::move(download_callback)
        .Run(TrustedVaultDownloadPasswordPublicKeyStatus::kNoPublicKey,
             std::vector<uint8_t>());

    EXPECT_THAT(future.Get<0>(), IsEmpty());
    EXPECT_EQ(future.Get<1>(), -1);
    EXPECT_EQ(future.Get<2>(),
              TrustedVaultEnrollOutcomeForUMA::kNoEligibleGaiaPassword);
  }

  // RegisterAuthenticationFactor responds with kLocalDataObsolete.
  {
    TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
        download_callback;
    EXPECT_CALL(*connection(),
                DownloadGaiaPasswordPublicKey(Eq(account_info()), _))
        .WillOnce(
            [&](const CoreAccountInfo&,
                TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
                    callback) {
              download_callback = std::move(callback);
              return std::make_unique<TrustedVaultConnection::Request>();
            });
    TrustedVaultConnection::RegisterAuthenticationFactorCallback
        register_callback;
    EXPECT_CALL(*connection(), RegisterAuthenticationFactor)
        .WillOnce(
            [&](const CoreAccountInfo&, SecurityDomainId,
                const MemberKeysSource&, const SecureBoxPublicKey&,
                AuthenticationFactorTypeAndRegistrationParams,
                TrustedVaultConnection::RegisterAuthenticationFactorCallback
                    callback) {
              register_callback = std::move(callback);
              return std::make_unique<TrustedVaultConnection::Request>();
            });

    base::test::TestFuture<const std::vector<uint8_t>&, int,
                           TrustedVaultEnrollOutcomeForUMA>
        future;
    handler()->Enroll(future.GetCallback());
    ASSERT_FALSE(download_callback.is_null());
    std::move(download_callback)
        .Run(TrustedVaultDownloadPasswordPublicKeyStatus::kSuccess,
             kValidPublicKey);
    ASSERT_FALSE(register_callback.is_null());
    std::move(register_callback)
        .Run(TrustedVaultRegistrationStatus::kLocalDataObsolete, 0);

    EXPECT_THAT(future.Get<0>(), IsEmpty());
    EXPECT_EQ(future.Get<1>(), -1);
    EXPECT_EQ(future.Get<2>(),
              TrustedVaultEnrollOutcomeForUMA::kLocalDataObsolete);
  }

  // RotateSharedKey responds with kLocalDataObsolete.
  {
    TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
        download_callback;
    EXPECT_CALL(*connection(),
                DownloadGaiaPasswordPublicKey(Eq(account_info()), _))
        .WillOnce(
            [&](const CoreAccountInfo&,
                TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
                    callback) {
              download_callback = std::move(callback);
              return std::make_unique<TrustedVaultConnection::Request>();
            });
    TrustedVaultConnection::RegisterAuthenticationFactorCallback
        register_callback;
    EXPECT_CALL(*connection(), RegisterAuthenticationFactor)
        .WillOnce(
            [&](const CoreAccountInfo&, SecurityDomainId,
                const MemberKeysSource&, const SecureBoxPublicKey&,
                AuthenticationFactorTypeAndRegistrationParams,
                TrustedVaultConnection::RegisterAuthenticationFactorCallback
                    callback) {
              register_callback = std::move(callback);
              return std::make_unique<TrustedVaultConnection::Request>();
            });
    TrustedVaultConnection::RotateSharedKeyCallback rotate_callback;
    EXPECT_CALL(*connection(), RotateSharedKey)
        .WillOnce(
            [&](const CoreAccountInfo&, SecurityDomainId,
                const TrustedVaultKeyAndVersion&, const std::vector<uint8_t>&,
                TrustedVaultConnection::RotateSharedKeyCallback callback) {
              rotate_callback = std::move(callback);
              return std::make_unique<TrustedVaultConnection::Request>();
            });

    base::test::TestFuture<const std::vector<uint8_t>&, int,
                           TrustedVaultEnrollOutcomeForUMA>
        future;
    handler()->Enroll(future.GetCallback());
    ASSERT_FALSE(download_callback.is_null());
    std::move(download_callback)
        .Run(TrustedVaultDownloadPasswordPublicKeyStatus::kSuccess,
             kValidPublicKey);
    ASSERT_FALSE(register_callback.is_null());
    std::move(register_callback)
        .Run(TrustedVaultRegistrationStatus::kSuccess, kConstantKeyVersion);
    ASSERT_FALSE(rotate_callback.is_null());
    std::move(rotate_callback)
        .Run(TrustedVaultKeyRotationStatus::kLocalDataObsolete, 0);

    EXPECT_THAT(future.Get<0>(), IsEmpty());
    EXPECT_EQ(future.Get<1>(), -1);
    EXPECT_EQ(future.Get<2>(),
              TrustedVaultEnrollOutcomeForUMA::kLocalDataObsolete);
  }
}

TEST_F(ChromeSyncEnrollmentHandlerTest,
       ShouldRetryKeyRotationOnMembershipMismatchAndSucceed) {
  const int kConstantKeyVersion = 5;
  const int kNewKeyVersion = 6;
  const std::vector<uint8_t> kValidPublicKey =
      SecureBoxKeyPair::GenerateRandom()->public_key().ExportToBytes();

  StoreConstantKeys(kConstantKeyVersion);

  TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
      download_callback;
  EXPECT_CALL(*connection(),
              DownloadGaiaPasswordPublicKey(Eq(account_info()), _))
      .WillOnce(
          [&](const CoreAccountInfo&,
              TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
                  callback) {
            download_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      register_callback;
  EXPECT_CALL(*connection(), RegisterAuthenticationFactor)
      .WillOnce([&](const CoreAccountInfo&, SecurityDomainId,
                    const MemberKeysSource&, const SecureBoxPublicKey&,
                    AuthenticationFactorTypeAndRegistrationParams,
                    TrustedVaultConnection::RegisterAuthenticationFactorCallback
                        callback) {
        register_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  // First attempt returns kMembershipMismatch; second attempt succeeds.
  TrustedVaultConnection::RotateSharedKeyCallback rotate_callback1;
  TrustedVaultConnection::RotateSharedKeyCallback rotate_callback2;
  EXPECT_CALL(*connection(), RotateSharedKey)
      .WillOnce([&](const CoreAccountInfo&, SecurityDomainId,
                    const TrustedVaultKeyAndVersion&,
                    const std::vector<uint8_t>&,
                    TrustedVaultConnection::RotateSharedKeyCallback callback) {
        rotate_callback1 = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      })
      .WillOnce([&](const CoreAccountInfo&, SecurityDomainId,
                    const TrustedVaultKeyAndVersion&,
                    const std::vector<uint8_t>&,
                    TrustedVaultConnection::RotateSharedKeyCallback callback) {
        rotate_callback2 = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  base::test::TestFuture<const std::vector<uint8_t>&, int,
                         TrustedVaultEnrollOutcomeForUMA>
      future;
  handler()->Enroll(future.GetCallback());

  ASSERT_FALSE(download_callback.is_null());
  std::move(download_callback)
      .Run(TrustedVaultDownloadPasswordPublicKeyStatus::kSuccess,
           kValidPublicKey);

  ASSERT_FALSE(register_callback.is_null());
  std::move(register_callback)
      .Run(TrustedVaultRegistrationStatus::kSuccess, kConstantKeyVersion);

  ASSERT_FALSE(rotate_callback1.is_null());
  std::move(rotate_callback1)
      .Run(TrustedVaultKeyRotationStatus::kMembershipMismatch, 0);
  EXPECT_FALSE(future.IsReady());

  ASSERT_FALSE(rotate_callback2.is_null());
  std::move(rotate_callback2)
      .Run(TrustedVaultKeyRotationStatus::kSuccess, kNewKeyVersion);

  EXPECT_THAT(future.Get<0>(), SizeIs(32));
  EXPECT_EQ(future.Get<1>(), kNewKeyVersion);
  EXPECT_EQ(future.Get<2>(),
            TrustedVaultEnrollOutcomeForUMA::kSuccessAfterRetry);
}

TEST_F(ChromeSyncEnrollmentHandlerTest,
       ShouldFailWhenKeyRotationRetryAlsoFails) {
  const int kConstantKeyVersion = 5;
  const std::vector<uint8_t> kValidPublicKey =
      SecureBoxKeyPair::GenerateRandom()->public_key().ExportToBytes();

  StoreConstantKeys(kConstantKeyVersion);

  TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
      download_callback;
  EXPECT_CALL(*connection(),
              DownloadGaiaPasswordPublicKey(Eq(account_info()), _))
      .WillOnce(
          [&](const CoreAccountInfo&,
              TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
                  callback) {
            download_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      register_callback;
  EXPECT_CALL(*connection(), RegisterAuthenticationFactor)
      .WillOnce([&](const CoreAccountInfo&, SecurityDomainId,
                    const MemberKeysSource&, const SecureBoxPublicKey&,
                    AuthenticationFactorTypeAndRegistrationParams,
                    TrustedVaultConnection::RegisterAuthenticationFactorCallback
                        callback) {
        register_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  TrustedVaultConnection::RotateSharedKeyCallback rotate_callback1;
  TrustedVaultConnection::RotateSharedKeyCallback rotate_callback2;
  EXPECT_CALL(*connection(), RotateSharedKey)
      .WillOnce([&](const CoreAccountInfo&, SecurityDomainId,
                    const TrustedVaultKeyAndVersion&,
                    const std::vector<uint8_t>&,
                    TrustedVaultConnection::RotateSharedKeyCallback callback) {
        rotate_callback1 = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      })
      .WillOnce([&](const CoreAccountInfo&, SecurityDomainId,
                    const TrustedVaultKeyAndVersion&,
                    const std::vector<uint8_t>&,
                    TrustedVaultConnection::RotateSharedKeyCallback callback) {
        rotate_callback2 = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  base::test::TestFuture<const std::vector<uint8_t>&, int,
                         TrustedVaultEnrollOutcomeForUMA>
      future;
  handler()->Enroll(future.GetCallback());

  ASSERT_FALSE(download_callback.is_null());
  std::move(download_callback)
      .Run(TrustedVaultDownloadPasswordPublicKeyStatus::kSuccess,
           kValidPublicKey);

  ASSERT_FALSE(register_callback.is_null());
  std::move(register_callback)
      .Run(TrustedVaultRegistrationStatus::kSuccess, kConstantKeyVersion);

  ASSERT_FALSE(rotate_callback1.is_null());
  std::move(rotate_callback1)
      .Run(TrustedVaultKeyRotationStatus::kMembershipMismatch, 0);
  EXPECT_FALSE(future.IsReady());

  ASSERT_FALSE(rotate_callback2.is_null());
  std::move(rotate_callback2)
      .Run(TrustedVaultKeyRotationStatus::kMembershipMismatch, 0);

  EXPECT_THAT(future.Get<0>(), IsEmpty());
  EXPECT_EQ(future.Get<1>(), -1);
  EXPECT_EQ(future.Get<2>(),
            TrustedVaultEnrollOutcomeForUMA::kKeyRotationFailed);
}

TEST_F(ChromeSyncEnrollmentHandlerTest, ShouldFailEnrollWhenStepsFail) {
  const int kConstantKeyVersion = 5;
  const std::vector<uint8_t> kValidPublicKey =
      SecureBoxKeyPair::GenerateRandom()->public_key().ExportToBytes();
  StoreConstantKeys(kConstantKeyVersion);

  // DownloadGaiaPasswordPublicKey fails.
  {
    TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
        download_callback;
    EXPECT_CALL(*connection(),
                DownloadGaiaPasswordPublicKey(Eq(account_info()), _))
        .WillOnce(
            [&](const CoreAccountInfo&,
                TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
                    callback) {
              download_callback = std::move(callback);
              return std::make_unique<TrustedVaultConnection::Request>();
            });

    base::test::TestFuture<const std::vector<uint8_t>&, int,
                           TrustedVaultEnrollOutcomeForUMA>
        future;
    handler()->Enroll(future.GetCallback());
    ASSERT_FALSE(download_callback.is_null());
    std::move(download_callback)
        .Run(TrustedVaultDownloadPasswordPublicKeyStatus::kNetworkError,
             std::vector<uint8_t>());

    EXPECT_THAT(future.Get<0>(), IsEmpty());
    EXPECT_EQ(future.Get<1>(), -1);
    EXPECT_EQ(future.Get<2>(),
              TrustedVaultEnrollOutcomeForUMA::kGaiaPasswordFetchFailed);
  }

  // Invalid public key bytes.
  {
    TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
        download_callback;
    EXPECT_CALL(*connection(),
                DownloadGaiaPasswordPublicKey(Eq(account_info()), _))
        .WillOnce(
            [&](const CoreAccountInfo&,
                TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
                    callback) {
              download_callback = std::move(callback);
              return std::make_unique<TrustedVaultConnection::Request>();
            });

    base::test::TestFuture<const std::vector<uint8_t>&, int,
                           TrustedVaultEnrollOutcomeForUMA>
        future;
    handler()->Enroll(future.GetCallback());
    ASSERT_FALSE(download_callback.is_null());
    std::move(download_callback)
        .Run(TrustedVaultDownloadPasswordPublicKeyStatus::kSuccess,
             /*invalid_public_key=*/{1, 2, 3});

    EXPECT_THAT(future.Get<0>(), IsEmpty());
    EXPECT_EQ(future.Get<1>(), -1);
    EXPECT_EQ(future.Get<2>(),
              TrustedVaultEnrollOutcomeForUMA::kGaiaPasswordFetchFailed);
  }

  // RegisterAuthenticationFactor fails.
  {
    TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
        download_callback;
    EXPECT_CALL(*connection(),
                DownloadGaiaPasswordPublicKey(Eq(account_info()), _))
        .WillOnce(
            [&](const CoreAccountInfo&,
                TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
                    callback) {
              download_callback = std::move(callback);
              return std::make_unique<TrustedVaultConnection::Request>();
            });
    TrustedVaultConnection::RegisterAuthenticationFactorCallback
        register_callback;
    EXPECT_CALL(*connection(), RegisterAuthenticationFactor)
        .WillOnce(
            [&](const CoreAccountInfo&, SecurityDomainId,
                const MemberKeysSource&, const SecureBoxPublicKey&,
                AuthenticationFactorTypeAndRegistrationParams,
                TrustedVaultConnection::RegisterAuthenticationFactorCallback
                    callback) {
              register_callback = std::move(callback);
              return std::make_unique<TrustedVaultConnection::Request>();
            });

    base::test::TestFuture<const std::vector<uint8_t>&, int,
                           TrustedVaultEnrollOutcomeForUMA>
        future;
    handler()->Enroll(future.GetCallback());
    ASSERT_FALSE(download_callback.is_null());
    std::move(download_callback)
        .Run(TrustedVaultDownloadPasswordPublicKeyStatus::kSuccess,
             kValidPublicKey);
    ASSERT_FALSE(register_callback.is_null());
    std::move(register_callback)
        .Run(TrustedVaultRegistrationStatus::kNetworkError, 0);

    EXPECT_THAT(future.Get<0>(), IsEmpty());
    EXPECT_EQ(future.Get<1>(), -1);
    EXPECT_EQ(future.Get<2>(),
              TrustedVaultEnrollOutcomeForUMA::kGaiaPasswordRegistrationFailed);
  }

  // RotateSharedKey fails with a non-retryable error.
  {
    TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
        download_callback;
    EXPECT_CALL(*connection(),
                DownloadGaiaPasswordPublicKey(Eq(account_info()), _))
        .WillOnce(
            [&](const CoreAccountInfo&,
                TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
                    callback) {
              download_callback = std::move(callback);
              return std::make_unique<TrustedVaultConnection::Request>();
            });
    TrustedVaultConnection::RegisterAuthenticationFactorCallback
        register_callback;
    EXPECT_CALL(*connection(), RegisterAuthenticationFactor)
        .WillOnce(
            [&](const CoreAccountInfo&, SecurityDomainId,
                const MemberKeysSource&, const SecureBoxPublicKey&,
                AuthenticationFactorTypeAndRegistrationParams,
                TrustedVaultConnection::RegisterAuthenticationFactorCallback
                    callback) {
              register_callback = std::move(callback);
              return std::make_unique<TrustedVaultConnection::Request>();
            });
    TrustedVaultConnection::RotateSharedKeyCallback rotate_callback;
    EXPECT_CALL(*connection(), RotateSharedKey)
        .WillOnce(
            [&](const CoreAccountInfo&, SecurityDomainId,
                const TrustedVaultKeyAndVersion&, const std::vector<uint8_t>&,
                TrustedVaultConnection::RotateSharedKeyCallback callback) {
              rotate_callback = std::move(callback);
              return std::make_unique<TrustedVaultConnection::Request>();
            });

    base::test::TestFuture<const std::vector<uint8_t>&, int,
                           TrustedVaultEnrollOutcomeForUMA>
        future;
    handler()->Enroll(future.GetCallback());
    ASSERT_FALSE(download_callback.is_null());
    std::move(download_callback)
        .Run(TrustedVaultDownloadPasswordPublicKeyStatus::kSuccess,
             kValidPublicKey);
    ASSERT_FALSE(register_callback.is_null());
    std::move(register_callback)
        .Run(TrustedVaultRegistrationStatus::kSuccess, kConstantKeyVersion);
    ASSERT_FALSE(rotate_callback.is_null());
    std::move(rotate_callback)
        .Run(TrustedVaultKeyRotationStatus::kNetworkError, 0);

    EXPECT_THAT(future.Get<0>(), IsEmpty());
    EXPECT_EQ(future.Get<1>(), -1);
    EXPECT_EQ(future.Get<2>(),
              TrustedVaultEnrollOutcomeForUMA::kKeyRotationFailed);
  }
}

TEST_F(ChromeSyncEnrollmentHandlerTest, ShouldAbortEnrollOnDestruction) {
  StoreConstantKeys();

  EXPECT_CALL(*connection(),
              DownloadGaiaPasswordPublicKey(Eq(account_info()), _))
      .WillOnce([](const CoreAccountInfo&,
                   TrustedVaultConnection::DownloadGaiaPasswordPublicKeyCallback
                       callback) {
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  base::MockCallback<ChromeSyncEnrollmentHandler::EnrollCallback> cb;
  EXPECT_CALL(cb, Run).Times(0);
  handler()->Enroll(cb.Get());

  ResetHandler();
}

}  // namespace

}  // namespace trusted_vault
