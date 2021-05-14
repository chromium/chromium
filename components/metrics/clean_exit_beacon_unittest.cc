// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/clean_exit_beacon.h"

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace {

const wchar_t kDummyWindowsRegistryKey[] = L"";

}  // namespace

TEST(CleanExitBeaconTest, CrashStreakMetricWithDefaultPrefs) {
  TestingPrefServiceSimple pref_service;
  CleanExitBeacon::RegisterPrefs(pref_service.registry());
  base::HistogramTester histogram_tester;

  CleanExitBeacon clean_exit_beacon(kDummyWindowsRegistryKey, &pref_service);
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 0,
                                      1);
}

TEST(CleanExitBeaconTest, CrashStreakMetricWithNoCrashes) {
  TestingPrefServiceSimple pref_service;
  CleanExitBeacon::RegisterPrefs(pref_service.registry());
  // The default value for kStabilityExitedCleanly is true, but defaults can
  // change, so we explicitly set it to true here. Similarly, we explicitly set
  // kVariationsCrashStreak to 0.
  pref_service.SetBoolean(prefs::kStabilityExitedCleanly, true);
  pref_service.SetInteger(variations::prefs::kVariationsCrashStreak, 0);
  base::HistogramTester histogram_tester;

  CleanExitBeacon clean_exit_beacon(kDummyWindowsRegistryKey, &pref_service);
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 0,
                                      1);
}

TEST(CleanExitBeaconTest, CrashStreakMetricWithSomeCrashes) {
  TestingPrefServiceSimple pref_service;
  CleanExitBeacon::RegisterPrefs(pref_service.registry());
  // The default value for kStabilityExitedCleanly is true, but defaults can
  // change, so we explicitly set it to true here.
  pref_service.SetBoolean(prefs::kStabilityExitedCleanly, true);
  pref_service.SetInteger(variations::prefs::kVariationsCrashStreak, 1);
  base::HistogramTester histogram_tester;

  CleanExitBeacon clean_exit_beacon(kDummyWindowsRegistryKey, &pref_service);
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 1,
                                      1);
}

TEST(CleanExitBeaconTest, CrashIncrementsCrashStreak) {
  TestingPrefServiceSimple pref_service;
  CleanExitBeacon::RegisterPrefs(pref_service.registry());
  pref_service.SetBoolean(prefs::kStabilityExitedCleanly, false);
  pref_service.SetInteger(variations::prefs::kVariationsCrashStreak, 1);
  base::HistogramTester histogram_tester;

  CleanExitBeacon clean_exit_beacon(kDummyWindowsRegistryKey, &pref_service);
  EXPECT_EQ(pref_service.GetInteger(variations::prefs::kVariationsCrashStreak),
            2);
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 2,
                                      1);
}

TEST(CleanExitBeaconTest,
     CrashIncrementsCrashStreakWithDefaultCrashStreakPref) {
  TestingPrefServiceSimple pref_service;
  CleanExitBeacon::RegisterPrefs(pref_service.registry());
  pref_service.SetBoolean(prefs::kStabilityExitedCleanly, false);
  base::HistogramTester histogram_tester;

  CleanExitBeacon clean_exit_beacon(kDummyWindowsRegistryKey, &pref_service);
  EXPECT_EQ(pref_service.GetInteger(variations::prefs::kVariationsCrashStreak),
            1);
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 1,
                                      1);
}

}  // namespace metrics
