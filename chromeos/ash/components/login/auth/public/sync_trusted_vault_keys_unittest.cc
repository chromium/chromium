// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/public/sync_trusted_vault_keys.h"

#include <optional>

#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::NotNull;

MATCHER(HasEmptyValue, "") {
  return arg.encryption_keys().empty() &&
         arg.last_encryption_key_version() == 0 &&
         arg.trusted_recovery_methods().empty();
}

MATCHER_P2(MatchesRecoveryMethod, public_key, type, "") {
  return arg.public_key == public_key && arg.type_hint == type;
}

base::Value::Dict MakeKeyValueWithoutVersion(
    const std::vector<uint8_t>& key_material) {
  base::Value::Dict key_dict;
  key_dict.Set("keyMaterial", base::Value(key_material));
  return key_dict;
}

base::Value::Dict MakeKeyValue(const std::vector<uint8_t>& key_material,
                               int version) {
  base::Value::Dict key_dict = MakeKeyValueWithoutVersion(key_material);
  key_dict.Set("version", version);
  return key_dict;
}

base::Value::Dict MakePublicKeyAndType(const std::vector<uint8_t>& public_key,
                                       int type) {
  base::Value::Dict key_dict;
  key_dict.Set("publicKey", base::Value(public_key));
  key_dict.Set("type", type);
  return key_dict;
}

}  // namespace

TEST(SyncTrustedVaultKeysTest, DefaultConstructor) {
  EXPECT_THAT(SyncTrustedVaultKeys(), HasEmptyValue());
}

TEST(SyncTrustedVaultKeysTest, FromJsWithEmptyDictionary) {
  EXPECT_THAT(SyncTrustedVaultKeys::FromJs(base::Value::Dict()),
              HasEmptyValue());
}

TEST(SyncTrustedVaultKeysTest, FromJsWithInvalidDictionary) {
  base::Value::Dict value;
  value.Set("foo", "bar");
  EXPECT_THAT(SyncTrustedVaultKeys::FromJs(value), HasEmptyValue());
}

TEST(SyncTrustedVaultKeysTest, FromJsWithGaiaId) {
  const std::string kGaiaId = "user1";
  base::Value::Dict value;
  value.Set("obfuscatedGaiaId", kGaiaId);
  EXPECT_THAT(SyncTrustedVaultKeys::FromJs(value).gaia_id(), Eq(kGaiaId));
}

TEST(SyncTrustedVaultKeysTest, FromJsWithEncryptionKeys) {
  const std::vector<uint8_t> kEncryptionKeyMaterial1 = {1, 2, 3, 4};
  const std::vector<uint8_t> kEncryptionKeyMaterial2 = {5, 6, 7, 8};
  const int kEncryptionKeyVersion1 = 17;
  const int kEncryptionKeyVersion2 = 15;

  base::Value::List key_values;
  key_values.Append(
      MakeKeyValue(kEncryptionKeyMaterial1, kEncryptionKeyVersion1));
  key_values.Append(
      MakeKeyValue(kEncryptionKeyMaterial2, kEncryptionKeyVersion2));

  base::Value::Dict root_value;
  root_value.Set("encryptionKeys", std::move(key_values));

  const SyncTrustedVaultKeys actual_converted_keys =
      SyncTrustedVaultKeys::FromJs(root_value);
  EXPECT_THAT(actual_converted_keys.last_encryption_key_version(),
              Eq(kEncryptionKeyVersion2));
  EXPECT_THAT(actual_converted_keys.encryption_keys(),
              ElementsAre(kEncryptionKeyMaterial1, kEncryptionKeyMaterial2));
  EXPECT_THAT(actual_converted_keys.trusted_recovery_methods(), IsEmpty());
}

TEST(SyncTrustedVaultKeysTest, FromJsWithEncryptionKeysWithMissingVersion) {
  const std::vector<uint8_t> kEncryptionKeyMaterial1 = {1, 2, 3, 4};
  const std::vector<uint8_t> kEncryptionKeyMaterial2 = {5, 6, 7, 8};
  const int kEncryptionKeyVersion1 = 17;

  base::Value::List key_values;
  key_values.Append(
      MakeKeyValue(kEncryptionKeyMaterial1, kEncryptionKeyVersion1));
  key_values.Append(MakeKeyValueWithoutVersion(kEncryptionKeyMaterial2));

  base::Value::Dict root_value;
  root_value.Set("encryptionKeys", std::move(key_values));

  const SyncTrustedVaultKeys actual_converted_keys =
      SyncTrustedVaultKeys::FromJs(root_value);
  EXPECT_THAT(actual_converted_keys.last_encryption_key_version(),
              Eq(kEncryptionKeyVersion1));
  EXPECT_THAT(actual_converted_keys.encryption_keys(),
              ElementsAre(kEncryptionKeyMaterial1));
  EXPECT_THAT(actual_converted_keys.trusted_recovery_methods(), IsEmpty());
}

TEST(SyncTrustedVaultKeysTest, FromJsWithTrustedRecoveryMethods) {
  const std::vector<uint8_t> kPublicKeyMaterial1 = {1, 2, 3, 4};
  const std::vector<uint8_t> kPublicKeyMaterial2 = {5, 6, 7, 8};
  const int kMethodType1 = 7;
  const int kMethodType2 = 8;

  base::Value::List key_values;
  key_values.Append(MakePublicKeyAndType(kPublicKeyMaterial1, kMethodType1));
  key_values.Append(MakePublicKeyAndType(kPublicKeyMaterial2, kMethodType2));

  base::Value::Dict root_value;
  root_value.Set("trustedRecoveryMethods", std::move(key_values));

  const SyncTrustedVaultKeys actual_converted_keys =
      SyncTrustedVaultKeys::FromJs(root_value);
  EXPECT_THAT(actual_converted_keys.last_encryption_key_version(), Eq(0));
  EXPECT_THAT(actual_converted_keys.encryption_keys(), IsEmpty());
  EXPECT_THAT(
      actual_converted_keys.trusted_recovery_methods(),
      ElementsAre(MatchesRecoveryMethod(kPublicKeyMaterial1, kMethodType1),
                  MatchesRecoveryMethod(kPublicKeyMaterial2, kMethodType2)));
}

}  // namespace ash
