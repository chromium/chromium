// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "chrome/browser/ui/tabs/tab_strip_api/types/node_id_traits.h"
#include "chrome/browser/ui/tabs/tab_strip_api/types/position_traits.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs_api {
namespace {

TEST(TabsStripServiceMojoTraitsTest, ConvertNodeId) {
  NodeId original(NodeId::Type::kCollection, "super_secret_id");

  auto serialized = mojom::NodeId::Serialize(&original);

  NodeId deserialized;
  mojom::NodeId::Deserialize(serialized, &deserialized);

  ASSERT_TRUE(original == deserialized);
}

TEST(TabsStripServiceMojoTraitsTest, ConvertPosition) {
  Position original(0, NodeId(NodeId::Type::kCollection, "super_secret_id"));

  auto serialized = mojom::Position::Serialize(&original);

  Position deserialized;
  mojom::Position::Deserialize(serialized, &deserialized);

  ASSERT_TRUE(original == deserialized);
}

TEST(TabsStripServiceMojoTraitsTest, ConvertTabGroupVisualData) {
  tab_groups::TabGroupVisualData original(
      u"super_secret_title", tab_groups::TabGroupColorId::kBlue, true);

  std::vector<uint8_t> serialized =
      mojom::TabGroupVisualData::Serialize(&original);

  tab_groups::TabGroupVisualData deserialized;
  ASSERT_TRUE(
      mojom::TabGroupVisualData::Deserialize(serialized, &deserialized));

  ASSERT_TRUE(original == deserialized);
}

TEST(TabsStripServiceMojoTraitsTest, ConvertSplitTabVisualData) {
  split_tabs::SplitTabVisualData original(split_tabs::SplitTabLayout::kVertical,
                                          0.75);

  std::vector<uint8_t> serialized =
      mojom::SplitTabVisualData::Serialize(&original);

  split_tabs::SplitTabVisualData deserialized;
  ASSERT_TRUE(
      mojom::SplitTabVisualData::Deserialize(serialized, &deserialized));

  ASSERT_TRUE(original == deserialized);
}

}  // namespace
}  // namespace tabs_api
