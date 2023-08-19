// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/client_filterable_state.h"

#include "base/build_time.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/variations_seed_store.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace variations {

class ClientFilterableStateTest : public ::testing::Test {
 public:
  ClientFilterableStateTest() : build_time_(base::GetBuildTime()) {
    VariationsSeedStore::RegisterPrefs(local_state_.registry());
  }
  const base::Time build_time() { return build_time_; }
  PrefService* local_state() { return &local_state_; }

 private:
  const base::Time build_time_;
  TestingPrefServiceSimple local_state_;
};

TEST_F(ClientFilterableStateTest, IsEnterprise) {
  // Test, for non enterprise clients, is_enterprise_function_ is called once.
  ClientFilterableState client_non_enterprise(
      base::BindOnce([] { return false; }),
      base::BindOnce([] { return base::flat_set<uint64_t>(); }));
  EXPECT_FALSE(client_non_enterprise.IsEnterprise());
  EXPECT_FALSE(client_non_enterprise.IsEnterprise());

  // Test, for enterprise clients, is_enterprise_function_ is called once.
  ClientFilterableState client_enterprise(
      base::BindOnce([] { return true; }),
      base::BindOnce([] { return base::flat_set<uint64_t>(); }));
  EXPECT_TRUE(client_enterprise.IsEnterprise());
  EXPECT_TRUE(client_enterprise.IsEnterprise());
}

TEST_F(ClientFilterableStateTest, GoogleGroups) {
  // Test that google_groups_function_ is called once.
  base::flat_set<uint64_t> expected_google_groups =
      base::flat_set<uint64_t>(1234, 5678);
  ClientFilterableState client(
      base::BindOnce([] { return false; }),
      base::BindOnce([] { return base::flat_set<uint64_t>(1234, 5678); }));
  EXPECT_EQ(client.GoogleGroups(), expected_google_groups);
  EXPECT_EQ(client.GoogleGroups(), expected_google_groups);
}

// Verifies that GetTimeForStudyDateChecks() returns the server timestamp for
// when the regular seed was fetched,|kVariationsSeedDate|, when the time is
// more recent than the build time.
TEST_F(ClientFilterableStateTest, RegularSeedTimeReturned) {
  const base::Time seed_fetch_time = build_time() + base::Days(4);
  local_state()->SetTime(prefs::kVariationsSeedDate, seed_fetch_time);
  EXPECT_EQ(ClientFilterableState::GetTimeForStudyDateChecks(
                /*is_safe_seed=*/false, local_state()),
            seed_fetch_time);
}

// Verifies that GetTimeForStudyDateChecks() returns the server timestamp for
// when the safe seed was fetched, |kVariationsSafeSeedDate|, when the time is
// more recent than the build time.
TEST_F(ClientFilterableStateTest, SafeSeedTimeReturned) {
  const base::Time safe_seed_fetch_time = build_time() + base::Days(7);
  local_state()->SetTime(prefs::kVariationsSafeSeedDate, safe_seed_fetch_time);
  EXPECT_EQ(ClientFilterableState::GetTimeForStudyDateChecks(
                /*is_safe_seed=*/true, local_state()),
            safe_seed_fetch_time);
}

// Verifies that GetTimeForStudyDateChecks() returns the build time when it is
// more recent than |kVariationsSeedDate|.
TEST_F(ClientFilterableStateTest, BuildTimeReturnedForRegularSeed) {
  const base::Time seed_fetch_time = build_time() - base::Days(2);
  local_state()->SetTime(prefs::kVariationsSeedDate, seed_fetch_time);
  EXPECT_EQ(ClientFilterableState::GetTimeForStudyDateChecks(
                /*is_safe_seed=*/false, local_state()),
            build_time());
}

// Verifies that GetTimeForStudyDateChecks() returns the build time when it is
// more recent than |kVariationsSafeSeedDate|.
TEST_F(ClientFilterableStateTest, BuildTimeReturnedForSafeSeed) {
  const base::Time safe_seed_fetch_time = build_time() - base::Days(3);
  local_state()->SetTime(prefs::kVariationsSafeSeedDate, safe_seed_fetch_time);
  EXPECT_EQ(ClientFilterableState::GetTimeForStudyDateChecks(
                /*is_safe_seed=*/true, local_state()),
            build_time());
}

// Verifies that GetTimeForStudyDateChecks() returns the build time when the
// seed time is null.
TEST_F(ClientFilterableStateTest, BuildTimeReturnedForNullSeedTimes) {
  ASSERT_TRUE(local_state()->GetTime(prefs::kVariationsSeedDate).is_null());
  EXPECT_EQ(ClientFilterableState::GetTimeForStudyDateChecks(
                /*is_safe_seed=*/false, local_state()),
            build_time());

  ASSERT_TRUE(local_state()->GetTime(prefs::kVariationsSafeSeedDate).is_null());
  EXPECT_EQ(ClientFilterableState::GetTimeForStudyDateChecks(
                /*is_safe_seed=*/true, local_state()),
            build_time());
}

}  // namespace variations
