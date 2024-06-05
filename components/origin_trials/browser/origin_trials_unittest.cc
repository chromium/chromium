// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_trials/browser/origin_trials.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/origin_trials/common/persisted_trial_token.h"
#include "components/origin_trials/test/test_persistence_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/origin_trials/scoped_test_origin_trial_policy.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/blink/public/mojom/origin_trial_feature/origin_trial_feature.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace origin_trials {
namespace {

using blink::mojom::OriginTrialFeature;

const char kPersistentTrialName[] = "FrobulatePersistent";
const char kPersistentThirdPartyDeprecationTrialName[] =
    "FrobulatePersistentThirdPartyDeprecation";
const char kNonPersistentTrialName[] = "Frobulate";
const char kInvalidTrialName[] = "InvalidTrial";
const char kTrialEnabledOriginA[] = "https://enabled.example.com";
const char kTrialEnabledOriginASubdomain[] = "https://sub.enabled.example.com";
const char kTrialEnabledOriginASubdomainAlt[] =
    "https://sub_alt.enabled.example.com";
const char kTrialEnabledOriginB[] = "https://enabled.alternate.com";
const char kThirdPartyTrialEnabledOrigin[] = "https://enabled.thirdparty.com";
const char kThirdPartyTrialEnabledOriginSubdomain[] =
    "https://sub.enabled.thirdparty.com";
const char kThirdPartyTrialEnabledOriginSubdomainAlt[] =
    "https://sub_alt.enabled.thirdparty.com";

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

// Valid subdomain matching header token for FrobulatePersistent
// generated with
// tools/origin_trials/generate_token.py enabled.example.com FrobulatePersistent
// --expire-timestamp=2000000000 --is-subdomain
const char kFrobulatePersistentSubdomainToken[] =
    "A5Bhn4lxDwMBPL0fCS02PRfpmP0MPYk2vR7Ye5D8Kkzp9lp7bwc8QhjA8zgZHiaUttfZAZ/"
    "1EQCqD2uBtPESHwoAAAB6eyJvcmlnaW4iOiAiaHR0cHM6Ly9lbmFibGVkLmV4YW1wbGUuY29tO"
    "jQ0MyIsICJmZWF0dXJlIjogIkZyb2J1bGF0ZVBlcnNpc3RlbnQiLCAiZXhwaXJ5IjogMjAwMDA"
    "wMDAwMCwgImlzU3ViZG9tYWluIjogdHJ1ZX0=";

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

// Valid header token for FrobulatePersistentThirdPartyDeprecation
// generated with
// tools/origin_trials/generate_token.py enabled.example.com
// FrobulatePersistentThirdPartyDeprecation --expire-timestamp=2000000000
// --is-subdomain
const char kFrobulatePersistentThirdPartyDeprecationSubdomainToken[] =
    "A13ICJLZZpEfymaQfvnaaE3gT9FdseEGYtN6gYnCw0+"
    "48XhvDCtfT1rTCU6CAfrtB7j46aOYvvdO5gJ853FLkgYAAACPeyJvcmlnaW4iOiAiaHR0cHM6L"
    "y9lbmFibGVkLmV4YW1wbGUuY29tOjQ0MyIsICJmZWF0dXJlIjogIkZyb2J1bGF0ZVBlcnNpc3R"
    "lbnRUaGlyZFBhcnR5RGVwcmVjYXRpb24iLCAiZXhwaXJ5IjogMjAwMDAwMDAwMCwgImlzU3ViZ"
    "G9tYWluIjogdHJ1ZX0=";

// Valid third-party token for FrobulatePersistentThirdPartyDeprecation
// generated with
// tools/origin_trials/generate_token.py enabled.thirdparty.com
// FrobulatePersistentThirdPartyDeprecation --expire-timestamp=2000000000
// --is-third-party
const char kFrobulatePersistentThirdPartyDeprecationThirdPartyToken[] =
    "Az+ztSNd9o+3cmaiCk7QgSU5/2jSa1qiNKsoJOOxvMVxf/"
    "8xPWsKraWc0US05bYHmTAIdzZAxh1DMMRhMir5yg4AAACTeyJvcmlnaW4iOiAiaHR0cHM6Ly9l"
    "bmFibGVkLnRoaXJkcGFydHkuY29tOjQ0MyIsICJmZWF0dXJlIjogIkZyb2J1bGF0ZVBlcnNpc3"
    "RlbnRUaGlyZFBhcnR5RGVwcmVjYXRpb24iLCAiZXhwaXJ5IjogMjAwMDAwMDAwMCwgImlzVGhp"
    "cmRQYXJ0eSI6IHRydWV9";

// Valid subdomain matching third-party token for
// FrobulatePersistentThirdPartyDeprecation
// generated with
// tools/origin_trials/generate_token.py enabled.thirdparty.com
// FrobulatePersistentThirdPartyDeprecation --expire-timestamp=2000000000
// --is-subdomain --is-third-party
const char kFrobulatePersistentThirdPartyDeprecationSubdomainThirdPartyToken[] =
    "A9uzT+"
    "YSACBeKX1GM2pukXch9Zwb35MV9NBpoQYj2EQMeiw0TwyNcOFXWItCDpewHcPSnGUrOX73AtEq"
    "eP69KwcAAACoeyJvcmlnaW4iOiAiaHR0cHM6Ly9lbmFibGVkLnRoaXJkcGFydHkuY29tOjQ0My"
    "IsICJmZWF0dXJlIjogIkZyb2J1bGF0ZVBlcnNpc3RlbnRUaGlyZFBhcnR5RGVwcmVjYXRpb24i"
    "LCAiZXhwaXJ5IjogMjAwMDAwMDAwMCwgImlzU3ViZG9tYWluIjogdHJ1ZSwgImlzVGhpcmRQYX"
    "J0eSI6IHRydWV9";

const char kFrobulatePersistentInvalidOsToken[] =
    "Az7+hGm6XhszDNmzi9/cLyLCjiciNqCrtlIilym1+wg6c/owVYMJtjSx7Xjf8MHHLs3gzB/"
    "5D9/0PSSUOI/"
    "ujwoAAABueyJvcmlnaW4iOiAiaHR0cHM6Ly9lbmFibGVkLmV4YW1wbGUuY29tOjQ0MyIsICJmZ"
    "WF0dXJlIjogIkZyb2J1bGF0ZVBlcnNpc3RlbnRJbnZhbGlkT1MiLCAiZXhwaXJ5IjogMjAwMDA"
    "wMDAwMH0=";

const ukm::SourceId kFakeSourceId1 = 123456l;
const ukm::SourceId kFakeSourceId2 = 777888l;

class OpenScopedTestOriginTrialPolicy
    : public blink::ScopedTestOriginTrialPolicy {
 public:
  OpenScopedTestOriginTrialPolicy() = default;
  ~OpenScopedTestOriginTrialPolicy() override = default;

  // Check if the passed |trial_name| has been disabled.
  bool IsFeatureDisabled(std::string_view trial_name) const override {
    return disabled_trials_.contains(trial_name);
  }

  bool IsTokenDisabled(std::string_view token_signature) const override {
    return disabled_signatures_.contains(token_signature);
  }

  // Check if the passed |trial_name| has been disabled.
  bool IsFeatureDisabledForUser(std::string_view trial_name) const override {
    return user_disabled_trials_.contains(trial_name);
  }

  void DisableTrial(std::string_view trial) { disabled_trials_.emplace(trial); }

  void DisableToken(std::string_view token_signature) {
    disabled_signatures_.emplace(token_signature);
  }

  void DisableTrialForUser(std::string_view trial_name) {
    user_disabled_trials_.emplace(trial_name);
  }

 private:
  base::flat_set<std::string> disabled_trials_;
  base::flat_set<std::string> disabled_signatures_;
  base::flat_set<std::string> user_disabled_trials_;
};

class TestStatusObserver
    : public content::OriginTrialsControllerDelegate::Observer {
 public:
  explicit TestStatusObserver(const std::string& trial_name)
      : trial_name_(trial_name) {}

  TestStatusObserver(const TestStatusObserver&) = delete;
  TestStatusObserver& operator=(const TestStatusObserver&) = delete;

  void OnStatusChanged(const OriginTrialStatusChangeDetails& details) override {
    on_status_changed_count_++;
    last_status_change_ = details;
  }
  void OnPersistedTokensCleared() override {
    on_persisted_tokens_cleared_count_++;
  }
  std::string trial_name() override { return trial_name_; }

  int on_status_changed_count() { return on_status_changed_count_; }
  int on_persisted_tokens_cleared_count() {
    return on_persisted_tokens_cleared_count_;
  }
  const OriginTrialStatusChangeDetails& last_status_change() {
    return last_status_change_;
  }

 private:
  std::string trial_name_;
  OriginTrialStatusChangeDetails last_status_change_;
  int on_status_changed_count_ = 0;
  int on_persisted_tokens_cleared_count_ = 0;
};

}  // namespace

