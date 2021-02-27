// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/gl_renderer.h"

#include <stdint.h>

#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/pixel_test.h"
#include "cc/test/render_pass_test_utils.h"
#include "cc/test/resource_provider_test_utils.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/service/display/display_resource_provider_gl.h"
#include "components/viz/test/fake_output_surface.h"
#include "components/viz/test/test_gles2_interface.h"
#include "components/viz/test/viz_test_suite.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/config/gpu_finch_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/color_transform.h"
#include "ui/gfx/transform.h"
#include "ui/latency/latency_info.h"

#if defined(OS_WIN)
#include "components/viz/service/display/overlay_processor_win.h"
#elif defined(OS_APPLE)
#include "components/viz/service/display/overlay_processor_mac.h"
#elif defined(OS_ANDROID) || defined(USE_OZONE)
#include "components/viz/service/display/overlay_processor_using_strategy.h"
#include "components/viz/service/display/overlay_strategy_single_on_top.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#else  // Default
#include "components/viz/service/display/overlay_processor_stub.h"
#endif

using testing::_;
using testing::AnyNumber;
using testing::Args;
using testing::AtLeast;
using testing::Contains;
using testing::ElementsAre;
using testing::Expectation;
using testing::InSequence;
using testing::Invoke;
using testing::Mock;
using testing::Not;
using testing::Pointee;
using testing::Return;
using testing::StrictMock;

namespace viz {

MATCHER_P(MatchesSyncToken, sync_token, "") {
  gpu::SyncToken other;
  memcpy(&other, arg, sizeof(other));
  return other == sync_token;
}

class GLRendererTest : public testing::Test {
 protected:
  ~GLRendererTest() override {
    // Some tests create CopyOutputRequests which will PostTask ensure
    // they are all cleaned up and completed before destroying the test.
    VizTestSuite::RunUntilIdle();
  }
  AggregatedRenderPass* root_render_pass() {
    return render_passes_in_draw_order_.back().get();
  }
  void DrawFrame(GLRenderer* renderer,
                 const gfx::Size& viewport_size,
                 const gfx::DisplayColorSpaces& display_color_spaces =
                     gfx::DisplayColorSpaces()) {
    SurfaceDamageRectList surface_damage_rect_list;
    renderer->DrawFrame(&render_passes_in_draw_order_, 1.f, viewport_size,
                        display_color_spaces,
                        std::move(surface_damage_rect_list));
  }

  static const Program* current_program(GLRenderer* renderer) {
    return renderer->current_program_;
  }

  static TexCoordPrecision get_cached_tex_coord_precision(
      GLRenderer* renderer) {
    return renderer->draw_cache_.program_key.tex_coord_precision();
  }

  DebugRendererSettings debug_settings_;
  AggregatedRenderPassList render_passes_in_draw_order_;
};

#define EXPECT_PROGRAM_VALID(program_binding)      \
  do {                                             \
    ASSERT_TRUE(program_binding);                  \
    EXPECT_TRUE((program_binding)->program());     \
    EXPECT_TRUE((program_binding)->initialized()); \
  } while (false)

static inline SkBlendMode BlendModeToSkXfermode(BlendMode blend_mode) {
  switch (blend_mode) {
    case BLEND_MODE_NONE:
    case BLEND_MODE_NORMAL:
      return SkBlendMode::kSrcOver;
    case BLEND_MODE_DESTINATION_IN:
      return SkBlendMode::kDstIn;
    case BLEND_MODE_SCREEN:
      return SkBlendMode::kScreen;
    case BLEND_MODE_OVERLAY:
      return SkBlendMode::kOverlay;
    case BLEND_MODE_DARKEN:
      return SkBlendMode::kDarken;
    case BLEND_MODE_LIGHTEN:
      return SkBlendMode::kLighten;
    case BLEND_MODE_COLOR_DODGE:
      return SkBlendMode::kColorDodge;
    case BLEND_MODE_COLOR_BURN:
      return SkBlendMode::kColorBurn;
    case BLEND_MODE_HARD_LIGHT:
      return SkBlendMode::kHardLight;
    case BLEND_MODE_SOFT_LIGHT:
      return SkBlendMode::kSoftLight;
    case BLEND_MODE_DIFFERENCE:
      return SkBlendMode::kDifference;
    case BLEND_MODE_EXCLUSION:
      return SkBlendMode::kExclusion;
    case BLEND_MODE_MULTIPLY:
      return SkBlendMode::kMultiply;
    case BLEND_MODE_HUE:
      return SkBlendMode::kHue;
    case BLEND_MODE_SATURATION:
      return SkBlendMode::kSaturation;
    case BLEND_MODE_COLOR:
      return SkBlendMode::kColor;
    case BLEND_MODE_LUMINOSITY:
      return SkBlendMode::kLuminosity;
  }
  return SkBlendMode::kSrcOver;
}

// Explicitly named to be a friend in GLRenderer for shader access.
class GLRendererShaderPixelTest : public cc::PixelTest {
 public:
  void SetUp() override {
    SetUpGLRenderer(gfx::SurfaceOrigin::kBottomLeft);
    ASSERT_FALSE(renderer()->IsContextLost());
  }

  void TearDown() override {
    cc::PixelTest::TearDown();
    ASSERT_FALSE(renderer());
  }

  GLRenderer* renderer() { return static_cast<GLRenderer*>(renderer_.get()); }

  void TestShaderWithDrawingFrame(
      const ProgramKey& program_key,
      const DirectRenderer::DrawingFrame& drawing_frame,
      bool validate_output_color_matrix) {
    renderer()->SetCurrentFrameForTesting(drawing_frame);
    const gfx::ColorSpace kSrcColorSpaces[] = {
        gfx::ColorSpace::CreateSRGB(),
        gfx::ColorSpace(gfx::ColorSpace::PrimaryID::ADOBE_RGB,
                        gfx::ColorSpace::TransferID::GAMMA28),
        gfx::ColorSpace::CreateREC709(),
        gfx::ColorSpace::CreateExtendedSRGB(),
        // This will be adjusted to the display's SDR white level, because no
        // level was specified.
        gfx::ColorSpace::CreateSCRGBLinear(),
        // This won't be, because it has a set SDR white level.
        gfx::ColorSpace::CreateSCRGBLinear(123.0f),
        // This will be adjusted to the display's SDR white level, because no
        // level was specified.
        gfx::ColorSpace::CreateHDR10(),
        // This won't be, because it has a set SDR white level.
        gfx::ColorSpace::CreateHDR10(123.0f),
    };
    const gfx::ColorSpace kDstColorSpaces[] = {
        gfx::ColorSpace::CreateSRGB(),
        gfx::ColorSpace(gfx::ColorSpace::PrimaryID::ADOBE_RGB,
                        gfx::ColorSpace::TransferID::GAMMA18),
        gfx::ColorSpace::CreateExtendedSRGB(),
        gfx::ColorSpace::CreateSCRGBLinear(),
    };
    // Note: Use ASSERT_XXX() and not EXPECT_XXX() below since the size of the
    // loop will lead to useless timeout failures on the bots otherwise.
    for (const auto& src_color_space : kSrcColorSpaces) {
      for (const auto& dst_color_space : kDstColorSpaces) {
        renderer()->SetUseProgram(program_key, src_color_space, dst_color_space,
                                  /*adjust_src_white_level=*/true);
        ASSERT_TRUE(renderer()->current_program_->initialized());

        if (src_color_space != dst_color_space) {
          auto adjusted_color_space = src_color_space;
          if (src_color_space.IsHDR()) {
            adjusted_color_space = src_color_space.GetWithSDRWhiteLevel(
                drawing_frame.display_color_spaces.GetSDRWhiteLevel());
          }
          SCOPED_TRACE(
              base::StringPrintf("adjusted_color_space=%s, dst_color_space=%s",
                                 adjusted_color_space.ToString().c_str(),
                                 dst_color_space.ToString().c_str()));

          auto color_transform = gfx::ColorTransform::NewColorTransform(
              adjusted_color_space, dst_color_space,
              gfx::ColorTransform::Intent::INTENT_PERCEPTUAL);

          ASSERT_EQ(color_transform->GetShaderSource(),
                    renderer()
                        ->current_program_->color_transform_for_testing()
                        ->GetShaderSource());
        }

        if (validate_output_color_matrix) {
          if (program_key.type() == ProgramType::PROGRAM_TYPE_SOLID_COLOR) {
            ASSERT_EQ(
                -1,
                renderer()->current_program_->output_color_matrix_location());
          } else {
            ASSERT_NE(
                -1,
                renderer()->current_program_->output_color_matrix_location());
          }
        }
      }
    }
  }

  void TestShader(const ProgramKey& program_key) {
    TestShaderWithDrawingFrame(program_key, GLRenderer::DrawingFrame(), false);
  }

  void TestShadersWithOutputColorMatrix(const ProgramKey& program_key) {
    GLRenderer::DrawingFrame frame;

    AggregatedRenderPassList render_passes_in_draw_order;
    gfx::Size viewport_size(100, 100);
    AggregatedRenderPassId root_pass_id{1};
    auto* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    root_pass->damage_rect = gfx::Rect(0, 0, 25, 25);

    frame.root_render_pass = root_pass;
    frame.current_render_pass = root_pass;
    frame.render_passes_in_draw_order = &render_passes_in_draw_order;

    // Set a non-identity color matrix on the output surface.
    SkMatrix44 color_matrix(SkMatrix44::kIdentity_Constructor);
    color_matrix.set(0, 0, 0.7f);
    color_matrix.set(1, 1, 0.4f);
    color_matrix.set(2, 2, 0.5f);
    renderer()->output_surface_->set_color_matrix(color_matrix);

    TestShaderWithDrawingFrame(program_key, frame, true);
  }

  void TestShadersWithSDRWhiteLevel(const ProgramKey& program_key,
                                    float sdr_white_level) {
    GLRenderer::DrawingFrame frame;
    frame.display_color_spaces.SetSDRWhiteLevel(sdr_white_level);
    TestShaderWithDrawingFrame(program_key, frame, false);
  }

  void TestBasicShaders() {
    TestShader(ProgramKey::DebugBorder());
    TestShader(ProgramKey::SolidColor(NO_AA, false, false));
    TestShader(ProgramKey::SolidColor(USE_AA, false, false));
    TestShader(ProgramKey::SolidColor(NO_AA, true, false));

    TestShadersWithOutputColorMatrix(ProgramKey::DebugBorder());
    TestShadersWithOutputColorMatrix(
        ProgramKey::SolidColor(NO_AA, false, false));
    TestShadersWithOutputColorMatrix(
        ProgramKey::SolidColor(USE_AA, false, false));
    TestShadersWithOutputColorMatrix(
        ProgramKey::SolidColor(NO_AA, true, false));

    TestShadersWithSDRWhiteLevel(ProgramKey::DebugBorder(), 200.f);
    TestShadersWithSDRWhiteLevel(ProgramKey::SolidColor(NO_AA, false, false),
                                 200.f);
    TestShadersWithSDRWhiteLevel(ProgramKey::SolidColor(USE_AA, false, false),
                                 200.f);
    TestShadersWithSDRWhiteLevel(ProgramKey::SolidColor(NO_AA, true, false),
                                 200.f);
  }

  void TestColorShaders() {
    const size_t kNumTransferFns = 7;
    skcms_TransferFunction transfer_fns[kNumTransferFns] = {
        // The identity.
        {1.f, 1.f, 0.f, 1.f, 0.f, 0.f, 0.f},
        // The identity, with an if statement.
        {1.f, 1.f, 0.f, 1.f, 0.5f, 0.f, 0.f},
        // Just the power function.
        {1.1f, 1.f, 0.f, 1.f, 0.f, 0.f, 0.f},
        // Everything but the power function, nonlinear only.
        {1.f, 0.9f, 0.1f, 0.9f, 0.f, 0.1f, 0.1f},
        // Everything, nonlinear only.
        {1.1f, 0.9f, 0.1f, 0.9f, 0.f, 0.1f, 0.1f},
        // Everything but the power function.
        {1.f, 0.9f, 0.1f, 0.9f, 0.5f, 0.1f, 0.1f},
        // Everything.
        {1.1f, 0.9f, 0.1f, 0.9f, 0.5f, 0.1f, 0.1f},
    };

    for (size_t i = 0; i < kNumTransferFns; ++i) {
      skcms_Matrix3x3 primaries;
      gfx::ColorSpace::CreateSRGB().GetPrimaryMatrix(&primaries);
      gfx::ColorSpace src =
          gfx::ColorSpace::CreateCustom(primaries, transfer_fns[i]);

      renderer()->SetCurrentFrameForTesting(GLRenderer::DrawingFrame());
      renderer()->SetUseProgram(ProgramKey::SolidColor(NO_AA, false, false),
                                src, gfx::ColorSpace::CreateXYZD50());
      EXPECT_TRUE(renderer()->current_program_->initialized());
    }
  }

  void TestShadersWithPrecision(TexCoordPrecision precision) {
    // This program uses external textures and sampler, so it won't compile
    // everywhere.
    if (context_provider()->ContextCapabilities().egl_image_external) {
      TestShader(ProgramKey::VideoStream(precision, false));
    }
  }

  void TestShadersWithPrecisionAndBlend(TexCoordPrecision precision,
                                        BlendMode blend_mode) {
    TestShader(ProgramKey::RenderPass(precision, SAMPLER_TYPE_2D, blend_mode,
                                      NO_AA, NO_MASK, false, false, false,
                                      false));
    TestShader(ProgramKey::RenderPass(precision, SAMPLER_TYPE_2D, blend_mode,
                                      USE_AA, NO_MASK, false, false, false,
                                      false));
  }

  void TestShadersWithPrecisionAndSampler(
      TexCoordPrecision precision,
      SamplerType sampler,
      PremultipliedAlphaMode premultipliedAlpha,
      bool has_background_color,
      bool has_tex_clamp_rect) {
    TestShader(ProgramKey::Texture(precision, sampler, premultipliedAlpha,
                                   has_background_color, has_tex_clamp_rect,
                                   false, false));
  }

  void TestShadersWithPrecisionAndSamplerTiledAA(
      TexCoordPrecision precision,
      SamplerType sampler,
      PremultipliedAlphaMode premultipliedAlpha) {
    TestShader(ProgramKey::Tile(precision, sampler, USE_AA, premultipliedAlpha,
                                false, false, false, false));
  }

  void TestShadersWithPrecisionAndSamplerTiled(
      TexCoordPrecision precision,
      SamplerType sampler,
      PremultipliedAlphaMode premultipliedAlpha,
      bool is_opaque,
      bool has_tex_clamp_rect) {
    TestShader(ProgramKey::Tile(precision, sampler, NO_AA, premultipliedAlpha,
                                is_opaque, has_tex_clamp_rect, false, false));
  }

  void TestYUVShadersWithPrecisionAndSampler(TexCoordPrecision precision,
                                             SamplerType sampler) {
    // Iterate over alpha plane and nv12 parameters.
    UVTextureMode uv_modes[2] = {UV_TEXTURE_MODE_UV, UV_TEXTURE_MODE_U_V};
    YUVAlphaTextureMode a_modes[2] = {YUV_NO_ALPHA_TEXTURE,
                                      YUV_HAS_ALPHA_TEXTURE};
    for (auto uv_mode : uv_modes) {
      SCOPED_TRACE(uv_mode);
      for (auto a_mode : a_modes) {
        SCOPED_TRACE(a_mode);
        TestShader(ProgramKey::YUVVideo(precision, sampler, a_mode, uv_mode,
                                        false, false));
      }
    }
  }

