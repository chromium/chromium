// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/utilities/tab_strip_api_utilities.h"

#include "base/strings/string_number_conversions.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs_api::utils {
namespace {

tabs_api::NodeId CreateTestNodeId(enum tabs_api::NodeId::Type type) {
  static uint64_t counter = 0;
  return NodeId(type, base::NumberToString(++counter));
}

TEST(TabStripApiUtilsTest, GetNodeIdFromTab) {
  auto tab = mojom::Tab::New();
  tab->id = CreateTestNodeId(tabs_api::NodeId::Type::kContent);
  auto data = mojom::Data::NewTab(tab.Clone());
  EXPECT_EQ(GetNodeId(*data), tab->id);
}

TEST(TabStripApiUtilsTest, GetNodeIdFromTabGroup) {
  auto group = mojom::TabGroup::New();
  group->id = CreateTestNodeId(tabs_api::NodeId::Type::kCollection);
  auto data = mojom::Data::NewTabGroup(group.Clone());
  EXPECT_EQ(GetNodeId(*data), group->id);
}

TEST(TabStripApiUtilsTest, GetNodeIdFromTabStrip) {
  auto tab_strip = mojom::TabStrip::New();
  tab_strip->id = CreateTestNodeId(tabs_api::NodeId::Type::kCollection);
  auto data = mojom::Data::NewTabStrip(tab_strip.Clone());
  EXPECT_EQ(GetNodeId(*data), tab_strip->id);
}

TEST(TabStripApiUtilsTest, GetNodeIdFromPinnedTabs) {
  auto pinned = mojom::PinnedTabs::New();
  pinned->id = CreateTestNodeId(tabs_api::NodeId::Type::kCollection);
  auto data = mojom::Data::NewPinnedTabs(pinned.Clone());
  EXPECT_EQ(GetNodeId(*data), pinned->id);
}

TEST(TabStripApiUtilsTest, GetNodeIdFromUnpinnedTabs) {
  auto unpinned = mojom::UnpinnedTabs::New();
  unpinned->id = CreateTestNodeId(tabs_api::NodeId::Type::kCollection);
  auto data = mojom::Data::NewUnpinnedTabs(unpinned.Clone());
  EXPECT_EQ(GetNodeId(*data), unpinned->id);
}

TEST(TabStripApiUtilsTest, GetNodeIdFromSplitTab) {
  auto split = mojom::SplitTab::New();
  split->id = CreateTestNodeId(tabs_api::NodeId::Type::kCollection);
  auto data = mojom::Data::NewSplitTab(split.Clone());
  EXPECT_EQ(GetNodeId(*data), split->id);
}

}  // namespace
}  // namespace tabs_api::utils
