// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>

#include "base/memory/aligned_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "cc/test/fake_raster_source.h"
#include "cc/test/fake_recording_source.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/render_pass_test_utils.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/picture_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/service/display/renderer_pixeltest_utils.h"
#include "components/viz/service/display/viz_pixel_test.h"
#include "components/viz/test/buildflags.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/renderers/video_resource_updater.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/mask_filter_info.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/video_types.h"
#include "ui/gl/gl_implementation.h"

namespace viz {
namespace {

const gfx::DisplayColorSpaces kRec601DisplayColorSpaces(
    gfx::ColorSpace(gfx::ColorSpace::PrimaryID::SMPTE170M,
                    gfx::ColorSpace::TransferID::SMPTE170M));

void CreateTestY16TextureDrawQuad_FromVideoFrame(
    scoped_refptr<media::VideoFrame> video_frame,
    const gfx::Transform& transform,
    int sorting_context_id,
    CompositorRenderPass* render_pass,
    media::VideoResourceUpdater* video_resource_updater,
    const gfx::Rect& rect,
    const gfx::Rect& visible_rect) {
  bool contents_opaque = false;
  float draw_opacity = 1.0f;

  // Obtain frame resources and perform AppendQuads which chooses the correct
  // quad to append to.
  video_resource_updater->ObtainFrameResource(video_frame);
  video_resource_updater->AppendQuad(render_pass, video_frame, transform, rect,
                                     visible_rect, gfx::MaskFilterInfo(),
                                     /*clip_rect=*/std::nullopt,
                                     contents_opaque, draw_opacity,
                                     sorting_context_id);
}

void CreateTestY16TextureDrawQuad_TwoColor(
    const gfx::Transform& transform,
    int sorting_context_id,
    uint8_t g_foreground,
    uint8_t g_background,
    CompositorRenderPass* render_pass,
    media::VideoResourceUpdater* video_resource_updater,
    const gfx::Rect& rect,
    const gfx::Rect& visible_rect,
    const gfx::Rect& foreground_rect) {
  base::AlignedHeapArray<uint8_t> memory = base::AlignedUninit<uint8_t>(
      rect.size().GetArea() * 2, media::VideoFrame::kFrameAddressAlignment);
  const gfx::Rect video_visible_rect = gfx::Rect(rect.width(), rect.height());
  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::WrapExternalData(
          media::PIXEL_FORMAT_Y16, rect.size(), video_visible_rect,
          visible_rect.size(), memory, base::TimeDelta());
  DCHECK_EQ(video_frame->rows(0) % 2, 0);
  DCHECK_EQ(video_frame->stride(0) % 2, 0ul);

  for (int j = 0; j < video_frame->rows(0); ++j) {
    uint8_t* row =
        UNSAFE_TODO(video_frame->writable_data(0) + j * video_frame->stride(0));
    if (j < foreground_rect.y() || j >= foreground_rect.bottom()) {
      for (size_t i = 0; i < video_frame->stride(0) / 2; ++i) {
        *UNSAFE_TODO(row++) =
            i & 0xFF;  // Fill R with anything. It is not rendered.
        *UNSAFE_TODO(row++) = g_background;
      }
    } else {
      for (size_t i = 0; i < std::min<size_t>(video_frame->stride(0) / 2,
                                              foreground_rect.x());
           ++i) {
        *UNSAFE_TODO(row++) = i & 0xFF;
        *UNSAFE_TODO(row++) = g_background;
      }
      for (size_t i = foreground_rect.x();
           i < std::min<size_t>(video_frame->stride(0) / 2,
                                foreground_rect.right());
           ++i) {
        *UNSAFE_TODO(row++) = i & 0xFF;
        *UNSAFE_TODO(row++) = g_foreground;
      }
      for (size_t i = foreground_rect.right(); i < video_frame->stride(0) / 2;
           ++i) {
        *UNSAFE_TODO(row++) = i & 0xFF;
        *UNSAFE_TODO(row++) = g_background;
      }
    }
  }

  CreateTestY16TextureDrawQuad_FromVideoFrame(
      video_frame, transform, sorting_context_id, render_pass,
      video_resource_updater, rect, visible_rect);
}

void CreateTestMultiplanarVideoDrawQuad(
    scoped_refptr<media::VideoFrame> video_frame,
    uint8_t alpha_value,
    gfx::Transform transform,
    gfx::MaskFilterInfo mask_filter_info,
    int sorting_context_id,
    CompositorRenderPass* render_pass,
    media::VideoResourceUpdater* video_resource_updater,
    const gfx::Rect& rect,
    const gfx::Rect& visible_rect) {
  DCHECK(video_frame->ColorSpace().IsValid());

  float draw_opacity = 1.0f;
  const bool with_alpha = (video_frame->format() == media::PIXEL_FORMAT_I420A);
  if (with_alpha) {
    UNSAFE_TODO(memset(video_frame->writable_data(media::VideoFrame::Plane::kA),
                       alpha_value,
                       video_frame->stride(media::VideoFrame::Plane::kA) *
                           video_frame->rows(media::VideoFrame::Plane::kA)));
  } else {
    EXPECT_EQ(alpha_value, 255);
  }

  // Obtain frame resources and perform AppendQuads which chooses the correct
  // quad to append to.
  video_resource_updater->ObtainFrameResource(video_frame);
  video_resource_updater->AppendQuad(
      render_pass, video_frame, transform, rect, visible_rect, mask_filter_info,
      /*clip_rect=*/std::nullopt, /*context_opaque=*/with_alpha, draw_opacity,
      sorting_context_id);
}

// A unit square at the origin to indicate full tex coordinate coverage.
constexpr gfx::RectF kUnitSquare(0.f, 0.f, 1.f, 1.f);

class TestVideoFrameBuilder {
 public:
  TestVideoFrameBuilder() = delete;

  // Create an empty video frame with common parameters.
  TestVideoFrameBuilder(media::VideoPixelFormat format,
                        const gfx::ColorSpace& color_space,
                        const gfx::RectF& tex_coord_rect,
                        const gfx::Size& coded_size,
                        const gfx::Size& natural_size) {
    const gfx::Rect video_visible_rect = gfx::ToNearestRect(
        gfx::RectF(tex_coord_rect.x() * coded_size.width(),
                   tex_coord_rect.y() * coded_size.height(),
                   tex_coord_rect.width() * coded_size.width(),
                   tex_coord_rect.height() * coded_size.height()));
    video_frame_ =
        media::VideoFrame::CreateFrame(format, coded_size, video_visible_rect,
                                       natural_size, base::TimeDelta());
    video_frame_->set_color_space(color_space);
  }