  void TestShadersWithMasks(TexCoordPrecision precision,
                            SamplerType sampler,
                            BlendMode blend_mode,
                            bool mask_for_background) {
    TestShader(ProgramKey::RenderPass(precision, sampler, blend_mode, NO_AA,
                                      HAS_MASK, mask_for_background, false,
                                      false, false));
    TestShader(ProgramKey::RenderPass(precision, sampler, blend_mode, NO_AA,
                                      HAS_MASK, mask_for_background, true,
                                      false, false));
    TestShader(ProgramKey::RenderPass(precision, sampler, blend_mode, USE_AA,
                                      HAS_MASK, mask_for_background, false,
                                      false, false));
    TestShader(ProgramKey::RenderPass(precision, sampler, blend_mode, USE_AA,
                                      HAS_MASK, mask_for_background, true,
                                      false, false));
  }
};

namespace {

#if !defined(OS_ANDROID)
static const TexCoordPrecision kPrecisionList[] = {TEX_COORD_PRECISION_MEDIUM,
                                                   TEX_COORD_PRECISION_HIGH};

static const BlendMode kBlendModeList[LAST_BLEND_MODE + 1] = {
    BLEND_MODE_NONE,       BLEND_MODE_NORMAL,      BLEND_MODE_DESTINATION_IN,
    BLEND_MODE_SCREEN,     BLEND_MODE_OVERLAY,     BLEND_MODE_DARKEN,
    BLEND_MODE_LIGHTEN,    BLEND_MODE_COLOR_DODGE, BLEND_MODE_COLOR_BURN,
    BLEND_MODE_HARD_LIGHT, BLEND_MODE_SOFT_LIGHT,  BLEND_MODE_DIFFERENCE,
    BLEND_MODE_EXCLUSION,  BLEND_MODE_MULTIPLY,    BLEND_MODE_HUE,
    BLEND_MODE_SATURATION, BLEND_MODE_COLOR,       BLEND_MODE_LUMINOSITY,
};

static const SamplerType kSamplerList[] = {
    SAMPLER_TYPE_2D, SAMPLER_TYPE_2D_RECT, SAMPLER_TYPE_EXTERNAL_OES,
};

static const PremultipliedAlphaMode kPremultipliedAlphaModeList[] = {
    PREMULTIPLIED_ALPHA, NON_PREMULTIPLIED_ALPHA};

TEST_F(GLRendererShaderPixelTest, BasicShadersCompile) {
  TestBasicShaders();
}

TEST_F(GLRendererShaderPixelTest, TestColorShadersCompile) {
  TestColorShaders();
}

class PrecisionShaderPixelTest
    : public GLRendererShaderPixelTest,
      public ::testing::WithParamInterface<TexCoordPrecision> {};

TEST_P(PrecisionShaderPixelTest, ShadersCompile) {
  TestShadersWithPrecision(GetParam());
}

INSTANTIATE_TEST_SUITE_P(PrecisionShadersCompile,
                         PrecisionShaderPixelTest,
                         ::testing::ValuesIn(kPrecisionList));

class PrecisionBlendShaderPixelTest
    : public GLRendererShaderPixelTest,
      public ::testing::WithParamInterface<
          std::tuple<TexCoordPrecision, BlendMode>> {};

TEST_P(PrecisionBlendShaderPixelTest, ShadersCompile) {
  TestShadersWithPrecisionAndBlend(std::get<0>(GetParam()),
                                   std::get<1>(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    PrecisionBlendShadersCompile,
    PrecisionBlendShaderPixelTest,
    ::testing::Combine(::testing::ValuesIn(kPrecisionList),
                       ::testing::ValuesIn(kBlendModeList)));

class PrecisionSamplerShaderPixelTest
    : public GLRendererShaderPixelTest,
      public ::testing::WithParamInterface<
          std::tuple<TexCoordPrecision,
                     SamplerType,
                     PremultipliedAlphaMode,
                     bool,       // has_background_color
                     bool>> {};  // has_tex_clamp_rect

TEST_P(PrecisionSamplerShaderPixelTest, ShadersCompile) {
  SamplerType sampler = std::get<1>(GetParam());
  if (sampler != SAMPLER_TYPE_2D_RECT ||
      context_provider()->ContextCapabilities().texture_rectangle) {
    TestShadersWithPrecisionAndSampler(
        std::get<0>(GetParam()),  // TexCoordPrecision
        sampler,
        std::get<2>(GetParam()),   // PremultipliedAlphaMode
        std::get<3>(GetParam()),   // has_background_color
        std::get<4>(GetParam()));  // has_tex_clamp_rect
  }
}

INSTANTIATE_TEST_SUITE_P(
    PrecisionSamplerShadersCompile,
    PrecisionSamplerShaderPixelTest,
    ::testing::Combine(::testing::ValuesIn(kPrecisionList),
                       ::testing::ValuesIn(kSamplerList),
                       ::testing::ValuesIn(kPremultipliedAlphaModeList),
                       ::testing::Bool(),    // has_background_color
                       ::testing::Bool()));  // has_tex_clamp_rect

class PrecisionSamplerShaderPixelTestTiled
    : public GLRendererShaderPixelTest,
      public ::testing::WithParamInterface<
          std::tuple<TexCoordPrecision,
                     SamplerType,
                     PremultipliedAlphaMode,
                     bool,   // is_opaque
                     bool>>  // has_tex_clamp_rect
{};

TEST_P(PrecisionSamplerShaderPixelTestTiled, ShadersCompile) {
  SamplerType sampler = std::get<1>(GetParam());
  if (sampler != SAMPLER_TYPE_2D_RECT ||
      context_provider()->ContextCapabilities().texture_rectangle) {
    TestShadersWithPrecisionAndSamplerTiled(
        std::get<0>(GetParam()),  // TexCoordPrecision
        sampler,
        std::get<2>(GetParam()),   // PremultipliedAlphaMode
        std::get<3>(GetParam()),   // is_opaque
        std::get<4>(GetParam()));  // has_tex_clamp_rect
  }
}

INSTANTIATE_TEST_SUITE_P(
    PrecisionSamplerShadersCompile,
    PrecisionSamplerShaderPixelTestTiled,
    ::testing::Combine(::testing::ValuesIn(kPrecisionList),
                       ::testing::ValuesIn(kSamplerList),
                       ::testing::ValuesIn(kPremultipliedAlphaModeList),
                       ::testing::Bool(),    // is_opaque
                       ::testing::Bool()));  // has_tex_clamp_rect

class PrecisionSamplerShaderPixelTestTiledAA
    : public GLRendererShaderPixelTest,
      public ::testing::WithParamInterface<
          std::tuple<TexCoordPrecision, SamplerType, PremultipliedAlphaMode>> {
};

TEST_P(PrecisionSamplerShaderPixelTestTiledAA, ShadersCompile) {
  SamplerType sampler = std::get<1>(GetParam());
  if (sampler != SAMPLER_TYPE_2D_RECT ||
      context_provider()->ContextCapabilities().texture_rectangle) {
    TestShadersWithPrecisionAndSamplerTiledAA(
        std::get<0>(GetParam()),  // TexCoordPrecision
        sampler,
        std::get<2>(GetParam()));  // PremultipliedAlphaMode
  }
}

INSTANTIATE_TEST_SUITE_P(
    PrecisionSamplerShadersCompile,
    PrecisionSamplerShaderPixelTestTiledAA,
    ::testing::Combine(::testing::ValuesIn(kPrecisionList),
                       ::testing::ValuesIn(kSamplerList),
                       ::testing::ValuesIn(kPremultipliedAlphaModeList)));

class PrecisionSamplerYUVShaderPixelTest
    : public GLRendererShaderPixelTest,
      public ::testing::WithParamInterface<
          std::tuple<TexCoordPrecision, SamplerType>> {};

TEST_P(PrecisionSamplerYUVShaderPixelTest, ShadersCompile) {
  SamplerType sampler = std::get<1>(GetParam());
  if (sampler != SAMPLER_TYPE_2D_RECT ||
      context_provider()->ContextCapabilities().texture_rectangle) {
    TestYUVShadersWithPrecisionAndSampler(
        std::get<0>(GetParam()),  // TexCoordPrecision
        sampler);
  }
}

INSTANTIATE_TEST_SUITE_P(PrecisionSamplerShadersCompile,
                         PrecisionSamplerYUVShaderPixelTest,
                         ::testing::Combine(::testing::ValuesIn(kPrecisionList),
                                            ::testing::ValuesIn(kSamplerList)));

class MaskShaderPixelTest
    : public GLRendererShaderPixelTest,
      public ::testing::WithParamInterface<
          std::tuple<TexCoordPrecision, SamplerType, BlendMode, bool>> {};

TEST_P(MaskShaderPixelTest, ShadersCompile) {
  SamplerType sampler = std::get<1>(GetParam());
  if (sampler != SAMPLER_TYPE_2D_RECT ||
      context_provider()->ContextCapabilities().texture_rectangle) {
    TestShadersWithMasks(std::get<0>(GetParam()), sampler,
                         std::get<2>(GetParam()), std::get<3>(GetParam()));
  }
}

INSTANTIATE_TEST_SUITE_P(MaskShadersCompile,
                         MaskShaderPixelTest,
                         ::testing::Combine(::testing::ValuesIn(kPrecisionList),
                                            ::testing::ValuesIn(kSamplerList),
                                            ::testing::ValuesIn(kBlendModeList),
                                            ::testing::Bool()));

#endif

class FakeRendererGL : public GLRenderer {
 public:
  FakeRendererGL(const RendererSettings* settings,
                 const DebugRendererSettings* debug_settings,
                 OutputSurface* output_surface,
                 DisplayResourceProviderGL* resource_provider)
      : GLRenderer(settings,
                   debug_settings,
                   output_surface,
                   resource_provider,
                   nullptr,
                   nullptr) {}

  FakeRendererGL(const RendererSettings* settings,
                 const DebugRendererSettings* debug_settings,
                 OutputSurface* output_surface,
                 DisplayResourceProviderGL* resource_provider,
                 OverlayProcessorInterface* overlay_processor)
      : GLRenderer(settings,
                   debug_settings,
                   output_surface,
                   resource_provider,
                   overlay_processor,
                   nullptr) {}

  FakeRendererGL(
      const RendererSettings* settings,
      const DebugRendererSettings* debug_settings,
      OutputSurface* output_surface,
      DisplayResourceProviderGL* resource_provider,
      OverlayProcessorInterface* overlay_processor,
      scoped_refptr<base::SingleThreadTaskRunner> current_task_runner)
      : GLRenderer(settings,
                   debug_settings,
                   output_surface,
                   resource_provider,
                   overlay_processor,
                   std::move(current_task_runner)) {}

  // GLRenderer methods.

  // Changing visibility to public.
  using GLRenderer::stencil_enabled;
};

class GLRendererWithDefaultHarnessTest : public GLRendererTest {
 protected:
  GLRendererWithDefaultHarnessTest() {
    output_surface_ = FakeOutputSurface::Create3d();
    output_surface_->BindToClient(&output_surface_client_);

    resource_provider_ = std::make_unique<DisplayResourceProviderGL>(
        output_surface_->context_provider());
    renderer_ = std::make_unique<FakeRendererGL>(&settings_, &debug_settings_,
                                                 output_surface_.get(),
                                                 resource_provider_.get());
    renderer_->Initialize();
    renderer_->SetVisible(true);
  }

  void SwapBuffers() { renderer_->SwapBuffers({}); }

  RendererSettings settings_;
  cc::FakeOutputSurfaceClient output_surface_client_;
  std::unique_ptr<FakeOutputSurface> output_surface_;
  std::unique_ptr<DisplayResourceProviderGL> resource_provider_;
  std::unique_ptr<FakeRendererGL> renderer_;
};

// Closing the namespace here so that GLRendererShaderTest can take advantage
// of the friend relationship with GLRenderer and all of the mock classes
// declared above it.
}  // namespace

class GLRendererShaderTest : public GLRendererTest {
 protected:
  GLRendererShaderTest() {
    output_surface_ = FakeOutputSurface::Create3d();
    output_surface_->BindToClient(&output_surface_client_);

    resource_provider_ = std::make_unique<DisplayResourceProviderGL>(
        output_surface_->context_provider());
    renderer_ = std::make_unique<FakeRendererGL>(
        &settings_, &debug_settings_, output_surface_.get(),
        resource_provider_.get(), nullptr);
    renderer_->Initialize();
    renderer_->SetVisible(true);

    child_context_provider_ = TestContextProvider::Create();
    child_context_provider_->BindToCurrentThread();
    child_resource_provider_ = std::make_unique<ClientResourceProvider>();
  }

  ~GLRendererShaderTest() override {
    child_resource_provider_->ShutdownAndReleaseAllResources();
  }

  void TestRenderPassProgram(TexCoordPrecision precision,
                             BlendMode blend_mode) {
    const Program* program = renderer_->GetProgramIfInitialized(
        ProgramKey::RenderPass(precision, SAMPLER_TYPE_2D, blend_mode, NO_AA,
                               NO_MASK, false, false, false, false));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  void TestRenderPassColorMatrixProgram(TexCoordPrecision precision,
                                        BlendMode blend_mode) {
    const Program* program = renderer_->GetProgramIfInitialized(
        ProgramKey::RenderPass(precision, SAMPLER_TYPE_2D, blend_mode, NO_AA,
                               NO_MASK, false, true, false, false));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  void TestRenderPassMaskProgram(TexCoordPrecision precision,
                                 SamplerType sampler,
                                 BlendMode blend_mode) {
    const Program* program = renderer_->GetProgramIfInitialized(
        ProgramKey::RenderPass(precision, sampler, blend_mode, NO_AA, HAS_MASK,
                               false, false, false, false));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  void TestRenderPassMaskColorMatrixProgram(TexCoordPrecision precision,
                                            SamplerType sampler,
                                            BlendMode blend_mode) {
    const Program* program = renderer_->GetProgramIfInitialized(
        ProgramKey::RenderPass(precision, sampler, blend_mode, NO_AA, HAS_MASK,
                               false, true, false, false));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  void TestRenderPassProgramAA(TexCoordPrecision precision,
                               BlendMode blend_mode) {
    const Program* program = renderer_->GetProgramIfInitialized(
        ProgramKey::RenderPass(precision, SAMPLER_TYPE_2D, blend_mode, USE_AA,
                               NO_MASK, false, false, false, false));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  void TestRenderPassColorMatrixProgramAA(TexCoordPrecision precision,
                                          BlendMode blend_mode) {
    const Program* program = renderer_->GetProgramIfInitialized(
        ProgramKey::RenderPass(precision, SAMPLER_TYPE_2D, blend_mode, USE_AA,
                               NO_MASK, false, true, false, false));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  void TestRenderPassMaskProgramAA(TexCoordPrecision precision,
                                   SamplerType sampler,
                                   BlendMode blend_mode) {
    const Program* program = renderer_->GetProgramIfInitialized(
        ProgramKey::RenderPass(precision, sampler, blend_mode, USE_AA, HAS_MASK,
                               false, false, false, false));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  void TestRenderPassMaskColorMatrixProgramAA(TexCoordPrecision precision,
                                              SamplerType sampler,
                                              BlendMode blend_mode) {
    const Program* program = renderer_->GetProgramIfInitialized(
        ProgramKey::RenderPass(precision, sampler, blend_mode, USE_AA, HAS_MASK,
                               false, true, false, false));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  void TestSolidColorProgramAA() {
    const Program* program = renderer_->GetProgramIfInitialized(
        ProgramKey::SolidColor(USE_AA, false, false));
    EXPECT_PROGRAM_VALID(program);
    EXPECT_EQ(program, renderer_->current_program_);
  }

  RendererSettings settings_;
  cc::FakeOutputSurfaceClient output_surface_client_;
  std::unique_ptr<FakeOutputSurface> output_surface_;
  std::unique_ptr<DisplayResourceProviderGL> resource_provider_;
  scoped_refptr<TestContextProvider> child_context_provider_;
  std::unique_ptr<ClientResourceProvider> child_resource_provider_;
  std::unique_ptr<FakeRendererGL> renderer_;
};

namespace {

TEST_F(GLRendererWithDefaultHarnessTest, ExternalStencil) {
  gfx::Size viewport_size(1, 1);
  EXPECT_FALSE(renderer_->stencil_enabled());

  output_surface_->set_has_external_stencil_test(true);

  auto* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, AggregatedRenderPassId{1},
      gfx::Rect(viewport_size), gfx::Transform(), cc::FilterOperations());
  root_pass->has_transparent_background = false;

  DrawFrame(renderer_.get(), viewport_size);
  EXPECT_TRUE(renderer_->stencil_enabled());
}

TEST_F(GLRendererWithDefaultHarnessTest, TextureDrawQuadShaderPrecisionHigh) {
  // TestContextProvider, used inside FakeOuputSurfaceClient, redefines
  // GetShaderPrecisionFormat() and sets the resolution of mediump with
  // 10-bits (1024). So any value higher than 1024 should use highp.
  // The goal is to make sure the fragment shaders used in DoDrawQuad() use
  // the correct precision qualifier.

  const gfx::Size viewport_size(1, 1);
  auto* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, AggregatedRenderPassId{1},
      gfx::Rect(viewport_size), gfx::Transform(), cc::FilterOperations());

  const bool needs_blending = false;
  const bool premultiplied_alpha = false;
  const bool flipped = false;
  const bool nearest_neighbor = false;
  const float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  const gfx::PointF uv_top_left(0, 0);
  const gfx::PointF uv_bottom_right(1, 1);

  auto child_context_provider = TestContextProvider::Create();
  child_context_provider->BindToCurrentThread();

  auto child_resource_provider = std::make_unique<ClientResourceProvider>();

  // Here is where the texture is created. Any value bigger than 1024 should use
  // a highp.
  auto transfer_resource = TransferableResource::MakeGL(
      gpu::Mailbox::Generate(), GL_LINEAR, GL_TEXTURE_2D, gpu::SyncToken(),
      gfx::Size(1025, 1025), true);
  ResourceId client_resource_id = child_resource_provider->ImportResource(
      transfer_resource, SingleReleaseCallback::Create(base::DoNothing()));

  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {client_resource_id}, resource_provider_.get(),
          child_resource_provider.get(), child_context_provider.get());
  ResourceId resource_id = resource_map[client_resource_id];

  // The values defined here should not alter the size of the already created
  // texture.
  TextureDrawQuad* overlay_quad =
      root_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  SharedQuadState* shared_state = root_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), gfx::Rect(viewport_size),
                       gfx::Rect(1023, 1023), gfx::MaskFilterInfo(),
                       gfx::Rect(1023, 1023), false, false, 1,
                       SkBlendMode::kSrcOver, 0);
  overlay_quad->SetNew(shared_state, gfx::Rect(1023, 1023),
                       gfx::Rect(1023, 1023), needs_blending, resource_id,
                       premultiplied_alpha, uv_top_left, uv_bottom_right,
                       SK_ColorTRANSPARENT, vertex_opacity, flipped,
                       nearest_neighbor, /*secure_output_only=*/false,
                       gfx::ProtectedVideoType::kClear);

  DrawFrame(renderer_.get(), viewport_size);

  TexCoordPrecision precision = get_cached_tex_coord_precision(renderer_.get());
  EXPECT_EQ(precision, TEX_COORD_PRECISION_HIGH);

  child_resource_provider->ShutdownAndReleaseAllResources();
}

TEST_F(GLRendererWithDefaultHarnessTest, TextureDrawQuadShaderPrecisionMedium) {
  // TestContextProvider, used inside FakeOuputSurfaceClient, redefines
  // GetShaderPrecisionFormat() and sets the resolution of mediump with
  // 10-bits (1024). So any value higher than 1024 should use highp.
  // The goal is to make sure the fragment shaders used in DoDrawQuad() use
  // the correct precision qualifier.

  const gfx::Size viewport_size(1, 1);
  auto* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, AggregatedRenderPassId{1},
      gfx::Rect(viewport_size), gfx::Transform(), cc::FilterOperations());

  const bool needs_blending = false;
  const bool premultiplied_alpha = false;
  const bool flipped = false;
  const bool nearest_neighbor = false;
  const float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  const gfx::PointF uv_top_left(0, 0);
  const gfx::PointF uv_bottom_right(1, 1);

  auto child_context_provider = TestContextProvider::Create();
  child_context_provider->BindToCurrentThread();

  auto child_resource_provider = std::make_unique<ClientResourceProvider>();

  // Here is where the texture is created. Any value smaller than 1024 should
  // use a mediump.
  auto transfer_resource = TransferableResource::MakeGL(
      gpu::Mailbox::Generate(), GL_LINEAR, GL_TEXTURE_2D, gpu::SyncToken(),
      gfx::Size(1023, 1023), true);
  ResourceId client_resource_id = child_resource_provider->ImportResource(
      transfer_resource, SingleReleaseCallback::Create(base::DoNothing()));

  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {client_resource_id}, resource_provider_.get(),
          child_resource_provider.get(), child_context_provider.get());
  ResourceId resource_id = resource_map[client_resource_id];

  // The values defined here should not alter the size of the already created
  // texture.
  TextureDrawQuad* overlay_quad =
      root_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  SharedQuadState* shared_state = root_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), gfx::Rect(viewport_size),
                       gfx::Rect(1025, 1025), gfx::MaskFilterInfo(),
                       gfx::Rect(1025, 1025), false, false, 1,
                       SkBlendMode::kSrcOver, 0);
  overlay_quad->SetNew(shared_state, gfx::Rect(1025, 1025),
                       gfx::Rect(1025, 1025), needs_blending, resource_id,
                       premultiplied_alpha, uv_top_left, uv_bottom_right,
                       SK_ColorTRANSPARENT, vertex_opacity, flipped,
                       nearest_neighbor, /*secure_output_only=*/false,
                       gfx::ProtectedVideoType::kClear);

  DrawFrame(renderer_.get(), viewport_size);

  TexCoordPrecision precision = get_cached_tex_coord_precision(renderer_.get());
  EXPECT_EQ(precision, TEX_COORD_PRECISION_MEDIUM);

  child_resource_provider->ShutdownAndReleaseAllResources();
}

class GLRendererTextureDrawQuadHDRTest
    : public GLRendererWithDefaultHarnessTest {
 protected:
  void RunTest(bool is_video_frame) {
    const gfx::Size viewport_size(10, 10);
    auto* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, AggregatedRenderPassId{1},
        gfx::Rect(viewport_size), gfx::Transform(), cc::FilterOperations());

    const bool needs_blending = false;
    const bool premultiplied_alpha = false;
    const bool flipped = false;
    const bool nearest_neighbor = false;
    const float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    const gfx::PointF uv_top_left(0, 0);
    const gfx::PointF uv_bottom_right(1, 1);

    auto child_context_provider = TestContextProvider::Create();
    child_context_provider->BindToCurrentThread();

    auto child_resource_provider = std::make_unique<ClientResourceProvider>();

    constexpr gfx::Size kTextureSize = gfx::Size(10, 10);
    auto transfer_resource = TransferableResource::MakeGL(
        gpu::Mailbox::Generate(), GL_LINEAR, GL_TEXTURE_2D, gpu::SyncToken(),
        kTextureSize, true);
    transfer_resource.color_space = gfx::ColorSpace::CreateSCRGBLinear();
    ResourceId client_resource_id = child_resource_provider->ImportResource(
        transfer_resource, SingleReleaseCallback::Create(base::DoNothing()));

    std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
        cc::SendResourceAndGetChildToParentMap(
            {client_resource_id}, resource_provider_.get(),
            child_resource_provider.get(), child_context_provider.get());
    ResourceId resource_id = resource_map[client_resource_id];

    TextureDrawQuad* overlay_quad =
        root_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
    SharedQuadState* shared_state = root_pass->CreateAndAppendSharedQuadState();
    shared_state->SetAll(gfx::Transform(), gfx::Rect(viewport_size),
                         gfx::Rect(kTextureSize), gfx::MaskFilterInfo(),
                         gfx::Rect(kTextureSize), false, false, 1,
                         SkBlendMode::kSrcOver, 0);
    overlay_quad->SetNew(shared_state, gfx::Rect(kTextureSize),
                         gfx::Rect(kTextureSize), needs_blending, resource_id,
                         premultiplied_alpha, uv_top_left, uv_bottom_right,
                         SK_ColorTRANSPARENT, vertex_opacity, flipped,
                         nearest_neighbor, /*secure_output_only=*/false,
                         gfx::ProtectedVideoType::kClear);
    overlay_quad->is_video_frame = is_video_frame;

    constexpr float kSDRWhiteLevel = 123.0f;
    gfx::DisplayColorSpaces display_color_spaces;
    display_color_spaces.SetSDRWhiteLevel(kSDRWhiteLevel);

    DrawFrame(renderer_.get(), viewport_size, display_color_spaces);

    const Program* program = current_program(renderer_.get());
    DCHECK(program);
    DCHECK(program->color_transform_for_testing())
        << program->fragment_shader().GetShaderString();

    const gfx::ColorSpace expected_src_color_space =
        is_video_frame
            ? gfx::ColorSpace::CreateSCRGBLinear().GetWithSDRWhiteLevel(
                  kSDRWhiteLevel)
            : gfx::ColorSpace::CreateSCRGBLinear();
    EXPECT_EQ(program->color_transform_for_testing()->GetSrcColorSpace(),
              expected_src_color_space);

    child_resource_provider->ShutdownAndReleaseAllResources();
  }
};

TEST_F(GLRendererTextureDrawQuadHDRTest, VideoFrame) {
  RunTest(/*is_video_frame=*/true);
}

TEST_F(GLRendererTextureDrawQuadHDRTest, NotVideoFrame) {
  RunTest(/*is_video_frame=*/false);
}

class ForbidSynchronousCallGLES2Interface : public TestGLES2Interface {
 public:
  ForbidSynchronousCallGLES2Interface() = default;

  void GetAttachedShaders(GLuint program,
                          GLsizei max_count,
                          GLsizei* count,
                          GLuint* shaders) override {
    ADD_FAILURE();
  }

  GLint GetAttribLocation(GLuint program, const GLchar* name) override {
    ADD_FAILURE();
    return 0;
  }

  void GetBooleanv(GLenum pname, GLboolean* value) override { ADD_FAILURE(); }

  void GetBufferParameteriv(GLenum target,
                            GLenum pname,
                            GLint* value) override {
    ADD_FAILURE();
  }

  GLenum GetError() override {
    ADD_FAILURE();
    return GL_NO_ERROR;
  }

  void GetFloatv(GLenum pname, GLfloat* value) override { ADD_FAILURE(); }

  void GetFramebufferAttachmentParameteriv(GLenum target,
                                           GLenum attachment,
                                           GLenum pname,
                                           GLint* value) override {
    ADD_FAILURE();
  }

  void GetIntegerv(GLenum pname, GLint* value) override {
    if (pname == GL_MAX_TEXTURE_SIZE) {
      // MAX_TEXTURE_SIZE is cached client side, so it's OK to query.
      *value = 1024;
    } else {
      ADD_FAILURE();
    }
  }

  // We allow querying the shader compilation and program link status in debug
  // mode, but not release.
  void GetProgramiv(GLuint program, GLenum pname, GLint* value) override {
    ADD_FAILURE();
  }

  void GetShaderiv(GLuint shader, GLenum pname, GLint* value) override {
    ADD_FAILURE();
  }

  void GetRenderbufferParameteriv(GLenum target,
                                  GLenum pname,
                                  GLint* value) override {
    ADD_FAILURE();
  }

  void GetShaderPrecisionFormat(GLenum shadertype,
                                GLenum precisiontype,
                                GLint* range,
                                GLint* precision) override {
    ADD_FAILURE();
  }

  void GetTexParameterfv(GLenum target, GLenum pname, GLfloat* value) override {
    ADD_FAILURE();
  }

  void GetTexParameteriv(GLenum target, GLenum pname, GLint* value) override {
    ADD_FAILURE();
  }

  void GetUniformfv(GLuint program, GLint location, GLfloat* value) override {
    ADD_FAILURE();
  }

  void GetUniformiv(GLuint program, GLint location, GLint* value) override {
    ADD_FAILURE();
  }

  GLint GetUniformLocation(GLuint program, const GLchar* name) override {
    ADD_FAILURE();
    return 0;
  }

  void GetVertexAttribfv(GLuint index, GLenum pname, GLfloat* value) override {
    ADD_FAILURE();
  }

  void GetVertexAttribiv(GLuint index, GLenum pname, GLint* value) override {
    ADD_FAILURE();
  }

  void GetVertexAttribPointerv(GLuint index,
                               GLenum pname,
                               void** pointer) override {
    ADD_FAILURE();
  }
};

TEST_F(GLRendererTest, InitializationDoesNotMakeSynchronousCalls) {
  auto gl_owned = std::make_unique<ForbidSynchronousCallGLES2Interface>();
  auto provider = TestContextProvider::Create(std::move(gl_owned));
  provider->BindToCurrentThread();

  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  auto resource_provider = std::make_unique<DisplayResourceProviderGL>(
      output_surface->context_provider());

  RendererSettings settings;
  FakeRendererGL renderer(&settings, &debug_settings_, output_surface.get(),
                          resource_provider.get());
}

class LoseContextOnFirstGetGLES2Interface : public TestGLES2Interface {
 public:
  LoseContextOnFirstGetGLES2Interface() {}

  void GetProgramiv(GLuint program, GLenum pname, GLint* value) override {
    LoseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                        GL_INNOCENT_CONTEXT_RESET_ARB);
    *value = 0;
  }

  void GetShaderiv(GLuint shader, GLenum pname, GLint* value) override {
    LoseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                        GL_INNOCENT_CONTEXT_RESET_ARB);
    *value = 0;
  }
};

TEST_F(GLRendererTest, InitializationWithQuicklyLostContextDoesNotAssert) {
  auto gl_owned = std::make_unique<LoseContextOnFirstGetGLES2Interface>();
  auto provider = TestContextProvider::Create(std::move(gl_owned));
  provider->BindToCurrentThread();

  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  auto resource_provider = std::make_unique<DisplayResourceProviderGL>(
      output_surface->context_provider());

  RendererSettings settings;
  FakeRendererGL renderer(&settings, &debug_settings_, output_surface.get(),
                          resource_provider.get());
}

class ClearCountingGLES2Interface : public TestGLES2Interface {
 public:
  ClearCountingGLES2Interface() = default;

  MOCK_METHOD3(DiscardFramebufferEXT,
               void(GLenum target,
                    GLsizei numAttachments,
                    const GLenum* attachments));
  MOCK_METHOD1(Clear, void(GLbitfield mask));
};

TEST_F(GLRendererTest, OpaqueBackground) {
  auto gl_owned = std::make_unique<ClearCountingGLES2Interface>();
  gl_owned->set_have_discard_framebuffer(true);

  auto* gl = gl_owned.get();

  auto provider = TestContextProvider::Create(std::move(gl_owned));
  provider->BindToCurrentThread();

  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  auto resource_provider = std::make_unique<DisplayResourceProviderGL>(
      output_surface->context_provider());

  RendererSettings settings;
  FakeRendererGL renderer(&settings, &debug_settings_, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  renderer.SetVisible(true);

  gfx::Size viewport_size(1, 1);
  AggregatedRenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, AggregatedRenderPassId{1},
      gfx::Rect(viewport_size), gfx::Transform(), cc::FilterOperations());
  root_pass->has_transparent_background = false;

  // On DEBUG builds, render passes with opaque background clear to blue to
  // easily see regions that were not drawn on the screen.
  EXPECT_CALL(*gl, DiscardFramebufferEXT(GL_FRAMEBUFFER, _, _))
      .With(Args<2, 1>(ElementsAre(GL_COLOR_EXT)))
      .Times(1);
#ifdef NDEBUG
  EXPECT_CALL(*gl, Clear(_)).Times(0);
#else
  EXPECT_CALL(*gl, Clear(_)).Times(1);
#endif
  DrawFrame(&renderer, viewport_size);
  Mock::VerifyAndClearExpectations(gl);
}

TEST_F(GLRendererTest, TransparentBackground) {
  auto gl_owned = std::make_unique<ClearCountingGLES2Interface>();
  auto* gl = gl_owned.get();
  gl_owned->set_have_discard_framebuffer(true);

  auto provider = TestContextProvider::Create(std::move(gl_owned));
  provider->BindToCurrentThread();

  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  auto resource_provider = std::make_unique<DisplayResourceProviderGL>(
      output_surface->context_provider());

  RendererSettings settings;
  FakeRendererGL renderer(&settings, &debug_settings_, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  renderer.SetVisible(true);

  gfx::Size viewport_size(1, 1);
  AggregatedRenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, AggregatedRenderPassId{1},
      gfx::Rect(viewport_size), gfx::Transform(), cc::FilterOperations());
  root_pass->has_transparent_background = true;

  EXPECT_CALL(*gl, DiscardFramebufferEXT(GL_FRAMEBUFFER, 1, _)).Times(1);
  EXPECT_CALL(*gl, Clear(_)).Times(1);
  DrawFrame(&renderer, viewport_size);

  Mock::VerifyAndClearExpectations(gl);
}

TEST_F(GLRendererTest, OffscreenOutputSurface) {
  auto gl_owned = std::make_unique<ClearCountingGLES2Interface>();
  auto* gl = gl_owned.get();
  gl_owned->set_have_discard_framebuffer(true);

  auto provider = TestContextProvider::Create(std::move(gl_owned));
  provider->BindToCurrentThread();

  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::CreateOffscreen(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  auto resource_provider = std::make_unique<DisplayResourceProviderGL>(
      output_surface->context_provider());

  RendererSettings settings;
  FakeRendererGL renderer(&settings, &debug_settings_, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  renderer.SetVisible(true);

  gfx::Size viewport_size(1, 1);
  cc::AddRenderPass(&render_passes_in_draw_order_, AggregatedRenderPassId{1},
                    gfx::Rect(viewport_size), gfx::Transform(),
                    cc::FilterOperations());

  EXPECT_CALL(*gl, DiscardFramebufferEXT(GL_FRAMEBUFFER, _, _))
      .With(Args<2, 1>(ElementsAre(GL_COLOR_ATTACHMENT0)))
      .Times(1);
  EXPECT_CALL(*gl, Clear(_)).Times(AnyNumber());
  DrawFrame(&renderer, viewport_size);
  Mock::VerifyAndClearExpectations(gl);
}

class TextureStateTrackingGLES2Interface : public TestGLES2Interface {
 public:
  TextureStateTrackingGLES2Interface() : active_texture_(GL_INVALID_ENUM) {}

  MOCK_METHOD1(WaitSyncTokenCHROMIUM, void(const GLbyte* sync_token));
  MOCK_METHOD3(TexParameteri, void(GLenum target, GLenum pname, GLint param));
  MOCK_METHOD4(
      DrawElements,
      void(GLenum mode, GLsizei count, GLenum type, const void* indices));

  void ActiveTexture(GLenum texture) override {
    EXPECT_NE(texture, active_texture_);
    active_texture_ = texture;
  }

  GLenum active_texture() const { return active_texture_; }

 private:
  GLenum active_texture_;
};

#define EXPECT_FILTER_CALL(filter)                                          \
  EXPECT_CALL(*gl,                                                          \
              TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter)); \
  EXPECT_CALL(*gl, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter));

