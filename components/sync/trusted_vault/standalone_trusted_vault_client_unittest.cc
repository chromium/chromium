// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/standalone_trusted_vault_client.h"

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/os_crypt/os_crypt.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/engine/sync_engine_switches.h"
#include "components/sync/protocol/local_trusted_vault.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::Ne;

MATCHER_P(KeyMaterialEq, expected, "") {
  const std::string& key_material = arg.key_material();
  const std::vector<uint8_t> key_material_as_bytes(key_material.begin(),
                                                   key_material.end());
  return key_material_as_bytes == expected;
}

base::FilePath CreateUniqueTempDir(base::ScopedTempDir* temp_dir) {
  EXPECT_TRUE(temp_dir->CreateUniqueTempDir());
  return temp_dir->GetPath();
}

// Issues a call to FetchKeys() for client |client| and waits until the callback
// completes. |client| must not be null.
std::vector<std::vector<uint8_t>> FetchKeysAndWaitForClient(
    const std::string& gaia_id,
    StandaloneTrustedVaultClient* client) {
  DCHECK(client);

  CoreAccountInfo account_info;
  account_info.gaia = gaia_id;

  base::RunLoop loop;
  std::vector<std::vector<uint8_t>> fetched_keys;
  client->FetchKeys(account_info,
                    base::BindLambdaForTesting(
                        [&](const std::vector<std::vector<uint8_t>>& keys) {
                          fetched_keys = keys;
                          loop.Quit();
                        }));
  loop.Run();
  return fetched_keys;
}

class StandaloneTrustedVaultClientTest : public testing::Test {
 protected:
  StandaloneTrustedVaultClientTest()
      : file_path_(CreateUniqueTempDir(&temp_dir_)
                       .Append(base::FilePath(FILE_PATH_LITERAL("some_file")))),
        client_(file_path_, identity_env_.identity_manager()) {
    override_features_.InitAndEnableFeature(
        switches::kSyncSupportTrustedVaultPassphrase);
  }

  ~StandaloneTrustedVaultClientTest() override = default;

  void SetUp() override { OSCryptMocker::SetUp(); }

  void TearDown() override { OSCryptMocker::TearDown(); }

  std::vector<std::vector<uint8_t>> FetchKeysAndWait(
      const std::string& gaia_id) {
    return FetchKeysAndWaitForClient(gaia_id, &client_);
  }

  void WaitForFlush() {
    base::RunLoop loop;
    client_.WaitForFlushForTesting(loop.QuitClosure());
    loop.Run();
  }

  base::Optional<CoreAccountInfo> FetchBackendPrimaryAccountAndWait() {
    base::RunLoop loop;
    base::Optional<CoreAccountInfo> fetched_primary_account;
    client_.FetchBackendPrimaryAccountForTesting(base::BindLambdaForTesting(
        [&](const base::Optional<CoreAccountInfo>& primary_account) {
          fetched_primary_account = primary_account;
          loop.Quit();
        }));
    loop.Run();
    return fetched_primary_account;
  }

  base::test::ScopedFeatureList override_features_;
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  const base::FilePath file_path_;
  // |identity_env_| should outlive |client_|.
  signin::IdentityTestEnvironment identity_env_;
  StandaloneTrustedVaultClient client_;
};

TEST_F(StandaloneTrustedVaultClientTest, ShouldFetchEmptyKeys) {
  const std::string kGaiaId = "user1";
  EXPECT_THAT(FetchKeysAndWait(kGaiaId), IsEmpty());
}

TEST_F(StandaloneTrustedVaultClientTest, ShouldFetchNonEmptyKeys) {
  const std::string kGaiaId1 = "user1";
  const std::string kGaiaId2 = "user2";
  const std::vector<uint8_t> kKey1 = {0, 1, 2, 3, 4};
  const std::vector<uint8_t> kKey2 = {1, 2, 3, 4};
  const std::vector<uint8_t> kKey3 = {2, 3, 4};

  sync_pb::LocalTrustedVault initial_data;
  sync_pb::LocalTrustedVaultPerUser* user_data1 = initial_data.add_user();
  sync_pb::LocalTrustedVaultPerUser* user_data2 = initial_data.add_user();
  user_data1->set_gaia_id(kGaiaId1);
  user_data2->set_gaia_id(kGaiaId2);
  user_data1->add_vault_key()->set_key_material(kKey1.data(), kKey1.size());
  user_data2->add_vault_key()->set_key_material(kKey2.data(), kKey2.size());
  user_data2->add_vault_key()->set_key_material(kKey3.data(), kKey3.size());

  std::string encrypted_data;
  ASSERT_TRUE(OSCrypt::EncryptString(initial_data.SerializeAsString(),
                                     &encrypted_data));
  ASSERT_NE(-1, base::WriteFile(file_path_, encrypted_data.c_str(),
                                encrypted_data.size()));

  // Initialize new client to read the data from the file.
  StandaloneTrustedVaultClient client(file_path_,
                                      identity_env_.identity_manager());
  EXPECT_THAT(FetchKeysAndWaitForClient(kGaiaId1, &client), ElementsAre(kKey1));
  EXPECT_THAT(FetchKeysAndWaitForClient(kGaiaId2, &client),
              ElementsAre(kKey2, kKey3));
}

