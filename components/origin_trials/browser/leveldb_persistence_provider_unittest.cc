// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_trials/browser/leveldb_persistence_provider.h"

#include <map>
#include <memory>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/origin_trials/scoped_test_origin_trial_policy.h"
#include "third_party/blink/public/common/origin_trials/trial_token.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace origin_trials {

namespace {

const char kExampleComOrigin[] = "https://example.com";
const char kSecondaryExampleComOrigin[] = "https://secondary.example.com";
const char kTrialName[] = "FrobulatePersistent";
const char kTrialSignature[] = "trial signature";
const char kTrialSignatureAlternate[] = "alternate trial signature";
const char kExampleComDomain[] = "https://example.com";
const char kGoogleComDomain[] = "https://google.com";

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

  SiteKey GetSiteKey(const url::Origin& origin) const {
    return OriginTrialsPersistenceProvider::GetSiteKey(origin);
  }

  url::Origin GetSecureOrigin(const std::string& domain_name) {
    return url::Origin::CreateFromNormalizedTuple("https", domain_name, 443);
  }

 protected:
  blink::ScopedTestOriginTrialPolicy origin_trial_policy_;
  content::BrowserTaskEnvironment task_environment_;
  std::map<std::string, origin_trials_pb::TrialTokenDbEntries> db_entries_;

  raw_ptr<FakeDB<origin_trials_pb::TrialTokenDbEntries>, DanglingUntriaged>
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
  ht.ExpectTotalCount("OriginTrials.PersistentOriginTrial.PartitionSetSize", 0);
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
  ht.ExpectTotalCount("OriginTrials.PersistentOriginTrial.PartitionSetSize", 0);
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
  ht.ExpectTotalCount("OriginTrials.PersistentOriginTrial.PartitionSetSize", 0);
}

TEST_F(LevelDbPersistenceProviderUnitTest, UpdatesAppliedInMemoryAndToDb) {
  InitPersistenceProvider();

  url::Origin origin = url::Origin::Create(GURL(kExampleComOrigin));
  base::flat_set<std::string> partition_sites = {kExampleComDomain};

  base::Time expiry = base::Time::Now() + base::Days(365);

  base::flat_set<PersistedTrialToken> tokens;
  tokens.emplace(/*match_subdomains=*/false, kTrialName, expiry,
                 blink::TrialToken::UsageRestriction::kNone, kTrialSignature,
                 partition_sites);

  persistence_provider_->SavePersistentTrialTokens(origin, tokens);

  base::flat_set<PersistedTrialToken> stored_tokens =
      persistence_provider_->GetPersistentTrialTokens(origin);

  EXPECT_EQ(tokens, stored_tokens);

  FlushUpdateCallback();

  // Expect the DB to have been updated in the back after an update
  EXPECT_EQ(1ul, db_entries_.size());
  EXPECT_NE(db_entries_.end(), db_entries_.find(kExampleComOrigin))
      << "Expect to find a value for kExampleComOrigin in the map";

  persistence_provider_->ClearPersistedTokens();
  stored_tokens = persistence_provider_->GetPersistentTrialTokens(origin);
  EXPECT_TRUE(stored_tokens.empty());

  FlushUpdateCallback();
  EXPECT_TRUE(db_entries_.empty());
}

