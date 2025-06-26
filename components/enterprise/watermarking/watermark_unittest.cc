// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/watermarking/watermark.h"

#include <vector>

#include "base/path_service.h"
#include "cc/paint/skia_paint_canvas.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace enterprise_watermark {

namespace {

struct WatermarkParams {
  WatermarkParams(SkColor color, const std::string& file_name)
      : color(color), file_name(file_name) {}

  SkColor color;
  std::string file_name;
};

class WatermarkTest : public testing::Test,
                      public testing::WithParamInterface<WatermarkParams> {};

}  // namespace

// Temporary code needed to generate png files:
//
// #include "base/files/file_util.h"
// #include "ui/gfx/codec/png_codec.h"
//
// std::optional<std::vector<uint8_t>> data =
//   gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, true);
// base::WriteFile(path, data.value());

// The consensus for pixel gold tests is that we create a single test for a
// single platform. In this case, this is to avoid issues with platform-specific
// font rendering.
// TODO(b/356446812): add reference images for other platforms.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_PageRenderedWithWatermark PageRenderedWithWatermark
#else
#define MAYBE_PageRenderedWithWatermark DISABLED_PageRenderedWithWatermark
#endif

TEST_P(WatermarkTest, MAYBE_PageRenderedWithWatermark) {
  const int kWidth = 200;
  const int kHeight = 200;
  constexpr SkColor kFillColor = SkColorSetARGB(0xb, 0x00, 0x00, 0x00);
  constexpr SkColor kOutlineColor = SkColorSetARGB(0x11, 0xff, 0xff, 0xff);
  constexpr int kFontSize = 24;

  const std::string kWatermarkText = "Private! Confidential";

  WatermarkBlock watermark_block = DrawWatermarkToPaintRecord(
      kWatermarkText, kFillColor, kOutlineColor, kFontSize);
  sk_sp<SkPicture> picture = watermark_block.record.ToSkPicture(
      SkRect::MakeWH(watermark_block.width, watermark_block.height));

  // Create bitmap-backed SkCanvas
  SkBitmap bitmap;
  bitmap.allocN32Pixels(kWidth, kHeight);
  SkCanvas canvas(bitmap);
  canvas.clear(GetParam().color);
  DrawWatermark(&canvas, picture.get(), watermark_block.width,
                watermark_block.height, SkSize::Make(kWidth, kHeight));

  base::FilePath path =
      base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT);
  path = path.AppendASCII("components")
             .AppendASCII("test")
             .AppendASCII("data")
             .AppendASCII("enterprise")
             .AppendASCII(GetParam().file_name);

  ASSERT_TRUE(cc::MatchesPNGFile(bitmap, path, cc::ExactPixelComparator()));

  // Test the cc::PaintRecord overload produces the same result
  canvas.clear(GetParam().color);
  cc::SkiaPaintCanvas skia_canvas(&canvas);
  DrawWatermark(&skia_canvas, &watermark_block.record, watermark_block.width,
                watermark_block.height, SkSize::Make(kWidth, kHeight));
  ASSERT_TRUE(cc::MatchesPNGFile(bitmap, path, cc::ExactPixelComparator()));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WatermarkTest,
    testing::Values(WatermarkParams(SkColorSetRGB(0x22, 0x22, 0x22),
                                    "watermark_dark.png"),
                    WatermarkParams(SkColorSetRGB(0xff, 0xff, 0xff),
                                    "watermark_light.png")));

}  // namespace enterprise_watermark