TEST_F(GLRendererTest, ActiveTextureState) {
  auto child_gl_owned = std::make_unique<TextureStateTrackingGLES2Interface>();
  auto child_context_provider =
      TestContextProvider::Create(std::move(child_gl_owned));
  child_context_provider->BindToCurrentThread();
  auto child_resource_provider = std::make_unique<ClientResourceProvider>();

  auto gl_owned = std::make_unique<TextureStateTrackingGLES2Interface>();
  gl_owned->set_have_extension_egl_image(true);
  auto* gl = gl_owned.get();

  auto provider = TestContextProvider::Create(std::move(gl_owned));
  provider->BindToCurrentThread();

  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  auto resource_provider = std::make_unique<DisplayResourceProviderGL>(
      output_surface->context_provider());

  RendererSettings settings;
  FakeRendererGL renderer(&settings, &debug_settings_, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  renderer.SetVisible(true);

  // During initialization we are allowed to set any texture parameters.
  EXPECT_CALL(*gl, TexParameteri(_, _, _)).Times(AnyNumber());

  AggregatedRenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, AggregatedRenderPassId{1},
      gfx::Rect(100, 100), gfx::Transform(), cc::FilterOperations());
  gpu::SyncToken mailbox_sync_token;
  cc::AddOneOfEveryQuadTypeInDisplayResourceProvider(
      root_pass, resource_provider.get(), child_resource_provider.get(),
      child_context_provider.get(), AggregatedRenderPassId{0},
      &mailbox_sync_token);

  EXPECT_EQ(12u, resource_provider->num_resources());
  renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  // Set up expected texture filter state transitions that match the quads
  // created in AppendOneOfEveryQuadType().
  Mock::VerifyAndClearExpectations(gl);
  {
    InSequence sequence;
    // The verified flush flag will be set by
    // ClientResourceProvider::PrepareSendToParent. Before checking if
    // the gpu::SyncToken matches, set this flag first.
    mailbox_sync_token.SetVerifyFlush();
    // In AddOneOfEveryQuadTypeInDisplayResourceProvider, resources are added
    // into RenderPass with the below order: resource6, resource1, resource8
    // (with mailbox), resource2, resource3, resource4, resource9, resource10,
    // resource11, resource12. resource8 has its own mailbox mailbox_sync_token.
    // The rest resources share a common default sync token.
    EXPECT_CALL(*gl, WaitSyncTokenCHROMIUM(_)).Times(2);
    EXPECT_CALL(*gl,
                WaitSyncTokenCHROMIUM(MatchesSyncToken(mailbox_sync_token)))
        .Times(1);
    EXPECT_CALL(*gl, WaitSyncTokenCHROMIUM(_)).Times(7);

    // yuv_quad is drawn with the default linear filter.
    for (int i = 0; i < 4; ++i) {
      EXPECT_FILTER_CALL(GL_LINEAR);
    }
    EXPECT_CALL(*gl, DrawElements(_, _, _, _));

    // tile_quad is drawn with GL_NEAREST because it is not transformed or
    // scaled.
    EXPECT_FILTER_CALL(GL_NEAREST);
    EXPECT_CALL(*gl, DrawElements(_, _, _, _));

    // transformed tile_quad
    EXPECT_FILTER_CALL(GL_LINEAR);
    EXPECT_CALL(*gl, DrawElements(_, _, _, _));

    // scaled tile_quad
    EXPECT_FILTER_CALL(GL_LINEAR);
    EXPECT_CALL(*gl, DrawElements(_, _, _, _));

    // texture_quad without nearest neighbor
    EXPECT_FILTER_CALL(GL_LINEAR);
    EXPECT_CALL(*gl, DrawElements(_, _, _, _));

    // texture_quad without nearest neighbor
    EXPECT_FILTER_CALL(GL_LINEAR);
    EXPECT_CALL(*gl, DrawElements(_, _, _, _));

    if (features::IsUsingFastPathForSolidColorQuad()) {
      // stream video and debug draw quads
      EXPECT_CALL(*gl, DrawElements(_, _, _, _)).Times(2);
    } else {
      // stream video, solid color, and debug draw quads
      EXPECT_CALL(*gl, DrawElements(_, _, _, _)).Times(3);
    }
  }

  gfx::Size viewport_size(100, 100);
  DrawFrame(&renderer, viewport_size);
  Mock::VerifyAndClearExpectations(gl);

  child_resource_provider->ShutdownAndReleaseAllResources();
}

class BufferSubDataTrackingGLES2Interface : public TestGLES2Interface {
 public:
  BufferSubDataTrackingGLES2Interface() = default;
  ~BufferSubDataTrackingGLES2Interface() override = default;

  void BufferSubData(GLenum target,
                     GLintptr offset,
                     GLsizeiptr size,
                     const void* data) override {
    if (target != GL_ARRAY_BUFFER)
      return;
    DCHECK_EQ(0, offset);
    last_array_data.resize(size);
    memcpy(last_array_data.data(), data, size);
  }

  std::vector<uint8_t> last_array_data;
};

TEST_F(GLRendererTest, DrawYUVVideoDrawQuadWithVisibleRect) {
  gfx::Size viewport_size(100, 100);

  auto mock_gl_owned = std::make_unique<BufferSubDataTrackingGLES2Interface>();
  BufferSubDataTrackingGLES2Interface* mock_gl = mock_gl_owned.get();
  auto provider = TestContextProvider::Create(std::move(mock_gl_owned));
  provider->BindToCurrentThread();

  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  auto resource_provider = std::make_unique<DisplayResourceProviderGL>(
      output_surface->context_provider());

  RendererSettings settings;
  FakeRendererGL renderer(&settings, &debug_settings_, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  renderer.SetVisible(true);

  AggregatedRenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, AggregatedRenderPassId{1},
      gfx::Rect(viewport_size), gfx::Transform(), cc::FilterOperations());
  root_pass->has_transparent_background = false;

  gfx::Rect rect(viewport_size);
  gfx::Rect visible_rect(rect);
  gfx::RectF tex_coord_rect(0, 0, 1, 1);
  visible_rect.Inset(10, 20, 30, 40);

  SharedQuadState* shared_state = root_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), gfx::Rect(), rect,
                       gfx::MaskFilterInfo(), rect, false, false, 1,
                       SkBlendMode::kSrcOver, 0);

  YUVVideoDrawQuad* quad =
      root_pass->CreateAndAppendDrawQuad<YUVVideoDrawQuad>();
  quad->SetNew(shared_state, rect, visible_rect, /*needs_blending=*/false,
               tex_coord_rect, tex_coord_rect, rect.size(), rect.size(),
               ResourceId(1), ResourceId(1), ResourceId(1), ResourceId(1),
               gfx::ColorSpace(), 0, 1.0, 8);

  DrawFrame(&renderer, viewport_size);

  ASSERT_EQ(96u, mock_gl->last_array_data.size());
  float* geometry_binding_vertexes =
      reinterpret_cast<float*>(mock_gl->last_array_data.data());

  const double kEpsilon = 1e-6;
  EXPECT_NEAR(-0.4f, geometry_binding_vertexes[0], kEpsilon);
  EXPECT_NEAR(-0.3f, geometry_binding_vertexes[1], kEpsilon);
  EXPECT_NEAR(0.1f, geometry_binding_vertexes[3], kEpsilon);
  EXPECT_NEAR(0.2f, geometry_binding_vertexes[4], kEpsilon);

  EXPECT_NEAR(0.2f, geometry_binding_vertexes[12], kEpsilon);
  EXPECT_NEAR(0.1f, geometry_binding_vertexes[13], kEpsilon);
  EXPECT_NEAR(0.7f, geometry_binding_vertexes[15], kEpsilon);
  EXPECT_NEAR(0.6f, geometry_binding_vertexes[16], kEpsilon);
}

class NoClearRootRenderPassMockGLES2Interface : public TestGLES2Interface {
 public:
  MOCK_METHOD1(Clear, void(GLbitfield mask));
  MOCK_METHOD4(
      DrawElements,
      void(GLenum mode, GLsizei count, GLenum type, const void* indices));
};

TEST_F(GLRendererTest, ShouldClearRootRenderPass) {
  auto mock_gl_owned =
      std::make_unique<NoClearRootRenderPassMockGLES2Interface>();
  NoClearRootRenderPassMockGLES2Interface* mock_gl = mock_gl_owned.get();

  auto provider = TestContextProvider::Create(std::move(mock_gl_owned));
  provider->BindToCurrentThread();

  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  auto resource_provider = std::make_unique<DisplayResourceProviderGL>(
      output_surface->context_provider());

  RendererSettings settings;
  settings.should_clear_root_render_pass = false;

  FakeRendererGL renderer(&settings, &debug_settings_, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  renderer.SetVisible(true);

  gfx::Size viewport_size(10, 10);

  AggregatedRenderPassId child_pass_id{2};
  AggregatedRenderPass* child_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, child_pass_id, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  cc::AddQuad(child_pass, gfx::Rect(viewport_size), SK_ColorBLUE);

  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  cc::AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorGREEN);

  cc::AddRenderPassQuad(root_pass, child_pass);

#ifdef NDEBUG
  GLint clear_bits = GL_COLOR_BUFFER_BIT;
#else
  GLint clear_bits = GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
#endif

  // First render pass is not the root one, clearing should happen.
  EXPECT_CALL(*mock_gl, Clear(clear_bits)).Times(AtLeast(1));

  Expectation first_render_pass =
      EXPECT_CALL(*mock_gl, DrawElements(_, _, _, _)).Times(1);

  if (features::IsUsingFastPathForSolidColorQuad()) {
    // The second render pass is the root one, clearing should be prevented. The
    // one call is expected due to the solid color draw quad which uses glClear
    // to draw the quad.
    EXPECT_CALL(*mock_gl, Clear(clear_bits)).Times(1).After(first_render_pass);
  } else {
    // The second render pass is the root one, clearing should be prevented.
    EXPECT_CALL(*mock_gl, Clear(clear_bits)).Times(0).After(first_render_pass);
  }

  EXPECT_CALL(*mock_gl, DrawElements(_, _, _, _))
      .Times(AnyNumber())
      .After(first_render_pass);

  renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
  DrawFrame(&renderer, viewport_size);

  // In multiple render passes all but the root pass should clear the
  // framebuffer.
  Mock::VerifyAndClearExpectations(&mock_gl);
}

class ScissorTestOnClearCheckingGLES2Interface : public TestGLES2Interface {
 public:
  ScissorTestOnClearCheckingGLES2Interface() = default;

  void ClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) override {
    // RGBA - {0, 0, 0, 0} is used to clear the buffer before drawing onto the
    // render target. Any other color means a solid color draw quad is being
    // drawn.
    if (features::IsUsingFastPathForSolidColorQuad())
      is_drawing_solid_color_quad_ = !(r == 0 && g == 0 && b == 0 && a == 0);
  }

  void Clear(GLbitfield bits) override {
    // GL clear is also used to draw solid color draw quads.
    if ((bits & GL_COLOR_BUFFER_BIT) && is_drawing_solid_color_quad_)
      return;
    EXPECT_FALSE(scissor_enabled_);
  }

  void Enable(GLenum cap) override {
    if (cap == GL_SCISSOR_TEST)
      scissor_enabled_ = true;
  }

  void Disable(GLenum cap) override {
    if (cap == GL_SCISSOR_TEST)
      scissor_enabled_ = false;
  }

 private:
  bool scissor_enabled_ = false;
  bool is_drawing_solid_color_quad_ = false;
};

TEST_F(GLRendererTest, ScissorTestWhenClearing) {
  auto gl_owned = std::make_unique<ScissorTestOnClearCheckingGLES2Interface>();

  auto provider = TestContextProvider::Create(std::move(gl_owned));
  provider->BindToCurrentThread();

  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  auto resource_provider = std::make_unique<DisplayResourceProviderGL>(
      output_surface->context_provider());

  RendererSettings settings;
  FakeRendererGL renderer(&settings, &debug_settings_, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  EXPECT_FALSE(renderer.use_partial_swap());
  renderer.SetVisible(true);

  gfx::Size viewport_size(100, 100);

  gfx::Rect grand_child_rect(25, 25);
  AggregatedRenderPassId grand_child_pass_id{3};
  AggregatedRenderPass* grand_child_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, grand_child_pass_id, grand_child_rect,
      gfx::Transform(), cc::FilterOperations());
  cc::AddClippedQuad(grand_child_pass, grand_child_rect, SK_ColorYELLOW);

  gfx::Rect child_rect(50, 50);
  AggregatedRenderPassId child_pass_id{2};
  AggregatedRenderPass* child_pass =
      cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                        child_rect, gfx::Transform(), cc::FilterOperations());
  cc::AddQuad(child_pass, child_rect, SK_ColorBLUE);

  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  cc::AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorGREEN);

  cc::AddRenderPassQuad(root_pass, child_pass);
  cc::AddRenderPassQuad(child_pass, grand_child_pass);

  renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
  DrawFrame(&renderer, viewport_size);
}

class DiscardCheckingGLES2Interface : public TestGLES2Interface {
 public:
  DiscardCheckingGLES2Interface() = default;

  void DiscardFramebufferEXT(GLenum target,
                             GLsizei numAttachments,
                             const GLenum* attachments) override {
    ++discarded_;
  }

  int discarded() const { return discarded_; }
  void reset_discarded() { discarded_ = 0; }

 private:
  int discarded_ = 0;
};

TEST_F(GLRendererTest, NoDiscardOnPartialUpdates) {
  auto gl_owned = std::make_unique<DiscardCheckingGLES2Interface>();
  gl_owned->set_have_post_sub_buffer(true);
  gl_owned->set_have_discard_framebuffer(true);

  auto* gl = gl_owned.get();

  auto provider = TestContextProvider::Create(std::move(gl_owned));
  provider->BindToCurrentThread();

  cc::FakeOutputSurfaceClient output_surface_client;
  auto output_surface = FakeOutputSurface::Create3d(std::move(provider));
  output_surface->BindToClient(&output_surface_client);

  auto resource_provider = std::make_unique<DisplayResourceProviderGL>(
      output_surface->context_provider());

  RendererSettings settings;
  settings.partial_swap_enabled = true;
  FakeRendererGL renderer(&settings, &debug_settings_, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  EXPECT_TRUE(renderer.use_partial_swap());
  renderer.SetVisible(true);

  gfx::Size viewport_size(100, 100);
  {
    // Draw one black frame to make sure the output surface is reshaped before
    // testes.
    AggregatedRenderPassId root_pass_id{1};
    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorBLACK);
    root_pass->damage_rect = gfx::Rect(viewport_size);

    renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
    DrawFrame(&renderer, viewport_size);
    gl->reset_discarded();
  }
  {
    // Partial frame, should not discard.
    AggregatedRenderPassId root_pass_id{1};
    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorGREEN);
    root_pass->damage_rect = gfx::Rect(2, 2, 3, 3);

    renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
    DrawFrame(&renderer, viewport_size);
    EXPECT_EQ(0, gl->discarded());
    gl->reset_discarded();
  }
  {
    // Full frame, should discard.
    AggregatedRenderPassId root_pass_id{1};
    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorGREEN);
    root_pass->damage_rect = root_pass->output_rect;

    renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
    DrawFrame(&renderer, viewport_size);
    EXPECT_EQ(1, gl->discarded());
    gl->reset_discarded();
  }
  {
    // Full frame, external scissor is set, should not discard.
    output_surface->set_has_external_stencil_test(true);
    AggregatedRenderPassId root_pass_id{1};
    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorGREEN);
    root_pass->damage_rect = root_pass->output_rect;
    root_pass->has_transparent_background = false;

    renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
    DrawFrame(&renderer, viewport_size);
    EXPECT_EQ(0, gl->discarded());
    gl->reset_discarded();
    output_surface->set_has_external_stencil_test(false);
  }
}

class ResourceTrackingGLES2Interface : public TestGLES2Interface {
 public:
  ResourceTrackingGLES2Interface() = default;
  ~ResourceTrackingGLES2Interface() override { CheckNoResources(); }

  void CheckNoResources() {
    EXPECT_TRUE(textures_.empty());
    EXPECT_TRUE(buffers_.empty());
    EXPECT_TRUE(framebuffers_.empty());
    EXPECT_TRUE(renderbuffers_.empty());
    EXPECT_TRUE(queries_.empty());
    EXPECT_TRUE(shaders_.empty());
    EXPECT_TRUE(programs_.empty());
  }

  void GenTextures(GLsizei n, GLuint* textures) override {
    GenIds(&textures_, n, textures);
  }

  void GenBuffers(GLsizei n, GLuint* buffers) override {
    GenIds(&buffers_, n, buffers);
  }

  void GenFramebuffers(GLsizei n, GLuint* framebuffers) override {
    GenIds(&framebuffers_, n, framebuffers);
  }

  void GenRenderbuffers(GLsizei n, GLuint* renderbuffers) override {
    GenIds(&renderbuffers_, n, renderbuffers);
  }

  void GenQueriesEXT(GLsizei n, GLuint* queries) override {
    GenIds(&queries_, n, queries);
  }

  GLuint CreateProgram() override { return GenId(&programs_); }

  GLuint CreateShader(GLenum type) override { return GenId(&shaders_); }

  void BindTexture(GLenum target, GLuint texture) override {
    CheckId(&textures_, texture);
  }

  void BindBuffer(GLenum target, GLuint buffer) override {
    CheckId(&buffers_, buffer);
  }

  void BindRenderbuffer(GLenum target, GLuint renderbuffer) override {
    CheckId(&renderbuffers_, renderbuffer);
  }

  void BindFramebuffer(GLenum target, GLuint framebuffer) override {
    CheckId(&framebuffers_, framebuffer);
  }

  void UseProgram(GLuint program) override { CheckId(&programs_, program); }

  void DeleteTextures(GLsizei n, const GLuint* textures) override {
    DeleteIds(&textures_, n, textures);
  }

  void DeleteBuffers(GLsizei n, const GLuint* buffers) override {
    DeleteIds(&buffers_, n, buffers);
  }

  void DeleteFramebuffers(GLsizei n, const GLuint* framebuffers) override {
    DeleteIds(&framebuffers_, n, framebuffers);
  }

  void DeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) override {
    DeleteIds(&renderbuffers_, n, renderbuffers);
  }

  void DeleteQueriesEXT(GLsizei n, const GLuint* queries) override {
    DeleteIds(&queries_, n, queries);
  }

  void DeleteProgram(GLuint program) override { DeleteId(&programs_, program); }

  void DeleteShader(GLuint shader) override { DeleteId(&shaders_, shader); }

  void BufferData(GLenum target,
                  GLsizeiptr size,
                  const void* data,
                  GLenum usage) override {}

 private:
  GLuint GenId(std::set<GLuint>* resource_set) {
    GLuint id = next_id_++;
    resource_set->insert(id);
    return id;
  }

  void GenIds(std::set<GLuint>* resource_set, GLsizei n, GLuint* ids) {
    for (GLsizei i = 0; i < n; ++i)
      ids[i] = GenId(resource_set);
  }

  void CheckId(std::set<GLuint>* resource_set, GLuint id) {
    if (id == 0)
      return;
    EXPECT_TRUE(resource_set->find(id) != resource_set->end());
  }

  void DeleteId(std::set<GLuint>* resource_set, GLuint id) {
    if (id == 0)
      return;
    size_t num_erased = resource_set->erase(id);
    EXPECT_EQ(1u, num_erased);
  }

  void DeleteIds(std::set<GLuint>* resource_set, GLsizei n, const GLuint* ids) {
    for (GLsizei i = 0; i < n; ++i)
      DeleteId(resource_set, ids[i]);
  }

  GLuint next_id_ = 1;
  std::set<GLuint> textures_;
  std::set<GLuint> buffers_;
  std::set<GLuint> framebuffers_;
  std::set<GLuint> renderbuffers_;
  std::set<GLuint> queries_;
  std::set<GLuint> shaders_;
  std::set<GLuint> programs_;
};

TEST_F(GLRendererTest, NoResourceLeak) {
  auto gl_owned = std::make_unique<ResourceTrackingGLES2Interface>();
  auto* gl = gl_owned.get();

  auto provider = TestContextProvider::Create(std::move(gl_owned));
  provider->BindToCurrentThread();

  cc::FakeOutputSurfaceClient output_surface_client;
  auto output_surface = FakeOutputSurface::Create3d(std::move(provider));
  output_surface->BindToClient(&output_surface_client);

  auto resource_provider = std::make_unique<DisplayResourceProviderGL>(
      output_surface->context_provider());

  {
    RendererSettings settings;
    FakeRendererGL renderer(&settings, &debug_settings_, output_surface.get(),
                            resource_provider.get());
    renderer.Initialize();
    renderer.SetVisible(true);

    gfx::Size viewport_size(100, 100);

    AggregatedRenderPassId root_pass_id{1};
    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorGREEN);
    root_pass->damage_rect = gfx::Rect(2, 2, 3, 3);

    renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
    DrawFrame(&renderer, viewport_size);
  }
  gl->CheckNoResources();
}

class DrawElementsGLES2Interface : public TestGLES2Interface {
 public:
  MOCK_METHOD4(
      DrawElements,
      void(GLenum mode, GLsizei count, GLenum type, const void* indices));
};

class GLRendererSkipTest : public GLRendererTest {
 protected:
  GLRendererSkipTest() {
    auto gl_owned = std::make_unique<StrictMock<DrawElementsGLES2Interface>>();
    gl_owned->set_have_post_sub_buffer(true);
    gl_ = gl_owned.get();

    auto provider = TestContextProvider::Create(std::move(gl_owned));
    provider->BindToCurrentThread();

    output_surface_ = FakeOutputSurface::Create3d(std::move(provider));
    output_surface_->BindToClient(&output_surface_client_);

    resource_provider_ = std::make_unique<DisplayResourceProviderGL>(
        output_surface_->context_provider());
    settings_.partial_swap_enabled = true;
    renderer_ = std::make_unique<FakeRendererGL>(&settings_, &debug_settings_,
                                                 output_surface_.get(),
                                                 resource_provider_.get());
    renderer_->Initialize();
    renderer_->SetVisible(true);
  }

  void DrawBlackFrame(const gfx::Size& viewport_size) {
    // The feature enables a faster path to draw solid color quads that does not
    // use GL draw calls but instead uses glClear.
    if (!features::IsUsingFastPathForSolidColorQuad())
      EXPECT_CALL(*gl_, DrawElements(_, _, _, _)).Times(1);

    AggregatedRenderPassId root_pass_id{1};
    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    root_pass->damage_rect = gfx::Rect(viewport_size);
    cc::AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorBLACK);
    renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    DrawFrame(renderer_.get(), viewport_size);
    Mock::VerifyAndClearExpectations(gl_);
  }

  StrictMock<DrawElementsGLES2Interface>* gl_;
  RendererSettings settings_;
  cc::FakeOutputSurfaceClient output_surface_client_;
  std::unique_ptr<FakeOutputSurface> output_surface_;
  std::unique_ptr<DisplayResourceProviderGL> resource_provider_;
  std::unique_ptr<FakeRendererGL> renderer_;
};

TEST_F(GLRendererSkipTest, DrawQuad) {
  gfx::Size viewport_size(100, 100);
  gfx::Rect quad_rect = gfx::Rect(20, 20, 20, 20);

  // Draw the a black frame to make sure output surface is reshaped before
  // tests.
  DrawBlackFrame(viewport_size);

  EXPECT_CALL(*gl_, DrawElements(_, _, _, _)).Times(1);

  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  root_pass->damage_rect = gfx::Rect(0, 0, 25, 25);
  cc::AddQuad(root_pass, quad_rect, SK_ColorGREEN);

  // Add rounded corners to the solid color draw quad so that the fast path
  // of drawing using glClear is not used.
  root_pass->shared_quad_state_list.front()->mask_filter_info =
      gfx::MaskFilterInfo(gfx::RRectF(gfx::RectF(quad_rect), 2.f));

  renderer_->DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
  DrawFrame(renderer_.get(), viewport_size);
}