TEST_F(LevelDbPersistenceProviderUnitTest, TokensLoadedFromDbOnStartup) {
  base::HistogramTester ht;
  base::Time expiry = base::Time::Now() + base::Days(365);

  url::Origin origin_a = url::Origin::Create(GURL(kExampleComOrigin));
  base::flat_set<std::string> partition_sites_a = {kExampleComDomain};
  base::flat_set<PersistedTrialToken> tokens_a;
  tokens_a.emplace(/*match_subdomains=*/false, kTrialName, expiry,
                   blink::TrialToken::UsageRestriction::kNone, kTrialSignature,
                   partition_sites_a);
  db_entries_[origin_a.Serialize()] =
      origin_trials_pb::ProtoFromTokens(origin_a, tokens_a);

  url::Origin origin_b = url::Origin::Create(GURL(kSecondaryExampleComOrigin));
  base::flat_set<std::string> partition_sites_b = {kExampleComDomain,
                                                   kGoogleComDomain};
  base::flat_set<PersistedTrialToken> tokens_b;
  tokens_b.emplace(/*match_subdomains=*/false, kTrialName, expiry,
                   blink::TrialToken::UsageRestriction::kNone, kTrialSignature,
                   partition_sites_b);
  db_entries_[origin_b.Serialize()] =
      origin_trials_pb::ProtoFromTokens(origin_b, tokens_b);

  InitPersistenceProvider();

  // Two items should have been loaded
  ht.ExpectUniqueSample("OriginTrials.PersistentOriginTrial.LevelDbLoadSize", 2,
                        1);
  // The PartitionSetSize should be reported for each token individually.
  ht.ExpectBucketCount("OriginTrials.PersistentOriginTrial.PartitionSetSize", 1,
                       1);
  ht.ExpectBucketCount("OriginTrials.PersistentOriginTrial.PartitionSetSize", 2,
                       1);
  // Both tokens are in a first-party partition.
  ht.ExpectBucketCount(
      "OriginTrials.PersistentOriginTrial.TokenHasFirstPartyPartition", true,
      2);
  // The DB should not have been used before load.
  ht.ExpectUniqueSample(
      "OriginTrials.PersistentOriginTrial.OriginsAddedBeforeDbLoad", 0, 1);
  ht.ExpectUniqueSample(
      "OriginTrials.PersistentOriginTrial.OriginLookupsBeforeDbLoad", 0, 1);

  EXPECT_EQ(tokens_a,
            persistence_provider_->GetPersistentTrialTokens(origin_a));
  EXPECT_EQ(tokens_b,
            persistence_provider_->GetPersistentTrialTokens(origin_b));
}

TEST_F(LevelDbPersistenceProviderUnitTest,
       TokensLoadedFromDbOnStartupAreCleanedUpIfExpired) {
  base::HistogramTester ht;
  url::Origin origin = url::Origin::Create(GURL(kExampleComOrigin));
  base::flat_set<std::string> partition_sites = {kExampleComDomain};

  base::Time expiry = base::Time::Now() - base::Days(1);

  base::flat_set<PersistedTrialToken> tokens;
  tokens.emplace(/*match_subdomains=*/false, kTrialName, expiry,
                 blink::TrialToken::UsageRestriction::kNone, kTrialSignature,
                 partition_sites);

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
  url::Origin origin_a = url::Origin::Create(GURL(kExampleComOrigin));
  url::Origin origin_b = url::Origin::Create(GURL(kSecondaryExampleComOrigin));
  base::flat_set<std::string> partition_sites = {kExampleComDomain};

  base::Time expiry = base::Time::Now() + base::Days(365);

  base::flat_set<PersistedTrialToken> tokens_in_db;
  tokens_in_db.emplace(/*match_subdomains=*/false, kTrialName, expiry,
                       blink::TrialToken::UsageRestriction::kNone,
                       kTrialSignature, partition_sites);
  db_entries_[origin_a.Serialize()] =
      origin_trials_pb::ProtoFromTokens(origin_a, tokens_in_db);

  base::flat_set<PersistedTrialToken> tokens_before_load;
  tokens_before_load.emplace(/*match_subdomains=*/false, kTrialName, expiry,
                             blink::TrialToken::UsageRestriction::kNone,
                             kTrialSignature, partition_sites);

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
  url::Origin db_origin = url::Origin::Create(GURL(kExampleComOrigin));
  base::flat_set<std::string> partition_sites = {kExampleComDomain};

  base::Time expiry = base::Time::Now() + base::Days(365);

  base::flat_set<PersistedTrialToken> db_tokens;
  db_tokens.emplace(/*match_subdomains=*/false, kTrialName, expiry,
                    blink::TrialToken::UsageRestriction::kNone, kTrialSignature,
                    partition_sites);

  db_entries_[db_origin.Serialize()] =
      origin_trials_pb::ProtoFromTokens(db_origin, db_tokens);

  CreatePersistenceProvider();

  // The website used a new token, which should be saved
  base::flat_set<PersistedTrialToken> live_tokens;
  url::Origin live_origin =
      url::Origin::Create(GURL(kSecondaryExampleComOrigin));
  live_tokens.emplace(/*match_subdomains=*/false, kTrialName, expiry,
                      blink::TrialToken::UsageRestriction::kNone,
                      kTrialSignatureAlternate, partition_sites);
  persistence_provider_->SavePersistentTrialTokens(live_origin, live_tokens);

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

  // After the load, we still expect to see the new token
  base::flat_set<PersistedTrialToken> stored_tokens =
      persistence_provider_->GetPersistentTrialTokens(live_origin);
  EXPECT_EQ(live_tokens, stored_tokens);

  // Check that the DB is updated with the new value as well after update
  EXPECT_FALSE(db_entries_.empty());
  auto lookup = db_entries_.find(live_origin.Serialize());
  ASSERT_NE(db_entries_.end(), lookup);
  auto db_token_array = lookup->second.tokens();
  ASSERT_EQ(1, db_token_array.size());
  origin_trials_pb::TrialTokenDbEntry& entry = *(db_token_array.begin());
  EXPECT_EQ(kTrialSignatureAlternate, entry.token_signature());
}

