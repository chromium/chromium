// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gl_scaler.h"

#include <sstream>
#include <tuple>
#include <vector>

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
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/color_transform.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace viz {

namespace {

// Base size of test images to be operated upon. Both dimensions must be
// divisible by 2, 3, or 4.
constexpr gfx::Size kBaseSize = gfx::Size(16 * 4 * 3, 9 * 4 * 3);

}  // namespace

class GLScalerShaderPixelTest
    : public cc::PixelTest,
      public testing::WithParamInterface<std::tuple<bool, bool>>,
      public GLScalerTestUtil {
 public:
  using Axis = GLScaler::Axis;
  using Shader = GLScaler::Shader;
  using ShaderProgram = GLScaler::ShaderProgram;

  GLScalerShaderPixelTest()
      : scoped_trace_(
            __FILE__,
            __LINE__,
            (testing::Message()
             << "is_converting_rgb_to_yuv=" << is_converting_rgb_to_yuv()
             << ", is_swizzling_output=" << is_swizzling_output())) {}

  bool is_converting_rgb_to_yuv() const { return std::get<0>(GetParam()); }
  bool is_swizzling_output() const { return std::get<1>(GetParam()); }

  bool AreMultipleRenderingTargetsSupported() const {
    return scaler_->GetMaxDrawBuffersSupported() > 1;
  }

  // Returns a cached ShaderProgram, maybe configured to convert RGB→YUV and/or
  // swizzle the 1st and 3rd bytes in the output (depending on GetParams()).
  ShaderProgram* GetShaderProgram(Shader shader) {
    std::unique_ptr<gfx::ColorTransform> transform;
    if (is_converting_rgb_to_yuv()) {
      transform = gfx::ColorTransform::NewColorTransform(
          DefaultRGBColorSpace(), DefaultYUVColorSpace(),
          gfx::ColorTransform::Intent::INTENT_ABSOLUTE);
    }
    const GLenum swizzle[2] = {
        is_swizzling_output() ? GL_BGRA_EXT : GL_RGBA,
        is_swizzling_output() ? GL_BGRA_EXT : GL_RGBA,
    };
    return scaler_->GetShaderProgram(shader, GL_UNSIGNED_BYTE, transform.get(),
                                     swizzle);
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

  GLuint RenderToNewTexture(GLuint src_texture, const gfx::Size& size) {
    return RenderToNewTextures(src_texture, size, false).first;
  }

  // Using the current shader program, creates new texture(s) of the given
  // |size| and draws using |src_texture| as input. If |dual_outputs| is true,
  // two new textures are created and drawn-to simultaneously; otherwise, only
  // one is created and drawn-to. The caller does not take ownership of the new
  // texture(s).
  std::pair<GLuint, GLuint> RenderToNewTextures(GLuint src_texture,
                                                const gfx::Size& size,
                                                bool dual_outputs) {
    std::pair<GLuint, GLuint> dst_textures(
        CreateTexture(size), dual_outputs ? CreateTexture(size) : 0u);
    GLuint framebuffer = 0;
    gl_->GenFramebuffers(1, &framebuffer);
    gl_->BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, dst_textures.first, 0);
    if (dual_outputs) {
      gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + 1,
                                GL_TEXTURE_2D, dst_textures.second, 0);
    }

    gl_->BindTexture(GL_TEXTURE_2D, src_texture);

    gl_->Viewport(0, 0, size.width(), size.height());
    const GLenum buffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT0 + 1};
    gl_->DrawBuffersEXT(dual_outputs ? 2 : 1, buffers);
    // Assumption: The |vertex_attributes_buffer_| created in SetUp() is
    // currently bound to GL_ARRAY_BUFFER.
    gl_->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    gl_->DeleteFramebuffers(1, &framebuffer);

    return dst_textures;
  }

  // Returns a texture that converts the input |texture| back to unswizzled RGB,
  // if necessary, depending on GetParam(). The caller does not take ownership
  // of the returned texture, which could be the same texture as the input
  // argument in some cases.
  GLuint ConvertBackToUnswizzledRGB(GLuint texture, const gfx::Size& size) {
    GLuint result = texture;
    if (is_swizzling_output()) {
      const GLenum swizzle[2] = {GL_BGRA_EXT, GL_BGRA_EXT};
      scaler_
          ->GetShaderProgram(Shader::BILINEAR, GL_UNSIGNED_BYTE, nullptr,
                             swizzle)
          ->UseProgram(size, gfx::RectF(gfx::Rect(size)), size,
                       Axis::HORIZONTAL, false);
      result = RenderToNewTexture(result, size);
    }
    if (is_converting_rgb_to_yuv()) {
      const auto transform = gfx::ColorTransform::NewColorTransform(
          DefaultYUVColorSpace(), DefaultRGBColorSpace(),
          gfx::ColorTransform::Intent::INTENT_ABSOLUTE);
      const GLenum swizzle[2] = {GL_RGBA, GL_RGBA};
      scaler_
          ->GetShaderProgram(Shader::BILINEAR, GL_UNSIGNED_BYTE,
                             transform.get(), swizzle)
          ->UseProgram(size, gfx::RectF(gfx::Rect(size)), size,
                       Axis::HORIZONTAL, false);
      result = RenderToNewTexture(result, size);
    }
    return result;
  }

  // A test case executed by RunSMPTEScalingTestCases().
  struct SMPTEScalingTestCase {
    gfx::Rect src_rect;    // Selects a subrect of the source.
    int scale_from;        // Scale ratio denominator.
    int scale_to;          // Scale ratio numerator.
    int fuzzy_bar_border;  // Ignored pixels between color bars, when comparing.
  };

  // Draws with the given shader for each of the provided |test_cases| and adds
  // gtest failure(s) if the output does not look like a part of the SMPTE test
  // image.
  void RunSMPTEScalingTestCases(
      Shader shader,
      const std::vector<SMPTEScalingTestCase>& test_cases) {
    const SkBitmap source = CreateSMPTETestImage(kBaseSize);
    const GLuint src_texture = UploadTexture(source);

    for (const auto& tc : test_cases) {
      for (int is_horizontal = 0; is_horizontal <= 1; ++is_horizontal) {
        gfx::Size dst_size = tc.src_rect.size();
        Axis axis;
        if (is_horizontal) {
          CHECK_EQ((dst_size.width() * tc.scale_to) % tc.scale_from, 0);
          dst_size.set_width(dst_size.width() * tc.scale_to / tc.scale_from);
          axis = Axis::HORIZONTAL;
        } else {
          CHECK_EQ((dst_size.height() * tc.scale_to) % tc.scale_from, 0);
          dst_size.set_height(dst_size.height() * tc.scale_to / tc.scale_from);
          axis = Axis::VERTICAL;
        }

        SCOPED_TRACE(testing::Message()
                     << "src_rect=" << tc.src_rect.ToString()
                     << ", scale from→to=" << tc.scale_from << "→"
                     << tc.scale_to << ", dst_size=" << dst_size.ToString());

        GetShaderProgram(shader)->UseProgram(kBaseSize, gfx::RectF(tc.src_rect),
                                             dst_size, axis, false);
        const SkBitmap actual = DownloadTexture(
            ConvertBackToUnswizzledRGB(
                RenderToNewTexture(src_texture, dst_size), dst_size),
            dst_size);
        int max_color_diff = GetMaxAllowedColorDifference();
        if (!LooksLikeSMPTETestImage(actual, kBaseSize, tc.src_rect,
                                     tc.fuzzy_bar_border, &max_color_diff)) {
          ADD_FAILURE() << "Scaled image does not look like the correct scaled "
                           "subrect of the SMPTE test image (max diff measured="
                        << max_color_diff
                        << "):\nActual: " << cc::GetPNGDataUrl(actual);
        }
      }
    }
  }

  // Adds test failures if an |actual| image does not match the |expected|
  // image. When not doing color space conversion, the images must match
  // exactly; otherwise, some minor differences are allowed.
  void ExpectAreTheSameImage(const SkBitmap& expected,
                             const SkBitmap& actual) const {
    const int max_color_diff = GetMaxAllowedColorDifference();
    if (!cc::FuzzyPixelComparator(false, 100.0f, 0.0f, max_color_diff,
                                  max_color_diff, 0)
             .Compare(expected, actual)) {
      ADD_FAILURE() << "Images are not similar enough (max_color_diff="
                    << max_color_diff
                    << "):\nExpected: " << cc::GetPNGDataUrl(expected)
                    << "\nActual: " << cc::GetPNGDataUrl(actual);
    }
  }

  // Draws with the given shader to downscale a "striped pattern" image by
  // |downscale_factor| in one dimension only, and adds gtest failure(s) if the
  // resulting image is not of the |expected_solid_color|. |cycle| specifies the
  // colors of the stripes, which should average to |expected_solid_color|.
  //
  // If the shader program is correct, it should be sampling the texture halfway
  // between each pair of stripes and then averaging the result. This means that
  // every N pixels in the source will be averaged to one pixel in the output,
  // creating a solid color fill as output. If the shader is sampling the
  // texture at the wrong points, the result will be tinted and/or contain
  // striping.
  void RunMultiplePassBilinearTest(Shader shader,
                                   int downscale_factor,
                                   const std::vector<SkColor>& cycle) {
    // Compute the expected solid fill color from the colors in |cycle|.
    uint32_t sum_red = 0;
    uint32_t sum_green = 0;
    uint32_t sum_blue = 0;
    uint32_t sum_alpha = 0;
    for (SkColor c : cycle) {
      sum_red += SkColorGetR(c);
      sum_green += SkColorGetG(c);
      sum_blue += SkColorGetB(c);
      sum_alpha += SkColorGetA(c);
    }
    const float count = cycle.size();
    // Note: Taking the rounded average for each color channel.
    const SkColor expected_solid_color =
        SkColorSetARGB(sum_alpha / count + 0.5f, sum_red / count + 0.5f,
                       sum_green / count + 0.5f, sum_blue / count + 0.5f);

    // Run the test for the vertical direction, and again for the horizontal
    // direction.
    const gfx::Rect src_rect =
        gfx::Rect(0, 0, 10 * downscale_factor, 10 * downscale_factor);
    for (int is_horizontal = 0; is_horizontal <= 1; ++is_horizontal) {
      gfx::Size dst_size = src_rect.size();
      Axis axis;
      CyclicalPattern pattern;
      if (is_horizontal) {
        dst_size.set_width(dst_size.width() / downscale_factor);
        axis = Axis::HORIZONTAL;
        pattern = VERTICAL_STRIPES;
      } else {
        dst_size.set_height(dst_size.height() / downscale_factor);
        axis = Axis::VERTICAL;
        pattern = HORIZONTAL_STRIPES;
      }

      // Create the expected output image consisting of a solid fill color.
      SkBitmap expected = AllocateRGBABitmap(dst_size);
      expected.eraseColor(expected_solid_color);

      // Run the test for each of N possible rotations of the |cycle| of
      // stripes.
      for (size_t rotation = 0; rotation < cycle.size(); ++rotation) {
        SCOPED_TRACE(testing::Message() << "is_horizontal=" << !!is_horizontal
                                        << ", rotation=" << rotation
                                        << ", expected_solid_color=" << std::hex
                                        << expected_solid_color);

        const SkBitmap source =
            CreateCyclicalTestImage(src_rect.size(), pattern, cycle, rotation);
        const GLuint src_texture = UploadTexture(source);

        // Execute the program, and convert the shader program's drawn result
        // back to an unswizzled RGB form, and compare that with the expected
        // image.
        GetShaderProgram(shader)->UseProgram(
            src_rect.size(), gfx::RectF(src_rect), dst_size, axis, false);
        const SkBitmap actual = DownloadTexture(
            ConvertBackToUnswizzledRGB(
                RenderToNewTexture(src_texture, dst_size), dst_size),
            dst_size);
        ExpectAreTheSameImage(expected, actual);
      }
    }
  }

 protected:
  void SetUp() final {
    cc::PixelTest::SetUpGLWithoutRenderer(gfx::SurfaceOrigin::kBottomLeft);

    scaler_ = std::make_unique<GLScaler>(context_provider());
    gl_ = context_provider()->ContextGL();
    CHECK(gl_);

    // Set up vertex attributes buffer and its data.
    gl_->GenBuffers(1, &vertex_attributes_buffer_);
    gl_->BindBuffer(GL_ARRAY_BUFFER, vertex_attributes_buffer_);
    gl_->BufferData(GL_ARRAY_BUFFER, sizeof(ShaderProgram::kVertexAttributes),
                    ShaderProgram::kVertexAttributes, GL_STATIC_DRAW);

    texture_helper_ = std::make_unique<GLScalerTestTextureHelper>(gl_);
  }

  void TearDown() final {
    texture_helper_.reset();

    if (vertex_attributes_buffer_) {
      gl_->DeleteBuffers(1, &vertex_attributes_buffer_);
      vertex_attributes_buffer_ = 0;
    }

    gl_ = nullptr;
    scaler_.reset();

    cc::PixelTest::TearDown();
  }

  // Returns the maximum allowed absolute difference between any two color
  // values in the expected vs actual image comparisons, given the current test
  // parameters and known platform-specific inaccuracy.
  int GetMaxAllowedColorDifference() const {
#if defined(OS_ANDROID)
    // Android seems to have texture sampling and/or readback accuracy issues
    // with these programs that are not at all seen on any of the desktop
    // platforms. Also, versions before Marshmallow seem to have a much larger
    // accuracy issues with a few of the programs. Thus, use higher thresholds,
    // assuming that the programs are correct if they can pass a much lower
    // threshold on other platforms.
    if (base::android::BuildInfo::GetInstance()->sdk_int() <
        base::android::SDK_VERSION_MARSHMALLOW) {
      return (is_converting_rgb_to_yuv() || is_swizzling_output()) ? 24 : 12;
    }
    return (is_converting_rgb_to_yuv() || is_swizzling_output()) ? 4 : 2;
#else
    return (is_converting_rgb_to_yuv() || is_swizzling_output()) ? 2 : 0;
#endif
  }

  bool IsAndroidMarshmallow() {
#if defined(OS_ANDROID)
    return base::android::BuildInfo::GetInstance()->sdk_int() ==
           base::android::SDK_VERSION_MARSHMALLOW;
#else
    return false;
#endif
  }

  testing::ScopedTrace scoped_trace_;
  std::unique_ptr<GLScaler> scaler_;
  gpu::gles2::GLES2Interface* gl_ = nullptr;
  GLuint vertex_attributes_buffer_ = 0;
  std::unique_ptr<GLScalerTestTextureHelper> texture_helper_;
};

