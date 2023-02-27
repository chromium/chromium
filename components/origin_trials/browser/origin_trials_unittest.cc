// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "components/origin_trials/browser/origin_trials.h"
#include "components/origin_trials/common/persisted_trial_token.h"
#include "components/origin_trials/test/test_persistence_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/origin_trials/scoped_test_origin_trial_policy.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace origin_trials {
namespace {

const char kPersistentTrialName[] = "FrobulatePersistent";
const char kNonPersistentTrialName[] = "Frobulate";
const char kPersistentExpiryPeriodTrialName[] =
    "FrobulatePersistentExpiryGracePeriod";
const char kPersistentThirdPartyDeprecationTrialName[] =
    "FrobulatePersistentThirdPartyDeprecation";
const char kInvalidTrialName[] = "InvalidTrial";
const char kTrialEnabledOriginA[] = "https://enabled.example.com";
const char kTrialEnabledOriginB[] = "https://enabled.alternate.com";
const char kThirdPartyTrialEnabledOrigin[] = "https://enabled.thirdparty.com";

// A dummy value that hasn't been explicitly disabled
const char kDummyTokenSignature[] = "";

const base::Time kValidTime = base::Time::FromTimeT(1000000000);
const base::Time kExpiryTime = base::Time::FromTimeT(2000000000);

// Valid header token for FrobulatePersistent
// generated with
// tools/origin_trials/generate_token.py enabled.example.com FrobulatePersistent
// --expire-timestamp=2000000000
const char kFrobulatePersistentToken[] =
    "A5J4wscfmyiy3zsOMNASl25jJffHq/XGKNoFCSWoAsL0aXv+ugtTQoE/"
    "Cs3a6pDezyQpKfYy10kcbdVC4i1n8AQAAABleyJvcmlnaW4iOiAiaHR0cHM6Ly9lbmFibGVkLm"
    "V4YW1wbGUuY29tOjQ0MyIsICJmZWF0dXJlIjogIkZyb2J1bGF0ZVBlcnNpc3RlbnQiLCAiZXhw"
    "aXJ5IjogMjAwMDAwMDAwMH0=";

// Valid header token for FrobulatePersistent
// generated with
// tools/origin_trials/generate_token.py enabled.alternate.com
// FrobulatePersistent
// --expire-timestamp=2000000000
const char kFrobulatePersistentTokenAlternate[] =
    "A9djuSDQQSirNBctmtYIXSXBz9NyOjFWTMQeuZ2N3AaBA0O/"
    "Rk8e1hZ8t5adUNzO5O+"
    "vGamPUaicRBPNKwe2TAYAAABneyJvcmlnaW4iOiAiaHR0cHM6Ly9lbmFibGVkLmFsdGVybmF0Z"
    "S5jb206NDQzIiwgImZlYXR1cmUiOiAiRnJvYnVsYXRlUGVyc2lzdGVudCIsICJleHBpcnkiOiA"
    "yMDAwMDAwMDAwfQ==";

// Valid header token for Frobulate
// generated with
// tools/origin_trials/generate_token.py enabled.example.com Frobulate
// --expire-timestamp=2000000000
const char kFrobulateToken[] =
    "A2QF2oWLF7q+qYSwt+"
    "AvY6DSGE5QAb9Aeg7eOmbanVINtCJcCjZtNAtUUE88BB5UlvOMpPUUQjfgs3LTO0YXzAcAAABb"
    "eyJvcmlnaW4iOiAiaHR0cHM6Ly9lbmFibGVkLmV4YW1wbGUuY29tOjQ0MyIsICJmZWF0dXJlIj"
    "ogIkZyb2J1bGF0ZSIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ==";

// Valid header token for Frobulate
// generated with
// tools/origin_trials/generate_token.py enabled.example.com
// FrobulateManualCompletion
// --expire-timestamp=2000000000
const char kFrobulateManualCompletionToken[] =
    "A4TCodS8fnQFVyShubc4TKr+"
    "Ss6br97EBk4Kh1bQiskjJHwHXKjhxMjwviiL60RD4byiVF3D9UmoPdXcz7Kg8w8AAAB2eyJvcm"
    "lnaW4iOiAiaHR0cHM6Ly9lbmFibGVkLmV4YW1wbGUuY29tOjQ0MyIsICJmZWF0dXJlIjogIkZy"
    "b2J1bGF0ZVBlcnNpc3RlbnRFeHBpcnlHcmFjZVBlcmlvZCIsICJleHBpcnkiOiAyMDAwMDAwMD"
    "AwfQ==";

// Valid header token for FrobulatePersistentExpiryGracePeriod
// generated with
// tools/origin_trials/generate_token.py enabled.example.com
// FrobulatePersistentExpiryGracePeriod
// --expire-timestamp=2000000000
const char kFrobulatePersistentExpiryGracePeriodToken[] =
    "A4TCodS8fnQFVyShubc4TKr+"
    "Ss6br97EBk4Kh1bQiskjJHwHXKjhxMjwviiL60RD4byiVF3D9UmoPdXcz7Kg8w8AAAB2eyJvcm"
    "lnaW4iOiAiaHR0cHM6Ly9lbmFibGVkLmV4YW1wbGUuY29tOjQ0MyIsICJmZWF0dXJlIjogIkZy"
    "b2J1bGF0ZVBlcnNpc3RlbnRFeHBpcnlHcmFjZVBlcmlvZCIsICJleHBpcnkiOiAyMDAwMDAwMD"
    "AwfQ==";

// Valid third-party token for FrobulatePersistent
// generated with
// tools/origin_trials/generate_token.py enabled.thirdparty.com
// FrobulatePersistent --expire-timestamp=2000000000 --is-third-party
const char kFrobulatePersistentThirdPartyToken[] =
    "A0k800P9maNhwX47OMx4NJk1cxcfwvudfdr4Vq12DLVLMqDlnOGxGJvxH4SkY2UUGmIt4SCuec"
    "zRqRHI81k9/"
    "w0AAAB+"
    "eyJvcmlnaW4iOiAiaHR0cHM6Ly9lbmFibGVkLnRoaXJkcGFydHkuY29tOjQ0MyIsICJmZWF0dX"
    "JlIjogIkZyb2J1bGF0ZVBlcnNpc3RlbnQiLCAiZXhwaXJ5IjogMjAwMDAwMDAwMCwgImlzVGhp"
    "cmRQYXJ0eSI6IHRydWV9";

// Valid token for FrobulatePersistentThirdPartyDeprecation
// generated with
// tools/origin_trials/generate_token.py enabled.thirdparty.com
// FrobulatePersistentThirdPartyDeprecation --expire-timestamp=2000000000
// --is-third-party
const char kFrobulatePersistentThirdPartyDeprecationToken[] =
    "Az+ztSNd9o+3cmaiCk7QgSU5/2jSa1qiNKsoJOOxvMVxf/"
    "8xPWsKraWc0US05bYHmTAIdzZAxh1DMMRhMir5yg4AAACTeyJvcmlnaW4iOiAiaHR0cHM6Ly9l"
    "bmFibGVkLnRoaXJkcGFydHkuY29tOjQ0MyIsICJmZWF0dXJlIjogIkZyb2J1bGF0ZVBlcnNpc3"
    "RlbnRUaGlyZFBhcnR5RGVwcmVjYXRpb24iLCAiZXhwaXJ5IjogMjAwMDAwMDAwMCwgImlzVGhp"
    "cmRQYXJ0eSI6IHRydWV9";

class OpenScopedTestOriginTrialPolicy
    : public blink::ScopedTestOriginTrialPolicy {
 public:
  OpenScopedTestOriginTrialPolicy() = default;
  ~OpenScopedTestOriginTrialPolicy() override = default;

  // Check if the passed |trial_name| has been disabled.
  bool IsFeatureDisabled(base::StringPiece trial_name) const override {
    return disabled_trials_.contains(trial_name);
  }

  bool IsTokenDisabled(base::StringPiece token_signature) const override {
    return disabled_signatures_.contains(token_signature);
  }

  // Check if the passed |trial_name| has been disabled.
  bool IsFeatureDisabledForUser(base::StringPiece trial_name) const override {
    return user_disabled_trials_.contains(trial_name);
  }

  void DisableTrial(base::StringPiece trial) {
    disabled_trials_.emplace(trial);
  }

  void DisableToken(base::StringPiece token_signature) {
    disabled_signatures_.emplace(token_signature);
  }

  void DisableTrialForUser(base::StringPiece trial_name) {
    user_disabled_trials_.emplace(trial_name);
  }

 private:
  base::flat_set<std::string> disabled_trials_;
  base::flat_set<std::string> disabled_signatures_;
  base::flat_set<std::string> user_disabled_trials_;
};

}  // namespace

