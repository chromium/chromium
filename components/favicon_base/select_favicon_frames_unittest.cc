// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/favicon_base/select_favicon_frames.h"

#include <stddef.h>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

using std::vector;

namespace {

const ui::ResourceScaleFactor FaviconScaleFactor1x[] = {
    ui::k100Percent,
};

const ui::ResourceScaleFactor FaviconScaleFactor1xAnd2x[] = {
    ui::k100Percent,
    ui::k200Percent,
};

#define SCOPED_FAVICON_SCALE_FACTOR(list)                  \
  ui::test::ScopedSetSupportedResourceScaleFactors scoped( \
      std::vector<ui::ResourceScaleFactor>(list, list + std::size(list)))

#define SCOPED_FAVICON_SCALE_FACTOR_1X \
  SCOPED_FAVICON_SCALE_FACTOR(FaviconScaleFactor1x)
#define SCOPED_FAVICON_SCALE_FACTOR_1XAND2X \
  SCOPED_FAVICON_SCALE_FACTOR(FaviconScaleFactor1xAnd2x)

// Return gfx::Size vector with the pixel sizes of |bitmaps|.
vector<gfx::Size> SizesFromBitmaps(const vector<SkBitmap>& bitmaps) {
  vector<gfx::Size> sizes;
  for (size_t i = 0; i < bitmaps.size(); ++i)
    sizes.push_back(gfx::Size(bitmaps[i].width(), bitmaps[i].height()));
  return sizes;
}

SkBitmap MakeBitmap(SkColor color, int w, int h) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(w, h);
  bitmap.eraseColor(color);
  return bitmap;
}

SkColor GetColor(const gfx::ImageSkia& image, float scale,
                 int x = -1, int y = -1) {
  const SkBitmap& bitmap = image.GetRepresentation(scale).GetBitmap();
  if (x == -1)
    x = bitmap.width() / 2;
  if (y == -1)
    y = bitmap.width() / 2;
  SkColor color = bitmap.getColor(x, y);
  return color;
}

SkColor GetColor1x(const gfx::ImageSkia& image) {
  return GetColor(image, 1.0f);
}

SkColor GetColor2x(const gfx::ImageSkia& image) {
  return GetColor(image, 2.0f);
}

TEST(SelectFaviconFramesTest, ZeroSizePicksLargest) {
  vector<SkBitmap> bitmaps;
  bitmaps.push_back(MakeBitmap(SK_ColorRED, 16, 16));
  bitmaps.push_back(MakeBitmap(SK_ColorGREEN, 48, 48));
  bitmaps.push_back(MakeBitmap(SK_ColorBLUE, 32, 32));

  gfx::ImageSkia image =
      CreateFaviconImageSkia(bitmaps, SizesFromBitmaps(bitmaps), 0, nullptr);
  EXPECT_EQ(1u, image.image_reps().size());
  ASSERT_TRUE(image.HasRepresentation(1.0f));
  EXPECT_EQ(48, image.width());
  EXPECT_EQ(48, image.height());

  EXPECT_EQ(SK_ColorGREEN, GetColor1x(image));
}

TEST(SelectFaviconFramesTest, _16From16) {
  SCOPED_FAVICON_SCALE_FACTOR_1X;

  vector<SkBitmap> bitmaps;
  bitmaps.push_back(MakeBitmap(SK_ColorRED, 15, 15));
  bitmaps.push_back(MakeBitmap(SK_ColorGREEN, 16, 16));
  bitmaps.push_back(MakeBitmap(SK_ColorBLUE, 17, 17));

  gfx::ImageSkia image =
      CreateFaviconImageSkia(bitmaps, SizesFromBitmaps(bitmaps), 16, nullptr);
  image.EnsureRepsForSupportedScales();
  EXPECT_EQ(1u, image.image_reps().size());
  ASSERT_TRUE(image.HasRepresentation(1.0f));
  EXPECT_EQ(16, image.width());
  EXPECT_EQ(16, image.height());
  EXPECT_EQ(SK_ColorGREEN, GetColor1x(image));

#if !BUILDFLAG(IS_IOS)
  const gfx::ImageSkiaRep& rep = image.GetRepresentation(1.5f);
  EXPECT_EQ(1.5f, rep.scale());
  EXPECT_EQ(16, rep.GetWidth());
  EXPECT_EQ(16, rep.GetHeight());
  EXPECT_EQ(24, rep.pixel_width());
  EXPECT_EQ(24, rep.pixel_height());
  EXPECT_EQ(2u, image.image_reps().size());
#endif
}