// As the BILINEAR shader is used by some of the test helpers, this test is
// necessary to ensure the correctness of the tools used by all the other tests.
TEST_P(GLScalerShaderPixelTest, ValidateTestHelpers) {
  // Disabled on Marshmallow. See crbug.com/933080
  if (IsAndroidMarshmallow())
    return;

  // Create/validate a SMPTE color bar test image.
  const SkBitmap original = CreateSMPTETestImage(kBaseSize);
  int max_color_diff = GetMaxAllowedColorDifference();
  ASSERT_TRUE(LooksLikeSMPTETestImage(original, kBaseSize, gfx::Rect(kBaseSize),
                                      0, &max_color_diff))
      << "max diff measured=" << max_color_diff;

  // Create and upload a test image that has had RGB→YUV conversion performed
  // and/or had its color channels swizzled, depending on the testing params.
  SkBitmap image = CreateSMPTETestImage(kBaseSize);
  if (is_converting_rgb_to_yuv()) {
    ConvertBitmapToYUV(&image);
  }
  if (is_swizzling_output()) {
    SwizzleBitmap(&image);
  }
  const GLuint uploaded_texture = UploadTexture(image);

  // Use the convert-back helper, which uses the BILINEAR shader to convert the
  // |uploaded_texture| back to an unswizzled RGB form. Then, download the
  // result and check whether it matches the original.
  const gfx::Size size(image.width(), image.height());
  const GLuint converted_back_texture =
      ConvertBackToUnswizzledRGB(uploaded_texture, size);
  const SkBitmap actual = DownloadTexture(converted_back_texture, size);
  ExpectAreTheSameImage(original, actual);
}