class OriginTrialsTest : public testing::Test {
 public:
  OriginTrialsTest()
      : origin_trials_(std::make_unique<test::TestPersistenceProvider>(),
                       std::make_unique<blink::TrialTokenValidator>()),
        trial_enabled_origin_(url::Origin::Create(GURL(kTrialEnabledOriginA))),
        trial_enabled_origin_subdomain_(
            url::Origin::Create(GURL(kTrialEnabledOriginASubdomain))) {}

  OriginTrialsTest(const OriginTrialsTest&) = delete;
  OriginTrialsTest& operator=(const OriginTrialsTest&) = delete;

  ~OriginTrialsTest() override = default;

  // PersistTrialsFromTokens using |origin| as partition origin.
  void PersistTrialsFromTokens(const url::Origin& origin,
                               base::span<std::string> tokens,
                               base::Time time,
                               std::optional<ukm::SourceId> source_id) {
    origin_trials_.PersistTrialsFromTokens(origin,
                                           /* partition_origin*/ origin, tokens,
                                           time, source_id);
  }

  // GetPersistedTrialsForOrigin using |trial_origin| as partition origin.
  base::flat_set<std::string> GetPersistedTrialsForOrigin(
      const url::Origin& trial_origin,
      base::Time lookup_time) {
    return origin_trials_.GetPersistedTrialsForOrigin(
        trial_origin, /* partition_origin */ trial_origin, lookup_time);
  }

  // IsFeaturePersistedForOrigin using |origin| as partition origin.
  bool IsFeaturePersistedForOrigin(const url::Origin& origin,
                                   blink::mojom::OriginTrialFeature feature,
                                   base::Time lookup_time) {
    return origin_trials_.IsFeaturePersistedForOrigin(
        origin, /* partition_origin */ origin, feature, lookup_time);
  }

  std::string GetTokenPartitionSite(const url::Origin& origin) {
    return OriginTrials::GetTokenPartitionSite(origin);
  }

  bool MatchesTokenOrigin(const url::Origin& token_origin,
                          bool match_subdomains,
                          const url::Origin& origin) {
    return origin_trials_.MatchesTokenOrigin(token_origin, match_subdomains,
                                             origin);
  }

  std::unique_ptr<TestStatusObserver> CreateAndAddObserver(
      const std::string& trial_name) {
    std::unique_ptr<TestStatusObserver> observer =
        std::make_unique<TestStatusObserver>(trial_name);
    origin_trials_.AddObserver(observer.get());

    return observer;
  }

  void RemoveObserver(TestStatusObserver* observer) {
    origin_trials_.RemoveObserver(observer);
  }

  OriginTrials::ObserverMap& GetObserverMap() {
    return origin_trials_.observer_map_;
  }

  // Test helper that creates an origin for the domain_name with https scheme
  // and port 443.
  url::Origin DomainAsOrigin(const std::string& domain_name) {
    return url::Origin::CreateFromNormalizedTuple("https", domain_name, 443);
  }

 protected:
  OriginTrials origin_trials_;
  url::Origin trial_enabled_origin_;
  url::Origin trial_enabled_origin_subdomain_;
  OpenScopedTestOriginTrialPolicy origin_trial_policy_;
};

TEST_F(OriginTrialsTest, CleanObjectHasNoPersistentTrials) {
  EXPECT_TRUE(
      GetPersistedTrialsForOrigin(trial_enabled_origin_, kValidTime).empty());
}

TEST_F(OriginTrialsTest, EnabledTrialsArePersisted) {
  std::vector<std::string> tokens = {kFrobulatePersistentToken};
  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime,
                          /*source_id=*/std::nullopt);

  base::flat_set<std::string> enabled_trials =
      GetPersistedTrialsForOrigin(trial_enabled_origin_, kValidTime);
  ASSERT_EQ(1ul, enabled_trials.size());
  EXPECT_TRUE(enabled_trials.contains(kPersistentTrialName));
}

