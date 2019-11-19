// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gl_scaler.h"

#include <sstream>

#include "base/strings/pattern.h"
#include "build/build_config.h"
#include "cc/test/pixel_test.h"
#include "cc/test/pixel_test_utils.h"
#include "components/viz/common/gl_scaler_test_util.h"
#include "components/viz/common/gpu/context_provider.h"
#include "gpu/GLES2/gl2chromium.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_space.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace viz {

#define EXPECT_STRING_MATCHES(expected, actual)                     \
  if (!base::MatchPattern(actual, expected)) {                      \
    ADD_FAILURE() << "\nActual: " << (actual)                       \
                  << "\nExpected to match pattern: " << (expected); \
  }

class GLScalerPixelTest : public cc::PixelTest, public GLScalerTestUtil {
 public:
  GLScaler* scaler() const { return scaler_.get(); }

  std::string GetScalerString() const {
    std::ostringstream oss;
    oss << *scaler_;
    return oss.str();
  }

  GLuint CreateTexture(const gfx::Size& size) {
    return texture_helper_->CreateTexture(size);
  }

  GLuint UploadTexture(const SkBitmap& bitmap) {
    return texture_helper_->UploadTexture(bitmap);
  }

  SkBitmap DownloadTexture(GLuint texture, const gfx::Size& size) {
    return texture_helper_->DownloadTexture(texture, size);
  }

  // Test convenience to upload |src_bitmap| to the GPU, execute the scaling,
  // then download the result from the GPU and return it as a SkBitmap.
  SkBitmap Scale(const SkBitmap& src_bitmap,
                 const gfx::Vector2d& src_offset,
                 const gfx::Rect& output_rect) {
    const GLuint src_texture = UploadTexture(src_bitmap);
    const GLuint dest_texture = CreateTexture(output_rect.size());
    if (!scaler()->Scale(src_texture,
                         gfx::Size(src_bitmap.width(), src_bitmap.height()),
                         src_offset, dest_texture, output_rect)) {
      return SkBitmap();
    }
    return DownloadTexture(dest_texture, output_rect.size());
  }

  // Returns the amount of color error expected due to bugs in the current
  // platform's bilinear texture sampler.
  int GetBaselineColorDifference() const {
#if defined(OS_ANDROID)
    // Android seems to have texture sampling problems that are not at all seen
    // on any of the desktop platforms. Also, versions before Marshmallow seem
    // to have a much larger accuracy issues.
    if (base::android::BuildInfo::GetInstance()->sdk_int() <
        base::android::SDK_VERSION_MARSHMALLOW) {
      return 12;
    }
    return 2;
#else
    return 0;
#endif
  }

 protected:
  void SetUp() final {
    cc::PixelTest::SetUpGLWithoutRenderer(false);

    scaler_ = std::make_unique<GLScaler>(context_provider());
    gl_ = context_provider()->ContextGL();
    CHECK(gl_);
    texture_helper_ = std::make_unique<GLScalerTestTextureHelper>(gl_);
  }

  bool IsAndroidMarshmallow() {
#if defined(OS_ANDROID)
    return base::android::BuildInfo::GetInstance()->sdk_int() ==
           base::android::SDK_VERSION_MARSHMALLOW;
#else
    return false;
#endif
  }

  void TearDown() final {
    texture_helper_.reset();
    gl_ = nullptr;
    scaler_.reset();

    cc::PixelTest::TearDown();
  }

 private:
  std::unique_ptr<GLScaler> scaler_;
  gpu::gles2::GLES2Interface* gl_ = nullptr;
  std::unique_ptr<GLScalerTestTextureHelper> texture_helper_;
};

// Tests that the default GLScaler::Parameters produces an unscaled copy.
TEST_F(GLScalerPixelTest, CopiesByDefault) {
  // Disabled on Marshmallow. See crbug.com/933080
  if (IsAndroidMarshmallow())
    return;

  ASSERT_TRUE(scaler()->Configure(GLScaler::Parameters()));
  EXPECT_EQ(u8"Output ← {BILINEAR/lowp copy} ← Source", GetScalerString());
  const SkBitmap source = CreateSMPTETestImage(kSMPTEFullSize);
  const SkBitmap actual =
      Scale(source, gfx::Vector2d(), gfx::Rect(kSMPTEFullSize));
  int max_color_diff = GetBaselineColorDifference();
  EXPECT_TRUE(LooksLikeSMPTETestImage(
      actual, kSMPTEFullSize, gfx::Rect(kSMPTEFullSize), 0, &max_color_diff))
      << "max_color_diff measured was " << max_color_diff
      << "\nActual: " << cc::GetPNGDataUrl(actual);
}

