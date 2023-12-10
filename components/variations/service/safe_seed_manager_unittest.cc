// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/safe_seed_manager.h"

#include <memory>
#include <string>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/metrics/clean_exit_beacon.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/safe_seed_manager_base.h"
#include "components/variations/variations_safe_seed_store_local_state.h"
#include "components/variations/variations_seed_store.h"
#include "components/variations/variations_switches.h"
#include "components/variations/variations_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

namespace {

const char kTestSeed[] = "compressed, base-64 encoded serialized seed data";
const char kTestSignature[] = "a completely unforged signature, I promise!";
const int kTestSeedMilestone = 92;
const char kTestLocale[] = "en-US";
const char kTestPermanentConsistencyCountry[] = "US";
const char kTestSessionConsistencyCountry[] = "CA";

base::Time GetTestFetchTime() {
  return base::Time::FromDeltaSinceWindowsEpoch(base::Days(123));
}

// A simple fake data store.
class FakeSeedStore : public VariationsSeedStore {
 public:
  explicit FakeSeedStore(TestingPrefServiceSimple* local_state)
      : VariationsSeedStore(
            local_state,
            std::make_unique<VariationsSafeSeedStoreLocalState>(local_state)) {
    VariationsSeedStore::RegisterPrefs(local_state->registry());
  }

  FakeSeedStore(const FakeSeedStore&) = delete;
  FakeSeedStore& operator=(const FakeSeedStore&) = delete;

  ~FakeSeedStore() override = default;

  bool StoreSafeSeed(const std::string& seed_data,
                     const std::string& base64_seed_signature,
                     int seed_milestone,
                     const ClientFilterableState& client_state,
                     base::Time seed_fetch_time) override {
    seed_data_ = seed_data;
    signature_ = base64_seed_signature;
    seed_milestone_ = seed_milestone;
    date_ = client_state.reference_date;
    locale_ = client_state.locale;
    permanent_consistency_country_ = client_state.permanent_consistency_country;
    session_consistency_country_ = client_state.session_consistency_country;
    fetch_time_ = seed_fetch_time;
    return true;
  }

  const std::string& seed_data() const { return seed_data_; }
  const std::string& signature() const { return signature_; }
  int seed_milestone() const { return seed_milestone_; }
  const base::Time& date() const { return date_; }
  const std::string& locale() const { return locale_; }
  const std::string& permanent_consistency_country() const {
    return permanent_consistency_country_;
  }
  const std::string& session_consistency_country() const {
    return session_consistency_country_;
  }
  const base::Time& fetch_time() const { return fetch_time_; }

 private:
  // The stored data.
  std::string seed_data_;
  std::string signature_;
  int seed_milestone_ = 0;
  base::Time date_;
  std::string locale_;
  std::string permanent_consistency_country_;
  std::string session_consistency_country_;
  base::Time fetch_time_;
};

// Passes the default test values as the active state into |safe_seed_manager|
// and |local_state|.
void SetDefaultActiveState(SafeSeedManager* safe_seed_manager,
                           PrefService* local_state) {
  std::unique_ptr<ClientFilterableState> client_state =
      CreateDummyClientFilterableState();
  client_state->locale = kTestLocale;
  client_state->permanent_consistency_country =
      kTestPermanentConsistencyCountry;
  client_state->session_consistency_country = kTestSessionConsistencyCountry;
  client_state->reference_date = base::Time::UnixEpoch();

  local_state->SetInteger(prefs::kVariationsSeedMilestone, kTestSeedMilestone);

  safe_seed_manager->SetActiveSeedState(
      kTestSeed, kTestSignature, kTestSeedMilestone, std::move(client_state),
      GetTestFetchTime());
}

// Verifies that the default test values were written to the seed store.
void ExpectDefaultActiveState(const FakeSeedStore& seed_store) {
  EXPECT_EQ(kTestSeed, seed_store.seed_data());
  EXPECT_EQ(kTestSignature, seed_store.signature());
  EXPECT_EQ(kTestSeedMilestone, seed_store.seed_milestone());
  EXPECT_EQ(kTestLocale, seed_store.locale());
  EXPECT_EQ(kTestPermanentConsistencyCountry,
            seed_store.permanent_consistency_country());
  EXPECT_EQ(kTestSessionConsistencyCountry,
            seed_store.session_consistency_country());
  EXPECT_EQ(base::Time::UnixEpoch(), seed_store.date());
  EXPECT_EQ(GetTestFetchTime(), seed_store.fetch_time());
}

}  // namespace