TEST_F(OriginTrialsTest, OnlyPersistentTrialsAreEnabled) {
  std::vector<std::string> tokens = {kFrobulateToken,
                                     kFrobulatePersistentToken};
  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime,
                          /*source_id=*/std::nullopt);

  base::flat_set<std::string> enabled_trials =
      GetPersistedTrialsForOrigin(trial_enabled_origin_, kValidTime);
  ASSERT_EQ(1ul, enabled_trials.size());
  EXPECT_TRUE(enabled_trials.contains(kPersistentTrialName));
  EXPECT_FALSE(enabled_trials.contains(kNonPersistentTrialName));
}

TEST_F(OriginTrialsTest, ResetClearsPersistedTrials) {
  std::vector<std::string> tokens = {kFrobulatePersistentToken};
  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime,
                          /*source_id=*/std::nullopt);

  EXPECT_FALSE(
      GetPersistedTrialsForOrigin(trial_enabled_origin_, kValidTime).empty());

  tokens = {};
  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime,
                          /*source_id=*/std::nullopt);

  EXPECT_TRUE(
      GetPersistedTrialsForOrigin(trial_enabled_origin_, kValidTime).empty());
}

TEST_F(OriginTrialsTest, TrialNotEnabledByDefault) {
  EXPECT_FALSE(IsFeaturePersistedForOrigin(
      trial_enabled_origin_,
      OriginTrialFeature::kOriginTrialsSampleAPIPersistentFeature, kValidTime));
}

TEST_F(OriginTrialsTest, TrialEnablesFeature) {
  std::vector<std::string> tokens = {kFrobulatePersistentToken};
  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime,
                          /*source_id=*/std::nullopt);

  EXPECT_TRUE(IsFeaturePersistedForOrigin(
      trial_enabled_origin_,
      OriginTrialFeature::kOriginTrialsSampleAPIPersistentFeature, kValidTime));
}

TEST_F(OriginTrialsTest, TrialDoesNotEnableOtherFeatures) {
  std::vector<std::string> tokens = {kFrobulatePersistentToken};
  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime,
                          /*source_id=*/std::nullopt);

  EXPECT_FALSE(IsFeaturePersistedForOrigin(
      trial_enabled_origin_, OriginTrialFeature::kOriginTrialsSampleAPI,
      kValidTime));
}

TEST_F(OriginTrialsTest, TrialIsNotEnabledOrPersistedOnInvalidOs) {
  std::vector<std::string> tokens = {kFrobulatePersistentInvalidOsToken};
  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime,
                          /*source_id=*/std::nullopt);

  EXPECT_FALSE(IsFeaturePersistedForOrigin(
      trial_enabled_origin_,
      OriginTrialFeature::kOriginTrialsSampleAPIPersistentInvalidOS,
      kValidTime));

  base::flat_set<std::string> enabled_trials =
      GetPersistedTrialsForOrigin(trial_enabled_origin_, kValidTime);
  ASSERT_TRUE(enabled_trials.empty());
}

TEST_F(OriginTrialsTest, TokensCanBeAppended) {
  std::vector<std::string> tokens = {kFrobulatePersistentToken};
  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime,
                          /*source_id=*/std::nullopt);

  EXPECT_TRUE(IsFeaturePersistedForOrigin(
      trial_enabled_origin_,
      OriginTrialFeature::kOriginTrialsSampleAPIPersistentFeature, kValidTime));
  EXPECT_FALSE(IsFeaturePersistedForOrigin(
      trial_enabled_origin_,
      OriginTrialFeature::kOriginTrialsSampleAPIPersistentExpiryGracePeriod,
      kValidTime));

  // Append an additional token for the same origin
  std::vector<std::string> additional_tokens = {
      kFrobulatePersistentExpiryGracePeriodToken};
  origin_trials_.PersistAdditionalTrialsFromTokens(
      trial_enabled_origin_, /*partition_origin=*/trial_enabled_origin_,
      /*script_origins=*/{}, additional_tokens, kValidTime,
      /*source_id=*/std::nullopt);
  // Check that both trials are now enabled
  EXPECT_TRUE(IsFeaturePersistedForOrigin(
      trial_enabled_origin_,
      OriginTrialFeature::kOriginTrialsSampleAPIPersistentFeature, kValidTime));
  EXPECT_TRUE(IsFeaturePersistedForOrigin(
      trial_enabled_origin_,
      OriginTrialFeature::kOriginTrialsSampleAPIPersistentExpiryGracePeriod,
      kValidTime));
}

TEST_F(OriginTrialsTest, ThirdPartyTokensCanBeAppendedOnlyIfDeprecation) {
  // TODO(crbug.com/40257643): Change test when all 3P tokens are supported.
  // Append third-party tokens.
  std::vector<std::string> third_party_tokens = {
      kFrobulatePersistentThirdPartyToken,
      kFrobulatePersistentThirdPartyDeprecationThirdPartyToken};
  url::Origin script_origin =
      url::Origin::Create(GURL(kThirdPartyTrialEnabledOrigin));
  std::vector<url::Origin> script_origins = {script_origin};

  origin_trials_.PersistAdditionalTrialsFromTokens(
      trial_enabled_origin_, /*partition_origin=*/trial_enabled_origin_,
      script_origins, third_party_tokens, kValidTime,
      /*source_id=*/std::nullopt);

  // The FrobulatePersistent should not be persisted, as it is not a deprecation
  // token.
  EXPECT_FALSE(origin_trials_.IsFeaturePersistedForOrigin(
      script_origin, /*partition_origin=*/trial_enabled_origin_,
      OriginTrialFeature::kOriginTrialsSampleAPIPersistentFeature, kValidTime));

  // FrobulatePersistentThirdPartyDeprecation is a deprecation trial, and should
  // be enabled.
  EXPECT_TRUE(origin_trials_.IsFeaturePersistedForOrigin(
      script_origin, /*partition_origin=*/trial_enabled_origin_,
      OriginTrialFeature::
          kOriginTrialsSampleAPIPersistentThirdPartyDeprecationFeature,
      kValidTime));
}

