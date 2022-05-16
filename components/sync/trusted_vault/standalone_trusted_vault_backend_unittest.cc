// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/standalone_trusted_vault_backend.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "components/os_crypt/os_crypt.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/sync/base/features.h"
#include "components/sync/trusted_vault/proto_string_bytes_conversion.h"
#include "components/sync/trusted_vault/securebox.h"
#include "components/sync/trusted_vault/trusted_vault_connection.h"
#include "components/sync/trusted_vault/trusted_vault_server_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::IsNull;
using testing::Mock;
using testing::Ne;
using testing::NotNull;
using testing::SaveArg;

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

MATCHER_P(PublicKeyWhenExportedEq, expected, "") {
  const SecureBoxPublicKey& actual_public_key = arg;
  return actual_public_key.ExportToBytes() == expected;
}

base::FilePath CreateUniqueTempDir(base::ScopedTempDir* temp_dir) {
  EXPECT_TRUE(temp_dir->CreateUniqueTempDir());
  return temp_dir->GetPath();
}

CoreAccountInfo MakeAccountInfoWithGaiaId(const std::string& gaia_id) {
  CoreAccountInfo account_info;
  account_info.gaia = gaia_id;
  return account_info;
}

sync_pb::LocalTrustedVault ReadLocalTrustedVaultFile(
    const base::FilePath& path) {
  std::string ciphertext;
  base::ReadFileToString(path, &ciphertext);

  std::string decrypted_content;
  OSCrypt::DecryptString(ciphertext, &decrypted_content);

  sync_pb::LocalTrustedVault proto;
  proto.ParseFromString(decrypted_content);
  return proto;
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
  MOCK_METHOD(std::unique_ptr<Request>,
              RegisterAuthenticationFactor,
              (const CoreAccountInfo& account_info,
               const std::vector<std::vector<uint8_t>>& trusted_vault_keys,
               int last_trusted_vault_key_version,
               const SecureBoxPublicKey& authentication_factor_public_key,
               AuthenticationFactorType authentication_factor_type,
               absl::optional<int> authentication_factor_type_hint,
               RegisterAuthenticationFactorCallback callback),
              (override));
  MOCK_METHOD(std::unique_ptr<Request>,
              RegisterDeviceWithoutKeys,
              (const CoreAccountInfo& account_info,
               const SecureBoxPublicKey& device_public_key,
               RegisterDeviceWithoutKeysCallback callback),
              (override));
  MOCK_METHOD(
      std::unique_ptr<Request>,
      DownloadNewKeys,
      (const CoreAccountInfo& account_info,
       const TrustedVaultKeyAndVersion& last_trusted_vault_key_and_version,
       std::unique_ptr<SecureBoxKeyPair> device_key_pair,
       DownloadNewKeysCallback callback),
      (override));
  MOCK_METHOD(std::unique_ptr<Request>,
              DownloadIsRecoverabilityDegraded,
              (const CoreAccountInfo& account_info,
               IsRecoverabilityDegradedCallback),
              (override));
};

class StandaloneTrustedVaultBackendTest : public testing::Test {
 public:
  StandaloneTrustedVaultBackendTest()
      : file_path_(
            CreateUniqueTempDir(&temp_dir_)
                .Append(base::FilePath(FILE_PATH_LITERAL("some_file")))) {
    clock_.SetNow(base::Time::Now());
    ResetBackend();
  }

  ~StandaloneTrustedVaultBackendTest() override = default;

  void SetUp() override { OSCryptMocker::SetUp(); }

  void TearDown() override { OSCryptMocker::TearDown(); }

  void ResetBackend() {
    auto delegate = std::make_unique<testing::NiceMock<MockDelegate>>();
    delegate_ = delegate.get();

    auto connection =
        std::make_unique<testing::NiceMock<MockTrustedVaultConnection>>();
    connection_ = connection.get();

    backend_ = base::MakeRefCounted<StandaloneTrustedVaultBackend>(
        file_path_, std::move(delegate), std::move(connection));
    backend_->SetClockForTesting(&clock_);

    // To avoid DCHECK failures in tests that exercise SetPrimaryAccount(),
    // return non-null for RegisterAuthenticationFactor(). This registration
    // operation will never complete, though.
    ON_CALL(*connection_, RegisterAuthenticationFactor)
        .WillByDefault(testing::InvokeWithoutArgs([&]() {
          return std::make_unique<TrustedVaultConnection::Request>();
        }));
    ON_CALL(*connection_, RegisterDeviceWithoutKeys)
        .WillByDefault(testing::InvokeWithoutArgs([&]() {
          return std::make_unique<TrustedVaultConnection::Request>();
        }));
  }

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
                    Eq(account_info), Eq(vault_keys), last_vault_key_version, _,
                    AuthenticationFactorType::kPhysicalDevice,
                    /*authentication_factor_type_hint=*/Eq(absl::nullopt), _))
        .WillOnce(
            [&](const CoreAccountInfo&,
                const std::vector<std::vector<uint8_t>>& vault_keys,
                int last_vault_key_version,
                const SecureBoxPublicKey& device_public_key,
                AuthenticationFactorType, absl::optional<int>,
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
    backend()->SetPrimaryAccount(account_info,
                                 /*has_persistent_auth_error=*/false);
    EXPECT_FALSE(device_registration_callback.is_null());

    // Pretend that the registration completed successfully.
    std::move(device_registration_callback)
        .Run(TrustedVaultRegistrationStatus::kSuccess);

    // Reset primary account.
    backend()->SetPrimaryAccount(absl::nullopt,
                                 /*has_persistent_auth_error=*/false);

    std::string device_private_key_material =
        backend_->GetDeviceRegistrationInfoForTesting(account_info.gaia)
            .private_key_material();
    return std::vector<uint8_t>(device_private_key_material.begin(),
                                device_private_key_material.end());
  }

 private:
  base::ScopedTempDir temp_dir_;
  const base::FilePath file_path_;
  raw_ptr<testing::NiceMock<MockDelegate>> delegate_;
  raw_ptr<testing::NiceMock<MockTrustedVaultConnection>> connection_;
  base::SimpleTestClock clock_;
  scoped_refptr<StandaloneTrustedVaultBackend> backend_;
};