// Tests a FAST quality scaling of 2→1 in X and 3→2 in Y.
TEST_F(GLScalerPixelTest, ScalesAtFastQuality) {
  GLScaler::Parameters params;
  params.scale_from = gfx::Vector2d(2, 3);
  params.scale_to = gfx::Vector2d(1, 2);
  params.quality = GLScaler::Parameters::Quality::FAST;
  params.is_flipped_source = false;
  ASSERT_TRUE(scaler()->Configure(params));
  EXPECT_EQ(u8"Output ← {BILINEAR/lowp [2 3] to [1 2]} ← Source",
            GetScalerString());
  const SkBitmap source = CreateSMPTETestImage(kSMPTEFullSize);
  static_assert(kSMPTEFullSize.width() % 2 == 0, "Fix kSMPTEFullSize.");
  static_assert(kSMPTEFullSize.height() % 3 == 0, "Fix kSMPTEFullSize.");
  const SkBitmap actual = Scale(source, gfx::Vector2d(),
                                gfx::Rect(0, 0, kSMPTEFullSize.width() / 2,
                                          kSMPTEFullSize.height() * 2 / 3));
  int max_color_diff = GetBaselineColorDifference();
  EXPECT_TRUE(LooksLikeSMPTETestImage(
      actual, kSMPTEFullSize, gfx::Rect(kSMPTEFullSize), 2, &max_color_diff))
      << "max_color_diff measured was " << max_color_diff
      << "\nActual: " << cc::GetPNGDataUrl(actual);
}

// Tests a GOOD quality scaling of 1280x720 → 1024x700.
TEST_F(GLScalerPixelTest, ScalesALittleAtGoodQuality) {
  GLScaler::Parameters params;
  params.scale_from = gfx::Vector2d(1280, 720);
  params.scale_to = gfx::Vector2d(1024, 700);
  params.quality = GLScaler::Parameters::Quality::GOOD;
  params.is_flipped_source = false;
  ASSERT_TRUE(scaler()->Configure(params));
  EXPECT_EQ(u8"Output ← {BILINEAR2X2/lowp [1280 720] to [1024 700]} ← Source",
            GetScalerString());
  constexpr gfx::Size kSourceSize = gfx::Size(1280, 720);
  const SkBitmap source = CreateSMPTETestImage(kSourceSize);
  const SkBitmap actual =
      Scale(source, gfx::Vector2d(), gfx::Rect(0, 0, 1024, 700));
  int max_color_diff = GetBaselineColorDifference();
  EXPECT_TRUE(LooksLikeSMPTETestImage(
      actual, kSourceSize, gfx::Rect(kSourceSize), 2, &max_color_diff))
      << "max_color_diff measured was " << max_color_diff
      << "\nActual: " << cc::GetPNGDataUrl(actual);
}

// Tests a large, skewed reduction at GOOD quality: 3840x720 → 128x256.
TEST_F(GLScalerPixelTest, ScalesALotHorizontallyAtGoodQuality) {
  GLScaler::Parameters params;
  params.scale_from = gfx::Vector2d(3840, 720);
  params.scale_to = gfx::Vector2d(128, 256);
  params.quality = GLScaler::Parameters::Quality::GOOD;
  params.is_flipped_source = false;
  ASSERT_TRUE(scaler()->Configure(params));
  EXPECT_EQ(
      u8"Output "
      u8"← {BILINEAR/lowp [256 256] to [128 256]} "
      u8"← {BILINEAR4/lowp [2048 512] to [256 256]} "
      u8"← {BILINEAR2X2/lowp [3840 720] to [2048 512]} "
      u8"← Source",
      GetScalerString());
  constexpr gfx::Size kSourceSize = gfx::Size(3840, 720);
  const SkBitmap source = CreateSMPTETestImage(kSourceSize);
  const SkBitmap actual =
      Scale(source, gfx::Vector2d(), gfx::Rect(0, 0, 128, 256));
  int max_color_diff = GetBaselineColorDifference();
  EXPECT_TRUE(LooksLikeSMPTETestImage(
      actual, kSourceSize, gfx::Rect(kSourceSize), 2, &max_color_diff))
      << "max_color_diff measured was " << max_color_diff
      << "\nActual: " << cc::GetPNGDataUrl(actual);
}

