// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/user_tuning/prefs.h"

#include <utility>
#include "base/json/values_util.h"
#include "base/values.h"
#include "components/performance_manager/public/features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::user_tuning::prefs {

class MemorySaverModePrefMigrationTest : public ::testing::Test {
 public:
  void SetUp() override { RegisterLocalStatePrefs(pref_service_.registry()); }

  TestingPrefServiceSimple pref_service_;
};

TEST_F(MemorySaverModePrefMigrationTest, NoChangeToUserSetNewPref) {
  // The old pref is set by the user, but so is the new pref so no migration
  // should happen.
  pref_service_.SetBoolean(kMemorySaverModeEnabled, true);
  pref_service_.SetInteger(kMemorySaverModeState,
                           static_cast<int>(MemorySaverModeState::kDisabled));

  MigrateMemorySaverModePref(&pref_service_);

  EXPECT_EQ(pref_service_.GetInteger(kMemorySaverModeState),
            static_cast<int>(MemorySaverModeState::kDisabled));
  // The old pref should be reset.
  EXPECT_TRUE(
      pref_service_.FindPreference(kMemorySaverModeEnabled)->IsDefaultValue());
  EXPECT_FALSE(pref_service_.GetBoolean(kMemorySaverModeEnabled));
}

TEST_F(MemorySaverModePrefMigrationTest, BothPrefsDefaultNoMigration) {
  // Simulate that the default enum state value is not "disabled"
  pref_service_.SetDefaultPrefValue(kMemorySaverModeState, base::Value(1));

  EXPECT_EQ(pref_service_.GetInteger(kMemorySaverModeState),
            static_cast<int>(MemorySaverModeState::kDeprecated));
  EXPECT_TRUE(
      pref_service_.FindPreference(kMemorySaverModeState)->IsDefaultValue());

  MigrateMemorySaverModePref(&pref_service_);

  // Both prefs were in the default state, no migration happens
  EXPECT_TRUE(
      pref_service_.FindPreference(kMemorySaverModeState)->IsDefaultValue());
  EXPECT_EQ(pref_service_.GetInteger(kMemorySaverModeState),
            static_cast<int>(MemorySaverModeState::kDeprecated));
}

TEST_F(MemorySaverModePrefMigrationTest,
       MigrateDefaultNewPrefUserSetOldPrefEnabled) {
  // Set the old pref as-if set by the user.
  pref_service_.SetBoolean(kMemorySaverModeEnabled, true);

  EXPECT_EQ(pref_service_.GetInteger(kMemorySaverModeState),
            static_cast<int>(MemorySaverModeState::kDisabled));
  EXPECT_TRUE(
      pref_service_.FindPreference(kMemorySaverModeState)->IsDefaultValue());

  MigrateMemorySaverModePref(&pref_service_);

  EXPECT_FALSE(
      pref_service_.FindPreference(kMemorySaverModeState)->IsDefaultValue());
  // "true" in the boolean pref maps to `2` (enabled on timer)
  EXPECT_EQ(pref_service_.GetInteger(kMemorySaverModeState),
            static_cast<int>(MemorySaverModeState::kEnabled));

  // The old pref should be reset.
  EXPECT_TRUE(
      pref_service_.FindPreference(kMemorySaverModeEnabled)->IsDefaultValue());
  EXPECT_FALSE(pref_service_.GetBoolean(kMemorySaverModeEnabled));
}

TEST_F(MemorySaverModePrefMigrationTest,
       MigrateDefaultNewPrefUserSetOldPrefDisabled) {
  // Set the old pref as-if set by the user.
  pref_service_.SetBoolean(kMemorySaverModeEnabled, false);

  EXPECT_EQ(pref_service_.GetInteger(kMemorySaverModeState),
            static_cast<int>(MemorySaverModeState::kDisabled));
  EXPECT_TRUE(
      pref_service_.FindPreference(kMemorySaverModeState)->IsDefaultValue());

  MigrateMemorySaverModePref(&pref_service_);

  EXPECT_FALSE(
      pref_service_.FindPreference(kMemorySaverModeState)->IsDefaultValue());
  // "false" in the boolean pref maps to `0` (disabled)
  EXPECT_EQ(pref_service_.GetInteger(kMemorySaverModeState),
            static_cast<int>(MemorySaverModeState::kDisabled));

  // The old pref should be reset.
  EXPECT_TRUE(
      pref_service_.FindPreference(kMemorySaverModeEnabled)->IsDefaultValue());
  EXPECT_FALSE(pref_service_.GetBoolean(kMemorySaverModeEnabled));
}