class OriginTrialsTest : public testing::Test {
 public:
  OriginTrialsTest()
      : origin_trials_(std::make_unique<test::TestPersistenceProvider>(),
                       std::make_unique<blink::TrialTokenValidator>()),
        trial_enabled_origin_(url::Origin::Create(GURL(kTrialEnabledOriginA))) {
  }

  OriginTrialsTest(const OriginTrialsTest&) = delete;
  OriginTrialsTest& operator=(const OriginTrialsTest&) = delete;

  ~OriginTrialsTest() override = default;

  // PersistTrialsFromTokens using |origin| as partition origin.
  void PersistTrialsFromTokens(const url::Origin& origin,
                               base::span<std::string> tokens,
                               base::Time time) {
    origin_trials_.PersistTrialsFromTokens(origin,
                                           /* partition_origin*/ origin, tokens,
                                           time);
  }

  // GetPersistedTrialsForOrigin using |trial_origin| as partition origin.
  base::flat_set<std::string> GetPersistedTrialsForOrigin(
      const url::Origin& trial_origin,
      base::Time lookup_time) {
    return origin_trials_.GetPersistedTrialsForOrigin(
        trial_origin, /* partition_origin */ trial_origin, lookup_time);
  }

  // IsTrialPersistedForOrigin using |origin| as partition origin.
  bool IsTrialPersistedForOrigin(const url::Origin& origin,
                                 const std::string& trial_name,
                                 base::Time lookup_time) {
    return origin_trials_.IsTrialPersistedForOrigin(
        origin, /* partition_origin */ origin, trial_name, lookup_time);
  }

