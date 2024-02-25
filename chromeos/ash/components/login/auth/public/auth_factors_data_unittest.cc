// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/public/session_auth_factors.h"

#include "base/strings/stringprintf.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
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

AuthFactor MakeGaiaAuthFactor() {
  AuthFactorRef ref(cryptohome::AuthFactorType::kPassword,
                    KeyLabel(kCryptohomeGaiaKeyLabel));
  return AuthFactor(std::move(ref), AuthFactorCommonMetadata());
}

AuthFactor MakeLocalPasswordFactor() {
  AuthFactorRef ref(cryptohome::AuthFactorType::kPassword,
                    KeyLabel(kCryptohomeLocalPasswordKeyLabel));
  return AuthFactor(std::move(ref), AuthFactorCommonMetadata());
}

AuthFactor MakeLegacyNonPasswordFactor() {
  AuthFactorRef ref(cryptohome::AuthFactorType::kPassword,
                    KeyLabel("someLegacyExperimentalFactor"));
  return AuthFactor(std::move(ref), AuthFactorCommonMetadata());
}

AuthFactor MakePinAuthFactor() {
  AuthFactorRef ref(cryptohome::AuthFactorType::kPin,
                    KeyLabel(kCryptohomePinLabel));
  return AuthFactor(std::move(ref), AuthFactorCommonMetadata());
}

AuthFactor MakeLegacyAuthFactor(int legacy_key_index) {
  AuthFactorRef ref(
      cryptohome::AuthFactorType::kPassword,
      KeyLabel(base::StringPrintf("legacy-%d", legacy_key_index)));
  return AuthFactor(std::move(ref), AuthFactorCommonMetadata());
}

}  // namespace

TEST(AuthFactorsDataTest, FindOnlinePasswordWithNothing) {
  SessionAuthFactors data;
  EXPECT_FALSE(data.FindOnlinePasswordKey());
}

TEST(AuthFactorsDataTest, FindOnlinePasswordWithGaia) {
  SessionAuthFactors data({MakeGaiaKeyDef()});
  const KeyDefinition* found = data.FindOnlinePasswordKey();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeGaiaKeyDef());
}

TEST(AuthFactorsDataTest, FindOnlinePasswordWithGaiaAndPin) {
  SessionAuthFactors data({MakeGaiaKeyDef(), MakePinKeyDef()});
  const KeyDefinition* found = data.FindOnlinePasswordKey();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeGaiaKeyDef());
}

TEST(AuthFactorsDataTest, FindOnlinePasswordWithPinAndGaia) {
  SessionAuthFactors data({MakePinKeyDef(), MakeGaiaKeyDef()});
  const KeyDefinition* found = data.FindOnlinePasswordKey();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeGaiaKeyDef());
}

// Check "gaia" is preferred to "legacy-..." keys when searching online password
// key.
TEST(AuthFactorsDataTest, FindOnlinePasswordWithGaiaAndLegacy) {
  SessionAuthFactors data({MakeGaiaKeyDef(), MakeLegacyKeyDef(0)});
  const KeyDefinition* found = data.FindOnlinePasswordKey();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeGaiaKeyDef());
}

// Check "gaia" is preferred to "legacy-..." keys when searching online password
// key, regardless of the order of input keys.
TEST(AuthFactorsDataTest, FindOnlinePasswordWithLegacyAndGaia) {
  SessionAuthFactors data({MakeLegacyKeyDef(0), MakeGaiaKeyDef()});
  const KeyDefinition* found = data.FindOnlinePasswordKey();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeGaiaKeyDef());
}

TEST(AuthFactorsDataTest, FindOnlinePasswordWithLegacy) {
  SessionAuthFactors data({MakeLegacyKeyDef(0)});
  const KeyDefinition* found = data.FindOnlinePasswordKey();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeLegacyKeyDef(0));
}

TEST(AuthFactorsDataTest, FindOnlinePasswordWithLegacyAndPin) {
  SessionAuthFactors data({MakeLegacyKeyDef(0), MakePinKeyDef()});
  const KeyDefinition* found = data.FindOnlinePasswordKey();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeLegacyKeyDef(0));
}

TEST(AuthFactorsDataTest, FindOnlinePasswordWithPinAndLegacy) {
  SessionAuthFactors data({MakePinKeyDef(), MakeLegacyKeyDef(0)});
  const KeyDefinition* found = data.FindOnlinePasswordKey();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeLegacyKeyDef(0));
}

