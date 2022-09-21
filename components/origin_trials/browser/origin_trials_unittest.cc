// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
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
const char kInvalidTrialName[] = "InvalidTrial";

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

// Valid header token for Frobulate
// generated with
// tools/origin_trials/generate_token.py enabled.example.com Frobulate
// --expire-timestamp=2000000000
const char kFrobulateToken[] =
    "A2QF2oWLF7q+qYSwt+"
    "AvY6DSGE5QAb9Aeg7eOmbanVINtCJcCjZtNAtUUE88BB5UlvOMpPUUQjfgs3LTO0YXzAcAAABb"
    "eyJvcmlnaW4iOiAiaHR0cHM6Ly9lbmFibGVkLmV4YW1wbGUuY29tOjQ0MyIsICJmZWF0dXJlIj"
    "ogIkZyb2J1bGF0ZSIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ==";

const char kFrobulateManualCompletionToken[] =
    "A4TCodS8fnQFVyShubc4TKr+"
    "Ss6br97EBk4Kh1bQiskjJHwHXKjhxMjwviiL60RD4byiVF3D9UmoPdXcz7Kg8w8AAAB2eyJvcm"
    "lnaW4iOiAiaHR0cHM6Ly9lbmFibGVkLmV4YW1wbGUuY29tOjQ0MyIsICJmZWF0dXJlIjogIkZy"
    "b2J1bGF0ZVBlcnNpc3RlbnRFeHBpcnlHcmFjZVBlcmlvZCIsICJleHBpcnkiOiAyMDAwMDAwMD"
    "AwfQ==";

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

class OriginTrialsTest : public testing::Test {
 public:
  OriginTrialsTest()
      : origin_trials_(std::make_unique<test::TestPersistenceProvider>(),
                       std::make_unique<blink::TrialTokenValidator>()),
        trial_enabled_origin_(
            url::Origin::Create(GURL("https://enabled.example.com"))) {}

  OriginTrialsTest(const OriginTrialsTest&) = delete;
  OriginTrialsTest& operator=(const OriginTrialsTest&) = delete;

  ~OriginTrialsTest() override = default;

 protected:
  OriginTrials origin_trials_;
  url::Origin trial_enabled_origin_;
  OpenScopedTestOriginTrialPolicy origin_trial_policy_;
};

TEST_F(OriginTrialsTest, CleanObjectHasNoPersistentTrials) {
  EXPECT_TRUE(
      origin_trials_
          .GetPersistedTrialsForOrigin(trial_enabled_origin_, kValidTime)
          .empty());
}

TEST_F(OriginTrialsTest, EnabledTrialsArePersisted) {
  std::vector<std::string> tokens = {kFrobulatePersistentToken};
  origin_trials_.PersistTrialsFromTokens(trial_enabled_origin_, tokens,
                                         kValidTime);

  base::flat_set<std::string> enabled_trials =
      origin_trials_.GetPersistedTrialsForOrigin(trial_enabled_origin_,
                                                 kValidTime);
  ASSERT_EQ(1ul, enabled_trials.size());
  EXPECT_TRUE(enabled_trials.contains(kPersistentTrialName));
}

TEST_F(OriginTrialsTest, OnlyPersistentTrialsAreEnabled) {
  std::vector<std::string> tokens = {kFrobulateToken,
                                     kFrobulatePersistentToken};
  origin_trials_.PersistTrialsFromTokens(trial_enabled_origin_, tokens,
                                         kValidTime);

  base::flat_set<std::string> enabled_trials =
      origin_trials_.GetPersistedTrialsForOrigin(trial_enabled_origin_,
                                                 kValidTime);
  ASSERT_EQ(1ul, enabled_trials.size());
  EXPECT_TRUE(enabled_trials.contains(kPersistentTrialName));
  EXPECT_FALSE(enabled_trials.contains(kNonPersistentTrialName));
}