// Tests the default, one-pass bilinear shader which can upscale or downscale by
// up to 2X.
TEST_P(GLScalerShaderPixelTest, Bilinear) {
  // Disabled on Marshmallow. See crbug.com/933080
  if (IsAndroidMarshmallow())
    return;

  constexpr gfx::Rect whole = gfx::Rect(kBaseSize);
  constexpr gfx::Rect quadrant =
      gfx::Rect(kBaseSize.width() / 2, kBaseSize.height() / 2,
                kBaseSize.width() / 2, kBaseSize.height() / 2);
  const std::vector<SMPTEScalingTestCase> kTestCases = {
      // No scaling.
      {whole, 1, 1, 0},
      // Downscale by half.
      {whole, 2, 1, 1},
      // Upscale by 1.5.
      {whole, 2, 3, 1},
      // No scaling; lower-right quadrant only.
      {quadrant, 1, 1, 0},
      // Downscale by half; lower-right quadrant only.
      {quadrant, 2, 1, 1},
      // Upscale by 1.5; lower-right quadrant only.
      {quadrant, 2, 3, 1},
  };

  RunSMPTEScalingTestCases(Shader::BILINEAR, kTestCases);
}

// Test the 2-tap bilinear shader, which downscales by 4X in one dimension.
TEST_P(GLScalerShaderPixelTest, TwoTapBilinear) {
  RunMultiplePassBilinearTest(Shader::BILINEAR2, 4,
                              {SkColorSetARGB(0xff, 0xff, 0x00, 0x00),
                               SkColorSetARGB(0x7f, 0x00, 0x80, 0x00),
                               SkColorSetARGB(0x7f, 0x00, 0x80, 0x00),
                               SkColorSetARGB(0xff, 0x00, 0x00, 0xff)});
}

