// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gl_scaler.h"

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
#include "third_party/skia/include/core/SkRect.h"

namespace viz {

class GLScalerOverscanPixelTest : public cc::PixelTest,
                                  public GLScalerTestUtil {
 public:
  using Axis = GLScaler::Axis;
  using ScalerStage = GLScaler::ScalerStage;
  using Shader = GLScaler::Shader;

  bool AreMultipleRenderingTargetsSupported() const {
    return scaler_->GetMaxDrawBuffersSupported() > 1;
  }

  // Creates a ScalerStage chain consisting of a single stage having the given
  // configuration.
  void UseScaler(Shader shader,
                 Axis primary_axis,
                 const gfx::Vector2d& scale_from,
                 const gfx::Vector2d& scale_to) {
    scaler_->chain_ = std::make_unique<ScalerStage>(gl_, shader, primary_axis,
                                                    scale_from, scale_to);
    scaler_->chain_->set_shader_program(scaler_->GetShaderProgram(
        shader, GL_UNSIGNED_BYTE, nullptr, GLScaler::Parameters().swizzle));
  }

  // Converts the given |source_rect| into a possibly-larger one that includes
  // all of the pixels that would be sampled by the current scaler (i.e.,
  // including overscan). This uses the math of the internal implementation to
  // compute the values.
  gfx::Rect ToInputRect(gfx::Rect source_rect) {
    CHECK(scaler_ && scaler_->chain_);
    return scaler_->chain_->ToInputRect(gfx::RectF(source_rect));
  }

  // Renders images using the current scaler to auto-detect its overscan. This
  // does NOT use the internal implementation to compute the values, but instead
  // discovers them experimentally. This is used to confirm that: a) the scaler
  // behaves as ToInputRect() expects; and b) the math internal to ToInputRect()
  // is correct.
  //
  // The general approach is to upload a source image containing a blue box in
  // the center, surrounded by a red background. The size of the blue box is
  // varied: It starts out at a size encompassing more than all of the pixels to
  // be sampled by the scaler, and is gradually shrunk until the scaler's output
  // begins to include red "bleed-in." At that point, the overscan amount is
  // confirmed experimentally.
  gfx::Vector2d DetectScalerOverscan(const gfx::Vector2d& scale_from,
                                     const gfx::Vector2d& scale_to) {
    // Assume a source size three times the "scale from" width and height. This
    // allows for scaling the middle third of a source image, to test possible
    // bleed-in on all sides of the output.
    const gfx::Size src_size(scale_from.x() * 3, scale_from.y() * 3);

    // The requested output rect is the center third of the image, in the
    // destination coordinate space.
    const gfx::Rect dst_rect(
        src_size.width() / 3 * scale_to.x() / scale_from.x(),
        src_size.height() / 3 * scale_to.y() / scale_from.y(),
        src_size.width() / 3 * scale_to.x() / scale_from.x(),
        src_size.height() / 3 * scale_to.y() / scale_from.y());
    const GLuint dst_texture = texture_helper_->CreateTexture(dst_rect.size());

    // This is our "basis for comparison" image. If scaled output images match
    // this, then there is no bleed-in.
    SkBitmap output_without_bleed_in;
    {
      const bool did_scale =
          scaler_->Scale(texture_helper_->UploadTexture(CreateBlueBoxOnRedImage(
                             src_size, gfx::Rect(src_size))),
                         src_size, gfx::Vector2d(0, 0), dst_texture, dst_rect);
      CHECK(did_scale);
      output_without_bleed_in =
          texture_helper_->DownloadTexture(dst_texture, dst_rect.size());
      VLOG(2) << scale_from.ToString() << "→" << scale_to.ToString()
              << ": Output without bleed-in is "
              << cc::GetPNGDataUrl(output_without_bleed_in);
    }

    // Perform a linear search for the minimal overscan values that do not cause
    // the red bleed-in in the scaled output image. There are actually two
    // separate searches here, one horizontally and one vertically. Note that an
    // overscan result of -1 indicates a failed search and/or a broken
    // implementation.
    gfx::Vector2d min_overscan(5, 5);
    for (int is_horizontal = 1; is_horizontal >= 0; --is_horizontal) {
      while (min_overscan.x() >= 0 && min_overscan.y() >= 0) {
        // Decrease the overscan by one pixel (one dimension at a time).
        const gfx::Vector2d overscan(
            is_horizontal ? (min_overscan.x() - 1) : min_overscan.x(),
            is_horizontal ? min_overscan.y() : (min_overscan.y() - 1));

        // Create the source texture consisting of a centered blue box
        // surrounded by red.
        const gfx::Rect blue_rect(scale_from.x() - overscan.x(),
                                  scale_from.y() - overscan.y(),
                                  scale_from.x() + 2 * overscan.x(),
                                  scale_from.y() + 2 * overscan.y());
        const SkBitmap source_image =
            CreateBlueBoxOnRedImage(src_size, blue_rect);
        const bool did_scale = scaler_->Scale(
            texture_helper_->UploadTexture(source_image), src_size,
            gfx::Vector2d(0, 0), dst_texture, dst_rect);
        CHECK(did_scale);
        const SkBitmap output =
            texture_helper_->DownloadTexture(dst_texture, dst_rect.size());

        // Compare |output| with |output_without_bleed_in|. If they are
        // different, then the blue rect became too small.
        bool output_has_bleed_in = false;
        for (int y = 0; y < output.height(); ++y) {
          for (int x = 0; x < output.width(); ++x) {
            if (output.getColor(x, y) !=
                output_without_bleed_in.getColor(x, y)) {
              output_has_bleed_in = true;
              break;
            }
          }
        }

        VLOG(2) << scale_from.ToString() << "→" << scale_to.ToString()
                << ": Testing overscan=" << overscan.ToString() << std::endl
                << "\tSource image is " << cc::GetPNGDataUrl(source_image)
                << std::endl
                << "\tOutput image is " << cc::GetPNGDataUrl(output);

        if (output_has_bleed_in) {
          break;  // Search complete: Red bleed-in detected.
        }

        min_overscan = overscan;
      }
    }

    return min_overscan;
  }

  static SkBitmap CreateBlueBoxOnRedImage(const gfx::Size& size,
                                          const gfx::Rect& blue_rect) {
    SkBitmap result = AllocateRGBABitmap(size);
    // Note: None of the color channel values should be close to 0 or 255. This
    // is because the bicubic scaler will generate values that overshoot and
    // clip, and this will mess-up detection of the number of overscan pixels.
    result.eraseColor(SkColorSetRGB(0xc0, 0x40, 0x40));
    result.erase(SkColorSetRGB(0x40, 0x40, 0xc0),
                 SkIRect{blue_rect.x(), blue_rect.y(), blue_rect.right(),
                         blue_rect.bottom()});
    return result;
  }

 protected:
  void SetUp() final {
    cc::PixelTest::SetUpGLWithoutRenderer(gfx::SurfaceOrigin::kBottomLeft);

    scaler_ = std::make_unique<GLScaler>(context_provider());
    gl_ = context_provider()->ContextGL();
    CHECK(gl_);
    texture_helper_ = std::make_unique<GLScalerTestTextureHelper>(gl_);
  }

  void TearDown() final {
    texture_helper_.reset();
    gl_ = nullptr;
    scaler_.reset();

    cc::PixelTest::TearDown();
  }

  std::unique_ptr<GLScaler> scaler_;
  gpu::gles2::GLES2Interface* gl_ = nullptr;
  std::unique_ptr<GLScalerTestTextureHelper> texture_helper_;
};

namespace {
constexpr gfx::Rect kTenByTenRect = gfx::Rect(10, 10, 10, 10);
}  // namespace

TEST_F(GLScalerOverscanPixelTest, Bilinear) {
  constexpr struct {
    gfx::Vector2d scale_from;
    gfx::Vector2d scale_to;
    gfx::Vector2d expected_overscan;
  } kTestCases[] = {
      // No scaling.
      {gfx::Vector2d(32, 20), gfx::Vector2d(32, 20), gfx::Vector2d(0, 0)},

      // Scale by 0.5X.
      {gfx::Vector2d(32, 20), gfx::Vector2d(16, 20), gfx::Vector2d(0, 0)},
      {gfx::Vector2d(32, 20), gfx::Vector2d(32, 10), gfx::Vector2d(0, 0)},
      {gfx::Vector2d(32, 20), gfx::Vector2d(16, 10), gfx::Vector2d(0, 0)},

      // Scale by 0.75X.
      {gfx::Vector2d(32, 20), gfx::Vector2d(24, 20), gfx::Vector2d(0, 0)},
      {gfx::Vector2d(32, 20), gfx::Vector2d(32, 15), gfx::Vector2d(0, 0)},
      {gfx::Vector2d(32, 20), gfx::Vector2d(24, 15), gfx::Vector2d(0, 0)},

      // Scale by 1.5X.
      {gfx::Vector2d(32, 20), gfx::Vector2d(48, 20), gfx::Vector2d(1, 0)},
      {gfx::Vector2d(32, 20), gfx::Vector2d(32, 30), gfx::Vector2d(0, 1)},
      {gfx::Vector2d(32, 20), gfx::Vector2d(48, 30), gfx::Vector2d(1, 1)},

      // Scale by 4X.
      {gfx::Vector2d(32, 20), gfx::Vector2d(128, 20), gfx::Vector2d(1, 0)},
      {gfx::Vector2d(32, 20), gfx::Vector2d(32, 80), gfx::Vector2d(0, 1)},
      {gfx::Vector2d(32, 20), gfx::Vector2d(128, 80), gfx::Vector2d(1, 1)},
  };

  for (const auto& tc : kTestCases) {
    SCOPED_TRACE(testing::Message() << "scale_from=" << tc.scale_from.ToString()
                                    << ", scale_to=" << tc.scale_to.ToString());

    // Test the effect on the pixels.
    UseScaler(Shader::BILINEAR, Axis::HORIZONTAL, tc.scale_from, tc.scale_to);
    EXPECT_EQ(tc.expected_overscan,
              DetectScalerOverscan(tc.scale_from, tc.scale_to));

    // Sanity-check that the internal math estimating the overscan is correct.
    gfx::Rect expected_input_rect = kTenByTenRect;
    expected_input_rect.Inset(-tc.expected_overscan.x(),
                              -tc.expected_overscan.y());
    EXPECT_EQ(expected_input_rect, ToInputRect(kTenByTenRect));
  }
}

TEST_F(GLScalerOverscanPixelTest, TwoTapBilinear) {
  constexpr struct {
    Axis primary_axis;
    gfx::Vector2d scale_from;
    gfx::Vector2d scale_to;
    gfx::Vector2d expected_overscan;
  } kTestCases[] = {
      // Scale by 0.25X in one direction only.
      {Axis::HORIZONTAL, gfx::Vector2d(64, 40), gfx::Vector2d(16, 40),
       gfx::Vector2d(0, 0)},
      {Axis::VERTICAL, gfx::Vector2d(64, 40), gfx::Vector2d(64, 10),
       gfx::Vector2d(0, 0)},

      // Scale by 0.25X in one direction, 0.5X in the other.
      {Axis::HORIZONTAL, gfx::Vector2d(64, 40), gfx::Vector2d(16, 20),
       gfx::Vector2d(0, 0)},
      {Axis::VERTICAL, gfx::Vector2d(64, 40), gfx::Vector2d(32, 10),
       gfx::Vector2d(0, 0)},

      // Scale by 0.75X (1.5X * 0.5X).
      {Axis::HORIZONTAL, gfx::Vector2d(64, 40), gfx::Vector2d(48, 40),
       gfx::Vector2d(1, 0)},
      {Axis::VERTICAL, gfx::Vector2d(64, 40), gfx::Vector2d(64, 30),
       gfx::Vector2d(0, 1)},
  };

  for (const auto& tc : kTestCases) {
    SCOPED_TRACE(testing::Message() << "scale_from=" << tc.scale_from.ToString()
                                    << ", scale_to=" << tc.scale_to.ToString());

    // Test the effect on the pixels.
    UseScaler(Shader::BILINEAR2, tc.primary_axis, tc.scale_from, tc.scale_to);
    EXPECT_EQ(tc.expected_overscan,
              DetectScalerOverscan(tc.scale_from, tc.scale_to));

    // Sanity-check that the internal math estimating the overscan is correct.
    gfx::Rect expected_input_rect = kTenByTenRect;
    expected_input_rect.Inset(-tc.expected_overscan.x(),
                              -tc.expected_overscan.y());
    EXPECT_EQ(expected_input_rect, ToInputRect(kTenByTenRect));
  }
}

TEST_F(GLScalerOverscanPixelTest, ThreeTapBilinear) {
  constexpr struct {
    Axis primary_axis;
    gfx::Vector2d scale_from;
    gfx::Vector2d scale_to;
    gfx::Vector2d expected_overscan;
  } kTestCases[] = {
      // Scale by 0.16...X in one direction only.
      {Axis::HORIZONTAL, gfx::Vector2d(66, 40), gfx::Vector2d(11, 40),
       gfx::Vector2d(0, 0)},
      {Axis::VERTICAL, gfx::Vector2d(32, 60), gfx::Vector2d(32, 10),
       gfx::Vector2d(0, 0)},

      // Scale by 0.16...X in one direction, 0.5X in the other.
      {Axis::HORIZONTAL, gfx::Vector2d(66, 40), gfx::Vector2d(11, 20),
       gfx::Vector2d(0, 0)},
      {Axis::VERTICAL, gfx::Vector2d(64, 60), gfx::Vector2d(32, 10),
       gfx::Vector2d(0, 0)},

      // Scale by 0.75X (3.0X * 0.5X * 0.5X).
      {Axis::HORIZONTAL, gfx::Vector2d(64, 40), gfx::Vector2d(48, 40),
       gfx::Vector2d(1, 0)},
      {Axis::VERTICAL, gfx::Vector2d(64, 40), gfx::Vector2d(64, 30),
       gfx::Vector2d(0, 1)},
  };

  for (const auto& tc : kTestCases) {
    SCOPED_TRACE(testing::Message() << "scale_from=" << tc.scale_from.ToString()
                                    << ", scale_to=" << tc.scale_to.ToString());

    // Test the effect on the pixels.
    UseScaler(Shader::BILINEAR3, tc.primary_axis, tc.scale_from, tc.scale_to);
    EXPECT_EQ(tc.expected_overscan,
              DetectScalerOverscan(tc.scale_from, tc.scale_to));

    // Sanity-check that the internal math estimating the overscan is correct.
    gfx::Rect expected_input_rect = kTenByTenRect;
    expected_input_rect.Inset(-tc.expected_overscan.x(),
                              -tc.expected_overscan.y());
    EXPECT_EQ(expected_input_rect, ToInputRect(kTenByTenRect));
  }
}

TEST_F(GLScalerOverscanPixelTest, FourTapBilinear) {
  constexpr struct {
    Axis primary_axis;
    gfx::Vector2d scale_from;
    gfx::Vector2d scale_to;
    gfx::Vector2d expected_overscan;
  } kTestCases[] = {
      // Scale by 0.125X in one direction only.
      {Axis::HORIZONTAL, gfx::Vector2d(64, 40), gfx::Vector2d(8, 40),
       gfx::Vector2d(0, 0)},
      {Axis::VERTICAL, gfx::Vector2d(64, 40), gfx::Vector2d(64, 5),
       gfx::Vector2d(0, 0)},

      // Scale by 0.125X in one direction, 0.5X in the other.
      {Axis::HORIZONTAL, gfx::Vector2d(64, 40), gfx::Vector2d(8, 20),
       gfx::Vector2d(0, 0)},
      {Axis::VERTICAL, gfx::Vector2d(64, 40), gfx::Vector2d(32, 5),
       gfx::Vector2d(0, 0)},

      // Scale by 0.75X (6.0X * 0.5X * 0.5X * 0.5X).
      {Axis::HORIZONTAL, gfx::Vector2d(64, 40), gfx::Vector2d(48, 40),
       gfx::Vector2d(1, 0)},
      {Axis::VERTICAL, gfx::Vector2d(64, 40), gfx::Vector2d(64, 30),
       gfx::Vector2d(0, 1)},
  };

  for (const auto& tc : kTestCases) {
    SCOPED_TRACE(testing::Message() << "scale_from=" << tc.scale_from.ToString()
                                    << ", scale_to=" << tc.scale_to.ToString());

    // Test the effect on the pixels.
    UseScaler(Shader::BILINEAR4, tc.primary_axis, tc.scale_from, tc.scale_to);
    EXPECT_EQ(tc.expected_overscan,
              DetectScalerOverscan(tc.scale_from, tc.scale_to));

    // Sanity-check that the internal math estimating the overscan is correct.
    gfx::Rect expected_input_rect = kTenByTenRect;
    expected_input_rect.Inset(-tc.expected_overscan.x(),
                              -tc.expected_overscan.y());
    EXPECT_EQ(expected_input_rect, ToInputRect(kTenByTenRect));
  }
}

TEST_F(GLScalerOverscanPixelTest, TwoByTwoTapBilinear) {
  constexpr struct {
    gfx::Vector2d scale_from;
    gfx::Vector2d scale_to;
    gfx::Vector2d expected_overscan;
  } kTestCases[] = {
      // Scale by 0.25X in both directions.
      {gfx::Vector2d(64, 40), gfx::Vector2d(16, 10), gfx::Vector2d(0, 0)},

      // Scale by 0.75X (1.5X * 0.5X) in one direction, 0.25X in the other.
      {gfx::Vector2d(64, 40), gfx::Vector2d(48, 10), gfx::Vector2d(1, 0)},
      {gfx::Vector2d(64, 40), gfx::Vector2d(16, 30), gfx::Vector2d(0, 1)},

      // Scale by 0.75X (1.5X * 0.5X) in both directions.
      {gfx::Vector2d(64, 40), gfx::Vector2d(48, 30), gfx::Vector2d(1, 1)},
  };

  for (const auto& tc : kTestCases) {
    SCOPED_TRACE(testing::Message() << "scale_from=" << tc.scale_from.ToString()
                                    << ", scale_to=" << tc.scale_to.ToString());

    // Test the effect on the pixels.
    UseScaler(Shader::BILINEAR2X2, Axis::HORIZONTAL, tc.scale_from,
              tc.scale_to);
    EXPECT_EQ(tc.expected_overscan,
              DetectScalerOverscan(tc.scale_from, tc.scale_to));

    // Sanity-check that the internal math estimating the overscan is correct.
    gfx::Rect expected_input_rect = kTenByTenRect;
    expected_input_rect.Inset(-tc.expected_overscan.x(),
                              -tc.expected_overscan.y());
    EXPECT_EQ(expected_input_rect, ToInputRect(kTenByTenRect));
  }
}

TEST_F(GLScalerOverscanPixelTest, BicubicUpscale) {
#if defined(OS_ANDROID)
  // Unfortunately, on our current Android bots, there are some inaccuracies
  // introduced by the platform that seem to throw-off the pixel testing of the
  // bicubic sampler.
  constexpr bool kSkipDetectionTest = true;
  LOG(WARNING) << "Skipping overscan detection due to platform issues.";
#else
  constexpr bool kSkipDetectionTest = false;
#endif

  constexpr struct {
    Axis primary_axis;
    gfx::Vector2d scale_from;
    gfx::Vector2d scale_to;
    gfx::Vector2d expected_overscan;
  } kTestCases[] = {
      // Scale by 1.5X, 2X, and 3.3...X horizontally.
      {Axis::HORIZONTAL, gfx::Vector2d(12, 10), gfx::Vector2d(18, 10),
       gfx::Vector2d(2, 0)},
      {Axis::HORIZONTAL, gfx::Vector2d(12, 10), gfx::Vector2d(24, 10),
       gfx::Vector2d(2, 0)},
      {Axis::HORIZONTAL, gfx::Vector2d(12, 10), gfx::Vector2d(40, 10),
       gfx::Vector2d(2, 0)},

      // Scale by 1.5X, 2X, and 3.3...X vertically.
      {Axis::VERTICAL, gfx::Vector2d(12, 10), gfx::Vector2d(12, 15),
       gfx::Vector2d(0, 2)},
      {Axis::VERTICAL, gfx::Vector2d(12, 10), gfx::Vector2d(12, 20),
       gfx::Vector2d(0, 2)},
      {Axis::VERTICAL, gfx::Vector2d(12, 9), gfx::Vector2d(12, 30),
       gfx::Vector2d(0, 2)},
  };

  for (const auto& tc : kTestCases) {
    SCOPED_TRACE(testing::Message() << "scale_from=" << tc.scale_from.ToString()
                                    << ", scale_to=" << tc.scale_to.ToString());

    // Test the effect on the pixels.
    UseScaler(Shader::BICUBIC_UPSCALE, tc.primary_axis, tc.scale_from,
              tc.scale_to);
    if (!kSkipDetectionTest) {
      EXPECT_EQ(tc.expected_overscan,
                DetectScalerOverscan(tc.scale_from, tc.scale_to));
    }

    // Sanity-check that the internal math estimating the overscan is correct.
    gfx::Rect expected_input_rect = kTenByTenRect;
    expected_input_rect.Inset(-tc.expected_overscan.x(),
                              -tc.expected_overscan.y());
    EXPECT_EQ(expected_input_rect, ToInputRect(kTenByTenRect));
  }
}

TEST_F(GLScalerOverscanPixelTest, BicubicHalving) {
  constexpr struct {
    Axis primary_axis;
    gfx::Vector2d scale_from;
    gfx::Vector2d scale_to;
    gfx::Vector2d expected_overscan;
  } kTestCases[] = {
      {Axis::HORIZONTAL, gfx::Vector2d(16, 16), gfx::Vector2d(8, 16),
       gfx::Vector2d(3, 0)},
      {Axis::VERTICAL, gfx::Vector2d(16, 16), gfx::Vector2d(16, 8),
       gfx::Vector2d(0, 3)},
  };

  for (const auto& tc : kTestCases) {
    SCOPED_TRACE(testing::Message() << "scale_from=" << tc.scale_from.ToString()
                                    << ", scale_to=" << tc.scale_to.ToString());

    // Test the effect on the pixels.
    UseScaler(Shader::BICUBIC_HALF_1D, tc.primary_axis, tc.scale_from,
              tc.scale_to);
    EXPECT_EQ(tc.expected_overscan,
              DetectScalerOverscan(tc.scale_from, tc.scale_to));

    // Sanity-check that the internal math estimating the overscan is correct.
    gfx::Rect expected_input_rect = kTenByTenRect;
    expected_input_rect.Inset(-tc.expected_overscan.x(),
                              -tc.expected_overscan.y());
    EXPECT_EQ(expected_input_rect, ToInputRect(kTenByTenRect));
  }
}

TEST_F(GLScalerOverscanPixelTest, Planerizers) {
  if (!AreMultipleRenderingTargetsSupported()) {
    LOG(WARNING) << "Skipping test due to lack of MRT support on this machine.";
    return;
  }

  constexpr struct {
    Shader shader;
    Axis primary_axis;
    gfx::Vector2d scale_from;
    gfx::Vector2d scale_to;
  } kTestCases[] = {
      {Shader::PLANAR_CHANNEL_0, Axis::HORIZONTAL, gfx::Vector2d(16, 16),
       gfx::Vector2d(4, 16)},
      // Note: Other PLANAR_CHANNEL_N shaders don't need to be tested since they
      // use the same code path.
      {Shader::I422_NV61_MRT, Axis::HORIZONTAL, gfx::Vector2d(16, 16),
       gfx::Vector2d(4, 16)},
      {
          Shader::DEINTERLEAVE_PAIRWISE_MRT, Axis::HORIZONTAL,
          gfx::Vector2d(16, 16), gfx::Vector2d(8, 16),
      },
  };

  for (const auto& tc : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "shader=" << static_cast<int>(tc.shader)
                 << ", scale_from=" << tc.scale_from.ToString()
                 << ", scale_to=" << tc.scale_to.ToString());

    // Test the effect on the pixels.
    UseScaler(tc.shader, tc.primary_axis, tc.scale_from, tc.scale_to);
    EXPECT_EQ(gfx::Vector2d(0, 0),
              DetectScalerOverscan(tc.scale_from, tc.scale_to));

    // Sanity-check that the internal math estimating the overscan is correct.
    EXPECT_EQ(kTenByTenRect, ToInputRect(kTenByTenRect));
  }
}

}  // namespace viz
