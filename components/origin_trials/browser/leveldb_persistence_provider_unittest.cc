// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_trials/browser/leveldb_persistence_provider.h"

#include <map>
#include <memory>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/origin_trials/common/persisted_trial_token.h"
#include "components/origin_trials/proto/db_trial_token.pb.h"
#include "components/origin_trials/proto/proto_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/origin_trials/scoped_test_origin_trial_policy.h"
#include "third_party/blink/public/common/origin_trials/trial_token.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace origin_trials {

namespace {

const char kTrialOrigin[] = "https://example.com";
const char kTrialOriginSecondary[] = "https://secondary.example.com";
const char kTrialName[] = "FrobulatePersistent";
const char kTrialSignature[] = "trial signature";
const char kTrialSignatureAlternate[] = "alternate trial signature";

using leveldb_proto::test::FakeDB;

class LevelDbPersistenceProviderUnitTest : public testing::Test {
 public:
  LevelDbPersistenceProviderUnitTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  LevelDbPersistenceProviderUnitTest(
      const LevelDbPersistenceProviderUnitTest&) = delete;
  LevelDbPersistenceProviderUnitTest& operator=(
      const LevelDbPersistenceProviderUnitTest&) = delete;

  ~LevelDbPersistenceProviderUnitTest() override = default;

  void CreatePersistenceProvider() {
    std::unique_ptr<FakeDB<origin_trials_pb::TrialTokenDbEntries>> db =
        std::make_unique<FakeDB<origin_trials_pb::TrialTokenDbEntries>>(
            &db_entries_);
    fake_db_unretained_ = db.get();
    persistence_provider_ =
        LevelDbPersistenceProvider::CreateForTesting(std::move(db));
    ;
  }

  void InitLevelDb(bool ok) {
    fake_db_unretained_->InitStatusCallback(ok ? leveldb_proto::Enums::kOK
                                               : leveldb_proto::Enums::kError);
  }

  void FlushLoadCallback(bool success) {
    fake_db_unretained_->LoadCallback(success);
    task_environment_.RunUntilIdle();  // Run the DB transformation callback
  }

  void FlushUpdateCallback() { fake_db_unretained_->UpdateCallback(true); }

  void InitPersistenceProvider() {
    CreatePersistenceProvider();
    InitLevelDb(true);
    FlushLoadCallback(true);
  }

 protected:
  blink::ScopedTestOriginTrialPolicy origin_trial_policy_;
  content::BrowserTaskEnvironment task_environment_;
  std::map<std::string, origin_trials_pb::TrialTokenDbEntries> db_entries_;