  scoped_refptr<media::VideoFrame> DrawStriped() {
    // YUV values representing a striped pattern, for validating texture
    // coordinates for sampling.
    uint8_t y_value = 0;
    uint8_t u_value = 0;
    uint8_t v_value = 0;
    for (int i = 0; i < video_frame_->rows(media::VideoFrame::Plane::kY); ++i) {
      uint8_t* y_row = UNSAFE_TODO(
          video_frame_->writable_data(media::VideoFrame::Plane::kY) +
          video_frame_->stride(media::VideoFrame::Plane::kY) * i);
      for (int j = 0; j < video_frame_->row_bytes(media::VideoFrame::Plane::kY);
           ++j) {
        UNSAFE_TODO(y_row[j]) = (y_value += 1);
      }
    }
    for (int i = 0; i < video_frame_->rows(media::VideoFrame::Plane::kU); ++i) {
      uint8_t* u_row = UNSAFE_TODO(
          video_frame_->writable_data(media::VideoFrame::Plane::kU) +
          video_frame_->stride(media::VideoFrame::Plane::kU) * i);
      uint8_t* v_row = UNSAFE_TODO(
          video_frame_->writable_data(media::VideoFrame::Plane::kV) +
          video_frame_->stride(media::VideoFrame::Plane::kV) * i);
      for (int j = 0; j < video_frame_->row_bytes(media::VideoFrame::Plane::kU);
           ++j) {
        UNSAFE_TODO(u_row[j]) = (u_value += 3);
        UNSAFE_TODO(v_row[j]) = (v_value += 5);
      }
    }

    return std::move(video_frame_);
  }

  // Creates a video frame of size background_size filled with yuv_background,
  // and then draws a foreground rectangle in a different color on top of
  // that. The foreground rectangle must have coordinates that are divisible
  // by 2 because YUV is a block format.
  scoped_refptr<media::VideoFrame> DrawTwoColor(
      uint8_t y_background,
      uint8_t u_background,
      uint8_t v_background,
      const gfx::Rect& foreground_rect,
      uint8_t y_foreground,
      uint8_t u_foreground,
      uint8_t v_foreground) {
    auto planes = std::to_array<int>({
        media::VideoFrame::Plane::kY,
        media::VideoFrame::Plane::kU,
        media::VideoFrame::Plane::kV,
    });
    auto yuv_background = std::to_array<uint8_t>({
        y_background,
        u_background,
        v_background,
    });
    auto yuv_foreground = std::to_array<uint8_t>({
        y_foreground,
        u_foreground,
        v_foreground,
    });
    auto sample_size = std::to_array<int>({1, 2, 2});

    for (int i = 0; i < 3; ++i) {
      UNSAFE_TODO(memset(
          video_frame_->writable_data(planes[i]), yuv_background[i],
          video_frame_->stride(planes[i]) * video_frame_->rows(planes[i])));
    }

    for (int i = 0; i < 3; ++i) {
      // Since yuv encoding uses block encoding, widths have to be divisible
      // by the sample size in order for this function to behave properly.
      DCHECK_EQ(foreground_rect.x() % sample_size[i], 0);
      DCHECK_EQ(foreground_rect.y() % sample_size[i], 0);
      DCHECK_EQ(foreground_rect.width() % sample_size[i], 0);
      DCHECK_EQ(foreground_rect.height() % sample_size[i], 0);

      gfx::Rect sample_rect(foreground_rect.x() / sample_size[i],
                            foreground_rect.y() / sample_size[i],
                            foreground_rect.width() / sample_size[i],
                            foreground_rect.height() / sample_size[i]);
      for (int y = sample_rect.y(); y < sample_rect.bottom(); ++y) {
        for (int x = sample_rect.x(); x < sample_rect.right(); ++x) {
          size_t offset = y * video_frame_->stride(planes[i]) + x;
          UNSAFE_TODO(video_frame_->writable_data(planes[i])[offset]) =
              yuv_foreground[i];
        }
      }
    }

    return std::move(video_frame_);
  }

  scoped_refptr<media::VideoFrame> DrawSolid(uint8_t y, uint8_t u, uint8_t v) {
    // YUV values of a solid, constant, color. Useful for testing that color
    // space/color range are being handled properly.
    UNSAFE_TODO(
        memset(video_frame_->writable_data(media::VideoFrame::Plane::kY), y,
               video_frame_->stride(media::VideoFrame::Plane::kY) *
                   video_frame_->rows(media::VideoFrame::Plane::kY)));
    if (video_frame_->format() == media::PIXEL_FORMAT_NV12) {
      const int stride_uv = video_frame_->stride(media::VideoFrame::Plane::kUV);
      const int half_height = (video_frame_->coded_size().height() + 1) / 2;
      uint8_t* uv_plane =
          video_frame_->writable_data(media::VideoFrame::Plane::kUV);
      // Set U and V.
      for (int row = 0; row < half_height; ++row) {
        for (int col = 0; col < stride_uv; col++) {
          *uv_plane = col % 2 == 0 ? u : v;
          UNSAFE_TODO(uv_plane++);
        }
      }
    } else {
      // Only NV12, YV12 and I420 formats are used for testing here.
      CHECK(video_frame_->format() == media::PIXEL_FORMAT_I420 ||
            video_frame_->format() == media::PIXEL_FORMAT_YV12);
      UNSAFE_TODO(
          memset(video_frame_->writable_data(media::VideoFrame::Plane::kU), u,
                 video_frame_->stride(media::VideoFrame::Plane::kU) *
                     video_frame_->rows(media::VideoFrame::Plane::kU)));
      UNSAFE_TODO(
          memset(video_frame_->writable_data(media::VideoFrame::Plane::kV), v,
                 video_frame_->stride(media::VideoFrame::Plane::kV) *
                     video_frame_->rows(media::VideoFrame::Plane::kV)));
    }

    return std::move(video_frame_);
  }

