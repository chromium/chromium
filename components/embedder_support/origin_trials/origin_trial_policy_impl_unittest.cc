// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/origin_trials/origin_trial_policy_impl.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/embedder_support/origin_trials/features.h"
#include "components/embedder_support/switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/origin_trials/origin_trials_settings_provider.h"
#include "third_party/blink/public/mojom/origin_trials/origin_trials_settings.mojom.h"

namespace embedder_support {

const blink::OriginTrialPublicKey kTestPublicKey1 = {
    0x75, 0x10, 0xac, 0xf9, 0x3a, 0x1c, 0xb8, 0xa9, 0x28, 0x70, 0xd2,
    0x9a, 0xd0, 0x0b, 0x59, 0xe1, 0xac, 0x2b, 0xb7, 0xd5, 0xca, 0x1f,
    0x64, 0x90, 0x08, 0x8e, 0xa8, 0xe0, 0x56, 0x3a, 0x04, 0xd0,
};

const blink::OriginTrialPublicKey kTestPublicKey2 = {
    0x50, 0x07, 0x4d, 0x76, 0x55, 0x56, 0x42, 0x17, 0x2d, 0x8a, 0x9c,
    0x47, 0x96, 0x25, 0xda, 0x70, 0xaa, 0xb9, 0xfd, 0x53, 0x5d, 0x51,
    0x3e, 0x16, 0xab, 0xb4, 0x86, 0xea, 0xf3, 0x35, 0xc6, 0xca,
};

// Base64 encoding of the `kTestPublicKey1`.
const char kTestPublicKeyString[] =
    "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=";

// Comma-separated Base64 encodings of `{kTestPublicKey1, kTestPublicKey2}`.
const char kTwoTestPublicKeysString[] =
    "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=,"
    "UAdNdlVWQhctipxHliXacKq5/VNdUT4Wq7SG6vM1xso=";

const char kBadEncodingPublicKeyString[] = "Not even base64!";
// Base64-encoded, 31 bytes long
const char kTooShortPublicKeyString[] =
    "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BN==";
// Base64-encoded, 33 bytes long
const char kTooLongPublicKeyString[] =
    "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNAA";
// Comma-separated bad encoding key and good test key.
const char kTwoPublicKeysString_BadAndGood[] =
    "Not even base64!, dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=";
// Comma-separated too short key and too long key.
const char kTwoBadPublicKeysString[] =
    "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BN==,"
    "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNAA";

const char kOneDisabledFeature[] = "A";
const char kTwoDisabledFeatures[] = "A|B";
const char kThreeDisabledFeatures[] = "A|B|C";
const char kSpacesInDisabledFeatures[] = "A|B C";

// Various tokens, each provide the command (in tools/origin_trials) used for
// generation.
// generate_token.py example.com A --expire-timestamp=2000000000
const uint8_t kToken1Signature[] = {
    0x43, 0xdd, 0xd3, 0x2b, 0x12, 0x09, 0x59, 0x52, 0x17, 0xf3, 0x60,
    0x44, 0xab, 0xae, 0x18, 0xcd, 0xcd, 0x20, 0xf4, 0x0f, 0x37, 0x8c,
    0x04, 0x98, 0x8b, 0x8e, 0xf5, 0x7f, 0x56, 0xe3, 0x22, 0xa8, 0xe5,
    0x02, 0x08, 0xfc, 0x2b, 0xd8, 0x6e, 0x91, 0x1f, 0x8f, 0xf1, 0xec,
    0x61, 0xbc, 0x0d, 0xb2, 0x96, 0xcf, 0xc3, 0xf0, 0xc2, 0xc3, 0x23,
    0xe9, 0x34, 0x4f, 0x55, 0x62, 0x46, 0xcb, 0x57, 0x0b};
const char kToken1SignatureEncoded[] =
    "Q93TKxIJWVIX82BEq64Yzc0g9A83jASYi471f1bjIqjlAgj8K9hukR+P8exhvA2yls/"
    "D8MLDI+k0T1ViRstXCw==";
// generate_token.py example.com A --expire-timestamp=2500000000
const uint8_t kToken2Signature[] = {
    0xcd, 0x7f, 0x73, 0xb4, 0x49, 0xf5, 0xff, 0xef, 0xf3, 0x71, 0x4e,
    0x3d, 0xbd, 0x07, 0xcb, 0x94, 0xd7, 0x25, 0x6f, 0x48, 0x14, 0x2f,
    0xb6, 0x9a, 0xc1, 0x33, 0xf6, 0x8f, 0x8f, 0x72, 0xab, 0xd8, 0xeb,
    0x52, 0x5a, 0x20, 0x49, 0xad, 0xf0, 0x84, 0x49, 0x22, 0x64, 0x65,
    0x25, 0xa2, 0xb4, 0xc8, 0x5d, 0xc3, 0xa4, 0x24, 0xaf, 0xac, 0xcd,
    0x48, 0x22, 0xa4, 0x21, 0x1f, 0x2b, 0xf0, 0xb1, 0x02};
const char kToken2SignatureEncoded[] =
    "zX9ztEn1/+/"
    "zcU49vQfLlNclb0gUL7aawTP2j49yq9jrUlogSa3whEkiZGUlorTIXcOkJK+szUgipCEfK/"
    "CxAg==";
// generate_token.py example.com B --expire-timestamp=2000000000
const uint8_t kToken3Signature[] = {
    0x33, 0x49, 0x37, 0x0e, 0x92, 0xbc, 0xf8, 0xf6, 0x71, 0xa9, 0x7a,
    0x46, 0xd5, 0x35, 0x6d, 0x30, 0xd6, 0x89, 0xe3, 0xa4, 0x5b, 0x0b,
    0xae, 0x6c, 0x77, 0x47, 0xe9, 0x5a, 0x20, 0x14, 0x0d, 0x6f, 0xde,
    0xb4, 0x20, 0xe6, 0xce, 0x3a, 0xf1, 0xcb, 0x92, 0xf9, 0xaf, 0xb2,
    0x89, 0x19, 0xce, 0x35, 0xcc, 0x63, 0x5f, 0x59, 0xd9, 0xef, 0x8f,
    0xf9, 0xa1, 0x92, 0xda, 0x8b, 0xda, 0xfd, 0xf1, 0x08};
const char kToken3SignatureEncoded[] =
    "M0k3DpK8+PZxqXpG1TVtMNaJ46RbC65sd0fpWiAUDW/etCDmzjrxy5L5r7KJGc41zGNfWdnvj/"
    "mhktqL2v3xCA==";
class OriginTrialPolicyImplTest : public testing::Test {
 protected:
  OriginTrialPolicyImplTest()
      : token1_signature_(
            std::string(reinterpret_cast<const char*>(kToken1Signature),
                        std::size(kToken1Signature))),
        token2_signature_(
            std::string(reinterpret_cast<const char*>(kToken2Signature),
                        std::size(kToken2Signature))),
        token3_signature_(
            std::string(reinterpret_cast<const char*>(kToken3Signature),
                        std::size(kToken3Signature))),
        two_disabled_tokens_(
            {kToken1SignatureEncoded, kToken2SignatureEncoded}),
        three_disabled_tokens_({kToken1SignatureEncoded,
                                kToken2SignatureEncoded,
                                kToken3SignatureEncoded}),
        manager_(base::WrapUnique(new OriginTrialPolicyImpl())),
        default_keys_(manager_->GetPublicKeys()) {}