TEST_F(StandaloneTrustedVaultBackendTest, ShouldFetchEmptyKeys) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  // Callback should be called immediately.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/IsEmpty()));
  backend()->FetchKeys(account_info, fetch_keys_callback.Get());
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldReadAndFetchNonEmptyKeys) {
  const CoreAccountInfo account_info_1 = MakeAccountInfoWithGaiaId("user1");
  const CoreAccountInfo account_info_2 = MakeAccountInfoWithGaiaId("user2");

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

TEST_F(StandaloneTrustedVaultBackendTest, ShouldFilterOutConstantKey) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user1");
  const std::vector<uint8_t> kKey = {1, 2, 3, 4};

  sync_pb::LocalTrustedVault initial_data;
  sync_pb::LocalTrustedVaultPerUser* user_data = initial_data.add_user();
  user_data->set_gaia_id(account_info.gaia);
  user_data->add_vault_key()->set_key_material(
      GetConstantTrustedVaultKey().data(), GetConstantTrustedVaultKey().size());
  user_data->add_vault_key()->set_key_material(kKey.data(), kKey.size());

  std::string encrypted_data;
  ASSERT_TRUE(OSCrypt::EncryptString(initial_data.SerializeAsString(),
                                     &encrypted_data));
  ASSERT_NE(-1, base::WriteFile(file_path(), encrypted_data.c_str(),
                                encrypted_data.size()));

  backend()->ReadDataFromDisk();

  // Keys should be fetched immediately, constant key must be filtered out.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/ElementsAre(kKey)));
  backend()->FetchKeys(account_info, fetch_keys_callback.Get());
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

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldUpgradeToVersion1AndFixMissingConstantKey) {
  const CoreAccountInfo account_info_1 = MakeAccountInfoWithGaiaId("user1");
  const CoreAccountInfo account_info_2 = MakeAccountInfoWithGaiaId("user2");

  const std::vector<uint8_t> kKey1 = {0, 1, 2, 3, 4};
  const std::vector<uint8_t> kKey2 = {1, 2, 3, 4};

  sync_pb::LocalTrustedVault initial_data;
  sync_pb::LocalTrustedVaultPerUser* user_data1 = initial_data.add_user();
  sync_pb::LocalTrustedVaultPerUser* user_data2 = initial_data.add_user();
  user_data1->set_gaia_id(account_info_1.gaia);
  user_data2->set_gaia_id(account_info_2.gaia);
  // Mimic |user_data1| to be affected by crbug.com/1267391 and |user_data2| to
  // be not affected.
  AssignBytesToProtoString(kKey1,
                           user_data1->add_vault_key()->mutable_key_material());
  AssignBytesToProtoString(GetConstantTrustedVaultKey(),
                           user_data2->add_vault_key()->mutable_key_material());
  AssignBytesToProtoString(kKey2,
                           user_data2->add_vault_key()->mutable_key_material());

  std::string encrypted_data;
  ASSERT_TRUE(OSCrypt::EncryptString(initial_data.SerializeAsString(),
                                     &encrypted_data));
  ASSERT_NE(-1, base::WriteFile(file_path(), encrypted_data.c_str(),
                                encrypted_data.size()));

  // Backend should fix corrupted data and write new state.
  backend()->ReadDataFromDisk();

  // Read the file from disk.
  std::string ciphertext;
  std::string decrypted_content;
  sync_pb::LocalTrustedVault proto;
  ASSERT_TRUE(base::ReadFileToString(file_path(), &ciphertext));
  ASSERT_THAT(ciphertext, Ne(""));
  ASSERT_TRUE(OSCrypt::DecryptString(ciphertext, &decrypted_content));
  ASSERT_TRUE(proto.ParseFromString(decrypted_content));
  ASSERT_THAT(proto.user_size(), Eq(2));
  // Constant key should be added for the first user.
  EXPECT_THAT(proto.user(0).vault_key(),
              ElementsAre(KeyMaterialEq(GetConstantTrustedVaultKey()),
                          KeyMaterialEq(kKey1)));
  // Sanity check that state for the second user isn't changed.
  EXPECT_THAT(proto.user(1).vault_key(),
              ElementsAre(KeyMaterialEq(GetConstantTrustedVaultKey()),
                          KeyMaterialEq(kKey2)));
  EXPECT_THAT(proto.data_version(), Eq(1));
}

