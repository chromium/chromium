// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/user_tuning/prefs.h"

#include "components/performance_manager/public/features.h"
#include "components/prefs/testing_pref_service.h"
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
            static_cast<int>(MemorySaverModeState::kEnabled));
  EXPECT_TRUE(
      pref_service_.FindPreference(kMemorySaverModeState)->IsDefaultValue());

  MigrateMemorySaverModePref(&pref_service_);

  // Both prefs were in the default state, no migration happens
  EXPECT_TRUE(
      pref_service_.FindPreference(kMemorySaverModeState)->IsDefaultValue());
  EXPECT_EQ(pref_service_.GetInteger(kMemorySaverModeState),
            static_cast<int>(MemorySaverModeState::kEnabled));
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
            static_cast<int>(MemorySaverModeState::kEnabledOnTimer));

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

}  // namespace performance_manager::user_tuning::prefs
