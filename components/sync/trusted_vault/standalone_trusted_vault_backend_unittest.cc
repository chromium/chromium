// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/standalone_trusted_vault_backend.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "components/os_crypt/os_crypt.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/trusted_vault/securebox.h"
#include "components/sync/trusted_vault/trusted_vault_connection.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::Mock;
using testing::Ne;
using testing::NotNull;

MATCHER_P(KeyMaterialEq, expected, "") {
  const std::string& key_material = arg.key_material();
  const std::vector<uint8_t> key_material_as_bytes(key_material.begin(),
                                                   key_material.end());
  return key_material_as_bytes == expected;
}

MATCHER_P2(TrustedVaultKeyAndVersionEq, expected_key, expected_version, "") {
  const TrustedVaultKeyAndVersion& key_and_version = arg;
  return key_and_version.key == expected_key &&
         key_and_version.version == expected_version;
}

base::FilePath CreateUniqueTempDir(base::ScopedTempDir* temp_dir) {
  EXPECT_TRUE(temp_dir->CreateUniqueTempDir());
  return temp_dir->GetPath();
}

class MockDelegate : public StandaloneTrustedVaultBackend::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;
  MOCK_METHOD(void, NotifyRecoverabilityDegradedChanged, (), (override));
};

class MockTrustedVaultConnection : public TrustedVaultConnection {
 public:
  MockTrustedVaultConnection() = default;
  ~MockTrustedVaultConnection() override = default;
  MOCK_METHOD(
      std::unique_ptr<Request>,
      RegisterAuthenticationFactor,
      (const CoreAccountInfo& account_info,
       const TrustedVaultKeyAndVersion& last_trusted_vault_key_and_version,
       const SecureBoxPublicKey& authentication_factor_public_key,
       RegisterAuthenticationFactorCallback callback),
      (override));
  MOCK_METHOD(
      std::unique_ptr<Request>,
      DownloadKeys,
      (const CoreAccountInfo& account_info,
       const TrustedVaultKeyAndVersion& last_trusted_vault_key_and_version,
       std::unique_ptr<SecureBoxKeyPair> device_key_pair,
       DownloadKeysCallback callback),
      (override));
};

class StandaloneTrustedVaultBackendTest : public testing::Test {
 public:
  StandaloneTrustedVaultBackendTest()
      : file_path_(
            CreateUniqueTempDir(&temp_dir_)
                .Append(base::FilePath(FILE_PATH_LITERAL("some_file")))) {
    override_features.InitAndEnableFeature(
        switches::kFollowTrustedVaultKeyRotation);

    auto delegate = std::make_unique<testing::NiceMock<MockDelegate>>();
    delegate_ = delegate.get();

    auto connection =
        std::make_unique<testing::NiceMock<MockTrustedVaultConnection>>();
    connection_ = connection.get();

    backend_ = base::MakeRefCounted<StandaloneTrustedVaultBackend>(
        file_path_, std::move(delegate), std::move(connection));
    backend_->SetClockForTesting(&clock_);
    clock_.SetNow(base::Time::Now());
  }

  ~StandaloneTrustedVaultBackendTest() override = default;

  void SetUp() override { OSCryptMocker::SetUp(); }

  void TearDown() override { OSCryptMocker::TearDown(); }

  MockTrustedVaultConnection* connection() { return connection_; }

  base::SimpleTestClock* clock() { return &clock_; }

  StandaloneTrustedVaultBackend* backend() { return backend_.get(); }

  const base::FilePath& file_path() { return file_path_; }