TEST(SelectFaviconFramesTest, _16From17) {
  SCOPED_FAVICON_SCALE_FACTOR_1X;

  vector<SkBitmap> bitmaps;
  bitmaps.push_back(MakeBitmap(SK_ColorRED, 15, 15));
  bitmaps.push_back(MakeBitmap(SK_ColorGREEN, 17, 17));

  // Should resample from the bigger candidate.
  gfx::ImageSkia image =
      CreateFaviconImageSkia(bitmaps, SizesFromBitmaps(bitmaps), 16, nullptr);
  image.EnsureRepsForSupportedScales();
  EXPECT_EQ(1u, image.image_reps().size());
  ASSERT_TRUE(image.HasRepresentation(1.0f));
  EXPECT_EQ(16, image.width());
  EXPECT_EQ(16, image.height());
  EXPECT_EQ(SK_ColorGREEN, GetColor1x(image));
}

TEST(SelectFaviconFramesTest, _16From15) {
  SCOPED_FAVICON_SCALE_FACTOR_1X;

  vector<SkBitmap> bitmaps;
  bitmaps.push_back(MakeBitmap(SK_ColorRED, 14, 14));
  bitmaps.push_back(MakeBitmap(SK_ColorGREEN, 15, 15));

  // If nothing else is available, should resample from the next smaller
  // candidate.
  gfx::ImageSkia image =
      CreateFaviconImageSkia(bitmaps, SizesFromBitmaps(bitmaps), 16, nullptr);
  image.EnsureRepsForSupportedScales();
  EXPECT_EQ(1u, image.image_reps().size());
  ASSERT_TRUE(image.HasRepresentation(1.0f));
  EXPECT_EQ(16, image.width());
  EXPECT_EQ(16, image.height());
  EXPECT_EQ(SK_ColorGREEN, GetColor1x(image));
}

TEST(SelectFaviconFramesTest, _16From16_Scale2x_32_From_16) {
  SCOPED_FAVICON_SCALE_FACTOR_1XAND2X;
  vector<SkBitmap> bitmaps;
  bitmaps.push_back(MakeBitmap(SK_ColorGREEN, 16, 16));

  gfx::ImageSkia image =
      CreateFaviconImageSkia(bitmaps, SizesFromBitmaps(bitmaps), 16, nullptr);
  image.EnsureRepsForSupportedScales();
  EXPECT_EQ(2u, image.image_reps().size());
  ASSERT_TRUE(image.HasRepresentation(1.0f));
  ASSERT_TRUE(image.HasRepresentation(2.0f));
  EXPECT_EQ(16, image.width());
  EXPECT_EQ(16, image.height());
  EXPECT_EQ(SK_ColorGREEN, GetColor1x(image));
  EXPECT_EQ(SK_ColorGREEN, GetColor2x(image));
}

TEST(SelectFaviconFramesTest, _16From16_Scale2x_32_From_32) {
  SCOPED_FAVICON_SCALE_FACTOR_1XAND2X;

  vector<SkBitmap> bitmaps;
  bitmaps.push_back(MakeBitmap(SK_ColorGREEN, 16, 16));
  bitmaps.push_back(MakeBitmap(SK_ColorBLUE, 32, 32));

  gfx::ImageSkia image =
      CreateFaviconImageSkia(bitmaps, SizesFromBitmaps(bitmaps), 16, nullptr);
  image.EnsureRepsForSupportedScales();
  EXPECT_EQ(2u, image.image_reps().size());
  ASSERT_TRUE(image.HasRepresentation(1.0f));
  ASSERT_TRUE(image.HasRepresentation(2.0f));
  EXPECT_EQ(16, image.width());
  EXPECT_EQ(16, image.height());
  EXPECT_EQ(SK_ColorGREEN, GetColor1x(image));
  EXPECT_EQ(SK_ColorBLUE, GetColor2x(image));

#if !BUILDFLAG(IS_IOS)
  const gfx::ImageSkiaRep& rep = image.GetRepresentation(1.5f);
  EXPECT_EQ(1.5f, rep.scale());
  EXPECT_EQ(16, rep.GetWidth());
  EXPECT_EQ(16, rep.GetHeight());
  EXPECT_EQ(24, rep.pixel_width());
  EXPECT_EQ(24, rep.pixel_height());
  EXPECT_EQ(3u, image.image_reps().size());
#endif
}

TEST(SelectFaviconFramesTest, ExactMatchBetterThanLargeBitmap) {
  SCOPED_FAVICON_SCALE_FACTOR_1XAND2X;

  float score1;
  vector<SkBitmap> bitmaps1;
  bitmaps1.push_back(MakeBitmap(SK_ColorGREEN, 48, 48));
  CreateFaviconImageSkia(
      bitmaps1,
      SizesFromBitmaps(bitmaps1), 16, &score1);

  float score2;
  vector<SkBitmap> bitmaps2;
  bitmaps2.push_back(MakeBitmap(SK_ColorGREEN, 16, 16));
  bitmaps2.push_back(MakeBitmap(SK_ColorGREEN, 32, 32));
  CreateFaviconImageSkia(bitmaps2,
      SizesFromBitmaps(bitmaps2), 16, &score2);

  EXPECT_GT(score2, score1);
}

