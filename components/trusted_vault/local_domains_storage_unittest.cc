// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/local_domains_storage.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/trusted_vault/features.h"
#include "components/trusted_vault/local_recovery_factor.h"
#include "components/trusted_vault/proto/local_domains_data.pb.h"
#include "components/trusted_vault/proto/local_trusted_vault.pb.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/standalone_trusted_vault_server_constants.h"
#include "components/trusted_vault/test/fake_local_domains_storage_file_access.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "crypto/hash.h"
#include "crypto/obsolete/md5.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

namespace {

using testing::ElementsAre;

bool WriteLegacyTrustedVaultFile(
    const trusted_vault_pb::LocalTrustedVault& proto,
    const base::FilePath& path) {
  trusted_vault_pb::LocalTrustedVaultFileContent file_proto;
  file_proto.set_serialized_local_trusted_vault(proto.SerializeAsString());
  file_proto.set_md5_digest_hex_string(
      MD5StringForTrustedVault(file_proto.serialized_local_trusted_vault()));
  if (base::FeatureList::IsEnabled(kEnableTrustedVaultSHA256)) {
    file_proto.set_sha256_digest_hex_string(
        base::Base64Encode(crypto::hash::Sha256(
            base::as_byte_span(file_proto.serialized_local_trusted_vault()))));
  }
  return base::WriteFile(path, file_proto.SerializeAsString());
}

class LocalDomainsStorageTest : public testing::Test {
 public:
  LocalDomainsStorageTest() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    auto file_access = std::make_unique<FakeLocalDomainsStorageFileAccess>();
    file_access_ = file_access.get();
    storage_ = LocalDomainsStorage::CreateForTesting(std::move(file_access));
    storage_->ReadDataFromDisk();
  }

  LocalDomainsStorage* storage() { return storage_.get(); }
  FakeLocalDomainsStorageFileAccess* file_access() { return file_access_; }
  GaiaId test_gaia() { return GaiaId("test_gaia_id"); }

 private:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<LocalDomainsStorage> storage_;
  raw_ptr<FakeLocalDomainsStorageFileAccess> file_access_;
};

TEST_F(LocalDomainsStorageTest, ShouldStoreAndRetrieveKeysPerDomain) {
  const std::vector<std::vector<uint8_t>> kSyncKeys = {{1, 2, 3}};
  const std::vector<std::vector<uint8_t>> kPasskeyKeys = {{4, 5, 6}, {7, 8}};

  storage()->SetVaultKeys(test_gaia(), SecurityDomainId::kChromeSync, kSyncKeys,
                          /*last_key_version=*/1);
  storage()->SetVaultKeys(test_gaia(), SecurityDomainId::kPasskeys,
                          kPasskeyKeys, /*last_key_version=*/2);

  EXPECT_EQ(storage()->GetVaultKeys(test_gaia(), SecurityDomainId::kChromeSync),
            kSyncKeys);
  EXPECT_EQ(
      storage()->GetLastKeyVersion(test_gaia(), SecurityDomainId::kChromeSync),
      1);

  EXPECT_EQ(storage()->GetVaultKeys(test_gaia(), SecurityDomainId::kPasskeys),
            kPasskeyKeys);
  EXPECT_EQ(
      storage()->GetLastKeyVersion(test_gaia(), SecurityDomainId::kPasskeys),
      2);
}

TEST_F(LocalDomainsStorageTest, ShouldSharePhysicalDevicePrivateKey) {
  PhysicalDeviceRecoveryFactorData factor_data;
  factor_data.set_private_key_material("shared_private_key");

  storage()->MutatePhysicalDeviceRecoveryFactorData(
      test_gaia(),
      [&](PhysicalDeviceRecoveryFactorData& data) { data = factor_data; });

  const PhysicalDeviceRecoveryFactorData retrieved =
      storage()->GetPhysicalDeviceRecoveryFactorData(test_gaia());
  EXPECT_EQ(retrieved.private_key_material(), "shared_private_key");
}

