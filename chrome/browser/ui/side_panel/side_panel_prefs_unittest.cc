// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/side_panel_prefs.h"

#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace side_panel_prefs {
namespace {

class SidePanelPrefsTest : public testing::Test {
 public:
  void SetUp() override { RegisterProfilePrefs(pref_service_.registry()); }

 protected:
  void SetOverride(SidePanelEntryId entry_id, bool is_right_aligned) {
    ScopedDictPrefUpdate update(&pref_service_,
                                prefs::kSidePanelAlignmentOverrides);
    update->Set(SidePanelEntryIdToString(entry_id), is_right_aligned);
  }

  void RemoveOverride(SidePanelEntryId entry_id) {
    ScopedDictPrefUpdate update(&pref_service_,
                                prefs::kSidePanelAlignmentOverrides);
    update->Remove(SidePanelEntryIdToString(entry_id));
  }

  sync_preferences::TestingPrefServiceSyncable pref_service_;
};

TEST_F(SidePanelPrefsTest, IsSidePanelRightAligned_UsesEntryOverride) {
  pref_service_.SetBoolean(prefs::kSidePanelHorizontalAlignment, false);
  SetOverride(SidePanelEntryId::kContextualTasks, true);
  EXPECT_TRUE(IsSidePanelRightAligned(pref_service_,
                                      SidePanelEntryId::kContextualTasks));

  pref_service_.SetBoolean(prefs::kSidePanelHorizontalAlignment, true);
  SetOverride(SidePanelEntryId::kContextualTasks, false);
  EXPECT_FALSE(IsSidePanelRightAligned(pref_service_,
                                       SidePanelEntryId::kContextualTasks));
}

TEST_F(SidePanelPrefsTest,
       IsSidePanelRightAligned_FallsBackToGlobalWithoutOverride) {
  RemoveOverride(SidePanelEntryId::kContextualTasks);

  pref_service_.SetBoolean(prefs::kSidePanelHorizontalAlignment, true);
  EXPECT_TRUE(IsSidePanelRightAligned(pref_service_,
                                      SidePanelEntryId::kContextualTasks));

  pref_service_.SetBoolean(prefs::kSidePanelHorizontalAlignment, false);
  EXPECT_FALSE(IsSidePanelRightAligned(pref_service_,
                                       SidePanelEntryId::kContextualTasks));
}

}  // namespace
}  // namespace side_panel_prefs
