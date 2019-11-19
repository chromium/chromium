// Copyright 2019 The Chromium Authors. All rights reserved.
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

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace paint_preview {

TEST(PlayerCompositorDelegateAndroidTest,
     TestCompositeResponseFramesToVectors) {
  base::flat_map<uint64_t, mojom::FrameDataPtr> frames;

  auto frame_data_main = mojom::FrameData::New();
  auto frame_data_subframe_1 = mojom::FrameData::New();
  auto frame_data_subframe_2 = mojom::FrameData::New();

  frame_data_main->scroll_extents = gfx::Size(100, 200);
  frame_data_subframe_1->scroll_extents = gfx::Size(50, 60);
  frame_data_subframe_2->scroll_extents = gfx::Size(10, 20);

  mojom::SubframeClipRect clip_rect1(50, gfx::Rect(5, 10, 50, 60));
  mojom::SubframeClipRect clip_rect2(76, gfx::Rect(15, 25, 30, 40));
  frame_data_main->subframes.push_back(clip_rect1.Clone());
  frame_data_subframe_1->subframes.push_back(clip_rect2.Clone());

  frames.insert({12, std::move(frame_data_main)});
  frames.insert({50, std::move(frame_data_subframe_1)});
  frames.insert({76, std::move(frame_data_subframe_2)});

  std::vector<int64_t> all_guids;
  std::vector<int> scroll_extents;
  std::vector<int> subframe_count;
  std::vector<int64_t> subframe_ids;
  std::vector<int> subframe_rects;

  PlayerCompositorDelegateAndroid::CompositeResponseFramesToVectors(
      frames, &all_guids, &scroll_extents, &subframe_count, &subframe_ids,
      &subframe_rects);

  EXPECT_THAT(all_guids, ::testing::ElementsAre(12, 50, 76));
  EXPECT_THAT(scroll_extents, ::testing::ElementsAre(100, 200, 50, 60, 10, 20));
  EXPECT_THAT(subframe_count, ::testing::ElementsAre(1, 1, 0));
  EXPECT_THAT(subframe_ids, ::testing::ElementsAre(50, 76));
  EXPECT_THAT(subframe_rects,
              ::testing::ElementsAre(5, 10, 50, 60, 15, 25, 30, 40));
}

}  // namespace paint_preview
