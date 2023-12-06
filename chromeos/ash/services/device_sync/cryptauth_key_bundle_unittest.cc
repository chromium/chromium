// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_key_bundle.h"

#include "chromeos/ash/services/device_sync/cryptauth_enrollment_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace device_sync {

namespace {

const char kFakeSymmetricKeyHandle[] = "fake-symmetric-key-handle";
const char kFakeAsymmetricKeyHandle[] = "fake-asymmetric-key-handle";
const char kFakeSymmetricKey[] = "fake-symmetric-key";
const char kFakePublicKey[] = "fake-public-key";
const char kFakePrivateKey[] = "fake-private-key";

}  // namespace

TEST(DeviceSyncCryptAuthKeyBundleTest, CreateKeyBundle) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  EXPECT_EQ(bundle.name(), CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  EXPECT_TRUE(bundle.handle_to_key_map().empty());
  EXPECT_FALSE(bundle.key_directive());
}

TEST(DeviceSyncCryptAuthKeyBundleTest, SetKeyDirective) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  cryptauthv2::KeyDirective key_directive;
  bundle.set_key_directive(key_directive);
  ASSERT_TRUE(bundle.key_directive());
  EXPECT_EQ(bundle.key_directive()->SerializeAsString(),
            key_directive.SerializeAsString());
}

TEST(DeviceSyncCryptAuthKeyBundleTest, AddKey) {
  CryptAuthKeyBundle bundle_legacy(CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  CryptAuthKey sym_key(kFakeSymmetricKey, CryptAuthKey::Status::kActive,
                       cryptauthv2::KeyType::RAW256, kFakeSymmetricKeyHandle);
  bundle_legacy.AddKey(sym_key);
  EXPECT_TRUE(bundle_legacy.handle_to_key_map().size() == 1);
  const auto& legacy_it =
      bundle_legacy.handle_to_key_map().find(kFakeSymmetricKeyHandle);
  ASSERT_TRUE(legacy_it != bundle_legacy.handle_to_key_map().end());
  EXPECT_EQ(legacy_it->first, sym_key.handle());
  EXPECT_EQ(legacy_it->second, sym_key);

  // Note: Handles for kUserKeyPair must be kCryptAuthFixedUserKeyPairHandle.
  CryptAuthKeyBundle bundle_user_key_pair(
      CryptAuthKeyBundle::Name::kUserKeyPair);
  CryptAuthKey asym_key(
      kFakePublicKey, kFakePrivateKey, CryptAuthKey::Status::kActive,
      cryptauthv2::KeyType::P256, kCryptAuthFixedUserKeyPairHandle);
  bundle_user_key_pair.AddKey(asym_key);
  EXPECT_TRUE(bundle_user_key_pair.handle_to_key_map().size() == 1);
  const auto& user_key_pair_it = bundle_user_key_pair.handle_to_key_map().find(
      kCryptAuthFixedUserKeyPairHandle);
  ASSERT_TRUE(user_key_pair_it !=
              bundle_user_key_pair.handle_to_key_map().end());
  EXPECT_EQ(user_key_pair_it->first, asym_key.handle());
  EXPECT_EQ(user_key_pair_it->second, asym_key);
}

TEST(DeviceSyncCryptAuthKeyBundleTest, AddKey_Inactive) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  CryptAuthKey symmetric_key(kFakeSymmetricKey, CryptAuthKey::Status::kActive,
                             cryptauthv2::KeyType::RAW256,
                             kFakeSymmetricKeyHandle);
  bundle.AddKey(symmetric_key);
  EXPECT_EQ(
      bundle.handle_to_key_map().find(kFakeSymmetricKeyHandle)->second.status(),
      CryptAuthKey::Status::kActive);

  CryptAuthKey asymmetric_key(
      kFakePublicKey, kFakePrivateKey, CryptAuthKey::Status::kInactive,
      cryptauthv2::KeyType::P256, kFakeAsymmetricKeyHandle);
  bundle.AddKey(asymmetric_key);
  EXPECT_EQ(
      bundle.handle_to_key_map().find(kFakeSymmetricKeyHandle)->second.status(),
      CryptAuthKey::Status::kActive);
  EXPECT_EQ(bundle.handle_to_key_map()
                .find(kFakeAsymmetricKeyHandle)
                ->second.status(),
            CryptAuthKey::Status::kInactive);
}

