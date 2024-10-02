// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/public/saved_tab_group.h"

#include "base/token.h"
#include "build/build_config.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace tab_groups {
namespace {

using testing::ElementsAre;

MATCHER_P(HasTabGuid, guid, "") {
  return arg.saved_tab_guid() == guid;
}

base::Uuid MakeUniqueGUID() {
  static uint64_t unique_value = 0;
  unique_value++;
  uint64_t kBytes[] = {0, unique_value};
  return base::Uuid::FormatRandomDataAsV4ForTesting(
      as_bytes(base::make_span(kBytes)));
}

LocalTabID MakeUniqueTabID() {
  static uint64_t unique_value = 0;
  unique_value++;
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return unique_value;
#else
  return base::Token(0, unique_value);
#endif
}

SavedTabGroup CreateDefaultEmptySavedTabGroup() {
  return SavedTabGroup(std::u16string(u"default_group"),
                       tab_groups::TabGroupColorId::kGrey, {}, std::nullopt);
}

SavedTabGroupTab CreateDefaultSavedTabGroupTab(const base::Uuid& group_guid) {
  return SavedTabGroupTab(GURL("www.google.com"), u"Default Title", group_guid,
                          /*position=*/std::nullopt);
}

void AddTabToEndOfGroup(SavedTabGroup& group,
                        std::optional<base::Uuid> saved_guid = std::nullopt,
                        std::optional<LocalTabID> local_tab_id = std::nullopt) {
  group.AddTabLocally(SavedTabGroupTab(
      GURL(url::kAboutBlankURL), std::u16string(u"default_title"),
      group.saved_guid(), /*position=*/group.saved_tabs().size(), saved_guid,
      local_tab_id));
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

TEST(SavedTabGroupTest, GetTabById) {
  LocalTabID tab_1_local_id = MakeUniqueTabID();
  LocalTabID tab_2_local_id = MakeUniqueTabID();

  // create a group with a couple tabs
  SavedTabGroup group = CreateDefaultEmptySavedTabGroup();
  AddTabToEndOfGroup(group, std::nullopt, tab_1_local_id);
  AddTabToEndOfGroup(group, std::nullopt, tab_2_local_id);
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
  group.AddTabLocally(std::move(tab_1));
  group.AddTabLocally(std::move(tab_2));
  ASSERT_EQ(2u, group.saved_tabs().size());

  // Locally added groups will be added into their preferred positions if
  // possible. If not, they will be added as close to the preferred position as
  // possible, and have their position updated to reflect this.
  SavedTabGroupTab* first_tab = group.GetTab(tab_1_saved_guid);
  EXPECT_EQ(&group.saved_tabs()[0], first_tab);
  EXPECT_EQ(first_tab->position(), 0u);

  // Expect tab_2 to be at the front of the group.
  SavedTabGroupTab* second_tab = group.GetTab(tab_2_saved_guid);
  EXPECT_EQ(&group.saved_tabs()[1], second_tab);
  EXPECT_EQ(second_tab->position(), 1u);
}

TEST(SavedTabGroupTest, RemoveTabLocallyReordersPositions) {
  // Create a group and 2 tabs
  SavedTabGroup group = CreateDefaultEmptySavedTabGroup();
  SavedTabGroupTab tab_1 = CreateDefaultSavedTabGroupTab(group.saved_guid());
  SavedTabGroupTab tab_2 = CreateDefaultSavedTabGroupTab(group.saved_guid());

  base::Uuid tab_1_saved_guid = tab_1.saved_tab_guid();
  base::Uuid tab_2_saved_guid = tab_2.saved_tab_guid();

  // Add both tabs to the group.
  group.AddTabLocally(std::move(tab_1));
  group.AddTabLocally(std::move(tab_2));
  ASSERT_EQ(2u, group.saved_tabs().size());

  // Verify tab_2 has a position of 1.
  {
    SavedTabGroupTab* second_tab = group.GetTab(tab_2_saved_guid);
    EXPECT_EQ(&group.saved_tabs()[1], second_tab);
    EXPECT_EQ(second_tab->position(), 1u);
  }

  // Remove tab_1 from the group.
  group.RemoveTabLocally(tab_1_saved_guid);

  // Verify only tab_2 is in the group.
  EXPECT_EQ(group.saved_tabs().size(), 1u);
  EXPECT_FALSE(group.ContainsTab(tab_1_saved_guid));
  EXPECT_TRUE(group.ContainsTab(tab_2_saved_guid));

  // Verify tab_2 has a position of 0 now.
  {
    // Expect tab two to be at the front of the group.
    SavedTabGroupTab* second_tab = group.GetTab(tab_2_saved_guid);
    EXPECT_EQ(&group.saved_tabs()[0], second_tab);
    EXPECT_EQ(second_tab->position(), 0u);
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

  group.AddTabFromSync(std::move(tab_1));
  group.AddTabFromSync(std::move(tab_2));
  ASSERT_EQ(2u, group.saved_tabs().size());

  // Expect tab one to be at the end of the group.
  SavedTabGroupTab* first_tab = group.GetTab(tab_1_saved_guid);
  EXPECT_EQ(&group.saved_tabs()[1], first_tab);
  EXPECT_EQ(first_tab->position(), 1u);

  // Expect tab two to be at the front of the group.
  SavedTabGroupTab* second_tab = group.GetTab(tab_2_saved_guid);
  EXPECT_EQ(&group.saved_tabs()[0], second_tab);
  EXPECT_EQ(second_tab->position(), 0u);
}

TEST(SavedTabGroupTest, AddTabFromSyncUsesPositionAsIndexForSharedGroup) {
  // Create a shared group and 2 tabs.
  SavedTabGroup group = CreateDefaultEmptySavedTabGroup();
  group.SetCollaborationId("collaboration");

  SavedTabGroupTab tab_0 = CreateDefaultSavedTabGroupTab(group.saved_guid());
  tab_0.SetPosition(0);
  group.AddTabFromSync(tab_0);

  // Insert a new tab to the end.
  SavedTabGroupTab tab_1 = CreateDefaultSavedTabGroupTab(group.saved_guid());
  tab_1.SetPosition(1);
  group.AddTabFromSync(tab_1);

  EXPECT_THAT(group.saved_tabs(),
              ElementsAre(HasTabGuid(tab_0.saved_tab_guid()),
                          HasTabGuid(tab_1.saved_tab_guid())));

  // Insert a new tab to the beginning (before the given position).
  SavedTabGroupTab tab_before_0 =
      CreateDefaultSavedTabGroupTab(group.saved_guid());
  tab_before_0.SetPosition(0);
  group.AddTabFromSync(tab_before_0);
  EXPECT_THAT(group.saved_tabs(),
              ElementsAre(HasTabGuid(tab_before_0.saved_tab_guid()),
                          HasTabGuid(tab_0.saved_tab_guid()),
                          HasTabGuid(tab_1.saved_tab_guid())));
}

TEST(SavedTabGroupTest, RemoveTabFromSyncMaintainsPositions) {
  // Create a group and 2 tabs
  SavedTabGroup group = CreateDefaultEmptySavedTabGroup();
  SavedTabGroupTab tab_1 = CreateDefaultSavedTabGroupTab(group.saved_guid());
  SavedTabGroupTab tab_2 = CreateDefaultSavedTabGroupTab(group.saved_guid());

  base::Uuid tab_1_saved_guid = tab_1.saved_tab_guid();
  base::Uuid tab_2_saved_guid = tab_2.saved_tab_guid();

  // Add both tabs to the group.
  group.AddTabLocally(std::move(tab_1));
  group.AddTabLocally(std::move(tab_2));
  ASSERT_EQ(2u, group.saved_tabs().size());

  // Verify tab_2 has a position of 1.
  {
    SavedTabGroupTab* second_tab = group.GetTab(tab_2_saved_guid);
    EXPECT_EQ(&group.saved_tabs()[1], second_tab);
    EXPECT_EQ(second_tab->position(), 1u);
  }

  // Remove tab_1 from the group.
  group.RemoveTabFromSync(tab_1_saved_guid);

  // Verify only tab_2 is in the group.
  EXPECT_EQ(group.saved_tabs().size(), 1u);
  EXPECT_FALSE(group.ContainsTab(tab_1_saved_guid));
  EXPECT_TRUE(group.ContainsTab(tab_2_saved_guid));

  // Verify tab_2 keeps its position of 1.
  {
    // Expect tab two to be at the front of the group.
    SavedTabGroupTab* second_tab = group.GetTab(tab_2_saved_guid);
    EXPECT_EQ(&group.saved_tabs()[0], second_tab);
    EXPECT_EQ(second_tab->position(), 1u);
  }
}

TEST(SavedTabGroupTest, PinAndUnpin) {
  SavedTabGroup group = CreateDefaultEmptySavedTabGroup();
  // Saved Tab Group should have position after pin.
  group.SetPinned(true);
  EXPECT_TRUE(group.is_pinned());
  EXPECT_TRUE(group.position().has_value());
  // Save Tab Group should not have position after unpin.
  group.SetPinned(false);
  EXPECT_FALSE(group.is_pinned());
  EXPECT_FALSE(group.position().has_value());
}

// Test updating the cache guid.
TEST(SavedTabGroupTest, UpdateCreatorCacheGuid) {
  std::string cache_guid_1 = "new_guid_1";
  std::string cache_guid_2 = "new_guid_2";
  SavedTabGroup group = CreateDefaultEmptySavedTabGroup();

  ASSERT_EQ(group.creator_cache_guid(), std::nullopt);
  group.SetCreatorCacheGuid(cache_guid_1);
  EXPECT_EQ(group.creator_cache_guid(), cache_guid_1);

  group.SetCreatorCacheGuid(cache_guid_2);
  EXPECT_EQ(group.creator_cache_guid(), cache_guid_2);
}

}  // namespace tab_groups
