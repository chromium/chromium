// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_menu_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/test_support/fake_tab_group_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace tab_groups {

class TabGroupMenuUtilsTest : public testing::Test {
 public:
  TabGroupMenuUtilsTest() = default;
  ~TabGroupMenuUtilsTest() override = default;

 protected:
  const std::u16string kGroupTitle1 = u"Group 1";
  const std::u16string kTabTitle1 = u"Tab 1";
  const GURL kTabUrl1{"https://www.google.com/"};
};

TEST_F(TabGroupMenuUtilsTest, GetMenuTextForGroup) {
  EXPECT_EQ(kGroupTitle1, TabGroupMenuUtils::GetMenuTextForGroup(
                              kGroupTitle1, /*tab_count=*/1));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(IDS_SAVED_TAB_GROUP_TABS_COUNT, 2),
            TabGroupMenuUtils::GetMenuTextForGroup(u"", /*tab_count=*/2));
}

TEST_F(TabGroupMenuUtilsTest, GetMenuTextForTab) {
  EXPECT_EQ(kTabTitle1,
            TabGroupMenuUtils::GetMenuTextForTab(kTabTitle1, kTabUrl1));
  EXPECT_EQ(base::UTF8ToUTF16(kTabUrl1.spec()),
            TabGroupMenuUtils::GetMenuTextForTab(u"", kTabUrl1));
}

TEST_F(TabGroupMenuUtilsTest, GetGroupsForDisplaySortedByCreationTime) {
  FakeTabGroupSyncService service;

  SavedTabGroup group1(u"Group 1", tab_groups::TabGroupColorId::kGrey, {},
                       std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                       std::nullopt, false, base::Time::Now());
  SavedTabGroupTab tab1(GURL("https://www.one.com/"), u"Tab 1",
                        group1.saved_guid(), 0);
  group1.AddTabLocally(tab1);
  service.AddGroup(group1);

  SavedTabGroup group2(u"Group 2", tab_groups::TabGroupColorId::kBlue, {},
                       std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                       std::nullopt, false, base::Time::Now() - base::Days(1));
  SavedTabGroupTab tab2(GURL("https://www.two.com/"), u"Tab 2",
                        group2.saved_guid(), 0);
  group2.AddTabLocally(tab2);
  service.AddGroup(group2);

  SavedTabGroup group3(u"Group 3", tab_groups::TabGroupColorId::kRed, {},
                       std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                       std::nullopt, false, base::Time::Now() + base::Days(1));
  SavedTabGroupTab tab3(GURL("https://www.three.com/"), u"Tab 3",
                        group3.saved_guid(), 0);
  group3.AddTabLocally(tab3);
  service.AddGroup(group3);

  SavedTabGroup empty_group(u"Empty Group", tab_groups::TabGroupColorId::kGreen,
                            {}, std::nullopt, std::nullopt, std::nullopt,
                            std::nullopt, std::nullopt, false,
                            base::Time::Now());
  service.AddGroup(empty_group);

  std::vector<base::Uuid> sorted_guids =
      TabGroupMenuUtils::GetGroupsForDisplaySortedByCreationTime(&service);

  ASSERT_EQ(3u, sorted_guids.size());
  EXPECT_EQ(group3.saved_guid(), sorted_guids[0]);
  EXPECT_EQ(group1.saved_guid(), sorted_guids[1]);
  EXPECT_EQ(group2.saved_guid(), sorted_guids[2]);
}

}  // namespace tab_groups
