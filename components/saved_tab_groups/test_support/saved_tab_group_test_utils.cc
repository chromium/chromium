// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"

#include "base/rand_util.h"
#include "base/token.h"
#include "build/build_config.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info.h"
#include "components/tab_groups/tab_group_color.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace tab_groups::test {

LocalTabGroupID GenerateRandomTabGroupID() {
#if BUILDFLAG(IS_ANDROID)
  return base::Token::CreateRandom();
#else
  return tab_groups::TabGroupId::GenerateNew();
#endif
}

LocalTabID GenerateRandomTabID() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return base::RandInt(0, 1000);
#else
  return base::Token::CreateRandom();
#endif
}

void CompareSavedTabGroupTabs(const std::vector<SavedTabGroupTab>& v1,
                              const std::vector<SavedTabGroupTab>& v2) {
  ASSERT_EQ(v1.size(), v2.size());
  for (size_t i = 0; i < v1.size(); i++) {
    SavedTabGroupTab tab1 = v1[i];
    SavedTabGroupTab tab2 = v2[i];
    EXPECT_EQ(tab1.url(), tab2.url());
    EXPECT_EQ(tab1.title(), tab2.title());
    EXPECT_EQ(tab1.favicon(), tab2.favicon());
  }
}

bool CompareSavedTabGroups(const SavedTabGroup& g1, const SavedTabGroup& g2) {
  if (g1.title() != g2.title()) {
    return false;
  }
  if (g1.color() != g2.color()) {
    return false;
  }
  if (g1.position() != g2.position()) {
    return false;
  }
  if (g1.saved_guid() != g2.saved_guid()) {
    return false;
  }
  if (g1.creation_time_windows_epoch_micros() !=
      g2.creation_time_windows_epoch_micros()) {
    return false;
  }

  return true;
}

SavedTabGroupTab CreateSavedTabGroupTab(const std::string& url,
                                        const std::u16string& title,
                                        const base::Uuid& group_guid,
                                        std::optional<int> position) {
  SavedTabGroupTab tab(GURL(url), title, group_guid, position);
  tab.SetFavicon(gfx::Image());
  return tab;
}

SavedTabGroup CreateTestSavedTabGroup(std::optional<base::Time> creation_date) {
  base::Uuid id = base::Uuid::GenerateRandomV4();
  const std::u16string title = u"Test Test";
  const tab_groups::TabGroupColorId& color = tab_groups::TabGroupColorId::kBlue;

  SavedTabGroupTab tab1 =
      CreateSavedTabGroupTab("www.google.com", u"Google", id, /*position=*/0);
  SavedTabGroupTab tab2 =
      CreateSavedTabGroupTab("chrome://newtab", u"new tab", id, /*position=*/1);

  tab1.SetFavicon(gfx::Image());
  tab2.SetFavicon(gfx::Image());

  std::vector<SavedTabGroupTab> tabs = {tab1, tab2};

  SavedTabGroup group(title, color, tabs, /*position=*/std::nullopt, id,
                      /*local_group_id=*/std::nullopt,
                      /*creator_cache_guid=*/std::nullopt,
                      /*last_updater_cache_guid=*/std::nullopt,
                      /*created_before_syncing_tab_groups=*/false,
                      /*creation_time_windows_epoch_micros=*/creation_date);
  return group;
}

SavedTabGroup CreateTestSavedTabGroupWithNoTabs() {
  const std::u16string title = u"Test Test";
  const tab_groups::TabGroupColorId& color = tab_groups::TabGroupColorId::kBlue;
  SavedTabGroup group(title, color, std::vector<SavedTabGroupTab>(),
                      std::nullopt);
  return group;
}

TabGroupVisualData CreateTabGroupVisualData() {
  const std::u16string title = u"Visuals Test";
  const tab_groups::TabGroupColorId& color =
      tab_groups::TabGroupColorId::kOrange;
  return TabGroupVisualData(title, color);
}

std::unique_ptr<syncer::DeviceInfo> CreateDeviceInfo(
    const std::string& guid,
    syncer::DeviceInfo::OsType os_type,
    syncer::DeviceInfo::FormFactor form_factor) {
  return std::make_unique<syncer::DeviceInfo>(
      guid, "name", "chrome_version", "user_agent",
      sync_pb::SyncEnums_DeviceType_TYPE_LINUX, os_type, form_factor,
      "scoped_id", "manufacturer", "model", "full_hardware_class",
      /*last_updated_timestamp=*/base::Time::Now(),
      /*pulse_interval=*/base::Days(1),
      /*send_tab_to_self_receiving_enabled=*/
      false,
      /*send_tab_to_self_receiving_type=*/
      sync_pb::
          SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED,
      /*sharing_info=*/std::nullopt,
      /*paask_info=*/std::nullopt,
      /*fcm_registration_token=*/std::string(),
      /*interested_data_types=*/syncer::DataTypeSet::All(),
      /*floating_workspace_last_signin_timestamp=*/std::nullopt);
}

}  // namespace tab_groups::test