// Tests a large, skewed reduction at GOOD quality: 640x2160 → 256x128.
TEST_F(GLScalerPixelTest, ScalesALotVerticallyAtGoodQuality) {
  GLScaler::Parameters params;
  params.scale_from = gfx::Vector2d(640, 2160);
  params.scale_to = gfx::Vector2d(256, 128);
  params.quality = GLScaler::Parameters::Quality::GOOD;
  params.is_flipped_source = false;
  ASSERT_TRUE(scaler()->Configure(params));
  EXPECT_EQ(
      u8"Output "
      u8"← {BILINEAR/lowp [256 256] to [256 128]} "
      u8"← {BILINEAR4/lowp [512 2048] to [256 256]} "
      u8"← {BILINEAR2X2/lowp [640 2160] to [512 2048]} "
      u8"← Source",
      GetScalerString());
  constexpr gfx::Size kSourceSize = gfx::Size(640, 2160);
  const SkBitmap source = CreateSMPTETestImage(kSourceSize);
  const SkBitmap actual =
      Scale(source, gfx::Vector2d(), gfx::Rect(0, 0, 256, 128));
  int max_color_diff = GetBaselineColorDifference();
  EXPECT_TRUE(LooksLikeSMPTETestImage(
      actual, kSourceSize, gfx::Rect(kSourceSize), 2, &max_color_diff))
      << "max_color_diff measured was " << max_color_diff
      << "\nActual: " << cc::GetPNGDataUrl(actual);
}

// Tests a BEST quality scaling of 1280x720 → 1024x700.
TEST_F(GLScalerPixelTest, ScalesAtBestQuality) {
  GLScaler::Parameters params;
  params.scale_from = gfx::Vector2d(1280, 720);
  params.scale_to = gfx::Vector2d(1024, 700);
  params.quality = GLScaler::Parameters::Quality::BEST;
  params.is_flipped_source = false;
  ASSERT_TRUE(scaler()->Configure(params));
  EXPECT_EQ(
      u8"Output "
      u8"← {BICUBIC_HALF_1D/lowp [2048 700] to [1024 700]} "
      u8"← {BICUBIC_UPSCALE/lowp [1280 700] to [2048 700]} "
      u8"← {BICUBIC_HALF_1D/lowp [1280 1400] to [1280 700]} "
      u8"← {BICUBIC_UPSCALE/lowp [1280 720] to [1280 1400]} "
      u8"← Source",
      GetScalerString());
  constexpr gfx::Size kSourceSize = gfx::Size(1280, 720);
  const SkBitmap source = CreateSMPTETestImage(kSourceSize);
  const SkBitmap actual =
      Scale(source, gfx::Vector2d(), gfx::Rect(0, 0, 1024, 700));
  int max_color_diff = GetBaselineColorDifference();
  EXPECT_TRUE(LooksLikeSMPTETestImage(
      actual, kSourceSize, gfx::Rect(kSourceSize), 4, &max_color_diff))
      << "max_color_diff measured was " << max_color_diff
      << "\nActual: " << cc::GetPNGDataUrl(actual);
}

// Tests that a source offset can be provided to sample the source starting at a
// different location.
TEST_F(GLScalerPixelTest, TranslatesWithSourceOffset) {
  // Disabled on Marshmallow. See crbug.com/933080
  if (IsAndroidMarshmallow())
    return;

  GLScaler::Parameters params;
  params.is_flipped_source = false;
  ASSERT_TRUE(scaler()->Configure(params));
  EXPECT_EQ(u8"Output ← {BILINEAR/lowp copy} ← Source", GetScalerString());
  const SkBitmap source = CreateSMPTETestImage(kSMPTEFullSize);
  static_assert(kSMPTEFullSize.width() % 2 == 0, "Fix kSMPTEFullSize.");
  static_assert(kSMPTEFullSize.height() % 4 == 0, "Fix kSMPTEFullSize.");
  const gfx::Vector2d offset(kSMPTEFullSize.width() / 2,
                             kSMPTEFullSize.height() / 4);
  const gfx::Rect src_rect(offset.x(), offset.y(),
                           kSMPTEFullSize.width() - offset.x(),
                           kSMPTEFullSize.height() - offset.y());
  const gfx::Rect output_rect(0, 0, kSMPTEFullSize.width() - offset.x(),
                              kSMPTEFullSize.height() - offset.y());
  const SkBitmap actual = Scale(source, offset, output_rect);
  int max_color_diff = GetBaselineColorDifference();
  EXPECT_TRUE(LooksLikeSMPTETestImage(actual, kSMPTEFullSize, src_rect, 0,
                                      &max_color_diff))
      << "max_color_diff measured was " << max_color_diff
      << "\nActual: " << cc::GetPNGDataUrl(actual);
}

