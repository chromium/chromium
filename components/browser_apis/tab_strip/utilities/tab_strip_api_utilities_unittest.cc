// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_strip/utilities/tab_strip_api_utilities.h"

#include "base/strings/string_number_conversions.h"
#include "components/browser_apis/tab_strip/adapters/tab_strip_model_adapter.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/browser_apis/tab_strip/testing/toy_tab_strip.h"
#include "components/browser_apis/tab_strip/testing/toy_tab_strip_model_adapter.h"
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
  Tab tab;
  tab.id = CreateTestNodeId(tabs_api::NodeId::Type::kContent);
  auto data = mojom::Data::NewTab(tab);
  EXPECT_EQ(GetNodeId(*data), tab.id);
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

TEST(TabStripApiUtilsTest, FindActiveTabAndFlattenTabs) {
  auto root = mojom::Container::New();
  auto root_strip = mojom::TabStrip::New();
  root_strip->id = CreateTestNodeId(tabs_api::NodeId::Type::kCollection);
  root->data = mojom::Data::NewTabStrip(std::move(root_strip));

  auto unpinned = mojom::Container::New();
  auto unpinned_data = mojom::UnpinnedTabs::New();
  unpinned_data->id = CreateTestNodeId(tabs_api::NodeId::Type::kCollection);
  unpinned->data = mojom::Data::NewUnpinnedTabs(std::move(unpinned_data));

  auto tab1_node = mojom::Container::New();
  Tab tab1;
  tab1.id = CreateTestNodeId(tabs_api::NodeId::Type::kContent);
  tab1.title = u"First";
  tab1.is_active = false;
  NodeId tab1_id = tab1.id;
  tab1_node->data = mojom::Data::NewTab(std::move(tab1));
  unpinned->children.push_back(std::move(tab1_node));

  auto tab2_node = mojom::Container::New();
  Tab tab2;
  tab2.id = CreateTestNodeId(tabs_api::NodeId::Type::kContent);
  tab2.title = u"Second";
  tab2.is_active = true;
  NodeId tab2_id = tab2.id;
  tab2_node->data = mojom::Data::NewTab(std::move(tab2));
  unpinned->children.push_back(std::move(tab2_node));

  root->children.push_back(std::move(unpinned));

  const Tab* active = FindActiveTab(*root);
  ASSERT_NE(active, nullptr);
  EXPECT_EQ(active->id, tab2_id);
  EXPECT_EQ(active->title, u"Second");

  std::vector<const Tab*> flat;
  FlattenTabs(*root, flat);
  ASSERT_EQ(flat.size(), 2u);
  EXPECT_EQ(flat[0]->id, tab1_id);
  EXPECT_EQ(flat[1]->id, tab2_id);
}

}  // namespace
}  // namespace tabs_api::utils