TEST_F(LocalDomainsStorageTest,
       ShouldTrackRecoveryFactorRegistrationPerDomain) {
  EXPECT_FALSE(storage()->IsRecoveryFactorRegistered(
      test_gaia(), SecurityDomainId::kChromeSync,
      LocalRecoveryFactorType::kPhysicalDevice));
  EXPECT_FALSE(storage()->IsRecoveryFactorRegistered(
      test_gaia(), SecurityDomainId::kPasskeys,
      LocalRecoveryFactorType::kPhysicalDevice));

  storage()->SetRecoveryFactorRegistered(
      test_gaia(), SecurityDomainId::kChromeSync,
      LocalRecoveryFactorType::kPhysicalDevice, true);

  EXPECT_TRUE(storage()->IsRecoveryFactorRegistered(
      test_gaia(), SecurityDomainId::kChromeSync,
      LocalRecoveryFactorType::kPhysicalDevice));
  EXPECT_FALSE(storage()->IsRecoveryFactorRegistered(
      test_gaia(), SecurityDomainId::kPasskeys,
      LocalRecoveryFactorType::kPhysicalDevice));

  storage()->SetRecoveryFactorRegistered(
      test_gaia(), SecurityDomainId::kPasskeys,
      LocalRecoveryFactorType::kPhysicalDevice, true);
  EXPECT_TRUE(storage()->IsRecoveryFactorRegistered(
      test_gaia(), SecurityDomainId::kPasskeys,
      LocalRecoveryFactorType::kPhysicalDevice));
}

TEST_F(LocalDomainsStorageTest, ShouldSupportDomainThrottling) {
  const int64_t kSyncMillis = 2000000;
  const int64_t kPasskeysMillis = 3000000;

  storage()->SetLastFailedRequestMillis(
      test_gaia(), SecurityDomainId::kChromeSync, kSyncMillis);
  storage()->SetLastFailedRequestMillis(
      test_gaia(), SecurityDomainId::kPasskeys, kPasskeysMillis);

  EXPECT_EQ(storage()->GetLastFailedRequestMillis(
                test_gaia(), SecurityDomainId::kChromeSync),
            kSyncMillis);
  EXPECT_EQ(storage()->GetLastFailedRequestMillis(test_gaia(),
                                                  SecurityDomainId::kPasskeys),
            kPasskeysMillis);
}

TEST_F(LocalDomainsStorageTest, ShouldClearDataForUser) {
  GaiaId gaia1("user1");
  GaiaId gaia2("user2");

  storage()->SetVaultKeys(gaia1, SecurityDomainId::kChromeSync, {{1, 2}}, 1);
  storage()->SetVaultKeys(gaia2, SecurityDomainId::kChromeSync, {{3, 4}}, 1);

  storage()->ClearDataForUser(gaia1);

  EXPECT_TRUE(
      storage()->GetVaultKeys(gaia1, SecurityDomainId::kChromeSync).empty());
  EXPECT_FALSE(
      storage()->GetVaultKeys(gaia2, SecurityDomainId::kChromeSync).empty());
}

TEST_F(LocalDomainsStorageTest,
       ShouldClearDataForUnknownUsersOrMarkForDeletion) {
  GaiaId primary_gaia("primary_user");
  GaiaId secondary_gaia("secondary_user");

  storage()->SetVaultKeys(primary_gaia, SecurityDomainId::kChromeSync, {{1, 2}},
                          1);
  storage()->SetVaultKeys(secondary_gaia, SecurityDomainId::kChromeSync,
                          {{3, 4}}, 1);

  // Neither user is in known_gaia_ids. Primary should be marked for deletion,
  // secondary cleared.
  storage()->ClearDataForUnknownUsersOrMarkForDeletion(
      /*known_gaia_ids=*/{}, /*primary_account_gaia_id=*/primary_gaia);

  EXPECT_FALSE(storage()
                   ->GetVaultKeys(primary_gaia, SecurityDomainId::kChromeSync)
                   .empty());
  EXPECT_TRUE(storage()
                  ->GetVaultKeys(secondary_gaia, SecurityDomainId::kChromeSync)
                  .empty());
}

TEST_F(LocalDomainsStorageTest, ShouldClearDataForUsersMarkedForDeletion) {
  GaiaId user_gaia("user_to_delete");

  storage()->SetVaultKeys(user_gaia, SecurityDomainId::kChromeSync, {{1, 2}},
                          1);
  storage()->ClearDataForUnknownUsersOrMarkForDeletion(
      /*known_gaia_ids=*/{}, /*primary_account_gaia_id=*/user_gaia);

  // Now user_gaia is no longer the primary account.
  storage()->ClearDataForUsersMarkedForDeletion(
      /*primary_account_gaia_id=*/std::nullopt);

  EXPECT_TRUE(storage()
                  ->GetVaultKeys(user_gaia, SecurityDomainId::kChromeSync)
                  .empty());
}

