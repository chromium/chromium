// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/saved_tab_group.h"
#include "base/token.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {
base::Uuid MakeUniqueGUID() {
  static uint64_t unique_value = 0;
  unique_value++;
  uint64_t kBytes[] = {0, unique_value};
  return base::Uuid::FormatRandomDataAsV4ForTesting(
      as_bytes(base::make_span(kBytes)));
}

base::Token MakeUniqueToken() {
  static uint64_t unique_value = 0;
  unique_value++;
  return base::Token(0, unique_value);
}

SavedTabGroup CreateDefaultEmptySavedTabGroup() {
  return SavedTabGroup(std::u16string(u"default_group"),
                       tab_groups::TabGroupColorId::kGrey, {});
}

SavedTabGroupTab CreateDefaultSavedTabGroupTab(const base::Uuid& group_guid) {
  return SavedTabGroupTab(GURL("www.google.com"), u"Default Title", group_guid);
}

void AddTabToEndOfGroup(
    SavedTabGroup& group,
    absl::optional<base::Uuid> saved_guid = absl::nullopt,
    absl::optional<base::Token> local_tab_id = absl::nullopt) {
  group.AddTab(SavedTabGroupTab(
      GURL(url::kAboutBlankURL), std::u16string(u"default_title"),
      group.saved_guid(), &group, saved_guid, local_tab_id));
}
}  // namespace

TEST(SavedTabGroupTest, GetTabByGUID) {
  base::Uuid tab_1_saved_guid = MakeUniqueGUID();
  base::Uuid tab_2_saved_guid = MakeUniqueGUID();

  // create a group with a couple tabs
  SavedTabGroup group = CreateDefaultEmptySavedTabGroup();
  AddTabToEndOfGroup(group, tab_1_saved_guid);
  AddTabToEndOfGroup(group, tab_2_saved_guid);
  ASSERT_EQ(2u, group.saved_tabs().size());

  SavedTabGroupTab* tab_1 = group.GetTab(tab_1_saved_guid);
  EXPECT_EQ(&group.saved_tabs()[0], tab_1);

  SavedTabGroupTab* tab_2 = group.GetTab(tab_2_saved_guid);
  EXPECT_EQ(&group.saved_tabs()[1], tab_2);
}

TEST(SavedTabGroupTest, GetTabByToken) {
  base::Token tab_1_local_id = MakeUniqueToken();
  base::Token tab_2_local_id = MakeUniqueToken();

  // create a group with a couple tabs
  SavedTabGroup group = CreateDefaultEmptySavedTabGroup();
  AddTabToEndOfGroup(group, absl::nullopt, tab_1_local_id);
  AddTabToEndOfGroup(group, absl::nullopt, tab_2_local_id);
  ASSERT_EQ(2u, group.saved_tabs().size());

  SavedTabGroupTab* tab_1 = group.GetTab(tab_1_local_id);
  EXPECT_EQ(&group.saved_tabs()[0], tab_1);

  SavedTabGroupTab* tab_2 = group.GetTab(tab_2_local_id);
  EXPECT_EQ(&group.saved_tabs()[1], tab_2);
}

TEST(SavedTabGroupTest, AddTabLocallyDisrespectsPositions) {
  // Create a group and 2 tabs
  SavedTabGroup group = CreateDefaultEmptySavedTabGroup();
  SavedTabGroupTab tab_1 = CreateDefaultSavedTabGroupTab(group.saved_guid());
  SavedTabGroupTab tab_2 = CreateDefaultSavedTabGroupTab(group.saved_guid());

  base::Uuid tab_1_saved_guid = tab_1.saved_tab_guid();
  base::Uuid tab_2_saved_guid = tab_2.saved_tab_guid();

  // Set the positions on the tabs and expect the group to ignore them.
  tab_1.SetPosition(1);
  tab_2.SetPosition(0);

  // Add both tabs to the group.
  group.AddTab(std::move(tab_1), /*update_tab_positions=*/true);
  group.AddTab(std::move(tab_2), /*update_tab_positions=*/true);
  ASSERT_EQ(2u, group.saved_tabs().size());

  // Locally added groups will be added into their preferred positions if
  // possible. If not, they will be added as close to the preferred position as
  // possible, and have their position updated to reflect this.
  SavedTabGroupTab* first_tab = group.GetTab(tab_1_saved_guid);
  EXPECT_EQ(&group.saved_tabs()[0], first_tab);
  EXPECT_EQ(first_tab->position(), 0);

  // Expect tab_2 to be at the front of the group.
  SavedTabGroupTab* second_tab = group.GetTab(tab_2_saved_guid);
  EXPECT_EQ(&group.saved_tabs()[1], second_tab);
  EXPECT_EQ(second_tab->position(), 1);
}