TEST(DeviceSyncCryptAuthKeyBundleTest, AddKey_ActiveKeyDeactivatesOthers) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  CryptAuthKey symmetric_key(kFakeSymmetricKey, CryptAuthKey::Status::kActive,
                             cryptauthv2::KeyType::RAW256,
                             kFakeSymmetricKeyHandle);
  bundle.AddKey(symmetric_key);
  EXPECT_EQ(
      bundle.handle_to_key_map().find(kFakeSymmetricKeyHandle)->second.status(),
      CryptAuthKey::Status::kActive);

  CryptAuthKey asymmetric_key(
      kFakePublicKey, kFakePrivateKey, CryptAuthKey::Status::kActive,
      cryptauthv2::KeyType::P256, kFakeAsymmetricKeyHandle);
  bundle.AddKey(asymmetric_key);
  EXPECT_EQ(
      bundle.handle_to_key_map().find(kFakeSymmetricKeyHandle)->second.status(),
      CryptAuthKey::Status::kInactive);
  EXPECT_EQ(bundle.handle_to_key_map()
                .find(kFakeAsymmetricKeyHandle)
                ->second.status(),
            CryptAuthKey::Status::kActive);
}

TEST(DeviceSyncCryptAuthKeyBundleTest, AddKey_ReplaceKeyWithSameHandle) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  CryptAuthKey symmetric_key(kFakeSymmetricKey, CryptAuthKey::Status::kActive,
                             cryptauthv2::KeyType::RAW256, "same-handle");
  bundle.AddKey(symmetric_key);
  EXPECT_EQ(bundle.handle_to_key_map().find("same-handle")->second,
            symmetric_key);
  CryptAuthKey asymmetric_key(kFakePublicKey, kFakePrivateKey,
                              CryptAuthKey::Status::kActive,
                              cryptauthv2::KeyType::P256, "same-handle");
  bundle.AddKey(asymmetric_key);
  EXPECT_TRUE(bundle.handle_to_key_map().size() == 1);
  EXPECT_EQ(bundle.handle_to_key_map().find("same-handle")->second,
            asymmetric_key);
}

TEST(DeviceSyncCryptAuthKeyBundleTest, GetActiveKey_DoesNotExist) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  EXPECT_FALSE(bundle.GetActiveKey());

  CryptAuthKey symmetric_key(kFakeSymmetricKey, CryptAuthKey::Status::kInactive,
                             cryptauthv2::KeyType::RAW256,
                             kFakeSymmetricKeyHandle);
  bundle.AddKey(symmetric_key);
  EXPECT_FALSE(bundle.GetActiveKey());
}

TEST(DeviceSyncCryptAuthKeyBundleTest, GetActiveKey_Exists) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  CryptAuthKey symmetric_key(kFakeSymmetricKey, CryptAuthKey::Status::kInactive,
                             cryptauthv2::KeyType::RAW256,
                             kFakeSymmetricKeyHandle);
  bundle.AddKey(symmetric_key);
  CryptAuthKey asymmetric_key(
      kFakePublicKey, kFakePrivateKey, CryptAuthKey::Status::kActive,
      cryptauthv2::KeyType::P256, kFakeAsymmetricKeyHandle);
  bundle.AddKey(asymmetric_key);

  ASSERT_TRUE(bundle.GetActiveKey());
  EXPECT_EQ(*bundle.GetActiveKey(), asymmetric_key);
}

TEST(DeviceSyncCryptAuthKeyBundleTest, SetActiveKey_InactiveToActive) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  CryptAuthKey symmetric_key(kFakeSymmetricKey, CryptAuthKey::Status::kInactive,
                             cryptauthv2::KeyType::RAW256,
                             kFakeSymmetricKeyHandle);
  bundle.AddKey(symmetric_key);
  CryptAuthKey asymmetric_key(
      kFakePublicKey, kFakePrivateKey, CryptAuthKey::Status::kActive,
      cryptauthv2::KeyType::P256, kFakeAsymmetricKeyHandle);
  bundle.AddKey(asymmetric_key);

  bundle.SetActiveKey(kFakeSymmetricKeyHandle);

  EXPECT_EQ(
      bundle.handle_to_key_map().find(kFakeSymmetricKeyHandle)->second.status(),
      CryptAuthKey::Status::kActive);
  EXPECT_EQ(bundle.handle_to_key_map()
                .find(kFakeAsymmetricKeyHandle)
                ->second.status(),
            CryptAuthKey::Status::kInactive);
}