TEST_F(OriginTrialsTest, ResetClearsPersistedTrials) {
  std::vector<std::string> tokens = {kFrobulatePersistentToken};
  origin_trials_.PersistTrialsFromTokens(trial_enabled_origin_, tokens,
                                         kValidTime);

  EXPECT_FALSE(
      origin_trials_
          .GetPersistedTrialsForOrigin(trial_enabled_origin_, kValidTime)
          .empty());

  tokens = {};
  origin_trials_.PersistTrialsFromTokens(trial_enabled_origin_, tokens,
                                         kValidTime);

  EXPECT_TRUE(
      origin_trials_
          .GetPersistedTrialsForOrigin(trial_enabled_origin_, kValidTime)
          .empty());
}

TEST_F(OriginTrialsTest, TrialNotEnabledByDefault) {
  EXPECT_FALSE(origin_trials_.IsTrialPersistedForOrigin(
      trial_enabled_origin_, kPersistentTrialName, kValidTime));
}

TEST_F(OriginTrialsTest, TrialEnablesFeature) {
  std::vector<std::string> tokens = {kFrobulatePersistentToken};
  origin_trials_.PersistTrialsFromTokens(trial_enabled_origin_, tokens,
                                         kValidTime);

  EXPECT_TRUE(origin_trials_.IsTrialPersistedForOrigin(
      trial_enabled_origin_, kPersistentTrialName, kValidTime));
}

TEST_F(OriginTrialsTest, TrialDoesNotEnableOtherFeatures) {
  std::vector<std::string> tokens = {kFrobulatePersistentToken};
  origin_trials_.PersistTrialsFromTokens(trial_enabled_origin_, tokens,
                                         kValidTime);

  EXPECT_FALSE(origin_trials_.IsTrialPersistedForOrigin(
      trial_enabled_origin_, kNonPersistentTrialName, kValidTime));
}