  std::string GetTokenPartitionSite(const url::Origin& origin) {
    return OriginTrials::GetTokenPartitionSite(origin);
  }

  // Test helper that creates an origin for the domain_name with https scheme
  // and port 443.
  url::Origin DomainAsOrigin(const std::string& domain_name) {
    return url::Origin::CreateFromNormalizedTuple("https", domain_name, 443);
  }

 protected:
  OriginTrials origin_trials_;
  url::Origin trial_enabled_origin_;
  OpenScopedTestOriginTrialPolicy origin_trial_policy_;
};

TEST_F(OriginTrialsTest, CleanObjectHasNoPersistentTrials) {
  EXPECT_TRUE(
      GetPersistedTrialsForOrigin(trial_enabled_origin_, kValidTime).empty());
}

TEST_F(OriginTrialsTest, EnabledTrialsArePersisted) {
  std::vector<std::string> tokens = {kFrobulatePersistentToken};
  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime);

  base::flat_set<std::string> enabled_trials =
      GetPersistedTrialsForOrigin(trial_enabled_origin_, kValidTime);
  ASSERT_EQ(1ul, enabled_trials.size());
  EXPECT_TRUE(enabled_trials.contains(kPersistentTrialName));
}

TEST_F(OriginTrialsTest, OnlyPersistentTrialsAreEnabled) {
  std::vector<std::string> tokens = {kFrobulateToken,
                                     kFrobulatePersistentToken};
  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime);

  base::flat_set<std::string> enabled_trials =
      GetPersistedTrialsForOrigin(trial_enabled_origin_, kValidTime);
  ASSERT_EQ(1ul, enabled_trials.size());
  EXPECT_TRUE(enabled_trials.contains(kPersistentTrialName));
  EXPECT_FALSE(enabled_trials.contains(kNonPersistentTrialName));
}