class LocalDomainsStorageDiskTest : public testing::Test {
 public:
  LocalDomainsStorageDiskTest() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  const base::FilePath& base_dir() const { return temp_dir_.GetPath(); }
  base::FilePath local_domains_file_path() const {
    return base_dir().Append(FILE_PATH_LITERAL("local_domains_data.pb"));
  }
  base::FilePath legacy_file_path() const {
    return base_dir().Append(FILE_PATH_LITERAL("trusted_vault.pb"));
  }

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(LocalDomainsStorageDiskTest,
       ShouldRecordNotFoundAndWriteEmptyFileWhenNoLegacyFile) {
  auto storage = LocalDomainsStorage::Create(base_dir());
  base::HistogramTester histogram_tester;
  storage->ReadDataFromDisk();

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.FileReadStatus",
      TrustedVaultFileReadStatusForUMA::kNotFound, 1);
  EXPECT_TRUE(base::PathExists(local_domains_file_path()));
}

TEST_F(LocalDomainsStorageDiskTest,
       ShouldRecordDataProtoDeserializationFailedWhenReadingFile) {
  ASSERT_TRUE(
      base::WriteFile(local_domains_file_path(), "corrupted_data_proto"));

  auto storage = LocalDomainsStorage::Create(base_dir());
  base::HistogramTester histogram_tester;
  storage->ReadDataFromDisk();

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.FileReadStatus",
      TrustedVaultFileReadStatusForUMA::kDataProtoDeserializationFailed, 1);
}

TEST_F(LocalDomainsStorageDiskTest, ShouldWriteAndReadLocalDomainsDataOnDisk) {
  const GaiaId kGaiaId("user1");
  const std::vector<std::vector<uint8_t>> kKeys = {{1, 2, 3}, {4, 5, 6}};

  {
    auto storage = LocalDomainsStorage::Create(base_dir());
    storage->ReadDataFromDisk();

    base::HistogramTester write_histogram_tester;
    storage->SetVaultKeys(kGaiaId, SecurityDomainId::kPasskeys, kKeys,
                          /*last_key_version=*/5);
    write_histogram_tester.ExpectUniqueSample("TrustedVault.FileWriteSuccess",
                                              true, 1);
  }

  ASSERT_TRUE(base::PathExists(local_domains_file_path()));

  auto reloaded_storage = LocalDomainsStorage::Create(base_dir());
  base::HistogramTester read_histogram_tester;
  reloaded_storage->ReadDataFromDisk();

  read_histogram_tester.ExpectUniqueSample(
      "TrustedVault.FileReadStatus", TrustedVaultFileReadStatusForUMA::kSuccess,
      1);

  EXPECT_EQ(
      reloaded_storage->GetVaultKeys(kGaiaId, SecurityDomainId::kPasskeys),
      kKeys);
  EXPECT_EQ(
      reloaded_storage->GetLastKeyVersion(kGaiaId, SecurityDomainId::kPasskeys),
      5);
}

TEST_F(LocalDomainsStorageDiskTest, ShouldClearDataOnVersionMismatch) {
  const GaiaId kGaiaId("user1");

  // Write data with a higher version than supported (e.g., after a Chrome
  // downgrade).
  trusted_vault_pb::LocalDomainsData future_data;
  future_data.set_data_version(2);
  trusted_vault_pb::UserDomainData* user = future_data.add_user();
  user->set_gaia_id(kGaiaId.ToString());
  trusted_vault_pb::DomainData* domain = user->add_domain_data();
  domain->set_domain_id(static_cast<int32_t>(SecurityDomainId::kChromeSync));
  domain->set_last_vault_key_version(5);
  domain->add_vault_key()->set_key_material("key");
  ASSERT_TRUE(base::WriteFile(local_domains_file_path(),
                              future_data.SerializeAsString()));

  auto storage = LocalDomainsStorage::Create(base_dir());
  base::HistogramTester histogram_tester;
  storage->ReadDataFromDisk();

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.FileReadStatus",
      TrustedVaultFileReadStatusForUMA::kUnsupportedVersion, 1);
  EXPECT_TRUE(
      storage->GetVaultKeys(kGaiaId, SecurityDomainId::kChromeSync).empty());
  EXPECT_EQ(storage->GetLastKeyVersion(kGaiaId, SecurityDomainId::kChromeSync),
            0);
}

