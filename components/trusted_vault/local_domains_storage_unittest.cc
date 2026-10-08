// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/local_domains_storage.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/trusted_vault/local_recovery_factor.h"
#include "components/trusted_vault/proto/local_domains_data.pb.h"
#include "components/trusted_vault/test/fake_local_domains_storage_file_access.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

namespace {

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

 private:
  base::ScopedTempDir temp_dir_;
};

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

}  // namespace

}  // namespace trusted_vault