 private:
  scoped_refptr<media::VideoFrame> video_frame_;
};

class IntersectingMultiplanarVideoQuadPixelTest : public VizPixelTestWithParam {
 public:
  void SetUp() override {
    VizPixelTestWithParam::SetUp();
    constexpr bool kUseGpuMemoryBufferResources = false;
    constexpr int kMaxResourceSize = 10000;

    video_resource_updater_ = std::make_unique<media::VideoResourceUpdater>(
        this->child_context_provider_.get(),
        this->child_resource_provider_.get(),
        /*shared_image_interface=*/nullptr, kUseGpuMemoryBufferResources,
        kMaxResourceSize);
    video_resource_updater2_ = std::make_unique<media::VideoResourceUpdater>(
        this->child_context_provider_.get(),
        this->child_resource_provider_.get(),
        /*shared_image_interface=*/nullptr, kUseGpuMemoryBufferResources,
        kMaxResourceSize);
  }

  void TearDown() override {
    video_resource_updater_.reset();
    video_resource_updater2_.reset();
    VizPixelTest::TearDown();
  }

 protected:
  void SetupQuadStateTransformsAndRenderPass() {
    // This sets up transforms for a pair of draw quads created by
    // VideoResourceUpdater. They are both rotated relative to the root plane,
    // they are also rotated relative to each other. The intersect in the middle
    // at a non-perpendicular angle so that any errors are hopefully magnified.
    // The quads should intersect correctly, as in the front quad should only
    // be partially in front of the back quad, and partially behind.

    viewport_rect_ = gfx::Rect(this->device_viewport_size_);
    quad_rect_ = gfx::Rect(0, 0, this->device_viewport_size_.width(),
                           this->device_viewport_size_.height() / 2.0);

    CompositorRenderPassId id{1};
    render_pass_ = CreateTestRootRenderPass(id, viewport_rect_);

    // Create the transform for front quad rotated on the Z and Y axis.
    transform_.Translate3d(0, 0,
                           0.707 * this->device_viewport_size_.width() / 2.0);
    transform_.RotateAboutZAxis(45.0);
    transform_.RotateAboutYAxis(45.0);

    // Create the transform for back quad, and rotate on just the y axis. This
    // will intersect the first quad partially.
    transform2_.Translate3d(0, 0,
                            -0.707 * this->device_viewport_size_.width() / 2.0);
    transform2_.RotateAboutYAxis(-45.0);
  }
  void AppendBackgroundAndRunTest(const cc::PixelComparator& comparator,
                                  const base::FilePath& ref_file) {
    SharedQuadState* background_quad_state =
        CreateTestSharedQuadState(gfx::Transform(), viewport_rect_,
                                  render_pass_.get(), gfx::MaskFilterInfo());
    auto* background_quad =
        render_pass_->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    background_quad->SetNew(background_quad_state, viewport_rect_,
                            viewport_rect_, SkColors::kWhite, false);

    AggregatedRenderPassId new_id{1};
    auto copy_pass = cc::CopyToAggregatedRenderPass(
        render_pass_.get(), new_id, gfx::ContentColorUsage::kSRGB,
        this->resource_provider_.get(), this->child_resource_provider_.get(),
        this->child_context_provider_.get());
    pass_list_.push_back(std::move(copy_pass));
    EXPECT_TRUE(this->RunPixelTest(&pass_list_, ref_file, comparator));
  }
  template <typename T>
  T* CreateAndAppendDrawQuad() {
    return render_pass_->CreateAndAppendDrawQuad<T>();
  }

  std::unique_ptr<CompositorRenderPass> render_pass_;
  gfx::Rect viewport_rect_;
  gfx::Rect quad_rect_;
  AggregatedRenderPassList pass_list_;
  gfx::Transform transform_;
  gfx::Transform transform2_;
  std::unique_ptr<media::VideoResourceUpdater> video_resource_updater_;
  std::unique_ptr<media::VideoResourceUpdater> video_resource_updater2_;

  // Make sure they end up in a 3d sorting context.
  const int sorting_context_id_ = 1;
};

INSTANTIATE_TEST_SUITE_P(,
                         IntersectingMultiplanarVideoQuadPixelTest,
                         testing::ValuesIn(GetGpuRendererTypes()),
                         testing::PrintToStringParamName());

// GetGpuRendererTypes() can return an empty list, e.g. on Fuchsia ARM64.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    IntersectingMultiplanarVideoQuadPixelTest);

TEST_P(IntersectingMultiplanarVideoQuadPixelTest, YUVVideoQuads) {
  this->SetupQuadStateTransformsAndRenderPass();
  gfx::Rect inner_rect(
      ((this->quad_rect_.x() + (this->quad_rect_.width() / 4)) & ~0xF),
      ((this->quad_rect_.y() + (this->quad_rect_.height() / 4)) & ~0xF),
      (this->quad_rect_.width() / 2) & ~0xF,
      (this->quad_rect_.height() / 2) & ~0xF);

  CreateTestMultiplanarVideoDrawQuad(
      TestVideoFrameBuilder(media::PIXEL_FORMAT_I420,
                            gfx::ColorSpace::CreateJpeg(), kUnitSquare,
                            this->quad_rect_.size(), this->quad_rect_.size())
          .DrawTwoColor(0, 128, 128, inner_rect, 29, 255, 107),
      /*alpha_value=*/255, transform_, gfx::MaskFilterInfo(),
      sorting_context_id_, this->render_pass_.get(),
      this->video_resource_updater_.get(), this->quad_rect_, this->quad_rect_);

  CreateTestMultiplanarVideoDrawQuad(
      TestVideoFrameBuilder(media::PIXEL_FORMAT_I420,
                            gfx::ColorSpace::CreateJpeg(), kUnitSquare,
                            this->quad_rect_.size(), this->quad_rect_.size())
          .DrawTwoColor(149, 43, 21, inner_rect, 0, 128, 128),
      /*alpha_value=*/255, transform2_, gfx::MaskFilterInfo(),
      sorting_context_id_, this->render_pass_.get(),
      this->video_resource_updater_.get(), this->quad_rect_, this->quad_rect_);

  base::FilePath baseline = base::FilePath(
      FILE_PATH_LITERAL("intersecting_blue_green_squares_video.png"));

  if (is_skia_graphite()) {
    baseline = baseline.InsertBeforeExtensionASCII(kGraphiteStr);
  }
  if (renderer_type() == RendererType::kSkiaGL && IsANGLEMetal()) {
    baseline = baseline.InsertBeforeExtensionASCII(kANGLEMetalStr);
  }

  this->AppendBackgroundAndRunTest(cc::FuzzyPixelComparator()
                                       .DiscardAlpha()
                                       .SetErrorPixelsPercentageLimit(0.50f)
                                       .SetAvgAbsErrorLimit(1.2f)
                                       .SetAbsErrorLimit(2),
                                   baseline);
}

TEST_P(IntersectingMultiplanarVideoQuadPixelTest, Y16VideoQuads) {
  this->SetupQuadStateTransformsAndRenderPass();
  gfx::Rect inner_rect(
      ((this->quad_rect_.x() + (this->quad_rect_.width() / 4)) & ~0xF),
      ((this->quad_rect_.y() + (this->quad_rect_.height() / 4)) & ~0xF),
      (this->quad_rect_.width() / 2) & ~0xF,
      (this->quad_rect_.height() / 2) & ~0xF);

  CreateTestY16TextureDrawQuad_TwoColor(
      transform_, sorting_context_id_, 18, 0, this->render_pass_.get(),
      this->video_resource_updater_.get(), this->quad_rect_, this->quad_rect_,
      inner_rect);

  CreateTestY16TextureDrawQuad_TwoColor(
      transform2_, sorting_context_id_, 0, 182, this->render_pass_.get(),
      this->video_resource_updater2_.get(), this->quad_rect_, this->quad_rect_,
      inner_rect);

  base::FilePath baseline = base::FilePath(
      FILE_PATH_LITERAL("intersecting_light_dark_squares_video.png"));

  if (is_skia_graphite()) {
    baseline = baseline.InsertBeforeExtensionASCII(kGraphiteStr);
  }
  if (renderer_type() == RendererType::kSkiaGL && IsANGLEMetal()) {
    baseline = baseline.InsertBeforeExtensionASCII(kANGLEMetalStr);
  }

  this->AppendBackgroundAndRunTest(cc::FuzzyPixelOffByOneComparator(),
                                   baseline);
}

class VideoRendererPixelTestBase : public VizPixelTest {
 public:
  explicit VideoRendererPixelTestBase(RendererType type) : VizPixelTest(type) {}