TEST_F(OriginTrialsTest, ResetClearsPersistedTrials) {
  std::vector<std::string> tokens = {kFrobulatePersistentToken};
  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime);

  EXPECT_FALSE(
      GetPersistedTrialsForOrigin(trial_enabled_origin_, kValidTime).empty());

  tokens = {};
  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime);

  EXPECT_TRUE(
      GetPersistedTrialsForOrigin(trial_enabled_origin_, kValidTime).empty());
}

TEST_F(OriginTrialsTest, TrialNotEnabledByDefault) {
  EXPECT_FALSE(IsTrialPersistedForOrigin(trial_enabled_origin_,
                                         kPersistentTrialName, kValidTime));
}

TEST_F(OriginTrialsTest, TrialEnablesFeature) {
  std::vector<std::string> tokens = {kFrobulatePersistentToken};
  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime);

  EXPECT_TRUE(IsTrialPersistedForOrigin(trial_enabled_origin_,
                                        kPersistentTrialName, kValidTime));
}

TEST_F(OriginTrialsTest, TrialDoesNotEnableOtherFeatures) {
  std::vector<std::string> tokens = {kFrobulatePersistentToken};
  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime);

  EXPECT_FALSE(IsTrialPersistedForOrigin(trial_enabled_origin_,
                                         kNonPersistentTrialName, kValidTime));
}

TEST_F(OriginTrialsTest, TokensCanBeAppended) {
  std::vector<std::string> tokens = {kFrobulatePersistentToken};
  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime);

  EXPECT_TRUE(IsTrialPersistedForOrigin(trial_enabled_origin_,
                                        kPersistentTrialName, kValidTime));
  EXPECT_FALSE(IsTrialPersistedForOrigin(
      trial_enabled_origin_, kPersistentExpiryPeriodTrialName, kValidTime));

  // Append an additional token for the same origin
  std::vector<std::string> additional_tokens = {
      kFrobulatePersistentExpiryGracePeriodToken};
  origin_trials_.PersistAdditionalTrialsFromTokens(
      trial_enabled_origin_, /*partition_origin=*/trial_enabled_origin_,
      /*script_origins=*/{}, additional_tokens, kValidTime);
  // Check that both trials are now enabled
  EXPECT_TRUE(IsTrialPersistedForOrigin(trial_enabled_origin_,
                                        kPersistentTrialName, kValidTime));
  EXPECT_TRUE(IsTrialPersistedForOrigin(
      trial_enabled_origin_, kPersistentExpiryPeriodTrialName, kValidTime));
}

TEST_F(OriginTrialsTest, ThirdPartyTokensCanBeAppendedOnlyIfDeprecation) {
  // TODO(crbug.com/1418340): Change test when all 3P tokens are supported.
  // Append third-party tokens.
  std::vector<std::string> third_party_tokens = {
      kFrobulatePersistentThirdPartyToken,
      kFrobulatePersistentThirdPartyDeprecationToken};
  url::Origin script_origin =
      url::Origin::Create(GURL(kThirdPartyTrialEnabledOrigin));
  std::vector<url::Origin> script_origins = {script_origin};

  origin_trials_.PersistAdditionalTrialsFromTokens(
      trial_enabled_origin_, /*partition_origin=*/trial_enabled_origin_,
      script_origins, third_party_tokens, kValidTime);

  // The FrobulatePersistent should not be persisted, as it is not a deprecation
  // token.
  EXPECT_FALSE(origin_trials_.IsTrialPersistedForOrigin(
      script_origin, /*partition_origin=*/trial_enabled_origin_,
      kPersistentTrialName, kValidTime));

  // FrobulatePersistentThirdPartyDeprecation is a deprecation trial, and should
  // be enabled.
  EXPECT_TRUE(origin_trials_.IsTrialPersistedForOrigin(
      script_origin, /*partition_origin=*/trial_enabled_origin_,
      kPersistentThirdPartyDeprecationTrialName, kValidTime));
}