// Tests that the source offset works when the source content is vertically
// flipped.
TEST_F(GLScalerPixelTest, TranslatesVerticallyFlippedSourceWithSourceOffset) {
  // Disabled on Marshmallow. See crbug.com/933080
  if (IsAndroidMarshmallow())
    return;

  GLScaler::Parameters params;
  params.is_flipped_source = true;
  ASSERT_TRUE(scaler()->Configure(params));
  EXPECT_EQ(u8"Output ← {BILINEAR/lowp copy} ← Source", GetScalerString());
  const SkBitmap flipped_source =
      CreateVerticallyFlippedBitmap(CreateSMPTETestImage(kSMPTEFullSize));
  const gfx::Vector2d offset(kSMPTEFullSize.width() / 2,
                             kSMPTEFullSize.height() / 4);
  const gfx::Rect src_rect(offset.x(), offset.y(),
                           kSMPTEFullSize.width() - offset.x(),
                           kSMPTEFullSize.height() - offset.y());
  const gfx::Rect output_rect(0, 0, kSMPTEFullSize.width() - offset.x(),
                              kSMPTEFullSize.height() - offset.y());
  const SkBitmap flipped_back_actual =
      CreateVerticallyFlippedBitmap(Scale(flipped_source, offset, output_rect));
  int max_color_diff = GetBaselineColorDifference();
  EXPECT_TRUE(LooksLikeSMPTETestImage(flipped_back_actual, kSMPTEFullSize,
                                      src_rect, 0, &max_color_diff))
      << "max_color_diff measured was " << max_color_diff
      << "\nActual (flipped-back): " << cc::GetPNGDataUrl(flipped_back_actual);
}

// Tests that the correct source selection is made when both translating the
// source and then scaling. Scale "from" and "to" values are chosen such that a
// multi-stage scaler will be configured (to test that offsets are correcty
// calculated and passed between multiple stages).
TEST_F(GLScalerPixelTest, ScalesWithTranslatedSourceOffset) {
  GLScaler::Parameters params;
  params.scale_from = gfx::Vector2d(640, 2160);
  params.scale_to = gfx::Vector2d(256, 128);
  params.quality = GLScaler::Parameters::Quality::GOOD;
  params.is_flipped_source = false;
  ASSERT_TRUE(scaler()->Configure(params));
  EXPECT_EQ(
      u8"Output "
      u8"← {BILINEAR/lowp [256 256] to [256 128]} "
      u8"← {BILINEAR4/lowp [512 2048] to [256 256]} "
      u8"← {BILINEAR2X2/lowp [640 2160] to [512 2048]} "
      u8"← Source",
      GetScalerString());
  constexpr gfx::Size kSourceSize = gfx::Size(640, 2160);
  const SkBitmap source = CreateSMPTETestImage(kSourceSize);
  const gfx::Vector2d offset(kSourceSize.width() / 2, kSourceSize.height() / 4);
  const gfx::Rect output_rect(0, 0, 128, 64);
  const SkBitmap actual = Scale(source, offset, output_rect);
  const gfx::Rect expected_copy_rect(
      offset.x(), offset.y(),
      output_rect.width() * params.scale_from.x() / params.scale_to.x(),
      output_rect.height() * params.scale_from.y() / params.scale_to.y());
  int max_color_diff = GetBaselineColorDifference();
  EXPECT_TRUE(LooksLikeSMPTETestImage(actual, kSourceSize, expected_copy_rect,
                                      2, &max_color_diff))
      << "max_color_diff measured was " << max_color_diff
      << "\nExpected crop region of source: " << expected_copy_rect.ToString()
      << "\nFull (uncropped) Source: " << cc::GetPNGDataUrl(source)
      << "\nActual: " << cc::GetPNGDataUrl(actual);
}

