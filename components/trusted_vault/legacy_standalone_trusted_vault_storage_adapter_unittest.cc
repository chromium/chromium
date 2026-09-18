// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/legacy_standalone_trusted_vault_storage_adapter.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "components/trusted_vault/local_recovery_factor.h"
#include "components/trusted_vault/proto/local_trusted_vault.pb.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/standalone_trusted_vault_server_constants.h"
#include "components/trusted_vault/test/legacy_fake_file_access.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

namespace {

using testing::ElementsAre;
using testing::Eq;

class LegacyStandaloneTrustedVaultStorageAdapterTest : public testing::Test {
 public:
  LegacyStandaloneTrustedVaultStorageAdapterTest() {
    auto file_access = std::make_unique<LegacyFakeFileAccess>();
    file_access_ = file_access.get();
    adapter_ = std::make_unique<LegacyStandaloneTrustedVaultStorageAdapter>(
        LegacyStandaloneTrustedVaultStorage::CreateForTesting(
            std::move(file_access)));
  }

  LegacyStandaloneTrustedVaultStorageAdapter* adapter() {
    return adapter_.get();
  }

  LegacyFakeFileAccess* file_access() { return file_access_; }

 private:
  std::unique_ptr<LegacyStandaloneTrustedVaultStorageAdapter> adapter_;
  raw_ptr<LegacyFakeFileAccess> file_access_ = nullptr;
};

TEST_F(LegacyStandaloneTrustedVaultStorageAdapterTest, ShouldReadDataFromDisk) {
  const GaiaId kGaiaId("user1");
  const std::vector<uint8_t> kKey = {1, 2, 3};

  trusted_vault_pb::LocalTrustedVault initial_data;
  initial_data.set_data_version(4);
  trusted_vault_pb::LocalTrustedVaultPerUser* user_vault =
      initial_data.add_user();
  user_vault->set_gaia_id(kGaiaId.ToString());
  user_vault->set_last_vault_key_version(3);
  AssignBytesToProtoString(kKey,
                           user_vault->add_vault_key()->mutable_key_material());
  file_access()->SetStoredLocalTrustedVault(initial_data);

  adapter()->ReadDataFromDisk();

  EXPECT_THAT(adapter()->GetVaultKeys(kGaiaId, SecurityDomainId::kChromeSync),
              ElementsAre(kKey));
  EXPECT_EQ(
      adapter()->GetLastKeyVersion(kGaiaId, SecurityDomainId::kChromeSync), 3);
}

TEST_F(LegacyStandaloneTrustedVaultStorageAdapterTest, ShouldClearDataForUser) {
  const GaiaId kGaiaId1("user1");
  const GaiaId kGaiaId2("user2");

  trusted_vault_pb::LocalTrustedVault initial_data;
  initial_data.set_data_version(4);
  initial_data.add_user()->set_gaia_id(kGaiaId1.ToString());
  initial_data.add_user()->set_gaia_id(kGaiaId2.ToString());
  file_access()->SetStoredLocalTrustedVault(initial_data);
  adapter()->ReadDataFromDisk();

  adapter()->ClearDataForUser(kGaiaId1);

  trusted_vault_pb::LocalTrustedVault stored_data =
      file_access()->GetStoredLocalTrustedVault();
  ASSERT_THAT(stored_data.user_size(), Eq(1));
  EXPECT_EQ(stored_data.user(0).gaia_id(), kGaiaId2.ToString());
}

TEST_F(LegacyStandaloneTrustedVaultStorageAdapterTest,
       ShouldClearDataForUnknownUsersOrMarkForDeletion) {
  const GaiaId kGaiaId1("user1");
  const GaiaId kGaiaId2("user2");

  trusted_vault_pb::LocalTrustedVault initial_data;
  initial_data.set_data_version(4);
  initial_data.add_user()->set_gaia_id(kGaiaId1.ToString());
  initial_data.add_user()->set_gaia_id(kGaiaId2.ToString());
  file_access()->SetStoredLocalTrustedVault(initial_data);
  adapter()->ReadDataFromDisk();

  // Neither user is in known gaia ids. Primary account (kGaiaId1) should be
  // marked for deletion, and unknown non-primary account (kGaiaId2) should be
  // deleted immediately.
  adapter()->ClearDataForUnknownUsersOrMarkForDeletion(
      /*known_gaia_ids=*/{}, /*primary_account_gaia_id=*/kGaiaId1);

  trusted_vault_pb::LocalTrustedVault stored_data =
      file_access()->GetStoredLocalTrustedVault();
  ASSERT_THAT(stored_data.user_size(), Eq(1));
  EXPECT_EQ(stored_data.user(0).gaia_id(), kGaiaId1.ToString());
  EXPECT_TRUE(stored_data.user(0).should_delete_keys_when_non_primary());
}

TEST_F(LegacyStandaloneTrustedVaultStorageAdapterTest,
       ShouldClearDataForUsersMarkedForDeletion) {
  const GaiaId kGaiaId("user1");

  trusted_vault_pb::LocalTrustedVault initial_data;
  initial_data.set_data_version(4);
  trusted_vault_pb::LocalTrustedVaultPerUser* user = initial_data.add_user();
  user->set_gaia_id(kGaiaId.ToString());
  user->set_should_delete_keys_when_non_primary(true);
  file_access()->SetStoredLocalTrustedVault(initial_data);
  adapter()->ReadDataFromDisk();

  // User is not primary account anymore.
  adapter()->ClearDataForUsersMarkedForDeletion(
      /*primary_account_gaia_id=*/std::nullopt);

  trusted_vault_pb::LocalTrustedVault stored_data =
      file_access()->GetStoredLocalTrustedVault();
  EXPECT_EQ(stored_data.user_size(), 0);
}

TEST_F(LegacyStandaloneTrustedVaultStorageAdapterTest,
       ShouldGetAndSetPhysicalDeviceRegistration) {
  const GaiaId kGaiaId("user1");

  // Read verification via LegacyFakeFileAccess.
  trusted_vault_pb::LocalTrustedVault initial_data;
  initial_data.set_data_version(4);
  trusted_vault_pb::LocalTrustedVaultPerUser* user = initial_data.add_user();
  user->set_gaia_id(kGaiaId.ToString());
  user->mutable_local_device_registration_info()->set_device_registered(true);
  user->mutable_local_device_registration_info()->set_device_registered_version(
      1);
  file_access()->SetStoredLocalTrustedVault(initial_data);
  adapter()->ReadDataFromDisk();

  EXPECT_TRUE(adapter()->IsRecoveryFactorRegistered(
      kGaiaId, SecurityDomainId::kChromeSync,
      LocalRecoveryFactorType::kPhysicalDevice));

  // Write verification via LegacyFakeFileAccess (unregister).
  adapter()->SetRecoveryFactorRegistered(
      kGaiaId, SecurityDomainId::kChromeSync,
      LocalRecoveryFactorType::kPhysicalDevice, /*registered=*/false);

  trusted_vault_pb::LocalTrustedVault stored_data =
      file_access()->GetStoredLocalTrustedVault();
  ASSERT_THAT(stored_data.user_size(), Eq(1));
  EXPECT_FALSE(
      stored_data.user(0).local_device_registration_info().device_registered());
  EXPECT_FALSE(stored_data.user(0)
                   .local_device_registration_info()
                   .has_device_registered_version());

  // Write verification via LegacyFakeFileAccess (register).
  adapter()->SetRecoveryFactorRegistered(
      kGaiaId, SecurityDomainId::kChromeSync,
      LocalRecoveryFactorType::kPhysicalDevice, /*registered=*/true);

  stored_data = file_access()->GetStoredLocalTrustedVault();
  ASSERT_THAT(stored_data.user_size(), Eq(1));
  EXPECT_TRUE(
      stored_data.user(0).local_device_registration_info().device_registered());
  EXPECT_EQ(stored_data.user(0)
                .local_device_registration_info()
                .device_registered_version(),
            1);
}

#if BUILDFLAG(IS_MAC)
TEST_F(LegacyStandaloneTrustedVaultStorageAdapterTest,
       ShouldGetAndSetICloudKeychainRegistration) {
  const GaiaId kGaiaId("user1");

  // Read verification via LegacyFakeFileAccess.
  trusted_vault_pb::LocalTrustedVault initial_data;
  initial_data.set_data_version(4);
  trusted_vault_pb::LocalTrustedVaultPerUser* user = initial_data.add_user();
  user->set_gaia_id(kGaiaId.ToString());
  user->mutable_icloud_keychain_registration_info()->set_registered(true);
  file_access()->SetStoredLocalTrustedVault(initial_data);
  adapter()->ReadDataFromDisk();

  EXPECT_TRUE(adapter()->IsRecoveryFactorRegistered(
      kGaiaId, SecurityDomainId::kChromeSync,
      LocalRecoveryFactorType::kICloudKeychain));

  // Write verification via LegacyFakeFileAccess.
  adapter()->SetRecoveryFactorRegistered(
      kGaiaId, SecurityDomainId::kChromeSync,
      LocalRecoveryFactorType::kICloudKeychain, /*registered=*/false);

  trusted_vault_pb::LocalTrustedVault stored_data =
      file_access()->GetStoredLocalTrustedVault();
  ASSERT_THAT(stored_data.user_size(), Eq(1));
  EXPECT_FALSE(
      stored_data.user(0).icloud_keychain_registration_info().registered());
}
#endif

TEST_F(LegacyStandaloneTrustedVaultStorageAdapterTest,
       ShouldGetAndSetLastRegistrationReturnedLocalDataObsolete) {
  const GaiaId kGaiaId("user1");

  // Read verification.
  trusted_vault_pb::LocalTrustedVault initial_data;
  initial_data.set_data_version(4);
  trusted_vault_pb::LocalTrustedVaultPerUser* user = initial_data.add_user();
  user->set_gaia_id(kGaiaId.ToString());
  user->set_last_registration_returned_local_data_obsolete(true);
  file_access()->SetStoredLocalTrustedVault(initial_data);
  adapter()->ReadDataFromDisk();

  EXPECT_TRUE(adapter()->GetLastRegistrationReturnedLocalDataObsolete(
      kGaiaId, SecurityDomainId::kChromeSync));

  // Write verification.
  adapter()->SetLastRegistrationReturnedLocalDataObsolete(
      kGaiaId, SecurityDomainId::kChromeSync, /*obsolete=*/false);

  trusted_vault_pb::LocalTrustedVault stored_data =
      file_access()->GetStoredLocalTrustedVault();
  ASSERT_THAT(stored_data.user_size(), Eq(1));
  EXPECT_FALSE(
      stored_data.user(0).last_registration_returned_local_data_obsolete());
}

TEST_F(LegacyStandaloneTrustedVaultStorageAdapterTest,
       ShouldGetAndMutateLocalDeviceRegistrationInfo) {
  const GaiaId kGaiaId("user1");
  const std::string kKeyMaterial = "test_key_pair";

  // Read verification.
  trusted_vault_pb::LocalTrustedVault initial_data;
  initial_data.set_data_version(4);
  trusted_vault_pb::LocalTrustedVaultPerUser* user = initial_data.add_user();
  user->set_gaia_id(kGaiaId.ToString());
  user->mutable_local_device_registration_info()->set_private_key_material(
      kKeyMaterial);
  file_access()->SetStoredLocalTrustedVault(initial_data);
  adapter()->ReadDataFromDisk();

  EXPECT_EQ(
      adapter()->GetLocalDeviceRegistrationInfo(kGaiaId).private_key_material(),
      kKeyMaterial);

  // Write / Mutate verification.
  const std::string kNewKeyMaterial = "mutated_key_pair";
  adapter()->MutateLocalDeviceRegistrationInfo(
      kGaiaId, [&](LocalDeviceRegistrationInfo& info) {
        info.set_private_key_material(kNewKeyMaterial);
      });

  trusted_vault_pb::LocalTrustedVault stored_data =
      file_access()->GetStoredLocalTrustedVault();
  ASSERT_THAT(stored_data.user_size(), Eq(1));
  EXPECT_EQ(stored_data.user(0)
                .local_device_registration_info()
                .private_key_material(),
            kNewKeyMaterial);
}

TEST_F(LegacyStandaloneTrustedVaultStorageAdapterTest,
       ShouldGetAndSetVaultKeys) {
  const GaiaId kGaiaId("user1");
  const std::vector<uint8_t> kKey1 = {1, 2, 3};
  const std::vector<uint8_t> kKey2 = {4, 5, 6};

  // Write verification.
  adapter()->SetVaultKeys(kGaiaId, SecurityDomainId::kChromeSync,
                          {kKey1, kKey2},
                          /*last_key_version=*/7);

  trusted_vault_pb::LocalTrustedVault stored_data =
      file_access()->GetStoredLocalTrustedVault();
  ASSERT_THAT(stored_data.user_size(), Eq(1));
  EXPECT_EQ(stored_data.user(0).last_vault_key_version(), 7);
  ASSERT_THAT(stored_data.user(0).vault_key_size(), Eq(2));
  EXPECT_EQ(ProtoStringToBytes(stored_data.user(0).vault_key(0).key_material()),
            kKey1);
  EXPECT_EQ(ProtoStringToBytes(stored_data.user(0).vault_key(1).key_material()),
            kKey2);

  // Read verification.
  EXPECT_THAT(adapter()->GetVaultKeys(kGaiaId, SecurityDomainId::kChromeSync),
              ElementsAre(kKey1, kKey2));
  EXPECT_EQ(
      adapter()->GetLastKeyVersion(kGaiaId, SecurityDomainId::kChromeSync), 7);
}

TEST_F(LegacyStandaloneTrustedVaultStorageAdapterTest,
       ShouldGetAndSetKeysMarkedAsStaleByConsumer) {
  const GaiaId kGaiaId("user1");

  // Read verification.
  trusted_vault_pb::LocalTrustedVault initial_data;
  initial_data.set_data_version(4);
  trusted_vault_pb::LocalTrustedVaultPerUser* user = initial_data.add_user();
  user->set_gaia_id(kGaiaId.ToString());
  user->set_keys_marked_as_stale_by_consumer(true);
  file_access()->SetStoredLocalTrustedVault(initial_data);
  adapter()->ReadDataFromDisk();

  EXPECT_TRUE(adapter()->GetKeysMarkedAsStaleByConsumer(
      kGaiaId, SecurityDomainId::kChromeSync));

  // Write verification.
  adapter()->SetKeysMarkedAsStaleByConsumer(
      kGaiaId, SecurityDomainId::kChromeSync, /*stale=*/false);

  trusted_vault_pb::LocalTrustedVault stored_data =
      file_access()->GetStoredLocalTrustedVault();
  ASSERT_THAT(stored_data.user_size(), Eq(1));
  EXPECT_FALSE(stored_data.user(0).keys_marked_as_stale_by_consumer());
}

TEST_F(LegacyStandaloneTrustedVaultStorageAdapterTest,
       ShouldDetectHasNonConstantKey) {
  const GaiaId kGaiaId("user1");

  // Constant key only -> false.
  trusted_vault_pb::LocalTrustedVault initial_data;
  initial_data.set_data_version(4);
  trusted_vault_pb::LocalTrustedVaultPerUser* user = initial_data.add_user();
  user->set_gaia_id(kGaiaId.ToString());
  AssignBytesToProtoString(GetConstantTrustedVaultKey(),
                           user->add_vault_key()->mutable_key_material());
  file_access()->SetStoredLocalTrustedVault(initial_data);
  adapter()->ReadDataFromDisk();

  EXPECT_FALSE(
      adapter()->HasNonConstantKey(kGaiaId, SecurityDomainId::kChromeSync));

  // Non-constant key present -> true.
  adapter()->SetVaultKeys(kGaiaId, SecurityDomainId::kChromeSync,
                          {{10, 20, 30}}, /*last_key_version=*/1);
  EXPECT_TRUE(
      adapter()->HasNonConstantKey(kGaiaId, SecurityDomainId::kChromeSync));
}

TEST_F(LegacyStandaloneTrustedVaultStorageAdapterTest,
       ShouldGetAndSetLastFailedRequestMillis) {
  const GaiaId kGaiaId("user1");
  const int64_t kTimestamp = 123456789;

  // Write verification.
  adapter()->SetLastFailedRequestMillis(kGaiaId, SecurityDomainId::kChromeSync,
                                        kTimestamp);

  trusted_vault_pb::LocalTrustedVault stored_data =
      file_access()->GetStoredLocalTrustedVault();
  ASSERT_THAT(stored_data.user_size(), Eq(1));
  EXPECT_EQ(stored_data.user(0).last_failed_request_millis_since_unix_epoch(),
            kTimestamp);

  // Read verification.
  EXPECT_EQ(adapter()->GetLastFailedRequestMillis(
                kGaiaId, SecurityDomainId::kChromeSync),
            kTimestamp);
}

TEST_F(LegacyStandaloneTrustedVaultStorageAdapterTest,
       ShouldGetAndSetDegradedRecoverabilityState) {
  const GaiaId kGaiaId("user1");
  trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState state;
  state.set_degraded_recoverability_value(
      trusted_vault_pb::DegradedRecoverabilityValue::kDegraded);

  // Write verification.
  adapter()->SetDegradedRecoverabilityState(
      kGaiaId, SecurityDomainId::kChromeSync, state);

  trusted_vault_pb::LocalTrustedVault stored_data =
      file_access()->GetStoredLocalTrustedVault();
  ASSERT_THAT(stored_data.user_size(), Eq(1));
  EXPECT_EQ(stored_data.user(0)
                .degraded_recoverability_state()
                .degraded_recoverability_value(),
            trusted_vault_pb::DegradedRecoverabilityValue::kDegraded);

  // Read verification.
  EXPECT_EQ(adapter()
                ->GetDegradedRecoverabilityState(kGaiaId,
                                                 SecurityDomainId::kChromeSync)
                .degraded_recoverability_value(),
            trusted_vault_pb::DegradedRecoverabilityValue::kDegraded);
}

}  // namespace

}  // namespace trusted_vault