// Check "legacy-0" is preferred among all legacy keys when searching online
// password key.
TEST(AuthFactorsDataTest, FindOnlinePasswordWithLegacy012) {
  SessionAuthFactors data(
      {MakeLegacyKeyDef(0), MakeLegacyKeyDef(1), MakeLegacyKeyDef(2)});
  const KeyDefinition* found = data.FindOnlinePasswordKey();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeLegacyKeyDef(0));
}

// Check "legacy-0" is preferred among all legacy keys when searching online
// password key, regardless of the order of input keys.
TEST(AuthFactorsDataTest, FindOnlinePasswordWithLegacy210) {
  SessionAuthFactors data(
      {MakeLegacyKeyDef(2), MakeLegacyKeyDef(1), MakeLegacyKeyDef(0)});
  const KeyDefinition* found = data.FindOnlinePasswordKey();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeLegacyKeyDef(0));
}

// --- Repeat same tests for AuthFactors instead of KeyData ---

TEST(AuthFactorsDataTest, FindOnlinePasswordFactorWithNothing) {
  SessionAuthFactors data;
  EXPECT_FALSE(data.FindOnlinePasswordFactor());
}

TEST(AuthFactorsDataTest, FindOnlinePasswordFactorWithGaia) {
  SessionAuthFactors data({MakeGaiaAuthFactor()});
  const AuthFactor* found = data.FindOnlinePasswordFactor();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeGaiaAuthFactor());
}

TEST(AuthFactorsDataTest, FindOnlinePasswordFactorWithGaiaAndPin) {
  SessionAuthFactors data({MakeLegacyNonPasswordFactor(), MakeGaiaAuthFactor(),
                           MakePinAuthFactor()});
  const AuthFactor* found = data.FindOnlinePasswordFactor();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeGaiaAuthFactor());
}

TEST(AuthFactorsDataTest, FindOnlinePasswordFactorWithPinAndGaia) {
  SessionAuthFactors data({MakeLegacyNonPasswordFactor(), MakePinAuthFactor(),
                           MakeGaiaAuthFactor()});
  const AuthFactor* found = data.FindOnlinePasswordFactor();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeGaiaAuthFactor());
}

// Check "gaia" is preferred to "legacy-..." keys when searching online password
// key.
TEST(AuthFactorsDataTest, FindOnlinePasswordFactorWithGaiaAndLegacy) {
  SessionAuthFactors data({MakeLegacyNonPasswordFactor(), MakeGaiaAuthFactor(),
                           MakeLegacyAuthFactor(0)});
  const AuthFactor* found = data.FindOnlinePasswordFactor();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeGaiaAuthFactor());
}

// Check "gaia" is preferred to "legacy-..." keys when searching online password
// key, regardless of the order of input keys.
TEST(AuthFactorsDataTest, FindOnlinePasswordFactorWithLegacyAndGaia) {
  SessionAuthFactors data({MakeLegacyAuthFactor(0), MakeGaiaAuthFactor()});
  const AuthFactor* found = data.FindOnlinePasswordFactor();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeGaiaAuthFactor());
}

TEST(AuthFactorsDataTest, FindOnlinePasswordFactorWithLegacy) {
  SessionAuthFactors data({MakeLegacyAuthFactor(0)});
  const AuthFactor* found = data.FindOnlinePasswordFactor();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeLegacyAuthFactor(0));
}

TEST(AuthFactorsDataTest, FindOnlinePasswordFactorWithLegacyAndPin) {
  SessionAuthFactors data({MakeLegacyAuthFactor(0), MakePinAuthFactor()});
  const AuthFactor* found = data.FindOnlinePasswordFactor();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeLegacyAuthFactor(0));
}

TEST(AuthFactorsDataTest, FindOnlinePasswordFactorWithPinAndLegacy) {
  SessionAuthFactors data({MakePinAuthFactor(), MakeLegacyAuthFactor(0)});
  const AuthFactor* found = data.FindOnlinePasswordFactor();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeLegacyAuthFactor(0));
}

// Check "legacy-0" is preferred among all legacy keys when searching online
// password key.
TEST(AuthFactorsDataTest, FindOnlinePasswordFactorWithLegacy012) {
  SessionAuthFactors data({MakeLegacyAuthFactor(0), MakeLegacyAuthFactor(1),
                           MakeLegacyAuthFactor(2)});
  const AuthFactor* found = data.FindOnlinePasswordFactor();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeLegacyAuthFactor(0));
}