TEST_F(LocalDomainsStorageDiskTest, ShouldMigrateLegacyTrustedVaultData) {
  trusted_vault_pb::LocalTrustedVault legacy_data;
  legacy_data.set_data_version(4);
  auto* user = legacy_data.add_user();
  user->set_gaia_id("migrated_user");
  user->set_last_vault_key_version(10);
  user->set_last_failed_request_millis_since_unix_epoch(1122334455);
  user->mutable_local_device_registration_info()->set_private_key_material(
      "migrated_private_key");
  user->mutable_local_device_registration_info()->set_device_registered(true);
  user->mutable_local_device_registration_info()->set_device_registered_version(
      1);

  ASSERT_TRUE(WriteLegacyTrustedVaultFile(legacy_data, legacy_file_path()));

  auto storage = LocalDomainsStorage::Create(base_dir());
  base::HistogramTester histogram_tester;
  storage->ReadDataFromDisk();

  histogram_tester.ExpectUniqueSample(
      "TrustedVault.FileReadStatus",
      TrustedVaultFileReadStatusForUMA::kNotFound, 1);
  histogram_tester.ExpectUniqueSample(
      "TrustedVault.FileReadStatus.ChromeSync",
      TrustedVaultFileReadStatusForUMA::kSuccess, 1);
  histogram_tester.ExpectUniqueSample("TrustedVault.FileWriteSuccess", true, 1);
  EXPECT_TRUE(base::PathExists(local_domains_file_path()));

  GaiaId gaia_id("migrated_user");
  EXPECT_EQ(storage->GetLastKeyVersion(gaia_id, SecurityDomainId::kChromeSync),
            10);
  EXPECT_EQ(storage->GetLastFailedRequestMillis(gaia_id,
                                                SecurityDomainId::kChromeSync),
            1122334455);

  const PhysicalDeviceRecoveryFactorData factor_data =
      storage->GetPhysicalDeviceRecoveryFactorData(gaia_id);
  EXPECT_EQ(factor_data.private_key_material(), "migrated_private_key");
  EXPECT_TRUE(storage->IsRecoveryFactorRegistered(
      gaia_id, SecurityDomainId::kChromeSync,
      LocalRecoveryFactorType::kPhysicalDevice));

  // Verify subsequent reads load from `local_domains_data.pb` without
  // re-migrating.
  auto reloaded_storage = LocalDomainsStorage::Create(base_dir());
  base::HistogramTester reload_histogram_tester;
  reloaded_storage->ReadDataFromDisk();
  reload_histogram_tester.ExpectUniqueSample(
      "TrustedVault.FileReadStatus", TrustedVaultFileReadStatusForUMA::kSuccess,
      1);
}

TEST_F(LocalDomainsStorageDiskTest,
       ShouldApplyLegacyVersionUpgradesDuringMigration) {
  // Create a version 0 legacy file with a single non-constant key, stale keys
  // set to true, device_registered_version = 0, and deprecated obsolete flag.
  trusted_vault_pb::LocalTrustedVault legacy_data;
  legacy_data.set_data_version(0);
  auto* user = legacy_data.add_user();
  user->set_gaia_id("legacy_v0_user");
  const std::vector<uint8_t> kNonConstantKey = {9, 8, 7};
  AssignBytesToProtoString(kNonConstantKey,
                           user->add_vault_key()->mutable_key_material());
  user->set_keys_marked_as_stale_by_consumer(true);
  user->mutable_local_device_registration_info()->set_device_registered(true);
  user->mutable_local_device_registration_info()->set_device_registered_version(
      0);
  user->mutable_local_device_registration_info()
      ->set_deprecated_last_registration_returned_local_data_obsolete(true);

  ASSERT_TRUE(WriteLegacyTrustedVaultFile(legacy_data, legacy_file_path()));

  auto storage = LocalDomainsStorage::Create(base_dir());
  storage->ReadDataFromDisk();

  const GaiaId gaia_id("legacy_v0_user");
  // UpgradeToVersion1 injects the constant key before the single non-constant
  // key.
  EXPECT_THAT(storage->GetVaultKeys(gaia_id, SecurityDomainId::kChromeSync),
              ElementsAre(GetConstantTrustedVaultKey(), kNonConstantKey));
  // UpgradeToVersion2 resets keys_marked_as_stale_by_consumer to false.
  EXPECT_FALSE(storage->GetKeysMarkedAsStaleByConsumer(
      gaia_id, SecurityDomainId::kChromeSync));
  // UpgradeToVersion3 resets device_registered to false when
  // device_registered_version == 0.
  EXPECT_FALSE(storage->IsRecoveryFactorRegistered(
      gaia_id, SecurityDomainId::kChromeSync,
      LocalRecoveryFactorType::kPhysicalDevice));
  // UpgradeToVersion4 migrates
  // deprecated_last_registration_returned_local_data_obsolete.
  EXPECT_TRUE(storage->GetLastRegistrationReturnedLocalDataObsolete(
      gaia_id, SecurityDomainId::kChromeSync));
}

}  // namespace

}  // namespace trusted_vault
