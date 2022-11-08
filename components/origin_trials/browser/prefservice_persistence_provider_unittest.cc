// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_trials/browser/prefservice_persistence_provider.h"

#include <memory>

#include "base/auto_reset.h"
#include "base/containers/flat_set.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/origin_trials/scoped_test_origin_trial_policy.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace origin_trials {
namespace {

const char kPersistentTrialName[] = "FrobulatePersistent";

const char kDummySignature[] = "dummy signature";

class PrefServicePersistenceProviderUnitTest
    : public content::RenderViewHostTestHarness {
 public:
  PrefServicePersistenceProviderUnitTest()
      : content::RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        trial_enabled_origin_(
            url::Origin::Create(GURL("https://enabled.example.com"))) {}

  PrefServicePersistenceProviderUnitTest(
      const PrefServicePersistenceProviderUnitTest&) = delete;
  PrefServicePersistenceProviderUnitTest& operator=(
      const PrefServicePersistenceProviderUnitTest&) = delete;

  ~PrefServicePersistenceProviderUnitTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    user_prefs::UserPrefs::Set(browser_context(), &prefs_);
    disable_cleanup_expired_tokens_ =
        PrefServicePersistenceProvider::DisableCleanupExpiredTokensForTesting();
    PrefServicePersistenceProvider::RegisterProfilePrefs(prefs_.registry());
    persistence_provider_ =
        std::make_unique<PrefServicePersistenceProvider>(browser_context());
  }

  void TearDown() override {
    persistence_provider_ = nullptr;
    RenderViewHostTestHarness::TearDown();
  }

  const base::Value::List* GetPrefListForOrigin(const url::Origin& origin) {
    const base::Value::Dict& stored_prefs = prefs_.GetDict(kOriginTrialPrefKey);
    return stored_prefs.FindList(origin.Serialize());
  }

 protected:
  std::unique_ptr<base::AutoReset<bool>> disable_cleanup_expired_tokens_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<PrefServicePersistenceProvider> persistence_provider_;
  url::Origin trial_enabled_origin_;
  blink::ScopedTestOriginTrialPolicy origin_trial_policy_;
};

TEST_F(PrefServicePersistenceProviderUnitTest, NewProviderHasNoStoredTrials) {
  EXPECT_TRUE(
      persistence_provider_->GetPersistentTrialTokens(trial_enabled_origin_)
          .empty());
}

TEST_F(PrefServicePersistenceProviderUnitTest, StoredTrialsCanBeRetrieved) {
  base::Time expiry = base::Time::Now() + base::Days(1);
  base::flat_set<PersistedTrialToken> tokens = {
      {kPersistentTrialName, expiry, blink::TrialToken::UsageRestriction::kNone,
       kDummySignature}};
  persistence_provider_->SavePersistentTrialTokens(trial_enabled_origin_,
                                                   tokens);

  base::flat_set<PersistedTrialToken> persisted_trials =
      persistence_provider_->GetPersistentTrialTokens(trial_enabled_origin_);
  EXPECT_EQ(tokens, persisted_trials);
}

TEST_F(PrefServicePersistenceProviderUnitTest, CleanupRemovesExpiredTokens) {
  // Create points in time 1 day apart
  base::Time expiry_1 = base::Time::Now() + base::Days(1);
  base::Time expiry_2 = base::Time::Now() + base::Days(2);
  base::Time expiry_3 = base::Time::Now() + base::Days(3);
  base::Time expiry_4 = base::Time::Now() + base::Days(4);

  // Persist tokens that expire 1 and 3 days from now
  PersistedTrialToken token_1(kPersistentTrialName, expiry_1,
                              blink::TrialToken::UsageRestriction::kNone,
                              kDummySignature);
  PersistedTrialToken token_3(kPersistentTrialName, expiry_3,
                              blink::TrialToken::UsageRestriction::kNone,
                              kDummySignature);
  base::flat_set<PersistedTrialToken> tokens = {token_1, token_3};
  persistence_provider_->SavePersistentTrialTokens(trial_enabled_origin_,
                                                   tokens);

  // Confirm that there are two stored prefs
  const base::Value::List* origin_prefs =
      GetPrefListForOrigin(trial_enabled_origin_);
  ASSERT_TRUE(origin_prefs);
  EXPECT_EQ(2UL, origin_prefs->size());

  // Clean up tokens that expire before day 2
  persistence_provider_->DeleteExpiredTokens(expiry_2);
  // There should now only be one pref stored
  origin_prefs = GetPrefListForOrigin(trial_enabled_origin_);
  ASSERT_TRUE(origin_prefs);
  EXPECT_EQ(1UL, origin_prefs->size());
  absl::optional<PersistedTrialToken> remainingToken =
      PersistedTrialToken::FromDict(origin_prefs->begin()->GetDict());
  ASSERT_TRUE(remainingToken);
  EXPECT_EQ(expiry_3, (*remainingToken).token_expiry);

  // Clean up tokens that expire before day 4
  persistence_provider_->DeleteExpiredTokens(expiry_4);
  origin_prefs = GetPrefListForOrigin(trial_enabled_origin_);
  // The origin key has been removed when there are no more tokens
  EXPECT_FALSE(origin_prefs);
}
TEST_F(PrefServicePersistenceProviderUnitTest, ClearTokens) {
  base::Time expiry = base::Time::Now() + base::Days(1);
  base::flat_set<PersistedTrialToken> tokens = {
      {kPersistentTrialName, expiry, blink::TrialToken::UsageRestriction::kNone,
       kDummySignature}};
  persistence_provider_->SavePersistentTrialTokens(trial_enabled_origin_,
                                                   tokens);

  EXPECT_TRUE(prefs_.HasPrefPath(kOriginTrialPrefKey));
  persistence_provider_->ClearPersistedTokens();

  EXPECT_FALSE(prefs_.HasPrefPath(kOriginTrialPrefKey));
}

}  // namespace

}  // namespace origin_trials