TEST_F(GLRendererSkipTest, SkipVisibleRect) {
  gfx::Size viewport_size(100, 100);
  gfx::Rect quad_rect = gfx::Rect(0, 0, 40, 40);

  // Draw the a black frame to make sure output surface is reshaped before
  // tests.
  DrawBlackFrame(viewport_size);

  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  root_pass->damage_rect = gfx::Rect(0, 0, 10, 10);
  cc::AddQuad(root_pass, quad_rect, SK_ColorGREEN);
  root_pass->shared_quad_state_list.front()->is_clipped = true;
  root_pass->shared_quad_state_list.front()->clip_rect =
      gfx::Rect(0, 0, 40, 40);
  root_pass->quad_list.front()->visible_rect = gfx::Rect(20, 20, 20, 20);

  // Add rounded corners to the solid color draw quad so that the fast path
  // of drawing using glClear is not used.
  root_pass->shared_quad_state_list.front()->mask_filter_info =
      gfx::MaskFilterInfo(gfx::RRectF(gfx::RectF(quad_rect), 1.f));

  renderer_->DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
  DrawFrame(renderer_.get(), viewport_size);
  // DrawElements should not be called because the visible rect is outside the
  // scissor, even though the clip rect and quad rect intersect the scissor.
}

TEST_F(GLRendererSkipTest, SkipClippedQuads) {
  gfx::Size viewport_size(100, 100);
  gfx::Rect quad_rect = gfx::Rect(25, 25, 90, 90);

  // Draw the a black frame to make sure output surface is reshaped before
  // tests.
  DrawBlackFrame(viewport_size);

  AggregatedRenderPassId root_pass_id{1};

  auto* root_pass = cc::AddRenderPass(&render_passes_in_draw_order_,
                                      root_pass_id, gfx::Rect(viewport_size),
                                      gfx::Transform(), cc::FilterOperations());
  root_pass->damage_rect = gfx::Rect(0, 0, 25, 25);
  cc::AddClippedQuad(root_pass, quad_rect, SK_ColorGREEN);
  root_pass->quad_list.front()->rect = gfx::Rect(20, 20, 20, 20);

  renderer_->DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
  DrawFrame(renderer_.get(), viewport_size);
  // DrawElements should not be called because the clip rect is outside the
  // scissor.
}

TEST_F(GLRendererTest, DrawFramePreservesFramebuffer) {
  // When using render-to-FBO to display the surface, all rendering is done
  // to a non-zero FBO. Make sure that the framebuffer is always restored to
  // the correct framebuffer during rendering, if changed.
  // Note: there is one path that will set it to 0, but that is after the render
  // has finished.
  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<FakeOutputSurface> output_surface(
      FakeOutputSurface::Create3d());
  output_surface->BindToClient(&output_surface_client);

  auto resource_provider = std::make_unique<DisplayResourceProviderGL>(
      output_surface->context_provider());

  RendererSettings settings;
  FakeRendererGL renderer(&settings, &debug_settings_, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  EXPECT_FALSE(renderer.use_partial_swap());
  renderer.SetVisible(true);

  gfx::Size viewport_size(100, 100);
  gfx::Rect quad_rect = gfx::Rect(20, 20, 20, 20);

  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  cc::AddClippedQuad(root_pass, quad_rect, SK_ColorGREEN);

  unsigned fbo;
  gpu::gles2::GLES2Interface* gl =
      output_surface->context_provider()->ContextGL();
  gl->GenFramebuffers(1, &fbo);
  output_surface->set_framebuffer(fbo, GL_RGB);

  renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
  DrawFrame(&renderer, viewport_size);

  int bound_fbo;
  gl->GetIntegerv(GL_FRAMEBUFFER_BINDING, &bound_fbo);
  EXPECT_EQ(static_cast<int>(fbo), bound_fbo);
}

TEST_F(GLRendererShaderTest, DrawRenderPassQuadShaderPermutations) {
  gfx::Size viewport_size(60, 75);

  gfx::Rect child_rect(50, 50);
  AggregatedRenderPassId child_pass_id{2};
  AggregatedRenderPass* child_pass;

  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPass* root_pass;

  auto transfer_resource = TransferableResource::MakeGL(
      gpu::Mailbox::Generate(), GL_LINEAR, GL_TEXTURE_2D, gpu::SyncToken(),
      child_rect.size(), false /* is_overlay_candidate */);
  ResourceId mask = child_resource_provider_->ImportResource(
      transfer_resource, SingleReleaseCallback::Create(base::DoNothing()));

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap({mask}, resource_provider_.get(),
                                             child_resource_provider_.get(),
                                             child_context_provider_.get());
  ResourceId mapped_mask = resource_map[mask];

  float matrix[20];
  float amount = 0.5f;
  matrix[0] = 0.213f + 0.787f * amount;
  matrix[1] = 0.715f - 0.715f * amount;
  matrix[2] = 1.f - (matrix[0] + matrix[1]);
  matrix[3] = matrix[4] = 0;
  matrix[5] = 0.213f - 0.213f * amount;
  matrix[6] = 0.715f + 0.285f * amount;
  matrix[7] = 1.f - (matrix[5] + matrix[6]);
  matrix[8] = matrix[9] = 0;
  matrix[10] = 0.213f - 0.213f * amount;
  matrix[11] = 0.715f - 0.715f * amount;
  matrix[12] = 1.f - (matrix[10] + matrix[11]);
  matrix[13] = matrix[14] = 0;
  matrix[15] = matrix[16] = matrix[17] = matrix[19] = 0;
  matrix[18] = 1;
  cc::FilterOperations filters;
  filters.Append(cc::FilterOperation::CreateReferenceFilter(
      sk_make_sp<cc::ColorFilterPaintFilter>(SkColorFilters::Matrix(matrix),
                                             nullptr)));

  gfx::Transform transform_causing_aa;
  transform_causing_aa.Rotate(20.0);

  for (int i = 0; i <= LAST_BLEND_MODE; ++i) {
    BlendMode blend_mode = static_cast<BlendMode>(i);
    SkBlendMode xfer_mode = BlendModeToSkXfermode(blend_mode);
    settings_.force_blending_with_shaders = (blend_mode != BLEND_MODE_NONE);
    // RenderPassProgram
    render_passes_in_draw_order_.clear();
    child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          child_rect, gfx::Transform(), cc::FilterOperations());

    root_pass = cc::AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                                  gfx::Rect(viewport_size), gfx::Transform(),
                                  cc::FilterOperations());

    cc::AddRenderPassQuad(root_pass, child_pass, kInvalidResourceId,
                          gfx::Transform(), xfer_mode);

    renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    DrawFrame(renderer_.get(), viewport_size);
    TestRenderPassProgram(TEX_COORD_PRECISION_MEDIUM, blend_mode);

    // RenderPassColorMatrixProgram
    render_passes_in_draw_order_.clear();

    child_pass = cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                                   child_rect, transform_causing_aa, filters);

    root_pass = cc::AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                                  gfx::Rect(viewport_size), gfx::Transform(),
                                  cc::FilterOperations());

    cc::AddRenderPassQuad(root_pass, child_pass, kInvalidResourceId,
                          gfx::Transform(), xfer_mode);

    renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    DrawFrame(renderer_.get(), viewport_size);
    TestRenderPassColorMatrixProgram(TEX_COORD_PRECISION_MEDIUM, blend_mode);

    // RenderPassMaskProgram
    render_passes_in_draw_order_.clear();

    child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          child_rect, gfx::Transform(), cc::FilterOperations());

    root_pass = cc::AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                                  gfx::Rect(viewport_size), gfx::Transform(),
                                  cc::FilterOperations());

    cc::AddRenderPassQuad(root_pass, child_pass, mapped_mask, gfx::Transform(),
                          xfer_mode);

    renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    DrawFrame(renderer_.get(), viewport_size);
    TestRenderPassMaskProgram(TEX_COORD_PRECISION_MEDIUM, SAMPLER_TYPE_2D,
                              blend_mode);

    // RenderPassMaskColorMatrixProgram
    render_passes_in_draw_order_.clear();

    child_pass = cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                                   child_rect, gfx::Transform(), filters);

    root_pass = cc::AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                                  gfx::Rect(viewport_size), gfx::Transform(),
                                  cc::FilterOperations());

    cc::AddRenderPassQuad(root_pass, child_pass, mapped_mask, gfx::Transform(),
                          xfer_mode);

    renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    DrawFrame(renderer_.get(), viewport_size);
    TestRenderPassMaskColorMatrixProgram(TEX_COORD_PRECISION_MEDIUM,
                                         SAMPLER_TYPE_2D, blend_mode);

    // RenderPassProgramAA
    render_passes_in_draw_order_.clear();

    child_pass = cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                                   child_rect, transform_causing_aa,
                                   cc::FilterOperations());

    root_pass = cc::AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                                  gfx::Rect(viewport_size), gfx::Transform(),
                                  cc::FilterOperations());

    cc::AddRenderPassQuad(root_pass, child_pass, kInvalidResourceId,
                          transform_causing_aa, xfer_mode);

    renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    DrawFrame(renderer_.get(), viewport_size);
    TestRenderPassProgramAA(TEX_COORD_PRECISION_MEDIUM, blend_mode);

    // RenderPassColorMatrixProgramAA
    render_passes_in_draw_order_.clear();

    child_pass = cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                                   child_rect, transform_causing_aa, filters);

    root_pass = cc::AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                                  gfx::Rect(viewport_size), gfx::Transform(),
                                  cc::FilterOperations());

    cc::AddRenderPassQuad(root_pass, child_pass, kInvalidResourceId,
                          transform_causing_aa, xfer_mode);

    renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    DrawFrame(renderer_.get(), viewport_size);
    TestRenderPassColorMatrixProgramAA(TEX_COORD_PRECISION_MEDIUM, blend_mode);

    // RenderPassMaskProgramAA
    render_passes_in_draw_order_.clear();

    child_pass = cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                                   child_rect, transform_causing_aa,
                                   cc::FilterOperations());

    root_pass = cc::AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                                  gfx::Rect(viewport_size), gfx::Transform(),
                                  cc::FilterOperations());

    cc::AddRenderPassQuad(root_pass, child_pass, mapped_mask,
                          transform_causing_aa, xfer_mode);

    renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    DrawFrame(renderer_.get(), viewport_size);
    TestRenderPassMaskProgramAA(TEX_COORD_PRECISION_MEDIUM, SAMPLER_TYPE_2D,
                                blend_mode);

    // RenderPassMaskColorMatrixProgramAA
    render_passes_in_draw_order_.clear();

    child_pass = cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                                   child_rect, transform_causing_aa, filters);

    root_pass = cc::AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                                  gfx::Rect(viewport_size),
                                  transform_causing_aa, cc::FilterOperations());

    cc::AddRenderPassQuad(root_pass, child_pass, mapped_mask,
                          transform_causing_aa, xfer_mode);

    renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    DrawFrame(renderer_.get(), viewport_size);
    TestRenderPassMaskColorMatrixProgramAA(TEX_COORD_PRECISION_MEDIUM,
                                           SAMPLER_TYPE_2D, blend_mode);
  }
}

// At this time, the AA code path cannot be taken if the surface's rect would
// project incorrectly by the given transform, because of w<0 clipping.
TEST_F(GLRendererShaderTest, DrawRenderPassQuadSkipsAAForClippingTransform) {
  gfx::Rect child_rect(50, 50);
  AggregatedRenderPassId child_pass_id{2};
  AggregatedRenderPass* child_pass;

  gfx::Size viewport_size(100, 100);
  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPass* root_pass;

  gfx::Transform transform_preventing_aa;
  transform_preventing_aa.ApplyPerspectiveDepth(40.0);
  transform_preventing_aa.RotateAboutYAxis(-20.0);
  transform_preventing_aa.Scale(30.0, 1.0);

  // Verify that the test transform and test rect actually do cause the clipped
  // flag to trigger. Otherwise we are not testing the intended scenario.
  bool clipped = false;
  cc::MathUtil::MapQuad(transform_preventing_aa,
                        gfx::QuadF(gfx::RectF(child_rect)), &clipped);
  ASSERT_TRUE(clipped);

  child_pass = cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                                 child_rect, transform_preventing_aa,
                                 cc::FilterOperations());

  root_pass = cc::AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                                gfx::Rect(viewport_size), gfx::Transform(),
                                cc::FilterOperations());

  cc::AddRenderPassQuad(root_pass, child_pass, kInvalidResourceId,
                        transform_preventing_aa, SkBlendMode::kSrcOver);

  renderer_->DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
  DrawFrame(renderer_.get(), viewport_size);

  // If use_aa incorrectly ignores clipping, it will use the
  // RenderPassProgramAA shader instead of the RenderPassProgram.
  TestRenderPassProgram(TEX_COORD_PRECISION_MEDIUM, BLEND_MODE_NONE);
}

TEST_F(GLRendererShaderTest, DrawSolidColorShader) {
  gfx::Size viewport_size(30, 30);  // Don't translate out of the viewport.
  gfx::Size quad_size(3, 3);
  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPass* root_pass;

  gfx::Transform pixel_aligned_transform_causing_aa;
  pixel_aligned_transform_causing_aa.Translate(25.5f, 25.5f);
  pixel_aligned_transform_causing_aa.Scale(0.5f, 0.5f);

  root_pass = cc::AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                                gfx::Rect(viewport_size), gfx::Transform(),
                                cc::FilterOperations());
  cc::AddTransformedQuad(root_pass, gfx::Rect(quad_size), SK_ColorYELLOW,
                         pixel_aligned_transform_causing_aa);

  renderer_->DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
  DrawFrame(renderer_.get(), viewport_size);

  TestSolidColorProgramAA();
}

class OutputSurfaceMockGLES2Interface : public TestGLES2Interface {
 public:
  OutputSurfaceMockGLES2Interface() = default;

  // Specifically override methods even if they are unused (used in conjunction
  // with StrictMock). We need to make sure that GLRenderer does not issue
  // framebuffer-related GLuint calls directly. Instead these are supposed to go
  // through the OutputSurface abstraction.
  MOCK_METHOD2(BindFramebuffer, void(GLenum target, GLuint framebuffer));
  MOCK_METHOD5(ResizeCHROMIUM,
               void(GLuint width,
                    GLuint height,
                    float device_scale,
                    GLcolorSpace color_space,
                    GLboolean has_alpha));
  MOCK_METHOD4(
      DrawElements,
      void(GLenum mode, GLsizei count, GLenum type, const void* indices));
};

class MockOutputSurface : public OutputSurface {
 public:
  explicit MockOutputSurface(scoped_refptr<ContextProvider> provider)
      : OutputSurface(std::move(provider)) {}
  ~MockOutputSurface() override {}

  void BindToClient(OutputSurfaceClient*) override {}
  unsigned UpdateGpuFence() override { return 0; }

  MOCK_METHOD0(EnsureBackbuffer, void());
  MOCK_METHOD0(DiscardBackbuffer, void());
  MOCK_METHOD5(Reshape,
               void(const gfx::Size& size,
                    float scale_factor,
                    const gfx::ColorSpace& color_space,
                    gfx::BufferFormat format,
                    bool use_stencil));
  MOCK_METHOD0(BindFramebuffer, void());
  MOCK_METHOD1(SetDrawRectangle, void(const gfx::Rect&));
  MOCK_METHOD1(SetEnableDCLayers, void(bool));
  MOCK_METHOD0(GetFramebufferCopyTextureFormat, GLenum());
  MOCK_METHOD1(SwapBuffers_, void(OutputSurfaceFrame& frame));  // NOLINT
  void SwapBuffers(OutputSurfaceFrame frame) override { SwapBuffers_(frame); }
  MOCK_CONST_METHOD0(IsDisplayedAsOverlayPlane, bool());
  MOCK_CONST_METHOD0(GetOverlayTextureId, unsigned());
  MOCK_CONST_METHOD0(HasExternalStencilTest, bool());
  MOCK_METHOD0(ApplyExternalStencil, void());
  MOCK_METHOD1(SetUpdateVSyncParametersCallback,
               void(UpdateVSyncParametersCallback));
  MOCK_METHOD1(SetDisplayTransformHint, void(gfx::OverlayTransform));

  gfx::OverlayTransform GetDisplayTransform() override {
    return gfx::OVERLAY_TRANSFORM_NONE;
  }
};

class MockOutputSurfaceTest : public GLRendererTest {
 protected:
  void SetUp() override {
    auto gl = std::make_unique<StrictMock<OutputSurfaceMockGLES2Interface>>();
    gl->set_have_post_sub_buffer(true);
    gl_ = gl.get();
    auto provider = TestContextProvider::Create(std::move(gl));
    provider->BindToCurrentThread();
    output_surface_ =
        std::make_unique<StrictMock<MockOutputSurface>>(std::move(provider));

    cc::FakeOutputSurfaceClient output_surface_client_;
    output_surface_->BindToClient(&output_surface_client_);

    resource_provider_ = std::make_unique<DisplayResourceProviderGL>(
        output_surface_->context_provider());

    renderer_.reset(new FakeRendererGL(&settings_, &debug_settings_,
                                       output_surface_.get(),
                                       resource_provider_.get()));
    renderer_->Initialize();

    EXPECT_CALL(*output_surface_, EnsureBackbuffer()).Times(1);
    renderer_->SetVisible(true);
    Mock::VerifyAndClearExpectations(output_surface_.get());
  }

  void SwapBuffers() {
    renderer_->SwapBuffers(DirectRenderer::SwapFrameData());
  }

  void DrawFrame(float device_scale_factor,
                 const gfx::Size& viewport_size,
                 bool transparent) {
    gfx::BufferFormat format = transparent ? gfx::BufferFormat::RGBA_8888
                                           : gfx::BufferFormat::RGBX_8888;
    AggregatedRenderPassId render_pass_id{1};
    AggregatedRenderPass* render_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, render_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddQuad(render_pass, gfx::Rect(viewport_size), SK_ColorGREEN);
    render_pass->has_transparent_background = transparent;

    EXPECT_CALL(*output_surface_, EnsureBackbuffer()).WillRepeatedly(Return());

    EXPECT_CALL(*output_surface_,
                Reshape(viewport_size, device_scale_factor, _, format, _))
        .Times(1);

    EXPECT_CALL(*output_surface_, BindFramebuffer()).Times(1);

    EXPECT_CALL(*gl_, DrawElements(_, _, _, _)).Times(1);

    renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    SurfaceDamageRectList surface_damage_rect_list;
    renderer_->DrawFrame(&render_passes_in_draw_order_, device_scale_factor,
                         viewport_size, gfx::DisplayColorSpaces(),
                         std::move(surface_damage_rect_list));
  }

  RendererSettings settings_;
  cc::FakeOutputSurfaceClient output_surface_client_;
  OutputSurfaceMockGLES2Interface* gl_ = nullptr;
  std::unique_ptr<StrictMock<MockOutputSurface>> output_surface_;
  std::unique_ptr<DisplayResourceProviderGL> resource_provider_;
  std::unique_ptr<FakeRendererGL> renderer_;
};

TEST_F(MockOutputSurfaceTest, BackbufferDiscard) {
  // Drop backbuffer on hide.
  EXPECT_CALL(*output_surface_, DiscardBackbuffer()).Times(1);
  renderer_->SetVisible(false);
  Mock::VerifyAndClearExpectations(output_surface_.get());

  // Restore backbuffer on show.
  EXPECT_CALL(*output_surface_, EnsureBackbuffer()).Times(1);
  renderer_->SetVisible(true);
  Mock::VerifyAndClearExpectations(output_surface_.get());
}

#if defined(OS_WIN)
class MockDCLayerOverlayProcessor : public DCLayerOverlayProcessor {
 public:
  MockDCLayerOverlayProcessor()
      : DCLayerOverlayProcessor(&debug_settings_,
                                /*allowed_yuv_overlay_count=*/1,
                                true) {}
  ~MockDCLayerOverlayProcessor() override = default;
  MOCK_METHOD8(Process,
               void(DisplayResourceProvider* resource_provider,
                    const gfx::RectF& display_rect,
                    const FilterOperationsMap& render_pass_filters,
                    const FilterOperationsMap& render_pass_backdrop_filters,
                    AggregatedRenderPassList* render_passes,
                    gfx::Rect* damage_rect,
                    SurfaceDamageRectList surface_damage_rect_list,
                    DCLayerOverlayList* dc_layer_overlays));

 protected:
  DebugRendererSettings debug_settings_;
};
class TestOverlayProcessor : public OverlayProcessorWin {
 public:
  explicit TestOverlayProcessor(OutputSurface* output_surface)
      : OverlayProcessorWin(output_surface,
                            std::make_unique<MockDCLayerOverlayProcessor>()) {}
  ~TestOverlayProcessor() override = default;

  MockDCLayerOverlayProcessor* GetTestProcessor() {
    return static_cast<MockDCLayerOverlayProcessor*>(GetOverlayProcessor());
  }
};
#elif defined(OS_APPLE)
class MockCALayerOverlayProcessor : public CALayerOverlayProcessor {
 public:
  MockCALayerOverlayProcessor() = default;
  ~MockCALayerOverlayProcessor() override = default;

  MOCK_CONST_METHOD6(
      ProcessForCALayerOverlays,
      bool(DisplayResourceProvider* resource_provider,
           const gfx::RectF& display_rect,
           const QuadList& quad_list,
           const base::flat_map<AggregatedRenderPassId, cc::FilterOperations*>&
               render_pass_filters,
           const base::flat_map<AggregatedRenderPassId, cc::FilterOperations*>&
               render_pass_backdrop_filters,
           CALayerOverlayList* ca_layer_overlays));
};

class TestOverlayProcessor : public OverlayProcessorMac {
 public:
  explicit TestOverlayProcessor(OutputSurface* output_surface)
      : OverlayProcessorMac(std::make_unique<MockCALayerOverlayProcessor>()) {}
  ~TestOverlayProcessor() override = default;

  const MockCALayerOverlayProcessor* GetTestProcessor() const {
    return static_cast<const MockCALayerOverlayProcessor*>(
        GetOverlayProcessor());
  }
};

#elif defined(OS_ANDROID) || defined(USE_OZONE)

class TestOverlayProcessor : public OverlayProcessorUsingStrategy {
 public:
  class Strategy : public OverlayProcessorUsingStrategy::Strategy {
   public:
    Strategy() = default;
    ~Strategy() override = default;

    MOCK_METHOD8(
        Attempt,
        bool(const SkMatrix44& output_color_matrix,
             const OverlayProcessorInterface::FilterOperationsMap&
                 render_pass_backdrop_filters,
             DisplayResourceProvider* resource_provider,
             AggregatedRenderPassList* render_pass_list,
             SurfaceDamageRectList* surface_damage_rect_list,
             const OverlayProcessorInterface::OutputSurfaceOverlayPlane*
                 primary_surface,
             OverlayCandidateList* candidates,
             std::vector<gfx::Rect>* content_bounds));

    void ProposePrioritized(
        const SkMatrix44& output_color_matrix,
        const FilterOperationsMap& render_pass_backdrop_filters,
        DisplayResourceProvider* resource_provider,
        AggregatedRenderPassList* render_pass_list,
        SurfaceDamageRectList* surface_damage_rect_list,
        const PrimaryPlane* primary_plane,
        OverlayProposedCandidateList* candidates,
        std::vector<gfx::Rect>* content_bounds) override {
      auto* render_pass = render_pass_list->back().get();
      QuadList& quad_list = render_pass->quad_list;
      OverlayCandidate candidate;
      candidates->push_back({quad_list.end(), candidate, this});
    }

    MOCK_METHOD9(AttemptPrioritized,
                 bool(const SkMatrix44& output_color_matrix,
                      const FilterOperationsMap& render_pass_backdrop_filters,
                      DisplayResourceProvider* resource_provider,
                      AggregatedRenderPassList* render_pass_list,
                      SurfaceDamageRectList* surface_damage_rect_list,
                      const PrimaryPlane* primary_plane,
                      OverlayCandidateList* candidates,
                      std::vector<gfx::Rect>* content_bounds,
                      OverlayProposedCandidate* proposed_candidate));
  };

  bool IsOverlaySupported() const override { return true; }

  // A list of possible overlay candidates is presented to this function.
  // The expected result is that those candidates that can be in a separate
  // plane are marked with |overlay_handled| set to true, otherwise they are
  // to be traditionally composited. Candidates with |overlay_handled| set to
  // true must also have their |display_rect| converted to integer
  // coordinates if necessary.
  void CheckOverlaySupport(
      const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
      OverlayCandidateList* surfaces) override {}

  Strategy& strategy() {
    auto* strategy = strategies_.back().get();
    return *(static_cast<Strategy*>(strategy));
  }

  MOCK_CONST_METHOD0(NeedsSurfaceDamageRectList, bool());
  explicit TestOverlayProcessor(OutputSurface* output_surface)
      : OverlayProcessorUsingStrategy() {
    strategies_.push_back(std::make_unique<Strategy>());
    prioritization_config_.changing_threshold = false;
    prioritization_config_.damage_rate_threshold = false;
  }
  ~TestOverlayProcessor() override = default;
};
#else  // Default to no overlay.
class TestOverlayProcessor : public OverlayProcessorStub {
 public:
  explicit TestOverlayProcessor(OutputSurface* output_surface)
      : OverlayProcessorStub() {}
  ~TestOverlayProcessor() override = default;
};
#endif