  // Stores |vault_keys| and mimics successful device registration, returns
  // private device key material.
  std::vector<uint8_t> StoreKeysAndMimicDeviceRegistration(
      const std::vector<std::vector<uint8_t>>& vault_keys,
      int last_vault_key_version,
      CoreAccountInfo account_info) {
    DCHECK(!vault_keys.empty());
    backend_->StoreKeys(account_info.gaia, vault_keys, last_vault_key_version);
    TrustedVaultConnection::RegisterAuthenticationFactorCallback
        device_registration_callback;

    EXPECT_CALL(*connection_,
                RegisterAuthenticationFactor(
                    Eq(account_info),
                    TrustedVaultKeyAndVersionEq(vault_keys.back(),
                                                last_vault_key_version),
                    _, _))
        .WillOnce(
            [&](const CoreAccountInfo&, const TrustedVaultKeyAndVersion&,
                const SecureBoxPublicKey& device_public_key,
                TrustedVaultConnection::RegisterAuthenticationFactorCallback
                    callback) {
              device_registration_callback = std::move(callback);
              // Note: TrustedVaultConnection::Request doesn't support
              // cancellation, so these tests don't cover the contract that
              // caller should store Request object until it's completed or need
              // to be cancelled.
              return std::make_unique<TrustedVaultConnection::Request>();
            });
    // Setting the primary account will trigger device registration.
    backend()->SetPrimaryAccount(account_info);
    EXPECT_FALSE(device_registration_callback.is_null());

    // Pretend that the registration completed successfully.
    std::move(device_registration_callback)
        .Run(TrustedVaultRequestStatus::kSuccess);

    // Reset primary account.
    backend()->SetPrimaryAccount(base::nullopt);

    std::string device_private_key_material =
        backend_->GetDeviceRegistrationInfoForTesting(account_info.gaia)
            .private_key_material();
    return std::vector<uint8_t>(device_private_key_material.begin(),
                                device_private_key_material.end());
  }

 private:
  base::test::ScopedFeatureList override_features;

  base::ScopedTempDir temp_dir_;
  const base::FilePath file_path_;
  testing::NiceMock<MockDelegate>* delegate_;
  testing::NiceMock<MockTrustedVaultConnection>* connection_;
  base::SimpleTestClock clock_;
  scoped_refptr<StandaloneTrustedVaultBackend> backend_;
};