// Check that a stored trial name is not returned if that trial is no longer
// valid or configured to be persistent
TEST_F(OriginTrialsTest, StoredEnabledTrialNotReturnedIfNoLongerPersistent) {
  std::unique_ptr<test::TestPersistenceProvider> persistence_provider =
      std::make_unique<test::TestPersistenceProvider>();

  base::Time token_expiry = base::Time::FromTimeT(2000000000);
  base::flat_set<std::string> partition_sites = {
      GetTokenPartitionSite(trial_enabled_origin_)};
  base::flat_set<PersistedTrialToken> stored_tokens = {
      {kNonPersistentTrialName, token_expiry,
       blink::TrialToken::UsageRestriction::kNone, kDummyTokenSignature,
       partition_sites},
      {kInvalidTrialName, token_expiry,
       blink::TrialToken::UsageRestriction::kNone, kDummyTokenSignature,
       partition_sites},
      {kPersistentTrialName, token_expiry,
       blink::TrialToken::UsageRestriction::kNone, kDummyTokenSignature,
       partition_sites}};
  persistence_provider->SavePersistentTrialTokens(trial_enabled_origin_,
                                                  stored_tokens);

  OriginTrials origin_trials(std::move(persistence_provider),
                             std::make_unique<blink::TrialTokenValidator>());
  base::flat_set<std::string> enabled_trials =
      origin_trials.GetPersistedTrialsForOrigin(
          trial_enabled_origin_, /*partition_origin=*/trial_enabled_origin_,
          kValidTime);
  ASSERT_EQ(1ul, enabled_trials.size());
  EXPECT_EQ(kPersistentTrialName, *(enabled_trials.begin()));
}

// Check that a saved trial name is not returned if it has been disabled by
// policy
TEST_F(OriginTrialsTest, DisabledTrialsNotReturned) {
  std::unique_ptr<test::TestPersistenceProvider> persistence_provider =
      std::make_unique<test::TestPersistenceProvider>();

  base::Time token_expiry = base::Time::FromTimeT(2000000000);
  base::flat_set<std::string> partition_sites = {
      GetTokenPartitionSite(trial_enabled_origin_)};
  base::flat_set<PersistedTrialToken> stored_tokens = {
      {kPersistentTrialName, token_expiry,
       blink::TrialToken::UsageRestriction::kNone, kDummyTokenSignature,
       partition_sites}};
  persistence_provider->SavePersistentTrialTokens(trial_enabled_origin_,
                                                  stored_tokens);

  origin_trial_policy_.DisableTrial(kPersistentTrialName);

  OriginTrials origin_trials(std::move(persistence_provider),
                             std::make_unique<blink::TrialTokenValidator>());
  base::flat_set<std::string> enabled_trials =
      origin_trials.GetPersistedTrialsForOrigin(
          trial_enabled_origin_, /*partition_origin=*/trial_enabled_origin_,
          kValidTime);
  EXPECT_TRUE(enabled_trials.empty());
}

// Check that a saved token is not returned if its signature has been disabled
// by policy
TEST_F(OriginTrialsTest, DisabledTokensNotReturned) {
  std::unique_ptr<test::TestPersistenceProvider> persistence_provider =
      std::make_unique<test::TestPersistenceProvider>();

  base::Time token_expiry = base::Time::FromTimeT(2000000000);
  base::flat_set<std::string> partition_sites = {
      GetTokenPartitionSite(trial_enabled_origin_)};
  base::flat_set<PersistedTrialToken> stored_tokens = {
      {kPersistentTrialName, token_expiry,
       blink::TrialToken::UsageRestriction::kNone, kDummyTokenSignature,
       partition_sites}};
  persistence_provider->SavePersistentTrialTokens(trial_enabled_origin_,
                                                  stored_tokens);

  origin_trial_policy_.DisableToken(kDummyTokenSignature);

  OriginTrials origin_trials(std::move(persistence_provider),
                             std::make_unique<blink::TrialTokenValidator>());
  base::flat_set<std::string> enabled_trials =
      origin_trials.GetPersistedTrialsForOrigin(
          trial_enabled_origin_, /*partition_origin=*/trial_enabled_origin_,
          kValidTime);
  EXPECT_TRUE(enabled_trials.empty());
}

