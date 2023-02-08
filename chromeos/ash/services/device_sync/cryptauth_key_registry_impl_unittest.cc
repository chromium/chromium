// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_key_registry_impl.h"

#include "base/containers/contains.h"
#include "chromeos/ash/services/device_sync/cryptauth_enrollment_constants.h"
#include "chromeos/ash/services/device_sync/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::device_sync {

class DeviceSyncCryptAuthKeyRegistryImplTest : public testing::Test {
 public:
  DeviceSyncCryptAuthKeyRegistryImplTest(
      const DeviceSyncCryptAuthKeyRegistryImplTest&) = delete;
  DeviceSyncCryptAuthKeyRegistryImplTest& operator=(
      const DeviceSyncCryptAuthKeyRegistryImplTest&) = delete;

 protected:
  DeviceSyncCryptAuthKeyRegistryImplTest() = default;

  ~DeviceSyncCryptAuthKeyRegistryImplTest() override = default;

  void SetUp() override {
    CryptAuthKeyRegistryImpl::RegisterPrefs(pref_service_.registry());
    key_registry_ = CryptAuthKeyRegistryImpl::Factory::Create(&pref_service_);
  }

  // Verify that changing the in-memory key bundle map updates the pref.
  void VerifyPrefValue(const base::Value::Dict& expected_dict) {
    const base::Value::Dict& dict =
        pref_service_.GetDict(prefs::kCryptAuthKeyRegistry);
    EXPECT_EQ(expected_dict, dict);
  }

  PrefService* pref_service() { return &pref_service_; }

  CryptAuthKeyRegistry* key_registry() { return key_registry_.get(); }

 private:
  TestingPrefServiceSimple pref_service_;

  std::unique_ptr<CryptAuthKeyRegistry> key_registry_;
};

TEST_F(DeviceSyncCryptAuthKeyRegistryImplTest, GetActiveKey_NoActiveKey) {
  CryptAuthKey sym_key("symmetric-key", CryptAuthKey::Status::kInactive,
                       cryptauthv2::KeyType::RAW256, "sym-handle");
  key_registry()->AddKey(CryptAuthKeyBundle::Name::kLegacyAuthzenKey, sym_key);

  EXPECT_FALSE(key_registry()->GetActiveKey(
      CryptAuthKeyBundle::Name::kLegacyAuthzenKey));
}