TEST(SavedTabGroupTest, RemoveTabLocallyReordersPositions) {
  // Create a group and 2 tabs
  SavedTabGroup group = CreateDefaultEmptySavedTabGroup();
  SavedTabGroupTab tab_1 = CreateDefaultSavedTabGroupTab(group.saved_guid());
  SavedTabGroupTab tab_2 = CreateDefaultSavedTabGroupTab(group.saved_guid());

  base::Uuid tab_1_saved_guid = tab_1.saved_tab_guid();
  base::Uuid tab_2_saved_guid = tab_2.saved_tab_guid();

  // Add both tabs to the group.
  group.AddTab(std::move(tab_1));
  group.AddTab(std::move(tab_2));
  ASSERT_EQ(2u, group.saved_tabs().size());

  // Verify tab_2 has a position of 1.
  {
    SavedTabGroupTab* second_tab = group.GetTab(tab_2_saved_guid);
    EXPECT_EQ(&group.saved_tabs()[1], second_tab);
    EXPECT_EQ(second_tab->position(), 1);
  }

  // Remove tab_1 from the group.
  group.RemoveTab(tab_1_saved_guid, /*update_tab_positions=*/true);

  // Verify only tab_2 is in the group.
  EXPECT_EQ(group.saved_tabs().size(), 1u);
  EXPECT_FALSE(group.ContainsTab(tab_1_saved_guid));
  EXPECT_TRUE(group.ContainsTab(tab_2_saved_guid));

  // Verify tab_2 has a position of 0 now.
  {
    // Expect tab two to be at the front of the group.
    SavedTabGroupTab* second_tab = group.GetTab(tab_2_saved_guid);
    EXPECT_EQ(&group.saved_tabs()[0], second_tab);
    EXPECT_EQ(second_tab->position(), 0);
  }
}

TEST(SavedTabGroupTest, AddTabFromSyncRespectsPositions) {
  // Create a group and 2 tabs
  SavedTabGroup group = CreateDefaultEmptySavedTabGroup();
  SavedTabGroupTab tab_1 = CreateDefaultSavedTabGroupTab(group.saved_guid());
  SavedTabGroupTab tab_2 = CreateDefaultSavedTabGroupTab(group.saved_guid());

  base::Uuid tab_1_saved_guid = tab_1.saved_tab_guid();
  base::Uuid tab_2_saved_guid = tab_2.saved_tab_guid();

  // Set the positions on the tabs and expect the group to respect them.
  tab_1.SetPosition(1);
  tab_2.SetPosition(0);

  group.AddTab(std::move(tab_1), /*update_tab_positions=*/false);
  group.AddTab(std::move(tab_2), /*update_tab_positions=*/false);
  ASSERT_EQ(2u, group.saved_tabs().size());

  // Expect tab one to be at the end of the group.
  SavedTabGroupTab* first_tab = group.GetTab(tab_1_saved_guid);
  EXPECT_EQ(&group.saved_tabs()[1], first_tab);
  EXPECT_EQ(first_tab->position(), 1);

  // Expect tab two to be at the front of the group.
  SavedTabGroupTab* second_tab = group.GetTab(tab_2_saved_guid);
  EXPECT_EQ(&group.saved_tabs()[0], second_tab);
  EXPECT_EQ(second_tab->position(), 0);
}

TEST(SavedTabGroupTest, RemoveTabFromSyncMaintainsPositions) {
  // Create a group and 2 tabs
  SavedTabGroup group = CreateDefaultEmptySavedTabGroup();
  SavedTabGroupTab tab_1 = CreateDefaultSavedTabGroupTab(group.saved_guid());
  SavedTabGroupTab tab_2 = CreateDefaultSavedTabGroupTab(group.saved_guid());

  base::Uuid tab_1_saved_guid = tab_1.saved_tab_guid();
  base::Uuid tab_2_saved_guid = tab_2.saved_tab_guid();

  // Add both tabs to the group.
  group.AddTab(std::move(tab_1));
  group.AddTab(std::move(tab_2));
  ASSERT_EQ(2u, group.saved_tabs().size());

  // Verify tab_2 has a position of 1.
  {
    SavedTabGroupTab* second_tab = group.GetTab(tab_2_saved_guid);
    EXPECT_EQ(&group.saved_tabs()[1], second_tab);
    EXPECT_EQ(second_tab->position(), 1);
  }

  // Remove tab_1 from the group.
  group.RemoveTab(tab_1_saved_guid, /*update_tab_positions=*/false);

  // Verify only tab_2 is in the group.
  EXPECT_EQ(group.saved_tabs().size(), 1u);
  EXPECT_FALSE(group.ContainsTab(tab_1_saved_guid));
  EXPECT_TRUE(group.ContainsTab(tab_2_saved_guid));

  // Verify tab_2 keeps its position of 1.
  {
    // Expect tab two to be at the front of the group.
    SavedTabGroupTab* second_tab = group.GetTab(tab_2_saved_guid);
    EXPECT_EQ(&group.saved_tabs()[0], second_tab);
    EXPECT_EQ(second_tab->position(), 1);
  }
}