void MailboxReleased(const gpu::SyncToken& sync_token, bool lost_resource) {}

static void CollectResources(std::vector<ReturnedResource>* array,
                             const std::vector<ReturnedResource>& returned) {
  array->insert(array->end(), returned.begin(), returned.end());
}

TEST_F(GLRendererTest, DontOverlayWithCopyRequests) {
  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<FakeOutputSurface> output_surface(
      FakeOutputSurface::Create3d());
#if defined(OS_WIN)
  output_surface->set_supports_dc_layers(true);
#endif
  output_surface->BindToClient(&output_surface_client);

  auto parent_resource_provider = std::make_unique<DisplayResourceProviderGL>(
      output_surface->context_provider());

  auto child_context_provider = TestContextProvider::Create();
  child_context_provider->BindToCurrentThread();
  auto child_resource_provider = std::make_unique<ClientResourceProvider>();

  auto transfer_resource = TransferableResource::MakeGL(
      gpu::Mailbox::Generate(), GL_LINEAR, GL_TEXTURE_2D, gpu::SyncToken(),
      gfx::Size(256, 256), true);
  auto release_callback =
      SingleReleaseCallback::Create(base::BindOnce(&MailboxReleased));
  ResourceId resource_id = child_resource_provider->ImportResource(
      transfer_resource, std::move(release_callback));

  std::vector<ReturnedResource> returned_to_child;
  int child_id = parent_resource_provider->CreateChild(
      base::BindRepeating(&CollectResources, &returned_to_child));

  // Transfer resource to the parent.
  std::vector<ResourceId> resource_ids_to_transfer;
  resource_ids_to_transfer.push_back(resource_id);
  std::vector<TransferableResource> list;
  child_resource_provider->PrepareSendToParent(
      resource_ids_to_transfer, &list,
      static_cast<RasterContextProvider*>(child_context_provider.get()));
  parent_resource_provider->ReceiveFromChild(child_id, list);

  // In DisplayResourceProvider's namespace, use the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      parent_resource_provider->GetChildToParentMap(child_id);
  ResourceId parent_resource_id = resource_map[list[0].id];

  auto processor = std::make_unique<TestOverlayProcessor>(output_surface.get());

  RendererSettings settings;
  FakeRendererGL renderer(&settings, &debug_settings_, output_surface.get(),
                          parent_resource_provider.get(), processor.get(),
                          base::ThreadTaskRunnerHandle::Get());
  renderer.Initialize();
  renderer.SetVisible(true);

#if defined(OS_APPLE)
  const MockCALayerOverlayProcessor* mock_ca_processor =
      processor->GetTestProcessor();
#elif defined(OS_WIN)
  MockDCLayerOverlayProcessor* dc_processor = processor->GetTestProcessor();
#endif

  gfx::Size viewport_size(1, 1);
  AggregatedRenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, AggregatedRenderPassId{1},
      gfx::Rect(viewport_size), gfx::Transform(), cc::FilterOperations());
  root_pass->has_transparent_background = false;
  root_pass->copy_requests.push_back(CopyOutputRequest::CreateStubForTesting());

  bool needs_blending = false;
  bool premultiplied_alpha = false;
  bool flipped = false;
  bool nearest_neighbor = false;
  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};

  TextureDrawQuad* overlay_quad =
      root_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  overlay_quad->SetNew(
      root_pass->CreateAndAppendSharedQuadState(), gfx::Rect(viewport_size),
      gfx::Rect(viewport_size), needs_blending, parent_resource_id,
      premultiplied_alpha, gfx::PointF(0, 0), gfx::PointF(1, 1),
      SK_ColorTRANSPARENT, vertex_opacity, flipped, nearest_neighbor,
      /*secure_output_only=*/false, gfx::ProtectedVideoType::kClear);

  // DirectRenderer::DrawFrame calls into OverlayProcessor::ProcessForOverlays.
  // Attempt will be called for each strategy in OverlayProcessor. We have
  // added a fake strategy, so checking for Attempt calls checks if there was
  // any attempt to overlay, which there shouldn't be. We can't use the quad
  // list because the render pass is cleaned up by DrawFrame.
#if defined(USE_OZONE) || defined(OS_ANDROID)
  if (features::IsOverlayPrioritizationEnabled()) {
    EXPECT_CALL(processor->strategy(),
                AttemptPrioritized(_, _, _, _, _, _, _, _, _))
        .Times(0);
  } else {
    EXPECT_CALL(processor->strategy(), Attempt(_, _, _, _, _, _, _, _))
        .Times(0);
  }
#elif defined(OS_APPLE)
  EXPECT_CALL(*mock_ca_processor, ProcessForCALayerOverlays(_, _, _, _, _, _))
      .Times(0);
#elif defined(OS_WIN)
  EXPECT_CALL(*dc_processor, Process(_, _, _, _, _, _, _, _)).Times(0);
#endif
  DrawFrame(&renderer, viewport_size);
#if defined(USE_OZONE) || defined(OS_ANDROID)
  Mock::VerifyAndClearExpectations(&processor->strategy());
#elif defined(OS_APPLE)
  Mock::VerifyAndClearExpectations(
      const_cast<MockCALayerOverlayProcessor*>(mock_ca_processor));
#elif defined(OS_WIN)
  Mock::VerifyAndClearExpectations(
      const_cast<MockDCLayerOverlayProcessor*>(dc_processor));
#endif

  // Without a copy request Attempt() should be called once.
  root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, AggregatedRenderPassId{1},
      gfx::Rect(viewport_size), gfx::Transform(), cc::FilterOperations());
  root_pass->has_transparent_background = false;

  overlay_quad = root_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  overlay_quad->SetNew(
      root_pass->CreateAndAppendSharedQuadState(), gfx::Rect(viewport_size),
      gfx::Rect(viewport_size), needs_blending, parent_resource_id,
      premultiplied_alpha, gfx::PointF(0, 0), gfx::PointF(1, 1),
      SK_ColorTRANSPARENT, vertex_opacity, flipped, nearest_neighbor,
      /*secure_output_only=*/false, gfx::ProtectedVideoType::kClear);
#if defined(USE_OZONE) || defined(OS_ANDROID)
  if (features::IsOverlayPrioritizationEnabled()) {
    EXPECT_CALL(processor->strategy(),
                AttemptPrioritized(_, _, _, _, _, _, _, _, _))
        .Times(1);
  } else {
    EXPECT_CALL(processor->strategy(), Attempt(_, _, _, _, _, _, _, _))
        .Times(1);
  }
#elif defined(OS_APPLE)
  EXPECT_CALL(*mock_ca_processor, ProcessForCALayerOverlays(_, _, _, _, _, _))
      .Times(1);
#elif defined(OS_WIN)
  EXPECT_CALL(*dc_processor, Process(_, _, _, _, _, _, _, _)).Times(1);
#endif
  DrawFrame(&renderer, viewport_size);

  // Transfer resources back from the parent to the child. Set no resources as
  // being in use.
  parent_resource_provider->DeclareUsedResourcesFromChild(child_id,
                                                          ResourceIdSet());

  child_resource_provider->RemoveImportedResource(resource_id);
  child_resource_provider->ShutdownAndReleaseAllResources();
}

#if defined(OS_ANDROID) || defined(USE_OZONE)
class SingleOverlayOnTopProcessor : public OverlayProcessorUsingStrategy {
 public:
  SingleOverlayOnTopProcessor() : OverlayProcessorUsingStrategy() {
    strategies_.push_back(std::make_unique<OverlayStrategySingleOnTop>(this));
    strategies_.push_back(std::make_unique<OverlayStrategyUnderlay>(this));
    prioritization_config_.changing_threshold = false;
    prioritization_config_.damage_rate_threshold = false;
  }

  bool NeedsSurfaceDamageRectList() const override { return true; }
  bool IsOverlaySupported() const override { return true; }

  void CheckOverlaySupport(
      const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
      OverlayCandidateList* surfaces) override {
    if (!multiple_candidates_)
      ASSERT_EQ(1U, surfaces->size());
    OverlayCandidate& candidate = surfaces->back();
    candidate.overlay_handled = true;
  }

  void AllowMultipleCandidates() { multiple_candidates_ = true; }

 private:
  bool multiple_candidates_ = false;
};

class WaitSyncTokenCountingGLES2Interface : public TestGLES2Interface {
 public:
  MOCK_METHOD1(WaitSyncTokenCHROMIUM, void(const GLbyte* sync_token));
};

class MockOverlayScheduler {
 public:
  MOCK_METHOD7(Schedule,
               void(int plane_z_order,
                    gfx::OverlayTransform plane_transform,
                    unsigned overlay_texture_id,
                    const gfx::Rect& display_bounds,
                    const gfx::RectF& uv_rect,
                    bool enable_blend,
                    unsigned gpu_fence_id));
};

TEST_F(GLRendererTest, OverlaySyncTokensAreProcessed) {
#if defined(USE_X11)
  // TODO(1096425): Remove this.
  if (!features::IsUsingOzonePlatform())
    GTEST_SKIP();
#endif
  auto gl_owned = std::make_unique<WaitSyncTokenCountingGLES2Interface>();
  WaitSyncTokenCountingGLES2Interface* gl = gl_owned.get();

  auto provider = TestContextProvider::Create(std::move(gl_owned));
  provider->BindToCurrentThread();

  MockOverlayScheduler overlay_scheduler;
  provider->support()->SetScheduleOverlayPlaneCallback(base::BindRepeating(
      &MockOverlayScheduler::Schedule, base::Unretained(&overlay_scheduler)));

  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<OutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->BindToClient(&output_surface_client);

  auto parent_resource_provider = std::make_unique<DisplayResourceProviderGL>(
      output_surface->context_provider());

  auto child_context_provider = TestContextProvider::Create();
  child_context_provider->BindToCurrentThread();
  auto child_resource_provider = std::make_unique<ClientResourceProvider>();

  gpu::SyncToken sync_token(gpu::CommandBufferNamespace::GPU_IO,
                            gpu::CommandBufferId::FromUnsafeValue(0x123), 29);
  auto transfer_resource = TransferableResource::MakeGL(
      gpu::Mailbox::Generate(), GL_LINEAR, GL_TEXTURE_2D, sync_token,
      gfx::Size(256, 256), true);
  auto release_callback =
      SingleReleaseCallback::Create(base::BindOnce(&MailboxReleased));
  ResourceId resource_id = child_resource_provider->ImportResource(
      transfer_resource, std::move(release_callback));

  std::vector<ReturnedResource> returned_to_child;
  int child_id = parent_resource_provider->CreateChild(
      base::BindRepeating(&CollectResources, &returned_to_child));

  // Transfer resource to the parent.
  std::vector<ResourceId> resource_ids_to_transfer;
  resource_ids_to_transfer.push_back(resource_id);
  std::vector<TransferableResource> list;
  child_resource_provider->PrepareSendToParent(
      resource_ids_to_transfer, &list,
      static_cast<RasterContextProvider*>(child_context_provider.get()));
  parent_resource_provider->ReceiveFromChild(child_id, list);

  // In DisplayResourceProvider's namespace, use the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      parent_resource_provider->GetChildToParentMap(child_id);
  ResourceId parent_resource_id = resource_map[list[0].id];

  RendererSettings settings;
  auto processor = std::make_unique<SingleOverlayOnTopProcessor>();
  FakeRendererGL renderer(&settings, &debug_settings_, output_surface.get(),
                          parent_resource_provider.get(), processor.get(),
                          base::ThreadTaskRunnerHandle::Get());
  renderer.Initialize();
  renderer.SetVisible(true);

  gfx::Size viewport_size(1, 1);
  AggregatedRenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, AggregatedRenderPassId{1},
      gfx::Rect(viewport_size), gfx::Transform(), cc::FilterOperations());
  root_pass->has_transparent_background = false;

  bool needs_blending = false;
  bool premultiplied_alpha = false;
  bool flipped = false;
  bool nearest_neighbor = false;
  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  gfx::PointF uv_top_left(0, 0);
  gfx::PointF uv_bottom_right(1, 1);

  TextureDrawQuad* overlay_quad =
      root_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  SharedQuadState* shared_state = root_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), gfx::Rect(viewport_size),
                       gfx::Rect(viewport_size), gfx::MaskFilterInfo(),
                       gfx::Rect(viewport_size), false, false, 1,
                       SkBlendMode::kSrcOver, 0);
  overlay_quad->SetNew(shared_state, gfx::Rect(viewport_size),
                       gfx::Rect(viewport_size), needs_blending,
                       parent_resource_id, premultiplied_alpha, uv_top_left,
                       uv_bottom_right, SK_ColorTRANSPARENT, vertex_opacity,
                       flipped, nearest_neighbor, /*secure_output_only=*/false,
                       gfx::ProtectedVideoType::kClear);

  // The verified flush flag will be set by
  // ClientResourceProvider::PrepareSendToParent. Before checking if the
  // gpu::SyncToken matches, set this flag first.
  sync_token.SetVerifyFlush();

  // Verify that overlay_quad actually gets turned into an overlay, and even
  // though it's not drawn, that its sync point is waited on.
  EXPECT_CALL(*gl, WaitSyncTokenCHROMIUM(MatchesSyncToken(sync_token)))
      .Times(1);

  EXPECT_CALL(
      overlay_scheduler,
      Schedule(1, gfx::OVERLAY_TRANSFORM_NONE, _, gfx::Rect(viewport_size),
               BoundingRect(uv_top_left, uv_bottom_right), _, _))
      .Times(1);

  DrawFrame(&renderer, viewport_size);

  // Transfer resources back from the parent to the child. Set no resources as
  // being in use.
  parent_resource_provider->DeclareUsedResourcesFromChild(child_id,
                                                          ResourceIdSet());

  child_resource_provider->RemoveImportedResource(resource_id);
  child_resource_provider->ShutdownAndReleaseAllResources();
}
#endif  // defined(USE_OZONE) || defined(OS_ANDROID)

class OutputColorMatrixMockGLES2Interface : public TestGLES2Interface {
 public:
  OutputColorMatrixMockGLES2Interface() = default;

  MOCK_METHOD4(UniformMatrix4fv,
               void(GLint location,
                    GLsizei count,
                    GLboolean transpose,
                    const GLfloat* value));
};

TEST_F(GLRendererTest, OutputColorMatrixTest) {
  // Initialize the mock GL interface, the output surface and the renderer.
  auto gl_owned = std::make_unique<OutputColorMatrixMockGLES2Interface>();
  auto* gl = gl_owned.get();
  auto provider = TestContextProvider::Create(std::move(gl_owned));
  provider->BindToCurrentThread();
  std::unique_ptr<FakeOutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  cc::FakeOutputSurfaceClient output_surface_client;
  output_surface->BindToClient(&output_surface_client);
  auto resource_provider = std::make_unique<DisplayResourceProviderGL>(
      output_surface->context_provider());
  RendererSettings settings;
  FakeRendererGL renderer(&settings, &debug_settings_, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  renderer.SetVisible(true);

  // Set a non-identity color matrix on the output surface.
  SkMatrix44 color_matrix(SkMatrix44::kIdentity_Constructor);
  color_matrix.set(0, 0, 0.7f);
  color_matrix.set(1, 1, 0.4f);
  color_matrix.set(2, 2, 0.5f);
  output_surface->set_color_matrix(color_matrix);

  // Create a root and a child passes to test that the output color matrix is
  // registered only for the root pass.
  gfx::Size viewport_size(100, 100);
  AggregatedRenderPassId child_pass_id{2};
  AggregatedRenderPass* child_pass =
      cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                        gfx::Rect(viewport_size) + gfx::Vector2d(1, 2),
                        gfx::Transform(), cc::FilterOperations());
  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  root_pass->damage_rect = gfx::Rect(0, 0, 25, 25);
  cc::AddRenderPassQuad(root_pass, child_pass);

  // Verify that UniformMatrix4fv() is called only once on the root pass with
  // the correct matrix values.
  int call_count = 0;
  bool output_color_matrix_invoked = false;
  EXPECT_CALL(*gl, UniformMatrix4fv(_, 1, false, _))
      .WillRepeatedly(testing::WithArgs<0, 3>(testing::Invoke(
          [&color_matrix, &renderer, &call_count, &output_color_matrix_invoked](
              int matrix_location, const GLfloat* gl_matrix) {
            DCHECK(current_program(&renderer));
            const int color_matrix_location =
                current_program(&renderer)->output_color_matrix_location();

            if (matrix_location != color_matrix_location)
              return;

            call_count++;
            output_color_matrix_invoked = true;
            float expected_matrix[16];
            color_matrix.asColMajorf(expected_matrix);
            for (int i = 0; i < 16; ++i)
              EXPECT_FLOAT_EQ(expected_matrix[i], gl_matrix[i]);
          })));

  renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
  DrawFrame(&renderer, viewport_size);

  EXPECT_EQ(1, call_count);
  EXPECT_TRUE(output_color_matrix_invoked);
}

class GenerateMipmapMockGLESInterface : public TestGLES2Interface {
 public:
  GenerateMipmapMockGLESInterface() = default;

  MOCK_METHOD3(TexParameteri, void(GLenum target, GLenum pname, GLint param));
  MOCK_METHOD1(GenerateMipmap, void(GLenum target));
};

// TODO(crbug.com/803286): Currently npot texture always return false on ubuntu
// desktop.  The npot texture check is probably failing on desktop GL. This test
// crashes DCHECK npot texture to catch this. When
// GLRendererPixelTest.DISABLED_TrilinearFiltering got passed, can remove this.
TEST_F(GLRendererTest, GenerateMipmap) {
  // Initialize the mock GL interface, the output surface and the renderer.
  auto gl_owned = std::make_unique<GenerateMipmapMockGLESInterface>();
  gl_owned->set_support_texture_npot(true);

  auto* gl = gl_owned.get();
  auto provider = TestContextProvider::Create(std::move(gl_owned));
  provider->BindToCurrentThread();

  std::unique_ptr<FakeOutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  cc::FakeOutputSurfaceClient output_surface_client;
  output_surface->BindToClient(&output_surface_client);
  auto resource_provider = std::make_unique<DisplayResourceProviderGL>(
      output_surface->context_provider());
  RendererSettings settings;
  FakeRendererGL renderer(&settings, &debug_settings_, output_surface.get(),
                          resource_provider.get());
  renderer.Initialize();
  renderer.SetVisible(true);

  gfx::Size viewport_size(100, 100);
  AggregatedRenderPassId child_pass_id{2};
  // Create a child pass with mipmap to verify that npot texture is enabled.
  AggregatedRenderPass* child_pass =
      cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                        gfx::Rect(viewport_size) + gfx::Vector2d(1, 2),
                        gfx::Transform(), cc::FilterOperations());
  child_pass->generate_mipmap = true;

  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  root_pass->damage_rect = gfx::Rect(0, 0, 25, 25);
  cc::AddRenderPassQuad(root_pass, child_pass);
  renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  EXPECT_CALL(*gl, TexParameteri(_, _, _)).Times(4);
  EXPECT_CALL(*gl, GenerateMipmap(GL_TEXTURE_2D)).Times(1);
  // When generate_mipmap enabled, the GL_TEXTURE_MIN_FILTER should be
  // GL_LINEAR_MIPMAP_LINEAR.
  EXPECT_CALL(*gl, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                 GL_LINEAR_MIPMAP_LINEAR));
  DrawFrame(&renderer, viewport_size);
}

class FastSolidColorMockGLES2Interface : public TestGLES2Interface {
 public:
  FastSolidColorMockGLES2Interface() = default;

  MOCK_METHOD1(Enable, void(GLenum cap));
  MOCK_METHOD1(Disable, void(GLenum cap));
  MOCK_METHOD4(ClearColor,
               void(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha));
  MOCK_METHOD4(Scissor, void(GLint x, GLint y, GLsizei width, GLsizei height));
};

class GLRendererFastSolidColorTest : public GLRendererTest {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kFastSolidColorDraw);
    GLRendererTest::SetUp();

    auto gl_owned = std::make_unique<FastSolidColorMockGLES2Interface>();
    gl_owned->set_have_post_sub_buffer(true);
    gl_ = gl_owned.get();

    auto provider = TestContextProvider::Create(std::move(gl_owned));
    provider->BindToCurrentThread();

    output_surface_ = FakeOutputSurface::Create3d(std::move(provider));
    output_surface_->BindToClient(&output_surface_client_);

    resource_provider_ = std::make_unique<DisplayResourceProviderGL>(
        output_surface_->context_provider());

    settings_.partial_swap_enabled = true;
    settings_.slow_down_compositing_scale_factor = 1;
    settings_.allow_antialiasing = true;

    fake_renderer_ = std::make_unique<FakeRendererGL>(
        &settings_, &debug_settings_, output_surface_.get(),
        resource_provider_.get());
    fake_renderer_->Initialize();
    EXPECT_TRUE(fake_renderer_->use_partial_swap());
    fake_renderer_->SetVisible(true);
  }

  void TearDown() override {
    resource_provider_.reset();
    fake_renderer_.reset();
    output_surface_.reset();
    gl_ = nullptr;

    GLRendererTest::TearDown();
  }

  FastSolidColorMockGLES2Interface* gl_ptr() { return gl_; }

  FakeOutputSurface* output_surface() { return output_surface_.get(); }

 protected:
  void AddExpectations(bool use_fast_path,
                       const gfx::Rect& scissor_rect,
                       SkColor color = SK_ColorBLACK,
                       bool enable_stencil = false) {
    auto* gl = gl_ptr();

    InSequence seq;

    // Restore GL state method calls
    EXPECT_CALL(*gl, Disable(GL_DEPTH_TEST));
    EXPECT_CALL(*gl, Disable(GL_CULL_FACE));
    EXPECT_CALL(*gl, Disable(GL_STENCIL_TEST));
    EXPECT_CALL(*gl, Enable(GL_BLEND));
    EXPECT_CALL(*gl, Disable(GL_SCISSOR_TEST));
    EXPECT_CALL(*gl, Scissor(0, 0, 0, 0));

    if (!enable_stencil)
      EXPECT_CALL(*gl, ClearColor(0, 0, 0, 0));

    if (use_fast_path) {
      EXPECT_CALL(*gl, Enable(GL_SCISSOR_TEST));
      EXPECT_CALL(*gl, Scissor(scissor_rect.x(), scissor_rect.y(),
                               scissor_rect.width(), scissor_rect.height()));

      SkColor4f color_f = SkColor4f::FromColor(color);
      EXPECT_CALL(*gl,
                  ClearColor(color_f.fR, color_f.fG, color_f.fB, color_f.fA));

      EXPECT_CALL(*gl, Disable(GL_SCISSOR_TEST));
      EXPECT_CALL(*gl, Scissor(0, 0, 0, 0));
    }

    if (enable_stencil) {
      EXPECT_CALL(*gl, Enable(GL_STENCIL_TEST));
      EXPECT_CALL(*gl, Disable(GL_BLEND));
    }

    EXPECT_CALL(*gl, Disable(GL_BLEND));
  }

  void RunTest(const gfx::Size& viewport_size) {
    fake_renderer_->DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    DrawFrame(fake_renderer_.get(), viewport_size);

    auto* gl = gl_ptr();
    ASSERT_TRUE(gl);
    Mock::VerifyAndClearExpectations(gl);
  }

 private:
  FastSolidColorMockGLES2Interface* gl_ = nullptr;
  std::unique_ptr<FakeRendererGL> fake_renderer_;
  std::unique_ptr<FakeOutputSurface> output_surface_;
  std::unique_ptr<DisplayResourceProviderGL> resource_provider_;
  cc::FakeOutputSurfaceClient output_surface_client_;
  RendererSettings settings_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(GLRendererFastSolidColorTest, RoundedCorners) {
  gfx::Size viewport_size(500, 500);
  gfx::Rect root_pass_output_rect(400, 400);
  gfx::Rect root_pass_damage_rect(10, 20, 300, 200);
  gfx::Rect quad_rect(0, 50, 100, 100);

  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPass* root_pass = cc::AddRenderPassWithDamage(
      &render_passes_in_draw_order_, root_pass_id, root_pass_output_rect,
      root_pass_damage_rect, gfx::Transform(), cc::FilterOperations());
  root_pass->damage_rect = root_pass_damage_rect;
  cc::AddQuad(root_pass, quad_rect, SK_ColorRED);

  root_pass->shared_quad_state_list.front()->mask_filter_info =
      gfx::MaskFilterInfo(gfx::RRectF(gfx::RectF(quad_rect), 5.f));

  // Fast Solid color draw quads should not be executed.
  AddExpectations(false /*use_fast_path*/, gfx::Rect());

  RunTest(viewport_size);
}

TEST_F(GLRendererFastSolidColorTest, Transform3DSlowPath) {
  gfx::Size viewport_size(500, 500);
  gfx::Rect root_pass_damage_rect(10, 20, 300, 200);
  gfx::Rect quad_rect(0, 50, 100, 100);

  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  root_pass->damage_rect = root_pass_damage_rect;
  cc::AddQuad(root_pass, quad_rect, SK_ColorRED);

  gfx::Transform tm_3d;
  tm_3d.RotateAboutYAxis(30.0);
  ASSERT_FALSE(tm_3d.IsFlat());

  root_pass->shared_quad_state_list.front()->quad_to_target_transform = tm_3d;

  AddExpectations(false /*use_fast_path*/, gfx::Rect());

  RunTest(viewport_size);
}