// This test ensures that migration logic in ReadDataFromDisk() doesn't create
// new file if there wasn't any.
TEST_F(StandaloneTrustedVaultBackendTest, ShouldNotWriteEmptyData) {
  backend()->ReadDataFromDisk();
  EXPECT_FALSE(base::PathExists(file_path()));
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldFetchPreviouslyStoredKeys) {
  const CoreAccountInfo account_info_1 = MakeAccountInfoWithGaiaId("user1");
  const CoreAccountInfo account_info_2 = MakeAccountInfoWithGaiaId("user2");

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

TEST_F(StandaloneTrustedVaultBackendTest, ShouldDeleteNonPrimaryAccountKeys) {
  const CoreAccountInfo account_info_1 = MakeAccountInfoWithGaiaId("user1");
  const CoreAccountInfo account_info_2 = MakeAccountInfoWithGaiaId("user2");

  const std::vector<uint8_t> kKey1 = {0, 1, 2, 3, 4};
  const std::vector<uint8_t> kKey2 = {1, 2, 3, 4};
  const std::vector<uint8_t> kKey3 = {2, 3, 4};

  backend()->StoreKeys(account_info_1.gaia, {kKey1}, /*last_key_version=*/0);
  backend()->StoreKeys(account_info_2.gaia, {kKey2, kKey3},
                       /*last_key_version=*/1);

  // Make sure that backend handles primary account changes prior
  // UpdateAccountsInCookieJarInfo() call.
  backend()->SetPrimaryAccount(account_info_1,
                               /*has_persistent_auth_error=*/false);
  backend()->SetPrimaryAccount(absl::nullopt,
                               /*has_persistent_auth_error=*/false);

  // Keys should be removed immediately if account is not primary and not in
  // cookie jar.
  backend()->UpdateAccountsInCookieJarInfo(signin::AccountsInCookieJarInfo());

  // Keys should be removed from both in-memory and disk storages.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/IsEmpty()));
  backend()->FetchKeys(account_info_1, fetch_keys_callback.Get());

  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/IsEmpty()));
  backend()->FetchKeys(account_info_2, fetch_keys_callback.Get());

  // Read the file from disk and verify that keys were removed from disk
  // storage.
  sync_pb::LocalTrustedVault proto = ReadLocalTrustedVaultFile(file_path());
  EXPECT_THAT(proto.user_size(), Eq(0));
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldDeferPrimaryAccountKeysDeletion) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user1");
  const std::vector<uint8_t> kKey = {0, 1, 2, 3, 4};
  backend()->StoreKeys(account_info.gaia, {kKey}, /*last_key_version=*/0);
  backend()->SetPrimaryAccount(account_info,
                               /*has_persistent_auth_error=*/false);

  // Keys should not be removed immediately.
  backend()->UpdateAccountsInCookieJarInfo(signin::AccountsInCookieJarInfo());
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/ElementsAre(kKey)));
  backend()->FetchKeys(account_info, fetch_keys_callback.Get());

  // Reset primary account, keys should be deleted from both in-memory and disk
  // storage.
  backend()->SetPrimaryAccount(absl::nullopt,
                               /*has_persistent_auth_error=*/false);
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/IsEmpty()));
  backend()->FetchKeys(account_info, fetch_keys_callback.Get());

  // Read the file from disk and verify that keys were removed from disk
  // storage.
  sync_pb::LocalTrustedVault proto = ReadLocalTrustedVaultFile(file_path());
  EXPECT_THAT(proto.user_size(), Eq(0));
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldCompletePrimaryAccountKeysDeletionAfterRestart) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user1");
  const std::vector<uint8_t> kKey = {0, 1, 2, 3, 4};
  backend()->StoreKeys(account_info.gaia, {kKey}, /*last_key_version=*/0);
  backend()->SetPrimaryAccount(account_info,
                               /*has_persistent_auth_error=*/false);

  // Keys should not be removed immediately.
  backend()->UpdateAccountsInCookieJarInfo(signin::AccountsInCookieJarInfo());
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/ElementsAre(kKey)));
  backend()->FetchKeys(account_info, fetch_keys_callback.Get());

  // Mimic browser restart and reset primary account.
  auto new_backend = base::MakeRefCounted<StandaloneTrustedVaultBackend>(
      file_path(),
      /*delegate=*/std::make_unique<testing::NiceMock<MockDelegate>>(),
      /*connection=*/nullptr);
  new_backend->ReadDataFromDisk();
  new_backend->SetPrimaryAccount(absl::nullopt,
                                 /*has_persistent_auth_error=*/false);

  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/IsEmpty()));
  new_backend->FetchKeys(account_info, fetch_keys_callback.Get());

  // Read the file from disk and verify that keys were removed from disk
  // storage.
  sync_pb::LocalTrustedVault proto = ReadLocalTrustedVaultFile(file_path());
  EXPECT_THAT(proto.user_size(), Eq(0));
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldRegisterDevice) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  backend()->StoreKeys(account_info.gaia, {kVaultKey}, kLastKeyVersion);

  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      device_registration_callback;
  std::vector<uint8_t> serialized_public_device_key;
  EXPECT_CALL(*connection(),
              RegisterAuthenticationFactor(
                  Eq(account_info), ElementsAre(kVaultKey), kLastKeyVersion, _,
                  AuthenticationFactorType::kPhysicalDevice,
                  /*authentication_factor_type_hint=*/Eq(absl::nullopt), _))
      .WillOnce([&](const CoreAccountInfo&,
                    const std::vector<std::vector<uint8_t>>&, int,
                    const SecureBoxPublicKey& device_public_key,
                    AuthenticationFactorType, absl::optional<int>,
                    TrustedVaultConnection::RegisterAuthenticationFactorCallback
                        callback) {
        serialized_public_device_key = device_public_key.ExportToBytes();
        device_registration_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  // Setting the primary account will trigger device registration.
  base::HistogramTester histogram_tester;
  backend()->SetPrimaryAccount(account_info,
                               /*has_persistent_auth_error=*/false);
  ASSERT_FALSE(device_registration_callback.is_null());
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultDeviceRegistrationState",
      /*sample=*/
      TrustedVaultDeviceRegistrationStateForUMA::
          kAttemptingRegistrationWithNewKeyPair,
      /*expected_bucket_count=*/1);

  // Pretend that the registration completed successfully.
  std::move(device_registration_callback)
      .Run(TrustedVaultRegistrationStatus::kSuccess);

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
       ShouldClearDataAndAttemptDeviceRegistration) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<std::vector<uint8_t>> kInitialVaultKeys = {{1, 2, 3}};
  const int kInitialLastKeyVersion = 1;

  // Mimic device previously registered with some keys.
  StoreKeysAndMimicDeviceRegistration(kInitialVaultKeys, kInitialLastKeyVersion,
                                      account_info);

  // Set primary account to trigger immediate device registration attempt upon
  // reset.
  backend()->SetPrimaryAccount(account_info,
                               /*has_persistent_auth_error=*/false);

  // Expect device registration attempt without keys.
  TrustedVaultConnection::RegisterDeviceWithoutKeysCallback
      device_registration_callback;
  std::vector<uint8_t> serialized_public_device_key;
  EXPECT_CALL(*connection(), RegisterDeviceWithoutKeys(Eq(account_info), _, _))
      .WillOnce([&](const CoreAccountInfo& account_info,
                    const SecureBoxPublicKey& device_public_key,
                    TrustedVaultConnection::RegisterDeviceWithoutKeysCallback
                        callback) {
        serialized_public_device_key = device_public_key.ExportToBytes();
        device_registration_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  // Clear data for |account_info|, keys should be removed and device
  // registration attempt should be triggered.
  backend()->ClearDataForAccount(account_info);

  // Callback should be called immediately.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/IsEmpty()));
  backend()->FetchKeys(account_info, fetch_keys_callback.Get());

  // Mimic successful device registration and verify the state.
  std::move(device_registration_callback)
      .Run(TrustedVaultRegistrationStatus::kSuccess,
           TrustedVaultKeyAndVersion(GetConstantTrustedVaultKey(),
                                     kInitialLastKeyVersion + 1));

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
       ShouldRecordAuthErrorAndAttemptDeviceRegistration) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  backend()->StoreKeys(account_info.gaia, {kVaultKey}, kLastKeyVersion);

  EXPECT_CALL(*connection(),
              RegisterAuthenticationFactor(
                  Eq(account_info), ElementsAre(kVaultKey), kLastKeyVersion, _,
                  AuthenticationFactorType::kPhysicalDevice,
                  /*authentication_factor_type_hint=*/Eq(absl::nullopt), _));

  base::HistogramTester histogram_tester;
  backend()->SetPrimaryAccount(account_info,
                               /*has_persistent_auth_error=*/true);
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultDeviceRegistrationState",
      /*sample=*/
      TrustedVaultDeviceRegistrationStateForUMA::
          kAttemptingRegistrationWithPersistentAuthError,
      /*expected_bucket_count=*/1);
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldNotRegisterDeviceIfLocalKeysAreStale) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  backend()->StoreKeys(account_info.gaia, {kVaultKey}, kLastKeyVersion);
  ASSERT_TRUE(backend()->MarkLocalKeysAsStale(account_info));

  EXPECT_CALL(*connection(), RegisterAuthenticationFactor).Times(0);
  EXPECT_CALL(*connection(), RegisterDeviceWithoutKeys).Times(0);

  base::HistogramTester histogram_tester;
  backend()->SetPrimaryAccount(account_info,
                               /*has_persistent_auth_error=*/false);

  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultDeviceRegistrationState",
      /*sample=*/
      TrustedVaultDeviceRegistrationStateForUMA::kLocalKeysAreStale,
      /*expected_bucket_count=*/1);
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldNotRegisterDeviceIfAlreadyRegistered) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      device_registration_callback;
  ON_CALL(*connection(),
          RegisterAuthenticationFactor(
              Eq(account_info), ElementsAre(kVaultKey), kLastKeyVersion, _,
              AuthenticationFactorType::kPhysicalDevice,
              /*authentication_factor_type_hint=*/Eq(absl::nullopt), _))
      .WillByDefault(
          [&](const CoreAccountInfo&, const std::vector<std::vector<uint8_t>>&,
              int, const SecureBoxPublicKey& device_public_key,
              AuthenticationFactorType, absl::optional<int>,
              TrustedVaultConnection::RegisterAuthenticationFactorCallback
                  callback) {
            device_registration_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  backend()->StoreKeys(account_info.gaia, {kVaultKey}, kLastKeyVersion);
  backend()->SetPrimaryAccount(account_info,
                               /*has_persistent_auth_error=*/false);
  ASSERT_FALSE(device_registration_callback.is_null());
  std::move(device_registration_callback)
      .Run(TrustedVaultRegistrationStatus::kSuccess);

  // Now the device should be registered.
  ASSERT_TRUE(backend()
                  ->GetDeviceRegistrationInfoForTesting(account_info.gaia)
                  .device_registered());

  // Mimic a restart. The device should remain registered.
  ResetBackend();
  backend()->ReadDataFromDisk();

  ASSERT_TRUE(backend()
                  ->GetDeviceRegistrationInfoForTesting(account_info.gaia)
                  .device_registered());

  // The device should not register again.
  EXPECT_CALL(*connection(), RegisterAuthenticationFactor).Times(0);
  EXPECT_CALL(*connection(), RegisterDeviceWithoutKeys).Times(0);

  base::HistogramTester histogram_tester;
  backend()->SetPrimaryAccount(account_info,
                               /*has_persistent_auth_error=*/false);

  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultDeviceRegistrationState",
      /*sample=*/
      TrustedVaultDeviceRegistrationStateForUMA::kAlreadyRegistered,
      /*expected_bucket_count=*/1);
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldThrottleAndUnthrottleDeviceRegistration) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  backend()->StoreKeys(account_info.gaia, {kVaultKey}, kLastKeyVersion);
  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      device_registration_callback;
  ON_CALL(*connection(), RegisterAuthenticationFactor)
      .WillByDefault(
          [&](const CoreAccountInfo&, const std::vector<std::vector<uint8_t>>&,
              int, const SecureBoxPublicKey&, AuthenticationFactorType,
              absl::optional<int>,
              TrustedVaultConnection::RegisterAuthenticationFactorCallback
                  callback) {
            device_registration_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  EXPECT_CALL(*connection(), RegisterAuthenticationFactor);
  // Setting the primary account will trigger device registration.
  backend()->SetPrimaryAccount(account_info,
                               /*has_persistent_auth_error=*/false);
  ASSERT_FALSE(device_registration_callback.is_null());
  Mock::VerifyAndClearExpectations(connection());

  // Mimic transient failure.
  std::move(device_registration_callback)
      .Run(TrustedVaultRegistrationStatus::kOtherError);

  // Mimic a restart to trigger device registration attempt, which should remain
  // throttled.
  base::HistogramTester histogram_tester;
  ResetBackend();
  EXPECT_CALL(*connection(), RegisterAuthenticationFactor).Times(0);
  backend()->ReadDataFromDisk();
  backend()->SetPrimaryAccount(account_info,
                               /*has_persistent_auth_error=*/false);
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultDeviceRegistrationState",
      /*sample=*/
      TrustedVaultDeviceRegistrationStateForUMA::kThrottledClientSide,
      /*expected_bucket_count=*/1);

  // Mimic a restart after sufficient time has passed, to trigger another device
  // registration attempt, which should now be unthrottled.
  base::HistogramTester histogram_tester2;
  ResetBackend();
  EXPECT_CALL(*connection(), RegisterAuthenticationFactor);
  clock()->Advance(kTrustedVaultServiceThrottlingDuration.Get());
  backend()->ReadDataFromDisk();
  backend()->SetPrimaryAccount(account_info,
                               /*has_persistent_auth_error=*/false);
  histogram_tester2.ExpectUniqueSample(
      "Sync.TrustedVaultDeviceRegistrationState",
      /*sample=*/
      TrustedVaultDeviceRegistrationStateForUMA::
          kAttemptingRegistrationWithExistingKeyPair,
      /*expected_bucket_count=*/1);
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldNotThrottleUponAccessTokenFetchingFailure) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  backend()->StoreKeys(account_info.gaia, {kVaultKey}, kLastKeyVersion);
  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      device_registration_callback;
  ON_CALL(*connection(), RegisterAuthenticationFactor)
      .WillByDefault(
          [&](const CoreAccountInfo&, const std::vector<std::vector<uint8_t>>&,
              int, const SecureBoxPublicKey&, AuthenticationFactorType,
              absl::optional<int>,
              TrustedVaultConnection::RegisterAuthenticationFactorCallback
                  callback) {
            device_registration_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  EXPECT_CALL(*connection(), RegisterAuthenticationFactor);
  // Setting the primary account will trigger device registration.
  backend()->SetPrimaryAccount(account_info,
                               /*has_persistent_auth_error=*/false);
  ASSERT_FALSE(device_registration_callback.is_null());
  Mock::VerifyAndClearExpectations(connection());

  // Mimic access token fetching failure.
  std::move(device_registration_callback)
      .Run(TrustedVaultRegistrationStatus::kAccessTokenFetchingFailure);

  // Mimic a restart to trigger device registration attempt, which should not be
  // throttled.
  ResetBackend();
  EXPECT_CALL(*connection(), RegisterAuthenticationFactor);
  backend()->ReadDataFromDisk();
  backend()->SetPrimaryAccount(account_info,
                               /*has_persistent_auth_error=*/false);
}

// System time can be changed to the past and if this situation not handled,
// requests could be throttled for unreasonable amount of time.
TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldUnthrottleDeviceRegistrationWhenTimeSetToPast) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kVaultKey = {1, 2, 3};
  const int kLastKeyVersion = 1;

  backend()->StoreKeys(account_info.gaia, {kVaultKey}, kLastKeyVersion);
  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      device_registration_callback;
  ON_CALL(*connection(), RegisterAuthenticationFactor)
      .WillByDefault(
          [&](const CoreAccountInfo&, const std::vector<std::vector<uint8_t>>&,
              int, const SecureBoxPublicKey&, AuthenticationFactorType,
              absl::optional<int>,
              TrustedVaultConnection::RegisterAuthenticationFactorCallback
                  callback) {
            device_registration_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  clock()->SetNow(base::Time::Now());

  EXPECT_CALL(*connection(), RegisterAuthenticationFactor);
  // Setting the primary account will trigger device registration.
  backend()->SetPrimaryAccount(account_info,
                               /*has_persistent_auth_error=*/false);
  ASSERT_FALSE(device_registration_callback.is_null());
  Mock::VerifyAndClearExpectations(connection());

  // Mimic transient failure.
  std::move(device_registration_callback)
      .Run(TrustedVaultRegistrationStatus::kOtherError);

  // Mimic system set to the past.
  clock()->Advance(base::Seconds(-1));

  device_registration_callback =
      TrustedVaultConnection::RegisterAuthenticationFactorCallback();
  EXPECT_CALL(*connection(), RegisterAuthenticationFactor);
  // Reset and set primary account to trigger device registration attempt.
  backend()->SetPrimaryAccount(absl::nullopt,
                               /*has_persistent_auth_error=*/false);
  backend()->SetPrimaryAccount(account_info,
                               /*has_persistent_auth_error=*/false);
  EXPECT_FALSE(device_registration_callback.is_null());
}

// Unless keys marked as stale, FetchKeys() should be completed immediately,
// without keys download attempt.
TEST_F(StandaloneTrustedVaultBackendTest, ShouldFetchKeysImmediately) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<std::vector<uint8_t>> kVaultKeys = {{1, 2, 3}};
  const int kLastKeyVersion = 1;

  // Make keys downloading theoretically possible.
  StoreKeysAndMimicDeviceRegistration(kVaultKeys, kLastKeyVersion,
                                      account_info);
  backend()->SetPrimaryAccount(account_info,
                               /*has_persistent_auth_error=*/false);

  EXPECT_CALL(*connection(), DownloadNewKeys).Times(0);

  std::vector<std::vector<uint8_t>> fetched_keys;
  // Callback should be called immediately.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/Eq(kVaultKeys)));
  backend()->FetchKeys(account_info, fetch_keys_callback.Get());
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldDownloadNewKeys) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kInitialVaultKey = {1, 2, 3};
  const int kInitialLastKeyVersion = 1;

  std::vector<uint8_t> private_device_key_material =
      StoreKeysAndMimicDeviceRegistration({kInitialVaultKey},
                                          kInitialLastKeyVersion, account_info);
  EXPECT_TRUE(backend()->MarkLocalKeysAsStale(account_info));
  backend()->SetPrimaryAccount(account_info,
                               /*has_persistent_auth_error=*/false);

  const std::vector<uint8_t> kNewVaultKey = {1, 3, 2};
  const int kNewLastKeyVersion = 2;

  std::unique_ptr<SecureBoxKeyPair> device_key_pair;
  TrustedVaultConnection::DownloadNewKeysCallback download_keys_callback;
  EXPECT_CALL(*connection(),
              DownloadNewKeys(Eq(account_info),
                              TrustedVaultKeyAndVersionEq(
                                  kInitialVaultKey, kInitialLastKeyVersion),
                              _, _))
      .WillOnce([&](const CoreAccountInfo&, const TrustedVaultKeyAndVersion&,
                    std::unique_ptr<SecureBoxKeyPair> key_pair,
                    TrustedVaultConnection::DownloadNewKeysCallback callback) {
        device_key_pair = std::move(key_pair);
        download_keys_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  // FetchKeys() should trigger keys downloading.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  backend()->FetchKeys(account_info, fetch_keys_callback.Get());
  ASSERT_FALSE(download_keys_callback.is_null());

  // Ensure that the right device key was passed into DownloadNewKeys().
  ASSERT_THAT(device_key_pair, NotNull());
  EXPECT_THAT(device_key_pair->private_key().ExportToBytes(),
              Eq(private_device_key_material));

  // Mimic successful key downloading, it should make fetch keys attempt
  // completed. Note that the client should keep old key as well.
  EXPECT_CALL(fetch_keys_callback,
              Run(/*keys=*/ElementsAre(kInitialVaultKey, kNewVaultKey)));
  std::move(download_keys_callback)
      .Run(TrustedVaultDownloadKeysStatus::kSuccess, {kNewVaultKey},
           kNewLastKeyVersion);
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldThrottleAndUntrottleKeysDownloading) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const std::vector<uint8_t> kInitialVaultKey = {1, 2, 3};
  const int kInitialLastKeyVersion = 1;

  std::vector<uint8_t> private_device_key_material =
      StoreKeysAndMimicDeviceRegistration({kInitialVaultKey},
                                          kInitialLastKeyVersion, account_info);
  EXPECT_TRUE(backend()->MarkLocalKeysAsStale(account_info));
  backend()->SetPrimaryAccount(account_info,
                               /*has_persistent_auth_error=*/false);

  TrustedVaultConnection::DownloadNewKeysCallback download_keys_callback;
  ON_CALL(*connection(), DownloadNewKeys)
      .WillByDefault(
          [&](const CoreAccountInfo&,
              const absl::optional<TrustedVaultKeyAndVersion>&,
              std::unique_ptr<SecureBoxKeyPair> key_pair,
              TrustedVaultConnection::DownloadNewKeysCallback callback) {
            download_keys_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  clock()->SetNow(base::Time::Now());
  EXPECT_CALL(*connection(), DownloadNewKeys);

  // FetchKeys() should trigger keys downloading.
  backend()->FetchKeys(account_info, /*callback=*/base::DoNothing());
  ASSERT_FALSE(download_keys_callback.is_null());
  Mock::VerifyAndClearExpectations(connection());

  // Mimic transient failure.
  std::move(download_keys_callback)
      .Run(TrustedVaultDownloadKeysStatus::kOtherError,
           /*keys=*/std::vector<std::vector<uint8_t>>(),
           /*last_key_version=*/0);

  download_keys_callback = TrustedVaultConnection::DownloadNewKeysCallback();
  EXPECT_CALL(*connection(), DownloadNewKeys).Times(0);
  // Following request should be throttled.
  backend()->FetchKeys(account_info, /*callback=*/base::DoNothing());
  EXPECT_TRUE(download_keys_callback.is_null());
  Mock::VerifyAndClearExpectations(connection());

  // Advance time to pass the throttling duration and trigger another attempt.
  clock()->Advance(kTrustedVaultServiceThrottlingDuration.Get());

  EXPECT_CALL(*connection(), DownloadNewKeys);
  backend()->FetchKeys(account_info, /*callback=*/base::DoNothing());
  EXPECT_FALSE(download_keys_callback.is_null());
}

// Tests silent device registration (when no vault keys available yet). After
// successful registration, the client should be able to download keys.
TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldSilentlyRegisterDeviceAndDownloadNewKeys) {
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const int kServerConstantKeyVersion = 100;

  TrustedVaultConnection::RegisterDeviceWithoutKeysCallback
      device_registration_callback;
  EXPECT_CALL(*connection(), RegisterDeviceWithoutKeys(account_info, _, _))
      .WillOnce([&](const CoreAccountInfo&, const SecureBoxPublicKey&,
                    TrustedVaultConnection::RegisterDeviceWithoutKeysCallback
                        callback) {
        device_registration_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  // Setting the primary account will trigger device registration.
  backend()->SetPrimaryAccount(account_info,
                               /*has_persistent_auth_error=*/false);
  ASSERT_FALSE(device_registration_callback.is_null());

  // Pretend that the registration completed successfully.
  std::move(device_registration_callback)
      .Run(TrustedVaultRegistrationStatus::kSuccess,
           TrustedVaultKeyAndVersion{GetConstantTrustedVaultKey(),
                                     kServerConstantKeyVersion});

  // Now the device should be registered.
  sync_pb::LocalDeviceRegistrationInfo registration_info =
      backend()->GetDeviceRegistrationInfoForTesting(account_info.gaia);
  EXPECT_TRUE(registration_info.device_registered());
  EXPECT_TRUE(registration_info.has_private_key_material());

  TrustedVaultConnection::DownloadNewKeysCallback download_keys_callback;
  ON_CALL(*connection(), DownloadNewKeys(account_info,
                                         TrustedVaultKeyAndVersionEq(
                                             GetConstantTrustedVaultKey(),
                                             kServerConstantKeyVersion),
                                         _, _))
      .WillByDefault(
          [&](const CoreAccountInfo&, const TrustedVaultKeyAndVersion&,
              std::unique_ptr<SecureBoxKeyPair> key_pair,
              TrustedVaultConnection::DownloadNewKeysCallback callback) {
            download_keys_callback = std::move(callback);
            return std::make_unique<TrustedVaultConnection::Request>();
          });

  // FetchKeys() should trigger keys downloading. Note: unlike tests with
  // following regular key rotation, in this case MarkLocalKeysAsStale() isn't
  // called intentionally.
  base::MockCallback<StandaloneTrustedVaultBackend::FetchKeysCallback>
      fetch_keys_callback;
  backend()->FetchKeys(account_info, fetch_keys_callback.Get());
  ASSERT_FALSE(download_keys_callback.is_null());

  // Mimic successful key downloading, it should make fetch keys attempt
  // completed.
  const std::vector<std::vector<uint8_t>> kNewVaultKeys = {{1, 2, 3}};
  EXPECT_CALL(fetch_keys_callback, Run(/*keys=*/kNewVaultKeys));
  std::move(download_keys_callback)
      .Run(TrustedVaultDownloadKeysStatus::kSuccess, kNewVaultKeys,
           /*last_key_version=*/kServerConstantKeyVersion + 1);
}

TEST_F(StandaloneTrustedVaultBackendTest, ShouldAddTrustedRecoveryMethod) {
  const std::vector<std::vector<uint8_t>> kVaultKeys = {{1, 2}, {1, 2, 3}};
  const int kLastKeyVersion = 1;
  const std::vector<uint8_t> kPublicKey =
      SecureBoxKeyPair::GenerateRandom()->public_key().ExportToBytes();
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const int kMethodTypeHint = 7;

  backend()->SetPrimaryAccount(account_info,
                               /*has_persistent_auth_error=*/false);
  backend()->StoreKeys(account_info.gaia, kVaultKeys, kLastKeyVersion);

  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      registration_callback;
  EXPECT_CALL(
      *connection(),
      RegisterAuthenticationFactor(
          Eq(account_info), Eq(kVaultKeys), kLastKeyVersion,
          PublicKeyWhenExportedEq(kPublicKey),
          AuthenticationFactorType::kUnspecified, Eq(kMethodTypeHint), _))
      .WillOnce([&](const CoreAccountInfo&,
                    const std::vector<std::vector<uint8_t>>&, int,
                    const SecureBoxPublicKey& device_public_key,
                    AuthenticationFactorType, absl::optional<int>,
                    TrustedVaultConnection::RegisterAuthenticationFactorCallback
                        callback) {
        registration_callback = std::move(callback);
        return std::make_unique<TrustedVaultConnection::Request>();
      });

  base::MockCallback<base::OnceClosure> completion_callback;
  backend()->AddTrustedRecoveryMethod(account_info.gaia, kPublicKey,
                                      kMethodTypeHint,
                                      completion_callback.Get());

  // The operation should be in flight.
  ASSERT_FALSE(registration_callback.is_null());

  // Mimic successful completion of the request.
  EXPECT_CALL(completion_callback, Run());
  std::move(registration_callback)
      .Run(TrustedVaultRegistrationStatus::kSuccess);
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldIgnoreTrustedRecoveryMethodWithInvalidPublicKey) {
  const std::vector<std::vector<uint8_t>> kVaultKeys = {{1, 2, 3}};
  const int kLastKeyVersion = 0;
  const std::vector<uint8_t> kInvalidPublicKey = {1, 2, 3, 4};
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const int kMethodTypeHint = 7;

  ASSERT_THAT(SecureBoxPublicKey::CreateByImport(kInvalidPublicKey), IsNull());

  backend()->SetPrimaryAccount(account_info,
                               /*has_persistent_auth_error=*/false);
  backend()->StoreKeys(account_info.gaia, kVaultKeys, kLastKeyVersion);

  EXPECT_CALL(*connection(), RegisterAuthenticationFactor).Times(0);

  base::MockCallback<base::OnceClosure> completion_callback;
  EXPECT_CALL(completion_callback, Run());
  backend()->AddTrustedRecoveryMethod(account_info.gaia, kInvalidPublicKey,
                                      kMethodTypeHint,
                                      completion_callback.Get());
}

TEST_F(StandaloneTrustedVaultBackendTest,
       ShouldDeferTrustedRecoveryMethodUntilPrimaryAccount) {
  const std::vector<std::vector<uint8_t>> kVaultKeys = {{1, 2, 3}};
  const int kLastKeyVersion = 1;
  const std::vector<uint8_t> kPublicKey =
      SecureBoxKeyPair::GenerateRandom()->public_key().ExportToBytes();
  const CoreAccountInfo account_info = MakeAccountInfoWithGaiaId("user");
  const int kMethodTypeHint = 7;

  backend()->StoreKeys(account_info.gaia, kVaultKeys, kLastKeyVersion);

  // No request should be issued while there is no primary account.
  base::MockCallback<base::OnceClosure> completion_callback;
  EXPECT_CALL(*connection(), RegisterAuthenticationFactor).Times(0);
  backend()->AddTrustedRecoveryMethod(account_info.gaia, kPublicKey,
                                      kMethodTypeHint,
                                      completion_callback.Get());

  // Upon setting a primary account, RegisterAuthenticationFactor() should be
  // invoked. It should in fact be called twice: one for device registration,
  // and one for the AddTrustedRecoveryMethod() call being tested here.
  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      registration_callback;
  EXPECT_CALL(*connection(),
              RegisterAuthenticationFactor(
                  _, _, _, _, AuthenticationFactorType::kPhysicalDevice, _, _));
  EXPECT_CALL(
      *connection(),
      RegisterAuthenticationFactor(
          Eq(account_info), Eq(kVaultKeys), kLastKeyVersion,
          PublicKeyWhenExportedEq(kPublicKey),
          AuthenticationFactorType::kUnspecified, Eq(kMethodTypeHint), _))
      .WillOnce([&](const CoreAccountInfo&,
                    const std::vector<std::vector<uint8_t>>&, int,
                    const SecureBoxPublicKey& device_public_key,
                    AuthenticationFactorType, absl::optional<int>,
                    TrustedVaultConnection::RegisterAuthenticationFactorCallback
                        callback) {
        registration_callback = std::move(callback);
        // Note: TrustedVaultConnection::Request doesn't support
        // cancellation, so these tests don't cover the contract that
        // caller should store Request object until it's completed or need
        // to be cancelled.
        return std::make_unique<TrustedVaultConnection::Request>();
      });
  backend()->SetPrimaryAccount(account_info,
                               /*has_persistent_auth_error=*/false);

  // The operation should be in flight.
  ASSERT_FALSE(registration_callback.is_null());

  // Mimic successful completion of the request.
  EXPECT_CALL(completion_callback, Run());
  std::move(registration_callback)
      .Run(TrustedVaultRegistrationStatus::kSuccess);
}

}  // namespace

}  // namespace syncer
