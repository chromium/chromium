// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/user_tuning/prefs.h"

#include "components/performance_manager/public/features.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::user_tuning::prefs {

class HighEfficiencyModePrefMigrationTest : public ::testing::Test {
 public:
  void SetUp() override { RegisterLocalStatePrefs(pref_service_.registry()); }

  TestingPrefServiceSimple pref_service_;
};

TEST_F(HighEfficiencyModePrefMigrationTest, NoChangeToUserSetNewPref) {
  // The old pref is set by the user, but so is the new pref so no migration
  // should happen.
  pref_service_.SetBoolean(kHighEfficiencyModeEnabled, true);
  pref_service_.SetInteger(
      kHighEfficiencyModeState,
      static_cast<int>(HighEfficiencyModeState::kDisabled));

  MigrateHighEfficiencyModePref(&pref_service_);

  EXPECT_EQ(pref_service_.GetInteger(kHighEfficiencyModeState),
            static_cast<int>(HighEfficiencyModeState::kDisabled));
  // The old pref should be reset.
  EXPECT_TRUE(pref_service_.FindPreference(kHighEfficiencyModeEnabled)
                  ->IsDefaultValue());
  EXPECT_FALSE(pref_service_.GetBoolean(kHighEfficiencyModeEnabled));
}

TEST_F(HighEfficiencyModePrefMigrationTest, BothPrefsDefaultNoMigration) {
  // Simulate that the default enum state value is not "disabled"
  pref_service_.SetDefaultPrefValue(kHighEfficiencyModeState, base::Value(1));

  EXPECT_EQ(pref_service_.GetInteger(kHighEfficiencyModeState),
            static_cast<int>(HighEfficiencyModeState::kEnabled));
  EXPECT_TRUE(
      pref_service_.FindPreference(kHighEfficiencyModeState)->IsDefaultValue());

  MigrateHighEfficiencyModePref(&pref_service_);

  // Both prefs were in the default state, no migration happens
  EXPECT_TRUE(
      pref_service_.FindPreference(kHighEfficiencyModeState)->IsDefaultValue());
  EXPECT_EQ(pref_service_.GetInteger(kHighEfficiencyModeState),
            static_cast<int>(HighEfficiencyModeState::kEnabled));
}

TEST_F(HighEfficiencyModePrefMigrationTest,
       MigrateDefaultNewPrefUserSetOldPref) {
  // Set the old pref as-if set by the user.
  pref_service_.SetBoolean(kHighEfficiencyModeEnabled, true);

  EXPECT_EQ(pref_service_.GetInteger(kHighEfficiencyModeState),
            static_cast<int>(HighEfficiencyModeState::kDisabled));
  EXPECT_TRUE(
      pref_service_.FindPreference(kHighEfficiencyModeState)->IsDefaultValue());

  MigrateHighEfficiencyModePref(&pref_service_);

  EXPECT_FALSE(
      pref_service_.FindPreference(kHighEfficiencyModeState)->IsDefaultValue());
  // "true" in the boolean pref maps to `2` (enabled on timer)
  EXPECT_EQ(pref_service_.GetInteger(kHighEfficiencyModeState),
            static_cast<int>(HighEfficiencyModeState::kEnabledOnTimer));

  // The old pref should be reset.
  EXPECT_TRUE(pref_service_.FindPreference(kHighEfficiencyModeEnabled)
                  ->IsDefaultValue());
  EXPECT_FALSE(pref_service_.GetBoolean(kHighEfficiencyModeEnabled));
}

}  // namespace performance_manager::user_tuning::prefs