TEST_F(LevelDbPersistenceProviderUnitTest,
       WriteBeforeLoadShouldMergePartitions) {
  url::Origin origin = url::Origin::Create(GURL(kExampleComOrigin));
  base::flat_set<std::string> db_partitions = {kExampleComDomain};

  base::Time expiry = base::Time::Now() + base::Days(365);

  // The database has a token stored in a single partition |kExampleComDomain|.
  base::flat_set<PersistedTrialToken> db_tokens;
  db_tokens.emplace(/*match_subdomains=*/false, kTrialName, expiry,
                    blink::TrialToken::UsageRestriction::kNone, kTrialSignature,
                    db_partitions);

  db_entries_[origin.Serialize()] =
      origin_trials_pb::ProtoFromTokens(origin, db_tokens);

  CreatePersistenceProvider();

  // Persist the same token in a new partition |kGoogleComDomain| before the
  // database has loaded.
  base::flat_set<std::string> live_partitions = {kGoogleComDomain};
  base::flat_set<PersistedTrialToken> live_tokens;
  live_tokens.emplace(false, kTrialName, expiry,
                      blink::TrialToken::UsageRestriction::kNone,
                      kTrialSignature, live_partitions);
  persistence_provider_->SavePersistentTrialTokens(origin, live_tokens);

  // Finish loading the DB
  InitLevelDb(true);
  FlushLoadCallback(true);
  // Process any queued update operations
  FlushUpdateCallback();

  // We expect to see both the stored and the newly persisted partitions when
  // we read from the persistence provider.
  base::flat_set<PersistedTrialToken> stored_tokens =
      persistence_provider_->GetPersistentTrialTokens(origin);
  EXPECT_EQ(1UL, stored_tokens.size());
  base::flat_set<std::string> expected_partitions = {kExampleComDomain,
                                                     kGoogleComDomain};
  EXPECT_EQ(expected_partitions, stored_tokens.begin()->partition_sites);

  // Check that the DB is updated with the new value as well.
  EXPECT_FALSE(db_entries_.empty());
  auto lookup = db_entries_.find(origin.Serialize());
  ASSERT_NE(db_entries_.end(), lookup);
  auto db_token_array = lookup->second.tokens();
  ASSERT_EQ(1, db_token_array.size());
  origin_trials_pb::TrialTokenDbEntry& entry = *(db_token_array.begin());
  base::flat_set<std::string> saved_partitions(entry.partition_sites().begin(),
                                               entry.partition_sites().end());
  EXPECT_EQ(expected_partitions, saved_partitions);
}