// Test the 3-tap bilinear shader, which downscales by 6X in one dimension.
TEST_P(GLScalerShaderPixelTest, ThreeTapBilinear) {
  // Disabled on Marshmallow. See crbug.com/933080
  if (IsAndroidMarshmallow())
    return;

  RunMultiplePassBilinearTest(Shader::BILINEAR3, 6,
                              {SkColorSetARGB(0xff, 0xff, 0x00, 0x00),
                               SkColorSetARGB(0xbf, 0x00, 0x80, 0xff),
                               SkColorSetARGB(0x7f, 0x00, 0x80, 0x00),
                               SkColorSetARGB(0x7f, 0x00, 0x80, 0x00),
                               SkColorSetARGB(0xbf, 0xff, 0x80, 0x00),
                               SkColorSetARGB(0xff, 0x00, 0x00, 0xff)});
}

// Test the 4-tap bilinear shader, which downscales by 8X in one dimension.
TEST_P(GLScalerShaderPixelTest, FourTapBilinear) {
  RunMultiplePassBilinearTest(Shader::BILINEAR4, 8,
                              {SkColorSetARGB(0xff, 0xff, 0x00, 0x00),
                               SkColorSetARGB(0x7f, 0x00, 0x80, 0x00),
                               SkColorSetARGB(0x7f, 0x00, 0x80, 0x00),
                               SkColorSetARGB(0xff, 0x00, 0x00, 0xff),
                               SkColorSetARGB(0xff, 0xff, 0xff, 0x00),
                               SkColorSetARGB(0x7f, 0x00, 0x80, 0x80),
                               SkColorSetARGB(0x7f, 0x00, 0x80, 0x80),
                               SkColorSetARGB(0xff, 0xff, 0x00, 0xff)});
}

