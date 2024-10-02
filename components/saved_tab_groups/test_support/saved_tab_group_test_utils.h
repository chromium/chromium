// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_TEST_SUPPORT_SAVED_TAB_GROUP_TEST_UTILS_H_
#define COMPONENTS_SAVED_TAB_GROUPS_TEST_SUPPORT_SAVED_TAB_GROUP_TEST_UTILS_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/sync_device_info/device_info.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace tab_groups::test {

MATCHER_P2(HasSavedGroupMetadata, title, color, "") {
  return base::UTF16ToUTF8(arg.title()) == title && arg.color() == color &&
         !arg.collaboration_id().has_value();
}

MATCHER_P3(HasSharedGroupMetadata, title, color, collaboration_id, "") {
  return base::UTF16ToUTF8(arg.title()) == title && arg.color() == color &&
         arg.collaboration_id() == collaboration_id;
}

MATCHER_P2(HasTabMetadata, title, url, "") {
  return base::UTF16ToUTF8(arg.title()) == title && arg.url() == GURL(url);
}

// Utility methods to generate IDs.
LocalTabGroupID GenerateRandomTabGroupID();
LocalTabID GenerateRandomTabID();

// Comparison methods. Note, some of the properties, such as position etc are
// generated automatically by the model when the caller passes std::nullopt and
// hence might differ from the value passed in to the model.
void CompareSavedTabGroupTabs(const std::vector<SavedTabGroupTab>& v1,
                              const std::vector<SavedTabGroupTab>& v2);
bool CompareSavedTabGroups(const SavedTabGroup& g1, const SavedTabGroup& g2);

// Helper method to create saved tabs and groups.
SavedTabGroupTab CreateSavedTabGroupTab(
    const std::string& url,
    const std::u16string& title,
    const base::Uuid& group_guid,
    std::optional<int> position = std::nullopt);
SavedTabGroup CreateTestSavedTabGroup(
    std::optional<base::Time> creation_date = std::nullopt);
SavedTabGroup CreateTestSavedTabGroupWithNoTabs();
TabGroupVisualData CreateTabGroupVisualData();

// Helper method to create a device info.
std::unique_ptr<syncer::DeviceInfo> CreateDeviceInfo(
    const std::string& guid,
    syncer::DeviceInfo::OsType os_type,
    syncer::DeviceInfo::FormFactor form_factor);

}  // namespace tab_groups::test

#endif  // COMPONENTS_SAVED_TAB_GROUPS_TEST_SUPPORT_SAVED_TAB_GROUP_TEST_UTILS_H_