// Tests that the output is vertically flipped, if requested in the parameters.
TEST_F(GLScalerPixelTest, VerticallyFlipsOutput) {
  // Disabled on Marshmallow. See crbug.com/933080
  if (IsAndroidMarshmallow())
    return;

  GLScaler::Parameters params;
  params.is_flipped_source = false;
  params.flip_output = true;
  ASSERT_TRUE(scaler()->Configure(params));
  EXPECT_EQ(u8"Output ← {BILINEAR/lowp+flip_y copy} ← Source",
            GetScalerString());
  const SkBitmap source = CreateSMPTETestImage(kSMPTEFullSize);
  const SkBitmap flipped_back_actual = CreateVerticallyFlippedBitmap(
      Scale(source, gfx::Vector2d(), gfx::Rect(kSMPTEFullSize)));
  int max_color_diff = GetBaselineColorDifference();
  EXPECT_TRUE(LooksLikeSMPTETestImage(flipped_back_actual, kSMPTEFullSize,
                                      gfx::Rect(kSMPTEFullSize), 0,
                                      &max_color_diff))
      << "max_color_diff measured was " << max_color_diff
      << "\nActual (flipped-back): " << cc::GetPNGDataUrl(flipped_back_actual);
}

// Tests that the single-channel export ScalerStage works by executing a red
// channel export.
TEST_F(GLScalerPixelTest, ExportsTheRedColorChannel) {
  // Disabled on Marshmallow. See crbug.com/933080
  if (IsAndroidMarshmallow())
    return;

  GLScaler::Parameters params;
  params.is_flipped_source = false;
  params.export_format = GLScaler::Parameters::ExportFormat::CHANNEL_0;
  ASSERT_TRUE(scaler()->Configure(params));
  EXPECT_EQ(u8"Output ← {PLANAR_CHANNEL_0/lowp [4 1] to [1 1]} ← Source",
            GetScalerString());
  const SkBitmap source = CreateSMPTETestImage(kSMPTEFullSize);
  const SkBitmap expected = CreatePackedPlanarBitmap(source, 0);
  const gfx::Size output_size(expected.width(), expected.height());
  const SkBitmap actual =
      Scale(source, gfx::Vector2d(), gfx::Rect(output_size));
  constexpr float kAvgAbsoluteErrorLimit = 1.f;
  constexpr int kMaxAbsoluteErrorLimit = 2;
  EXPECT_TRUE(cc::FuzzyPixelComparator(
                  false, 100.f, 0.f,
                  GetBaselineColorDifference() + kAvgAbsoluteErrorLimit,
                  GetBaselineColorDifference() + kMaxAbsoluteErrorLimit, 0)
                  .Compare(expected, actual))
      << "\nActual: " << cc::GetPNGDataUrl(actual)
      << "\Expected: " << cc::GetPNGDataUrl(expected);
}

