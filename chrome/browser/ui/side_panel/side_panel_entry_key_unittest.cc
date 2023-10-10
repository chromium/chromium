// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "testing/gtest/include/gtest/gtest.h"

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