TEST_F(GLRendererFastSolidColorTest, NonTransform3DFastPath) {
  gfx::Size viewport_size(500, 500);
  gfx::Rect root_pass_damage_rect(10, 20, 300, 200);
  gfx::Rect quad_rect(0, 0, 200, 200);

  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  root_pass->damage_rect = root_pass_damage_rect;
  cc::AddQuad(root_pass, quad_rect, SK_ColorRED);

  gfx::Transform tm_non_3d;
  tm_non_3d.Translate(10.f, 10.f);
  ASSERT_TRUE(tm_non_3d.IsFlat());

  root_pass->shared_quad_state_list.front()->quad_to_target_transform =
      tm_non_3d;

  AddExpectations(true /*use_fast_path*/, gfx::Rect(10, 290, 200, 200),
                  SK_ColorRED);

  RunTest(viewport_size);
}

TEST_F(GLRendererFastSolidColorTest, NonAxisAlignSlowPath) {
  gfx::Size viewport_size(500, 500);
  gfx::Rect root_pass_damage_rect(10, 20, 300, 200);
  gfx::Rect quad_rect(0, 0, 200, 200);

  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  root_pass->damage_rect = root_pass_damage_rect;
  cc::AddQuad(root_pass, quad_rect, SK_ColorRED);

  gfx::Transform tm_non_axis_align;
  tm_non_axis_align.RotateAboutZAxis(45.0);
  ASSERT_TRUE(tm_non_axis_align.IsFlat());

  root_pass->shared_quad_state_list.front()->quad_to_target_transform =
      tm_non_axis_align;

  AddExpectations(false /*use_fast_path*/, gfx::Rect());

  RunTest(viewport_size);
}

TEST_F(GLRendererFastSolidColorTest, StencilSlowPath) {
  gfx::Size viewport_size(500, 500);
  gfx::Rect root_pass_damage_rect(10, 20, 300, 200);
  gfx::Rect quad_rect(0, 0, 200, 200);

  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  root_pass->damage_rect = root_pass_damage_rect;
  root_pass->has_transparent_background = false;

  cc::AddQuad(root_pass, quad_rect, SK_ColorRED);

  AddExpectations(false /*use_fast_path*/, gfx::Rect(), SK_ColorRED,
                  true /*enable_stencil*/);
  output_surface()->set_has_external_stencil_test(true);

  RunTest(viewport_size);
}

TEST_F(GLRendererFastSolidColorTest, NeedsBlendingSlowPath) {
  gfx::Size viewport_size(500, 500);
  gfx::Rect root_pass_damage_rect(2, 3, 300, 200);
  gfx::Rect full_quad_rect(0, 0, 50, 50);
  gfx::Rect quad_rect_1(0, 0, 20, 20);
  gfx::Rect quad_rect_2(20, 0, 20, 20);
  gfx::Rect quad_rect_3(0, 20, 20, 20);

  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  root_pass->damage_rect = root_pass_damage_rect;

  cc::AddQuad(root_pass, quad_rect_1, SK_ColorRED);
  root_pass->quad_list.back()->needs_blending = true;

  cc::AddQuad(root_pass, quad_rect_2, SK_ColorBLUE);
  root_pass->shared_quad_state_list.back()->opacity = 0.5f;

  cc::AddQuad(root_pass, quad_rect_3, SK_ColorGREEN);
  root_pass->shared_quad_state_list.back()->blend_mode = SkBlendMode::kDstIn;

  cc::AddQuad(root_pass, full_quad_rect, SK_ColorBLACK);

  // The first solid color quad would use a fast path, but the other quads that
  // require blending will use the slower method.
  AddExpectations(true /*use_fast_path*/, gfx::Rect(0, 450, 50, 50),
                  SK_ColorBLACK, false /*enable_stencil*/);

  RunTest(viewport_size);
}

TEST_F(GLRendererFastSolidColorTest, NeedsBlendingFastPath) {
  gfx::Size viewport_size(500, 500);
  gfx::Rect root_pass_damage_rect(2, 3, 300, 200);
  gfx::Rect quad_rect_1(0, 0, 20, 20);
  gfx::Rect quad_rect_2(20, 0, 20, 20);
  gfx::Rect quad_rect_3(0, 20, 20, 20);

  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  root_pass->damage_rect = root_pass_damage_rect;

  cc::AddQuad(root_pass, quad_rect_1, SK_ColorRED);
  root_pass->quad_list.back()->needs_blending = true;

  cc::AddQuad(root_pass, quad_rect_2, SK_ColorBLUE);
  root_pass->shared_quad_state_list.back()->opacity = 0.5f;

  cc::AddQuad(root_pass, quad_rect_3, SK_ColorGREEN);
  root_pass->shared_quad_state_list.back()->blend_mode = SkBlendMode::kDstIn;

  auto* gl = gl_ptr();

  // The quads here despite having blend requirements can still use fast path
  // because they do not intersect with any other quad that has already been
  // drawn onto the render target.
  InSequence seq;

  // // Restore GL state method calls
  EXPECT_CALL(*gl, Disable(GL_DEPTH_TEST));
  EXPECT_CALL(*gl, Disable(GL_CULL_FACE));
  EXPECT_CALL(*gl, Disable(GL_STENCIL_TEST));
  EXPECT_CALL(*gl, Enable(GL_BLEND));
  EXPECT_CALL(*gl, Disable(GL_SCISSOR_TEST));
  EXPECT_CALL(*gl, Scissor(0, 0, 0, 0));
  EXPECT_CALL(*gl, ClearColor(0, 0, 0, 0));

  // Fast path draw used for green quad.
  EXPECT_CALL(*gl, Enable(GL_SCISSOR_TEST));
  EXPECT_CALL(*gl, Scissor(0, 460, 20, 20));
  EXPECT_CALL(*gl, ClearColor(0, 1, 0, 1));
  EXPECT_CALL(*gl, Disable(GL_SCISSOR_TEST));
  EXPECT_CALL(*gl, Scissor(0, 0, 0, 0));

  // Fast path draw used for blue quad.
  EXPECT_CALL(*gl, Enable(GL_SCISSOR_TEST));
  EXPECT_CALL(*gl, Scissor(20, 480, 20, 20));
  EXPECT_CALL(*gl, ClearColor(0, 0, 0.5f, 0.5f));
  EXPECT_CALL(*gl, Disable(GL_SCISSOR_TEST));
  EXPECT_CALL(*gl, Scissor(0, 0, 0, 0));

  // Fast path draw used for red quad.
  EXPECT_CALL(*gl, Enable(GL_SCISSOR_TEST));
  EXPECT_CALL(*gl, Scissor(0, 480, 20, 20));
  EXPECT_CALL(*gl, ClearColor(1, 0, 0, 1));
  EXPECT_CALL(*gl, Disable(GL_SCISSOR_TEST));
  EXPECT_CALL(*gl, Scissor(0, 0, 0, 0));

  EXPECT_CALL(*gl, Disable(GL_BLEND));

  RunTest(viewport_size);
}

TEST_F(GLRendererFastSolidColorTest, AntiAliasSlowPath) {
  gfx::Size viewport_size(500, 500);
  gfx::Rect root_pass_damage_rect(10, 20, 300, 200);
  gfx::Rect quad_rect(0, 0, 200, 200);

  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
      gfx::Transform(), cc::FilterOperations());
  root_pass->damage_rect = root_pass_damage_rect;
  cc::AddQuad(root_pass, quad_rect, SK_ColorRED);

  gfx::Transform tm_aa;
  tm_aa.Translate(0.1f, 0.1f);
  ASSERT_TRUE(tm_aa.IsFlat());

  root_pass->shared_quad_state_list.front()->quad_to_target_transform = tm_aa;

  AddExpectations(false /*use_fast_path*/, gfx::Rect());

  RunTest(viewport_size);
}

class PartialSwapMockGLES2Interface : public TestGLES2Interface {
 public:
  PartialSwapMockGLES2Interface() = default;

  MOCK_METHOD1(Enable, void(GLenum cap));
  MOCK_METHOD1(Disable, void(GLenum cap));
  MOCK_METHOD4(Scissor, void(GLint x, GLint y, GLsizei width, GLsizei height));
  MOCK_METHOD1(SetEnableDCLayersCHROMIUM, void(GLboolean enable));
};

class GLRendererPartialSwapTest : public GLRendererTest {
 public:
  void SetUp() override {
    // Force enable fast solid color draw path.
    scoped_feature_list_.InitAndEnableFeature(features::kFastSolidColorDraw);
    GLRendererTest::SetUp();
  }

 protected:
  void RunTest(bool partial_swap, bool set_draw_rectangle) {
    auto gl_owned = std::make_unique<PartialSwapMockGLES2Interface>();
    gl_owned->set_have_post_sub_buffer(true);

    auto* gl = gl_owned.get();

    auto provider = TestContextProvider::Create(std::move(gl_owned));
    provider->BindToCurrentThread();

    cc::FakeOutputSurfaceClient output_surface_client;
    std::unique_ptr<FakeOutputSurface> output_surface(
        FakeOutputSurface::Create3d(std::move(provider)));
    output_surface->set_supports_dc_layers(set_draw_rectangle);
    output_surface->BindToClient(&output_surface_client);

    auto resource_provider = std::make_unique<DisplayResourceProviderGL>(
        output_surface->context_provider());

    RendererSettings settings;
    settings.partial_swap_enabled = partial_swap;
    FakeRendererGL renderer(&settings, &debug_settings_, output_surface.get(),
                            resource_provider.get());
    renderer.Initialize();
    EXPECT_EQ(partial_swap, renderer.use_partial_swap());
    renderer.SetVisible(true);

    gfx::Size viewport_size(100, 100);
    gfx::Rect root_pass_output_rect(80, 80);
    gfx::Rect root_pass_damage_rect(2, 2, 3, 3);

    // Draw one black frame to make sure the output surface is reshaped before
    // tests.
    EXPECT_CALL(*gl, Disable(GL_DEPTH_TEST)).Times(1);
    EXPECT_CALL(*gl, Disable(GL_CULL_FACE)).Times(1);
    EXPECT_CALL(*gl, Disable(GL_STENCIL_TEST)).Times(1);
    EXPECT_CALL(*gl, Enable(GL_BLEND)).Times(1);

    if (output_surface->capabilities().supports_dc_layers) {
      EXPECT_CALL(*gl, Disable(GL_SCISSOR_TEST)).Times(1);
      EXPECT_CALL(*gl, Scissor(0, 0, 0, 0)).Times(1);

      // Root render pass requires a scissor if the output surface supports
      // dc layers.
      EXPECT_CALL(*gl, Enable(GL_SCISSOR_TEST)).Times(3);
      EXPECT_CALL(*gl, Scissor(0, 0, 100, 100)).Times(3);
    } else {
      EXPECT_CALL(*gl, Disable(GL_SCISSOR_TEST)).Times(2);
      EXPECT_CALL(*gl, Scissor(0, 0, 0, 0)).Times(2);
      if (set_draw_rectangle) {
        EXPECT_CALL(*gl, Enable(GL_SCISSOR_TEST)).Times(2);
        EXPECT_CALL(*gl, Scissor(0, 0, 100, 100)).Times(2);
      } else {
        EXPECT_CALL(*gl, Enable(GL_SCISSOR_TEST)).Times(1);
        EXPECT_CALL(*gl, Scissor(0, 0, 100, 100)).Times(1);
      }
    }

    EXPECT_CALL(*gl, Disable(GL_BLEND)).Times(1);

    AggregatedRenderPassId root_pass_id{1};
    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    root_pass->damage_rect = gfx::Rect(viewport_size);
    cc::AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorBLACK);

    renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
    DrawFrame(&renderer, viewport_size);
    Mock::VerifyAndClearExpectations(gl);

    for (int i = 0; i < 2; ++i) {
      AggregatedRenderPass* root_pass = cc::AddRenderPassWithDamage(
          &render_passes_in_draw_order_, root_pass_id, root_pass_output_rect,
          root_pass_damage_rect, gfx::Transform(), cc::FilterOperations());
      cc::AddQuad(root_pass, gfx::Rect(root_pass_output_rect), SK_ColorGREEN);

      InSequence seq;

      // A bunch of initialization that happens.
      EXPECT_CALL(*gl, Disable(GL_DEPTH_TEST));
      EXPECT_CALL(*gl, Disable(GL_CULL_FACE));
      EXPECT_CALL(*gl, Disable(GL_STENCIL_TEST));
      EXPECT_CALL(*gl, Enable(GL_BLEND));
      EXPECT_CALL(*gl, Disable(GL_SCISSOR_TEST));
      EXPECT_CALL(*gl, Scissor(0, 0, 0, 0));

      // Partial frame, we should use a scissor to swap only that part when
      // partial swap is enabled.
      gfx::Rect output_rectangle =
          partial_swap ? root_pass_damage_rect : gfx::Rect(viewport_size);

      // The scissor is flipped, so subtract the y coord and height from the
      // bottom of the GL viewport.
      gfx::Rect scissor_rect(output_rectangle.x(),
                             viewport_size.height() - output_rectangle.y() -
                                 output_rectangle.height(),
                             output_rectangle.width(),
                             output_rectangle.height());

      // Drawing the solid color quad using glClear and scissor rect.
      EXPECT_CALL(*gl, Enable(GL_SCISSOR_TEST));
      EXPECT_CALL(*gl, Scissor(scissor_rect.x(), scissor_rect.y(),
                               scissor_rect.width(), scissor_rect.height()));

      if (partial_swap || set_draw_rectangle) {
        EXPECT_CALL(*gl, Enable(GL_SCISSOR_TEST));
        EXPECT_CALL(*gl, Scissor(scissor_rect.x(), scissor_rect.y(),
                                 scissor_rect.width(), scissor_rect.height()));
      }

      // Restore GL state after solid color draw quad.
      if (partial_swap || set_draw_rectangle) {
        EXPECT_CALL(*gl, Enable(GL_SCISSOR_TEST));
        EXPECT_CALL(*gl, Scissor(scissor_rect.x(), scissor_rect.y(),
                                 scissor_rect.width(), scissor_rect.height()));
      } else {
        EXPECT_CALL(*gl, Disable(GL_SCISSOR_TEST));
        EXPECT_CALL(*gl, Scissor(0, 0, 0, 0));
      }

      // Blending is disabled at the end of the frame.
      EXPECT_CALL(*gl, Disable(GL_BLEND));

      renderer.DecideRenderPassAllocationsForFrame(
          render_passes_in_draw_order_);
      DrawFrame(&renderer, viewport_size);

      if (set_draw_rectangle) {
        EXPECT_EQ(output_rectangle, output_surface->last_set_draw_rectangle());
      }

      Mock::VerifyAndClearExpectations(gl);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(GLRendererPartialSwapTest, PartialSwap) {
  RunTest(true, false);
}

TEST_F(GLRendererPartialSwapTest, NoPartialSwap) {
  RunTest(false, false);
}

#if defined(OS_WIN)
TEST_F(GLRendererPartialSwapTest, SetDrawRectangle_PartialSwap) {
  RunTest(true, true);
}

TEST_F(GLRendererPartialSwapTest, SetDrawRectangle_NoPartialSwap) {
  RunTest(false, true);
}

// Test that SetEnableDCLayersCHROMIUM is properly called when enabling
// and disabling DC layers.
TEST_F(GLRendererTest, DCLayerOverlaySwitch) {
  auto gl_owned = std::make_unique<PartialSwapMockGLES2Interface>();
  gl_owned->set_have_post_sub_buffer(true);
  auto* gl = gl_owned.get();

  auto provider = TestContextProvider::Create(std::move(gl_owned));
  provider->BindToCurrentThread();

  cc::FakeOutputSurfaceClient output_surface_client;
  std::unique_ptr<FakeOutputSurface> output_surface(
      FakeOutputSurface::Create3d(std::move(provider)));
  output_surface->set_supports_dc_layers(true);
  output_surface->BindToClient(&output_surface_client);

  auto parent_resource_provider = std::make_unique<DisplayResourceProviderGL>(
      output_surface->context_provider());

  auto child_context_provider = TestContextProvider::Create();
  child_context_provider->BindToCurrentThread();
  auto child_resource_provider = std::make_unique<ClientResourceProvider>();

  auto transfer_resource = TransferableResource::MakeGL(
      gpu::Mailbox::Generate(), GL_LINEAR, GL_TEXTURE_2D, gpu::SyncToken(),
      gfx::Size(256, 256), true);
  auto release_callback =
      SingleReleaseCallback::Create(base::BindOnce(&MailboxReleased));
  ResourceId resource_id = child_resource_provider->ImportResource(
      transfer_resource, std::move(release_callback));

  std::vector<ReturnedResource> returned_to_child;
  int child_id = parent_resource_provider->CreateChild(
      base::BindRepeating(&CollectResources, &returned_to_child));

  // Transfer resource to the parent.
  std::vector<ResourceId> resource_ids_to_transfer;
  resource_ids_to_transfer.push_back(resource_id);
  std::vector<TransferableResource> list;
  child_resource_provider->PrepareSendToParent(
      resource_ids_to_transfer, &list,
      static_cast<RasterContextProvider*>(child_context_provider.get()));
  parent_resource_provider->ReceiveFromChild(child_id, list);
  // In DisplayResourceProvider's namespace, use the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      parent_resource_provider->GetChildToParentMap(child_id);
  ResourceId parent_resource_id = resource_map[list[0].id];

  auto processor = std::make_unique<OverlayProcessorWin>(
      output_surface.get(),
      std::make_unique<DCLayerOverlayProcessor>(
          &debug_settings_, /*allowed_yuv_overlay_count=*/1, true));

  RendererSettings settings;
  settings.partial_swap_enabled = true;
  FakeRendererGL renderer(&settings, &debug_settings_, output_surface.get(),
                          parent_resource_provider.get(), processor.get());
  renderer.Initialize();
  renderer.SetVisible(true);

  gfx::Size viewport_size(100, 100);

  for (int i = 0; i < 65; i++) {
    AggregatedRenderPassId root_pass_id{1};
    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    if (i == 0) {
      gfx::Rect rect(0, 0, 100, 100);
      bool needs_blending = false;
      gfx::RectF tex_coord_rect(0, 0, 1, 1);
      SharedQuadState* shared_state =
          root_pass->CreateAndAppendSharedQuadState();
      shared_state->SetAll(gfx::Transform(), rect, rect, gfx::MaskFilterInfo(),
                           rect, false, false, 1, SkBlendMode::kSrcOver, 0);
      YUVVideoDrawQuad* quad =
          root_pass->CreateAndAppendDrawQuad<YUVVideoDrawQuad>();
      quad->SetNew(shared_state, rect, rect, needs_blending, tex_coord_rect,
                   tex_coord_rect, rect.size(), rect.size(), parent_resource_id,
                   parent_resource_id, parent_resource_id, parent_resource_id,
                   gfx::ColorSpace(), 0, 1.0, 8);
    }

    // A bunch of initialization that happens.
    EXPECT_CALL(*gl, Disable(_)).Times(AnyNumber());
    EXPECT_CALL(*gl, Enable(_)).Times(AnyNumber());
    EXPECT_CALL(*gl, Scissor(_, _, _, _)).Times(AnyNumber());

    // Partial frame, we should use a scissor to swap only that part when
    // partial swap is enabled.
    root_pass->damage_rect = gfx::Rect(2, 2, 3, 3);
    // Frame 0 should be completely damaged because it's the first.
    // Frame 1 should be because it changed. Frame 60 should be
    // because it's disabling DC layers.
    gfx::Rect output_rectangle = (i == 0 || i == 1 || i == 60)
                                     ? root_pass->output_rect
                                     : root_pass->damage_rect;

    // Frame 0 should have DC Layers enabled because of the overlay.
    // After 60 frames of no overlays DC layers should be disabled again.
    if (i == 0)
      EXPECT_CALL(*gl, SetEnableDCLayersCHROMIUM(GL_TRUE));
    else if (i == 60)
      EXPECT_CALL(*gl, SetEnableDCLayersCHROMIUM(GL_FALSE));

    renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
    DrawFrame(&renderer, viewport_size);
    EXPECT_EQ(output_rectangle, output_surface->last_set_draw_rectangle());
    testing::Mock::VerifyAndClearExpectations(gl);
  }

  // Transfer resources back from the parent to the child. Set no resources as
  // being in use.
  parent_resource_provider->DeclareUsedResourcesFromChild(child_id,
                                                          ResourceIdSet());

  child_resource_provider->RemoveImportedResource(resource_id);
  child_resource_provider->ShutdownAndReleaseAllResources();
}
#endif

class GLRendererWithMockContextTest : public ::testing::Test {
 protected:
  class MockContextSupport : public TestContextSupport {
   public:
    MockContextSupport() {}
    MOCK_METHOD1(SetAggressivelyFreeResources,
                 void(bool aggressively_free_resources));
  };

  void SetUp() override {
    auto context_support = std::make_unique<MockContextSupport>();
    context_support_ptr_ = context_support.get();
    auto context_provider =
        TestContextProvider::Create(std::move(context_support));
    ASSERT_EQ(context_provider->BindToCurrentThread(),
              gpu::ContextResult::kSuccess);
    output_surface_ = FakeOutputSurface::Create3d(std::move(context_provider));
    output_surface_->BindToClient(&output_surface_client_);
    resource_provider_ = std::make_unique<DisplayResourceProviderGL>(
        output_surface_->context_provider());
    renderer_ = std::make_unique<GLRenderer>(
        &settings_, &debug_settings_, output_surface_.get(),
        resource_provider_.get(), nullptr, nullptr);
    renderer_->Initialize();
  }

  RendererSettings settings_;
  DebugRendererSettings debug_settings_;
  cc::FakeOutputSurfaceClient output_surface_client_;
  MockContextSupport* context_support_ptr_;
  std::unique_ptr<OutputSurface> output_surface_;
  std::unique_ptr<DisplayResourceProviderGL> resource_provider_;
  std::unique_ptr<GLRenderer> renderer_;
};

TEST_F(GLRendererWithMockContextTest,
       ContextPurgedWhenRendererBecomesInvisible) {
  EXPECT_CALL(*context_support_ptr_, SetAggressivelyFreeResources(false));
  renderer_->SetVisible(true);
  Mock::VerifyAndClearExpectations(context_support_ptr_);

  EXPECT_CALL(*context_support_ptr_, SetAggressivelyFreeResources(true));
  renderer_->SetVisible(false);
  Mock::VerifyAndClearExpectations(context_support_ptr_);
}

#if defined(USE_OZONE) || defined(OS_ANDROID)
class ContentBoundsOverlayProcessor : public OverlayProcessorUsingStrategy {
 public:
  class Strategy : public OverlayProcessorUsingStrategy::Strategy {
   public:
    explicit Strategy(const std::vector<gfx::Rect>& content_bounds)
        : content_bounds_(content_bounds) {}
    ~Strategy() override = default;

    bool Attempt(const SkMatrix44& output_color_matrix,
                 const OverlayProcessorInterface::FilterOperationsMap&
                     render_pass_backdrop_filters,
                 DisplayResourceProvider* resource_provider,
                 AggregatedRenderPassList* render_pass_list,
                 SurfaceDamageRectList* surface_damage_rect_list,
                 const PrimaryPlane* primary_plane,
                 OverlayCandidateList* candidates,
                 std::vector<gfx::Rect>* content_bounds) override {
      content_bounds->insert(content_bounds->end(), content_bounds_.begin(),
                             content_bounds_.end());
      return true;
    }

    void ProposePrioritized(
        const SkMatrix44& output_color_matrix,
        const FilterOperationsMap& render_pass_backdrop_filters,
        DisplayResourceProvider* resource_provider,
        AggregatedRenderPassList* render_pass_list,
        SurfaceDamageRectList* surface_damage_rect_list,
        const PrimaryPlane* primary_plane,
        OverlayProposedCandidateList* candidates,
        std::vector<gfx::Rect>* content_bounds) override {
      auto* render_pass = render_pass_list->back().get();
      QuadList& quad_list = render_pass->quad_list;
      OverlayCandidate candidate;
      // Adding a mock candidate to the propose list so that
      // 'AttemptPrioritized' will be called.
      candidates->push_back({quad_list.end(), candidate, this});
    }

    bool AttemptPrioritized(
        const SkMatrix44& output_color_matrix,
        const FilterOperationsMap& render_pass_backdrop_filters,
        DisplayResourceProvider* resource_provider,
        AggregatedRenderPassList* render_pass_list,
        SurfaceDamageRectList* surface_damage_rect_list,
        const PrimaryPlane* primary_plane,
        OverlayCandidateList* candidates,
        std::vector<gfx::Rect>* content_bounds,
        OverlayProposedCandidate* proposed_candidate) override {
      content_bounds->insert(content_bounds->end(), content_bounds_.begin(),
                             content_bounds_.end());
      return true;
    }

   private:
    const std::vector<gfx::Rect> content_bounds_;
  };

  explicit ContentBoundsOverlayProcessor(
      const std::vector<gfx::Rect>& content_bounds)
      : OverlayProcessorUsingStrategy(), content_bounds_(content_bounds) {
    strategies_.push_back(
        std::make_unique<Strategy>(std::move(content_bounds_)));
    prioritization_config_.changing_threshold = false;
    prioritization_config_.damage_rate_threshold = false;
  }

  Strategy& strategy() { return static_cast<Strategy&>(*strategies_.back()); }
  // Empty mock methods since this test set up uses strategies, which are only
  // for ozone and android.
  MOCK_CONST_METHOD0(NeedsSurfaceDamageRectList, bool());
  bool IsOverlaySupported() const override { return true; }

  // A list of possible overlay candidates is presented to this function.
  // The expected result is that those candidates that can be in a separate
  // plane are marked with |overlay_handled| set to true, otherwise they are
  // to be traditionally composited. Candidates with |overlay_handled| set to
  // true must also have their |display_rect| converted to integer
  // coordinates if necessary.
  void CheckOverlaySupport(
      const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
      OverlayCandidateList* surfaces) override {}

 private:
  std::vector<gfx::Rect> content_bounds_;
};

class GLRendererSwapWithBoundsTest : public GLRendererTest {
 protected:
  void RunTest(const std::vector<gfx::Rect>& content_bounds) {
    auto gl_owned = std::make_unique<TestGLES2Interface>();
    gl_owned->set_have_swap_buffers_with_bounds(true);

    auto provider = TestContextProvider::Create(std::move(gl_owned));
    provider->BindToCurrentThread();

    cc::FakeOutputSurfaceClient output_surface_client;
    std::unique_ptr<FakeOutputSurface> output_surface(
        FakeOutputSurface::Create3d(std::move(provider)));
    output_surface->BindToClient(&output_surface_client);

    auto resource_provider = std::make_unique<DisplayResourceProviderGL>(
        output_surface->context_provider());

    RendererSettings settings;
    auto processor =
        std::make_unique<ContentBoundsOverlayProcessor>(content_bounds);
    FakeRendererGL renderer(&settings, &debug_settings_, output_surface.get(),
                            resource_provider.get(), processor.get());
    renderer.Initialize();
    EXPECT_EQ(true, renderer.use_swap_with_bounds());
    renderer.SetVisible(true);

    gfx::Size viewport_size(100, 100);

    {
      AggregatedRenderPassId root_pass_id{1};
      cc::AddRenderPass(&render_passes_in_draw_order_, root_pass_id,
                        gfx::Rect(viewport_size), gfx::Transform(),
                        cc::FilterOperations());

      renderer.DecideRenderPassAllocationsForFrame(
          render_passes_in_draw_order_);
      DrawFrame(&renderer, viewport_size);
      renderer.SwapBuffers({});

      std::vector<gfx::Rect> expected_content_bounds;
      EXPECT_EQ(content_bounds,
                output_surface->last_sent_frame()->content_bounds);
    }
  }
};

TEST_F(GLRendererSwapWithBoundsTest, EmptyContent) {
  std::vector<gfx::Rect> content_bounds;
  RunTest(content_bounds);
}

TEST_F(GLRendererSwapWithBoundsTest, NonEmpty) {
  std::vector<gfx::Rect> content_bounds;
  content_bounds.push_back(gfx::Rect(0, 0, 10, 10));
  content_bounds.push_back(gfx::Rect(20, 20, 30, 30));
  RunTest(content_bounds);
}
#endif  // defined(USE_OZONE) || defined(OS_ANDROID)

#if defined(OS_APPLE)
class MockCALayerGLES2Interface : public TestGLES2Interface {
 public:
  MOCK_METHOD6(ScheduleCALayerSharedStateCHROMIUM,
               void(GLfloat opacity,
                    GLboolean is_clipped,
                    const GLfloat* clip_rect,
                    const GLfloat* rounded_corner_bounds,
                    GLint sorting_context_id,
                    const GLfloat* transform));
  MOCK_METHOD6(ScheduleCALayerCHROMIUM,
               void(GLuint contents_texture_id,
                    const GLfloat* contents_rect,
                    GLuint background_color,
                    GLuint edge_aa_mask,
                    const GLfloat* bounds_rect,
                    GLuint filter));
  MOCK_METHOD2(ScheduleCALayerInUseQueryCHROMIUM,
               void(GLsizei count, const GLuint* textures));
  MOCK_METHOD5(
      Uniform4f,
      void(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w));
};

class CALayerGLRendererTest : public GLRendererTest {
 protected:
  void SetUp() override {
    // A mock GLES2Interface that can watch CALayer stuff happen.
    auto gles2_interface = std::make_unique<MockCALayerGLES2Interface>();
    // Support image storage for GpuMemoryBuffers, needed for
    // CALayers/IOSurfaces backed by textures.
    gles2_interface->set_support_texture_storage_image(true);
    // Allow the renderer to make an empty SwapBuffers - skipping even the
    // root RenderPass.
    gles2_interface->set_have_commit_overlay_planes(true);

    gl_ = gles2_interface.get();

    auto provider = TestContextProvider::Create(std::move(gles2_interface));
    provider->BindToCurrentThread();

    cc::FakeOutputSurfaceClient output_surface_client;
    output_surface_ = FakeOutputSurface::Create3d(std::move(provider));
    output_surface_->BindToClient(&output_surface_client);

    display_resource_provider_ = std::make_unique<DisplayResourceProviderGL>(
        output_surface_->context_provider());

    settings_ = std::make_unique<RendererSettings>();
    // This setting is enabled to use CALayer overlays.
    settings_->release_overlay_resources_after_gpu_query = true;
    // The Mac TestOverlayProcessor default to enable CALayer overlays, then all
    // damage is removed and we can skip the root RenderPass, swapping empty.
    overlay_processor_ = std::make_unique<OverlayProcessorMac>(
        std::make_unique<CALayerOverlayProcessor>());
    renderer_ = std::make_unique<FakeRendererGL>(
        settings_.get(), &debug_settings_, output_surface_.get(),
        display_resource_provider_.get(), overlay_processor_.get(),
        base::ThreadTaskRunnerHandle::Get());
    renderer_->Initialize();
    renderer_->SetVisible(true);
  }

  void TearDown() override {
    renderer_.reset();
    display_resource_provider_.reset();
    output_surface_.reset();
  }

  void DrawBlackFrame(const gfx::Size& viewport_size) {
    AggregatedRenderPassId root_pass_id{1};

    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorBLACK);

    renderer().DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);

    DrawFrame(&renderer(), viewport_size);
    renderer().SwapBuffers(DirectRenderer::SwapFrameData());
    renderer().SwapBuffersComplete();
    Mock::VerifyAndClearExpectations(&gl());
  }

