// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gl_i420_converter.h"

#include <GLES2/gl2ext.h>

#include "cc/test/pixel_test.h"
#include "cc/test/pixel_test_utils.h"
#include "components/viz/common/gl_scaler_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace viz {

class GLI420ConverterPixelTest : public cc::PixelTest,
                                 public GLScalerTestUtil,
                                 public testing::WithParamInterface<bool> {
 public:
  bool allow_mrt_path() const { return GetParam(); }
  GLI420Converter* converter() const { return converter_.get(); }

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
    converter_.reset(new GLI420Converter(context_provider(), allow_mrt_path()));
    texture_helper_ = std::make_unique<GLScalerTestTextureHelper>(
        context_provider()->ContextGL());
  }

  void TearDown() final {
    texture_helper_.reset();
    converter_.reset();
    cc::PixelTest::TearDown();
  }

 private:
  std::unique_ptr<GLI420Converter> converter_;
  std::unique_ptr<GLScalerTestTextureHelper> texture_helper_;
};

// Note: This test is pretty much the same as
// GLScalerPixelTest.Example_ScaleAndExportForScreenVideoCapture. The goal of
// this pixel test is to just confirm that everything internal to
// GLI420Converter has been plumbed-through correctly.
TEST_P(GLI420ConverterPixelTest, ScaleAndConvert) {
  // These parameters have been chosen based on: 1) overriding defaults, to
  // confirm Parameters plumbing; and 2) typical operation on most platforms
  // (e.g., flipped source textures, the need to swizzle outputs, etc.).
  GLI420Converter::Parameters params;
  params.scale_from = gfx::Vector2d(2160, 1440);
  params.scale_to = gfx::Vector2d(1280, 720);
  params.source_color_space = DefaultRGBColorSpace();
  params.output_color_space = DefaultYUVColorSpace();
  params.enable_precise_color_management =
      converter()->SupportsPreciseColorManagement();
  params.quality = GLScaler::Parameters::Quality::GOOD;
  params.is_flipped_source = true;
  params.flip_output = true;
  params.swizzle[0] = GL_BGRA_EXT;  // Swizzle for readback.
  ASSERT_TRUE(converter()->Configure(params));

  constexpr gfx::Size kSourceSize = gfx::Size(2160, 1440);
  const GLuint src_texture = UploadTexture(
      CreateVerticallyFlippedBitmap(CreateSMPTETestImage(kSourceSize)));
  constexpr gfx::Rect kOutputRect = gfx::Rect(0, 0, 1280, 720);
  ASSERT_EQ(kOutputRect, GLI420Converter::ToAlignedRect(kOutputRect));
  SkBitmap expected = CreateSMPTETestImage(kOutputRect.size());
  ConvertBitmapToYUV(&expected);

  // While the output size is 1280x720, the packing of 4 pixels into one RGBA
  // quad means that the texture width must be divided by 4 (for the Y
  // plane). Then, the other two planes are half the size of the Y plane in both
  // dimensions.
  const gfx::Size y_plane_size(kOutputRect.width() / 4, kOutputRect.height());
  const gfx::Size chroma_plane_size(y_plane_size.width() / 2,
                                    y_plane_size.height() / 2);
  const GLuint yuv_textures[3] = {CreateTexture(y_plane_size),
                                  CreateTexture(chroma_plane_size),
                                  CreateTexture(chroma_plane_size)};

  ASSERT_TRUE(converter()->Convert(src_texture, kSourceSize, gfx::Vector2d(),
                                   kOutputRect, yuv_textures));

  // Download the textures, and unpack them into an interleaved YUV bitmap, for
  // comparison against the |expected| rendition.
  SkBitmap actual = AllocateRGBABitmap(kOutputRect.size());
  actual.eraseColor(SkColorSetARGB(0xff, 0x00, 0x80, 0x80));
  SkBitmap y_plane = DownloadTexture(yuv_textures[0], y_plane_size);
  SwizzleBitmap(&y_plane);
  UnpackPlanarBitmap(y_plane, 0, &actual);
  SkBitmap u_plane = DownloadTexture(yuv_textures[1], chroma_plane_size);
  SwizzleBitmap(&u_plane);
  UnpackPlanarBitmap(u_plane, 1, &actual);
  SkBitmap v_plane = DownloadTexture(yuv_textures[2], chroma_plane_size);
  SwizzleBitmap(&v_plane);
  UnpackPlanarBitmap(v_plane, 2, &actual);

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

// Run the tests twice, once disallowing use of the MRT path, and once allowing
// its use (auto-detecting whether the current platform supports it).
INSTANTIATE_TEST_SUITE_P(All, GLI420ConverterPixelTest, testing::Bool());

}  // namespace viz