// Check "legacy-0" is preferred among all legacy keys when searching online
// password key, regardless of the order of input keys.
TEST(AuthFactorsDataTest, FindOnlinePasswordFactorWithLegacy210) {
  SessionAuthFactors data({MakeLegacyAuthFactor(2), MakeLegacyAuthFactor(1),
                           MakeLegacyAuthFactor(0)});
  const AuthFactor* found = data.FindOnlinePasswordFactor();
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, MakeLegacyAuthFactor(0));
}

TEST(AuthFactorsDataTest, FindRecoveryFactorWithNothing) {
  SessionAuthFactors data;
  EXPECT_FALSE(data.FindRecoveryFactor());
}

TEST(AuthFactorsDataTest, FindRecoveryFactorWithRecovery) {
  SessionAuthFactors data({MakeRecoveryFactor()});
  const AuthFactor* factor = data.FindRecoveryFactor();
  ASSERT_TRUE(factor);
  EXPECT_EQ(*factor, MakeRecoveryFactor());
}

TEST(AuthFactorsDataTest, FindRecoveryFactorWithMutlipleFactors) {
  SessionAuthFactors data(
      {MakePinAuthFactor(), MakeRecoveryFactor(), MakeGaiaAuthFactor()});
  const AuthFactor* factor = data.FindRecoveryFactor();
  EXPECT_TRUE(factor);
  ASSERT_TRUE(factor);
  EXPECT_EQ(*factor, MakeRecoveryFactor());
}

TEST(AuthFactorsDataTest, FindLocalPassword) {
  SessionAuthFactors data({MakePinAuthFactor(), MakeLegacyNonPasswordFactor(),
                           MakeLocalPasswordFactor(), MakeRecoveryFactor(),
                           MakeGaiaAuthFactor()});
  const AuthFactor* factor = data.FindLocalPasswordFactor();
  EXPECT_TRUE(factor);
  ASSERT_TRUE(factor);
  EXPECT_EQ(*factor, MakeLocalPasswordFactor());
}

TEST(AuthFactorsDataTest, FindAnyPasswordGaia) {
  SessionAuthFactors data({MakePinAuthFactor(), MakeLegacyNonPasswordFactor(),
                           MakeGaiaAuthFactor(), MakeRecoveryFactor()});
  const AuthFactor* factor = data.FindAnyPasswordFactor();
  EXPECT_TRUE(factor);
  ASSERT_TRUE(factor);
  EXPECT_EQ(*factor, MakeGaiaAuthFactor());
}

TEST(AuthFactorsDataTest, FindAnyPasswordLocal) {
  SessionAuthFactors data({MakePinAuthFactor(), MakeLegacyNonPasswordFactor(),
                           MakeLocalPasswordFactor(), MakeRecoveryFactor()});
  const AuthFactor* factor = data.FindAnyPasswordFactor();
  EXPECT_TRUE(factor);
  ASSERT_TRUE(factor);
  EXPECT_EQ(*factor, MakeLocalPasswordFactor());
}

TEST(AuthFactorsDataTest, HasSinglePasswordNone) {
  SessionAuthFactors data({MakePinAuthFactor(), MakeLegacyNonPasswordFactor(),
                           MakeRecoveryFactor()});
  EXPECT_FALSE(data.HasSinglePasswordFactor());
}

TEST(AuthFactorsDataTest, HasSinglePasswordGaia) {
  SessionAuthFactors data({MakePinAuthFactor(), MakeLegacyNonPasswordFactor(),
                           MakeRecoveryFactor(), MakeGaiaAuthFactor()});
  EXPECT_TRUE(data.HasSinglePasswordFactor());
}

TEST(AuthFactorsDataTest, HasSinglePasswordLegacy) {
  SessionAuthFactors data({MakePinAuthFactor(), MakeLegacyNonPasswordFactor(),
                           MakeRecoveryFactor(), MakeLegacyAuthFactor(0)});
  EXPECT_TRUE(data.HasSinglePasswordFactor());
}

TEST(AuthFactorsDataTest, HasSinglePasswordLocal) {
  SessionAuthFactors data({MakePinAuthFactor(), MakeLegacyNonPasswordFactor(),
                           MakeRecoveryFactor(), MakeLocalPasswordFactor()});
  EXPECT_TRUE(data.HasSinglePasswordFactor());
}

TEST(AuthFactorsDataTest, HasSinglePasswordMultiple) {
  SessionAuthFactors data({MakePinAuthFactor(), MakeLegacyNonPasswordFactor(),
                           MakeRecoveryFactor(), MakeLegacyAuthFactor(0),
                           MakeLocalPasswordFactor()});
  EXPECT_FALSE(data.HasSinglePasswordFactor());
}

}  // namespace ash
