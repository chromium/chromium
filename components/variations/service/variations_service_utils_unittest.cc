// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_service_utils.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/network_time/network_time_tracker.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_ids_provider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"  // IWYU pragma: keep
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
namespace {

using VariationsServiceUtilsTest = ::testing::Test;

// Verifies that the seed expiration status is correctly computed based on its
// fetch time and the binary build time.
TEST(VariationsServiceUtilsTest, HasSeedExpiredSinceTime) {
  base::Time now = base::Time::Now();
  struct {
    const base::Time fetch_time;
    const base::Time build_time;
    const bool expired;
  } test_cases[] = {
      // Verifies that seed should NOT expire if it does NOT exceed the maximum
      // age permitted for a variations seed.
      {now - base::Days(30), now, false},
      // Verifies that when the binary is newer than the seed, the seed should
      // be considered expired if it exceeds maximum age permitted.
      {now - base::Days(31), now, true},
      // Verifies that when the binary is older than the seed, the seed should
      // NOT be considered expired even when it exceeds maximum age permitted.
      {now - base::Days(31), now - base::Days(32), false},
  };
  for (const auto& test : test_cases) {
    EXPECT_EQ(HasSeedExpiredSinceTimeHelperForTesting(test.fetch_time,
                                                      test.build_time),
              test.expired);
  }
}

TEST(VariationsServiceUtilsTest, CreateVariationsIdsProvider) {
  // Declare `feature_list` before `task_environment` so `base::FeatureList`
  // outlives the worker threads.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(network_time::kNetworkTimeServiceQuerying);
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestingPrefServiceSimple pref_service;
  network_time::NetworkTimeTracker::RegisterPrefs(pref_service.registry());

  std::unique_ptr<network_time::NetworkTimeTracker> tracker =
      network_time::NetworkTimeTracker::CreateUninitialized();
  VariationsIdsProvider* provider = CreateVariationsIdsProvider(
      VariationsIdsProvider::Mode::kUseSignedInState, tracker.get());
  ASSERT_NE(provider, nullptr);
  test::ScopedVariationsIdsProvider scoped_provider =
      test::ScopedVariationsIdsProvider::AdoptCurrentInstanceForTesting();

  EXPECT_EQ(provider, VariationsIdsProvider::GetInstance());
  EXPECT_EQ(provider->mode(), VariationsIdsProvider::Mode::kUseSignedInState);
  EXPECT_EQ(provider->GetCurrentTime(), base::Time::Now());

  tracker->Initialize(&pref_service, /*url_loader_factory=*/nullptr);
  const base::Time network_time = base::Time::Now() + base::Hours(2);
  constexpr base::TimeDelta kLatency = base::Milliseconds(50);
  tracker->UpdateNetworkTime(network_time - kLatency / 2,
                             /*resolution=*/base::Milliseconds(17), kLatency,
                             base::TimeTicks::Now());
  EXPECT_EQ(provider->GetCurrentTime(), network_time);
}

}  // namespace
}  // namespace variations