TEST_F(OriginTrialsTest, SubdomainTokensEnableTrialForSubdomainsOfTokenOrigin) {
  std::vector<std::string> tokens = {kFrobulatePersistentSubdomainToken};
  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime,
                          /*source_id=*/std::nullopt);

  base::flat_set<std::string> origin_enabled_trials =
      GetPersistedTrialsForOrigin(trial_enabled_origin_, kValidTime);
  ASSERT_EQ(1ul, origin_enabled_trials.size());
  EXPECT_TRUE(origin_enabled_trials.contains(kPersistentTrialName));

  base::flat_set<std::string> origin_subdomain_enabled_trials =
      GetPersistedTrialsForOrigin(trial_enabled_origin_subdomain_, kValidTime);
  ASSERT_EQ(1ul, origin_subdomain_enabled_trials.size());
  EXPECT_TRUE(origin_subdomain_enabled_trials.contains(kPersistentTrialName));

  url::Origin trial_enabled_origin_alt_subdomain =
      url::Origin::Create(GURL(kTrialEnabledOriginASubdomainAlt));

  base::flat_set<std::string> origin_alt_subdomain_enabled_trials =
      GetPersistedTrialsForOrigin(trial_enabled_origin_alt_subdomain,
                                  kValidTime);
  ASSERT_EQ(1ul, origin_alt_subdomain_enabled_trials.size());
  EXPECT_TRUE(
      origin_alt_subdomain_enabled_trials.contains(kPersistentTrialName));
}

TEST_F(OriginTrialsTest,
       TrialNotEnabledForNonSubdomainsOfSubdomainTokenOrigin) {
  std::vector<std::string> tokens = {kFrobulatePersistentSubdomainToken};
  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime,
                          /*source_id=*/std::nullopt);

  // The trial should not be enabled for https://example.com since
  // `trial_enabled_origin_` (https://enabled.example.com) is a subdomain of it.
  url::Origin enabled_origin_etld =
      url::Origin::Create(GURL("https://example.com"));
  ASSERT_TRUE(
      GetPersistedTrialsForOrigin(enabled_origin_etld, kValidTime).empty());

  url::Origin alternate_origin =
      url::Origin::Create(GURL("https://alternate.com"));
  ASSERT_TRUE(
      GetPersistedTrialsForOrigin(alternate_origin, kValidTime).empty());
}

// Verifies that the trial is enabled for the token origin when a
// subdomain-matching token is provided by a subdomain of the token origin.
TEST_F(OriginTrialsTest, SubdomainTokensEnableTrialForTokenOrigin) {
  // Provide a subdomain-matching token from a subdomain
  // (`trial_enabled_origin_subdomain_`) of the token origin
  // (`trial_enabled_origin_`).
  std::vector<std::string> tokens = {kFrobulatePersistentSubdomainToken};
  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime,
                          /*source_id=*/std::nullopt);

  base::flat_set<std::string> origin_enabled_trials =
      GetPersistedTrialsForOrigin(trial_enabled_origin_, kValidTime);
  ASSERT_EQ(1ul, origin_enabled_trials.size());
  EXPECT_TRUE(origin_enabled_trials.contains(kPersistentTrialName));

  base::flat_set<std::string> origin_subdomain_enabled_trials =
      GetPersistedTrialsForOrigin(trial_enabled_origin_subdomain_, kValidTime);
  ASSERT_EQ(1ul, origin_subdomain_enabled_trials.size());
  EXPECT_TRUE(origin_subdomain_enabled_trials.contains(kPersistentTrialName));
}

TEST_F(OriginTrialsTest,
       ThirdPartySubdomainTokensEnableTrialForSubdomainsOfTokenOrigin) {
  // TODO(crbug.com/40257643): Also test 3P tokens for non-deprecation trials
  // when those are supported.

  // Append third-party tokens.
  std::vector<std::string> third_party_tokens = {
      kFrobulatePersistentThirdPartyDeprecationSubdomainThirdPartyToken};
  url::Origin script_origin =
      url::Origin::Create(GURL(kThirdPartyTrialEnabledOrigin));
  std::vector<url::Origin> script_origins = {script_origin};

  origin_trials_.PersistAdditionalTrialsFromTokens(
      trial_enabled_origin_, /*partition_origin=*/trial_enabled_origin_,
      script_origins, third_party_tokens, kValidTime,
      /*source_id=*/std::nullopt);

  url::Origin script_origin_subdomain =
      url::Origin::Create(GURL(kThirdPartyTrialEnabledOriginSubdomain));
  EXPECT_TRUE(origin_trials_.IsFeaturePersistedForOrigin(
      script_origin_subdomain, /*partition_origin=*/trial_enabled_origin_,
      OriginTrialFeature::
          kOriginTrialsSampleAPIPersistentThirdPartyDeprecationFeature,
      kValidTime));

  url::Origin script_origin_alt_subdomain =
      url::Origin::Create(GURL(kThirdPartyTrialEnabledOriginSubdomainAlt));
  EXPECT_TRUE(origin_trials_.IsFeaturePersistedForOrigin(
      script_origin_alt_subdomain, /*partition_origin=*/trial_enabled_origin_,
      OriginTrialFeature::
          kOriginTrialsSampleAPIPersistentThirdPartyDeprecationFeature,
      kValidTime));
}

// Verifies that a subdomain-matching header token being provided by a subdomain
// of the token origin appends the token for the token origin (instead of
// overwriting).
TEST_F(OriginTrialsTest,
       SubdomainTokensProvidedBySubdomainsAppendForTokenOrigin) {
  // Provide a subdomain-matching token for `FrobulatePersistent` from a
  // subdomain
  // (`trial_enabled_origin_subdomain_`) of the token origin
  // (`trial_enabled_origin_`).
  std::vector<std::string> tokens = {kFrobulatePersistentSubdomainToken};
  PersistTrialsFromTokens(trial_enabled_origin_subdomain_, tokens, kValidTime,
                          /*source_id=*/std::nullopt);

  {
    base::flat_set<std::string> origin_enabled_trials =
        GetPersistedTrialsForOrigin(trial_enabled_origin_, kValidTime);
    ASSERT_EQ(1ul, origin_enabled_trials.size());
    EXPECT_TRUE(origin_enabled_trials.contains(kPersistentTrialName));

    base::flat_set<std::string> origin_subdomain_enabled_trials =
        GetPersistedTrialsForOrigin(trial_enabled_origin_subdomain_,
                                    kValidTime);
    ASSERT_EQ(1ul, origin_subdomain_enabled_trials.size());
    EXPECT_TRUE(origin_subdomain_enabled_trials.contains(kPersistentTrialName));
  }

  // Provide a subdomain-matching token for
  // `FrobulatePersistentThirdPartyDeprecation` from a subdomain
  // (`trial_enabled_origin_subdomain_`) of the token origin
  // (`trial_enabled_origin_`).
  std::vector<std::string> different_tokens = {
      kFrobulatePersistentThirdPartyDeprecationSubdomainToken};
  PersistTrialsFromTokens(trial_enabled_origin_subdomain_, different_tokens,
                          kValidTime,
                          /*source_id=*/std::nullopt);

  {
    base::flat_set<std::string> origin_enabled_trials =
        GetPersistedTrialsForOrigin(trial_enabled_origin_, kValidTime);
    EXPECT_EQ(2ul, origin_enabled_trials.size());
    EXPECT_TRUE(origin_enabled_trials.contains(kPersistentTrialName));
    EXPECT_TRUE(origin_enabled_trials.contains(
        kPersistentThirdPartyDeprecationTrialName));

    base::flat_set<std::string> origin_subdomain_enabled_trials =
        GetPersistedTrialsForOrigin(trial_enabled_origin_subdomain_,
                                    kValidTime);
    EXPECT_EQ(2ul, origin_subdomain_enabled_trials.size());
    EXPECT_TRUE(origin_subdomain_enabled_trials.contains(kPersistentTrialName));
    EXPECT_TRUE(origin_subdomain_enabled_trials.contains(
        kPersistentThirdPartyDeprecationTrialName));
  }
}

