// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/strings/string_number_conversions.h"
#include "components/paint_preview/player/android/player_compositor_delegate_android.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_f.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace paint_preview {

TEST(PlayerCompositorDelegateAndroidTest,
     TestCompositeResponseFramesToVectors) {
  base::flat_map<base::UnguessableToken, mojom::FrameDataPtr> frames;

  auto main_guid = base::UnguessableToken::Create();
  auto frame_data_main = mojom::FrameData::New();
  auto subframe_1_guid = base::UnguessableToken::Create();
  auto frame_data_subframe_1 = mojom::FrameData::New();
  auto subframe_2_guid = base::UnguessableToken::Create();
  auto frame_data_subframe_2 = mojom::FrameData::New();

  frame_data_main->scroll_extents = gfx::Size(100, 200);
  frame_data_subframe_1->scroll_extents = gfx::Size(50, 60);
  frame_data_subframe_2->scroll_extents = gfx::Size(10, 20);

  frame_data_main->scroll_offsets = gfx::Size(150, 250);
  frame_data_subframe_1->scroll_offsets = gfx::Size(55, 65);
  frame_data_subframe_2->scroll_offsets = gfx::Size(15, 25);

  mojom::SubframeClipRect clip_rect1(subframe_1_guid,
                                     gfx::RectF(5, 10, 50, 60));
  mojom::SubframeClipRect clip_rect2(subframe_2_guid,
                                     gfx::RectF(15, 25, 30, 40));
  frame_data_main->subframes.push_back(clip_rect1.Clone());
  frame_data_subframe_1->subframes.push_back(clip_rect2.Clone());

  frames.insert({main_guid, std::move(frame_data_main)});
  frames.insert({subframe_1_guid, std::move(frame_data_subframe_1)});
  frames.insert({subframe_2_guid, std::move(frame_data_subframe_2)});

  std::vector<base::UnguessableToken> all_guids;
  std::vector<int> scroll_extents;
  std::vector<int> scroll_offsets;
  std::vector<int> subframe_count;
  std::vector<base::UnguessableToken> subframe_ids;
  std::vector<int> subframe_rects;

  PlayerCompositorDelegateAndroid::CompositeResponseFramesToVectors(
      frames, &all_guids, &scroll_extents, &scroll_offsets, &subframe_count,
      &subframe_ids, &subframe_rects);

  EXPECT_EQ(all_guids.size(), frames.size());
  EXPECT_EQ(scroll_extents.size(), 2 * frames.size());
  EXPECT_EQ(scroll_offsets.size(), 2 * frames.size());
  EXPECT_EQ(subframe_count.size(), frames.size());
  EXPECT_EQ(subframe_ids.size(), 2U);         // 2 subframes.
  EXPECT_EQ(subframe_rects.size(), 2U * 4U);  // 2 * subframes * 4 per rect.
  size_t subframe_idx = 0;
  for (size_t i = 0; i < all_guids.size(); ++i) {
    auto it = frames.find(all_guids[i]);
    ASSERT_NE(it, frames.end());

    const size_t scroll_index = i * 2;
    EXPECT_EQ(scroll_extents[scroll_index], it->second->scroll_extents.width());
    EXPECT_EQ(scroll_extents[scroll_index + 1],
              it->second->scroll_extents.height());
    EXPECT_EQ(scroll_offsets[scroll_index], it->second->scroll_offsets.width());
    EXPECT_EQ(scroll_offsets[scroll_index + 1],
              it->second->scroll_offsets.height());
    EXPECT_EQ(subframe_count[i],
              static_cast<int>(it->second->subframes.size()));
    for (size_t j = 0; j < it->second->subframes.size(); ++j) {
      const size_t offset = subframe_idx + j;
      EXPECT_EQ(subframe_ids[offset], it->second->subframes[j]->frame_guid);

      const size_t rect_offset = offset * 4;
      EXPECT_EQ(subframe_rects[rect_offset],
                it->second->subframes[j]->clip_rect.x());
      EXPECT_EQ(subframe_rects[rect_offset + 1],
                it->second->subframes[j]->clip_rect.y());
      EXPECT_EQ(subframe_rects[rect_offset + 2],
                it->second->subframes[j]->clip_rect.width());
      EXPECT_EQ(subframe_rects[rect_offset + 3],
                it->second->subframes[j]->clip_rect.height());
    }
    subframe_idx += it->second->subframes.size();
  }
}

}  // namespace paint_preview