// Check that a stored trial name is not returned if that trial is no longer
// valid or configured to be persistent
TEST_F(OriginTrialsTest, StoredEnabledTrialNotReturnedIfNoLongerPersistent) {
  std::unique_ptr<test::TestPersistenceProvider> persistence_provider =
      std::make_unique<test::TestPersistenceProvider>();

  base::Time token_expiry = base::Time::FromTimeT(2000000000);
  base::flat_set<PersistedTrialToken> stored_tokens = {
      {kNonPersistentTrialName, token_expiry,
       blink::TrialToken::UsageRestriction::kNone, kDummyTokenSignature},
      {kInvalidTrialName, token_expiry,
       blink::TrialToken::UsageRestriction::kNone, kDummyTokenSignature},
      {kPersistentTrialName, token_expiry,
       blink::TrialToken::UsageRestriction::kNone, kDummyTokenSignature}};
  persistence_provider->SavePersistentTrialTokens(trial_enabled_origin_,
                                                  stored_tokens);

  OriginTrials origin_trials(std::move(persistence_provider),
                             std::make_unique<blink::TrialTokenValidator>());
  base::flat_set<std::string> enabled_trials =
      origin_trials.GetPersistedTrialsForOrigin(trial_enabled_origin_,
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
  base::flat_set<PersistedTrialToken> stored_tokens = {
      {kPersistentTrialName, token_expiry,
       blink::TrialToken::UsageRestriction::kNone, kDummyTokenSignature}};
  persistence_provider->SavePersistentTrialTokens(trial_enabled_origin_,
                                                  stored_tokens);

  origin_trial_policy_.DisableTrial(kPersistentTrialName);

  OriginTrials origin_trials(std::move(persistence_provider),
                             std::make_unique<blink::TrialTokenValidator>());
  base::flat_set<std::string> enabled_trials =
      origin_trials.GetPersistedTrialsForOrigin(trial_enabled_origin_,
                                                kValidTime);
  EXPECT_TRUE(enabled_trials.empty());
}

// Check that a saved token is not returned if its signature has been disabled
// by policy
TEST_F(OriginTrialsTest, DisabledTokensNotReturned) {
  std::unique_ptr<test::TestPersistenceProvider> persistence_provider =
      std::make_unique<test::TestPersistenceProvider>();

  base::Time token_expiry = base::Time::FromTimeT(2000000000);
  base::flat_set<PersistedTrialToken> stored_tokens = {
      {kPersistentTrialName, token_expiry,
       blink::TrialToken::UsageRestriction::kNone, kDummyTokenSignature}};
  persistence_provider->SavePersistentTrialTokens(trial_enabled_origin_,
                                                  stored_tokens);

  origin_trial_policy_.DisableToken(kDummyTokenSignature);

  OriginTrials origin_trials(std::move(persistence_provider),
                             std::make_unique<blink::TrialTokenValidator>());
  base::flat_set<std::string> enabled_trials =
      origin_trials.GetPersistedTrialsForOrigin(trial_enabled_origin_,
                                                kValidTime);
  EXPECT_TRUE(enabled_trials.empty());
}

TEST_F(OriginTrialsTest, UserDisabledTokensNotReturned) {
  std::unique_ptr<test::TestPersistenceProvider> persistence_provider =
      std::make_unique<test::TestPersistenceProvider>();

  base::Time token_expiry = base::Time::FromTimeT(2000000000);
  base::flat_set<PersistedTrialToken> stored_tokens = {
      {kPersistentTrialName, token_expiry,
       blink::TrialToken::UsageRestriction::kSubset, kDummyTokenSignature}};
  persistence_provider->SavePersistentTrialTokens(trial_enabled_origin_,
                                                  stored_tokens);

  origin_trial_policy_.DisableTrialForUser(kPersistentTrialName);

  OriginTrials origin_trials(std::move(persistence_provider),
                             std::make_unique<blink::TrialTokenValidator>());
  base::flat_set<std::string> enabled_trials =
      origin_trials.GetPersistedTrialsForOrigin(trial_enabled_origin_,
                                                kValidTime);
  EXPECT_TRUE(enabled_trials.empty());
}

TEST_F(OriginTrialsTest, GracePeriodIsRespected) {
  std::vector<std::string> tokens = {kFrobulateManualCompletionToken};
  origin_trials_.PersistTrialsFromTokens(trial_enabled_origin_, tokens,
                                         kValidTime);

  base::flat_set<std::string> enabled_trials =
      origin_trials_.GetPersistedTrialsForOrigin(trial_enabled_origin_,
                                                 kExpiryTime);
  EXPECT_EQ(1ul, enabled_trials.size())
      << "Expect trial to be valid at the expiry time limit";

  base::Time in_grace_period = kExpiryTime + base::Days(1);
  enabled_trials = origin_trials_.GetPersistedTrialsForOrigin(
      trial_enabled_origin_, in_grace_period);
  EXPECT_EQ(1ul, enabled_trials.size())
      << "Expect trial to be valid within the grace period";

  base::Time end_of_grace_period = kExpiryTime + blink::kExpiryGracePeriod;
  enabled_trials = origin_trials_.GetPersistedTrialsForOrigin(
      trial_enabled_origin_, end_of_grace_period);
  EXPECT_EQ(0ul, enabled_trials.size())
      << "Do not expect the trial to be valid after the grace period ends";
}

TEST_F(OriginTrialsTest, GracefullyHandleOpaqueOrigins) {
  std::vector<std::string> tokens = {kFrobulateManualCompletionToken};
  url::Origin opaque_origin;
  origin_trials_.PersistTrialsFromTokens(opaque_origin, tokens, kValidTime);
  // No assert, this just shouldn't crash

  base::flat_set<std::string> trials =
      origin_trials_.GetPersistedTrialsForOrigin(opaque_origin, kValidTime);
  EXPECT_TRUE(trials.empty());

  EXPECT_FALSE(origin_trials_.IsTrialPersistedForOrigin(
      opaque_origin, kPersistentTrialName, kValidTime));
}

}  // namespace
}  // namespace origin_trials