// Checks that when a trial is enabled using a subdomain-matching token, the
// trial is not disabled by subdomains of the token origin, even if the
// subdomain provided the token.
TEST_F(OriginTrialsTest, SubdomainTokenNotDisabledBySubdomain) {
  // Provide a subdomain-matching token from a subdomain
  // (`trial_enabled_origin_subdomain_`) of the token origin
  // (`trial_enabled_origin_`).
  std::vector<std::string> tokens = {kFrobulatePersistentSubdomainToken};
  PersistTrialsFromTokens(trial_enabled_origin_subdomain_, tokens, kValidTime,
                          /*source_id=*/std::nullopt);

  {
    base::flat_set<std::string> origin_enabled_trials =
        GetPersistedTrialsForOrigin(trial_enabled_origin_, kValidTime);
    ASSERT_EQ(1ul, origin_enabled_trials.size());
    EXPECT_TRUE(origin_enabled_trials.contains(kPersistentTrialName));

    base::flat_set<std::string> origin_subdomain_enabled_trials =
        GetPersistedTrialsForOrigin(trial_enabled_origin_subdomain_,
                                    kValidTime);
    ASSERT_EQ(1ul, origin_subdomain_enabled_trials.size());
    EXPECT_TRUE(origin_subdomain_enabled_trials.contains(kPersistentTrialName));
  }

  // Clear the tokens for the subdomain that provided the token
  // (`trial_enabled_origin_subdomain_`).
  PersistTrialsFromTokens(trial_enabled_origin_subdomain_,
                          std::vector<std::string>(), kValidTime,
                          /*source_id=*/std::nullopt);

  {
    base::flat_set<std::string> origin_enabled_trials =
        GetPersistedTrialsForOrigin(trial_enabled_origin_, kValidTime);
    EXPECT_EQ(1ul, origin_enabled_trials.size());
    EXPECT_TRUE(origin_enabled_trials.contains(kPersistentTrialName));

    base::flat_set<std::string> origin_subdomain_enabled_trials =
        GetPersistedTrialsForOrigin(trial_enabled_origin_subdomain_,
                                    kValidTime);
    EXPECT_EQ(1ul, origin_subdomain_enabled_trials.size());
    EXPECT_TRUE(origin_subdomain_enabled_trials.contains(kPersistentTrialName));
  }
}

// Checks that when a trial is enabled using a subdomain-matching token, the
// trial is disabled if the origin that's stored in the token is loaded without
// it.
TEST_F(OriginTrialsTest, SubdomainTokenDisabledByTokenOrigin) {
  // Provide a subdomain-matching token from a subdomain
  // (`trial_enabled_origin_subdomain_`) of the token origin
  // (`trial_enabled_origin_`).
  std::vector<std::string> tokens = {kFrobulatePersistentSubdomainToken};
  PersistTrialsFromTokens(trial_enabled_origin_subdomain_, tokens, kValidTime,
                          /*source_id=*/std::nullopt);

  {
    base::flat_set<std::string> origin_enabled_trials =
        GetPersistedTrialsForOrigin(trial_enabled_origin_, kValidTime);
    ASSERT_EQ(1ul, origin_enabled_trials.size());
    EXPECT_TRUE(origin_enabled_trials.contains(kPersistentTrialName));

    base::flat_set<std::string> origin_subdomain_enabled_trials =
        GetPersistedTrialsForOrigin(trial_enabled_origin_subdomain_,
                                    kValidTime);
    ASSERT_EQ(1ul, origin_subdomain_enabled_trials.size());
    EXPECT_TRUE(origin_subdomain_enabled_trials.contains(kPersistentTrialName));
  }

  // Clear the tokens for the token origin (`trial_enabled_origin_`).
  PersistTrialsFromTokens(trial_enabled_origin_, std::vector<std::string>(),
                          kValidTime, /*source_id=*/std::nullopt);

  {
    base::flat_set<std::string> origin_enabled_trials =
        GetPersistedTrialsForOrigin(trial_enabled_origin_, kValidTime);
    ASSERT_TRUE(origin_enabled_trials.empty());

    base::flat_set<std::string> origin_subdomain_enabled_trials =
        GetPersistedTrialsForOrigin(trial_enabled_origin_subdomain_,
                                    kValidTime);
    ASSERT_TRUE(origin_subdomain_enabled_trials.empty());
  }
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
      {/*match_subdomains=*/false, kNonPersistentTrialName, token_expiry,
       blink::TrialToken::UsageRestriction::kNone, kDummyTokenSignature,
       partition_sites},
      {/*match_subdomains=*/false, kInvalidTrialName, token_expiry,
       blink::TrialToken::UsageRestriction::kNone, kDummyTokenSignature,
       partition_sites},
      {/*match_subdomains=*/false, kPersistentTrialName, token_expiry,
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

TEST_F(OriginTrialsTest, StatusObserverIsAdded) {
  std::unique_ptr<TestStatusObserver> observer =
      CreateAndAddObserver(kPersistentTrialName);

  EXPECT_TRUE(
      GetObserverMap()[kPersistentTrialName].HasObserver(observer.get()));
}

TEST_F(OriginTrialsTest, StatusObserverIsRemoved) {
  std::unique_ptr<TestStatusObserver> observer =
      CreateAndAddObserver(kPersistentTrialName);

  ASSERT_TRUE(
      GetObserverMap()[kPersistentTrialName].HasObserver(observer.get()));

  RemoveObserver(observer.get());

  EXPECT_FALSE(
      GetObserverMap()[kPersistentTrialName].HasObserver(observer.get()));
}

TEST_F(OriginTrialsTest, NotifyOnEnable) {
  std::unique_ptr<TestStatusObserver> observer =
      CreateAndAddObserver(kPersistentTrialName);

  std::vector<std::string> tokens = {kFrobulatePersistentToken};
  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime,
                          kFakeSourceId1);

  EXPECT_EQ(observer->on_status_changed_count(), 1);
  EXPECT_EQ(observer->on_persisted_tokens_cleared_count(), 0);
  EXPECT_EQ(
      observer->last_status_change(),
      OriginTrialStatusChangeDetails(
          trial_enabled_origin_, GetTokenPartitionSite(trial_enabled_origin_),
          /*match_subdomains=*/false, /*enabled=*/true, kFakeSourceId1));
}

TEST_F(OriginTrialsTest, NotifyOnEnableWithSubdomainMatching) {
  std::unique_ptr<TestStatusObserver> observer =
      CreateAndAddObserver(kPersistentTrialName);

  std::vector<std::string> tokens = {kFrobulatePersistentSubdomainToken};
  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime,
                          kFakeSourceId1);

  EXPECT_EQ(observer->on_status_changed_count(), 1);
  EXPECT_EQ(observer->on_persisted_tokens_cleared_count(), 0);
  EXPECT_EQ(
      observer->last_status_change(),
      OriginTrialStatusChangeDetails(
          trial_enabled_origin_, GetTokenPartitionSite(trial_enabled_origin_),
          /*match_subdomains=*/true, /*enabled=*/true, kFakeSourceId1));
}

