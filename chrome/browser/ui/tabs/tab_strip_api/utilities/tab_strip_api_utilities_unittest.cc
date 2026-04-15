// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/utilities/tab_strip_api_utilities.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip.h"
#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip_model_adapter.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "mojo/public/mojom/base/error.mojom.h"
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

TEST(TabStripApiUtilsTest, GetNodeIdFromWindow) {
  auto window = mojom::Window::New();
  window->id = CreateTestNodeId(tabs_api::NodeId::Type::kWindow);
  auto data = mojom::Data::NewWindow(window.Clone());
  EXPECT_EQ(GetNodeId(*data), window->id);
}

TEST(TabStripApiUtilsTest, GetTabGroupId_Valid) {
  testing::ToyTabStrip tab_strip;
  testing::ToyTabStripModelAdapter adapter(&tab_strip);
  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  tab_groups::TabGroupVisualData visual_data;
  tabs::TabCollectionHandle handle = tab_strip.AddGroup(group_id, visual_data);

  auto result = GetTabGroupId(adapter, NodeId::FromTabCollectionHandle(handle));
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(group_id, result.value());
}

TEST(TabStripApiUtilsTest, GetTabGroupId_NotFound) {
  testing::ToyTabStrip tab_strip;
  testing::ToyTabStripModelAdapter adapter(&tab_strip);
  auto result = GetTabGroupId(
      adapter, NodeId::FromTabCollectionHandle(tabs::TabCollectionHandle(123)));
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(mojo_base::mojom::Code::kNotFound, result.error()->code);
}

TEST(TabStripApiUtilsTest, GetTabGroupId_WrongType) {
  testing::ToyTabStrip tab_strip;
  testing::ToyTabStripModelAdapter adapter(&tab_strip);
  auto result =
      GetTabGroupId(adapter, NodeId::FromTabHandle(tabs::TabHandle(123)));
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(mojo_base::mojom::Code::kInvalidArgument, result.error()->code);
}

TEST(TabStripApiUtilsTest, MergeTabGroupVisualData_EmptyMask) {
  tab_groups::TabGroupVisualData current(u"Current",
                                         tab_groups::TabGroupColorId::kBlue,
                                         /*is_collapsed=*/false);
  tab_groups::TabGroupVisualData incoming(u"Incoming",
                                          tab_groups::TabGroupColorId::kRed,
                                          /*is_collapsed=*/true);

  auto result_no_mask =
      MergeTabGroupVisualData(current, incoming, std::nullopt);
  ASSERT_TRUE(result_no_mask.has_value());
  EXPECT_EQ(u"Incoming", result_no_mask.value().title());
  EXPECT_EQ(tab_groups::TabGroupColorId::kRed, result_no_mask.value().color());
  EXPECT_TRUE(result_no_mask.value().is_collapsed());

  auto result_empty_mask =
      MergeTabGroupVisualData(current, incoming, std::vector<std::string>());
  ASSERT_TRUE(result_empty_mask.has_value());
  EXPECT_EQ(result_no_mask.value(), result_empty_mask.value());
}

TEST(TabStripApiUtilsTest, MergeTabGroupVisualData_PartialMask) {
  tab_groups::TabGroupVisualData current(u"Current",
                                         tab_groups::TabGroupColorId::kBlue,
                                         /*is_collapsed=*/false);
  tab_groups::TabGroupVisualData incoming(u"Incoming",
                                          tab_groups::TabGroupColorId::kRed,
                                          /*is_collapsed=*/true);

  auto result = MergeTabGroupVisualData(
      current, incoming, std::vector<std::string>({std::string(kTitleField)}));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(u"Incoming", result.value().title());
  EXPECT_EQ(tab_groups::TabGroupColorId::kBlue, result.value().color());
  EXPECT_FALSE(result.value().is_collapsed());
}

TEST(TabStripApiUtilsTest, MergeTabGroupVisualData_InvalidField) {
  tab_groups::TabGroupVisualData current;
  tab_groups::TabGroupVisualData incoming;

  auto result = MergeTabGroupVisualData(
      current, incoming, std::vector<std::string>({"invalid_field"}));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(mojo_base::mojom::Code::kInvalidArgument, result.error()->code);
}

}  // namespace
}  // namespace tabs_api::utils