TEST_F(
    LevelDbPersistenceProviderUnitTest,
    GetPotentialPersistentTrialTokensReturnsTokensForOriginsWithSameSiteKey) {
  url::Origin origin = GetSecureOrigin("foo.com");
  url::Origin origin_sub = GetSecureOrigin("sub.foo.com");
  url::Origin other_origin = GetSecureOrigin("bar.com");
  url::Origin other_origin_sub = GetSecureOrigin("sub.bar.com");

  base::flat_set<std::string> partitions = {kExampleComDomain};
  base::Time expiry = base::Time::Now() + base::Days(365);

  base::flat_set<PersistedTrialToken> tokens;
  tokens.emplace(/*match_subdomains=*/false, kTrialName, expiry,
                 blink::TrialToken::UsageRestriction::kNone, kTrialSignature,
                 partitions);

  db_entries_[origin.Serialize()] =
      origin_trials_pb::ProtoFromTokens(origin, tokens);
  db_entries_[origin_sub.Serialize()] =
      origin_trials_pb::ProtoFromTokens(origin_sub, tokens);

  db_entries_[other_origin.Serialize()] =
      origin_trials_pb::ProtoFromTokens(other_origin, tokens);
  db_entries_[other_origin_sub.Serialize()] =
      origin_trials_pb::ProtoFromTokens(other_origin_sub, tokens);

  InitPersistenceProvider();

  SiteOriginTrialTokens origin_site_potential_tokens =
      persistence_provider_->GetPotentialPersistentTrialTokens(origin);
  EXPECT_EQ(origin_site_potential_tokens.size(), 2UL);
  EXPECT_THAT(origin_site_potential_tokens,
              testing::UnorderedElementsAre(std::pair(origin, tokens),
                                            std::pair(origin_sub, tokens)));
  EXPECT_THAT(origin_site_potential_tokens,
              testing::ContainerEq(
                  persistence_provider_->GetPotentialPersistentTrialTokens(
                      origin_sub)));

  SiteOriginTrialTokens other_origin_site_potential_tokens =
      persistence_provider_->GetPotentialPersistentTrialTokens(other_origin);
  EXPECT_EQ(other_origin_site_potential_tokens.size(), 2UL);
  EXPECT_THAT(
      other_origin_site_potential_tokens,
      testing::UnorderedElementsAre(std::pair(other_origin, tokens),
                                    std::pair(other_origin_sub, tokens)));
  EXPECT_THAT(other_origin_site_potential_tokens,
              testing::ContainerEq(
                  persistence_provider_->GetPotentialPersistentTrialTokens(
                      other_origin_sub)));
}

TEST_F(LevelDbPersistenceProviderUnitTest, SiteKeyIsETLDPlusOne) {
  EXPECT_EQ("https://example.com",
            GetSiteKey(GetSecureOrigin("example.com")).Serialize());
  EXPECT_EQ("https://example.com",
            GetSiteKey(GetSecureOrigin("enabled.example.com")).Serialize());
  EXPECT_EQ("https://example.co.uk",
            GetSiteKey(GetSecureOrigin("example.co.uk")).Serialize());
  EXPECT_EQ("https://example.co.uk",
            GetSiteKey(GetSecureOrigin("enabled.example.co.uk")).Serialize());
}

// Note: this test relies on "blogspot.com" being recognized as an eTLD by the
// underlying origin parsing logic. Examples of other private registries this
// should also work for include "glitch.me" and "github.io".
TEST_F(LevelDbPersistenceProviderUnitTest,
       SiteKeyUsesPrivateRegistryAsEffectiveTopLevelDomain) {
  EXPECT_EQ("https://example.blogspot.com",
            GetSiteKey(GetSecureOrigin("example.blogspot.com")).Serialize());
  EXPECT_EQ(
      "https://example.blogspot.com",
      GetSiteKey(GetSecureOrigin("enabled.example.blogspot.com")).Serialize());
}

TEST_F(LevelDbPersistenceProviderUnitTest, SiteKeyCanBeIpAddress) {
  EXPECT_EQ("http://127.0.0.1",
            GetSiteKey(
                url::Origin::CreateFromNormalizedTuple("http", "127.0.0.1", 80))
                .Serialize());
}

TEST_F(LevelDbPersistenceProviderUnitTest, SiteKeyCanBeLocalhost) {
  EXPECT_EQ("http://localhost",
            GetSiteKey(
                url::Origin::CreateFromNormalizedTuple("http", "localhost", 80))
                .Serialize());
}

TEST_F(LevelDbPersistenceProviderUnitTest, SiteKeyCanHaveNonstandardPort) {
  EXPECT_EQ("http://example.com",
            GetSiteKey(url::Origin::CreateFromNormalizedTuple(
                           "http", "enabled.example.com", 5555))
                .Serialize());
}

TEST_F(LevelDbPersistenceProviderUnitTest,
       OpaqueOriginAsSiteKeySerializesAsEmptyValue) {
  EXPECT_EQ("null", GetSiteKey(url::Origin()).Serialize());
}
}  // namespace

}  // namespace origin_trials