TEST_F(OriginTrialsTest, NotifyOnDisable) {
  std::vector<std::string> tokens = {kFrobulatePersistentToken};
  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime,
                          kFakeSourceId1);

  std::unique_ptr<TestStatusObserver> observer =
      CreateAndAddObserver(kPersistentTrialName);

  tokens = {};
  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime,
                          kFakeSourceId1);

  EXPECT_EQ(observer->on_status_changed_count(), 1);
  EXPECT_EQ(observer->on_persisted_tokens_cleared_count(), 0);
  EXPECT_EQ(
      observer->last_status_change(),
      OriginTrialStatusChangeDetails(
          trial_enabled_origin_, GetTokenPartitionSite(trial_enabled_origin_),
          /*match_subdomains=*/false, /*enabled=*/false, kFakeSourceId1));
}

TEST_F(OriginTrialsTest, NotifyOnDisableWithSubdomainMatching) {
  std::vector<std::string> tokens = {kFrobulatePersistentSubdomainToken};
  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime,
                          kFakeSourceId1);

  std::unique_ptr<TestStatusObserver> observer =
      CreateAndAddObserver(kPersistentTrialName);

  tokens = {};
  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime,
                          kFakeSourceId1);

  EXPECT_EQ(observer->on_status_changed_count(), 1);
  EXPECT_EQ(observer->on_persisted_tokens_cleared_count(), 0);
  EXPECT_EQ(
      observer->last_status_change(),
      OriginTrialStatusChangeDetails(
          trial_enabled_origin_, GetTokenPartitionSite(trial_enabled_origin_),
          /*match_subdomains=*/true, /*enabled=*/false, kFakeSourceId1));
}

// Verifies that the origin stored in a subdomain-matching token is provided in
// the notification when one of its subdomain uses the token to enable a trial.
TEST_F(OriginTrialsTest, NotifyUsingTokenOriginOnEnable) {
  std::unique_ptr<TestStatusObserver> observer =
      CreateAndAddObserver(kPersistentTrialName);

  // Provide a subdomain-matching token from a subdomain
  // (`trial_enabled_origin_subdomain_`) of the token origin
  // (`trial_enabled_origin_`).
  std::vector<std::string> tokens = {kFrobulatePersistentSubdomainToken};
  PersistTrialsFromTokens(trial_enabled_origin_subdomain_, tokens, kValidTime,
                          kFakeSourceId1);

  EXPECT_EQ(observer->on_status_changed_count(), 1);
  EXPECT_EQ(
      observer->last_status_change(),
      OriginTrialStatusChangeDetails(
          trial_enabled_origin_, GetTokenPartitionSite(trial_enabled_origin_),
          /*match_subdomains=*/true, /*enabled=*/true, kFakeSourceId1));
}

TEST_F(OriginTrialsTest, DontNotifyOnDisableIfNotPreviouslyEnabled) {
  std::unique_ptr<TestStatusObserver> observer =
      CreateAndAddObserver(kPersistentTrialName);

  EXPECT_TRUE(
      GetPersistedTrialsForOrigin(trial_enabled_origin_, kValidTime).empty());

  PersistTrialsFromTokens(trial_enabled_origin_, {}, kValidTime,
                          kFakeSourceId1);

  EXPECT_EQ(observer->on_status_changed_count(), 0);
}

TEST_F(OriginTrialsTest, NotifyOnEnabledOnlyIfPreviouslyDisabled) {
  std::unique_ptr<TestStatusObserver> observer =
      CreateAndAddObserver(kPersistentTrialName);

  EXPECT_TRUE(
      GetPersistedTrialsForOrigin(trial_enabled_origin_, kValidTime).empty());

  std::vector<std::string> tokens = {kFrobulatePersistentToken};
  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime,
                          kFakeSourceId1);

  EXPECT_EQ(observer->on_status_changed_count(), 1);
  EXPECT_EQ(
      observer->last_status_change(),
      OriginTrialStatusChangeDetails(
          trial_enabled_origin_, GetTokenPartitionSite(trial_enabled_origin_),
          /*match_subdomains=*/false, /*enabled=*/true, kFakeSourceId1));

  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime,
                          kFakeSourceId1);
  origin_trials_.PersistAdditionalTrialsFromTokens(
      trial_enabled_origin_, /*partition_origin=*/trial_enabled_origin_,
      /*script_origins=*/{}, tokens, kValidTime, kFakeSourceId1);

  EXPECT_EQ(observer->on_status_changed_count(), 1);
}