 protected:
  // Include the protected member variables from the parent class.
  using cc::PixelTest::child_context_provider_;
  using cc::PixelTest::child_resource_provider_;
  using cc::PixelTest::resource_provider_;

  void CreateEdgeBleedPass(media::VideoPixelFormat format,
                           const gfx::ColorSpace& color_space,
                           AggregatedRenderPassList* pass_list) {
    gfx::Rect rect(200, 200);

    CompositorRenderPassId id{1};
    auto pass = CreateTestRootRenderPass(id, rect);

    // Scale the video up so that bilinear filtering kicks in to sample more
    // than just nearest neighbor would.
    gfx::Transform scale_by_2;
    scale_by_2.Scale(2.f, 2.f);

    gfx::Size background_size(200, 200);
    gfx::Rect green_rect(16, 20, 100, 100);
    gfx::RectF tex_coord_rect(
        static_cast<float>(green_rect.x()) / background_size.width(),
        static_cast<float>(green_rect.y()) / background_size.height(),
        static_cast<float>(green_rect.width()) / background_size.width(),
        static_cast<float>(green_rect.height()) / background_size.height());

    // YUV of (149,43,21) should be green (0,255,0) in RGB.
    // Create a video frame that has a non-green background rect, with a
    // green sub-rectangle that should be the only thing displayed in
    // the final image.  Bleeding will appear on all four sides of the video
    // if the tex coords are not clamped.
    CreateTestMultiplanarVideoDrawQuad(
        TestVideoFrameBuilder(format, color_space, tex_coord_rect,
                              background_size, background_size)
            .DrawTwoColor(128, 128, 128, green_rect, 149, 43, 21),
        /*alpha_value=*/255, scale_by_2, gfx::MaskFilterInfo(),
        /*sorting_context_id=*/0, pass.get(),
        this->video_resource_updater_.get(), gfx::Rect(background_size),
        gfx::Rect(background_size));

    AggregatedRenderPassId new_id{1};
    auto copy_pass = cc::CopyToAggregatedRenderPass(
        pass.get(), new_id, gfx::ContentColorUsage::kSRGB,
        this->resource_provider_.get(), this->child_resource_provider_.get(),
        this->child_context_provider_.get());
    pass_list->push_back(std::move(copy_pass));
  }

  void SetUp() override {
    VizPixelTest::SetUp();
    constexpr bool kUseGpuMemoryBufferResources = false;
    constexpr int kMaxResourceSize = 10000;
    video_resource_updater_ = std::make_unique<media::VideoResourceUpdater>(
        child_context_provider_.get(), child_resource_provider_.get(),
        /*shared_image_interface=*/nullptr, kUseGpuMemoryBufferResources,
        kMaxResourceSize);
  }

  void TearDown() override {
    video_resource_updater_ = nullptr;
    VizPixelTest::TearDown();
  }

