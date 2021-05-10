// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_use_measurement/core/data_use_tracker_prefs.h"

#include <string>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "components/data_use_measurement/core/data_use_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_use_measurement {

void RegisterPrefs(TestingPrefServiceSimple* test_prefs) {
  test_prefs->registry()->RegisterDictionaryPref(
      prefs::kDataUsedUserForeground);
  test_prefs->registry()->RegisterDictionaryPref(
      prefs::kDataUsedUserBackground);
  test_prefs->registry()->RegisterDictionaryPref(
      prefs::kDataUsedServicesForeground);
  test_prefs->registry()->RegisterDictionaryPref(
      prefs::kDataUsedServicesBackground);
}

class DataUseTrackerPrefsTest {
 public:
  DataUseTrackerPrefsTest(base::SimpleTestClock* clock,
                          TestingPrefServiceSimple* test_prefs)
      : data_use_tracker_prefs_(clock, test_prefs) {
    // Register the prefs before accessing them.
  }

  DataUseTrackerPrefsTest(const DataUseTrackerPrefsTest&) = delete;
  DataUseTrackerPrefsTest& operator=(const DataUseTrackerPrefsTest&) = delete;

  DataUseTrackerPrefs* data_use_tracker_prefs() {
    return &data_use_tracker_prefs_;
  }

 private:
  DataUseTrackerPrefs data_use_tracker_prefs_;
};

// Verifies that the prefs are stored correctly: The date is used as the key
// in the pref and the expired keys are removed.
TEST(DataUseTrackerPrefsTest, PrefsOnMeteredConnection) {
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  TestingPrefServiceSimple test_prefs;
  RegisterPrefs(&test_prefs);

  // Report 2 data uses for the same day.
  DataUseTrackerPrefsTest tracker_prefs_test_1(&clock, &test_prefs);
  tracker_prefs_test_1.data_use_tracker_prefs()->ReportNetworkServiceDataUse(
      true, true, true, 10);
  EXPECT_EQ(
      1u, test_prefs.GetDictionary(prefs::kDataUsedUserForeground)->DictSize());
  tracker_prefs_test_1.data_use_tracker_prefs()->ReportNetworkServiceDataUse(
      true, true, true, 10);
  EXPECT_EQ(
      1u, test_prefs.GetDictionary(prefs::kDataUsedUserForeground)->DictSize());

  // Verify other prefs are not set.
  EXPECT_TRUE(
      test_prefs.GetDictionary(prefs::kDataUsedUserBackground)->DictEmpty());
  EXPECT_TRUE(
      test_prefs.GetDictionary(prefs::kDataUsedServicesForeground)->DictEmpty());
  EXPECT_TRUE(
      test_prefs.GetDictionary(prefs::kDataUsedServicesBackground)->DictEmpty());

  // Move clock forward 10 days. New data use reported must go in a separate
  // entry in the dictionary pref.
  clock.Advance(base::TimeDelta::FromDays(10));
  DataUseTrackerPrefsTest tracker_prefs_test_2(&clock, &test_prefs);
  EXPECT_EQ(
      1u, test_prefs.GetDictionary(prefs::kDataUsedUserForeground)->DictSize());
  tracker_prefs_test_2.data_use_tracker_prefs()->ReportNetworkServiceDataUse(
      true, true, true, 10);
  EXPECT_EQ(
      2u, test_prefs.GetDictionary(prefs::kDataUsedUserForeground)->DictSize());

  // Move clock forward 55 days. This should clean up the first entry since they
  // are now 65 days older (i.e., more than 60 days old). New data use reported
  // must go in a separate entry in the dictionary pref.
  clock.Advance(base::TimeDelta::FromDays(55));
  DataUseTrackerPrefsTest tracker_prefs_test_3(&clock, &test_prefs);
  EXPECT_EQ(
      1u, test_prefs.GetDictionary(prefs::kDataUsedUserForeground)->DictSize());
  tracker_prefs_test_2.data_use_tracker_prefs()->ReportNetworkServiceDataUse(
      true, true, true, 10);
  EXPECT_EQ(
      2u, test_prefs.GetDictionary(prefs::kDataUsedUserForeground)->DictSize());
}