TEST_F(OriginTrialsTest, NotifyOnStatusChangeMultiplePartitionSites) {
  url::Origin origin_a = trial_enabled_origin_;
  url::Origin origin_b = url::Origin::Create(GURL(kTrialEnabledOriginB));
  std::vector<std::string> tokens = {kFrobulatePersistentToken};
  std::unique_ptr<TestStatusObserver> observer =
      CreateAndAddObserver(kPersistentTrialName);

  // Enable trial for `trial_enabled_origin_`, partitioned under `origin_a`.
  origin_trials_.PersistTrialsFromTokens(trial_enabled_origin_, origin_a,
                                         tokens, kValidTime, kFakeSourceId1);
  EXPECT_EQ(observer->on_status_changed_count(), 1);
  EXPECT_EQ(observer->last_status_change(),
            OriginTrialStatusChangeDetails(
                trial_enabled_origin_, GetTokenPartitionSite(origin_a),
                /*match_subdomains=*/false, /*enabled=*/true, kFakeSourceId1));

  // Enable trial for `trial_enabled_origin_`, partitioned under `origin_b`.
  origin_trials_.PersistTrialsFromTokens(trial_enabled_origin_, origin_b,
                                         tokens, kValidTime, kFakeSourceId2);
  EXPECT_EQ(observer->on_status_changed_count(), 2);
  EXPECT_EQ(observer->last_status_change(),
            OriginTrialStatusChangeDetails(
                trial_enabled_origin_, GetTokenPartitionSite(origin_b),
                /*match_subdomains=*/false, /*enabled=*/true, kFakeSourceId2));

  // Disable trial for `trial_enabled_origin_`, partitioned under `origin_a`.
  tokens = {};
  origin_trials_.PersistTrialsFromTokens(trial_enabled_origin_, origin_a,
                                         tokens, kValidTime, kFakeSourceId1);
  EXPECT_EQ(observer->on_status_changed_count(), 3);
  EXPECT_EQ(observer->last_status_change(),
            OriginTrialStatusChangeDetails(
                trial_enabled_origin_, GetTokenPartitionSite(origin_a),
                /*match_subdomains=*/false, /*enabled=*/false, kFakeSourceId1));

  // Disable trial for `trial_enabled_origin_`, partitioned under `origin_b`.
  tokens = {};
  origin_trials_.PersistTrialsFromTokens(trial_enabled_origin_, origin_b,
                                         tokens, kValidTime, kFakeSourceId2);
  EXPECT_EQ(observer->on_status_changed_count(), 4);
  EXPECT_EQ(observer->last_status_change(),
            OriginTrialStatusChangeDetails(
                trial_enabled_origin_, GetTokenPartitionSite(origin_b),
                /*match_subdomains=*/false, /*enabled=*/false, kFakeSourceId2));
}

// Check that observers are only notified of status change events for the trial
// corresponding to their `trial_name` value.
TEST_F(OriginTrialsTest, NotifyForCorrectTrial) {
  url::Origin origin_a = trial_enabled_origin_;
  url::Origin origin_b =
      url::Origin::Create(GURL(kThirdPartyTrialEnabledOrigin));
  std::vector<std::string> tokens_a = {kFrobulatePersistentToken};
  std::vector<std::string> tokens_b = {
      kFrobulatePersistentThirdPartyDeprecationThirdPartyToken};

  std::unique_ptr<TestStatusObserver> observer_a =
      CreateAndAddObserver(kPersistentTrialName);
  std::unique_ptr<TestStatusObserver> observer_b =
      CreateAndAddObserver(kPersistentThirdPartyDeprecationTrialName);

  // Enable `kPersistentTrialName` for `origin_a`.
  origin_trials_.PersistTrialsFromTokens(origin_a,
                                         /*partition_origin=*/origin_a,
                                         tokens_a, kValidTime, kFakeSourceId1);

  EXPECT_EQ(observer_a->on_status_changed_count(), 1);
  EXPECT_EQ(observer_a->last_status_change(),
            OriginTrialStatusChangeDetails(
                origin_a, GetTokenPartitionSite(origin_a),
                /*match_subdomains=*/false, /*enabled=*/true, kFakeSourceId1));
  EXPECT_EQ(observer_b->on_status_changed_count(), 0);

  // Enable `kPersistentThirdPartyDeprecationTrialName` for `origin_b`
  // (partitioned under `origin_a`).
  std::vector<url::Origin> script_origins = {origin_b};
  origin_trials_.PersistAdditionalTrialsFromTokens(
      origin_a, /*partition_origin=*/origin_a, script_origins, tokens_b,
      kValidTime, kFakeSourceId1);

  EXPECT_EQ(observer_a->on_status_changed_count(), 1);
  EXPECT_EQ(observer_b->on_status_changed_count(), 1);
  EXPECT_EQ(observer_b->last_status_change(),
            OriginTrialStatusChangeDetails(
                origin_b, GetTokenPartitionSite(origin_a),
                /*match_subdomains=*/false, /*enabled=*/true, kFakeSourceId1));

  // Disable `kPersistentTrialName` for `origin_a`.
  tokens_a = {};
  origin_trials_.PersistTrialsFromTokens(origin_a,
                                         /*partition_origin=*/origin_a,
                                         tokens_a, kValidTime, kFakeSourceId1);

  EXPECT_EQ(observer_a->on_status_changed_count(), 2);
  EXPECT_EQ(observer_a->last_status_change(),
            OriginTrialStatusChangeDetails(
                origin_a, GetTokenPartitionSite(origin_a),
                /*match_subdomains=*/false, /*enabled=*/false, kFakeSourceId1));
  EXPECT_EQ(observer_b->on_status_changed_count(), 1);
  EXPECT_EQ(observer_b->last_status_change(),
            OriginTrialStatusChangeDetails(
                origin_b, GetTokenPartitionSite(origin_a),
                /*match_subdomains=*/false, /*enabled=*/true, kFakeSourceId1));
}

TEST_F(OriginTrialsTest, NotifyWithTokenOriginForSubdomainTokens) {
  std::unique_ptr<TestStatusObserver> observer =
      CreateAndAddObserver(kPersistentTrialName);

  std::vector<std::string> tokens = {kFrobulatePersistentSubdomainToken};
  PersistTrialsFromTokens(trial_enabled_origin_subdomain_, tokens, kValidTime,
                          kFakeSourceId1);

  EXPECT_EQ(observer->on_status_changed_count(), 1);
  EXPECT_EQ(observer->on_persisted_tokens_cleared_count(), 0);
  EXPECT_EQ(observer->last_status_change(),
            OriginTrialStatusChangeDetails(
                trial_enabled_origin_,
                GetTokenPartitionSite(trial_enabled_origin_subdomain_),
                /*match_subdomains=*/true, /*enabled=*/true, kFakeSourceId1));
}