  std::unique_ptr<media::VideoResourceUpdater> video_resource_updater_;
};

#if BUILDFLAG(ENABLE_GL_BACKEND_TESTS)
// Upshift video frame to 10 bit.
scoped_refptr<media::VideoFrame> CreateHighbitVideoFrame(
    media::VideoFrame* video_frame) {
  media::VideoPixelFormat format;
  switch (video_frame->format()) {
    case media::PIXEL_FORMAT_I420:
      format = media::PIXEL_FORMAT_YUV420P10;
      break;
    case media::PIXEL_FORMAT_I422:
      format = media::PIXEL_FORMAT_YUV422P10;
      break;
    case media::PIXEL_FORMAT_I444:
      format = media::PIXEL_FORMAT_YUV444P10;
      break;

    default:
      NOTREACHED();
  }
  scoped_refptr<media::VideoFrame> ret = media::VideoFrame::CreateFrame(
      format, video_frame->coded_size(), video_frame->visible_rect(),
      video_frame->natural_size(), video_frame->timestamp());
  ret->set_color_space(video_frame->ColorSpace());

  // Copy all metadata.
  ret->metadata().MergeMetadataFrom(video_frame->metadata());

  for (int plane = media::VideoFrame::Plane::kY;
       plane <= media::VideoFrame::Plane::kV; ++plane) {
    int width = video_frame->row_bytes(plane);
    const uint8_t* src = video_frame->data(plane);
    uint16_t* dst = reinterpret_cast<uint16_t*>(ret->writable_data(plane));
    for (int row = 0; row < video_frame->rows(plane); row++) {
      for (int x = 0; x < width; x++) {
        // Replicate the top bits into the lower bits, this way
        // 0xFF becomes 0x3FF.
        UNSAFE_TODO(dst[x]) =
            (UNSAFE_TODO(src[x]) << 2) | (UNSAFE_TODO(src[x]) >> 6);
      }
      UNSAFE_TODO(src += video_frame->stride(plane));
      UNSAFE_TODO(dst += ret->stride(plane) / 2);
    }
  }
  return ret;
}

class VideoRendererPixelHiLoTest : public VideoRendererPixelTestBase,
                                   public testing::WithParamInterface<bool> {
 public:
  VideoRendererPixelHiLoTest()
      : VideoRendererPixelTestBase(RendererType::kSkiaGL) {}

  bool IsHighbit() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(, VideoRendererPixelHiLoTest, testing::Bool());

TEST_P(VideoRendererPixelHiLoTest, SimpleYUVRect) {
  gfx::Rect rect(this->device_viewport_size_);

  CompositorRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);
  // Set the output color space to match the input primaries and transfer.
  this->display_color_spaces_ = kRec601DisplayColorSpaces;

  auto video_frame =
      TestVideoFrameBuilder(media::PIXEL_FORMAT_I420,
                            gfx::ColorSpace::CreateREC601(), kUnitSquare,
                            rect.size(), rect.size())
          .DrawStriped();
  if (IsHighbit()) {
    video_frame = CreateHighbitVideoFrame(video_frame.get());
  }
  CreateTestMultiplanarVideoDrawQuad(
      std::move(video_frame), /*alpha_value=*/255, gfx::Transform(),
      gfx::MaskFilterInfo(),
      /*sorting_context_id=*/0, pass.get(), this->video_resource_updater_.get(),
      rect, rect);

  AggregatedRenderPassId new_id{1};
  auto copy_pass = cc::CopyToAggregatedRenderPass(
      pass.get(), new_id, gfx::ContentColorUsage::kSRGB,
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(copy_pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("yuv_stripes.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

class VideoRendererPixelHiLoColorSpaceTest
    : public VideoRendererPixelTestBase,
      public testing::WithParamInterface<std::tuple<bool, gfx::ColorSpace>> {
 public:
  VideoRendererPixelHiLoColorSpaceTest()
      : VideoRendererPixelTestBase(RendererType::kSkiaGL) {}

  bool IsHighbit() const { return std::get<0>(GetParam()); }
  gfx::ColorSpace GetColorSpace() const { return std::get<1>(GetParam()); }
  const std::string GetName() const {
    auto cs = GetColorSpace();
    switch (cs.GetMatrixID()) {
      case gfx::ColorSpace::MatrixID::FCC:
        return "_fcc_limited";
      case gfx::ColorSpace::MatrixID::YCOCG:
        return "_ycocg_limited";
      case gfx::ColorSpace::MatrixID::SMPTE240M:
        return "_smpte240m_limited";
      case gfx::ColorSpace::MatrixID::YDZDX:
        return "_ydzdx_limited";
      case gfx::ColorSpace::MatrixID::GBR:
        return "_gbr_limited";
      default:
        NOTREACHED();
    }
  }
};

gfx::ColorSpace yuv_color_spaces[] = {
    gfx::ColorSpace(gfx::ColorSpace::PrimaryID::SMPTE170M,
                    gfx::ColorSpace::TransferID::SMPTE170M,
                    gfx::ColorSpace::MatrixID::YCOCG,
                    gfx::ColorSpace::RangeID::LIMITED),
    gfx::ColorSpace(gfx::ColorSpace::PrimaryID::SMPTE170M,
                    gfx::ColorSpace::TransferID::SMPTE170M,
                    gfx::ColorSpace::MatrixID::FCC,
                    gfx::ColorSpace::RangeID::LIMITED),
    gfx::ColorSpace(gfx::ColorSpace::PrimaryID::SMPTE170M,
                    gfx::ColorSpace::TransferID::SMPTE170M,
                    gfx::ColorSpace::MatrixID::SMPTE240M,
                    gfx::ColorSpace::RangeID::LIMITED),
    gfx::ColorSpace(gfx::ColorSpace::PrimaryID::SMPTE170M,
                    gfx::ColorSpace::TransferID::SMPTE170M,
                    gfx::ColorSpace::MatrixID::YDZDX,
                    gfx::ColorSpace::RangeID::LIMITED),
    gfx::ColorSpace(gfx::ColorSpace::PrimaryID::SMPTE170M,
                    gfx::ColorSpace::TransferID::SMPTE170M,
                    gfx::ColorSpace::MatrixID::GBR,
                    gfx::ColorSpace::RangeID::LIMITED),
};

INSTANTIATE_TEST_SUITE_P(,
                         VideoRendererPixelHiLoColorSpaceTest,
                         testing::Combine(testing::Bool(),
                                          testing::ValuesIn(yuv_color_spaces)));

TEST_P(VideoRendererPixelHiLoColorSpaceTest, SimpleYUVRect) {
  gfx::Rect rect(this->device_viewport_size_);

  CompositorRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);
  // Set the output color space to match the input primaries and transfer.
  this->display_color_spaces_ = kRec601DisplayColorSpaces;

  scoped_refptr<media::VideoFrame> video_frame =
      TestVideoFrameBuilder(media::PIXEL_FORMAT_I420, GetColorSpace(),
                            kUnitSquare, rect.size(), rect.size())
          .DrawStriped();
  if (IsHighbit()) {
    video_frame = CreateHighbitVideoFrame(video_frame.get());
  }
  CreateTestMultiplanarVideoDrawQuad(
      std::move(video_frame), /*alpha_value=*/255, gfx::Transform(),
      gfx::MaskFilterInfo(),
      /*sorting_context_id=*/0, pass.get(), this->video_resource_updater_.get(),
      rect, rect);

  AggregatedRenderPassId new_id{1};
  auto copy_pass = cc::CopyToAggregatedRenderPass(
      pass.get(), new_id, gfx::ContentColorUsage::kSRGB,
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(copy_pass));

  base::FilePath expected_result =
      base::FilePath(FILE_PATH_LITERAL("yuv_stripes.png"));
  expected_result = expected_result.InsertBeforeExtensionASCII(GetName());
  // YCgCo color space supports highbit formats.
  if (IsHighbit() &&
      GetColorSpace().GetMatrixID() == gfx::ColorSpace::MatrixID::YCOCG) {
    expected_result = expected_result.InsertBeforeExtensionASCII("_highbit");
  }

  EXPECT_TRUE(
      this->RunPixelTest(&pass_list, expected_result,
                         cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

#if BUILDFLAG(IS_IOS)
// TODO(crbug.com/40259140): currently failing on iOS.
#define MAYBE_ClippedYUVRect DISABLED_ClippedYUVRect
#else
#define MAYBE_ClippedYUVRect ClippedYUVRect
#endif  // BUILDFLAG(IS_IOS)
TEST_P(VideoRendererPixelHiLoTest, MAYBE_ClippedYUVRect) {
  gfx::Rect viewport(this->device_viewport_size_);
  gfx::Rect draw_rect(this->device_viewport_size_.width() * 1.5,
                      this->device_viewport_size_.height() * 1.5);

  CompositorRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, viewport);
  // Set the output color space to match the input primaries and transfer.
  this->display_color_spaces_ = kRec601DisplayColorSpaces;

  scoped_refptr<media::VideoFrame> video_frame =
      TestVideoFrameBuilder(media::PIXEL_FORMAT_I420,
                            gfx::ColorSpace::CreateREC601(), kUnitSquare,
                            draw_rect.size(), viewport.size())
          .DrawStriped();
  if (IsHighbit()) {
    video_frame = CreateHighbitVideoFrame(video_frame.get());
  }
  CreateTestMultiplanarVideoDrawQuad(
      std::move(video_frame), /*alpha_value=*/255, gfx::Transform(),
      gfx::MaskFilterInfo(),
      /*sorting_context_id=*/0, pass.get(), this->video_resource_updater_.get(),
      draw_rect, viewport);

  AggregatedRenderPassId new_id{1};
  auto copy_pass = cc::CopyToAggregatedRenderPass(
      pass.get(), new_id, gfx::ContentColorUsage::kSRGB,
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(copy_pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("yuv_stripes_clipped.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}
#endif  // #if BUILDFLAG(ENABLE_GL_BACKEND_TESTS)

class VideoRendererPixelTest
    : public VideoRendererPixelTestBase,
      public testing::WithParamInterface<RendererType> {
 public:
  VideoRendererPixelTest() : VideoRendererPixelTestBase(GetParam()) {}
};

INSTANTIATE_TEST_SUITE_P(,
                         VideoRendererPixelTest,
                         // TODO(crbug.com/40106226): Enable these tests for
                         // SkiaRenderer Dawn once video is supported.
                         testing::ValuesIn(GetGpuRendererTypes()),
                         testing::PrintToStringParamName());

// GetGpuRendererTypes() can return an empty list, e.g. on Fuchsia ARM64.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(VideoRendererPixelTest);

TEST_P(VideoRendererPixelTest, OffsetYUVRect) {
  gfx::Rect rect(this->device_viewport_size_);

  CompositorRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);
  // Set the output color space to match the input primaries and transfer.
  this->display_color_spaces_ = kRec601DisplayColorSpaces;

  // Intentionally sets frame format to I420 for testing coverage.
  CreateTestMultiplanarVideoDrawQuad(
      TestVideoFrameBuilder(
          media::PIXEL_FORMAT_I420, gfx::ColorSpace::CreateREC601(),
          gfx::RectF(0.125f, 0.25f, 0.75f, 0.5f), rect.size(), rect.size())
          .DrawStriped(),
      /*alpha_value=*/255, gfx::Transform(), gfx::MaskFilterInfo(),
      /*sorting_context_id=*/0, pass.get(), this->video_resource_updater_.get(),
      rect, rect);

  AggregatedRenderPassId new_id{1};
  auto copy_pass = cc::CopyToAggregatedRenderPass(
      pass.get(), new_id, gfx::ContentColorUsage::kSRGB,
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(copy_pass));

  base::FilePath expected_result =
      base::FilePath(FILE_PATH_LITERAL("yuv_stripes_offset.png"));
  if (is_skia_graphite()) {
    expected_result = expected_result.InsertBeforeExtensionASCII(kGraphiteStr);
  }
  EXPECT_TRUE(
      this->RunPixelTest(&pass_list, expected_result,
                         cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(VideoRendererPixelTest, SimpleYUVRectBlack) {
  gfx::Rect rect(this->device_viewport_size_);

  CompositorRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);
  // Set the output color space to match the input primaries and transfer.
  this->display_color_spaces_ = kRec601DisplayColorSpaces;

  // In MPEG color range YUV values of (15,128,128) should produce black.
  CreateTestMultiplanarVideoDrawQuad(
      TestVideoFrameBuilder(media::PIXEL_FORMAT_I420,
                            gfx::ColorSpace::CreateREC601(), kUnitSquare,
                            rect.size(), rect.size())
          .DrawSolid(15, 128, 128),
      /*alpha_value=*/255, gfx::Transform(), gfx::MaskFilterInfo(),
      /*sorting_context_id=*/0, pass.get(), this->video_resource_updater_.get(),
      rect, rect);

  AggregatedRenderPassId new_id{1};
  auto copy_pass = cc::CopyToAggregatedRenderPass(
      pass.get(), new_id, gfx::ContentColorUsage::kSRGB,
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(copy_pass));

  // If we didn't get black out of the YUV values above, then we probably have a
  // color range issue.
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("black.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(VideoRendererPixelTest, SimpleYUVJRect) {
  gfx::Rect rect(this->device_viewport_size_);

  CompositorRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  // YUV of (149,43,21) should be green (0,255,0) in RGB.
  CreateTestMultiplanarVideoDrawQuad(
      TestVideoFrameBuilder(media::PIXEL_FORMAT_I420,
                            gfx::ColorSpace::CreateJpeg(), kUnitSquare,
                            rect.size(), rect.size())
          .DrawSolid(149, 43, 21),
      /*alpha_value=*/255, gfx::Transform(), gfx::MaskFilterInfo(),
      /*sorting_context_id=*/0, pass.get(), this->video_resource_updater_.get(),
      rect, rect);

  AggregatedRenderPassId new_id{1};
  auto copy_pass = cc::CopyToAggregatedRenderPass(
      pass.get(), new_id, gfx::ContentColorUsage::kSRGB,
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(copy_pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("green.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(VideoRendererPixelTest, SimpleYUVJRectWithYV12) {
  gfx::Rect rect(this->device_viewport_size_);

  CompositorRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  // YUV of (84,114,224) should be crimson red (220,20,60) in RGB.
  CreateTestMultiplanarVideoDrawQuad(
      TestVideoFrameBuilder(media::PIXEL_FORMAT_YV12,
                            gfx::ColorSpace::CreateJpeg(), kUnitSquare,
                            rect.size(), rect.size())
          .DrawSolid(84, 114, 224),
      /*alpha_value=*/255, gfx::Transform(), gfx::MaskFilterInfo(),
      /*sorting_context_id=*/0, pass.get(), this->video_resource_updater_.get(),
      rect, rect);

  AggregatedRenderPassId new_id{1};
  auto copy_pass = cc::CopyToAggregatedRenderPass(
      pass.get(), new_id, gfx::ContentColorUsage::kSRGB,
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(copy_pass));

  SkBitmap ref_bitmap;
  ref_bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(rect.width(), rect.height()));
  ref_bitmap.eraseColor(SkColor4f::FromColor(SkColorSetARGB(255, 220, 20, 60)));

  EXPECT_TRUE(
      this->RunPixelTest(&pass_list, ref_bitmap,
                         cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(VideoRendererPixelTest, SimpleYUVJRectWithTemperature) {
  gfx::Rect rect(this->device_viewport_size_);

  CompositorRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  // YUV of (225,0,148) should be yellow (255,255,0) in RGB.
  CreateTestMultiplanarVideoDrawQuad(
      TestVideoFrameBuilder(media::PIXEL_FORMAT_I420,
                            gfx::ColorSpace::CreateJpeg(), kUnitSquare,
                            rect.size(), rect.size())
          .DrawSolid(225, 0, 148),
      /*alpha_value=*/255, gfx::Transform(), gfx::MaskFilterInfo(),
      /*sorting_context_id=*/0, pass.get(), this->video_resource_updater_.get(),
      rect, rect);

  AggregatedRenderPassId new_id{1};
  auto copy_pass = cc::CopyToAggregatedRenderPass(
      pass.get(), new_id, gfx::ContentColorUsage::kSRGB,
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(copy_pass));

  SkM44 color_matrix;
  color_matrix.setRC(0, 0, 0.7f);
  color_matrix.setRC(1, 1, 0.4f);
  color_matrix.setRC(2, 2, 0.5f);
  this->output_surface_->set_color_matrix(color_matrix);

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("temperature_brown.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(VideoRendererPixelTest, SimpleNV12JRect) {
  gfx::Rect rect(this->device_viewport_size_);

  CompositorRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  // YUV of (149,100,50) should be emerald green (39, 214, 99) in RGB.
  CreateTestMultiplanarVideoDrawQuad(
      TestVideoFrameBuilder(media::PIXEL_FORMAT_NV12,
                            gfx::ColorSpace::CreateJpeg(), kUnitSquare,
                            rect.size(), rect.size())
          .DrawSolid(149, 100, 50),
      /*alpha_value=*/255, gfx::Transform(), gfx::MaskFilterInfo(),
      /*sorting_context_id=*/0, pass.get(), this->video_resource_updater_.get(),
      rect, rect);

  AggregatedRenderPassId new_id{1};
  auto copy_pass = cc::CopyToAggregatedRenderPass(
      pass.get(), new_id, gfx::ContentColorUsage::kSRGB,
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(copy_pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("emerald_green.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

// Test that a YUV video doesn't bleed outside of its tex coords when the
// tex coord rect is only a partial subrectangle of the coded contents.
TEST_P(VideoRendererPixelTest, YUVEdgeBleed) {
  AggregatedRenderPassList pass_list;
  this->CreateEdgeBleedPass(media::PIXEL_FORMAT_I420,
                            gfx::ColorSpace::CreateJpeg(), &pass_list);
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("green.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(VideoRendererPixelTest, YUVAEdgeBleed) {
  AggregatedRenderPassList pass_list;
  this->CreateEdgeBleedPass(media::PIXEL_FORMAT_I420A,
                            gfx::ColorSpace::CreateREC601(), &pass_list);
  // Set the output color space to match the input primaries and transfer.
  this->display_color_spaces_ = kRec601DisplayColorSpaces;
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("green.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(VideoRendererPixelTest, SimpleYUVJRectGrey) {
  gfx::Rect rect(this->device_viewport_size_);

  CompositorRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  // Dark grey in JPEG color range (in MPEG, this is black).
  CreateTestMultiplanarVideoDrawQuad(
      TestVideoFrameBuilder(media::PIXEL_FORMAT_I420,
                            gfx::ColorSpace::CreateJpeg(), kUnitSquare,
                            rect.size(), rect.size())
          .DrawSolid(15, 128, 128),
      /*alpha_value=*/255, gfx::Transform(), gfx::MaskFilterInfo(),
      /*sorting_context_id=*/0, pass.get(), this->video_resource_updater_.get(),
      rect, rect);

  AggregatedRenderPassId new_id{1};
  auto copy_pass = cc::CopyToAggregatedRenderPass(
      pass.get(), new_id, gfx::ContentColorUsage::kSRGB,
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(copy_pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("dark_grey.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(VideoRendererPixelTest, SimpleYUVARect) {
  gfx::Rect rect(this->device_viewport_size_);

  CompositorRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);
  // Set the output color space to match the input primaries and transfer.
  this->display_color_spaces_ = kRec601DisplayColorSpaces;

  CreateTestMultiplanarVideoDrawQuad(
      TestVideoFrameBuilder(media::PIXEL_FORMAT_I420A,
                            gfx::ColorSpace::CreateREC601(), kUnitSquare,
                            rect.size(), rect.size())
          .DrawStriped(),
      /*alpha_value=*/128, gfx::Transform(), gfx::MaskFilterInfo(),
      /*sorting_context_id=*/0, pass.get(), this->video_resource_updater_.get(),
      rect, rect);

  SharedQuadState* shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());
  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_state, rect, rect, SkColors::kWhite, false);

  AggregatedRenderPassId new_id{1};
  auto copy_pass = cc::CopyToAggregatedRenderPass(
      pass.get(), new_id, gfx::ContentColorUsage::kSRGB,
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(copy_pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("yuv_stripes_alpha.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(VideoRendererPixelTest, FullyTransparentYUVARect) {
  gfx::Rect rect(this->device_viewport_size_);

  CompositorRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);
  // Set the output color space to match the input primaries and transfer.
  this->display_color_spaces_ = kRec601DisplayColorSpaces;

  CreateTestMultiplanarVideoDrawQuad(
      TestVideoFrameBuilder(media::PIXEL_FORMAT_I420A,
                            gfx::ColorSpace::CreateREC601(), kUnitSquare,
                            rect.size(), rect.size())
          .DrawStriped(),
      /*alpha_value=*/0, gfx::Transform(), gfx::MaskFilterInfo(),
      /*sorting_context_id=*/0, pass.get(), this->video_resource_updater_.get(),
      rect, rect);

  SharedQuadState* shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());
  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_state, rect, rect, SkColors::kBlack, false);

  AggregatedRenderPassId new_id{1};
  auto copy_pass = cc::CopyToAggregatedRenderPass(
      pass.get(), new_id, gfx::ContentColorUsage::kSRGB,
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(copy_pass));

  EXPECT_TRUE(this->RunPixelTest(&pass_list,
                                 base::FilePath(FILE_PATH_LITERAL("black.png")),
                                 cc::AlphaDiscardingExactPixelComparator()));
}

TEST_P(VideoRendererPixelTest, TwoColorY16Rect) {
  gfx::Rect rect(this->device_viewport_size_);

  CompositorRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  gfx::Rect upper_rect(rect.x(), rect.y(), rect.width(), rect.height() / 2);
  CreateTestY16TextureDrawQuad_TwoColor(
      gfx::Transform(), /*sorting_context_id=*/0, 68, 123, pass.get(),
      this->video_resource_updater_.get(), rect, rect, upper_rect);

  AggregatedRenderPassId new_id{1};
  auto copy_pass = cc::CopyToAggregatedRenderPass(
      pass.get(), new_id, gfx::ContentColorUsage::kSRGB,
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(copy_pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_filter_chain.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

#if BUILDFLAG(IS_WIN)
class VideoPixelRendererPixelTestColorConversion
    : public VideoRendererPixelTestBase,
      public testing::WithParamInterface<RendererType> {
 public:
  VideoPixelRendererPixelTestColorConversion()
      : VideoRendererPixelTestBase(GetParam()) {}

  void SetUp() override {
    // Set a color space that is not suitable for blending to ensure we go
    // through the color conversion code paths.
    this->display_color_spaces_ =
        gfx::DisplayColorSpaces(gfx::ColorSpace::CreateSCRGBLinear80Nits());
    this->display_color_spaces_.SetSDRMaxLuminanceNits(80.f);
    this->display_color_spaces_.SetOutputFormats(SinglePlaneFormat::kRGBA_F16,
                                                 SinglePlaneFormat::kRGBA_F16);

    // Allow non-root render passes to have the above non-suitable-for-blending
    // color space by being scanout.
    renderer_settings_.force_non_scanout_backing_for_pixel_tests = true;

    features_.InitAndEnableFeature(features::kDelegatedCompositing);

    VideoRendererPixelTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList features_;
};

// This checks the correct color conversion is happening in the following case:
// a non-root render pass has a texture quad that requires a color conversion
// filter, but no quads in the render pass require blending.
//
// In this case, the color conversion layer is elided, but we choose the wrong
// "destination" color space for the color conversion filter when we draw the
// texture quad.
// See: crbug.com/397995970
TEST_P(VideoPixelRendererPixelTestColorConversion,
       RenderPassWithHdrVideoDoesntNeedBlending) {
  // Create a test frame that embeds a pass which:
  // - contains a texture quad that requires a color conversion filter and
  // - needs blending, iff `child_pass_needs_blending`.
  auto CreateFrame =
      [&](bool child_pass_needs_blending) -> AggregatedRenderPassList {
    const gfx::Rect rect(this->device_viewport_size_);

    CompositorRenderPassId id{1};
    auto child_pass = CreateTestRootRenderPass(id, rect);

    auto color_space = gfx::ColorSpace(
        gfx::ColorSpace::PrimaryID::BT2020, gfx::ColorSpace::TransferID::PQ,
        gfx::ColorSpace::MatrixID::BT2020_NCL, gfx::ColorSpace::RangeID::FULL);

    CreateTestMultiplanarVideoDrawQuad(
        TestVideoFrameBuilder(media::PIXEL_FORMAT_I420, color_space,
                              kUnitSquare, rect.size(), rect.size())
            .DrawSolid(144, 54, 34),
        /*alpha_value=*/255, gfx::Transform(), gfx::MaskFilterInfo(),
        /*sorting_context_id=*/0, child_pass.get(),
        this->video_resource_updater_.get(), rect, rect);

    AggregatedRenderPassList pass_list;

    AggregatedRenderPassId hdr_child_id{2};
    {
      auto child_pass_copy = cc::CopyToAggregatedRenderPass(
          child_pass.get(), hdr_child_id, gfx::ContentColorUsage::kHDR,
          this->resource_provider_.get(), this->child_resource_provider_.get(),
          this->child_context_provider_.get());

      // Make `is_scanout == true` on Windows for non-root pass.
      // See: `DirectRenderer::CalculateRenderPassRequirements`
      {
        EXPECT_TRUE(
            base::FeatureList::IsEnabled(features::kDelegatedCompositing));
        child_pass_copy->is_from_surface_root_pass = true;
        child_pass_copy->will_backing_be_read_by_viz = false;
      }

      // Make the HDR child render pass not use the color conversion layer if
      // it doesn't contain quads that require blending.
      for (auto* quad : child_pass_copy->quad_list) {
        quad->needs_blending = child_pass_needs_blending;
      }

      pass_list.push_back(std::move(child_pass_copy));
    }

    // Add a root pass that embeds the problematic pass. The root render pass
    // color space is handled specially and we are testing the non-root case.
    {
      AggregatedRenderPassId root_id{1};
      auto root_pass = CreateTestRootRenderPass(root_id, rect);
      root_pass->content_color_usage = gfx::ContentColorUsage::kSRGB;

      SharedQuadState* shared_state = CreateTestSharedQuadState(
          gfx::Transform(), rect, root_pass.get(), gfx::MaskFilterInfo());
      CreateTestRenderPassDrawQuad(shared_state, rect, hdr_child_id,
                                   root_pass.get());

      pass_list.push_back(std::move(root_pass));
    }

    return pass_list;
  };

  // Render the child pass with blending to use as a baseline, since we expect
  // the output to be the same in both cases.
  AggregatedRenderPassList pass_list_with_blending =
      CreateFrame(/*child_pass_needs_blending=*/true);

  AggregatedRenderPassList pass_list_without_blending =
      CreateFrame(/*child_pass_needs_blending=*/false);
  EXPECT_TRUE(this->RunPixelTest(&pass_list_without_blending,
                                 &pass_list_with_blending,
                                 cc::AlphaDiscardingExactPixelComparator()));
}

INSTANTIATE_TEST_SUITE_P(,
                         VideoPixelRendererPixelTestColorConversion,
                         testing::ValuesIn(GetGpuRendererTypes()),
                         testing::PrintToStringParamName());
#endif  // BUILDFLAG(IS_WIN)

}  // namespace
}  // namespace viz
