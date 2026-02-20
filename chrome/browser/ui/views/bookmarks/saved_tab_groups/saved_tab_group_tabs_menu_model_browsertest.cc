// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_tabs_menu_model.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/grit/generated_resources.h"  // For IDS_OPEN_GROUP_IN_BROWSER_MENU
#include "chrome/test/base/in_process_browser_test.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"  // For GetStringUTF16

namespace tab_groups {

class SavedTabGroupTabsMenuModelBrowserTest : public InProcessBrowserTest {
 public:
  SavedTabGroupTabsMenuModelBrowserTest() {
    feature_list_.InitAndDisableFeature(tab_groups::kProjectsPanel);
  }

  int GetNextCommandId() { return next_command_id_++; }

  bool IsOpenGroupItemEnabled(STGTabsMenuModel* model) {
    std::u16string target_label =
        l10n_util::GetStringUTF16(IDS_OPEN_GROUP_IN_BROWSER_MENU);

    size_t item_count = model->GetItemCount();
    for (size_t i = 0; i < item_count; ++i) {
      if (model->GetLabelAt(i) == target_label) {
        return model->IsEnabledAt(i);
      }
    }
    return false;
  }

  bool IsPinItemPresent(STGTabsMenuModel* model) {
    std::u16string pin_label =
        l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_PIN_GROUP);
    std::u16string unpin_label =
        l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_UNPIN_GROUP);

    size_t item_count = model->GetItemCount();
    for (size_t i = 0; i < item_count; ++i) {
      if (model->GetLabelAt(i) == pin_label ||
          model->GetLabelAt(i) == unpin_label) {
        return true;
      }
    }
    return false;
  }

 protected:
  TabGroupSyncService* GetSyncService() {
    return TabGroupSyncServiceFactory::GetForProfile(browser()->profile());
  }

  base::test::ScopedFeatureList feature_list_;

 private:
  int next_command_id_ = 1;
};

// TEST 1: Verify "Open group" is ENABLED when the group is closed.
IN_PROC_BROWSER_TEST_F(SavedTabGroupTabsMenuModelBrowserTest,
                       OpenGroupEnabledWhenClosed) {
  SavedTabGroup group(u"Test Group", tab_groups::TabGroupColorId::kGrey, {},
                      std::nullopt);
  GetSyncService()->AddGroup(group);

  STGTabsMenuModel model(
      browser(), TabGroupMenuContext::SAVED_TAB_GROUP_BUTTON_CONTEXT_MENU);
  model.Build(group,
              base::BindRepeating(
                  &SavedTabGroupTabsMenuModelBrowserTest::GetNextCommandId,
                  base::Unretained(this)));

  EXPECT_TRUE(IsOpenGroupItemEnabled(&model));
}

// TEST 2: Verify "Open group" is DISABLED when the group is open and expanded.
IN_PROC_BROWSER_TEST_F(SavedTabGroupTabsMenuModelBrowserTest,
                       OpenGroupDisabledWhenOpenAndExpanded) {
  ASSERT_TRUE(AddTabAtIndex(0, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));
  TabGroupId local_id = browser()->tab_strip_model()->AddToNewGroup({0});

  browser()->tab_strip_model()->ActivateTabAt(0);
  SavedTabGroup group(u"Test Group", tab_groups::TabGroupColorId::kGrey, {},
                      std::nullopt);
  group.SetLocalGroupId(local_id);
  GetSyncService()->AddGroup(group);

  STGTabsMenuModel model(
      browser(), TabGroupMenuContext::SAVED_TAB_GROUP_BUTTON_CONTEXT_MENU);
  model.Build(group,
              base::BindRepeating(
                  &SavedTabGroupTabsMenuModelBrowserTest::GetNextCommandId,
                  base::Unretained(this)));

  EXPECT_FALSE(IsOpenGroupItemEnabled(&model));
}

IN_PROC_BROWSER_TEST_F(SavedTabGroupTabsMenuModelBrowserTest,
                       PinUnpinOptionVisibility) {
  SavedTabGroup group(u"Test Group", tab_groups::TabGroupColorId::kGrey, {},
                      std::nullopt);
  GetSyncService()->AddGroup(group);

  // Pin/unpin should be present when the projects panel is disabled.
  STGTabsMenuModel model(
      browser(), TabGroupMenuContext::SAVED_TAB_GROUP_BUTTON_CONTEXT_MENU);
  model.Build(group,
              base::BindRepeating(
                  &SavedTabGroupTabsMenuModelBrowserTest::GetNextCommandId,
                  base::Unretained(this)));

  EXPECT_TRUE(IsPinItemPresent(&model));
}

class SavedTabGroupTabsMenuModelWithProjectsPanelEnabledBrowserTest
    : public SavedTabGroupTabsMenuModelBrowserTest {
 public:
  SavedTabGroupTabsMenuModelWithProjectsPanelEnabledBrowserTest() {
    feature_list_.Reset();
    feature_list_.InitAndEnableFeature(tab_groups::kProjectsPanel);
  }
};

IN_PROC_BROWSER_TEST_F(
    SavedTabGroupTabsMenuModelWithProjectsPanelEnabledBrowserTest,
    PinUnpinOptionVisibility) {
  SavedTabGroup group(u"Test Group", tab_groups::TabGroupColorId::kGrey, {},
                      std::nullopt);
  GetSyncService()->AddGroup(group);

  // Pin/unpin should be hidden when the projects panel is enabled.
  STGTabsMenuModel model(
      browser(), TabGroupMenuContext::SAVED_TAB_GROUP_BUTTON_CONTEXT_MENU);
  model.Build(group,
              base::BindRepeating(
                  &SavedTabGroupTabsMenuModelBrowserTest::GetNextCommandId,
                  base::Unretained(this)));

  EXPECT_FALSE(IsPinItemPresent(&model));
}

}  // namespace tab_groups
