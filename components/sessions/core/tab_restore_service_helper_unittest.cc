// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/tab_restore_service_helper.h"

#include <optional>

#include "testing/gtest/include/gtest/gtest.h"

namespace sessions {

std::unique_ptr<tab_restore::Group> CreateTabRestoreGroup(
    tab_groups::TabGroupId group_id,
    const std::optional<base::Uuid>& saved_group_id) {
  auto group = std::make_unique<tab_restore::Group>();
  group->group_id = group_id;

  group->saved_group_id = saved_group_id;
  return group;
}

std::unique_ptr<tab_restore::Tab> CreateTabWithGroupInfo(
    tab_groups::TabGroupId group_id,
    const std::optional<base::Uuid>& saved_group_id) {
  auto tab = std::make_unique<tab_restore::Tab>();
  tab->group = group_id;
  tab->saved_group_id = saved_group_id;
  return tab;
}

class TabRestoreServiceHelperTest : public testing::Test {};

TEST_F(TabRestoreServiceHelperTest, CreateLocalSavedGroupIDMapping) {
  std::map<tab_groups::TabGroupId, std::unique_ptr<tab_restore::Group>> groups;

  tab_groups::TabGroupId local_group_id_1 =
      tab_groups::TabGroupId::GenerateNew();
  tab_groups::TabGroupId local_group_id_2 =
      tab_groups::TabGroupId::GenerateNew();

  base::Uuid saved_uuid_1 = base::Uuid::GenerateRandomV4();

  groups[local_group_id_1] =
      CreateTabRestoreGroup(local_group_id_1, saved_uuid_1);
  groups[local_group_id_2] =
      CreateTabRestoreGroup(local_group_id_2, std::nullopt);

  std::map<tab_groups::TabGroupId, base::Uuid> result =
      TabRestoreServiceHelper::CreateLocalSavedGroupIDMapping(groups);

  // Verify that only groups with saved_group_id appear in the mapping.
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[local_group_id_1], saved_uuid_1);
}

TEST_F(TabRestoreServiceHelperTest, UpdateSavedGroupIDsForTabEntries) {
  tab_groups::TabGroupId local_group_id_1 =
      tab_groups::TabGroupId::GenerateNew();
  tab_groups::TabGroupId local_group_id_2 =
      tab_groups::TabGroupId::GenerateNew();
  tab_groups::TabGroupId local_group_id_3 =
      tab_groups::TabGroupId::GenerateNew();

  base::Uuid saved_guid_1 = base::Uuid::GenerateRandomV4();
  base::Uuid saved_guid_2 = base::Uuid::GenerateRandomV4();

  std::vector<std::unique_ptr<tab_restore::Tab>> tabs;
  tabs.push_back(CreateTabWithGroupInfo(local_group_id_1, std::nullopt));
  tabs.push_back(CreateTabWithGroupInfo(local_group_id_2, std::nullopt));
  tabs.push_back(CreateTabWithGroupInfo(local_group_id_3, std::nullopt));
  tabs.push_back(std::make_unique<tab_restore::Tab>());

  std::map<tab_groups::TabGroupId, std::unique_ptr<tab_restore::Group>> groups;
  groups[local_group_id_1] =
      CreateTabRestoreGroup(local_group_id_1, saved_guid_1);
  groups[local_group_id_2] =
      CreateTabRestoreGroup(local_group_id_2, saved_guid_2);
  groups[local_group_id_3] =
      CreateTabRestoreGroup(local_group_id_3, std::nullopt);

  TabRestoreServiceHelper::UpdateSavedGroupIDsForTabEntries(
      tabs, TabRestoreServiceHelper::CreateLocalSavedGroupIDMapping(groups));

  // Verify that only tabs with a group in the mapping are updated.
  ASSERT_TRUE(tabs[0]->saved_group_id.has_value());
  EXPECT_EQ(tabs[0]->saved_group_id.value(), saved_guid_1);

  ASSERT_TRUE(tabs[1]->saved_group_id.has_value());
  EXPECT_EQ(tabs[1]->saved_group_id.value(), saved_guid_2);

  // 3rd tab's group isn't in the mapping, so Tab 2 remains unchanged.
  EXPECT_FALSE(tabs[2]->saved_group_id.has_value());

  // The last tab has no group, so it should also remain nullopt.
  EXPECT_FALSE(tabs[3]->saved_group_id.has_value());
}

}  // namespace sessions