TEST_F(MemorySaverModePrefMigrationTest, MigrateMultiStateModePref) {
  // Set the old pref as-if set by the user.
  pref_service_.SetInteger(kMemorySaverModeState,
                           static_cast<int>(MemorySaverModeState::kDeprecated));

  EXPECT_EQ(pref_service_.GetInteger(kMemorySaverModeState),
            static_cast<int>(MemorySaverModeState::kDeprecated));

  MigrateMultiStateMemorySaverModePref(&pref_service_);

  EXPECT_EQ(pref_service_.GetInteger(kMemorySaverModeState),
            static_cast<int>(MemorySaverModeState::kEnabled));
}

class TabDiscardingExceptionsPrefMigrationTest : public ::testing::Test {
 public:
  TabDiscardingExceptionsPrefMigrationTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    pref_service_.registry()->RegisterListPref(
        performance_manager::user_tuning::prefs::kTabDiscardingExceptions);
    pref_service_.registry()->RegisterDictionaryPref(
        performance_manager::user_tuning::prefs::
            kTabDiscardingExceptionsWithTime);
  }

  TestingPrefServiceSimple pref_service_;

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
};

TEST_F(TabDiscardingExceptionsPrefMigrationTest,
       NoChangeWhenNewPrefIsNotDefault) {
  // Set up inconsistent discard exception list and map.
  base::Value::List discard_exception_list;
  discard_exception_list.Append("a.com");
  pref_service_.SetList(kTabDiscardingExceptions,
                        std::move(discard_exception_list));
  base::Value::Dict discard_exception_map;
  discard_exception_map.Set("b.com", base::TimeToValue(base::Time::Now()));
  pref_service_.SetDict(kTabDiscardingExceptionsWithTime,
                        std::move(discard_exception_map));

  // Migrate prefs.
  MigrateTabDiscardingExceptionsPref(&pref_service_);

  // Since the new pref has already been edited, the entries of the old pref
  // aren't migrated and the old pref is reset.
  EXPECT_FALSE(pref_service_.GetDict(kTabDiscardingExceptionsWithTime)
                   .contains("a.com"));
  EXPECT_TRUE(
      pref_service_.FindPreference(kTabDiscardingExceptions)->IsDefaultValue());
}

TEST_F(TabDiscardingExceptionsPrefMigrationTest,
       NoChangeWhenNeitherPrefIsChanged) {
  // Migrate prefs when both are default values.
  MigrateTabDiscardingExceptionsPref(&pref_service_);

  // Neither pref is updated.
  EXPECT_TRUE(
      pref_service_.FindPreference(kTabDiscardingExceptions)->IsDefaultValue());
  EXPECT_TRUE(pref_service_.FindPreference(kTabDiscardingExceptionsWithTime)
                  ->IsDefaultValue());
}

TEST_F(TabDiscardingExceptionsPrefMigrationTest, MigratesExistingPrefs) {
  // Set up just the old prefs
  base::Value::List discard_exception_list;
  discard_exception_list.Append("a.com");
  discard_exception_list.Append("z.com");
  pref_service_.SetList(kTabDiscardingExceptions,
                        std::move(discard_exception_list));

  // Migrate prefs.
  MigrateTabDiscardingExceptionsPref(&pref_service_);

  // Entries from the old pref are migrated and the old pref is reset.
  ASSERT_TRUE(pref_service_.GetDict(kTabDiscardingExceptionsWithTime)
                  .contains("a.com"));
  EXPECT_EQ(
      base::ValueToTime(pref_service_.GetDict(kTabDiscardingExceptionsWithTime)
                            .Find("a.com")),
      base::Time::Now());
  ASSERT_TRUE(pref_service_.GetDict(kTabDiscardingExceptionsWithTime)
                  .contains("z.com"));
  EXPECT_EQ(
      base::ValueToTime(pref_service_.GetDict(kTabDiscardingExceptionsWithTime)
                            .Find("z.com")),
      base::Time::Now());
  EXPECT_TRUE(
      pref_service_.FindPreference(kTabDiscardingExceptions)->IsDefaultValue());
}

}  // namespace performance_manager::user_tuning::prefs