  OriginTrialPolicyImpl* manager() { return manager_.get(); }
  const std::vector<blink::OriginTrialPublicKey>& default_keys() {
    return default_keys_;
  }
  std::vector<blink::OriginTrialPublicKey> test_keys() {
    return {kTestPublicKey1};
  }
  std::vector<blink::OriginTrialPublicKey> test_keys2() {
    return {kTestPublicKey1, kTestPublicKey2};
  }
  std::string token1_signature_;
  std::string token2_signature_;
  std::string token3_signature_;
  std::vector<std::string> two_disabled_tokens_;
  std::vector<std::string> three_disabled_tokens_;

 private:
  std::unique_ptr<OriginTrialPolicyImpl> manager_;
  std::vector<blink::OriginTrialPublicKey> default_keys_;
};

TEST_F(OriginTrialPolicyImplTest, DefaultConstructor) {
  // We don't specify here what the keys should be, but make sure those are
  // returned, valid and consistent.
  for (const blink::OriginTrialPublicKey& key : manager()->GetPublicKeys()) {
    EXPECT_EQ(32UL, key.size());
  }
  EXPECT_EQ(default_keys(), manager()->GetPublicKeys());
}

TEST_F(OriginTrialPolicyImplTest, DefaultKeysAreConsistent) {
  OriginTrialPolicyImpl manager2;
  EXPECT_EQ(manager()->GetPublicKeys(), manager2.GetPublicKeys());
}

TEST_F(OriginTrialPolicyImplTest, OverridePublicKeys) {
  EXPECT_TRUE(manager()->SetPublicKeysFromASCIIString(kTestPublicKeyString));
  EXPECT_EQ(test_keys(), manager()->GetPublicKeys());
}

TEST_F(OriginTrialPolicyImplTest, OverridePublicKeysWithTwoKeys) {
  EXPECT_TRUE(
      manager()->SetPublicKeysFromASCIIString(kTwoTestPublicKeysString));
  EXPECT_EQ(test_keys2(), manager()->GetPublicKeys());
}

TEST_F(OriginTrialPolicyImplTest, OverrideKeysNotBase64) {
  EXPECT_FALSE(
      manager()->SetPublicKeysFromASCIIString(kBadEncodingPublicKeyString));
  EXPECT_EQ(default_keys(), manager()->GetPublicKeys());
}

TEST_F(OriginTrialPolicyImplTest, OverrideKeysTooShort) {
  EXPECT_FALSE(
      manager()->SetPublicKeysFromASCIIString(kTooShortPublicKeyString));
  EXPECT_EQ(default_keys(), manager()->GetPublicKeys());
}

TEST_F(OriginTrialPolicyImplTest, OverrideKeysTooLong) {
  EXPECT_FALSE(
      manager()->SetPublicKeysFromASCIIString(kTooLongPublicKeyString));
  EXPECT_EQ(default_keys(), manager()->GetPublicKeys());
}

TEST_F(OriginTrialPolicyImplTest, OverridePublicKeysWithBadAndGoodKey) {
  EXPECT_FALSE(
      manager()->SetPublicKeysFromASCIIString(kTwoPublicKeysString_BadAndGood));
  EXPECT_EQ(default_keys(), manager()->GetPublicKeys());
}

TEST_F(OriginTrialPolicyImplTest, OverridePublicKeysWithTwoBadKeys) {
  EXPECT_FALSE(
      manager()->SetPublicKeysFromASCIIString(kTwoBadPublicKeysString));
  EXPECT_EQ(default_keys(), manager()->GetPublicKeys());
}

TEST_F(OriginTrialPolicyImplTest, NoDisabledFeatures) {
  EXPECT_FALSE(manager()->IsFeatureDisabled("A"));
  EXPECT_FALSE(manager()->IsFeatureDisabled("B"));
  EXPECT_FALSE(manager()->IsFeatureDisabled("C"));
}

TEST_F(OriginTrialPolicyImplTest, DisableOneFeature) {
  EXPECT_TRUE(manager()->SetDisabledFeatures(kOneDisabledFeature));
  EXPECT_TRUE(manager()->IsFeatureDisabled("A"));
  EXPECT_FALSE(manager()->IsFeatureDisabled("B"));
}

TEST_F(OriginTrialPolicyImplTest, DisableTwoFeatures) {
  EXPECT_TRUE(manager()->SetDisabledFeatures(kTwoDisabledFeatures));
  EXPECT_TRUE(manager()->IsFeatureDisabled("A"));
  EXPECT_TRUE(manager()->IsFeatureDisabled("B"));
  EXPECT_FALSE(manager()->IsFeatureDisabled("C"));
}

TEST_F(OriginTrialPolicyImplTest, DisableThreeFeatures) {
  EXPECT_TRUE(manager()->SetDisabledFeatures(kThreeDisabledFeatures));
  EXPECT_TRUE(manager()->IsFeatureDisabled("A"));
  EXPECT_TRUE(manager()->IsFeatureDisabled("B"));
  EXPECT_TRUE(manager()->IsFeatureDisabled("C"));
}

TEST_F(OriginTrialPolicyImplTest, DisableFeatureWithSpace) {
  EXPECT_TRUE(manager()->SetDisabledFeatures(kSpacesInDisabledFeatures));
  EXPECT_TRUE(manager()->IsFeatureDisabled("A"));
  EXPECT_TRUE(manager()->IsFeatureDisabled("B C"));
  EXPECT_FALSE(manager()->IsFeatureDisabled("B"));
  EXPECT_FALSE(manager()->IsFeatureDisabled("C"));
}

TEST_F(OriginTrialPolicyImplTest, NoDisabledTokens) {
  EXPECT_FALSE(manager()->IsTokenDisabled(token1_signature_));
  EXPECT_FALSE(manager()->IsTokenDisabled(token2_signature_));
  EXPECT_FALSE(manager()->IsTokenDisabled(token3_signature_));
}

TEST_F(OriginTrialPolicyImplTest, DisableOneToken) {
  EXPECT_TRUE(manager()->SetDisabledTokens({kToken1SignatureEncoded}));
  EXPECT_TRUE(manager()->IsTokenDisabled(token1_signature_));
  EXPECT_FALSE(manager()->IsTokenDisabled(token2_signature_));
}

TEST_F(OriginTrialPolicyImplTest, DisableTwoTokens) {
  EXPECT_TRUE(manager()->SetDisabledTokens(two_disabled_tokens_));
  EXPECT_TRUE(manager()->IsTokenDisabled(token1_signature_));
  EXPECT_TRUE(manager()->IsTokenDisabled(token2_signature_));
  EXPECT_FALSE(manager()->IsTokenDisabled(token3_signature_));
}

TEST_F(OriginTrialPolicyImplTest, DisableThreeTokens) {
  EXPECT_TRUE(manager()->SetDisabledTokens(three_disabled_tokens_));
  EXPECT_TRUE(manager()->IsTokenDisabled(token1_signature_));
  EXPECT_TRUE(manager()->IsTokenDisabled(token2_signature_));
  EXPECT_TRUE(manager()->IsTokenDisabled(token3_signature_));
}

TEST_F(OriginTrialPolicyImplTest, AllNonDeprecationTrialsAreDisabledByFlag) {
  const char kFrobulateThirdPartyTrialName[] = "FrobulateThirdParty";
  const char kFrobulateDeprecationTrialName[] = "FrobulateDeprecation";
  EXPECT_FALSE(manager()->GetAllowOnlyDeprecationTrials());
  EXPECT_FALSE(manager()->IsFeatureDisabled(kFrobulateThirdPartyTrialName));
  EXPECT_FALSE(manager()->IsFeatureDisabled(kFrobulateDeprecationTrialName));
  manager()->SetAllowOnlyDeprecationTrials(true);
  EXPECT_TRUE(manager()->IsFeatureDisabled(kFrobulateThirdPartyTrialName));
  EXPECT_FALSE(manager()->IsFeatureDisabled(kFrobulateDeprecationTrialName));
}

TEST_F(OriginTrialPolicyImplTest, DisableFeatureForUser) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      kOriginTrialsSampleAPIThirdPartyAlternativeUsage);
  EXPECT_FALSE(manager()->IsFeatureDisabledForUser("FrobulateThirdParty"));
  feature_list.Reset();
  feature_list.InitAndDisableFeature(
      kOriginTrialsSampleAPIThirdPartyAlternativeUsage);
  EXPECT_TRUE(manager()->IsFeatureDisabledForUser("FrobulateThirdParty"));
}