TEST_F(StandaloneTrustedVaultClientTest, ShouldStoreKeys) {
  const std::string kGaiaId1 = "user1";
  const std::string kGaiaId2 = "user2";
  const std::vector<uint8_t> kKey1 = {0, 1, 2, 3, 4};
  const std::vector<uint8_t> kKey2 = {1, 2, 3, 4};
  const std::vector<uint8_t> kKey3 = {2, 3, 4};
  const std::vector<uint8_t> kKey4 = {3, 4};

  client_.StoreKeys(kGaiaId1, {kKey1}, /*last_key_version=*/7);
  client_.StoreKeys(kGaiaId2, {kKey2}, /*last_key_version=*/8);
  // Keys for |kGaiaId2| overriden, so |kKey2| should be lost.
  client_.StoreKeys(kGaiaId2, {kKey3, kKey4}, /*last_key_version=*/9);

  // Wait until the last write completes.
  WaitForFlush();

  // Read the file from disk.
  std::string ciphertext;
  std::string decrypted_content;
  sync_pb::LocalTrustedVault proto;
  EXPECT_TRUE(base::ReadFileToString(file_path_, &ciphertext));
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

TEST_F(StandaloneTrustedVaultClientTest, ShouldFetchPreviouslyStoredKeys) {
  const std::string kGaiaId1 = "user1";
  const std::string kGaiaId2 = "user2";
  const std::vector<uint8_t> kKey1 = {0, 1, 2, 3, 4};
  const std::vector<uint8_t> kKey2 = {1, 2, 3, 4};
  const std::vector<uint8_t> kKey3 = {2, 3, 4};

  client_.StoreKeys(kGaiaId1, {kKey1}, /*last_key_version=*/0);
  client_.StoreKeys(kGaiaId2, {kKey2, kKey3}, /*last_key_version=*/1);

  // Wait until the last write completes.
  WaitForFlush();

  // Instantiate a second client to read the file.
  StandaloneTrustedVaultClient other_client(file_path_,
                                            identity_env_.identity_manager());
  EXPECT_THAT(FetchKeysAndWaitForClient(kGaiaId1, &other_client),
              ElementsAre(kKey1));
  EXPECT_THAT(FetchKeysAndWaitForClient(kGaiaId2, &other_client),
              ElementsAre(kKey2, kKey3));
}

TEST_F(StandaloneTrustedVaultClientTest, ShouldRemoveAllStoredKeys) {
  const std::string kGaiaId1 = "user1";
  const std::string kGaiaId2 = "user2";
  const std::vector<uint8_t> kKey1 = {0, 1, 2, 3, 4};
  const std::vector<uint8_t> kKey2 = {1, 2, 3, 4};
  const std::vector<uint8_t> kKey3 = {2, 3, 4};

  client_.StoreKeys(kGaiaId1, {kKey1}, /*last_key_version=*/0);
  client_.StoreKeys(kGaiaId2, {kKey2, kKey3}, /*last_key_version=*/1);

  // Wait until the last write completes.
  WaitForFlush();
  client_.RemoveAllStoredKeys();

  // Wait until the last write completes.
  WaitForFlush();

  // Keys should be removed from both in-memory and disk storages.
  EXPECT_THAT(FetchKeysAndWait(kGaiaId1), IsEmpty());
  EXPECT_THAT(FetchKeysAndWait(kGaiaId2), IsEmpty());
  EXPECT_FALSE(base::PathExists(file_path_));
}

// ChromeOS doesn't support sign-out.
#if !defined(OS_CHROMEOS)
TEST_F(StandaloneTrustedVaultClientTest, ShouldPopulatePrimaryAccountChanges) {
  // Set initial primary account before backend initialization.
  const std::string email1 = "user1@gmail.com";
  identity_env_.SetPrimaryAccount(email1);

  // Verify that current primary account corresponds to |email1|.
  base::Optional<CoreAccountInfo> current_primary_account =
      FetchBackendPrimaryAccountAndWait();
  ASSERT_THAT(current_primary_account, Ne(base::nullopt));
  EXPECT_THAT(current_primary_account->email, Eq(email1));

  // Remove primary account.
  identity_env_.ClearPrimaryAccount();
  current_primary_account = FetchBackendPrimaryAccountAndWait();
  EXPECT_THAT(current_primary_account, Eq(base::nullopt));

  // Set new primary account.
  const std::string email2 = "user2@gmail.com";
  identity_env_.SetPrimaryAccount(email2);
  current_primary_account = FetchBackendPrimaryAccountAndWait();
  ASSERT_THAT(current_primary_account, Ne(base::nullopt));
  EXPECT_THAT(current_primary_account->email, Eq(email2));

  // Set unconsented primary account.
  const std::string email3 = "user3@gmail.com";
  identity_env_.ClearPrimaryAccount();
  identity_env_.SetUnconsentedPrimaryAccount(email3);
  current_primary_account = FetchBackendPrimaryAccountAndWait();
  ASSERT_THAT(current_primary_account, Ne(base::nullopt));
  EXPECT_THAT(current_primary_account->email, Eq(email3));
}
#endif

}  // namespace

}  // namespace syncer