  base::raw_ptr<FakeDB<origin_trials_pb::TrialTokenDbEntries>>
      fake_db_unretained_;
  std::unique_ptr<LevelDbPersistenceProvider> persistence_provider_;
};

TEST_F(LevelDbPersistenceProviderUnitTest, NormalStartupLogsHistograms) {
  base::HistogramTester ht;
  InitPersistenceProvider();

  ht.ExpectUniqueSample("OriginTrials.PersistentOriginTrial.LevelDbInitSuccess",
                        true, 1);
  ht.ExpectUniqueSample("OriginTrials.PersistentOriginTrial.LevelDbLoadSuccess",
                        true, 1);
  ht.ExpectUniqueSample(
      "OriginTrials.PersistentOriginTrial.OriginsAddedBeforeDbLoad", 0, 1);
  ht.ExpectUniqueSample(

      "OriginTrials.PersistentOriginTrial.OriginLookupsBeforeDbLoad", 0, 1);
  ht.ExpectUniqueSample("OriginTrials.PersistentOriginTrial.LevelDbLoadSize", 0,
                        1);
  ht.ExpectTotalCount("OriginTrials.PersistentOriginTrial.LevelDbLoadTime", 1);
}

TEST_F(LevelDbPersistenceProviderUnitTest, FailedInitLogsHistograms) {
  base::HistogramTester ht;
  CreatePersistenceProvider();
  InitLevelDb(false);

  // We only expect the init success histogram to be logged in case of init
  // failure
  ht.ExpectUniqueSample("OriginTrials.PersistentOriginTrial.LevelDbInitSuccess",
                        false, 1);
  ht.ExpectTotalCount("OriginTrials.PersistentOriginTrial.LevelDbLoadSuccess",
                      0);
  ht.ExpectTotalCount(
      "OriginTrials.PersistentOriginTrial.OriginsAddedBeforeDbLoad", 0);
  ht.ExpectTotalCount(
      "OriginTrials.PersistentOriginTrial.OriginLookupsBeforeDbLoad", 0);
  ht.ExpectTotalCount("OriginTrials.PersistentOriginTrial.LevelDbLoadSize", 0);
  ht.ExpectTotalCount("OriginTrials.PersistentOriginTrial.LevelDbLoadTime", 0);
}

TEST_F(LevelDbPersistenceProviderUnitTest, FailedLoadLogsHistograms) {
  base::HistogramTester ht;
  CreatePersistenceProvider();
  InitLevelDb(true);
  FlushLoadCallback(false);

  // In case of load failure, only the init and load histograms should be
  // logged.
  ht.ExpectUniqueSample("OriginTrials.PersistentOriginTrial.LevelDbInitSuccess",
                        true, 1);
  ht.ExpectUniqueSample("OriginTrials.PersistentOriginTrial.LevelDbLoadSuccess",
                        false, 1);
  ht.ExpectTotalCount(
      "OriginTrials.PersistentOriginTrial.OriginsAddedBeforeDbLoad", 0);
  ht.ExpectTotalCount(
      "OriginTrials.PersistentOriginTrial.OriginLookupsBeforeDbLoad", 0);
  ht.ExpectTotalCount("OriginTrials.PersistentOriginTrial.LevelDbLoadSize", 0);
  ht.ExpectTotalCount("OriginTrials.PersistentOriginTrial.LevelDbLoadTime", 0);
}

TEST_F(LevelDbPersistenceProviderUnitTest, UpdatesAppliedInMemoryAndToDb) {
  InitPersistenceProvider();

  url::Origin origin = url::Origin::Create(GURL(kTrialOrigin));

  base::Time expiry = base::Time::Now() + base::Days(365);

  base::flat_set<PersistedTrialToken> tokens;
  tokens.emplace(kTrialName, expiry, blink::TrialToken::UsageRestriction::kNone,
                 kTrialSignature);

  persistence_provider_->SavePersistentTrialTokens(origin, tokens);

  base::flat_set<PersistedTrialToken> stored_tokens =
      persistence_provider_->GetPersistentTrialTokens(origin);

  EXPECT_EQ(tokens, stored_tokens);

  FlushUpdateCallback();

  // Expect the DB to have been updated in the back after an update
  EXPECT_EQ(1ul, db_entries_.size());
  EXPECT_NE(db_entries_.end(), db_entries_.find(kTrialOrigin))
      << "Expect to find a value for kTrialOrigin in the map";

  persistence_provider_->ClearPersistedTokens();
  stored_tokens = persistence_provider_->GetPersistentTrialTokens(origin);
  EXPECT_TRUE(stored_tokens.empty());

  FlushUpdateCallback();
  EXPECT_TRUE(db_entries_.empty());
}

TEST_F(LevelDbPersistenceProviderUnitTest, TokensLoadedFromDbOnStartup) {
  base::HistogramTester ht;
  url::Origin origin = url::Origin::Create(GURL(kTrialOrigin));

  base::Time expiry = base::Time::Now() + base::Days(365);

  base::flat_set<PersistedTrialToken> tokens;
  tokens.emplace(kTrialName, expiry, blink::TrialToken::UsageRestriction::kNone,
                 kTrialSignature);

  db_entries_[origin.Serialize()] =
      origin_trials_pb::ProtoFromTokens(origin, tokens);

  InitPersistenceProvider();

  // One item should have been loaded
  ht.ExpectUniqueSample("OriginTrials.PersistentOriginTrial.LevelDbLoadSize", 1,
                        1);
  // The DB should not have been used before load
  ht.ExpectUniqueSample(
      "OriginTrials.PersistentOriginTrial.OriginsAddedBeforeDbLoad", 0, 1);
  ht.ExpectUniqueSample(
      "OriginTrials.PersistentOriginTrial.OriginLookupsBeforeDbLoad", 0, 1);

  base::flat_set<PersistedTrialToken> stored_tokens =
      persistence_provider_->GetPersistentTrialTokens(origin);
  EXPECT_EQ(tokens, stored_tokens);
}

TEST_F(LevelDbPersistenceProviderUnitTest,
       TokensLoadedFromDbOnStartupAreCleanedUpIfExpired) {
  base::HistogramTester ht;
  url::Origin origin = url::Origin::Create(GURL(kTrialOrigin));

  base::Time expiry = base::Time::Now() - base::Days(1);

  base::flat_set<PersistedTrialToken> tokens;
  tokens.emplace(kTrialName, expiry, blink::TrialToken::UsageRestriction::kNone,
                 kTrialSignature);

  db_entries_[origin.Serialize()] =
      origin_trials_pb::ProtoFromTokens(origin, tokens);

  InitPersistenceProvider();
  // One item should have been loaded from the database
  ht.ExpectUniqueSample("OriginTrials.PersistentOriginTrial.LevelDbLoadSize", 1,
                        1);
  // The DB should not have been used before load
  ht.ExpectUniqueSample(
      "OriginTrials.PersistentOriginTrial.OriginsAddedBeforeDbLoad", 0, 1);
  ht.ExpectUniqueSample(
      "OriginTrials.PersistentOriginTrial.OriginLookupsBeforeDbLoad", 0, 1);

  base::flat_set<PersistedTrialToken> stored_tokens =
      persistence_provider_->GetPersistentTrialTokens(origin);
  EXPECT_TRUE(stored_tokens.empty());

  FlushUpdateCallback();
  EXPECT_TRUE(db_entries_.empty());
}

TEST_F(LevelDbPersistenceProviderUnitTest, QueriesBeforeDbLoad) {
  base::HistogramTester ht;
  url::Origin origin_a = url::Origin::Create(GURL(kTrialOrigin));
  url::Origin origin_b = url::Origin::Create(GURL(kTrialOriginSecondary));

  base::Time expiry = base::Time::Now() + base::Days(365);

  base::flat_set<PersistedTrialToken> tokens_in_db;
  tokens_in_db.emplace(kTrialName, expiry,
                       blink::TrialToken::UsageRestriction::kNone,
                       kTrialSignature);
  db_entries_[origin_a.Serialize()] =
      origin_trials_pb::ProtoFromTokens(origin_a, tokens_in_db);

  base::flat_set<PersistedTrialToken> tokens_before_load;
  tokens_before_load.emplace(kTrialName, expiry,
                             blink::TrialToken::UsageRestriction::kNone,
                             kTrialSignature);

  base::flat_set<PersistedTrialToken> all_tokens;
  all_tokens.insert(tokens_in_db.begin(), tokens_in_db.end());
  all_tokens.insert(tokens_before_load.begin(), tokens_before_load.end());

  CreatePersistenceProvider();

  // Use the persistence provider before DB is ready, and check that it behaves
  // as expected
  base::flat_set<PersistedTrialToken> stored_tokens =
      persistence_provider_->GetPersistentTrialTokens(origin_b);
  EXPECT_TRUE(stored_tokens.empty())
      << "No tokens should be available before the DB has loaded";

  persistence_provider_->SavePersistentTrialTokens(origin_b,
                                                   tokens_before_load);

  stored_tokens = persistence_provider_->GetPersistentTrialTokens(origin_b);
  EXPECT_EQ(tokens_before_load, stored_tokens)
      << "The in-memory map should ensure synchronous operation, even before "
         "DB load";

  // Finish loading the DB
  InitLevelDb(true);
  FlushLoadCallback(true);

  // One item should have been loaded
  ht.ExpectUniqueSample("OriginTrials.PersistentOriginTrial.LevelDbLoadSize", 1,
                        1);
  ht.ExpectUniqueSample(
      "OriginTrials.PersistentOriginTrial.OriginsAddedBeforeDbLoad", 1, 1);
  ht.ExpectUniqueSample(
      "OriginTrials.PersistentOriginTrial.OriginLookupsBeforeDbLoad", 2, 1);

  EXPECT_EQ(tokens_in_db,
            persistence_provider_->GetPersistentTrialTokens(origin_a));
  EXPECT_EQ(tokens_before_load,
            persistence_provider_->GetPersistentTrialTokens(origin_b));
}

TEST_F(LevelDbPersistenceProviderUnitTest,
       LoadFromDbDoesNotOverwriteInMemoryData) {
  base::HistogramTester ht;
  url::Origin origin = url::Origin::Create(GURL(kTrialOrigin));

  base::Time expiry = base::Time::Now() + base::Days(365);

  base::flat_set<PersistedTrialToken> db_tokens;
  db_tokens.emplace(kTrialName, expiry,
                    blink::TrialToken::UsageRestriction::kNone,
                    kTrialSignature);
  base::flat_set<PersistedTrialToken> live_tokens;
  live_tokens.emplace(kTrialName, expiry,
                      blink::TrialToken::UsageRestriction::kNone,
                      kTrialSignatureAlternate);

  db_entries_[origin.Serialize()] =
      origin_trials_pb::ProtoFromTokens(origin, db_tokens);

  CreatePersistenceProvider();

  // The website used a new token, which should be saved
  persistence_provider_->SavePersistentTrialTokens(origin, live_tokens);

  // Finish loading the DB
  InitLevelDb(true);
  FlushLoadCallback(true);
  // Process any queued update operations
  FlushUpdateCallback();

  // One item should have been loaded
  ht.ExpectUniqueSample("OriginTrials.PersistentOriginTrial.LevelDbLoadSize", 1,
                        1);
  ht.ExpectUniqueSample(
      "OriginTrials.PersistentOriginTrial.OriginsAddedBeforeDbLoad", 1, 1);
  ht.ExpectUniqueSample(
      "OriginTrials.PersistentOriginTrial.OriginLookupsBeforeDbLoad", 0, 1);

  // We expect that a read will see the value set most recently, i.e. that set
  // before DB load.
  base::flat_set<PersistedTrialToken> stored_tokens =
      persistence_provider_->GetPersistentTrialTokens(origin);
  EXPECT_EQ(live_tokens, stored_tokens);

  // Check that the DB is updated with the new value as well after update
  EXPECT_FALSE(db_entries_.empty());
  auto lookup = db_entries_.find(origin.Serialize());
  ASSERT_NE(db_entries_.end(), lookup);
  auto db_token_array = lookup->second.tokens();
  ASSERT_EQ(1, db_token_array.size());
  origin_trials_pb::TrialTokenDbEntry& entry = *(db_token_array.begin());
  ASSERT_EQ(kTrialSignatureAlternate, entry.token_signature());
}

}  // namespace

}  // namespace origin_trials
