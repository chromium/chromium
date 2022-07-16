// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gl_nv12_converter.h"

#include <GLES2/gl2ext.h>

#include "cc/test/pixel_test.h"
#include "cc/test/pixel_test_utils.h"
#include "components/viz/test/gl_scaler_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace viz {

class GLNV12ConverterPixelTest
    : public cc::PixelTest,
      public GLScalerTestUtil,
      public testing::WithParamInterface<
          std::tuple<bool, bool, gfx::Vector2d, gfx::Vector2d>> {
 public:
  bool allow_mrt_path() const { return std::get<0>(GetParam()); }
  bool is_rgba() const { return std::get<1>(GetParam()); }
  gfx::Vector2d scale_from() const { return std::get<2>(GetParam()); }
  gfx::Vector2d scale_to() const { return std::get<3>(GetParam()); }

  GLNV12Converter* converter() const { return converter_.get(); }

  GLuint CreateTexture(const gfx::Size& size) {
    return texture_helper_->CreateTexture(size);
  }

  GLuint UploadTexture(const SkBitmap& bitmap) {
    return texture_helper_->UploadTexture(bitmap);
  }

  SkBitmap DownloadTexture(GLuint texture, const gfx::Size& size) {
    return texture_helper_->DownloadTexture(texture, size);
  }

 protected:
  void SetUp() final {
    cc::PixelTest::SetUpGLWithoutRenderer(gfx::SurfaceOrigin::kBottomLeft);
    converter_ = GLNV12Converter::CreateConverterForTest(context_provider(),
                                                         allow_mrt_path());
    texture_helper_ = std::make_unique<GLScalerTestTextureHelper>(
        context_provider()->ContextGL());
  }

  void TearDown() final {
    texture_helper_.reset();
    converter_.reset();
    cc::PixelTest::TearDown();
  }

 private:
  std::unique_ptr<GLNV12Converter> converter_;
  std::unique_ptr<GLScalerTestTextureHelper> texture_helper_;
};

// Note: This test is pretty much the same as
// GLScalerPixelTest.Example_ScaleAndExportForScreenVideoCapture. The goal of
// this pixel test is to just confirm that everything internal to
// GLNV12Converter has been plumbed-through correctly.
TEST_P(GLNV12ConverterPixelTest, ScaleAndConvert) {
  GLNV12Converter::Parameters params;
  params.scale_from = scale_from();
  params.scale_to = scale_to();
  params.source_color_space = DefaultRGBColorSpace();
  params.output_color_space = DefaultYUVColorSpace();
  params.enable_precise_color_management =
      converter()->SupportsPreciseColorManagement();
  params.quality = GLScaler::Parameters::Quality::GOOD;
  params.is_flipped_source = true;
  params.flip_output = true;
  params.swizzle[0] =
      is_rgba() ? GL_RGBA : GL_BGRA_EXT;  // Swizzle for readback.
  ASSERT_TRUE(converter()->Configure(params));

  const gfx::Size kSourceSize = gfx::Size(scale_from().x(), scale_from().y());
  const GLuint src_texture = UploadTexture(
      CreateVerticallyFlippedBitmap(CreateSMPTETestImage(kSourceSize)));
  const gfx::Rect kOutputRect = gfx::Rect(0, 0, scale_to().x(), scale_to().y());
  ASSERT_EQ(kOutputRect, GLNV12Converter::ToAlignedRect(kOutputRect));
  SkBitmap expected = CreateSMPTETestImage(kOutputRect.size());
  ConvertRGBABitmapToYUV(&expected);

  // While the output size is `kOutputRect.Size()`, the packing of 4 pixels into
  // one RGBA quad means that the texture width must be divided by 4 (for the Y
  // plane). Then, the chroma plane is half the size of the Y plane in both
  // dimensions, but the width is actually double that since we pack 2 values,
  // so `y_plane_size.width() * 1/2 * 2` term is simplified to just
  // `y_plane_size.width()`.
  const gfx::Size y_plane_size(kOutputRect.width() / 4, kOutputRect.height());
  const gfx::Size chroma_plane_size(y_plane_size.width(),
                                    y_plane_size.height() / 2);

  const GLuint yuv_textures[2] = {CreateTexture(y_plane_size),
                                  CreateTexture(chroma_plane_size)};

  ASSERT_TRUE(converter()->Convert(src_texture, kSourceSize, gfx::Vector2d(),
                                   kOutputRect, yuv_textures));

  // Download the textures, and unpack them into an interleaved YUV bitmap, for
  // comparison against the |expected| rendition.
  SkBitmap actual = AllocateRGBABitmap(kOutputRect.size());
  actual.eraseColor(SkColorSetARGB(0xff, 0x00, 0x00, 0x00));

  SkBitmap y_plane = DownloadTexture(yuv_textures[0], y_plane_size);
  SkBitmap uv_plane = DownloadTexture(yuv_textures[1], chroma_plane_size);

  if (!is_rgba()) {
    // We've asked the converter to produce output in BGRA, & downloaded it to
    // RGBA SkBitmap, so swizzle it.
    SwizzleBitmap(&y_plane);
    SwizzleBitmap(&uv_plane);
  }

  UnpackPlanarBitmap(y_plane, 0, &actual);
  UnpackUVBitmap(uv_plane, &actual);

  // Provide generous error limits to account for the chroma subsampling in the
  // |actual| result when compared to the perfect |expected| rendition.
  constexpr float kAvgAbsoluteErrorLimit = 16.f;
  constexpr int kMaxAbsoluteErrorLimit = 0x80;
  EXPECT_TRUE(cc::FuzzyPixelComparator(false, 100.f, 0.f,
                                       kAvgAbsoluteErrorLimit,
                                       kMaxAbsoluteErrorLimit, 0)
                  .Compare(expected, actual))
      << "\nActual: " << cc::GetPNGDataUrl(actual)
      << "\nExpected: " << cc::GetPNGDataUrl(expected);
}

// Run the tests, first parameter controls whether the MRT path is allowed,
// the second controls whether the converter will use RGBA format vs BGRA (true
// for RGBA, i.e. no swizzling done by the converter, false for BGRA, i.e. the
// converter will swizzle the results when writing to texture).
// These parameters have been chosen based on: 1) overriding defaults, to
// confirm Parameters plumbing; and 2) typical operation on most platforms
// (e.g., flipped source textures, the need to swizzle outputs, etc.). In
// addition, the `scale_to()` is made to return width that is divisible by 4,
// but not by 8, to test alignment requirements.
INSTANTIATE_TEST_SUITE_P(
    All,
    GLNV12ConverterPixelTest,
    testing::Combine(testing::Bool(),
                     testing::Bool(),
                     testing::Values(gfx::Vector2d(2160, 1440)),
                     testing::Values(gfx::Vector2d(1280, 720),
                                     gfx::Vector2d(900, 600))));

}  // namespace viz