class SafeSeedManagerTest : public ::testing::Test {
 public:
  SafeSeedManagerTest() {
    metrics::CleanExitBeacon::RegisterPrefs(prefs_.registry());
    SafeSeedManager::RegisterPrefs(prefs_.registry());
  }
  ~SafeSeedManagerTest() override = default;

 protected:
  TestingPrefServiceSimple prefs_;
};

TEST_F(SafeSeedManagerTest, RecordSuccessfulFetch_FirstCallSavesSafeSeed) {
  SafeSeedManager safe_seed_manager(&prefs_);
  FakeSeedStore seed_store(&prefs_);
  SetDefaultActiveState(&safe_seed_manager, &prefs_);

  safe_seed_manager.RecordSuccessfulFetch(&seed_store);
  ExpectDefaultActiveState(seed_store);
}

TEST_F(SafeSeedManagerTest, RecordSuccessfulFetch_RepeatedCallsRetainSafeSeed) {
  SafeSeedManager safe_seed_manager(&prefs_);
  FakeSeedStore seed_store(&prefs_);
  SetDefaultActiveState(&safe_seed_manager, &prefs_);

  safe_seed_manager.RecordSuccessfulFetch(&seed_store);
  safe_seed_manager.RecordSuccessfulFetch(&seed_store);
  safe_seed_manager.RecordSuccessfulFetch(&seed_store);
  ExpectDefaultActiveState(seed_store);
}

TEST_F(SafeSeedManagerTest,
       RecordSuccessfulFetch_NoActiveState_DoesntSaveSafeSeed) {
  SafeSeedManager safe_seed_manager(&prefs_);
  FakeSeedStore seed_store(&prefs_);
  // Omit setting any active state.

  safe_seed_manager.RecordSuccessfulFetch(&seed_store);
  EXPECT_EQ(std::string(), seed_store.seed_data());
  EXPECT_EQ(std::string(), seed_store.signature());
  EXPECT_EQ(0, seed_store.seed_milestone());
  EXPECT_EQ(std::string(), seed_store.locale());
  EXPECT_EQ(std::string(), seed_store.permanent_consistency_country());
  EXPECT_EQ(std::string(), seed_store.session_consistency_country());
  EXPECT_EQ(base::Time(), seed_store.date());
  EXPECT_EQ(base::Time(), seed_store.fetch_time());
}

TEST_F(SafeSeedManagerTest, FetchFailureMetrics_DefaultPrefs) {
  base::HistogramTester histogram_tester;
  SafeSeedManager safe_seed_manager(&prefs_);
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.Streak.FetchFailures", 0, 1);
}

TEST_F(SafeSeedManagerTest, FetchFailureMetrics_NoFailures) {
  prefs_.SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 0);

  base::HistogramTester histogram_tester;
  SafeSeedManager safe_seed_manager(&prefs_);
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.Streak.FetchFailures", 0, 1);
}

TEST_F(SafeSeedManagerTest, FetchFailureMetrics_SomeFailures) {
  prefs_.SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 2);

  base::HistogramTester histogram_tester;
  SafeSeedManager safe_seed_manager(&prefs_);
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.Streak.FetchFailures", 2, 1);
}

