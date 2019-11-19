// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/safe_seed_manager.h"

#include <memory>
#include <string>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/pref_names.h"
#include "components/variations/variations_seed_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
namespace {

const char kTestSeed[] = "compressed, base-64 encoded serialized seed data";
const char kTestSignature[] = "a completely unforged signature, I promise!";
const char kTestLocale[] = "en-US";
const char kTestPermanentConsistencyCountry[] = "US";
const char kTestSessionConsistencyCountry[] = "CA";

base::Time GetTestFetchTime() {
  return base::Time::FromDeltaSinceWindowsEpoch(base::TimeDelta::FromDays(123));
}

// A simple fake data store.
class FakeSeedStore : public VariationsSeedStore {
 public:
  explicit FakeSeedStore(PrefService* local_state)
      : VariationsSeedStore(local_state) {}
  ~FakeSeedStore() override = default;

  bool StoreSafeSeed(const std::string& seed_data,
                     const std::string& base64_seed_signature,
                     const ClientFilterableState& client_state,
                     base::Time seed_fetch_time) override {
    seed_data_ = seed_data;
    signature_ = base64_seed_signature;
    date_ = client_state.reference_date;
    locale_ = client_state.locale;
    permanent_consistency_country_ = client_state.permanent_consistency_country;
    session_consistency_country_ = client_state.session_consistency_country;
    fetch_time_ = seed_fetch_time;
    return true;
  }

  const std::string& seed_data() const { return seed_data_; }
  const std::string& signature() const { return signature_; }
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
  base::Time date_;
  std::string locale_;
  std::string permanent_consistency_country_;
  std::string session_consistency_country_;
  base::Time fetch_time_;

  DISALLOW_COPY_AND_ASSIGN(FakeSeedStore);
};

// Passes the default test values as the active state into the
// |safe_seed_manager|.
void SetDefaultActiveState(SafeSeedManager* safe_seed_manager) {
  std::unique_ptr<ClientFilterableState> client_state =
      std::make_unique<ClientFilterableState>(base::OnceCallback<bool()>());
  client_state->locale = kTestLocale;
  client_state->permanent_consistency_country =
      kTestPermanentConsistencyCountry;
  client_state->session_consistency_country = kTestSessionConsistencyCountry;
  client_state->reference_date = base::Time::UnixEpoch();

  safe_seed_manager->SetActiveSeedState(
      kTestSeed, kTestSignature, std::move(client_state), GetTestFetchTime());
}

// Verifies that the default test values were written to the seed store.
void ExpectDefaultActiveState(const FakeSeedStore& seed_store) {
  EXPECT_EQ(kTestSeed, seed_store.seed_data());
  EXPECT_EQ(kTestSignature, seed_store.signature());
  EXPECT_EQ(kTestLocale, seed_store.locale());
  EXPECT_EQ(kTestPermanentConsistencyCountry,
            seed_store.permanent_consistency_country());
  EXPECT_EQ(kTestSessionConsistencyCountry,
            seed_store.session_consistency_country());
  EXPECT_EQ(base::Time::UnixEpoch(), seed_store.date());
  EXPECT_EQ(GetTestFetchTime(), seed_store.fetch_time());
}

}  // namespace

class SafeSeedManagerTest : public testing::Test {
 public:
  SafeSeedManagerTest() { SafeSeedManager::RegisterPrefs(prefs_.registry()); }
  ~SafeSeedManagerTest() override = default;

 protected:
  TestingPrefServiceSimple prefs_;
};

TEST_F(SafeSeedManagerTest, RecordSuccessfulFetch_FirstCallSavesSafeSeed) {
  SafeSeedManager safe_seed_manager(true, &prefs_);
  SetDefaultActiveState(&safe_seed_manager);

  FakeSeedStore seed_store(&prefs_);
  safe_seed_manager.RecordSuccessfulFetch(&seed_store);

  ExpectDefaultActiveState(seed_store);
}

TEST_F(SafeSeedManagerTest, RecordSuccessfulFetch_RepeatedCallsRetainSafeSeed) {
  SafeSeedManager safe_seed_manager(true, &prefs_);
  SetDefaultActiveState(&safe_seed_manager);

  FakeSeedStore seed_store(&prefs_);
  safe_seed_manager.RecordSuccessfulFetch(&seed_store);
  safe_seed_manager.RecordSuccessfulFetch(&seed_store);
  safe_seed_manager.RecordSuccessfulFetch(&seed_store);

  ExpectDefaultActiveState(seed_store);
}

TEST_F(SafeSeedManagerTest,
       RecordSuccessfulFetch_NoActiveState_DoesntSaveSafeSeed) {
  SafeSeedManager safe_seed_manager(true, &prefs_);
  // Omit setting any active state.

  FakeSeedStore seed_store(&prefs_);
  safe_seed_manager.RecordSuccessfulFetch(&seed_store);

  EXPECT_EQ(std::string(), seed_store.seed_data());
  EXPECT_EQ(std::string(), seed_store.signature());
  EXPECT_EQ(std::string(), seed_store.locale());
  EXPECT_EQ(std::string(), seed_store.permanent_consistency_country());
  EXPECT_EQ(std::string(), seed_store.session_consistency_country());
  EXPECT_EQ(base::Time(), seed_store.date());
  EXPECT_EQ(base::Time(), seed_store.fetch_time());
}

