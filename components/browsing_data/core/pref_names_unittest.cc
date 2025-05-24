// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/pref_names.h"

#include <gtest/gtest.h>

#include "base/test/metrics/histogram_tester.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"

namespace {

const int kPreviousDefaultTimePeriod =
    static_cast<int>(browsing_data::TimePeriod::LAST_HOUR);
const int kNewDefaultTimePeriod =
    static_cast<int>(browsing_data::TimePeriod::LAST_15_MINUTES);
const int kNeitherDefaultTimePeriod =
    static_cast<int>(browsing_data::TimePeriod::LAST_DAY);

}  // namespace

// Tests that the kDeleteTimePeriod and kCloseTabs prefs are migrated to the new
// defaults when the prefs are using the default values.
TEST(BrowsingDataPrefNamesMigrationTest,
     MaybeMigrateToQuickDeletePrefValues_DefaultValues) {
  base::HistogramTester histogram_tester;
  sync_preferences::TestingPrefServiceSyncable prefs;
  browsing_data::prefs::RegisterBrowserUserPrefs(prefs.registry());

  browsing_data::prefs::MaybeMigrateToQuickDeletePrefValues(&prefs);

  EXPECT_EQ(prefs.GetInteger(browsing_data::prefs::kDeleteTimePeriod),
            kNewDefaultTimePeriod);
  EXPECT_EQ(prefs.GetBoolean(browsing_data::prefs::kCloseTabs), true);
  EXPECT_EQ(
      prefs.GetBoolean(browsing_data::prefs::kMigratedToQuickDeletePrefValues),
      true);

  histogram_tester.ExpectUniqueSample(
      "Privacy.DeleteBrowsingData.MigratedToNewDefaults", true, 1);
}

// Tests that the kDeleteTimePeriod and kCloseTabs prefs are not migrated to the
// new defaults when the prefs are not using the default values.
TEST(BrowsingDataPrefNamesMigrationTest,
     MaybeMigrateToQuickDeletePrefValues_NotDefaultValues) {
  base::HistogramTester histogram_tester;
  sync_preferences::TestingPrefServiceSyncable prefs;
  browsing_data::prefs::RegisterBrowserUserPrefs(prefs.registry());

  // Set time period to another value other than the defaults.
  prefs.SetInteger(browsing_data::prefs::kDeleteTimePeriod,
                   kNeitherDefaultTimePeriod);

  browsing_data::prefs::MaybeMigrateToQuickDeletePrefValues(&prefs);

  EXPECT_EQ(prefs.GetInteger(browsing_data::prefs::kDeleteTimePeriod),
            kNeitherDefaultTimePeriod);
  EXPECT_EQ(prefs.GetBoolean(browsing_data::prefs::kCloseTabs), false);
  EXPECT_EQ(
      prefs.GetBoolean(browsing_data::prefs::kMigratedToQuickDeletePrefValues),
      true);

  histogram_tester.ExpectUniqueSample(
      "Privacy.DeleteBrowsingData.MigratedToNewDefaults", false, 1);
}

// Tests that the kDeleteTimePeriod and kCloseTabs prefs are not migrated when
// the user has already been migrated before.
TEST(BrowsingDataPrefNamesMigrationTest,
     MaybeMigrateToQuickDeletePrefValues_AlreadyMigrated) {
  base::HistogramTester histogram_tester;
  sync_preferences::TestingPrefServiceSyncable prefs;
  browsing_data::prefs::RegisterBrowserUserPrefs(prefs.registry());

  // Set time period to the old default and tabs to disabled.
  prefs.SetInteger(browsing_data::prefs::kDeleteTimePeriod,
                   kPreviousDefaultTimePeriod);
  prefs.SetBoolean(browsing_data::prefs::kCloseTabs, true);

  // Set the migration pref to true.
  prefs.SetBoolean(browsing_data::prefs::kMigratedToQuickDeletePrefValues,
                   true);

  browsing_data::prefs::MaybeMigrateToQuickDeletePrefValues(&prefs);

  EXPECT_EQ(prefs.GetInteger(browsing_data::prefs::kDeleteTimePeriod),
            kPreviousDefaultTimePeriod);
  EXPECT_EQ(prefs.GetBoolean(browsing_data::prefs::kCloseTabs), true);
  EXPECT_EQ(
      prefs.GetBoolean(browsing_data::prefs::kMigratedToQuickDeletePrefValues),
      true);

  histogram_tester.ExpectUniqueSample(
      "Privacy.DeleteBrowsingData.MigratedToNewDefaults", false, 0);
  histogram_tester.ExpectUniqueSample(
      "Privacy.DeleteBrowsingData.MigratedToNewDefaults", true, 0);
}