TEST(DeviceSyncCryptAuthKeyBundleTest, SetActiveKey_ActiveToActive) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  CryptAuthKey symmetric_key(kFakeSymmetricKey, CryptAuthKey::Status::kInactive,
                             cryptauthv2::KeyType::RAW256,
                             kFakeSymmetricKeyHandle);
  bundle.AddKey(symmetric_key);
  CryptAuthKey asymmetric_key(
      kFakePublicKey, kFakePrivateKey, CryptAuthKey::Status::kActive,
      cryptauthv2::KeyType::P256, kFakeAsymmetricKeyHandle);
  bundle.AddKey(asymmetric_key);

  bundle.SetActiveKey(kFakeAsymmetricKeyHandle);

  EXPECT_EQ(
      bundle.handle_to_key_map().find(kFakeSymmetricKeyHandle)->second.status(),
      CryptAuthKey::Status::kInactive);
  EXPECT_EQ(bundle.handle_to_key_map()
                .find(kFakeAsymmetricKeyHandle)
                ->second.status(),
            CryptAuthKey::Status::kActive);
}

TEST(DeviceSyncCryptAuthKeyBundleTest, DeactivateKeys) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  CryptAuthKey symmetric_key(kFakeSymmetricKey, CryptAuthKey::Status::kInactive,
                             cryptauthv2::KeyType::RAW256,
                             kFakeSymmetricKeyHandle);
  bundle.AddKey(symmetric_key);
  CryptAuthKey asymmetric_key(
      kFakePublicKey, kFakePrivateKey, CryptAuthKey::Status::kActive,
      cryptauthv2::KeyType::P256, kFakeAsymmetricKeyHandle);
  bundle.AddKey(asymmetric_key);

  bundle.DeactivateKeys();

  EXPECT_EQ(
      bundle.handle_to_key_map().find(kFakeSymmetricKeyHandle)->second.status(),
      CryptAuthKey::Status::kInactive);
  EXPECT_EQ(bundle.handle_to_key_map()
                .find(kFakeAsymmetricKeyHandle)
                ->second.status(),
            CryptAuthKey::Status::kInactive);
}

TEST(DeviceSyncCryptAuthKeyBundleTest, DeleteKey) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  CryptAuthKey symmetric_key(kFakeSymmetricKey, CryptAuthKey::Status::kInactive,
                             cryptauthv2::KeyType::RAW256,
                             kFakeSymmetricKeyHandle);
  bundle.AddKey(symmetric_key);

  EXPECT_TRUE(bundle.handle_to_key_map().size() == 1);
  bundle.DeleteKey(kFakeSymmetricKeyHandle);
  EXPECT_TRUE(bundle.handle_to_key_map().empty());
}

TEST(DeviceSyncCryptAuthKeyBundleTest, ToAndFromDictionary_Trivial) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  std::optional<CryptAuthKeyBundle> bundle_from_dict =
      CryptAuthKeyBundle::FromDictionary(bundle.AsDictionary());
  ASSERT_TRUE(bundle_from_dict);
  EXPECT_EQ(*bundle_from_dict, bundle);
}

TEST(DeviceSyncCryptAuthKeyBundleTest, ToAndFromDictionary) {
  CryptAuthKeyBundle bundle(CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  CryptAuthKey symmetric_key(kFakeSymmetricKey, CryptAuthKey::Status::kInactive,
                             cryptauthv2::KeyType::RAW256,
                             kFakeSymmetricKeyHandle);
  bundle.AddKey(symmetric_key);
  CryptAuthKey asymmetric_key(
      kFakePublicKey, kFakePrivateKey, CryptAuthKey::Status::kActive,
      cryptauthv2::KeyType::P256, kFakeAsymmetricKeyHandle);
  bundle.AddKey(asymmetric_key);

  cryptauthv2::KeyDirective key_directive;
  key_directive.mutable_policy_reference()->set_name("fake-policy-name");
  key_directive.mutable_policy_reference()->set_version(42);
  *key_directive.add_crossproof_key_names() = "fake-key-name-1";
  *key_directive.add_crossproof_key_names() = "fake-key-name-2";
  key_directive.set_enroll_time_millis(1000);
  bundle.set_key_directive(key_directive);

  std::optional<CryptAuthKeyBundle> bundle_from_dict =
      CryptAuthKeyBundle::FromDictionary(bundle.AsDictionary());
  ASSERT_TRUE(bundle_from_dict);
  EXPECT_EQ(*bundle_from_dict, bundle);
}

}  // namespace device_sync

}  // namespace ash