TEST_F(OriginTrialsTest, UserDisabledTokensNotReturned) {
  std::unique_ptr<test::TestPersistenceProvider> persistence_provider =
      std::make_unique<test::TestPersistenceProvider>();

  base::Time token_expiry = base::Time::FromTimeT(2000000000);
  base::flat_set<std::string> partition_sites = {
      GetTokenPartitionSite(trial_enabled_origin_)};
  base::flat_set<PersistedTrialToken> stored_tokens = {
      {kPersistentTrialName, token_expiry,
       blink::TrialToken::UsageRestriction::kSubset, kDummyTokenSignature,
       partition_sites}};
  persistence_provider->SavePersistentTrialTokens(trial_enabled_origin_,
                                                  stored_tokens);

  origin_trial_policy_.DisableTrialForUser(kPersistentTrialName);

  OriginTrials origin_trials(std::move(persistence_provider),
                             std::make_unique<blink::TrialTokenValidator>());
  base::flat_set<std::string> enabled_trials =
      origin_trials.GetPersistedTrialsForOrigin(
          trial_enabled_origin_, /*partition_origin=*/trial_enabled_origin_,
          kValidTime);
  EXPECT_TRUE(enabled_trials.empty());
}

TEST_F(OriginTrialsTest, GracePeriodIsRespected) {
  std::vector<std::string> tokens = {kFrobulateManualCompletionToken};
  origin_trials_.PersistTrialsFromTokens(
      trial_enabled_origin_, /*partition_origin=*/trial_enabled_origin_, tokens,
      kValidTime);

  base::flat_set<std::string> enabled_trials =
      origin_trials_.GetPersistedTrialsForOrigin(
          trial_enabled_origin_, /*partition_origin=*/trial_enabled_origin_,
          kExpiryTime);
  EXPECT_EQ(1ul, enabled_trials.size())
      << "Expect trial to be valid at the expiry time limit";

  base::Time in_grace_period = kExpiryTime + base::Days(1);
  enabled_trials = origin_trials_.GetPersistedTrialsForOrigin(
      trial_enabled_origin_, /*partition_origin=*/trial_enabled_origin_,
      in_grace_period);
  EXPECT_EQ(1ul, enabled_trials.size())
      << "Expect trial to be valid within the grace period";

  base::Time end_of_grace_period = kExpiryTime + blink::kExpiryGracePeriod;
  enabled_trials = origin_trials_.GetPersistedTrialsForOrigin(
      trial_enabled_origin_, /*partition_origin=*/trial_enabled_origin_,
      end_of_grace_period);
  EXPECT_EQ(0ul, enabled_trials.size())
      << "Do not expect the trial to be valid after the grace period ends";
}

TEST_F(OriginTrialsTest, DoNotPersistTokensForOpaqueOrigins) {
  std::vector<std::string> tokens = {kFrobulatePersistentToken};
  url::Origin opaque_origin;
  // Opaque primary origin
  origin_trials_.PersistTrialsFromTokens(
      opaque_origin, /*partition_origin=*/trial_enabled_origin_, tokens,
      kValidTime);

  EXPECT_TRUE(origin_trials_
                  .GetPersistedTrialsForOrigin(
                      opaque_origin, /*partition_origin=*/trial_enabled_origin_,
                      kValidTime)
                  .empty());
}

TEST_F(OriginTrialsTest, PersistTokensInOpaquePartition) {
  std::vector<std::string> tokens = {kFrobulatePersistentToken};
  url::Origin opaque_origin;
  // Opaque partition origin
  origin_trials_.PersistTrialsFromTokens(trial_enabled_origin_,
                                         /*partition_origin=*/opaque_origin,
                                         tokens, kValidTime);

  EXPECT_TRUE(origin_trials_.IsTrialPersistedForOrigin(
      trial_enabled_origin_, /*partition_origin=*/opaque_origin,
      kPersistentTrialName, kValidTime));
}

