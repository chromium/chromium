// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/browser_apis/tab_strip/types/image_mojom_traits.h"
#include "components/browser_apis/tab_strip/types/node_id_mojom_traits.h"
#include "components/browser_apis/tab_strip/types/path_mojom_traits.h"
#include "components/browser_apis/tab_strip/types/position_mojom_traits.h"
#include "components/browser_apis/tab_strip/types/tab.h"
#include "components/browser_apis/tab_strip/types/tab_mojom_traits.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tab_groups/tab_group_visual_data.h"
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

TEST(TabsStripServiceMojoTraitsTest, ConvertPath) {
  Path original({NodeId(NodeId::Type::kCollection, "id1"),
                 NodeId(NodeId::Type::kCollection, "id2")});

  auto serialized = mojom::Path::Serialize(&original);

  Path deserialized;
  mojom::Path::Deserialize(serialized, &deserialized);

  ASSERT_TRUE(original == deserialized);
}

TEST(TabsStripServiceMojoTraitsTest, ConvertPosition) {
  Position original(0, Path({NodeId(NodeId::Type::kCollection, "id1")}));

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
  split_tabs::SplitTabVisualData original(
      split_tabs::SplitTabLayout::kSideBySide, 0.75);

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

TEST(TabsStripServiceMojoTraitsTest, ConvertTab) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(SK_ColorBLUE);

  Tab original;
  original.id = NodeId::FromTabHandle(tabs::TabHandle(42));
  original.title = u"Example Tab Title";
  original.url = GURL("https://example.com/path");
  original.favicon = gfx::ImageSkia::CreateFromBitmap(bitmap, 1.0f);
  original.alert_states = {tabs::TabAlert::kAudioPlaying,
                           tabs::TabAlert::kPipPlaying};
  original.network_state = tabs::TabNetworkState::kLoading;
  original.is_active = true;
  original.is_selected = true;
  original.is_blocked = false;
  original.last_active = base::TimeTicks() + base::Seconds(123);

  std::vector<uint8_t> serialized = mojom::Tab::Serialize(&original);

  Tab deserialized;
  ASSERT_TRUE(mojom::Tab::Deserialize(serialized, &deserialized));

  EXPECT_EQ(original.id, deserialized.id);
  EXPECT_EQ(deserialized.ToTabHandle(), tabs::TabHandle(42));
  EXPECT_EQ(Tab().ToTabHandle(), tabs::TabHandle::Null());
  EXPECT_EQ(original.title, deserialized.title);
  EXPECT_EQ(original.url, deserialized.url);
  ASSERT_FALSE(deserialized.favicon.isNull());
  EXPECT_EQ(original.favicon.bitmap()->getColor(0, 0),
            deserialized.favicon.bitmap()->getColor(0, 0));
  EXPECT_EQ(original.alert_states, deserialized.alert_states);
  EXPECT_EQ(original.network_state, deserialized.network_state);
  EXPECT_EQ(original.is_active, deserialized.is_active);
  EXPECT_EQ(original.is_selected, deserialized.is_selected);
  EXPECT_EQ(original.is_blocked, deserialized.is_blocked);
  EXPECT_EQ(original.last_active, deserialized.last_active);
}

}  // namespace
}  // namespace tabs_api