TEST_F(OriginTrialPolicyImplTest, DisableFeatureForUserAfterCheck) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      kOriginTrialsSampleAPIThirdPartyAlternativeUsage);
  // Regression test for https://crbug.com/1244566: This assert is called for
  // its side effect of registering the address of the base::Feature used here.
  // If IsFeatureDisabledForUser erroneously makes a copy of the feature, then
  // that will trigger a DCHECK failure in CheckFeatureIdentity.
  ASSERT_TRUE(base::FeatureList::IsEnabled(
      kOriginTrialsSampleAPIThirdPartyAlternativeUsage));
  EXPECT_FALSE(manager()->IsFeatureDisabledForUser("FrobulateThirdParty"));
}

// Tests for initialization from command line and settings
class OriginTrialPolicyImplInitializationTest
    : public OriginTrialPolicyImplTest {
 protected:
  OriginTrialPolicyImplInitializationTest() = default;

  OriginTrialPolicyImpl* initialized_manager() {
    return initialized_manager_.get();
  }

  void SetUp() override {
    OriginTrialPolicyImplTest::SetUp();

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    ASSERT_FALSE(command_line->HasSwitch(kOriginTrialPublicKey));
    ASSERT_FALSE(command_line->HasSwitch(kOriginTrialDisabledFeatures));

    // Setup command line with various updated values
    // New public key
    command_line->AppendSwitchASCII(kOriginTrialPublicKey,
                                    kTestPublicKeyString);
    // One disabled feature
    command_line->AppendSwitchASCII(kOriginTrialDisabledFeatures,
                                    kOneDisabledFeature);
    // One disabled token in the settings returned by the provider.
    blink::mojom::OriginTrialsSettingsPtr settings =
        blink::mojom::OriginTrialsSettings::New();
    settings->disabled_tokens = {kToken1SignatureEncoded};
    blink::OriginTrialsSettingsProvider::Get()->SetSettings(
        std::move(settings));

    initialized_manager_ = base::WrapUnique(new OriginTrialPolicyImpl());

    // Provider is no longer needed after manager is initialized.
    // Reset the provider to free up the mock.
    blink::OriginTrialsSettingsProvider::Get()->SetSettings(nullptr);
  }

 private:
  std::unique_ptr<OriginTrialPolicyImpl> initialized_manager_;
};

TEST_F(OriginTrialPolicyImplInitializationTest, PublicKeyInitialized) {
  EXPECT_NE(default_keys(), initialized_manager()->GetPublicKeys());
  EXPECT_EQ(test_keys(), initialized_manager()->GetPublicKeys());
}

TEST_F(OriginTrialPolicyImplInitializationTest, DisabledFeaturesInitialized) {
  EXPECT_TRUE(initialized_manager()->IsFeatureDisabled("A"));
  EXPECT_FALSE(initialized_manager()->IsFeatureDisabled("B"));
}

TEST_F(OriginTrialPolicyImplInitializationTest, DisabledTokensInitialized) {
  EXPECT_TRUE(initialized_manager()->IsTokenDisabled(token1_signature_));
  EXPECT_FALSE(initialized_manager()->IsTokenDisabled(token2_signature_));
}

}  // namespace embedder_support