TEST(SelectFaviconFramesTest, UpsampleABitBetterThanHugeBitmap) {
  SCOPED_FAVICON_SCALE_FACTOR_1XAND2X;

  float score1;
  vector<SkBitmap> bitmaps1;
  bitmaps1.push_back(MakeBitmap(SK_ColorGREEN, 128, 128));
  CreateFaviconImageSkia(bitmaps1,
      SizesFromBitmaps(bitmaps1), 16, &score1);

  float score2;
  vector<SkBitmap> bitmaps2;
  bitmaps2.push_back(MakeBitmap(SK_ColorGREEN, 24, 24));
  CreateFaviconImageSkia(bitmaps2,
      SizesFromBitmaps(bitmaps2), 16, &score2);

  float score3;
  vector<SkBitmap> bitmaps3;
  bitmaps3.push_back(MakeBitmap(SK_ColorGREEN, 16, 16));
  CreateFaviconImageSkia(bitmaps3,
      SizesFromBitmaps(bitmaps3), 16, &score3);

  float score4;
  vector<SkBitmap> bitmaps4;
  bitmaps4.push_back(MakeBitmap(SK_ColorGREEN, 15, 15));
  CreateFaviconImageSkia(bitmaps4,
      SizesFromBitmaps(bitmaps4), 16, &score4);

  EXPECT_GT(score2, score1);
  EXPECT_GT(score3, score1);
  EXPECT_GT(score4, score1);
}

TEST(SelectFaviconFramesTest, DownsamplingBetterThanUpsampling) {
  SCOPED_FAVICON_SCALE_FACTOR_1XAND2X;

  float score1;
  vector<SkBitmap> bitmaps1;
  bitmaps1.push_back(MakeBitmap(SK_ColorGREEN, 8, 8));
  CreateFaviconImageSkia(bitmaps1,
                         SizesFromBitmaps(bitmaps1), 16, &score1);

  float score2;
  vector<SkBitmap> bitmaps2;
  bitmaps2.push_back(MakeBitmap(SK_ColorGREEN, 24, 24));
  CreateFaviconImageSkia(bitmaps2,
                         SizesFromBitmaps(bitmaps2), 16, &score2);

  EXPECT_GT(score2, score1);
}

TEST(SelectFaviconFramesTest, DownsamplingLessIsBetter) {
  SCOPED_FAVICON_SCALE_FACTOR_1XAND2X;

  float score1;
  vector<SkBitmap> bitmaps1;
  bitmaps1.push_back(MakeBitmap(SK_ColorGREEN, 34, 34));
  CreateFaviconImageSkia(bitmaps1,
      SizesFromBitmaps(bitmaps1), 16, &score1);

  float score2;
  vector<SkBitmap> bitmaps2;
  bitmaps2.push_back(MakeBitmap(SK_ColorGREEN, 33, 33));
  CreateFaviconImageSkia(bitmaps2,
      SizesFromBitmaps(bitmaps2), 16, &score2);

  EXPECT_GT(score2, score1);
}

TEST(SelectFaviconFramesTest, UpsamplingLessIsBetter) {
  SCOPED_FAVICON_SCALE_FACTOR_1XAND2X;

  float score1;
  vector<SkBitmap> bitmaps1;
  bitmaps1.push_back(MakeBitmap(SK_ColorGREEN, 8, 8));
  CreateFaviconImageSkia(bitmaps1,
      SizesFromBitmaps(bitmaps1), 16, &score1);

  float score2;
  vector<SkBitmap> bitmaps2;
  bitmaps2.push_back(MakeBitmap(SK_ColorGREEN, 9, 9));
  CreateFaviconImageSkia(bitmaps2,
      SizesFromBitmaps(bitmaps2), 16, &score2);

  EXPECT_GT(score2, score1);
}

// Test that the score is determined by the |original_sizes| parameter, not the
// |bitmaps| parameter to SelectFaviconFrames().
TEST(SelectFaviconFramesTest, ScoreDeterminedByOriginalSizes) {
  SCOPED_FAVICON_SCALE_FACTOR_1XAND2X;

  vector<SkBitmap> bitmaps1;
  bitmaps1.push_back(MakeBitmap(SK_ColorGREEN, 16, 16));
  vector<gfx::Size> sizes1;
  sizes1.push_back(gfx::Size(256, 256));
  float score1;
  CreateFaviconImageSkia(bitmaps1, sizes1, 16, &score1);

  vector<SkBitmap> bitmaps2;
  bitmaps2.push_back(MakeBitmap(SK_ColorGREEN, 15, 15));
  vector<gfx::Size> sizes2;
  sizes2.push_back(gfx::Size(15, 15));
  float score2;
  CreateFaviconImageSkia(bitmaps2, sizes2, 16, &score2);

  EXPECT_GT(score2, score1);
}

}  // namespace
