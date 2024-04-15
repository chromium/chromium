// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_TEST_UTILS_H_
#define COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_TEST_UTILS_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/uuid.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/saved_tab_groups/types.h"
#include "components/tab_groups/tab_group_visual_data.h"

namespace tab_groups::test {

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
SavedTabGroup CreateTestSavedTabGroup();
SavedTabGroup CreateTestSavedTabGroupWithNoTabs();
TabGroupVisualData CreateTabGroupVisualData();

}  // namespace tab_groups::test

#endif  // COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_TEST_UTILS_H_