// A test that also stands as an example for how to use the GLScaler to scale a
// screen-sized RGB source (2160x1440, 16:10 aspect ratio) to a typical video
// resolution (720p, 16:9). The end-goal is to produce three textures, which
// contain the three YUV planes in I420 format.
//
// This is a two step process: First, the source is scaled and color space
// converted, with the final result exported as NV61 format (a full size luma
// plane + a half-width interleaved UV image). Second, the interleaved UV image
// is scaled by half in the vertical direction and then separated into one U and
// one V plane.
TEST_F(GLScalerPixelTest, Example_ScaleAndExportForScreenVideoCapture) {
  if (scaler()->GetMaxDrawBuffersSupported() < 2) {
    LOG(WARNING) << "Skipping test due to lack of MRT support.";
    return;
  }

  // Step 1: Produce a scaled NV61-format result.
  GLScaler::Parameters params;
  params.scale_from = gfx::Vector2d(2160, 1440);
  params.scale_to = gfx::Vector2d(1280, 720);
  params.source_color_space = DefaultRGBColorSpace();
  params.output_color_space = DefaultYUVColorSpace();
  params.enable_precise_color_management = true;
  params.quality = GLScaler::Parameters::Quality::GOOD;
  params.is_flipped_source = true;
  params.flip_output = true;
  params.export_format = GLScaler::Parameters::ExportFormat::NV61;
  params.swizzle[0] = GL_BGRA_EXT;  // Swizzle for readback.
  params.swizzle[1] = GL_RGBA;      // Don't swizzle output for Step 2.
  ASSERT_TRUE(scaler()->Configure(params));
  EXPECT_STRING_MATCHES(
      u8"Output "
      u8"← {I422_NV61_MRT/mediump [5120 720] to [1280 720], with color x-form "
      u8"to *BT709*, with swizzle(0)} "
      u8"← {BILINEAR2/mediump [2160 1440] to [1280 720]} "
      u8"← {BILINEAR/mediump+flip_y copy, with color x-form *BT709* to "
      u8"*transfer:1.0000\\*x*} "
      u8"← Source",
      GetScalerString());

  constexpr gfx::Size kSourceSize = gfx::Size(2160, 1440);
  const GLuint src_texture = UploadTexture(
      CreateVerticallyFlippedBitmap(CreateSMPTETestImage(kSourceSize)));
  constexpr gfx::Size kOutputSize = gfx::Size(1280, 720);
  SkBitmap expected = CreateSMPTETestImage(kOutputSize);
  ConvertBitmapToYUV(&expected);

  // While the output size is 1280x720, the packing of 4 pixels into one RGBA
  // quad means that the texture width must be divided by 4, and that size
  // passed in the output_rect argument in the call to ScaleToMultipleOutputs().
  const gfx::Size y_plane_size(kOutputSize.width() / 4, kOutputSize.height());
  const GLuint y_plane_texture = CreateTexture(y_plane_size);
  const GLuint uv_interleaved_texture = CreateTexture(y_plane_size);

  ASSERT_TRUE(scaler()->ScaleToMultipleOutputs(
      src_texture, kSourceSize, gfx::Vector2d(), y_plane_texture,
      uv_interleaved_texture, gfx::Rect(y_plane_size)));

  // Step 2: Run the scaler again with the deinterleaver exporter, to produce
  // the I420 U and V planes from the NV61 UV interleaved image.
  params = GLScaler::Parameters();  // Reset params.
  params.scale_from = gfx::Vector2d(1, 2);
  params.scale_to = gfx::Vector2d(1, 1);
  params.source_color_space = DefaultYUVColorSpace();
  params.quality = GLScaler::Parameters::Quality::GOOD;
  params.is_flipped_source = false;  // Output was already flipped in Step 1.
  params.export_format =
      GLScaler::Parameters::ExportFormat::DEINTERLEAVE_PAIRWISE;
  params.swizzle[0] = GL_BGRA_EXT;  // Swizzle for readback.
  params.swizzle[1] = GL_BGRA_EXT;  // Swizzle for readback.
  ASSERT_TRUE(scaler()->Configure(params));
  EXPECT_EQ(
      u8"Output "
      u8"← {DEINTERLEAVE_PAIRWISE_MRT/lowp [2 2] to [1 1], with swizzle(0), "
      u8"with swizzle(1)} "
      u8"← Source",
      GetScalerString());

  const gfx::Size uv_plane_size(y_plane_size.width() / 2,
                                y_plane_size.height() / 2);
  const GLuint u_plane_texture = CreateTexture(uv_plane_size);
  const GLuint v_plane_texture = CreateTexture(uv_plane_size);
  ASSERT_TRUE(scaler()->ScaleToMultipleOutputs(
      uv_interleaved_texture, y_plane_size, gfx::Vector2d(), u_plane_texture,
      v_plane_texture, gfx::Rect(uv_plane_size)));

  // Download the textures, and unpack them into an interleaved YUV bitmap, for
  // comparison against the |expected| rendition.
  SkBitmap actual = AllocateRGBABitmap(kOutputSize);
  actual.eraseColor(SkColorSetARGB(0xff, 0x00, 0x80, 0x80));
  SkBitmap y_plane = DownloadTexture(y_plane_texture, y_plane_size);
  SwizzleBitmap(&y_plane);
  UnpackPlanarBitmap(y_plane, 0, &actual);
  SkBitmap u_plane = DownloadTexture(u_plane_texture, uv_plane_size);
  SwizzleBitmap(&u_plane);
  UnpackPlanarBitmap(u_plane, 1, &actual);
  SkBitmap v_plane = DownloadTexture(v_plane_texture, uv_plane_size);
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

// Performs a scaling-with-gamma-correction experiment to test GLScaler's
// "precise color management" feature. A 50% scale is executed on the same
// source image, once with color management turned on, and once with it turned
// off. The results, each of which should be different, are then examined.
TEST_F(GLScalerPixelTest, ScalesWithColorManagement) {
  if (!scaler()->SupportsPreciseColorManagement()) {
    LOG(WARNING) << "Skipping test due to lack of 16-bit float support.";
    return;
  }

  // An image of a raspberry (source:
  // https://commons.wikimedia.org/wiki/File:Framboise_Margy_3.jpg) has been
  // transformed in such a way that scaling it by half in both directions will
  // reveal whether scaling is occurring on linearized color values. When scaled
  // correctly, the output image should contain a visible raspberry blended
  // heavily with solid gray. However, if done naively, the output will be a
  // solid 50% gray. For details, see: http://www.ericbrasseur.org/gamma.html
  //
  // Note that the |source| and |expected| images both use the sRGB color space.
  const SkBitmap source = LoadPNGTestImage("rasp-grayator.png");
  ASSERT_FALSE(source.isNull());
  const SkBitmap expected = LoadPNGTestImage("rasp-grayator-half.png");
  ASSERT_FALSE(expected.isNull());
  const gfx::Size output_size =
      gfx::Size(source.width() / 2, source.height() / 2);
  ASSERT_EQ(gfx::Size(expected.width(), expected.height()), output_size);
  const SkBitmap expected_naive = AllocateRGBABitmap(output_size);
  expected_naive.eraseColor(SkColorSetARGB(0xff, 0x7f, 0x7f, 0x7f));

  // Scale the right way: With color management enabled, the raspberry should be
  // visible in the downscaled result.
  GLScaler::Parameters params;
  params.scale_from = gfx::Vector2d(2, 2);
  params.scale_to = gfx::Vector2d(1, 1);
  params.source_color_space = gfx::ColorSpace::CreateSRGB();
  params.enable_precise_color_management = true;
  params.quality = GLScaler::Parameters::Quality::GOOD;
  params.is_flipped_source = false;
  ASSERT_TRUE(scaler()->Configure(params));
  EXPECT_STRING_MATCHES(
      u8"Output "
      u8"← {BILINEAR/mediump [2 2] to [1 1], with color x-form to *BT709*} "
      u8"← {BILINEAR/mediump copy, with color x-form *BT709* to "
      u8"*transfer:1.0000\\*x*} "
      u8"← Source",
      GetScalerString());
  const SkBitmap actual =
      Scale(source, gfx::Vector2d(), gfx::Rect(output_size));
  constexpr float kAvgAbsoluteErrorLimit = 1.f;
  constexpr int kMaxAbsoluteErrorLimit = 2;
  EXPECT_TRUE(cc::FuzzyPixelComparator(
                  false, 100.f, 0.f,
                  GetBaselineColorDifference() + kAvgAbsoluteErrorLimit,
                  GetBaselineColorDifference() + kMaxAbsoluteErrorLimit, 0)
                  .Compare(expected, actual))
      << "\nActual: " << cc::GetPNGDataUrl(actual)
      << "\nExpected (half size): " << cc::GetPNGDataUrl(expected)
      << "\nOriginal: " << cc::GetPNGDataUrl(source);

  // Scale the naive way: Without color management, expect a solid gray result.
  params.enable_precise_color_management = false;
  ASSERT_TRUE(scaler()->Configure(params));
  EXPECT_EQ(u8"Output ← {BILINEAR/lowp [2 2] to [1 1]} ← Source",
            GetScalerString());
  const SkBitmap actual_naive =
      Scale(source, gfx::Vector2d(), gfx::Rect(output_size));
  EXPECT_TRUE(cc::FuzzyPixelComparator(
                  false, 100.f, 0.f,
                  GetBaselineColorDifference() + kAvgAbsoluteErrorLimit,
                  GetBaselineColorDifference() + kMaxAbsoluteErrorLimit, 0)
                  .Compare(expected_naive, actual_naive))
      << "\nActual: " << cc::GetPNGDataUrl(actual_naive)
      << "\nExpected (half size): " << cc::GetPNGDataUrl(expected_naive)
      << "\nOriginal: " << cc::GetPNGDataUrl(source);
}

#undef EXPECT_STRING_MATCHES

}  // namespace viz