// Test the 2-by-2-tap bilinear shader, which downscales by 4X in both
// dimensions at the same time.
TEST_P(GLScalerShaderPixelTest, TwoByTwoTapBilinear) {
  RunMultiplePassBilinearTest(Shader::BILINEAR2X2, 4,
                              {SkColorSetARGB(0xff, 0xff, 0x00, 0x00),
                               SkColorSetARGB(0x7f, 0x00, 0x80, 0x00),
                               SkColorSetARGB(0x7f, 0x00, 0x80, 0x00),
                               SkColorSetARGB(0xff, 0x00, 0x00, 0xff)});
}

// Tests the bicubic upscaler for a variety of scaling factors between 1X and
// 2X, and over the entire source texture versus just its lower-right quadrant.
TEST_P(GLScalerShaderPixelTest, BicubicUpscale) {
  // Disabled on Marshmallow. See crbug.com/933080
  if (IsAndroidMarshmallow())
    return;

  constexpr gfx::Rect whole = gfx::Rect(kBaseSize);
  constexpr gfx::Rect quadrant =
      gfx::Rect(kBaseSize.width() / 2, kBaseSize.height() / 2,
                kBaseSize.width() / 2, kBaseSize.height() / 2);
  const std::vector<SMPTEScalingTestCase> kTestCases = {
      // No scaling.
      {whole, 1, 1, 0},
      // Upscale by 4/3.
      {whole, 3, 4, 3},
      // Upscale by 3/2.
      {whole, 2, 3, 3},
      // Upscale by 2X.
      {whole, 1, 2, 3},
      // No scaling; lower-right quadrant only.
      {quadrant, 1, 1, 0},
      // Upscale by 4/3.
      {quadrant, 3, 4, 3},
      // Upscale by 3/2.
      {quadrant, 2, 3, 3},
      // Upscale by 2X.
      {quadrant, 1, 2, 3},
  };

  RunSMPTEScalingTestCases(Shader::BICUBIC_UPSCALE, kTestCases);
}

