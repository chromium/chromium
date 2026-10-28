// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/full_screen_video_util.h"

#include <optional>

#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"

namespace viz {
namespace {

constexpr gfx::Rect kOutputRect(0, 0, 1920, 1080);

TEST(FullScreenVideoUtilTest, ContainedAndCenteredRect) {
  // Letterboxed.
  EXPECT_EQ(
      ContainedAndCenteredRect(gfx::RectF(10, 20, 239, 100),
                               gfx::SizeF(1920, 1080)),
      gfx::RectF(0, (1080 - 1920 * 100 / 239.f) / 2, 1920, 1920 * 100 / 239.f));
  // Pillarboxed.
  EXPECT_EQ(
      ContainedAndCenteredRect(gfx::RectF(0, 0, 4, 3), gfx::SizeF(1920, 1080)),
      gfx::RectF(240, 0, 1440, 1080));
  // Same aspect ratio.
  EXPECT_EQ(
      ContainedAndCenteredRect(gfx::RectF(5, 5, 16, 9), gfx::SizeF(1920, 1080)),
      gfx::RectF(0, 0, 1920, 1080));
}

TEST(FullScreenVideoUtilTest, ContainedAndCenteredRectEmpty) {
  EXPECT_EQ(ContainedAndCenteredRect(gfx::RectF(10, 20, 0, 0),
                                     gfx::SizeF(1920, 1080)),
            gfx::RectF());
  EXPECT_EQ(ContainedAndCenteredRect(gfx::RectF(0, 0, 100, 0),
                                     gfx::SizeF(1920, 1080)),
            gfx::RectF());
  EXPECT_EQ(ContainedAndCenteredRect(gfx::RectF(0, 0, 0, 100),
                                     gfx::SizeF(1920, 1080)),
            gfx::RectF());
}

class FullScreenLetterboxingTest : public testing::Test {
 protected:
  const DrawQuad* AddSolidColorQuad(const gfx::Rect& rect, SkColor4f color) {
    SharedQuadState* sqs = pass_.CreateAndAppendSharedQuadState();
    sqs->quad_layer_rect = rect;
    sqs->visible_quad_layer_rect = rect;
    auto* quad = pass_.CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    quad->SetNew(sqs, rect, rect, color, /*anti_aliasing_off=*/false);
    return quad;
  }

  AggregatedRenderPass pass_;
};

TEST_F(FullScreenLetterboxingTest, BlackBackground) {
  EXPECT_TRUE(IsPossibleFullScreenLetterboxing(
      AddSolidColorQuad(kOutputRect, SkColors::kBlack), kOutputRect));
  // Touching two sides from the origin is enough.
  EXPECT_TRUE(IsPossibleFullScreenLetterboxing(
      AddSolidColorQuad(gfx::Rect(0, 0, 1920, 500), SkColors::kBlack),
      kOutputRect));
}

TEST_F(FullScreenLetterboxingTest, NotLetterboxing) {
  EXPECT_FALSE(IsPossibleFullScreenLetterboxing(nullptr, kOutputRect));
  EXPECT_FALSE(IsPossibleFullScreenLetterboxing(
      AddSolidColorQuad(kOutputRect, SkColors::kWhite), kOutputRect));
  EXPECT_FALSE(IsPossibleFullScreenLetterboxing(
      AddSolidColorQuad(gfx::Rect(10, 0, 1910, 1080), SkColors::kBlack),
      kOutputRect));
  EXPECT_FALSE(IsPossibleFullScreenLetterboxing(
      AddSolidColorQuad(gfx::Rect(0, 0, 1000, 1000), SkColors::kBlack),
      kOutputRect));
}

TEST(FullScreenVideoUtilTest, GetFullScreenVideoRect) {
  // Exactly full screen.
  EXPECT_EQ(GetFullScreenVideoRect(gfx::RectF(kOutputRect), std::nullopt,
                                   kOutputRect, kFullScreenVideoTightTolerance),
            gfx::RectF(kOutputRect));

  // Letterboxed 2.39:1, within a pixel of the ideal position.
  const gfx::RectF letterboxed(0, 138, 1920, 803);
  std::optional<gfx::RectF> rect = GetFullScreenVideoRect(
      letterboxed, std::nullopt, kOutputRect, kFullScreenVideoTightTolerance);
  ASSERT_TRUE(rect);
  EXPECT_TRUE(rect->ApproximatelyEqual(letterboxed, 1, 1));

  // Off center.
  EXPECT_FALSE(GetFullScreenVideoRect(gfx::RectF(0, 0, 1920, 803), std::nullopt,
                                      kOutputRect,
                                      kFullScreenVideoTightTolerance));
  // Off center, but within a larger tolerance.
  EXPECT_TRUE(GetFullScreenVideoRect(gfx::RectF(0, 130, 1920, 803),
                                     std::nullopt, kOutputRect, 10.f));

  // Not filling the output in either dimension.
  EXPECT_FALSE(GetFullScreenVideoRect(gfx::RectF(160, 90, 1600, 900),
                                      std::nullopt, kOutputRect,
                                      kFullScreenVideoTightTolerance));

  // Clipped to the ideal rect is allowed, clipped more is not.
  EXPECT_TRUE(GetFullScreenVideoRect(gfx::RectF(kOutputRect), kOutputRect,
                                     kOutputRect,
                                     kFullScreenVideoTightTolerance));
  EXPECT_FALSE(GetFullScreenVideoRect(gfx::RectF(kOutputRect),
                                      gfx::Rect(0, 0, 960, 1080), kOutputRect,
                                      kFullScreenVideoTightTolerance));

  // A video one pixel larger than the output, e.g. due to rounding, is snapped
  // to the output.
  EXPECT_EQ(GetFullScreenVideoRect(gfx::RectF(0, 0, 1921, 1081), std::nullopt,
                                   kOutputRect, kFullScreenVideoTightTolerance),
            gfx::RectF(kOutputRect));

  // An empty video is never full screen.
  EXPECT_FALSE(GetFullScreenVideoRect(gfx::RectF(), std::nullopt, kOutputRect,
                                      kFullScreenVideoTightTolerance));
}

}  // namespace
}  // namespace viz