  MockCALayerGLES2Interface& gl() const { return *gl_; }
  FakeRendererGL& renderer() const { return *renderer_; }
  FakeOutputSurface& output_surface() const { return *output_surface_; }

 private:
  MockCALayerGLES2Interface* gl_;
  std::unique_ptr<FakeOutputSurface> output_surface_;
  std::unique_ptr<DisplayResourceProviderGL> display_resource_provider_;
  std::unique_ptr<RendererSettings> settings_;
  std::unique_ptr<OverlayProcessorInterface> overlay_processor_;
  std::unique_ptr<FakeRendererGL> renderer_;
};

TEST_F(CALayerGLRendererTest, CALayerOverlaysWithAllQuadsPromoted) {
  gfx::Size viewport_size(10, 10);

  // Draw an empty frame to make sure output surface is reshaped before tests.
  DrawBlackFrame(viewport_size);

  // This frame has a root pass with a CompositorRenderPassDrawQuad pointing to
  // a child pass that is at 1,2 to make it identifiable.
  AggregatedRenderPassId child_pass_id{2};
  AggregatedRenderPassId root_pass_id{1};
  {
    AggregatedRenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          gfx::Rect(viewport_size) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, kInvalidResourceId,
                          gfx::Transform(), SkBlendMode::kSrcOver);
  }

  renderer().DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  // The child pass is drawn, promoted to an overlay, and scheduled as a
  // CALayer.
  {
    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
        .WillOnce(
            Invoke([](GLuint contents_texture_id, const GLfloat* contents_rect,
                      GLuint background_color, GLuint edge_aa_mask,
                      const GLfloat* bounds_rect, GLuint filter) {
              // This is the child CompositorRenderPassDrawQuad.
              EXPECT_EQ(1, bounds_rect[0]);
              EXPECT_EQ(2, bounds_rect[1]);
            }));
  }
  DrawFrame(&renderer(), viewport_size);
  Mock::VerifyAndClearExpectations(&gl());

  renderer().SwapBuffers(DirectRenderer::SwapFrameData());

  // The damage was eliminated when everything was promoted to CALayers.
  ASSERT_TRUE(output_surface().last_sent_frame()->sub_buffer_rect);
  EXPECT_TRUE(output_surface().last_sent_frame()->sub_buffer_rect->IsEmpty());

  // Frame number 2. Same inputs, except...
  {
    AggregatedRenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          gfx::Rect(viewport_size) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, kInvalidResourceId,
                          gfx::Transform(), SkBlendMode::kSrcOver);

    // Use a cached RenderPass for the child.
    child_pass->cache_render_pass = true;
  }

  renderer().DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  // The child CompositorRenderPassDrawQuad gets promoted again, but importantly
  // it did not itself have to be drawn this time as it can use the cached
  // texture. Because we can skip the child pass, and the root pass (all quads
  // were promoted), this exposes edge cases in GLRenderer if it assumes we draw
  // at least one RenderPass. This still works, doesn't crash, etc, and the
  // CompositorRenderPassDrawQuad is emitted.
  {
    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _));
  }
  DrawFrame(&renderer(), viewport_size);
  Mock::VerifyAndClearExpectations(&gl());

  renderer().SwapBuffers(DirectRenderer::SwapFrameData());
}

TEST_F(CALayerGLRendererTest, CALayerRoundRects) {
  gfx::Size viewport_size(10, 10);

  // Draw an empty frame to make sure output surface is reshaped before tests.
  DrawBlackFrame(viewport_size);

  for (size_t subtest = 0; subtest < 3; ++subtest) {
    AggregatedRenderPass* child_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, AggregatedRenderPassId{1},
        gfx::Rect(250, 250), gfx::Transform(), cc::FilterOperations());

    AggregatedRenderPassId root_pass_id{1};
    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    auto* quad = cc::AddRenderPassQuad(root_pass, child_pass);
    SharedQuadState* sqs =
        const_cast<SharedQuadState*>(quad->shared_quad_state);

    sqs->is_clipped = true;
    sqs->clip_rect = gfx::Rect(2, 2, 6, 6);
    const float radius = 2;
    sqs->mask_filter_info =
        gfx::MaskFilterInfo(gfx::RRectF(gfx::RectF(sqs->clip_rect), radius));

    switch (subtest) {
      case 0:
        // Subtest 0 is a simple round rect that matches the clip rect, and
        // should be handled by CALayers.
        EXPECT_CALL(gl(), Uniform4f(_, _, _, _, _)).Times(1);
        EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _, _))
            .Times(1);
        EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _)).Times(1);
        break;
      case 1:
        // Subtest 1 doesn't match clip and rounded rect, but we can still
        // use CALayers.
        sqs->clip_rect = gfx::Rect(3, 3, 4, 4);
        EXPECT_CALL(gl(), Uniform4f(_, _, _, _, _)).Times(1);
        EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _)).Times(1);
        break;
      case 2:
        // Subtest 2 has a non-simple rounded rect.
        gfx::RRectF rounded_corner_bounds =
            sqs->mask_filter_info.rounded_corner_bounds();
        rounded_corner_bounds.SetCornerRadii(gfx::RRectF::Corner::kUpperLeft, 1,
                                             1);
        sqs->mask_filter_info = gfx::MaskFilterInfo(rounded_corner_bounds);
        // Called 2 extra times in order to set up the rounded corner
        // parameters in the shader, because the CALayer is not handling
        // the rounded corners.
        EXPECT_CALL(gl(), Uniform4f(_, _, _, _, _)).Times(3);
        EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _)).Times(0);
        break;
    }

    renderer().DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);
    DrawFrame(&renderer(), viewport_size);
    Mock::VerifyAndClearExpectations(&gl());
  }
}

TEST_F(CALayerGLRendererTest, CALayerOverlaysReusesTextureWithDifferentSizes) {
  gfx::Size viewport_size(300, 300);

  // Draw an empty frame to make sure output surface is reshaped before tests.
  DrawBlackFrame(viewport_size);

  // This frame has a root pass with a CompositorRenderPassDrawQuad pointing to
  // a child pass that is at 1,2 to make it identifiable. The child's size is
  // 250x251, but it will be rounded up to a multiple of 64 in order to promote
  // easier texture reuse. See https://crbug.com/146070.
  AggregatedRenderPassId child_pass_id{2};
  AggregatedRenderPassId root_pass_id{1};
  {
    AggregatedRenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          gfx::Rect(250, 251) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, kInvalidResourceId,
                          gfx::Transform(), SkBlendMode::kSrcOver);
  }

  renderer().DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  // The child pass is drawn, promoted to an overlay, and scheduled as a
  // CALayer. The bounds of the texture are rounded up to 256x256. We save the
  // texture ID to make sure we reuse it correctly.
  uint32_t saved_texture_id = 0;
  {
    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
        .WillOnce(
            Invoke([&](GLuint contents_texture_id, const GLfloat* contents_rect,
                       GLuint background_color, GLuint edge_aa_mask,
                       const GLfloat* bounds_rect, GLuint filter) {
              // This is the child CompositorRenderPassDrawQuad.
              EXPECT_EQ(1, bounds_rect[0]);
              EXPECT_EQ(2, bounds_rect[1]);
              // The size is rounded to a multiple of 64.
              EXPECT_EQ(256, bounds_rect[2]);
              EXPECT_EQ(256, bounds_rect[3]);
              saved_texture_id = contents_texture_id;
            }));
  }
  DrawFrame(&renderer(), viewport_size);
  Mock::VerifyAndClearExpectations(&gl());
  renderer().SwapBuffers(DirectRenderer::SwapFrameData());

  // ScheduleCALayerCHROMIUM happened and used a non-0 texture.
  EXPECT_NE(saved_texture_id, 0u);

  // The damage was eliminated when everything was promoted to CALayers.
  ASSERT_TRUE(output_surface().last_sent_frame()->sub_buffer_rect);
  EXPECT_TRUE(output_surface().last_sent_frame()->sub_buffer_rect->IsEmpty());

  // The texture will be checked to verify if it is free yet.
  EXPECT_CALL(gl(), ScheduleCALayerInUseQueryCHROMIUM(1, _));
  renderer().SwapBuffersComplete();
  Mock::VerifyAndClearExpectations(&gl());

  // Frame number 2. We change the size of the child RenderPass to be smaller
  // than the next multiple of 64, but larger than half the previous size so
  // that our texture reuse heuristics will reuse the texture if it is free.
  // For now, it is not.
  {
    AggregatedRenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          gfx::Rect(190, 191) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, kInvalidResourceId,
                          gfx::Transform(), SkBlendMode::kSrcOver);
  }

  renderer().DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  // The child RenderPass will use a new 192x192 texture, since the last texture
  // is still in use.
  {
    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
        .WillOnce(
            Invoke([&](GLuint contents_texture_id, const GLfloat* contents_rect,
                       GLuint background_color, GLuint edge_aa_mask,
                       const GLfloat* bounds_rect, GLuint filter) {
              // New texture id.
              EXPECT_NE(saved_texture_id, contents_texture_id);
              EXPECT_EQ(1, bounds_rect[0]);
              EXPECT_EQ(2, bounds_rect[1]);
              // The texture is 192x192 since we snap up to multiples of 64.
              EXPECT_EQ(192, bounds_rect[2]);
              EXPECT_EQ(192, bounds_rect[3]);
            }));
  }
  DrawFrame(&renderer(), viewport_size);
  Mock::VerifyAndClearExpectations(&gl());
  renderer().SwapBuffers(DirectRenderer::SwapFrameData());

  // There are now 2 textures to check if they are free.
  EXPECT_CALL(gl(), ScheduleCALayerInUseQueryCHROMIUM(2, _));
  renderer().SwapBuffersComplete();
  Mock::VerifyAndClearExpectations(&gl());

  // The first (256x256) texture is returned to the GLRenderer.
  renderer().DidReceiveTextureInUseResponses({{saved_texture_id, false}});

  // Frame number 3 looks just like frame number 2. The child RenderPass is
  // smaller than the next multiple of 64 from the released texture, but larger
  // than half of its size so that our texture reuse heuristics will kick in.
  {
    AggregatedRenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          gfx::Rect(190, 191) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, kInvalidResourceId,
                          gfx::Transform(), SkBlendMode::kSrcOver);
  }

  renderer().DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  // The child RenderPass would try to use a 192x192 texture, but since we have
  // an existing 256x256 texture, we can reuse that.
  {
    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
        .WillOnce(
            Invoke([&](GLuint contents_texture_id, const GLfloat* contents_rect,
                       GLuint background_color, GLuint edge_aa_mask,
                       const GLfloat* bounds_rect, GLuint filter) {
              // The first texture is reused.
              EXPECT_EQ(saved_texture_id, contents_texture_id);
              // This is the child CompositorRenderPassDrawQuad.
              EXPECT_EQ(1, bounds_rect[0]);
              EXPECT_EQ(2, bounds_rect[1]);
              // The size here is the size of the texture being used, not
              // the size we tried to use (192x192).
              EXPECT_EQ(256, bounds_rect[2]);
              EXPECT_EQ(256, bounds_rect[3]);
            }));
  }
  DrawFrame(&renderer(), viewport_size);
  Mock::VerifyAndClearExpectations(&gl());
  renderer().SwapBuffers(DirectRenderer::SwapFrameData());
}

TEST_F(CALayerGLRendererTest, CALayerOverlaysDontReuseTooBigTexture) {
  gfx::Size viewport_size(300, 300);

  // Draw an empty frame to make sure output surface is reshaped before tests.
  DrawBlackFrame(viewport_size);

  // This frame has a root pass with a CompositorRenderPassDrawQuad pointing to
  // a child pass that is at 1,2 to make it identifiable. The child's size is
  // 250x251, but it will be rounded up to a multiple of 64 in order to promote
  // easier texture reuse. See https://crbug.com/146070.
  AggregatedRenderPassId child_pass_id{2};
  AggregatedRenderPassId root_pass_id{1};
  {
    AggregatedRenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          gfx::Rect(250, 251) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, kInvalidResourceId,
                          gfx::Transform(), SkBlendMode::kSrcOver);
  }

  renderer().DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  // The child pass is drawn, promoted to an overlay, and scheduled as a
  // CALayer. The bounds of the texture are rounded up to 256x256. We save the
  // texture ID to make sure we reuse it correctly.
  uint32_t saved_texture_id = 0;
  {
    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
        .WillOnce(
            Invoke([&](GLuint contents_texture_id, const GLfloat* contents_rect,
                       GLuint background_color, GLuint edge_aa_mask,
                       const GLfloat* bounds_rect, GLuint filter) {
              // This is the child CompositorRenderPassDrawQuad.
              EXPECT_EQ(1, bounds_rect[0]);
              EXPECT_EQ(2, bounds_rect[1]);
              // The size is rounded to a multiple of 64.
              EXPECT_EQ(256, bounds_rect[2]);
              EXPECT_EQ(256, bounds_rect[3]);
              saved_texture_id = contents_texture_id;
            }));
  }
  DrawFrame(&renderer(), viewport_size);
  Mock::VerifyAndClearExpectations(&gl());
  renderer().SwapBuffers(DirectRenderer::SwapFrameData());

  // ScheduleCALayerCHROMIUM happened and used a non-0 texture.
  EXPECT_NE(saved_texture_id, 0u);

  // The damage was eliminated when everything was promoted to CALayers.
  ASSERT_TRUE(output_surface().last_sent_frame()->sub_buffer_rect);
  EXPECT_TRUE(output_surface().last_sent_frame()->sub_buffer_rect->IsEmpty());

  // The texture will be checked to verify if it is free yet.
  EXPECT_CALL(gl(), ScheduleCALayerInUseQueryCHROMIUM(1, _));
  renderer().SwapBuffersComplete();
  Mock::VerifyAndClearExpectations(&gl());

  // Frame number 2. We change the size of the child RenderPass to be much
  // smaller.
  {
    AggregatedRenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          gfx::Rect(20, 21) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, kInvalidResourceId,
                          gfx::Transform(), SkBlendMode::kSrcOver);
  }

  renderer().DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  // The child RenderPass will use a new 64x64 texture, since the last texture
  // is still in use.
  {
    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
        .WillOnce(
            Invoke([&](GLuint contents_texture_id, const GLfloat* contents_rect,
                       GLuint background_color, GLuint edge_aa_mask,
                       const GLfloat* bounds_rect, GLuint filter) {
              // New texture id.
              EXPECT_NE(saved_texture_id, contents_texture_id);
              EXPECT_EQ(1, bounds_rect[0]);
              EXPECT_EQ(2, bounds_rect[1]);
              // The texture is 64x64 since we snap up to multiples of 64.
              EXPECT_EQ(64, bounds_rect[2]);
              EXPECT_EQ(64, bounds_rect[3]);
            }));
  }
  DrawFrame(&renderer(), viewport_size);
  Mock::VerifyAndClearExpectations(&gl());
  renderer().SwapBuffers(DirectRenderer::SwapFrameData());

  // There are now 2 textures to check if they are free.
  EXPECT_CALL(gl(), ScheduleCALayerInUseQueryCHROMIUM(2, _));
  renderer().SwapBuffersComplete();
  Mock::VerifyAndClearExpectations(&gl());

  // The first (256x256) texture is returned to the GLRenderer.
  renderer().DidReceiveTextureInUseResponses({{saved_texture_id, false}});

  // Frame number 3 looks just like frame number 2. The child RenderPass is
  // too small to reuse the old texture.
  {
    AggregatedRenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          gfx::Rect(20, 21) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, kInvalidResourceId,
                          gfx::Transform(), SkBlendMode::kSrcOver);
  }

  renderer().DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  // The child RenderPass would try to use a 64x64 texture. We have a free and
  // existing 256x256 texture, but it's too large for us to reuse it.
  {
    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
        .WillOnce(
            Invoke([&](GLuint contents_texture_id, const GLfloat* contents_rect,
                       GLuint background_color, GLuint edge_aa_mask,
                       const GLfloat* bounds_rect, GLuint filter) {
              // The first texture is not reused.
              EXPECT_NE(saved_texture_id, contents_texture_id);
              // This is the child CompositorRenderPassDrawQuad.
              EXPECT_EQ(1, bounds_rect[0]);
              EXPECT_EQ(2, bounds_rect[1]);
              // The new texture has a smaller size.
              EXPECT_EQ(64, bounds_rect[2]);
              EXPECT_EQ(64, bounds_rect[3]);
            }));
  }
  DrawFrame(&renderer(), viewport_size);
  Mock::VerifyAndClearExpectations(&gl());
  renderer().SwapBuffers(DirectRenderer::SwapFrameData());
}

