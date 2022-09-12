// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/public/auth_factors_data.h"

#include "base/strings/stringprintf.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using ::cryptohome::AuthFactor;
using ::cryptohome::AuthFactorCommonMetadata;
using ::cryptohome::AuthFactorRef;
using ::cryptohome::AuthFactorType;
using ::cryptohome::KeyDefinition;
using ::cryptohome::KeyLabel;

KeyDefinition MakeGaiaKeyDef() {
  return KeyDefinition::CreateForPassword(
      "gaia-secret", KeyLabel(kCryptohomeGaiaKeyLabel), /*privileges=*/0);
}

KeyDefinition MakePinKeyDef() {
  KeyDefinition key_def = KeyDefinition::CreateForPassword(
      "pin-secret", KeyLabel(kCryptohomePinLabel), /*privileges=*/0);
  key_def.policy.low_entropy_credential = true;
  return key_def;
}

KeyDefinition MakeLegacyKeyDef(int legacy_key_index) {
  return KeyDefinition::CreateForPassword(
      "legacy-secret",
      KeyLabel(base::StringPrintf("legacy-%d", legacy_key_index)),
      /*privileges=*/0);
}

AuthFactor MakeRecoveryFactor() {
  AuthFactorRef ref(AuthFactorType::kRecovery,
                    KeyLabel(kCryptohomeRecoveryKeyLabel));
  AuthFactor factor(std::move(ref), AuthFactorCommonMetadata());
  return factor;
}

}  // namespace

TEST(AuthFactorsDataTest, FindOnlinePasswordWithNothing) {
  AuthFactorsData data;
  EXPECT_FALSE(data.FindOnlinePasswordKey());
}

TEST(AuthFactorsDataTest, FindOnlinePasswordWithGaia) {
  AuthFactorsData data({MakeGaiaKeyDef()});
  const KeyDefinition* found = data.FindOnlinePasswordKey();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeGaiaKeyDef());
}

TEST(AuthFactorsDataTest, FindOnlinePasswordWithGaiaAndPin) {
  AuthFactorsData data({MakePinKeyDef(), MakeGaiaKeyDef()});
  const KeyDefinition* found = data.FindOnlinePasswordKey();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeGaiaKeyDef());
}

TEST(AuthFactorsDataTest, FindOnlinePasswordWithPinAndGaia) {
  AuthFactorsData data({MakePinKeyDef(), MakeGaiaKeyDef()});
  const KeyDefinition* found = data.FindOnlinePasswordKey();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeGaiaKeyDef());
}

// Check "gaia" is preferred to "legacy-..." keys when searching online password
// key.
TEST(AuthFactorsDataTest, FindOnlinePasswordWithGaiaAndLegacy) {
  AuthFactorsData data({MakeGaiaKeyDef(), MakeLegacyKeyDef(0)});
  const KeyDefinition* found = data.FindOnlinePasswordKey();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeGaiaKeyDef());
}

// Check "gaia" is preferred to "legacy-..." keys when searching online password
// key, regardless of the order of input keys.
TEST(AuthFactorsDataTest, FindOnlinePasswordWithLegacyAndGaia) {
  AuthFactorsData data({MakeLegacyKeyDef(0), MakeGaiaKeyDef()});
  const KeyDefinition* found = data.FindOnlinePasswordKey();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeGaiaKeyDef());
}

TEST(AuthFactorsDataTest, FindOnlinePasswordWithLegacy) {
  AuthFactorsData data({MakeLegacyKeyDef(0)});
  const KeyDefinition* found = data.FindOnlinePasswordKey();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeLegacyKeyDef(0));
}

TEST(AuthFactorsDataTest, FindOnlinePasswordWithLegacyAndPin) {
  AuthFactorsData data({MakeLegacyKeyDef(0), MakePinKeyDef()});
  const KeyDefinition* found = data.FindOnlinePasswordKey();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeLegacyKeyDef(0));
}

TEST(AuthFactorsDataTest, FindOnlinePasswordWithPinAndLegacy) {
  AuthFactorsData data({MakePinKeyDef(), MakeLegacyKeyDef(0)});
  const KeyDefinition* found = data.FindOnlinePasswordKey();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeLegacyKeyDef(0));
}

// Check "legacy-0" is preferred among all legacy keys when searching online
// password key.
TEST(AuthFactorsDataTest, FindOnlinePasswordWithLegacy012) {
  AuthFactorsData data(
      {MakeLegacyKeyDef(0), MakeLegacyKeyDef(1), MakeLegacyKeyDef(2)});
  const KeyDefinition* found = data.FindOnlinePasswordKey();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeLegacyKeyDef(0));
}

// Check "legacy-0" is preferred among all legacy keys when searching online
// password key, regardless of the order of input keys.
TEST(AuthFactorsDataTest, FindOnlinePasswordWithLegacy210) {
  AuthFactorsData data(
      {MakeLegacyKeyDef(2), MakeLegacyKeyDef(1), MakeLegacyKeyDef(0)});
  const KeyDefinition* found = data.FindOnlinePasswordKey();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeLegacyKeyDef(0));
}

TEST(AuthFactorsDataTest, FindRecoveryFactorWithNothing) {
  AuthFactorsData data;
  EXPECT_FALSE(data.FindRecoveryFactor());
}

TEST(AuthFactorsDataTest, FindRecoveryFactorWithSomething) {
  AuthFactorsData data({MakeRecoveryFactor()});
  const AuthFactor* factor = data.FindRecoveryFactor();
  EXPECT_TRUE(factor);
  EXPECT_EQ(factor->ref().type(), AuthFactorType::kRecovery);
  EXPECT_EQ(factor->ref().label(), KeyLabel(kCryptohomeRecoveryKeyLabel));
}

}  // namespace ash
