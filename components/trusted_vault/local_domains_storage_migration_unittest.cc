// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/trusted_vault/features.h"
#include "components/trusted_vault/legacy_standalone_trusted_vault_storage.h"
#include "components/trusted_vault/legacy_standalone_trusted_vault_storage_adapter.h"
#include "components/trusted_vault/local_domains_storage.h"
#include "components/trusted_vault/local_recovery_factor.h"
#include "components/trusted_vault/proto/local_domains_data.pb.h"
#include "components/trusted_vault/proto/local_trusted_vault.pb.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/standalone_trusted_vault_server_constants.h"
#include "components/trusted_vault/standalone_trusted_vault_storage.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "crypto/hash.h"
#include "crypto/obsolete/md5.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace trusted_vault {

namespace {

constexpr int kNumCandidateUsers = 5;

GaiaId CandidateGaiaId(int index) {
  return GaiaId("user_" + base::NumberToString(index));
}

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

struct FuzzedUserVault {
  int gaia_id_index = 0;
  bool prepend_constant_key = false;
  std::vector<std::vector<uint8_t>> vault_keys;
  std::optional<int> last_vault_key_version;
  std::optional<bool> keys_marked_as_stale_by_consumer;
  bool has_local_device_registration_info = false;
  std::optional<std::string> private_key_material;
  std::optional<bool> device_registered;
  std::optional<int> device_registered_version;
  std::optional<bool> deprecated_last_registration_returned_local_data_obsolete;
  std::optional<bool> last_registration_returned_local_data_obsolete;
  bool has_icloud_keychain_registration_info = false;
  std::optional<bool> icloud_keychain_registered;
  std::optional<int64_t> last_failed_request_millis_since_unix_epoch;
  bool has_degraded_recoverability_state = false;
  std::optional<int> degraded_recoverability_value;
  std::optional<int64_t> last_refresh_time_millis_since_unix_epoch;
  std::optional<bool> should_delete_keys_when_non_primary;
};

enum class MutationType {
  kSetVaultKeys,
  kSetKeysMarkedAsStaleByConsumer,
  kMutatePhysicalDeviceRecoveryFactorData,
  kSetPhysicalDeviceRegistered,
#if BUILDFLAG(IS_MAC)
  kSetICloudKeychainRegistered,
#endif
  kSetLastRegistrationReturnedLocalDataObsolete,
  kSetLastFailedRequestMillis,
  kSetDegradedRecoverabilityState,
  kClearDataForUser,
  kClearDataForUnknownUsersOrMarkForDeletion,
  kClearDataForUsersMarkedForDeletion,
};

struct FuzzedMutation {
  MutationType type = MutationType::kSetVaultKeys;
  int target_gaia_id_index = 0;
  bool bool_arg = false;
  int int_arg = 0;
  int64_t int64_arg = 0;
  std::optional<std::string> optional_string_arg;
  std::vector<std::vector<uint8_t>> keys_arg;
  std::vector<int> known_gaia_id_indices;
  std::optional<int> primary_account_index;
};

trusted_vault_pb::LocalTrustedVault BuildLegacyProto(
    int data_version,
    const std::vector<FuzzedUserVault>& users) {
  trusted_vault_pb::LocalTrustedVault proto;
  proto.set_data_version(data_version);

  base::flat_set<int> seen_gaia_indices;
  for (const FuzzedUserVault& fuzzed_user : users) {
    if (!seen_gaia_indices.insert(fuzzed_user.gaia_id_index).second) {
      continue;
    }

    trusted_vault_pb::LocalTrustedVaultPerUser* user = proto.add_user();
    user->set_gaia_id(CandidateGaiaId(fuzzed_user.gaia_id_index).ToString());

    if (fuzzed_user.prepend_constant_key) {
      AssignBytesToProtoString(GetConstantTrustedVaultKey(),
                               user->add_vault_key()->mutable_key_material());
    }
    for (const std::vector<uint8_t>& key : fuzzed_user.vault_keys) {
      AssignBytesToProtoString(key,
                               user->add_vault_key()->mutable_key_material());
    }

    if (fuzzed_user.last_vault_key_version.has_value()) {
      user->set_last_vault_key_version(*fuzzed_user.last_vault_key_version);
    }
    if (fuzzed_user.keys_marked_as_stale_by_consumer.has_value()) {
      user->set_keys_marked_as_stale_by_consumer(
          *fuzzed_user.keys_marked_as_stale_by_consumer);
    }

    if (fuzzed_user.has_local_device_registration_info) {
      auto* reg_info = user->mutable_local_device_registration_info();
      if (fuzzed_user.private_key_material.has_value()) {
        reg_info->set_private_key_material(*fuzzed_user.private_key_material);
      }
      if (fuzzed_user.device_registered.has_value()) {
        reg_info->set_device_registered(*fuzzed_user.device_registered);
      }
      if (fuzzed_user.device_registered_version.has_value()) {
        reg_info->set_device_registered_version(
            *fuzzed_user.device_registered_version);
      }
      if (fuzzed_user.deprecated_last_registration_returned_local_data_obsolete
              .has_value()) {
        reg_info->set_deprecated_last_registration_returned_local_data_obsolete(
            *fuzzed_user
                 .deprecated_last_registration_returned_local_data_obsolete);
      }
    }

    if (fuzzed_user.last_registration_returned_local_data_obsolete
            .has_value()) {
      user->set_last_registration_returned_local_data_obsolete(
          *fuzzed_user.last_registration_returned_local_data_obsolete);
    }

    if (fuzzed_user.has_icloud_keychain_registration_info) {
      auto* icloud_info = user->mutable_icloud_keychain_registration_info();
      if (fuzzed_user.icloud_keychain_registered.has_value()) {
        icloud_info->set_registered(*fuzzed_user.icloud_keychain_registered);
      }
    }

    if (fuzzed_user.last_failed_request_millis_since_unix_epoch.has_value()) {
      user->set_last_failed_request_millis_since_unix_epoch(
          *fuzzed_user.last_failed_request_millis_since_unix_epoch);
    }

    if (fuzzed_user.has_degraded_recoverability_state) {
      auto* state = user->mutable_degraded_recoverability_state();
      if (fuzzed_user.degraded_recoverability_value.has_value()) {
        state->set_degraded_recoverability_value(
            static_cast<trusted_vault_pb::DegradedRecoverabilityValue>(
                *fuzzed_user.degraded_recoverability_value));
      }
      if (fuzzed_user.last_refresh_time_millis_since_unix_epoch.has_value()) {
        state->set_last_refresh_time_millis_since_unix_epoch(
            *fuzzed_user.last_refresh_time_millis_since_unix_epoch);
      }
    }

    if (fuzzed_user.should_delete_keys_when_non_primary.has_value()) {
      user->set_should_delete_keys_when_non_primary(
          *fuzzed_user.should_delete_keys_when_non_primary);
    }
  }

  return proto;
}

void ExpectStoragesEquivalent(const StandaloneTrustedVaultStorage& expected,
                              const StandaloneTrustedVaultStorage& actual) {
  const SecurityDomainId kDomain = SecurityDomainId::kChromeSync;
  for (int i = 0; i < kNumCandidateUsers; ++i) {
    const GaiaId gaia_id = CandidateGaiaId(i);
    SCOPED_TRACE("GaiaId: " + gaia_id.ToString());

    EXPECT_EQ(actual.GetVaultKeys(gaia_id, kDomain),
              expected.GetVaultKeys(gaia_id, kDomain));
    EXPECT_EQ(actual.GetLastKeyVersion(gaia_id, kDomain),
              expected.GetLastKeyVersion(gaia_id, kDomain));
    EXPECT_EQ(actual.HasNonConstantKey(gaia_id, kDomain),
              expected.HasNonConstantKey(gaia_id, kDomain));
    EXPECT_EQ(actual.GetKeysMarkedAsStaleByConsumer(gaia_id, kDomain),
              expected.GetKeysMarkedAsStaleByConsumer(gaia_id, kDomain));

    const PhysicalDeviceRecoveryFactorData expected_device =
        expected.GetPhysicalDeviceRecoveryFactorData(gaia_id);
    const PhysicalDeviceRecoveryFactorData actual_device =
        actual.GetPhysicalDeviceRecoveryFactorData(gaia_id);
    EXPECT_EQ(actual_device.has_private_key_material(),
              expected_device.has_private_key_material());
    EXPECT_EQ(actual_device.private_key_material(),
              expected_device.private_key_material());

    EXPECT_EQ(actual.IsRecoveryFactorRegistered(
                  gaia_id, kDomain, LocalRecoveryFactorType::kPhysicalDevice),
              expected.IsRecoveryFactorRegistered(
                  gaia_id, kDomain, LocalRecoveryFactorType::kPhysicalDevice));
#if BUILDFLAG(IS_MAC)
    EXPECT_EQ(actual.IsRecoveryFactorRegistered(
                  gaia_id, kDomain, LocalRecoveryFactorType::kICloudKeychain),
              expected.IsRecoveryFactorRegistered(
                  gaia_id, kDomain, LocalRecoveryFactorType::kICloudKeychain));
#endif

    EXPECT_EQ(
        actual.GetLastRegistrationReturnedLocalDataObsolete(gaia_id, kDomain),
        expected.GetLastRegistrationReturnedLocalDataObsolete(gaia_id,
                                                              kDomain));
    EXPECT_EQ(actual.GetLastFailedRequestMillis(gaia_id, kDomain),
              expected.GetLastFailedRequestMillis(gaia_id, kDomain));

    const auto expected_degraded =
        expected.GetDegradedRecoverabilityState(gaia_id, kDomain);
    const auto actual_degraded =
        actual.GetDegradedRecoverabilityState(gaia_id, kDomain);
    EXPECT_EQ(actual_degraded.SerializeAsString(),
              expected_degraded.SerializeAsString());
  }
}

void ApplyMutation(StandaloneTrustedVaultStorage& storage,
                   const FuzzedMutation& mutation) {
  const SecurityDomainId kDomain = SecurityDomainId::kChromeSync;
  const GaiaId gaia_id = CandidateGaiaId(mutation.target_gaia_id_index);

  switch (mutation.type) {
    case MutationType::kSetVaultKeys:
      storage.SetVaultKeys(gaia_id, kDomain, mutation.keys_arg,
                           mutation.int_arg);
      return;
    case MutationType::kSetKeysMarkedAsStaleByConsumer:
      storage.SetKeysMarkedAsStaleByConsumer(gaia_id, kDomain,
                                             mutation.bool_arg);
      return;
    case MutationType::kMutatePhysicalDeviceRecoveryFactorData:
      storage.MutatePhysicalDeviceRecoveryFactorData(
          gaia_id, [&](PhysicalDeviceRecoveryFactorData& data) {
            if (mutation.optional_string_arg.has_value()) {
              data.set_private_key_material(*mutation.optional_string_arg);
            } else {
              data.clear_private_key_material();
            }
          });
      return;
    case MutationType::kSetPhysicalDeviceRegistered:
      storage.SetRecoveryFactorRegistered(
          gaia_id, kDomain, LocalRecoveryFactorType::kPhysicalDevice,
          mutation.bool_arg);
      return;
#if BUILDFLAG(IS_MAC)
    case MutationType::kSetICloudKeychainRegistered:
      storage.SetRecoveryFactorRegistered(
          gaia_id, kDomain, LocalRecoveryFactorType::kICloudKeychain,
          mutation.bool_arg);
      return;
#endif
    case MutationType::kSetLastRegistrationReturnedLocalDataObsolete:
      storage.SetLastRegistrationReturnedLocalDataObsolete(gaia_id, kDomain,
                                                           mutation.bool_arg);
      return;
    case MutationType::kSetLastFailedRequestMillis:
      storage.SetLastFailedRequestMillis(gaia_id, kDomain, mutation.int64_arg);
      return;
    case MutationType::kSetDegradedRecoverabilityState: {
      trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState state;
      state.set_degraded_recoverability_value(
          static_cast<trusted_vault_pb::DegradedRecoverabilityValue>(
              mutation.int_arg));
      state.set_last_refresh_time_millis_since_unix_epoch(mutation.int64_arg);
      storage.SetDegradedRecoverabilityState(gaia_id, kDomain, state);
      return;
    }
    case MutationType::kClearDataForUser:
      storage.ClearDataForUser(gaia_id);
      return;
    case MutationType::kClearDataForUnknownUsersOrMarkForDeletion: {
      base::flat_set<GaiaId> known_gaia_ids;
      for (int idx : mutation.known_gaia_id_indices) {
        known_gaia_ids.insert(CandidateGaiaId(idx));
      }
      std::optional<GaiaId> primary_account;
      if (mutation.primary_account_index.has_value()) {
        primary_account = CandidateGaiaId(*mutation.primary_account_index);
      }
      storage.ClearDataForUnknownUsersOrMarkForDeletion(known_gaia_ids,
                                                        primary_account);
      return;
    }
    case MutationType::kClearDataForUsersMarkedForDeletion: {
      std::optional<GaiaId> primary_account;
      if (mutation.primary_account_index.has_value()) {
        primary_account = CandidateGaiaId(*mutation.primary_account_index);
      }
      storage.ClearDataForUsersMarkedForDeletion(primary_account);
      return;
    }
  }
}

auto FuzzedUserVaultDomain() {
  return fuzztest::StructOf<FuzzedUserVault>(
      fuzztest::InRange(0, kNumCandidateUsers - 1), fuzztest::Arbitrary<bool>(),
      fuzztest::VectorOf(
          fuzztest::VectorOf(fuzztest::Arbitrary<uint8_t>()).WithMaxSize(8))
          .WithMaxSize(3),
      fuzztest::OptionalOf(fuzztest::InRange(-1, 100)),
      fuzztest::OptionalOf(fuzztest::Arbitrary<bool>()),
      fuzztest::Arbitrary<bool>(),
      fuzztest::OptionalOf(fuzztest::Arbitrary<std::string>().WithMaxSize(16)),
      fuzztest::OptionalOf(fuzztest::Arbitrary<bool>()),
      fuzztest::OptionalOf(fuzztest::InRange(0, 1)),
      fuzztest::OptionalOf(fuzztest::Arbitrary<bool>()),
      fuzztest::OptionalOf(fuzztest::Arbitrary<bool>()),
      fuzztest::Arbitrary<bool>(),
      fuzztest::OptionalOf(fuzztest::Arbitrary<bool>()),
      fuzztest::OptionalOf(fuzztest::InRange<int64_t>(0, 1000000)),
      fuzztest::Arbitrary<bool>(),
      fuzztest::OptionalOf(fuzztest::InRange(0, 2)),
      fuzztest::OptionalOf(fuzztest::InRange<int64_t>(0, 1000000)),
      fuzztest::OptionalOf(fuzztest::Arbitrary<bool>()));
}

auto FuzzedMutationDomain() {
  return fuzztest::StructOf<FuzzedMutation>(
      fuzztest::ElementOf({
          MutationType::kSetVaultKeys,
          MutationType::kSetKeysMarkedAsStaleByConsumer,
          MutationType::kMutatePhysicalDeviceRecoveryFactorData,
          MutationType::kSetPhysicalDeviceRegistered,
#if BUILDFLAG(IS_MAC)
          MutationType::kSetICloudKeychainRegistered,
#endif
          MutationType::kSetLastRegistrationReturnedLocalDataObsolete,
          MutationType::kSetLastFailedRequestMillis,
          MutationType::kSetDegradedRecoverabilityState,
          MutationType::kClearDataForUser,
          MutationType::kClearDataForUnknownUsersOrMarkForDeletion,
          MutationType::kClearDataForUsersMarkedForDeletion,
      }),
      fuzztest::InRange(0, kNumCandidateUsers - 1), fuzztest::Arbitrary<bool>(),
      fuzztest::InRange(0, 2), fuzztest::InRange<int64_t>(0, 1000000),
      fuzztest::OptionalOf(fuzztest::Arbitrary<std::string>().WithMaxSize(16)),
      fuzztest::VectorOf(
          fuzztest::VectorOf(fuzztest::Arbitrary<uint8_t>()).WithMaxSize(8))
          .WithMaxSize(3),
      fuzztest::VectorOf(fuzztest::InRange(0, kNumCandidateUsers - 1))
          .WithMaxSize(kNumCandidateUsers),
      fuzztest::OptionalOf(fuzztest::InRange(0, kNumCandidateUsers - 1)));
}

void MigrationAndMutationsMatchLegacyAdapter(
    int data_version,
    const std::vector<FuzzedUserVault>& users,
    const std::vector<FuzzedMutation>& mutations) {
  base::ScopedTempDir legacy_dir;
  base::ScopedTempDir migrated_dir;
  ASSERT_TRUE(legacy_dir.CreateUniqueTempDir());
  ASSERT_TRUE(migrated_dir.CreateUniqueTempDir());

  const trusted_vault_pb::LocalTrustedVault legacy_proto =
      BuildLegacyProto(data_version, users);

  ASSERT_TRUE(WriteLegacyTrustedVaultFile(
      legacy_proto, LegacyStandaloneTrustedVaultStorage::GetBackendFilePath(
                        legacy_dir.GetPath(), SecurityDomainId::kChromeSync)));
  ASSERT_TRUE(WriteLegacyTrustedVaultFile(
      legacy_proto,
      LegacyStandaloneTrustedVaultStorage::GetBackendFilePath(
          migrated_dir.GetPath(), SecurityDomainId::kChromeSync)));

  auto legacy_adapter =
      std::make_unique<LegacyStandaloneTrustedVaultStorageAdapter>(
          std::make_unique<LegacyStandaloneTrustedVaultStorage>(
              legacy_dir.GetPath(), SecurityDomainId::kChromeSync));
  auto local_domains_storage =
      LocalDomainsStorage::Create(migrated_dir.GetPath());

  legacy_adapter->ReadDataFromDisk();
  local_domains_storage->ReadDataFromDisk();

  ExpectStoragesEquivalent(*legacy_adapter, *local_domains_storage);

  for (const FuzzedMutation& mutation : mutations) {
    ApplyMutation(*legacy_adapter, mutation);
    ApplyMutation(*local_domains_storage, mutation);
    ExpectStoragesEquivalent(*legacy_adapter, *local_domains_storage);
  }

  // Reload both from disk to verify that persisted state after mutations also
  // remains equivalent.
  auto reloaded_legacy_adapter =
      std::make_unique<LegacyStandaloneTrustedVaultStorageAdapter>(
          std::make_unique<LegacyStandaloneTrustedVaultStorage>(
              legacy_dir.GetPath(), SecurityDomainId::kChromeSync));
  auto reloaded_local_domains_storage =
      LocalDomainsStorage::Create(migrated_dir.GetPath());
  reloaded_legacy_adapter->ReadDataFromDisk();
  reloaded_local_domains_storage->ReadDataFromDisk();

  ExpectStoragesEquivalent(*reloaded_legacy_adapter,
                           *reloaded_local_domains_storage);
}

FUZZ_TEST(LocalDomainsStorageMigrationFuzzTest,
          MigrationAndMutationsMatchLegacyAdapter)
    .WithDomains(fuzztest::InRange(0, 4),
                 fuzztest::VectorOf(FuzzedUserVaultDomain()).WithMaxSize(4),
                 fuzztest::VectorOf(FuzzedMutationDomain()).WithMaxSize(5));

}  // namespace

}  // namespace trusted_vault