// Tests the bicubic half-downscaler, both over an entire source texture and
// over just its lower-right quadrant.
TEST_P(GLScalerShaderPixelTest, BicubicDownscaleByHalf) {
  constexpr gfx::Rect whole = gfx::Rect(kBaseSize);
  constexpr gfx::Rect quadrant =
      gfx::Rect(kBaseSize.width() / 2, kBaseSize.height() / 2,
                kBaseSize.width() / 2, kBaseSize.height() / 2);
  const std::vector<SMPTEScalingTestCase> kTestCases = {
      // Downscale by half.
      {whole, 2, 1, 2},
      // Downscale by half; lower-right quadrant only.
      {quadrant, 2, 1, 2},
  };

  RunSMPTEScalingTestCases(Shader::BICUBIC_HALF_1D, kTestCases);
}

// Tests the shaders that read a normal 4-channel interleaved texture and
// produce a planar texture consisting of just one color channel, packed into
// RGBA quads.
TEST_P(GLScalerShaderPixelTest, Export_Planar) {
  // Disabled on Marshmallow. See crbug.com/933080
  if (IsAndroidMarshmallow())
    return;

  const std::vector<SkColor> kCycle = {SkColorSetARGB(0xff, 0xff, 0x00, 0x00),
                                       SkColorSetARGB(0x80, 0x00, 0x80, 0x00),
                                       SkColorSetARGB(0x80, 0x00, 0x80, 0x00),
                                       SkColorSetARGB(0xff, 0x00, 0x00, 0xff)};
  SkBitmap source = CreateCyclicalTestImage(kBaseSize, STAGGERED, kCycle, 0);
  const GLuint src_texture = UploadTexture(source);

  // For each channel, create an expected bitmap and compare it to the result
  // from drawing with the shader program.
  if (is_converting_rgb_to_yuv()) {
    ConvertBitmapToYUV(&source);
  }
  for (int channel = 0; channel <= 3; ++channel) {
    SkBitmap expected = CreatePackedPlanarBitmap(source, channel);
    if (is_swizzling_output()) {
      SwizzleBitmap(&expected);
    }

    const Shader shader = static_cast<Shader>(
        static_cast<int>(Shader::PLANAR_CHANNEL_0) + channel);
    const gfx::Size dst_size(kBaseSize.width() / 4, kBaseSize.height());
    GetShaderProgram(shader)->UseProgram(kBaseSize,
                                         gfx::RectF(gfx::Rect(kBaseSize)),
                                         dst_size, Axis::HORIZONTAL, false);
    const SkBitmap actual =
        DownloadTexture(RenderToNewTexture(src_texture, dst_size), dst_size);
    ExpectAreTheSameImage(expected, actual);
  }
}

