// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/browser_apis/tab_strip/types/image_traits.h"
#include "components/browser_apis/tab_strip/types/node_id_traits.h"
#include "components/browser_apis/tab_strip/types/position_traits.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

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

TEST(TabsStripServiceMojoTraitsTest, ConvertImage) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(SK_ColorRED);
  gfx::ImageSkia original = gfx::ImageSkia::CreateFromBitmap(bitmap, 1.0f);

  std::vector<uint8_t> serialized = mojom::Image::Serialize(&original);

  gfx::ImageSkia deserialized;
  ASSERT_TRUE(mojom::Image::Deserialize(serialized, &deserialized));

  ASSERT_FALSE(deserialized.isNull());
  ASSERT_EQ(original.bitmap()->getColor(0, 0),
            deserialized.bitmap()->getColor(0, 0));
}

}  // namespace
}  // namespace tabs_api
