// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/public/saved_tab_group.h"

#include "base/token.h"
#include "build/build_config.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace tab_groups {
namespace {

using testing::Contains;
using testing::ElementsAre;
using testing::SizeIs;
using testing::UnorderedElementsAre;

MATCHER_P(HasTabGuid, guid, "") {
  return arg.saved_tab_guid() == guid;
}

LocalTabID MakeUniqueTabID() {
  static uint32_t unique_value = 0;
  return unique_value++;
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
  base::Uuid tab_1_saved_guid = base::Uuid::GenerateRandomV4();
  base::Uuid tab_2_saved_guid = base::Uuid::GenerateRandomV4();

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

TEST(SavedTabGroupTest, RemoveTabLocallyStoresMetadata) {
  SavedTabGroup group = CreateDefaultEmptySavedTabGroup().CloneAsSharedTabGroup(
      syncer::CollaborationId("collaboration"));
  SavedTabGroupTab tab_1 = CreateDefaultSavedTabGroupTab(group.saved_guid());
  SavedTabGroupTab tab_2 = CreateDefaultSavedTabGroupTab(group.saved_guid());

  group.AddTabLocally(tab_1);
  group.AddTabLocally(tab_2);

  GaiaId removed_by("user_id");
  group.RemoveTabLocally(tab_2.saved_tab_guid(), removed_by);

  EXPECT_THAT(group.last_removed_tabs_metadata(),
              UnorderedElementsAre(testing::Key(tab_2.saved_tab_guid())));
  EXPECT_EQ(
      group.last_removed_tabs_metadata().at(tab_2.saved_tab_guid()).removed_by,
      removed_by);
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
  group.SetCollaborationId(syncer::CollaborationId("collaboration"));

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
  group.RemoveTabFromSync(tab_1_saved_guid, /*removed_by=*/GaiaId());

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

TEST(SavedTabGroupTest, RemoveSharedTabFromSyncStoresMetadata) {
  SavedTabGroup group = CreateDefaultEmptySavedTabGroup().CloneAsSharedTabGroup(
      syncer::CollaborationId("collaboration"));
  SavedTabGroupTab tab_1 = CreateDefaultSavedTabGroupTab(group.saved_guid());
  SavedTabGroupTab tab_2 = CreateDefaultSavedTabGroupTab(group.saved_guid());

  group.AddTabLocally(tab_1);
  group.AddTabLocally(tab_2);

  GaiaId removed_by("user_id");
  group.RemoveTabFromSync(tab_2.saved_tab_guid(), removed_by);

  EXPECT_THAT(group.last_removed_tabs_metadata(),
              UnorderedElementsAre(testing::Key(tab_2.saved_tab_guid())));
  EXPECT_EQ(
      group.last_removed_tabs_metadata().at(tab_2.saved_tab_guid()).removed_by,
      removed_by);
}

TEST(SavedTabGroupTest, RemoveSharedTabFromSyncShouldCleanOldEntries) {
  const GaiaId kRemovedBy("user_id");

  SavedTabGroup group = CreateDefaultEmptySavedTabGroup().CloneAsSharedTabGroup(
      syncer::CollaborationId("collaboration"));
  group.AddTabLocally(CreateDefaultSavedTabGroupTab(group.saved_guid()));

  // Add and remove tabs up to the limit.
  const size_t removed_tabs_limit =
      SavedTabGroup::GetMaxLastRemovedTabsMetadataForTesting();
  for (size_t i = 0; i < removed_tabs_limit; ++i) {
    SavedTabGroupTab tab = CreateDefaultSavedTabGroupTab(group.saved_guid());
    group.AddTabLocally(tab);
    group.RemoveTabFromSync(tab.saved_tab_guid(), kRemovedBy);
  }

  EXPECT_THAT(group.last_removed_tabs_metadata(), SizeIs(removed_tabs_limit));

  // All the new tabs should replace previous ones.
  std::vector<base::Uuid> last_removed_tabs;
  for (size_t i = 0; i < removed_tabs_limit; ++i) {
    SavedTabGroupTab tab = CreateDefaultSavedTabGroupTab(group.saved_guid());
    group.AddTabLocally(tab);
    group.RemoveTabFromSync(tab.saved_tab_guid(), kRemovedBy);
    last_removed_tabs.push_back(tab.saved_tab_guid());
  }

  // The number of tabs metadata should remain the same.
  EXPECT_THAT(group.last_removed_tabs_metadata(), SizeIs(removed_tabs_limit));
  for (const base::Uuid& removed_tab_guid : last_removed_tabs) {
    EXPECT_THAT(group.last_removed_tabs_metadata(),
                Contains(testing::Key(removed_tab_guid)));
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

TEST(SavedTabGroupTest, GetOriginatingTabGroupGuid) {
  const base::Uuid kOriginatingTabGroupGuid = base::Uuid::GenerateRandomV4();

  SavedTabGroup saved_group = CreateDefaultEmptySavedTabGroup();
  saved_group.SetOriginatingTabGroupGuid(
      kOriginatingTabGroupGuid,
      /*use_originating_tab_group_guid=*/true);

  EXPECT_EQ(saved_group.GetOriginatingTabGroupGuid(), kOriginatingTabGroupGuid);

  SavedTabGroup shared_group = saved_group.CloneAsSharedTabGroup(
      syncer::CollaborationId("collaboration"));
  EXPECT_EQ(shared_group.GetOriginatingTabGroupGuid(),
            saved_group.saved_guid());

  shared_group.SetOriginatingTabGroupGuid(
      kOriginatingTabGroupGuid,
      /*use_originating_tab_group_guid=*/false);

  // For the shared tab group, the originating tab group guid should be returned
  // only for the user who created the group.
  EXPECT_EQ(shared_group.GetOriginatingTabGroupGuid(), std::nullopt);

  // However, for sync, the originating tab group guid should be returned
  // regardless of the group owner.
  EXPECT_EQ(shared_group.GetOriginatingTabGroupGuid(/*for_sync=*/true),
            kOriginatingTabGroupGuid);
}

TEST(SavedTabGroupTest, ConvertToSharedTabGroupAndBackRetainsPosition) {
  std::optional<size_t> position = 0;
  SavedTabGroup saved_group(std::u16string(u"default_group"),
                            tab_groups::TabGroupColorId::kGrey, {}, position);
  EXPECT_EQ(position, saved_group.position());
  SavedTabGroup shared_group = saved_group.CloneAsSharedTabGroup(
      syncer::CollaborationId("collaboration"));
  EXPECT_EQ(position, shared_group.position());
  SavedTabGroup saved_group2 = shared_group.CloneAsSavedTabGroup();
  EXPECT_EQ(position, saved_group2.position());
}

TEST(SavedTabGroupTest, MergeRemoteGroupPosition) {
  std::u16string title = u"title";
  tab_groups::TabGroupColorId color = tab_groups::TabGroupColorId::kBlue;
  std::optional<size_t> position = 0;

  // Saved group should merge position.
  SavedTabGroup saved_group = CreateDefaultEmptySavedTabGroup();
  saved_group.MergeRemoteGroupMetadata(title, color, position, std::nullopt,
                                       std::nullopt, base::Time::Now());
  EXPECT_EQ(position, saved_group.position());

  // Shared group should not merge position.
  SavedTabGroup shared_group =
      CreateDefaultEmptySavedTabGroup().CloneAsSharedTabGroup(
          syncer::CollaborationId("collaboration"));
  shared_group.MergeRemoteGroupMetadata(title, color, position, std::nullopt,
                                        std::nullopt, base::Time::Now());
  EXPECT_EQ(std::nullopt, shared_group.position());
}

}  // namespace tab_groups