// Tests that the I422/NV61 formatter shader program produces a planar texture
// and an interleaved half-width texture from a normal 4-channel interleaved
// texture. See gl_shader.h for more specifics.
TEST_P(GLScalerShaderPixelTest, Export_I422_NV61) {
  if (!AreMultipleRenderingTargetsSupported()) {
    LOG(WARNING) << "Skipping test due to lack of MRT support on this machine.";
    return;
  }

  // Use a vertical stripes source image/texture to test that the shader is
  // sampling the texture at the correct points and performing
  // downscale-blending in the second texture.
  const std::vector<SkColor> kCycle = {SkColorSetARGB(0xff, 0xff, 0x00, 0x00),
                                       SkColorSetARGB(0x80, 0x00, 0x80, 0x00),
                                       SkColorSetARGB(0x80, 0x00, 0x80, 0x00),
                                       SkColorSetARGB(0xff, 0x00, 0x00, 0xff)};
  SkBitmap source =
      CreateCyclicalTestImage(kBaseSize, VERTICAL_STRIPES, kCycle, 0);
  const GLuint src_texture = UploadTexture(source);

  // Create the expected output images: The first (A) is simply the first color
  // channel of the source packed into a planar format. The second (BC) consists
  // of the second and third color channels downscaled by half and interleaved.
  // The following can be considered a reference implementation for what the
  // shader program is supposed to do.
  if (is_converting_rgb_to_yuv()) {
    ConvertBitmapToYUV(&source);
  }
  SkBitmap expected_a = CreatePackedPlanarBitmap(source, 0);
  if (is_swizzling_output()) {
    SwizzleBitmap(&expected_a);
  }
  const gfx::Size dst_size(expected_a.width(), expected_a.height());
  SkBitmap expected_bc = AllocateRGBABitmap(dst_size);
  for (int y = 0; y < dst_size.height(); ++y) {
    const uint32_t* const src = source.getAddr32(0, y);
    uint32_t* const dst_bc = expected_bc.getAddr32(0, y);
    for (int x = 0; x < dst_size.width(); ++x) {
      //     (src[0..3])        (dst_bc)
      // RGBA RGBA rgba rgba --> GBgb     (e.g, two G's blended into one G)
      const uint32_t g01 = ((((src[x * 4 + 0] >> kGreenShift) & 0xff) +
                             ((src[x * 4 + 1] >> kGreenShift) & 0xff)) /
                                2.f +
                            0.5f);
      const uint32_t b01 = ((((src[x * 4 + 0] >> kBlueShift) & 0xff) +
                             ((src[x * 4 + 1] >> kBlueShift) & 0xff)) /
                                2.f +
                            0.5f);
      const uint32_t g23 = ((((src[x * 4 + 2] >> kGreenShift) & 0xff) +
                             ((src[x * 4 + 3] >> kGreenShift) & 0xff)) /
                                2.f +
                            0.5f);
      const uint32_t b23 = ((((src[x * 4 + 2] >> kBlueShift) & 0xff) +
                             ((src[x * 4 + 3] >> kBlueShift) & 0xff)) /
                                2.f +
                            0.5f);
      dst_bc[x] = ((g01 << kRedShift) | (b01 << kGreenShift) |
                   (g23 << kBlueShift) | (b23 << kAlphaShift));
    }
  }
  if (is_swizzling_output()) {
    SwizzleBitmap(&expected_bc);
  }

  // Execute the program, and compare the shader program's drawn result with the
  // expected images.
  GetShaderProgram(Shader::I422_NV61_MRT)
      ->UseProgram(kBaseSize, gfx::RectF(gfx::Rect(kBaseSize)), dst_size,
                   Axis::HORIZONTAL, false);
  const auto textures = RenderToNewTextures(src_texture, dst_size, true);
  const SkBitmap actual_a = DownloadTexture(textures.first, dst_size);
  ExpectAreTheSameImage(expected_a, actual_a);
  const SkBitmap actual_bc = DownloadTexture(textures.second, dst_size);
  ExpectAreTheSameImage(expected_bc, actual_bc);
}