TEST_F(DeviceSyncCryptAuthKeyRegistryImplTest, GetActiveKey) {
  CryptAuthKey sym_key("symmetric-key", CryptAuthKey::Status::kInactive,
                       cryptauthv2::KeyType::RAW256, "sym-handle");
  CryptAuthKey asym_key("public-key", "private-key",
                        CryptAuthKey::Status::kActive,
                        cryptauthv2::KeyType::P256, "asym-handle");
  key_registry()->AddKey(CryptAuthKeyBundle::Name::kLegacyAuthzenKey, sym_key);
  key_registry()->AddKey(CryptAuthKeyBundle::Name::kLegacyAuthzenKey, asym_key);

  const CryptAuthKey* key =
      key_registry()->GetActiveKey(CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  ASSERT_TRUE(key);
  EXPECT_EQ(asym_key, *key);
}

TEST_F(DeviceSyncCryptAuthKeyRegistryImplTest, AddKey) {
  CryptAuthKey sym_key("symmetric-key", CryptAuthKey::Status::kActive,
                       cryptauthv2::KeyType::RAW256, "sym-handle");
  key_registry()->AddKey(CryptAuthKeyBundle::Name::kLegacyAuthzenKey, sym_key);
  const CryptAuthKeyBundle* key_bundle =
      key_registry()->GetKeyBundle(CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  ASSERT_TRUE(key_bundle);

  const CryptAuthKey* active_key =
      key_registry()->GetActiveKey(CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  ASSERT_TRUE(active_key);
  EXPECT_EQ(sym_key, *active_key);

  CryptAuthKeyBundle expected_bundle(
      CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  expected_bundle.AddKey(sym_key);
  EXPECT_EQ(expected_bundle, *key_bundle);

  base::Value::Dict expected_dict;
  expected_dict.Set(
      CryptAuthKeyBundle::KeyBundleNameEnumToString(expected_bundle.name()),
      expected_bundle.AsDictionary());
  VerifyPrefValue(expected_dict);

  // Add another key to same bundle
  CryptAuthKey asym_key("public-key", "private-key",
                        CryptAuthKey::Status::kActive,
                        cryptauthv2::KeyType::P256, "asym-handle");
  key_registry()->AddKey(CryptAuthKeyBundle::Name::kLegacyAuthzenKey, asym_key);

  expected_bundle.AddKey(asym_key);
  EXPECT_EQ(expected_bundle, *key_bundle);

  active_key =
      key_registry()->GetActiveKey(CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  ASSERT_TRUE(active_key);
  EXPECT_EQ(asym_key, *active_key);

  expected_dict.Set(
      CryptAuthKeyBundle::KeyBundleNameEnumToString(expected_bundle.name()),
      expected_bundle.AsDictionary());
  VerifyPrefValue(expected_dict);
}

TEST_F(DeviceSyncCryptAuthKeyRegistryImplTest, SetActiveKey) {
  CryptAuthKey sym_key("symmetric-key", CryptAuthKey::Status::kInactive,
                       cryptauthv2::KeyType::RAW256, "sym-handle");
  CryptAuthKey asym_key("public-key", "private-key",
                        CryptAuthKey::Status::kActive,
                        cryptauthv2::KeyType::P256, "asym-handle");
  key_registry()->AddKey(CryptAuthKeyBundle::Name::kLegacyAuthzenKey, sym_key);
  key_registry()->AddKey(CryptAuthKeyBundle::Name::kLegacyAuthzenKey, asym_key);

  key_registry()->SetActiveKey(CryptAuthKeyBundle::Name::kLegacyAuthzenKey,
                               "sym-handle");

  const CryptAuthKey* key =
      key_registry()->GetActiveKey(CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  EXPECT_TRUE(key);

  sym_key.set_status(CryptAuthKey::Status::kActive);
  EXPECT_EQ(sym_key, *key);

  CryptAuthKeyBundle expected_bundle(
      CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  expected_bundle.AddKey(sym_key);
  asym_key.set_status(CryptAuthKey::Status::kInactive);
  expected_bundle.AddKey(asym_key);
  base::Value::Dict expected_dict;
  expected_dict.Set(
      CryptAuthKeyBundle::KeyBundleNameEnumToString(expected_bundle.name()),
      expected_bundle.AsDictionary());
  VerifyPrefValue(expected_dict);
}

TEST_F(DeviceSyncCryptAuthKeyRegistryImplTest, DeactivateKeys) {
  CryptAuthKey sym_key("symmetric-key", CryptAuthKey::Status::kInactive,
                       cryptauthv2::KeyType::RAW256, "sym-handle");
  CryptAuthKey asym_key("public-key", "private-key",
                        CryptAuthKey::Status::kActive,
                        cryptauthv2::KeyType::P256, "asym-handle");
  key_registry()->AddKey(CryptAuthKeyBundle::Name::kLegacyAuthzenKey, sym_key);
  key_registry()->AddKey(CryptAuthKeyBundle::Name::kLegacyAuthzenKey, asym_key);

  key_registry()->DeactivateKeys(CryptAuthKeyBundle::Name::kLegacyAuthzenKey);

  EXPECT_FALSE(key_registry()->GetActiveKey(
      CryptAuthKeyBundle::Name::kLegacyAuthzenKey));

  CryptAuthKeyBundle expected_bundle(
      CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  expected_bundle.AddKey(sym_key);
  asym_key.set_status(CryptAuthKey::Status::kInactive);
  expected_bundle.AddKey(asym_key);
  base::Value::Dict expected_dict;
  expected_dict.Set(
      CryptAuthKeyBundle::KeyBundleNameEnumToString(expected_bundle.name()),
      expected_bundle.AsDictionary());
  VerifyPrefValue(expected_dict);
}

TEST_F(DeviceSyncCryptAuthKeyRegistryImplTest, DeleteKey) {
  CryptAuthKey sym_key("symmetric-key", CryptAuthKey::Status::kInactive,
                       cryptauthv2::KeyType::RAW256, "sym-handle");
  CryptAuthKey asym_key("public-key", "private-key",
                        CryptAuthKey::Status::kActive,
                        cryptauthv2::KeyType::P256, "asym-handle");
  key_registry()->AddKey(CryptAuthKeyBundle::Name::kLegacyAuthzenKey, sym_key);
  key_registry()->AddKey(CryptAuthKeyBundle::Name::kLegacyAuthzenKey, asym_key);

  key_registry()->DeleteKey(CryptAuthKeyBundle::Name::kLegacyAuthzenKey,
                            "sym-handle");

  const CryptAuthKeyBundle* key_bundle =
      key_registry()->GetKeyBundle(CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  ASSERT_TRUE(key_bundle);

  EXPECT_FALSE(base::Contains(key_bundle->handle_to_key_map(), "sym-handle"));
  EXPECT_TRUE(base::Contains(key_bundle->handle_to_key_map(), "asym-handle"));

  CryptAuthKeyBundle expected_bundle(
      CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  expected_bundle.AddKey(asym_key);
  base::Value::Dict expected_dict;
  expected_dict.Set(
      CryptAuthKeyBundle::KeyBundleNameEnumToString(expected_bundle.name()),
      expected_bundle.AsDictionary());
  VerifyPrefValue(expected_dict);
}

TEST_F(DeviceSyncCryptAuthKeyRegistryImplTest, SetKeyDirective) {
  CryptAuthKey sym_key("symmetric-key", CryptAuthKey::Status::kInactive,
                       cryptauthv2::KeyType::RAW256, "sym-handle");
  key_registry()->AddKey(CryptAuthKeyBundle::Name::kLegacyAuthzenKey, sym_key);

  cryptauthv2::KeyDirective key_directive;
  key_directive.set_enroll_time_millis(1000);
  key_registry()->SetKeyDirective(CryptAuthKeyBundle::Name::kLegacyAuthzenKey,
                                  key_directive);

  const CryptAuthKeyBundle* key_bundle =
      key_registry()->GetKeyBundle(CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  ASSERT_TRUE(key_bundle);

  EXPECT_TRUE(key_bundle->key_directive());
  EXPECT_EQ(key_directive.SerializeAsString(),
            key_bundle->key_directive()->SerializeAsString());

  CryptAuthKeyBundle expected_bundle(
      CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  expected_bundle.AddKey(sym_key);
  expected_bundle.set_key_directive(key_directive);
  base::Value::Dict expected_dict;
  expected_dict.Set(
      CryptAuthKeyBundle::KeyBundleNameEnumToString(expected_bundle.name()),
      expected_bundle.AsDictionary());
  VerifyPrefValue(expected_dict);
}

TEST_F(DeviceSyncCryptAuthKeyRegistryImplTest,
       ConstructorPopulatesBundlesUsingPref) {
  CryptAuthKey asym_key(
      "public-key", "private-key", CryptAuthKey::Status::kActive,
      cryptauthv2::KeyType::P256, kCryptAuthFixedUserKeyPairHandle);
  key_registry()->AddKey(CryptAuthKeyBundle::Name::kUserKeyPair, asym_key);

  CryptAuthKey sym_key("symmetric-key", CryptAuthKey::Status::kActive,
                       cryptauthv2::KeyType::RAW256, "sym-handle");
  cryptauthv2::KeyDirective key_directive;
  key_directive.set_enroll_time_millis(1000);
  key_registry()->AddKey(CryptAuthKeyBundle::Name::kLegacyAuthzenKey, sym_key);
  key_registry()->SetKeyDirective(CryptAuthKeyBundle::Name::kLegacyAuthzenKey,
                                  key_directive);

  // A new registry using the same pref service that was just written.
  std::unique_ptr<CryptAuthKeyRegistry> new_registry =
      CryptAuthKeyRegistryImpl::Factory::Create(pref_service());

  EXPECT_EQ(2u, new_registry->key_bundles().size());

  const CryptAuthKeyBundle* key_bundle_user_key_pair =
      key_registry()->GetKeyBundle(CryptAuthKeyBundle::Name::kUserKeyPair);
  ASSERT_TRUE(key_bundle_user_key_pair);
  CryptAuthKeyBundle expected_bundle_user_key_pair(
      CryptAuthKeyBundle::Name::kUserKeyPair);
  expected_bundle_user_key_pair.AddKey(asym_key);
  EXPECT_EQ(expected_bundle_user_key_pair, *key_bundle_user_key_pair);

  const CryptAuthKeyBundle* key_bundle_legacy_authzen_key =
      key_registry()->GetKeyBundle(CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  ASSERT_TRUE(key_bundle_legacy_authzen_key);
  CryptAuthKeyBundle expected_bundle_legacy_authzen_key(
      CryptAuthKeyBundle::Name::kLegacyAuthzenKey);
  expected_bundle_legacy_authzen_key.AddKey(sym_key);
  expected_bundle_legacy_authzen_key.set_key_directive(key_directive);
  EXPECT_EQ(expected_bundle_legacy_authzen_key, *key_bundle_legacy_authzen_key);
}

}  // namespace ash::device_sync
