// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_prefs.h"

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/toolbar/toolbar_pref_names.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"

namespace tabs {

class TabStripPrefsTest : public testing::Test {
 protected:
  void SetUp() override {
    RegisterProfilePrefs(prefs_.registry());
    // Register pinned actions pref which is used by migration.
    prefs_.registry()->RegisterListPref(prefs::kPinnedActions);

    // Initialize ActionIdMap with kActionTabSearch if not already there.
    if (!actions::ActionIdMap::ActionIdToString(kActionTabSearch).has_value()) {
      actions::ActionIdMap::AddActionIdToStringMappings(
          {{kActionTabSearch, "kActionTabSearch"}});
    }
  }

  void TearDown() override {
    glic::GlicEnabling::SetBypassEnablementChecksForTesting(false);
  }

  sync_preferences::TestingPrefServiceSyncable prefs_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(TabStripPrefsTest, MigrateTabSearchPref_ExperimentEnabled_Pinned) {
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);

  // Simulate tab search being pinned in the toolbar.
  base::ListValue pinned_actions;
  pinned_actions.Append("kActionTabSearch");
  prefs_.SetList(prefs::kPinnedActions, std::move(pinned_actions));

  // Default should be true, but we want to see it migrated.
  prefs_.SetBoolean(prefs::kTabSearchPinnedToTabstrip, false);

  MigrateTabSearchPref(&prefs_);

  EXPECT_TRUE(prefs_.GetBoolean(prefs::kTabSearchPinnedToTabstrip));
  EXPECT_TRUE(
      prefs_.GetBoolean(prefs::kTabSearchPinnedToTabstripMigrationComplete2));
}

TEST_F(TabStripPrefsTest, MigrateTabSearchPref_ExperimentEnabled_Unpinned) {
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);

  // Simulate tab search NOT being pinned in the toolbar.
  prefs_.SetList(prefs::kPinnedActions, base::ListValue());

  // Default is true.
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kTabSearchPinnedToTabstrip));

  MigrateTabSearchPref(&prefs_);

  // Should be migrated to false because it's not pinned in toolbar.
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kTabSearchPinnedToTabstrip));
  EXPECT_TRUE(
      prefs_.GetBoolean(prefs::kTabSearchPinnedToTabstripMigrationComplete2));
}

TEST_F(TabStripPrefsTest, MigrateTabSearchPref_ExperimentDisabled) {
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(false);
  feature_list_.InitAndDisableFeature(features::kGlic);

  // Simulate tab search NOT being pinned in the toolbar (it shouldn't be
  // anyway).
  prefs_.SetList(prefs::kPinnedActions, base::ListValue());

  // Default is true.
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kTabSearchPinnedToTabstrip));

  MigrateTabSearchPref(&prefs_);

  // Should stay true if the experiment is disabled.
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kTabSearchPinnedToTabstrip));
  EXPECT_TRUE(
      prefs_.GetBoolean(prefs::kTabSearchPinnedToTabstripMigrationComplete2));
}

TEST_F(TabStripPrefsTest, MigrateTabSearchPref_RecoveryFromBrokenMigration) {
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(false);
  feature_list_.InitAndDisableFeature(features::kGlic);

  // Simulate user who was hit by the broken migration.
  prefs_.SetBoolean(prefs::kTabSearchPinnedToTabstripMigrationComplete, true);
  prefs_.SetBoolean(prefs::kTabSearchPinnedToTabstrip, false);  // Broken state

  MigrateTabSearchPref(&prefs_);

  // Should be recovered to true.
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kTabSearchPinnedToTabstrip));
  EXPECT_TRUE(
      prefs_.GetBoolean(prefs::kTabSearchPinnedToTabstripMigrationComplete2));
}

}  // namespace tabs