// Tests the pairwise-deinterleave shader program that produces two planar
// textures from a single interleaved one.
TEST_P(GLScalerShaderPixelTest, Export_PairwiseDeinterleave) {
  if (!AreMultipleRenderingTargetsSupported()) {
    LOG(WARNING) << "Skipping test due to lack of MRT support on this machine.";
    return;
  }

  // This shader does not provide color space conversion. It is just a
  // demultiplexer/repackager.
  if (is_converting_rgb_to_yuv()) {
    return;
  }

  // Create a source image/texture with a pattern suitable for ensuring the
  // shader is sampling the texture at the correct points.
  const std::vector<SkColor> kCycle = {SkColorSetARGB(0xff, 0xff, 0x00, 0x00),
                                       SkColorSetARGB(0xc0, 0x00, 0xc0, 0x00),
                                       SkColorSetARGB(0x80, 0x00, 0x00, 0x80),
                                       SkColorSetARGB(0xff, 0xff, 0xff, 0xff)};
  const SkBitmap source =
      CreateCyclicalTestImage(kBaseSize, STAGGERED, kCycle, 0);
  const GLuint src_texture = UploadTexture(source);

  // Create the expected pair of planar images.
  const gfx::Size dst_size(kBaseSize.width() / 2, kBaseSize.height());
  SkBitmap expected_a = AllocateRGBABitmap(dst_size);
  SkBitmap expected_b = AllocateRGBABitmap(dst_size);
  for (int y = 0; y < dst_size.height(); ++y) {
    const uint32_t* const src = source.getAddr32(0, y);
    uint32_t* const dst_a = expected_a.getAddr32(0, y);
    uint32_t* const dst_b = expected_b.getAddr32(0, y);
    for (int x = 0; x < dst_size.width(); ++x) {
      //   (src)       (dst_a) (dst_b)
      // ABAB abab --> { AAaa + BBbb }
      dst_a[x] = ((((src[x * 2 + 0] >> kRedShift) & 0xff) << kRedShift) |
                  (((src[x * 2 + 0] >> kBlueShift) & 0xff) << kGreenShift) |
                  (((src[x * 2 + 1] >> kRedShift) & 0xff) << kBlueShift) |
                  (((src[x * 2 + 1] >> kBlueShift) & 0xff) << kAlphaShift));
      dst_b[x] = ((((src[x * 2 + 0] >> kGreenShift) & 0xff) << kRedShift) |
                  (((src[x * 2 + 0] >> kAlphaShift) & 0xff) << kGreenShift) |
                  (((src[x * 2 + 1] >> kGreenShift) & 0xff) << kBlueShift) |
                  (((src[x * 2 + 1] >> kAlphaShift) & 0xff) << kAlphaShift));
    }
  }
  if (is_swizzling_output()) {
    SwizzleBitmap(&expected_a);
    SwizzleBitmap(&expected_b);
  }

  // Execute the program, and compare the shader program's drawn result with the
  // expected images.
  GetShaderProgram(Shader::DEINTERLEAVE_PAIRWISE_MRT)
      ->UseProgram(kBaseSize, gfx::RectF(gfx::Rect(kBaseSize)), dst_size,
                   Axis::HORIZONTAL, false);
  const auto textures = RenderToNewTextures(src_texture, dst_size, true);
  const SkBitmap actual_a = DownloadTexture(textures.first, dst_size);
  ExpectAreTheSameImage(expected_a, actual_a);
  const SkBitmap actual_b = DownloadTexture(textures.second, dst_size);
  ExpectAreTheSameImage(expected_b, actual_b);
}

INSTANTIATE_TEST_SUITE_P(All,
                         GLScalerShaderPixelTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace viz