TEST_F(OriginTrialsTest, NotifyOnPersistedTokensCleared) {
  std::vector<std::string> tokens = {kFrobulatePersistentToken};
  PersistTrialsFromTokens(trial_enabled_origin_, tokens, kValidTime,
                          kFakeSourceId1);

  std::unique_ptr<TestStatusObserver> observer =
      CreateAndAddObserver(kPersistentTrialName);
  EXPECT_EQ(observer->on_persisted_tokens_cleared_count(), 0);

  origin_trials_.ClearPersistedTokens();

  EXPECT_EQ(observer->on_persisted_tokens_cleared_count(), 1);
  EXPECT_EQ(observer->on_status_changed_count(), 0);
}

TEST_F(OriginTrialsTest, NotifyOnPersistedTokensClearedNoTokens) {
  std::unique_ptr<TestStatusObserver> observer =
      CreateAndAddObserver(kPersistentTrialName);
  EXPECT_EQ(observer->on_persisted_tokens_cleared_count(), 0);

  origin_trials_.ClearPersistedTokens();

  EXPECT_EQ(observer->on_persisted_tokens_cleared_count(), 1);
  EXPECT_EQ(observer->on_status_changed_count(), 0);
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
      {/*match_subdomains=*/false, kPersistentTrialName, token_expiry,
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
      {/*match_subdomains=*/false, kPersistentTrialName, token_expiry,
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
      {/*match_subdomains=*/false, kPersistentTrialName, token_expiry,
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
      kValidTime, /*source_id=*/std::nullopt);

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
      kValidTime, /*source_id=*/std::nullopt);

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
                                         tokens, kValidTime,
                                         /*source_id=*/std::nullopt);

  EXPECT_TRUE(origin_trials_.IsFeaturePersistedForOrigin(
      trial_enabled_origin_, /*partition_origin=*/opaque_origin,
      blink::mojom::OriginTrialFeature::kOriginTrialsSampleAPIPersistentFeature,
      kValidTime));
}

TEST_F(OriginTrialsTest, TokensArePartitionedByTopLevelSite) {
  url::Origin origin_a = trial_enabled_origin_;
  url::Origin origin_b = url::Origin::Create(GURL(kTrialEnabledOriginB));
  url::Origin partition_site_a = origin_a;
  url::Origin partition_site_b = origin_b;
  std::vector<std::string> tokens_a = {kFrobulatePersistentToken};
  std::vector<std::string> tokens_b = {kFrobulatePersistentTokenAlternate};

  origin_trials_.PersistTrialsFromTokens(origin_a, partition_site_a, tokens_a,
                                         kValidTime,
                                         /*source_id=*/std::nullopt);
  origin_trials_.PersistTrialsFromTokens(origin_a, partition_site_b, tokens_a,
                                         kValidTime,
                                         /*source_id=*/std::nullopt);

  origin_trials_.PersistTrialsFromTokens(origin_b, partition_site_b, tokens_b,
                                         kValidTime,
                                         /*source_id=*/std::nullopt);

  // Only expect trials to be enabled for partitions where they have been set
  EXPECT_TRUE(origin_trials_.IsFeaturePersistedForOrigin(
      origin_a, partition_site_a,
      OriginTrialFeature::kOriginTrialsSampleAPIPersistentFeature, kValidTime));

  EXPECT_TRUE(origin_trials_.IsFeaturePersistedForOrigin(
      origin_a, partition_site_b,
      OriginTrialFeature::kOriginTrialsSampleAPIPersistentFeature, kValidTime));

  EXPECT_TRUE(origin_trials_.IsFeaturePersistedForOrigin(
      origin_b, partition_site_b,
      OriginTrialFeature::kOriginTrialsSampleAPIPersistentFeature, kValidTime));

  EXPECT_FALSE(origin_trials_.IsFeaturePersistedForOrigin(
      origin_b, partition_site_a,
      OriginTrialFeature::kOriginTrialsSampleAPIPersistentFeature, kValidTime));

  // Removing a token should only be from one partition
  origin_trials_.PersistTrialsFromTokens(
      origin_a, partition_site_b, {}, kValidTime, /*source_id=*/std::nullopt);

  EXPECT_TRUE(origin_trials_.IsFeaturePersistedForOrigin(
      origin_a, partition_site_a,
      OriginTrialFeature::kOriginTrialsSampleAPIPersistentFeature, kValidTime));

  EXPECT_FALSE(origin_trials_.IsFeaturePersistedForOrigin(
      origin_a, partition_site_b,
      OriginTrialFeature::kOriginTrialsSampleAPIPersistentFeature, kValidTime));
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

struct OriginValidationTestCase {
  const std::string test_origin_str;
  const std::string token_origin_str;
  const bool match_subdomains;
  const bool expected_result;
};

std::vector<OriginValidationTestCase> kOriginValidationTestCases = {
    {"https://example.com", "http://example.com",
     /*match_subdomains=*/false, /*enabled=*/false},
    {"https://example.com", "http://example.com",
     /*match_subdomains=*/true, /*enabled=*/false},
    {"http://foo.example.com", "http://example.com",
     /*match_subdomains=*/true, /*enabled=*/true},
    {"http://badexample.com", "http://example.com",
     /*match_subdomains=*/true, /*enabled=*/false},
    {"http://example.com", "http://foo.example.com",
     /*match_subdomains=*/true, /*enabled=*/false},
    {"https://bar.foo.example.com", "https://example.com",
     /*match_subdomains=*/true, /*enabled=*/true},
    {"", "https://example.com", /*match_subdomains=*/true, /*enabled=*/false},
};

// Test parsing of fields from JSON token.
class OriginTrialsTokenOriginValidationTest
    : public OriginTrialsTest,
      public testing::WithParamInterface<OriginValidationTestCase> {};

TEST_P(OriginTrialsTokenOriginValidationTest, MatchesTokenOrigin) {
  const OriginValidationTestCase test_case = GetParam();
  url::Origin subdomain_origin =
      url::Origin::Create(GURL(test_case.test_origin_str));
  url::Origin token_origin =
      url::Origin::Create(GURL(test_case.token_origin_str));

  EXPECT_EQ(MatchesTokenOrigin(token_origin, test_case.match_subdomains,
                               subdomain_origin),
            test_case.expected_result);
}

INSTANTIATE_TEST_SUITE_P(OriginTrialsTest,
                         OriginTrialsTokenOriginValidationTest,
                         testing::ValuesIn(kOriginValidationTestCases));

}  // namespace origin_trials