// Verifies that the prefs are not updated on unmetered connections.
TEST(DataUseTrackerPrefsTest, PrefsOnUnmeteredConnection) {
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  TestingPrefServiceSimple test_prefs;
  RegisterPrefs(&test_prefs);

  // Report 2 data uses for the same day.
  DataUseTrackerPrefsTest tracker_prefs_test_1(&clock, &test_prefs);
  tracker_prefs_test_1.data_use_tracker_prefs()->ReportNetworkServiceDataUse(
      /*is_metered_connection=*/false, true, true, 10);
  tracker_prefs_test_1.data_use_tracker_prefs()->ReportNetworkServiceDataUse(
      /*is_metered_connection=*/false, true, true, 10);

  // Verify prefs are not set.
  EXPECT_TRUE(
      test_prefs.GetDictionary(prefs::kDataUsedUserForeground)->DictEmpty());
  EXPECT_TRUE(
      test_prefs.GetDictionary(prefs::kDataUsedUserBackground)->DictEmpty());
  EXPECT_TRUE(
      test_prefs.GetDictionary(prefs::kDataUsedServicesForeground)->DictEmpty());
  EXPECT_TRUE(
      test_prefs.GetDictionary(prefs::kDataUsedServicesBackground)->DictEmpty());
}

TEST(DataUseTrackerPrefsTest, TestBasicUserForeground) {
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  TestingPrefServiceSimple test_prefs;
  RegisterPrefs(&test_prefs);

  // Report 2 data uses for the same day.
  DataUseTrackerPrefsTest tracker_prefs_test(&clock, &test_prefs);

  const struct {
    bool foreground;
    bool user_initiated;
    std::string pref_expected_as_non_empty;
  } tests[] = {
      {false, false, prefs::kDataUsedServicesBackground},
      {false, true, prefs::kDataUsedUserBackground},
      {true, false, prefs::kDataUsedServicesForeground},
      {true, true, prefs::kDataUsedUserForeground},
  };

  for (const auto& test : tests) {
    test_prefs.ClearPref(prefs::kDataUsedServicesBackground);
    test_prefs.ClearPref(prefs::kDataUsedUserBackground);
    test_prefs.ClearPref(prefs::kDataUsedServicesForeground);
    test_prefs.ClearPref(prefs::kDataUsedServicesForeground);

    tracker_prefs_test.data_use_tracker_prefs()->ReportNetworkServiceDataUse(
        true, test.foreground, test.user_initiated, 10);
    // Verify that the expected pref has an entry.
    EXPECT_FALSE(
        test_prefs.GetDictionary(test.pref_expected_as_non_empty)->DictEmpty());

    // Verify other prefs are not set.
    EXPECT_TRUE(
        test.pref_expected_as_non_empty == prefs::kDataUsedUserForeground ||
        test_prefs.GetDictionary(prefs::kDataUsedUserForeground)->DictEmpty());
    EXPECT_TRUE(
        test.pref_expected_as_non_empty == prefs::kDataUsedUserBackground ||
        test_prefs.GetDictionary(prefs::kDataUsedUserBackground)->DictEmpty());
    EXPECT_TRUE(
        test.pref_expected_as_non_empty == prefs::kDataUsedServicesForeground ||
        test_prefs.GetDictionary(prefs::kDataUsedServicesForeground)->DictEmpty());
    EXPECT_TRUE(
        test.pref_expected_as_non_empty == prefs::kDataUsedServicesBackground ||
        test_prefs.GetDictionary(prefs::kDataUsedServicesBackground)->DictEmpty());
  }
}

}  // namespace data_use_measurement
