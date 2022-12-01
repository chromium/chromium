// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/browser_tabs_model.h"

#include "chromeos/ash/components/phonehub/phone_model_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace phonehub {

TEST(BrowserTabsModelTest, Initialization) {
  std::vector<BrowserTabsModel::BrowserTabMetadata> most_recent_tabs(
      {CreateFakeBrowserTabMetadata()});

  BrowserTabsModel success(/*is_tab_sync_enabled=*/true, most_recent_tabs);
  EXPECT_TRUE(success.is_tab_sync_enabled());
  EXPECT_EQ(most_recent_tabs, success.most_recent_tabs());

  // If tab metadata is provided by tab sync is disabled, the data should be
  // cleared.
  BrowserTabsModel invalid_metadata(/*is_tab_sync_enabled=*/false,
                                    most_recent_tabs);
  EXPECT_TRUE(invalid_metadata.most_recent_tabs().empty());

  // If the number of metadata is provided exceeds |kMaxMostRecentTabs|, the
  // metadata vector will be truncated to be of size |kMaxMostRecentTabs|.
  const int exceed_max_offset = BrowserTabsModel::kMaxMostRecentTabs + 1;
  std::vector<BrowserTabsModel::BrowserTabMetadata> most_recent_tabs_exceeded(
      exceed_max_offset, CreateFakeBrowserTabMetadata());
  BrowserTabsModel truncated_metadata(/*is_tab_sync_enabled=*/true,
                                      most_recent_tabs_exceeded);
  EXPECT_EQ(BrowserTabsModel::kMaxMostRecentTabs,
            truncated_metadata.most_recent_tabs().size());
}

}  // namespace phonehub
}  // namespace ash