TEST_F(OriginTrialsTest, TokensArePartitionedByTopLevelSite) {
  url::Origin origin_a = trial_enabled_origin_;
  url::Origin origin_b = url::Origin::Create(GURL(kTrialEnabledOriginB));
  url::Origin partition_site_a = origin_a;
  url::Origin partition_site_b = origin_b;
  std::vector<std::string> tokens_a = {kFrobulatePersistentToken};
  std::vector<std::string> tokens_b = {kFrobulatePersistentTokenAlternate};

  origin_trials_.PersistTrialsFromTokens(origin_a, partition_site_a, tokens_a,
                                         kValidTime);
  origin_trials_.PersistTrialsFromTokens(origin_a, partition_site_b, tokens_a,
                                         kValidTime);

  origin_trials_.PersistTrialsFromTokens(origin_b, partition_site_b, tokens_b,
                                         kValidTime);

  // Only expect trials to be enabled for partitions where they have been set
  EXPECT_TRUE(origin_trials_.IsTrialPersistedForOrigin(
      origin_a, partition_site_a, kPersistentTrialName, kValidTime));

  EXPECT_TRUE(origin_trials_.IsTrialPersistedForOrigin(
      origin_a, partition_site_b, kPersistentTrialName, kValidTime));

  EXPECT_TRUE(origin_trials_.IsTrialPersistedForOrigin(
      origin_b, partition_site_b, kPersistentTrialName, kValidTime));

  EXPECT_FALSE(origin_trials_.IsTrialPersistedForOrigin(
      origin_b, partition_site_a, kPersistentTrialName, kValidTime));

  // Removing a token should only be from one partition
  origin_trials_.PersistTrialsFromTokens(origin_a, partition_site_b, {},
                                         kValidTime);

  EXPECT_TRUE(origin_trials_.IsTrialPersistedForOrigin(
      origin_a, partition_site_a, kPersistentTrialName, kValidTime));

  EXPECT_FALSE(origin_trials_.IsTrialPersistedForOrigin(
      origin_a, partition_site_b, kPersistentTrialName, kValidTime));
}

TEST_F(OriginTrialsTest, PartitionSiteIsETLDPlusOne) {
  EXPECT_EQ("https://example.com",
            GetTokenPartitionSite(DomainAsOrigin("example.com")));
  EXPECT_EQ("https://example.com",
            GetTokenPartitionSite(DomainAsOrigin("enabled.example.com")));
  EXPECT_EQ("https://example.co.uk",
            GetTokenPartitionSite(DomainAsOrigin("example.co.uk")));
  EXPECT_EQ("https://example.co.uk",
            GetTokenPartitionSite(DomainAsOrigin("enabled.example.co.uk")));
}

TEST_F(OriginTrialsTest,
       PartitionSiteUsesPrivateRegistryAsEffectiveTopLevelDomain) {
  EXPECT_EQ("https://example.blogspot.com",
            GetTokenPartitionSite(DomainAsOrigin("example.blogspot.com")));
  EXPECT_EQ(
      "https://example.blogspot.com",
      GetTokenPartitionSite(DomainAsOrigin("enabled.example.blogspot.com")));
}

TEST_F(OriginTrialsTest, PartitionSiteCanBeIpAddress) {
  EXPECT_EQ("http://127.0.0.1",
            GetTokenPartitionSite(url::Origin::CreateFromNormalizedTuple(
                "http", "127.0.0.1", 80)));
}

TEST_F(OriginTrialsTest, PartitionSiteCanBeLocalhost) {
  EXPECT_EQ("http://localhost",
            GetTokenPartitionSite(url::Origin::CreateFromNormalizedTuple(
                "http", "localhost", 80)));
}

TEST_F(OriginTrialsTest, PartitionSiteCanHaveNonstandardPort) {
  EXPECT_EQ("http://example.com",
            GetTokenPartitionSite(url::Origin::CreateFromNormalizedTuple(
                "http", "enabled.example.com", 5555)));
}

TEST_F(OriginTrialsTest, OpaqueOriginAsPartitionSiteSerializesAsSentinelValue) {
  EXPECT_EQ(":opaque", GetTokenPartitionSite(url::Origin()));
}

}  // namespace origin_trials