TEST_F(CALayerGLRendererTest, CALayerOverlaysReuseAfterNoSwapBuffers) {
  gfx::Size viewport_size(300, 300);

  // This frame has a root pass with a CompositorRenderPassDrawQuad pointing to
  // a child pass that is at 1,2 to make it identifiable.
  AggregatedRenderPassId child_pass_id{2};
  AggregatedRenderPassId root_pass_id{1};
  {
    AggregatedRenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          gfx::Rect(100, 100) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, kInvalidResourceId,
                          gfx::Transform(), SkBlendMode::kSrcOver);
  }

  renderer().DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  // The child pass is drawn, promoted to an overlay, and scheduled as a
  // CALayer. We save the texture ID to make sure we reuse it correctly.
  uint32_t saved_texture_id = 0;
  {
    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
        .WillOnce(
            Invoke([&](GLuint contents_texture_id, const GLfloat* contents_rect,
                       GLuint background_color, GLuint edge_aa_mask,
                       const GLfloat* bounds_rect, GLuint filter) {
              // This is the child CompositorRenderPassDrawQuad.
              EXPECT_EQ(1, bounds_rect[0]);
              EXPECT_EQ(2, bounds_rect[1]);
              saved_texture_id = contents_texture_id;
            }));
  }
  DrawFrame(&renderer(), viewport_size);
  Mock::VerifyAndClearExpectations(&gl());

  // ScheduleCALayerCHROMIUM happened and used a non-0 texture.
  EXPECT_NE(saved_texture_id, 0u);

  // SwapBuffers() is *not* called though! Display can do this sometimes.

  // Frame number 2. We can not reuse the texture since the last one isn't
  // returned yet. We use a different size so we can control which texture gets
  // reused later.
  {
    AggregatedRenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          gfx::Rect(200, 200) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, kInvalidResourceId,
                          gfx::Transform(), SkBlendMode::kSrcOver);
  }

  renderer().DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  uint32_t second_saved_texture_id = 0;
  {
    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
        .WillOnce(
            Invoke([&](GLuint contents_texture_id, const GLfloat* contents_rect,
                       GLuint background_color, GLuint edge_aa_mask,
                       const GLfloat* bounds_rect, GLuint filter) {
              // New texture id.
              EXPECT_NE(saved_texture_id, contents_texture_id);
              EXPECT_EQ(1, bounds_rect[0]);
              EXPECT_EQ(2, bounds_rect[1]);
              second_saved_texture_id = contents_texture_id;
            }));
  }
  DrawFrame(&renderer(), viewport_size);
  Mock::VerifyAndClearExpectations(&gl());

  // SwapBuffers() *does* happen this time.
  renderer().SwapBuffers(DirectRenderer::SwapFrameData());

  // There are 2 textures to check if they are free.
  EXPECT_CALL(gl(), ScheduleCALayerInUseQueryCHROMIUM(2, _));
  renderer().SwapBuffersComplete();
  Mock::VerifyAndClearExpectations(&gl());

  // Both textures get returned and the 2nd one can be reused.
  renderer().DidReceiveTextureInUseResponses(
      {{saved_texture_id, false}, {second_saved_texture_id, false}});

  // Frame number 3 looks just like frame number 2.
  {
    AggregatedRenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          gfx::Rect(200, 200) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, kInvalidResourceId,
                          gfx::Transform(), SkBlendMode::kSrcOver);
  }

  renderer().DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  // The 2nd texture that we sent has been returned so we can reuse it. We
  // verify that happened.
  {
    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
        .WillOnce(
            Invoke([&](GLuint contents_texture_id, const GLfloat* contents_rect,
                       GLuint background_color, GLuint edge_aa_mask,
                       const GLfloat* bounds_rect, GLuint filter) {
              // The second texture is reused.
              EXPECT_EQ(second_saved_texture_id, contents_texture_id);
              // This is the child CompositorRenderPassDrawQuad.
              EXPECT_EQ(1, bounds_rect[0]);
              EXPECT_EQ(2, bounds_rect[1]);
            }));
  }
  DrawFrame(&renderer(), viewport_size);
  Mock::VerifyAndClearExpectations(&gl());
  renderer().SwapBuffers(DirectRenderer::SwapFrameData());
}

TEST_F(CALayerGLRendererTest, CALayerOverlaysReuseManyIfReturnedSlowly) {
  gfx::Size viewport_size(300, 300);

  // Draw an empty frame to make sure output surface is reshaped before tests.
  DrawBlackFrame(viewport_size);

  // Each frame has a root pass with a CompositorRenderPassDrawQuad pointing to
  // a child pass. We generate a bunch of frames and swap them, each with a
  // different child RenderPass id, without getting any of the resources back
  // from the OS.
  AggregatedRenderPassId root_pass_id{1};

  // The number is at least 2 larger than the number of textures we expect to
  // reuse, so that we can leave one in the OS, and have 1 texture returned but
  // not reused.
  const int kNumSendManyTextureIds = 7;
  uint32_t sent_texture_ids[kNumSendManyTextureIds];
  for (int i = 0; i < kNumSendManyTextureIds; ++i) {
    AggregatedRenderPass* child_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, AggregatedRenderPassId{i + 2},
        gfx::Rect(250, 251) + gfx::Vector2d(1, 2), gfx::Transform(),
        cc::FilterOperations());
    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, kInvalidResourceId,
                          gfx::Transform(), SkBlendMode::kSrcOver);

    renderer().DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);

    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
        .WillOnce(
            Invoke([&](GLuint contents_texture_id, const GLfloat* contents_rect,
                       GLuint background_color, GLuint edge_aa_mask,
                       const GLfloat* bounds_rect, GLuint filter) {
              // This is the child CompositorRenderPassDrawQuad.
              EXPECT_EQ(1, bounds_rect[0]);
              EXPECT_EQ(2, bounds_rect[1]);
              sent_texture_ids[i] = contents_texture_id;
            }));
    DrawFrame(&renderer(), viewport_size);
    Mock::VerifyAndClearExpectations(&gl());
    renderer().SwapBuffers(DirectRenderer::SwapFrameData());

    // ScheduleCALayerCHROMIUM happened and used a non-0 texture.
    EXPECT_NE(sent_texture_ids[i], 0u);

    // The damage was eliminated when everything was promoted to CALayers.
    ASSERT_TRUE(output_surface().last_sent_frame()->sub_buffer_rect);
    EXPECT_TRUE(output_surface().last_sent_frame()->sub_buffer_rect->IsEmpty());

    // All sent textures will be checked to verify if they are free yet.
    EXPECT_CALL(gl(), ScheduleCALayerInUseQueryCHROMIUM(i + 1, _));
    renderer().SwapBuffersComplete();
    Mock::VerifyAndClearExpectations(&gl());
  }

  // Now all but 1 texture get returned by the OS, so they are all inserted
  // into the cache for reuse.
  std::vector<uint32_t> returned_texture_ids;
  for (int i = 0; i < kNumSendManyTextureIds - 1; ++i) {
    uint32_t id = sent_texture_ids[i];
    renderer().DidReceiveTextureInUseResponses({{id, false}});
    returned_texture_ids.push_back(id);
  }

  // We should keep *some* of these textures around to reuse them across
  // multiple frames. https://crbug.com/146070 motivates this, and empirical
  // testing found 5 to be a good number.
  const int kNumSendReusedTextures = 5;
  // See comment on |kNumSendManyTextureIds|.
  ASSERT_LT(kNumSendReusedTextures, kNumSendManyTextureIds - 1);

  for (int i = 0; i < kNumSendReusedTextures + 1; ++i) {
    // We use different RenderPass ids to ensure that the cache allows reuse
    // even if they don't match.
    AggregatedRenderPass* child_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, AggregatedRenderPassId{i + 100},
        gfx::Rect(250, 251) + gfx::Vector2d(1, 2), gfx::Transform(),
        cc::FilterOperations());
    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, kInvalidResourceId,
                          gfx::Transform(), SkBlendMode::kSrcOver);

    renderer().DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);

    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
        .WillOnce(Invoke([&](GLuint contents_texture_id,
                             const GLfloat* contents_rect,
                             GLuint background_color, GLuint edge_aa_mask,
                             const GLfloat* bounds_rect, GLuint filter) {
          // This is the child CompositorRenderPassDrawQuad.
          EXPECT_EQ(1, bounds_rect[0]);
          EXPECT_EQ(2, bounds_rect[1]);

          if (i < kNumSendReusedTextures) {
            // The texture id should be from the set of returned ones.
            EXPECT_THAT(returned_texture_ids, Contains(contents_texture_id));
            base::Erase(returned_texture_ids, contents_texture_id);
          } else {
            // More textures were returned at once than we expect to reuse
            // so eventually we should be making a new texture to show we're
            // not just keeping infinity textures in the cache.
            EXPECT_THAT(returned_texture_ids,
                        Not(Contains(contents_texture_id)));
            // This shows that there was some returned id that we didn't use.
            EXPECT_FALSE(returned_texture_ids.empty());
          }
        }));
    DrawFrame(&renderer(), viewport_size);
    Mock::VerifyAndClearExpectations(&gl());
    renderer().SwapBuffers(DirectRenderer::SwapFrameData());

    // All sent textures will be checked to verify if they are free yet. There's
    // also 1 outstanding texture to check for that wasn't returned yet from the
    // above loop.
    EXPECT_CALL(gl(), ScheduleCALayerInUseQueryCHROMIUM(i + 2, _));
    renderer().SwapBuffersComplete();
    Mock::VerifyAndClearExpectations(&gl());
  }
}

TEST_F(CALayerGLRendererTest, CALayerOverlaysCachedTexturesAreFreed) {
  gfx::Size viewport_size(300, 300);

  // Draw an empty frame to make sure output surface is reshaped before tests.
  DrawBlackFrame(viewport_size);

  // Each frame has a root pass with a CompositorRenderPassDrawQuad pointing to
  // a child pass. We generate a bunch of frames and swap them, each with a
  // different child RenderPass id, without getting any of the resources back
  // from the OS.
  AggregatedRenderPassId child_pass_id{2};
  AggregatedRenderPassId root_pass_id{1};

  // We send a whole bunch of textures as overlays to the OS.
  const int kNumSendManyTextureIds = 7;
  uint32_t sent_texture_ids[kNumSendManyTextureIds];
  for (int i = 0; i < kNumSendManyTextureIds; ++i) {
    AggregatedRenderPass* child_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, AggregatedRenderPassId{i + 2},
        gfx::Rect(250, 251) + gfx::Vector2d(1, 2), gfx::Transform(),
        cc::FilterOperations());
    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, kInvalidResourceId,
                          gfx::Transform(), SkBlendMode::kSrcOver);

    renderer().DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);

    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
        .WillOnce(
            Invoke([&](GLuint contents_texture_id, const GLfloat* contents_rect,
                       GLuint background_color, GLuint edge_aa_mask,
                       const GLfloat* bounds_rect, GLuint filter) {
              // This is the child CompositorRenderPassDrawQuad.
              EXPECT_EQ(1, bounds_rect[0]);
              EXPECT_EQ(2, bounds_rect[1]);
              sent_texture_ids[i] = contents_texture_id;
            }));
    DrawFrame(&renderer(), viewport_size);
    Mock::VerifyAndClearExpectations(&gl());
    renderer().SwapBuffers(DirectRenderer::SwapFrameData());

    // ScheduleCALayerCHROMIUM happened and used a non-0 texture.
    EXPECT_NE(sent_texture_ids[i], 0u);

    // The damage was eliminated when everything was promoted to CALayers.
    ASSERT_TRUE(output_surface().last_sent_frame()->sub_buffer_rect);
    EXPECT_TRUE(output_surface().last_sent_frame()->sub_buffer_rect->IsEmpty());

    // All sent textures will be checked to verify if they are free yet.
    EXPECT_CALL(gl(), ScheduleCALayerInUseQueryCHROMIUM(i + 1, _));
    renderer().SwapBuffersComplete();
    Mock::VerifyAndClearExpectations(&gl());
  }

  // Now all but 1 texture get returned by the OS, so they are all inserted
  // into the cache for reuse.
  std::vector<uint32_t> returned_texture_ids;
  for (int i = 0; i < kNumSendManyTextureIds - 1; ++i) {
    uint32_t id = sent_texture_ids[i];
    renderer().DidReceiveTextureInUseResponses({{id, false}});
    returned_texture_ids.push_back(id);
  }

  // We generate a bunch of frames that don't use the cache, one less than the
  // number of textures returned.
  for (int i = 0; i < kNumSendManyTextureIds - 2; ++i) {
    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddQuad(root_pass, gfx::Rect(100, 100), SK_ColorRED);

    renderer().DecideRenderPassAllocationsForFrame(
        render_passes_in_draw_order_);

    InSequence sequence;
    EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _, _));
    EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _));
    DrawFrame(&renderer(), viewport_size);
    Mock::VerifyAndClearExpectations(&gl());
    renderer().SwapBuffers(DirectRenderer::SwapFrameData());

    // There's just 1 outstanding RenderPass texture to query for.
    EXPECT_CALL(gl(), ScheduleCALayerInUseQueryCHROMIUM(1, _));
    renderer().SwapBuffersComplete();
    Mock::VerifyAndClearExpectations(&gl());
  }

  // By now the cache should be empty, to show that we don't keep cached
  // textures that won't be used forever. We generate a frame with a
  // CompositorRenderPassDrawQuad and verify that it does not reuse a texture
  // from the (empty) cache.
  {
    AggregatedRenderPass* child_pass =
        cc::AddRenderPass(&render_passes_in_draw_order_, child_pass_id,
                          gfx::Rect(250, 251) + gfx::Vector2d(1, 2),
                          gfx::Transform(), cc::FilterOperations());
    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, root_pass_id, gfx::Rect(viewport_size),
        gfx::Transform(), cc::FilterOperations());
    cc::AddRenderPassQuad(root_pass, child_pass, kInvalidResourceId,
                          gfx::Transform(), SkBlendMode::kSrcOver);
  }

  renderer().DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);

  InSequence sequence;
  EXPECT_CALL(gl(), ScheduleCALayerSharedStateCHROMIUM(_, _, _, _, _, _));
  EXPECT_CALL(gl(), ScheduleCALayerCHROMIUM(_, _, _, _, _, _))
      .WillOnce(Invoke([&](GLuint contents_texture_id,
                           const GLfloat* contents_rect,
                           GLuint background_color, GLuint edge_aa_mask,
                           const GLfloat* bounds_rect, GLuint filter) {
        // This is the child CompositorRenderPassDrawQuad.
        EXPECT_EQ(1, bounds_rect[0]);
        EXPECT_EQ(2, bounds_rect[1]);

        // More textures were returned at once than we expect to reuse
        // so eventually we should be making a new texture to show we're
        // not just keeping infinity textures in the cache.
        EXPECT_THAT(returned_texture_ids, Not(Contains(contents_texture_id)));
        // This shows that there was some returned id that we didn't use.
        EXPECT_FALSE(returned_texture_ids.empty());
      }));
  DrawFrame(&renderer(), viewport_size);
  Mock::VerifyAndClearExpectations(&gl());
  renderer().SwapBuffers(DirectRenderer::SwapFrameData());
}
#endif

class FramebufferWatchingGLRenderer : public FakeRendererGL {
 public:
  FramebufferWatchingGLRenderer(RendererSettings* settings,
                                const DebugRendererSettings* debug_settings,
                                OutputSurface* output_surface,
                                DisplayResourceProviderGL* resource_provider)
      : FakeRendererGL(settings,
                       debug_settings,
                       output_surface,
                       resource_provider) {}

  void BindFramebufferToOutputSurface() override {
    ++bind_root_framebuffer_calls_;
    FakeRendererGL::BindFramebufferToOutputSurface();
  }

  void BindFramebufferToTexture(
      const AggregatedRenderPassId render_pass_id) override {
    ++bind_child_framebuffer_calls_;
    FakeRendererGL::BindFramebufferToTexture(render_pass_id);
  }

  int bind_root_framebuffer_calls() const {
    return bind_root_framebuffer_calls_;
  }
  int bind_child_framebuffer_calls() const {
    return bind_child_framebuffer_calls_;
  }

  void ResetBindCalls() {
    bind_root_framebuffer_calls_ = bind_child_framebuffer_calls_ = 0;
  }

 private:
  int bind_root_framebuffer_calls_ = 0;
  int bind_child_framebuffer_calls_ = 0;
};

TEST_F(GLRendererTest, UndamagedRenderPassStillDrawnWhenNoPartialSwap) {
  auto provider = TestContextProvider::Create();
  provider->UnboundTestContextGL()->set_have_post_sub_buffer(true);
  provider->BindToCurrentThread();

  cc::FakeOutputSurfaceClient output_surface_client;
  auto output_surface = FakeOutputSurface::Create3d(std::move(provider));
  output_surface->BindToClient(&output_surface_client);

  auto resource_provider = std::make_unique<DisplayResourceProviderGL>(
      output_surface->context_provider());

  for (int i = 0; i < 2; ++i) {
    bool use_partial_swap = i == 0;
    SCOPED_TRACE(use_partial_swap);

    RendererSettings settings;
    settings.partial_swap_enabled = use_partial_swap;
    FramebufferWatchingGLRenderer renderer(&settings, &debug_settings_,
                                           output_surface.get(),
                                           resource_provider.get());
    renderer.Initialize();
    EXPECT_EQ(use_partial_swap, renderer.use_partial_swap());
    renderer.SetVisible(true);

    gfx::Size viewport_size(100, 100);
    gfx::Rect child_rect(10, 10);

    // First frame, the child and root RenderPass each have damage.
    AggregatedRenderPass* child_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, AggregatedRenderPassId{2}, child_rect,
        gfx::Transform(), cc::FilterOperations());
    cc::AddQuad(child_pass, child_rect, SK_ColorGREEN);
    child_pass->damage_rect = child_rect;

    AggregatedRenderPass* root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, AggregatedRenderPassId{1},
        gfx::Rect(viewport_size), gfx::Transform(), cc::FilterOperations());
    cc::AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorRED);
    cc::AddRenderPassQuad(root_pass, child_pass, kInvalidResourceId,
                          gfx::Transform(), SkBlendMode::kSrcOver);
    root_pass->damage_rect = gfx::Rect(viewport_size);

    EXPECT_EQ(0, renderer.bind_root_framebuffer_calls());
    EXPECT_EQ(0, renderer.bind_child_framebuffer_calls());

    renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
    DrawFrame(&renderer, viewport_size);

    // We had to draw the root, and the child.
    EXPECT_EQ(1, renderer.bind_child_framebuffer_calls());
    // When the CompositorRenderPassDrawQuad in the root is drawn, we may
    // re-bind the root framebuffer. So it can be bound more than once.
    EXPECT_GE(renderer.bind_root_framebuffer_calls(), 1);

    // Reset counting.
    renderer.ResetBindCalls();

    // Second frame, the child RenderPass has no damage in it.
    child_pass = cc::AddRenderPass(&render_passes_in_draw_order_,
                                   AggregatedRenderPassId{2}, child_rect,
                                   gfx::Transform(), cc::FilterOperations());
    cc::AddQuad(child_pass, child_rect, SK_ColorGREEN);
    child_pass->damage_rect = gfx::Rect();

    // Root RenderPass has some damage that doesn't intersect the child.
    root_pass = cc::AddRenderPass(
        &render_passes_in_draw_order_, AggregatedRenderPassId{1},
        gfx::Rect(viewport_size), gfx::Transform(), cc::FilterOperations());
    cc::AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorRED);
    cc::AddRenderPassQuad(root_pass, child_pass, kInvalidResourceId,
                          gfx::Transform(), SkBlendMode::kSrcOver);
    root_pass->damage_rect = gfx::Rect(child_rect.right(), 0, 10, 10);

    EXPECT_EQ(0, renderer.bind_root_framebuffer_calls());
    EXPECT_EQ(0, renderer.bind_child_framebuffer_calls());

    renderer.DecideRenderPassAllocationsForFrame(render_passes_in_draw_order_);
    DrawFrame(&renderer, viewport_size);

    if (use_partial_swap) {
      // Without damage overlapping the child, it didn't need to be drawn (it
      // may choose to anyway but that'd be a waste). So we don't check for
      // |bind_child_framebuffer_calls|. But the root should have been drawn.
      EXPECT_EQ(renderer.bind_root_framebuffer_calls(), 1);
    } else {
      // Without partial swap, we have to draw the child still, this means
      // the child is bound as the framebuffer.
      EXPECT_EQ(1, renderer.bind_child_framebuffer_calls());
      // When the CompositorRenderPassDrawQuad in the root is drawn, as it must
      // be since we must draw the entire output, we may re-bind the root
      // framebuffer. So it can be bound more than once.
      EXPECT_GE(renderer.bind_root_framebuffer_calls(), 1);
    }
  }
}

#if defined(USE_OZONE) || defined(OS_ANDROID)
class GLRendererWithGpuFenceTest : public GLRendererTest {
 protected:
  GLRendererWithGpuFenceTest() {
    auto provider = TestContextProvider::Create();
    provider->BindToCurrentThread();
    provider->TestContextGL()->set_have_commit_overlay_planes(true);
    test_context_support_ = provider->support();

    output_surface_ = FakeOutputSurface::Create3d(std::move(provider));
    output_surface_->set_overlay_texture_id(kSurfaceOverlayTextureId);
    output_surface_->set_gpu_fence_id(kGpuFenceId);
    resource_provider_ = std::make_unique<DisplayResourceProviderGL>(
        output_surface_->context_provider());
    overlay_processor_ = std::make_unique<SingleOverlayOnTopProcessor>();
    overlay_processor_->AllowMultipleCandidates();
    renderer_ = std::make_unique<FakeRendererGL>(
        &settings_, &debug_settings_, output_surface_.get(),
        resource_provider_.get(), overlay_processor_.get(),
        base::ThreadTaskRunnerHandle::Get());
    renderer_->Initialize();
    renderer_->SetVisible(true);

    test_context_support_->SetScheduleOverlayPlaneCallback(
        base::BindRepeating(&MockOverlayScheduler::Schedule,
                            base::Unretained(&overlay_scheduler_)));
  }

  ~GLRendererWithGpuFenceTest() override {
    if (child_resource_provider_)
      child_resource_provider_->ShutdownAndReleaseAllResources();
  }

  ResourceId create_overlay_resource() {
    child_context_provider_ = TestContextProvider::Create();
    child_context_provider_->BindToCurrentThread();

    child_resource_provider_ = std::make_unique<ClientResourceProvider>();
    auto transfer_resource = TransferableResource::MakeGL(
        gpu::Mailbox::Generate(), GL_LINEAR, GL_TEXTURE_2D, gpu::SyncToken(),
        gfx::Size(256, 256), true);
    ResourceId client_resource_id = child_resource_provider_->ImportResource(
        transfer_resource, SingleReleaseCallback::Create(base::DoNothing()));

    std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
        cc::SendResourceAndGetChildToParentMap(
            {client_resource_id}, resource_provider_.get(),
            child_resource_provider_.get(), child_context_provider_.get());
    return resource_map[client_resource_id];
  }

  static constexpr unsigned kSurfaceOverlayTextureId = 33;
  static constexpr unsigned kGpuFenceId = 66;
  static constexpr unsigned kGpuNoFenceId = 0;

  TestContextSupport* test_context_support_;

  cc::FakeOutputSurfaceClient output_surface_client_;
  std::unique_ptr<FakeOutputSurface> output_surface_;
  std::unique_ptr<DisplayResourceProviderGL> resource_provider_;
  scoped_refptr<TestContextProvider> child_context_provider_;
  std::unique_ptr<ClientResourceProvider> child_resource_provider_;
  RendererSettings settings_;
  std::unique_ptr<SingleOverlayOnTopProcessor> overlay_processor_;
  std::unique_ptr<FakeRendererGL> renderer_;
  MockOverlayScheduler overlay_scheduler_;
};

TEST_F(GLRendererWithGpuFenceTest, GpuFenceIdIsUsedWithRootRenderPassOverlay) {
  gfx::Size viewport_size(100, 100);
  AggregatedRenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, AggregatedRenderPassId{1},
      gfx::Rect(viewport_size), gfx::Transform(), cc::FilterOperations());
  root_pass->has_transparent_background = false;

  EXPECT_CALL(overlay_scheduler_,
              Schedule(0, gfx::OVERLAY_TRANSFORM_NONE, kSurfaceOverlayTextureId,
                       gfx::Rect(viewport_size), _, _, kGpuFenceId))
      .Times(1);
  DrawFrame(renderer_.get(), viewport_size);
}

TEST_F(GLRendererWithGpuFenceTest,
       GpuFenceIdIsUsedOnlyForRootRenderPassOverlay) {
#if defined(USE_X11)
  // TODO(1096425): Remove this.
  if (!features::IsUsingOzonePlatform())
    GTEST_SKIP();
#endif
  gfx::Size viewport_size(100, 100);
  AggregatedRenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order_, AggregatedRenderPassId{1},
      gfx::Rect(viewport_size), gfx::Transform(), cc::FilterOperations());
  root_pass->has_transparent_background = false;

  bool needs_blending = false;
  bool premultiplied_alpha = false;
  bool flipped = false;
  bool nearest_neighbor = false;
  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  gfx::PointF uv_top_left(0, 0);
  gfx::PointF uv_bottom_right(1, 1);

  TextureDrawQuad* overlay_quad =
      root_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  SharedQuadState* shared_state = root_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), gfx::Rect(viewport_size),
                       gfx::Rect(50, 50), gfx::MaskFilterInfo(),
                       gfx::Rect(viewport_size), false, false, 1,
                       SkBlendMode::kSrcOver, 0);
  overlay_quad->SetNew(
      shared_state, gfx::Rect(viewport_size), gfx::Rect(viewport_size),
      needs_blending, create_overlay_resource(), premultiplied_alpha,
      uv_top_left, uv_bottom_right, SK_ColorTRANSPARENT, vertex_opacity,
      flipped, nearest_neighbor,
      /*secure_output_only=*/false, gfx::ProtectedVideoType::kClear);

  EXPECT_CALL(overlay_scheduler_,
              Schedule(0, gfx::OVERLAY_TRANSFORM_NONE, kSurfaceOverlayTextureId,
                       gfx::Rect(viewport_size), _, _, kGpuFenceId))
      .Times(1);
  EXPECT_CALL(overlay_scheduler_,
              Schedule(1, gfx::OVERLAY_TRANSFORM_NONE, _,
                       gfx::Rect(viewport_size), _, _, kGpuNoFenceId))
      .Times(1);
  DrawFrame(renderer_.get(), viewport_size);
}
#endif  // defined(USE_OZONE) || defined(OS_ANDROID)

}  // namespace
}  // namespace viz