TEST_F(StandaloneTrustedVaultBackendTest, ShouldFetchEmptyKeys) {
  CoreAccountInfo account_info;
  account_info.gaia = "user";
  // Callback should be called immediately.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/IsEmpty()));
  backend()->FetchKeys(account_info, fetch_keys_callback.Get());
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldReadAndFetchNonEmptyKeys) {
  CoreAccountInfo account_info_1;
  account_info_1.gaia = "user1";
  CoreAccountInfo account_info_2;
  account_info_2.gaia = "user2";

  const std::vector<uint8_t> kKey1 = {0, 1, 2, 3, 4};
  const std::vector<uint8_t> kKey2 = {1, 2, 3, 4};
  const std::vector<uint8_t> kKey3 = {2, 3, 4};

  sync_pb::LocalTrustedVault initial_data;
  sync_pb::LocalTrustedVaultPerUser* user_data1 = initial_data.add_user();
  sync_pb::LocalTrustedVaultPerUser* user_data2 = initial_data.add_user();
  user_data1->set_gaia_id(account_info_1.gaia);
  user_data2->set_gaia_id(account_info_2.gaia);
  user_data1->add_vault_key()->set_key_material(kKey1.data(), kKey1.size());
  user_data2->add_vault_key()->set_key_material(kKey2.data(), kKey2.size());
  user_data2->add_vault_key()->set_key_material(kKey3.data(), kKey3.size());

  std::string encrypted_data;
  ASSERT_TRUE(OSCrypt::EncryptString(initial_data.SerializeAsString(),
                                     &encrypted_data));
  ASSERT_NE(-1, base::WriteFile(file_path(), encrypted_data.c_str(),
                                encrypted_data.size()));

  backend()->ReadDataFromDisk();

  // Keys should be fetched immediately for both accounts.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/ElementsAre(kKey1)));
  backend()->FetchKeys(account_info_1, fetch_keys_callback.Get());
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/ElementsAre(kKey2, kKey3)));
  backend()->FetchKeys(account_info_2, fetch_keys_callback.Get());
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldStoreKeys) {
  const std::string kGaiaId1 = "user1";
  const std::string kGaiaId2 = "user2";
  const std::vector<uint8_t> kKey1 = {0, 1, 2, 3, 4};
  const std::vector<uint8_t> kKey2 = {1, 2, 3, 4};
  const std::vector<uint8_t> kKey3 = {2, 3, 4};
  const std::vector<uint8_t> kKey4 = {3, 4};

  backend()->StoreKeys(kGaiaId1, {kKey1}, /*last_key_version=*/7);
  backend()->StoreKeys(kGaiaId2, {kKey2}, /*last_key_version=*/8);
  // Keys for |kGaiaId2| overridden, so |kKey2| should be lost.
  backend()->StoreKeys(kGaiaId2, {kKey3, kKey4}, /*last_key_version=*/9);

  // Read the file from disk.
  std::string ciphertext;
  std::string decrypted_content;
  sync_pb::LocalTrustedVault proto;
  EXPECT_TRUE(base::ReadFileToString(file_path(), &ciphertext));
  EXPECT_THAT(ciphertext, Ne(""));
  EXPECT_TRUE(OSCrypt::DecryptString(ciphertext, &decrypted_content));
  EXPECT_TRUE(proto.ParseFromString(decrypted_content));
  ASSERT_THAT(proto.user_size(), Eq(2));
  EXPECT_THAT(proto.user(0).vault_key(), ElementsAre(KeyMaterialEq(kKey1)));
  EXPECT_THAT(proto.user(0).last_vault_key_version(), Eq(7));
  EXPECT_THAT(proto.user(1).vault_key(),
              ElementsAre(KeyMaterialEq(kKey3), KeyMaterialEq(kKey4)));
  EXPECT_THAT(proto.user(1).last_vault_key_version(), Eq(9));
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldFetchPreviouslyStoredKeys) {
  CoreAccountInfo account_info_1;
  account_info_1.gaia = "user1";
  CoreAccountInfo account_info_2;
  account_info_2.gaia = "user2";

  const std::vector<uint8_t> kKey1 = {0, 1, 2, 3, 4};
  const std::vector<uint8_t> kKey2 = {1, 2, 3, 4};
  const std::vector<uint8_t> kKey3 = {2, 3, 4};

  backend()->StoreKeys(account_info_1.gaia, {kKey1}, /*last_key_version=*/0);
  backend()->StoreKeys(account_info_2.gaia, {kKey2, kKey3},
                       /*last_key_version=*/1);

  // Instantiate a second backend to read the file.
  auto other_backend = base::MakeRefCounted<StandaloneTrustedVaultBackend>(
      file_path(), std::make_unique<testing::NiceMock<MockDelegate>>(),
      std::make_unique<testing::NiceMock<MockTrustedVaultConnection>>());
  other_backend->ReadDataFromDisk();

  // Keys should be fetched immediately for both accounts.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/ElementsAre(kKey1)));
  other_backend->FetchKeys(account_info_1, fetch_keys_callback.Get());
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/ElementsAre(kKey2, kKey3)));
  other_backend->FetchKeys(account_info_2, fetch_keys_callback.Get());
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldRemoveAllStoredKeys) {
  CoreAccountInfo account_info_1;
  account_info_1.gaia = "user1";
  CoreAccountInfo account_info_2;
  account_info_2.gaia = "user2";

  const std::vector<uint8_t> kKey1 = {0, 1, 2, 3, 4};
  const std::vector<uint8_t> kKey2 = {1, 2, 3, 4};
  const std::vector<uint8_t> kKey3 = {2, 3, 4};

  backend()->StoreKeys(account_info_1.gaia, {kKey1}, /*last_key_version=*/0);
  backend()->StoreKeys(account_info_2.gaia, {kKey2, kKey3},
                       /*last_key_version=*/1);

  backend()->RemoveAllStoredKeys();

  // Keys should be removed from both in-memory and disk storages.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/IsEmpty()));
  backend()->FetchKeys(account_info_1, fetch_keys_callback.Get());

  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/IsEmpty()));
  backend()->FetchKeys(account_info_2, fetch_keys_callback.Get());

  EXPECT_FALSE(base::PathExists(file_path()));
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldRegisterDevice) {
  CoreAccountInfo account_info;
  account_info.gaia = "user";

  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 0;

  backend()->StoreKeys(account_info.gaia, {kVaultKey}, kLastKeyVersion);

  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      device_registration_callback;
  std::vector<uint8_t> serialized_public_device_key;
  EXPECT_CALL(
      *connection(),
      RegisterAuthenticationFactor(
          Eq(account_info),
          TrustedVaultKeyAndVersionEq(kVaultKey, kLastKeyVersion), _, _))
      .WillOnce([&](const CoreAccountInfo&, const TrustedVaultKeyAndVersion&,
                    const SecureBoxPublicKey& device_public_key,
                    TrustedVaultConnection::RegisterAuthenticationFactorCallback
                        callback) {
        serialized_public_device_key = device_public_key.ExportToBytes();
        device_registration_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  // Setting the primary account will trigger device registration.
  backend()->SetPrimaryAccount(account_info);
  ASSERT_FALSE(device_registration_callback.is_null());

  // Pretend that the registration completed successfully.
  std::move(device_registration_callback)
      .Run(TrustedVaultRequestStatus::kSuccess);

  // Now the device should be registered.
  sync_pb::LocalDeviceRegistrationInfo registration_info =
      backend()->GetDeviceRegistrationInfoForTesting(account_info.gaia);
  EXPECT_TRUE(registration_info.device_registered());
  EXPECT_TRUE(registration_info.has_private_key_material());

  std::unique_ptr<SecureBoxKeyPair> key_pair =
      SecureBoxKeyPair::CreateByPrivateKeyImport(base::as_bytes(
          base::make_span(registration_info.private_key_material())));
  EXPECT_THAT(key_pair->public_key().ExportToBytes(),
              Eq(serialized_public_device_key));
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldThrottleAndUnthrottleDeviceRegistration) {
  CoreAccountInfo account_info;
  account_info.gaia = "user";

  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 0;

  backend()->StoreKeys(account_info.gaia, {kVaultKey}, kLastKeyVersion);
  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      device_registration_callback;
  ON_CALL(*connection(), RegisterAuthenticationFactor(_, _, _, _))
      .WillByDefault(
          [&](const CoreAccountInfo&, const TrustedVaultKeyAndVersion&,
              const SecureBoxPublicKey&,
              TrustedVaultConnection::RegisterAuthenticationFactorCallback
                  callback) {
            device_registration_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  clock()->SetNow(base::Time::Now());

  EXPECT_CALL(*connection(), RegisterAuthenticationFactor(_, _, _, _));
  // Setting the primary account will trigger device registration.
  backend()->SetPrimaryAccount(account_info);
  ASSERT_FALSE(device_registration_callback.is_null());
  Mock::VerifyAndClearExpectations(connection());

  // Mimic transient failure.
  std::move(device_registration_callback)
      .Run(TrustedVaultRequestStatus::kOtherError);

  // Following request should be throttled.
  device_registration_callback =
      TrustedVaultConnection::RegisterAuthenticationFactorCallback();
  EXPECT_CALL(*connection(), RegisterAuthenticationFactor(_, _, _, _)).Times(0);
  // Reset and set primary account to trigger device registration attempt.
  backend()->SetPrimaryAccount(base::nullopt);
  backend()->SetPrimaryAccount(account_info);
  EXPECT_TRUE(device_registration_callback.is_null());
  Mock::VerifyAndClearExpectations(connection());

  // Advance time to pass the throttling duration and trigger another attempt.
  clock()->Advance(switches::kTrustedVaultServiceThrottlingDuration.Get());

  EXPECT_CALL(*connection(), RegisterAuthenticationFactor(_, _, _, _));
  // Reset and set primary account to trigger device registration attempt.
  backend()->SetPrimaryAccount(base::nullopt);
  backend()->SetPrimaryAccount(account_info);
  EXPECT_FALSE(device_registration_callback.is_null());
}

// System time can be changed to the past and if this situation not handled,
// requests could be throttled for unreasonable amount of time.
TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldUnthrottleDeviceRegistrationWhenTimeSetToPast) {
  CoreAccountInfo account_info;
  account_info.gaia = "user";

  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 0;

  backend()->StoreKeys(account_info.gaia, {kVaultKey}, kLastKeyVersion);
  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      device_registration_callback;
  ON_CALL(*connection(), RegisterAuthenticationFactor(_, _, _, _))
      .WillByDefault(
          [&](const CoreAccountInfo&, const TrustedVaultKeyAndVersion&,
              const SecureBoxPublicKey&,
              TrustedVaultConnection::RegisterAuthenticationFactorCallback
                  callback) {
            device_registration_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  clock()->SetNow(base::Time::Now());

  EXPECT_CALL(*connection(), RegisterAuthenticationFactor(_, _, _, _));
  // Setting the primary account will trigger device registration.
  backend()->SetPrimaryAccount(account_info);
  ASSERT_FALSE(device_registration_callback.is_null());
  Mock::VerifyAndClearExpectations(connection());

  // Mimic transient failure.
  std::move(device_registration_callback)
      .Run(TrustedVaultRequestStatus::kOtherError);

  // Mimic system set to the past.
  clock()->Advance(base::TimeDelta::FromSeconds(-1));

  device_registration_callback =
      TrustedVaultConnection::RegisterAuthenticationFactorCallback();
  EXPECT_CALL(*connection(), RegisterAuthenticationFactor(_, _, _, _));
  // Reset and set primary account to trigger device registration attempt.
  backend()->SetPrimaryAccount(base::nullopt);
  backend()->SetPrimaryAccount(account_info);
  EXPECT_FALSE(device_registration_callback.is_null());
}

// Unless keys marked as stale, FetchKeys() should be completed immediately,
// without keys download attempt.
TEST_F(StandaloneTrustedVaultBackendTest, ShouldFetchKeysImmediately) {
  CoreAccountInfo account_info;
  account_info.gaia = "user";

  const std::vector<std::vector<uint8_t>> kVaultKeys = {{1, 2, 3}};
  const int kLastKeyVersion = 0;

  // Make keys downloading theoretically possible.
  StoreKeysAndMimicDeviceRegistration(kVaultKeys, kLastKeyVersion,
                                      account_info);
  backend()->SetPrimaryAccount(account_info);

  EXPECT_CALL(*connection(), DownloadKeys).Times(0);

  std::vector<std::vector<uint8_t>> fetched_keys;
  // Callback should be called immediately.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/Eq(kVaultKeys)));
  backend()->FetchKeys(account_info, fetch_keys_callback.Get());
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldDownloadKeys) {
  CoreAccountInfo account_info;
  account_info.gaia = "user";

  const std::vector<uint8_t> kInitialVaultKey = {1, 2, 3};
  const int kInitialLastKeyVersion = 0;

  std::vector<uint8_t> private_device_key_material =
      StoreKeysAndMimicDeviceRegistration({kInitialVaultKey},
                                          kInitialLastKeyVersion, account_info);
  EXPECT_TRUE(backend()->MarkKeysAsStale(account_info));
  backend()->SetPrimaryAccount(account_info);

  const std::vector<std::vector<uint8_t>> kNewVaultKeys = {kInitialVaultKey,
                                                           {1, 3, 2}};
  const int kNewLastKeyVersion = 1;

  std::unique_ptr<SecureBoxKeyPair> device_key_pair;
  TrustedVaultConnection::DownloadKeysCallback download_keys_callback;
  EXPECT_CALL(*connection(),
              DownloadKeys(Eq(account_info),
                           TrustedVaultKeyAndVersionEq(kInitialVaultKey,
                                                       kInitialLastKeyVersion),
                           _, _))
      .WillOnce([&](const CoreAccountInfo&, const TrustedVaultKeyAndVersion&,
                    std::unique_ptr<SecureBoxKeyPair> key_pair,
                    TrustedVaultConnection::DownloadKeysCallback callback) {
        device_key_pair = std::move(key_pair);
        download_keys_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  // FetchKeys() should trigger keys downloading.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  backend()->FetchKeys(account_info, fetch_keys_callback.Get());
  ASSERT_FALSE(download_keys_callback.is_null());

  // Ensure that the right device key was passed into DonwloadKeys().
  ASSERT_THAT(device_key_pair, NotNull());
  EXPECT_THAT(device_key_pair->private_key().ExportToBytes(),
              Eq(private_device_key_material));

  // Mimic successful key downloading, it should make fetch keys attempt
  // completed.
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/Eq(kNewVaultKeys)));
  std::move(download_keys_callback)
      .Run(TrustedVaultRequestStatus::kSuccess, kNewVaultKeys,
           kNewLastKeyVersion);
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldThrottleAndUntrottleKeysDownloading) {
  CoreAccountInfo account_info;
  account_info.gaia = "user";

  const std::vector<uint8_t> kInitialVaultKey = {1, 2, 3};
  const int kInitialLastKeyVersion = 0;

  std::vector<uint8_t> private_device_key_material =
      StoreKeysAndMimicDeviceRegistration({kInitialVaultKey},
                                          kInitialLastKeyVersion, account_info);
  EXPECT_TRUE(backend()->MarkKeysAsStale(account_info));
  backend()->SetPrimaryAccount(account_info);

  TrustedVaultConnection::DownloadKeysCallback download_keys_callback;
  ON_CALL(*connection(), DownloadKeys(_, _, _, _))
      .WillByDefault(
          [&](const CoreAccountInfo&, const TrustedVaultKeyAndVersion&,
              std::unique_ptr<SecureBoxKeyPair> key_pair,
              TrustedVaultConnection::DownloadKeysCallback callback) {
            download_keys_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  clock()->SetNow(base::Time::Now());
  EXPECT_CALL(*connection(), DownloadKeys(_, _, _, _));

  // FetchKeys() should trigger keys downloading.
  backend()->FetchKeys(account_info, /*callback=*/base::DoNothing());
  ASSERT_FALSE(download_keys_callback.is_null());
  Mock::VerifyAndClearExpectations(connection());

  // Mimic transient failure.
  std::move(download_keys_callback)
      .Run(TrustedVaultRequestStatus::kOtherError,
           /*keys=*/std::vector<std::vector<uint8_t>>(),
           /*last_key_version=*/0);

  download_keys_callback = TrustedVaultConnection::DownloadKeysCallback();
  EXPECT_CALL(*connection(), DownloadKeys(_, _, _, _)).Times(0);
  // Following request should be throttled.
  backend()->FetchKeys(account_info, /*callback=*/base::DoNothing());
  EXPECT_TRUE(download_keys_callback.is_null());
  Mock::VerifyAndClearExpectations(connection());

  // Advance time to pass the throttling duration and trigger another attempt.
  clock()->Advance(switches::kTrustedVaultServiceThrottlingDuration.Get());

  EXPECT_CALL(*connection(), DownloadKeys(_, _, _, _));
  backend()->FetchKeys(account_info, /*callback=*/base::DoNothing());
  EXPECT_FALSE(download_keys_callback.is_null());
}

}  // namespace

}  // namespace syncer
