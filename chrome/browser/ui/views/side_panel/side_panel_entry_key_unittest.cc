// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/action_id.h"

class SidePanelEntryKeyTest : public ::testing::Test {
 protected:
  SidePanelEntryKeyTest() = default;
  ~SidePanelEntryKeyTest() override = default;
};

// Tests the `ToString()` method for regular entries.
TEST_F(SidePanelEntryKeyTest, ReturnsCorrectStringForRegularEntry) {
  SidePanelEntryKey entryKey(SidePanelEntryId::kReadingList);
  std::string result = entryKey.ToString();
  EXPECT_EQ(result, "kReadingList");
}

// Tests the `ToString()` method for entries with extension.
TEST_F(SidePanelEntryKeyTest, ReturnsCorrectStringForExtensionEntry) {
  SidePanelEntryKey extension_key(SidePanelEntryId::kExtension, "extension_id");
  std::string result = extension_key.ToString();
  EXPECT_EQ(result, "kExtension" + extension_key.extension_id().value());
}

TEST_F(SidePanelEntryKeyTest, ReturnsCorrectActionIdForEntryId) {
  std::optional<actions::ActionId> reading_list_action_id =
      SidePanelEntryIdToActionId(SidePanelEntryId::kReadingList);
  EXPECT_TRUE(reading_list_action_id.has_value());
  EXPECT_EQ(kActionSidePanelShowReadingList, reading_list_action_id.value());
  std::optional<actions::ActionId> bookmarks_action_id =
      SidePanelEntryIdToActionId(SidePanelEntryId::kBookmarks);
  EXPECT_TRUE(bookmarks_action_id.has_value());
  EXPECT_EQ(kActionSidePanelShowBookmarks, bookmarks_action_id.value());
  EXPECT_FALSE(
      SidePanelEntryIdToActionId(SidePanelEntryId::kExtension).has_value());
}