TEST_F(SafeSeedManagerTest, ShouldRunInSafeMode_OverriddenByCommandlineFlag) {
  // So many failures.
  prefs_.SetInteger(prefs::kVariationsCrashStreak, 100);
  prefs_.SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 100);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableVariationsSafeMode);

  SafeSeedManager safe_seed_manager(&prefs_);
  EXPECT_EQ(SeedType::kRegularSeed, safe_seed_manager.GetSeedType());
}

TEST_F(SafeSeedManagerTest, ShouldRunInSafeMode_NoCrashes_NoFetchFailures) {
  prefs_.SetInteger(prefs::kVariationsCrashStreak, 0);
  prefs_.SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 0);

  SafeSeedManager safe_seed_manager(&prefs_);
  EXPECT_EQ(SeedType::kRegularSeed, safe_seed_manager.GetSeedType());
}

TEST_F(SafeSeedManagerTest, ShouldRunInSafeMode_NoPrefs) {
  // Don't explicitly set either of the prefs. The implicit/default values
  // should be zero.
  SafeSeedManager safe_seed_manager(&prefs_);
  EXPECT_EQ(SeedType::kRegularSeed, safe_seed_manager.GetSeedType());
}

TEST_F(SafeSeedManagerTest, ShouldRunInSafeMode_FewCrashes_FewFetchFailures) {
  prefs_.SetInteger(prefs::kVariationsCrashStreak, 2);
  prefs_.SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 2);

  SafeSeedManager safe_seed_manager(&prefs_);
  EXPECT_EQ(SeedType::kRegularSeed, safe_seed_manager.GetSeedType());
}

TEST_F(SafeSeedManagerTest, ShouldRunInSafeMode_ManyCrashes_NoFetchFailures) {
  prefs_.SetInteger(prefs::kVariationsCrashStreak, 3);
  prefs_.SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 0);

  SafeSeedManager safe_seed_manager(&prefs_);
  EXPECT_EQ(SeedType::kSafeSeed, safe_seed_manager.GetSeedType());
}

TEST_F(SafeSeedManagerTest,
       ShouldRunInSafeMode_ManyMoreCrashes_NoFetchFailures) {
  prefs_.SetInteger(prefs::kVariationsCrashStreak, 6);
  prefs_.SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 0);

  SafeSeedManager safe_seed_manager(&prefs_);
  EXPECT_EQ(SeedType::kNullSeed, safe_seed_manager.GetSeedType());
}

TEST_F(SafeSeedManagerTest, ShouldRunInSafeMode_NoCrashes_ManyFetchFailures) {
  prefs_.SetInteger(prefs::kVariationsCrashStreak, 0);
  prefs_.SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 25);

  SafeSeedManager safe_seed_manager(&prefs_);
  EXPECT_EQ(SeedType::kSafeSeed, safe_seed_manager.GetSeedType());
}

TEST_F(SafeSeedManagerTest,
       ShouldRunInSafeMode_NoCrashes_ManyMoreFetchFailures) {
  prefs_.SetInteger(prefs::kVariationsCrashStreak, 0);
  prefs_.SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 50);

  SafeSeedManager safe_seed_manager(&prefs_);
  EXPECT_EQ(SeedType::kNullSeed, safe_seed_manager.GetSeedType());
}

TEST_F(SafeSeedManagerTest, ShouldRunInSafeMode_ManyCrashes_ManyFetchFailures) {
  prefs_.SetInteger(prefs::kVariationsCrashStreak, 3);
  prefs_.SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 25);

  SafeSeedManager safe_seed_manager(&prefs_);
  EXPECT_EQ(SeedType::kSafeSeed, safe_seed_manager.GetSeedType());
}

TEST_F(SafeSeedManagerTest,
       ShouldRunInSafeMode_ManyMoreCrashes_ManyMoreFetchFailures) {
  prefs_.SetInteger(prefs::kVariationsCrashStreak, 6);
  prefs_.SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 50);

  SafeSeedManager safe_seed_manager(&prefs_);
  EXPECT_EQ(SeedType::kNullSeed, safe_seed_manager.GetSeedType());
}

}  // namespace variations
