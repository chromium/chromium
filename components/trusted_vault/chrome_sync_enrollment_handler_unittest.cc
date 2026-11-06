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
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

namespace {

using testing::_;
using testing::Eq;
using testing::NiceMock;

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

  void StoreConstantKeys() {
    storage_->SetVaultKeys(account_info_.gaia, SecurityDomainId::kChromeSync,
                           {GetConstantTrustedVaultKey()},
                           /*last_key_version=*/1);
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

  base::test::TestFuture<bool> future;
  handler()->CanEnroll(future.GetCallback());
  ASSERT_FALSE(download_callback.is_null());
  std::move(download_callback)
      .Run(TrustedVaultDownloadPasswordPublicKeyStatus::kSuccess,
           kValidPublicKey);
  EXPECT_TRUE(future.Get());
}

TEST_F(ChromeSyncEnrollmentHandlerTest,
       ShouldReturnFalseIfPreconditionsNotMet) {
  EXPECT_CALL(*connection(), DownloadGaiaPasswordPublicKey).Times(0);

  // No keys in storage.
  {
    base::test::TestFuture<bool> future;
    handler()->CanEnroll(future.GetCallback());
    EXPECT_FALSE(future.Get());
  }

  // Non-constant key in storage.
  storage()->SetVaultKeys(account_info().gaia, SecurityDomainId::kChromeSync,
                          {{1, 2, 3}}, /*last_key_version=*/1);
  {
    base::test::TestFuture<bool> future;
    handler()->CanEnroll(future.GetCallback());
    EXPECT_FALSE(future.Get());
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

  base::test::TestFuture<bool> future1;
  base::test::TestFuture<bool> future2;
  handler()->CanEnroll(future1.GetCallback());
  handler()->CanEnroll(future2.GetCallback());

  ASSERT_FALSE(download_callback.is_null());
  std::move(download_callback)
      .Run(TrustedVaultDownloadPasswordPublicKeyStatus::kSuccess,
           kValidPublicKey);

  EXPECT_TRUE(future1.Get());
  EXPECT_TRUE(future2.Get());
}

TEST_F(ChromeSyncEnrollmentHandlerTest,
       ShouldReturnFalseIfDownloadFailsOrInvalid) {
  StoreConstantKeys();

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

    base::test::TestFuture<bool> future;
    handler()->CanEnroll(future.GetCallback());
    ASSERT_FALSE(download_callback.is_null());
    std::move(download_callback)
        .Run(TrustedVaultDownloadPasswordPublicKeyStatus::kNetworkError,
             std::vector<uint8_t>());
    EXPECT_FALSE(future.Get());
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

    base::test::TestFuture<bool> future;
    handler()->CanEnroll(future.GetCallback());
    ASSERT_FALSE(download_callback.is_null());
    std::move(download_callback)
        .Run(TrustedVaultDownloadPasswordPublicKeyStatus::kSuccess,
             /*invalid_public_key=*/{1, 2, 3});
    EXPECT_FALSE(future.Get());
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

}  // namespace

}  // namespace trusted_vault