TEST_F(SafeSeedManagerTest, StreakMetrics_NoPrefs) {
  base::HistogramTester histogram_tester;
  SafeSeedManager safe_seed_manager(true, &prefs_);
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 0,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.Streak.FetchFailures", 0, 1);
}

TEST_F(SafeSeedManagerTest, StreakMetrics_NoCrashes_NoFetchFailures) {
  prefs_.SetInteger(prefs::kVariationsCrashStreak, 0);
  prefs_.SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 0);

  base::HistogramTester histogram_tester;
  SafeSeedManager safe_seed_manager(true, &prefs_);
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 0,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.Streak.FetchFailures", 0, 1);
}

TEST_F(SafeSeedManagerTest, StreakMetrics_SomeCrashes_SomeFetchFailures) {
  prefs_.SetInteger(prefs::kVariationsCrashStreak, 1);
  prefs_.SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 2);

  base::HistogramTester histogram_tester;
  SafeSeedManager safe_seed_manager(true, &prefs_);
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 1,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.Streak.FetchFailures", 2, 1);
}

TEST_F(SafeSeedManagerTest, StreakMetrics_CrashIncrementsCrashStreak) {
  prefs_.SetInteger(prefs::kVariationsCrashStreak, 1);

  base::HistogramTester histogram_tester;
  SafeSeedManager safe_seed_manager(false, &prefs_);

  EXPECT_EQ(2, prefs_.GetInteger(prefs::kVariationsCrashStreak));
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 2,
                                      1);
}

TEST_F(SafeSeedManagerTest, StreakMetrics_CrashIncrementsCrashStreak_NoPrefs) {
  base::HistogramTester histogram_tester;
  SafeSeedManager safe_seed_manager(false, &prefs_);

  EXPECT_EQ(1, prefs_.GetInteger(prefs::kVariationsCrashStreak));
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 1,
                                      1);
}

TEST_F(SafeSeedManagerTest, ShouldRunInSafeMode_OverriddenByCommandlineFlag) {
  // So many failures.
  prefs_.SetInteger(prefs::kVariationsCrashStreak, 100);
  prefs_.SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 100);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ::switches::kForceFieldTrials, "SomeFieldTrial");

  SafeSeedManager safe_seed_manager(true, &prefs_);
  EXPECT_FALSE(safe_seed_manager.ShouldRunInSafeMode());
}

TEST_F(SafeSeedManagerTest, ShouldRunInSafeMode_NoCrashes_NoFetchFailures) {
  prefs_.SetInteger(prefs::kVariationsCrashStreak, 0);
  prefs_.SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 0);

  SafeSeedManager safe_seed_manager(true, &prefs_);
  EXPECT_FALSE(safe_seed_manager.ShouldRunInSafeMode());
}

TEST_F(SafeSeedManagerTest, ShouldRunInSafeMode_NoPrefs) {
  // Don't explicitly set either of the prefs. The implicit/default values
  // should be zero.
  SafeSeedManager safe_seed_manager(true, &prefs_);
  EXPECT_FALSE(safe_seed_manager.ShouldRunInSafeMode());
}

TEST_F(SafeSeedManagerTest, ShouldRunInSafeMode_FewCrashes_FewFetchFailures) {
  prefs_.SetInteger(prefs::kVariationsCrashStreak, 2);
  prefs_.SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 2);

  SafeSeedManager safe_seed_manager(true, &prefs_);
  EXPECT_FALSE(safe_seed_manager.ShouldRunInSafeMode());
}

TEST_F(SafeSeedManagerTest, ShouldRunInSafeMode_ManyCrashes_NoFetchFailures) {
  prefs_.SetInteger(prefs::kVariationsCrashStreak, 3);
  prefs_.SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 0);

  SafeSeedManager safe_seed_manager(true, &prefs_);
  EXPECT_TRUE(safe_seed_manager.ShouldRunInSafeMode());
}

TEST_F(SafeSeedManagerTest, ShouldRunInSafeMode_NoCrashes_ManyFetchFailures) {
  prefs_.SetInteger(prefs::kVariationsCrashStreak, 0);
  prefs_.SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 50);

  SafeSeedManager safe_seed_manager(true, &prefs_);
  EXPECT_TRUE(safe_seed_manager.ShouldRunInSafeMode());
}

TEST_F(SafeSeedManagerTest, ShouldRunInSafeMode_ManyCrashes_ManyFetchFailures) {
  prefs_.SetInteger(prefs::kVariationsCrashStreak, 3);
  prefs_.SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 50);

  SafeSeedManager safe_seed_manager(true, &prefs_);
  EXPECT_TRUE(safe_seed_manager.ShouldRunInSafeMode());
}

}  // namespace variations
