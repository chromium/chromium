// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/viz/service/frame_sinks/video_capture/video_capture_overlay.h"

#include <array>
#include <optional>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "components/viz/test/paths.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/base/video_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_transform.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

using media::VideoFrame;
using media::VideoPixelFormat;

using testing::_;
using testing::InvokeWithoutArgs;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

namespace viz {
namespace {

class MockFrameSource : public VideoCaptureOverlay::FrameSource {
 public:
  MOCK_METHOD0(GetSourceSize, gfx::Size());
  MOCK_METHOD1(InvalidateRect, void(const gfx::Rect& rect));
  MOCK_METHOD0(RefreshNow, void());
  MOCK_METHOD1(OnOverlayConnectionLost, void(VideoCaptureOverlay* overlay));
};

class VideoCaptureOverlayTest : public testing::Test {
 public:
  VideoCaptureOverlayTest() = default;

  VideoCaptureOverlayTest(const VideoCaptureOverlayTest&) = delete;
  VideoCaptureOverlayTest& operator=(const VideoCaptureOverlayTest&) = delete;

  NiceMock<MockFrameSource>& frame_source() { return frame_source_; }

  std::unique_ptr<VideoCaptureOverlay> CreateOverlay() {
    mojo::Remote<mojom::FrameSinkVideoCaptureOverlay> overlay_remote;
    return std::make_unique<VideoCaptureOverlay>(
        frame_source(), overlay_remote.BindNewPipeAndPassReceiver());
  }

  void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  // Makes a SkBitmap filled with a 50% white background color plus four rects
  // of four different colors/opacities. |cycle| causes the four rects to rotate
  // positions (counter-clockwise by N steps).
  static SkBitmap MakeTestBitmap(int cycle) {
    constexpr gfx::Size kTestImageSize = gfx::Size(24, 16);
    // Test colors have been chosen to exercise different opacities,
    // intensities, and color channels; to confirm all aspects of the "SrcOver"
    // image blending algorithms are working properly.
    constexpr SkColor4f kTestImageBackground =
        SkColor4f{1.0f, 1.0f, 1.0f, 1.0f};
    constexpr SkColor4f kTestImageColors[4] = {
        SkColor4f{1.0f, 0.0f, 0.0f, 0.667f},
        SkColor4f{0.0f, 0.933f, 0.0f, 0.733f},
        SkColor4f{0.0f, 0.0f, 0.467f, 0.8f},
        SkColor4f{0.4f, 0.4f, 0.0f, 0.867f},
    };
    constexpr SkIRect kTestImageColorRects[4] = {
        SkIRect::MakeXYWH(4, 2, 4, 4), SkIRect::MakeXYWH(16, 2, 4, 4),
        SkIRect::MakeXYWH(4, 10, 4, 4), SkIRect::MakeXYWH(16, 10, 4, 4),
    };

    SkBitmap result;
    const SkImageInfo info = SkImageInfo::MakeN32Premul(
        kTestImageSize.width(), kTestImageSize.height(),
        GetLinearSRGB().ToSkColorSpace());
    CHECK(result.tryAllocPixels(info, info.minRowBytes()));
    SkCanvas canvas(result, SkSurfaceProps{});
    canvas.drawColor(kTestImageBackground);
    for (size_t i = 0; i < std::size(kTestImageColors); ++i) {
      const size_t idx = (i + cycle) % std::size(kTestImageColors);
      SkPaint paint;
      paint.setBlendMode(SkBlendMode::kSrc);
      paint.setColor(kTestImageColors[idx], info.colorSpace());
      canvas.drawIRect(kTestImageColorRects[i], paint);
    }

    return result;
  }

  // Returns the sRGB color space, but with a linear transfer function.
  static gfx::ColorSpace GetLinearSRGB() {
    return gfx::ColorSpace(
        gfx::ColorSpace::PrimaryID::BT709, gfx::ColorSpace::TransferID::LINEAR,
        gfx::ColorSpace::MatrixID::RGB, gfx::ColorSpace::RangeID::FULL);
  }

  // Returns the BT709 color space (YUV), but with a linear transfer function.
  static gfx::ColorSpace GetLinearRec709() {
    return gfx::ColorSpace(
        gfx::ColorSpace::PrimaryID::BT709, gfx::ColorSpace::TransferID::LINEAR,
        gfx::ColorSpace::MatrixID::BT709, gfx::ColorSpace::RangeID::LIMITED);
  }

  static constexpr auto kARGBFormat = VideoPixelFormat::PIXEL_FORMAT_ARGB;
  static constexpr auto kI420Format = VideoPixelFormat::PIXEL_FORMAT_I420;

 private:
  NiceMock<MockFrameSource> frame_source_;
};

// Tests that, when the VideoCaptureOverlay binds to a mojo pending receiver, it
// reports when the receiver is closed.
TEST_F(VideoCaptureOverlayTest, ReportsLostMojoConnection) {
  mojo::Remote<mojom::FrameSinkVideoCaptureOverlay> overlay_remote;
  VideoCaptureOverlay overlay(frame_source(),
                              overlay_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(overlay_remote);
  RunUntilIdle();  // Propagate mojo tasks.

  EXPECT_CALL(frame_source(), OnOverlayConnectionLost(&overlay));
  overlay_remote.reset();
  RunUntilIdle();  // Propagate mojo tasks.
}

// This type is used for trace logging and needs a valid ToString method.
TEST_F(VideoCaptureOverlayTest, CapturedFramePropertiesAreStringifiable) {
  constexpr gfx::Size kSize = gfx::Size(100, 75);
  VideoCaptureOverlay::CapturedFrameProperties frame_properties{
      .region_properties =
          CapturableFrameSink::RegionProperties{
              .root_render_pass_size = kSize,
              .render_pass_subrect = gfx::Rect(23, 24, 27, 26),
              .transform_to_root = gfx::Transform::MakeTranslation(10, 11)},
      .content_rect = gfx::Rect(10, 20, 40, 30),
      .format = kI420Format};

  EXPECT_STREQ(
      "23,24 27x26 from 100x75 into 10,20 40x30 via transform "
      "[ 1 0 0 10\n  0 1 0 11\n  0 0 1 0\n  0 0 0 1 ]\n, "
      "format PIXEL_FORMAT_I420",
      frame_properties.ToString().c_str());
}

TEST_F(VideoCaptureOverlayTest, BlendInformationIsStringifiable) {
  VideoCaptureOverlay::BlendInformation blend_info{
      gfx::Rect(1, 2, 3, 4), gfx::Rect(5, 6, 7, 8), gfx::Rect(9, 10, 11, 12)};

  EXPECT_STREQ(
      "source_region=1,2 3x4, source_region_scaled=5,6 7x8, "
      "destination_region_content=9,10 11x12",
      blend_info.ToString().c_str());
}

// The CalculateBlendInformation method is tested implicitly through its use
// in the MakeRenderer() method, however it is a public method still and
// warrants its own tests.
TEST_F(VideoCaptureOverlayTest,
       CalculateBlendInformation_ReturnsNulloptIfEmptyProperties) {
  EXPECT_FALSE(CreateOverlay()->CalculateBlendInformation(
      VideoCaptureOverlay::CapturedFrameProperties{}));
}

TEST_F(VideoCaptureOverlayTest,
       CalculateBlendInformation_ReturnsNulloptIfNoImage) {
  std::unique_ptr<VideoCaptureOverlay> overlay = CreateOverlay();
  VideoCaptureOverlay::CapturedFrameProperties frame_properties{
      .region_properties =
          CapturableFrameSink::RegionProperties{
              .root_render_pass_size = gfx::Size(100, 75),
              .render_pass_subrect = gfx::Rect(23, 24, 27, 26),
          },
      .content_rect = gfx::Rect(10, 20, 40, 30),
      .format = kI420Format};

  EXPECT_FALSE(overlay->CalculateBlendInformation(frame_properties));
}

TEST_F(VideoCaptureOverlayTest, CalculateBlendInformation_GoldenCase) {
  std::unique_ptr<VideoCaptureOverlay> overlay = CreateOverlay();
  VideoCaptureOverlay::CapturedFrameProperties frame_properties{
      .region_properties =
          CapturableFrameSink::RegionProperties{
              .root_render_pass_size = gfx::Size(99, 75),
              .render_pass_subrect = gfx::Rect(23, 24, 27, 26),
          },
      .content_rect = gfx::Rect(0, 1, 30, 32),
      .format = kI420Format};

  overlay->SetImageAndBounds(MakeTestBitmap(1), gfx::RectF(.3, .5, .2, .1));

  const std::optional<VideoCaptureOverlay::BlendInformation> blend_info =
      overlay->CalculateBlendInformation(frame_properties);

  ASSERT_TRUE(blend_info);
  // We should use the entire sprite, which is of size 24x16.
  EXPECT_EQ((gfx::Rect{0, 0, 24, 16}), blend_info->source_region);

  // Sprite should be scaled down slightly.
  EXPECT_EQ((gfx::Rect{0, 0, 20, 8}), blend_info->source_region_scaled);

  // And then translated towards the middle of the video frame.
  EXPECT_EQ((gfx::Rect{8, 18, 20, 8}), blend_info->destination_region_content);
}

// Tests that MakeRenderer() does not make a OnceRenderer until the client has
// set the image.
TEST_F(VideoCaptureOverlayTest, DoesNotRenderWithoutImage) {
  constexpr gfx::Size kSize = gfx::Size(100, 75);
  EXPECT_CALL(frame_source(), GetSourceSize()).WillRepeatedly(Return(kSize));
  std::unique_ptr<VideoCaptureOverlay> overlay = CreateOverlay();

  // The overlay does not have an image yet, so the renderer should be null.
  constexpr gfx::Rect kRegionInFrame = gfx::Rect(kSize);
  EXPECT_FALSE(
      overlay->MakeRenderer(VideoCaptureOverlay::CapturedFrameProperties{
          .region_properties =
              CapturableFrameSink::RegionProperties{
                  .root_render_pass_size = kSize,
                  .render_pass_subrect = kRegionInFrame,
              },
          .content_rect = kRegionInFrame,
          .format = kI420Format}));

  // Once an image is set, the renderer should not be null.
  overlay->SetImageAndBounds(MakeTestBitmap(1), gfx::RectF(0, 0, 1, 1));
  EXPECT_TRUE(
      overlay->MakeRenderer(VideoCaptureOverlay::CapturedFrameProperties{
          .region_properties =
              CapturableFrameSink::RegionProperties{
                  .root_render_pass_size = kSize,
                  .render_pass_subrect = kRegionInFrame,
              },
          .content_rect = kRegionInFrame,
          .format = kI420Format}));
}

// Tests that MakeRenderer() does not make a OnceRenderer if the bounds are set
// to something outside the frame's content region.
TEST_F(VideoCaptureOverlayTest, DoesNotRenderIfCompletelyOutOfBounds) {
  constexpr gfx::Size kSize = gfx::Size(100, 75);
  EXPECT_CALL(frame_source(), GetSourceSize()).WillRepeatedly(Return(kSize));
  std::unique_ptr<VideoCaptureOverlay> overlay = CreateOverlay();

  // The overlay does not have an image yet, so the renderer should be null.
  constexpr gfx::Rect kRegionInFrame = gfx::Rect(kSize);
  EXPECT_FALSE(
      overlay->MakeRenderer(VideoCaptureOverlay::CapturedFrameProperties{
          .region_properties =
              CapturableFrameSink::RegionProperties{
                  .root_render_pass_size = kSize,
                  .render_pass_subrect = kRegionInFrame,
              },
          .content_rect = kRegionInFrame,
          .format = kI420Format}));

  // Setting an image, but out-of-bounds, should always result in a null
  // renderer.
  overlay->SetImageAndBounds(MakeTestBitmap(0), gfx::RectF(-1, -1, 1, 1));
  EXPECT_FALSE(
      overlay->MakeRenderer(VideoCaptureOverlay::CapturedFrameProperties{
          .region_properties =
              CapturableFrameSink::RegionProperties{
                  .root_render_pass_size = kSize,
                  .render_pass_subrect = kRegionInFrame,
              },
          .content_rect = kRegionInFrame,
          .format = kI420Format}));
  overlay->SetBounds(gfx::RectF(1, 1, 1, 1));
  EXPECT_FALSE(
      overlay->MakeRenderer(VideoCaptureOverlay::CapturedFrameProperties{
          .region_properties =
              CapturableFrameSink::RegionProperties{
                  .root_render_pass_size = kSize,
                  .render_pass_subrect = kRegionInFrame,
              },
          .content_rect = kRegionInFrame,
          .format = kI420Format}));
  overlay->SetBounds(gfx::RectF(-1, 1, 1, 1));
  EXPECT_FALSE(
      overlay->MakeRenderer(VideoCaptureOverlay::CapturedFrameProperties{
          .region_properties =
              CapturableFrameSink::RegionProperties{
                  .root_render_pass_size = kSize,
                  .render_pass_subrect = kRegionInFrame,
              },
          .content_rect = kRegionInFrame,
          .format = kI420Format}));
  overlay->SetBounds(gfx::RectF(1, -1, 1, 1));
  EXPECT_FALSE(
      overlay->MakeRenderer(VideoCaptureOverlay::CapturedFrameProperties{
          .region_properties =
              CapturableFrameSink::RegionProperties{
                  .root_render_pass_size = kSize,
                  .render_pass_subrect = kRegionInFrame,
              },
          .content_rect = kRegionInFrame,
          .format = kI420Format}));
}

TEST_F(VideoCaptureOverlayTest, DoesNotRenderIfEmptyBlitRect) {
  constexpr gfx::Size kSize = gfx::Size(100, 200);
  constexpr gfx::Rect kFrameRect = gfx::Rect(kSize);
  EXPECT_CALL(frame_source(), GetSourceSize()).WillRepeatedly(Return(kSize));
  std::unique_ptr<VideoCaptureOverlay> overlay = CreateOverlay();

  overlay->SetImageAndBounds(MakeTestBitmap(0), gfx::RectF(1, 1, 1, 1));
  EXPECT_FALSE(
      overlay->MakeRenderer(VideoCaptureOverlay::CapturedFrameProperties{
          .region_properties =
              CapturableFrameSink::RegionProperties{
                  .root_render_pass_size = kSize,
                  .render_pass_subrect = kFrameRect,
              },
          .content_rect = gfx::Rect(0, 0, 50, 100),
          .format = kI420Format}));
}

// Tests that that MakeCombinedRenderer() only makes a OnceRenderer when one or
// more overlays are set to make visible changes to a video frame.
TEST_F(VideoCaptureOverlayTest,
       DoesNotDoCombinedRenderIfNoOverlaysWouldRender) {
  constexpr gfx::Size kSize = gfx::Size(100, 75);
  EXPECT_CALL(frame_source(), GetSourceSize()).WillRepeatedly(Return(kSize));
  const std::unique_ptr<VideoCaptureOverlay> overlay0 = CreateOverlay();
  const std::unique_ptr<VideoCaptureOverlay> overlay1 = CreateOverlay();
  const std::vector<VideoCaptureOverlay*> overlays{overlay0.get(),
                                                   overlay1.get()};

  // Neither overlay has an image yet, so the combined renderer should be null.
  constexpr gfx::Rect kRegionInFrame = gfx::Rect(kSize);
  EXPECT_FALSE(VideoCaptureOverlay::MakeCombinedRenderer(
      overlays, VideoCaptureOverlay::CapturedFrameProperties{
                    .region_properties =
                        CapturableFrameSink::RegionProperties{
                            .root_render_pass_size = kSize,
                            .render_pass_subrect = kRegionInFrame,
                        },
                    .content_rect = kRegionInFrame,
                    .format = kI420Format}));

  // If just the first overlay renders, the combined renderer should not be
  // null.
  overlays[0]->SetImageAndBounds(MakeTestBitmap(0), gfx::RectF(0, 0, 1, 1));
  EXPECT_TRUE(VideoCaptureOverlay::MakeCombinedRenderer(
      overlays, VideoCaptureOverlay::CapturedFrameProperties{
                    .region_properties =
                        CapturableFrameSink::RegionProperties{
                            .root_render_pass_size = kSize,
                            .render_pass_subrect = kRegionInFrame,
                        },
                    .content_rect = kRegionInFrame,
                    .format = kI420Format}));

  // If both overlays render, the combined renderer should not be null.
  overlays[1]->SetImageAndBounds(MakeTestBitmap(1), gfx::RectF(0, 0, 1, 1));
  EXPECT_TRUE(VideoCaptureOverlay::MakeCombinedRenderer(
      overlays, VideoCaptureOverlay::CapturedFrameProperties{
                    .region_properties =
                        CapturableFrameSink::RegionProperties{
                            .root_render_pass_size = kSize,
                            .render_pass_subrect = kRegionInFrame,
                        },
                    .content_rect = kRegionInFrame,
                    .format = kI420Format}));

  // If only the second overlay renders, because the first is hidden, the
  // combined renderer should not be null.
  overlays[0]->SetBounds(gfx::RectF());
  EXPECT_TRUE(VideoCaptureOverlay::MakeCombinedRenderer(
      overlays, VideoCaptureOverlay::CapturedFrameProperties{
                    .region_properties =
                        CapturableFrameSink::RegionProperties{
                            .root_render_pass_size = kSize,
                            .render_pass_subrect = kRegionInFrame,
                        },
                    .content_rect = kRegionInFrame,
                    .format = kI420Format}));

  // Both overlays are hidden, so the combined renderer should be null.
  overlays[1]->SetBounds(gfx::RectF());
  EXPECT_FALSE(VideoCaptureOverlay::MakeCombinedRenderer(
      overlays, VideoCaptureOverlay::CapturedFrameProperties{
                    .region_properties =
                        CapturableFrameSink::RegionProperties{
                            .root_render_pass_size = kSize,
                            .render_pass_subrect = kRegionInFrame,
                        },
                    .content_rect = kRegionInFrame,
                    .format = kI420Format}));
}

class VideoCaptureOverlayRenderTest
    : public VideoCaptureOverlayTest,
      public testing::WithParamInterface<VideoPixelFormat> {
 public:
  VideoCaptureOverlayRenderTest()
      : trace_(__FILE__, __LINE__, VideoPixelFormatToString(pixel_format())) {}

  VideoCaptureOverlayRenderTest(const VideoCaptureOverlayRenderTest&) = delete;
  VideoCaptureOverlayRenderTest& operator=(
      const VideoCaptureOverlayRenderTest&) = delete;

  VideoPixelFormat pixel_format() const { return GetParam(); }

  bool is_argb_test() const {
    return pixel_format() == media::PIXEL_FORMAT_ARGB;
  }

  gfx::ColorSpace GetColorSpace() const {
    // For these tests, we use linear RGB and YUV color spaces. This is because
    // VideoCaptureOverlay does not account for non-linear color spaces when
    // blending. See class notes.
    return is_argb_test() ? GetLinearSRGB() : GetLinearRec709();
  }

  scoped_refptr<VideoFrame> CreateVideoFrame(const gfx::Size& size) const {
    auto frame = VideoFrame::CreateFrame(pixel_format(), size, gfx::Rect(size),
                                         size, base::TimeDelta());

    // Fill the video frame with black. For ARGB tests, also set alpha channel
    // to 1.0. This allows the expected results of the ARGB tests to be the same
    // as those of the YUV tests, and so only one set of golden files needs to
    // be used.
    if (is_argb_test()) {
      uint8_t* dst = frame->GetWritableVisibleData(VideoFrame::Plane::kARGB);
      const int stride = frame->stride(VideoFrame::Plane::kARGB);
      for (int row = 0; row < size.height(); ++row, dst += stride) {
        uint32_t* const begin = reinterpret_cast<uint32_t*>(dst);
        std::fill(begin, begin + size.width(), UINT32_C(0xff000000));
      }
    } else /* if (!is_argb_test()) */ {
      media::FillYUV(frame.get(), 0x00, 0x80, 0x80);
    }

    frame->set_color_space(GetColorSpace());
    return frame;
  }

  bool FrameMatchesPNG(const VideoFrame& frame, const char* golden_file) {
    const gfx::ColorSpace png_color_space = GetLinearSRGB();
    // Note: Using kUnpremul_SkAlphaType since that is the semantics of
    // PIXEL_FORMAT_ARGB, and converting to kPremul_SkAlphaType before producing
    // the PNG would lose precision for no good reason.
    const SkImageInfo canonical_format = SkImageInfo::Make(
        frame.visible_rect().width(), frame.visible_rect().height(),
        kN32_SkColorType, kUnpremul_SkAlphaType,
        png_color_space.ToSkColorSpace());
    SkBitmap canonical_bitmap;
    CHECK(canonical_bitmap.tryAllocPixels(canonical_format, 0));

    // Populate |canonical_bitmap| with data from the frame. For I420, use
    // gfx::ColorTransform to map back from YUV→RGB.
    switch (frame.format()) {
      case media::PIXEL_FORMAT_ARGB: {
        // Map from the video frame's ARGB format to the canonical
        // representation.
        const SkImageInfo frame_format = SkImageInfo::Make(
            frame.visible_rect().width(), frame.visible_rect().height(),
            kBGRA_8888_SkColorType, kUnpremul_SkAlphaType,
            frame.ColorSpace().ToSkColorSpace());
        canonical_bitmap.writePixels(
            SkPixmap(frame_format, frame.visible_data(VideoFrame::Plane::kARGB),
                     frame.stride(VideoFrame::Plane::kARGB)),
            0, 0);
        break;
      }

      case media::PIXEL_FORMAT_I420: {
        // Map from I420 planar [0,255] (of which only [16,235] is used) values
        // to interleaved [0.0,1.0] values.
        const gfx::Size& size = frame.visible_rect().size();
        std::unique_ptr<gfx::ColorTransform::TriStim[]> colors(
            new gfx::ColorTransform::TriStim[size.GetArea()]);
        int pos = 0;
        for (int row = 0; row < size.height(); ++row) {
          const uint8_t* y = frame.visible_data(VideoFrame::Plane::kY) +
                             (row * frame.stride(VideoFrame::Plane::kY));
          const uint8_t* u = frame.visible_data(VideoFrame::Plane::kU) +
                             ((row / 2) * frame.stride(VideoFrame::Plane::kU));
          const uint8_t* v = frame.visible_data(VideoFrame::Plane::kV) +
                             ((row / 2) * frame.stride(VideoFrame::Plane::kV));
          for (int col = 0; col < size.width(); ++col) {
            colors[pos].SetPoint(y[col] / 255.0f, u[col / 2] / 255.0f,
                                 v[col / 2] / 255.0f);
            ++pos;
          }
        }

        // Execute the YUV→RGB conversion.
        gfx::ColorTransform::NewColorTransform(frame.ColorSpace(),
                                               png_color_space)
            ->Transform(colors.get(), size.GetArea());

        // Map back from interleaved [0.0,1.0] values to intervealed ARGB,
        // setting alpha=100%.
        const auto ToClamped255 = [](float value) -> uint32_t {
          value = (value * 255.0f) + 0.5f /* rounding */;
          return base::saturated_cast<uint8_t>(value);
        };
        pos = 0;
        for (int row = 0; row < size.height(); ++row) {
          uint32_t* out = canonical_bitmap.getAddr32(0, row);
          for (int col = 0; col < size.width(); ++col) {
            out[col] = ((UINT32_C(255) << SK_A32_SHIFT) |
                        (ToClamped255(colors[pos].x()) << SK_R32_SHIFT) |
                        (ToClamped255(colors[pos].y()) << SK_G32_SHIFT) |
                        (ToClamped255(colors[pos].z()) << SK_B32_SHIFT));
            ++pos;
          }
        }

        break;
      }

      default:
        NOTREACHED_IN_MIGRATION();
        return false;
    }

    // Determine the full path to the golden file to compare the results.
    base::FilePath golden_file_path;
    base::PathService::Get(Paths::DIR_TEST_DATA, &golden_file_path);
    golden_file_path =
        golden_file_path.Append(FILE_PATH_LITERAL("video_capture"))
            .Append(base::FilePath::FromUTF8Unsafe(golden_file));

    // If the very-specific command-line switch is present, rewrite the golden
    // file. This is only done when the ARGB test runs, for the reasons outlined
    // in the comments below (regarding FuzzyPixelComparator).
    if (is_argb_test() &&
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            "video-overlay-capture-test-update-golden-files")) {
      LOG(INFO) << "Rewriting golden file: " << golden_file_path.AsUTF8Unsafe();
      cc::WritePNGFile(canonical_bitmap, golden_file_path, false);
    }

    // FuzzyPixelComparator configuration: Allow 100% of pixels to mismatch, but
    // no single pixel component should be different by more than 1/255 (64/255
    // for YUV tests), and the absolute average error must not exceed 1/255
    // (16/255 for YUV tests). The YUV tests allow for more error due to the
    // expected errors introduced by both color space (dynamic range) and format
    // (chroma subsampling) conversion.
    auto comparator = cc::FuzzyPixelComparator()
                          .SetErrorPixelsPercentageLimit(100.0f)
                          .SetAvgAbsErrorLimit(is_argb_test() ? 1.0f : 16.0f)
                          .SetAbsErrorLimit(is_argb_test() ? 1 : 64);
    const bool matches_golden_file =
        cc::MatchesPNGFile(canonical_bitmap, golden_file_path, comparator);
    // If MatchesPNGFile() returned false, it will have LOG(ERROR)'ed the
    // expected versus actual PNG data URLs. So, only do the VLOG(1)'s when
    // MatchesPNGFile() returned true.
    if (matches_golden_file && VLOG_IS_ON(1)) {
      SkBitmap expected;
      if (cc::ReadPNGFile(golden_file_path, &expected)) {
        VLOG(1) << "Expected bitmap: " << cc::GetPNGDataUrl(expected);
      }
      VLOG(1) << "Actual bitmap: " << cc::GetPNGDataUrl(canonical_bitmap);
    }
    return matches_golden_file;
  }

  void ExpectRendersAs(VideoCaptureOverlay::OnceRenderer* renderers,
                       const char* const* expected_files,
                       const std::size_t count,
                       const gfx::Size& video_frame_size) {
    for (std::size_t i = 0; i < count; ++i) {
      auto frame = CreateVideoFrame(video_frame_size);
      CHECK(renderers[i]);
      std::move(renderers[i]).Run(frame.get());
      EXPECT_TRUE(FrameMatchesPNG(*frame, expected_files[i]));
    }
  }

  // The size of the compositor frame sink's Surface.
  static constexpr gfx::Size kSourceSize = gfx::Size(96, 40);

 private:
  testing::ScopedTrace trace_;
};

// static
constexpr gfx::Size VideoCaptureOverlayRenderTest::kSourceSize;

// Basic test: Render an overlay image that covers the entire video frame and is
// not scaled.
TEST_P(VideoCaptureOverlayRenderTest, FullCover_NoScaling) {
  StrictMock<MockFrameSource> frame_source;
  mojo::Remote<mojom::FrameSinkVideoCaptureOverlay> overlay_remote;
  VideoCaptureOverlay overlay(frame_source,
                              overlay_remote.BindNewPipeAndPassReceiver());

  EXPECT_CALL(frame_source, GetSourceSize())
      .WillRepeatedly(Return(kSourceSize));
  EXPECT_CALL(frame_source, InvalidateRect(gfx::Rect())).RetiresOnSaturation();
  EXPECT_CALL(frame_source, InvalidateRect(gfx::Rect(kSourceSize)))
      .RetiresOnSaturation();
  EXPECT_CALL(frame_source, RefreshNow());

  const SkBitmap test_bitmap = MakeTestBitmap(0);
  overlay.SetImageAndBounds(test_bitmap, gfx::RectF(0, 0, 1, 1));
  const gfx::Size output_size(test_bitmap.width(), test_bitmap.height());
  VideoCaptureOverlay::OnceRenderer renderer =
      overlay.MakeRenderer(VideoCaptureOverlay::CapturedFrameProperties{
          .region_properties =
              CapturableFrameSink::RegionProperties{
                  .root_render_pass_size = output_size,
                  .render_pass_subrect = gfx::Rect(output_size),
              },
          .content_rect = gfx::Rect(output_size),
          .format = pixel_format()});
  ASSERT_TRUE(renderer);
  auto frame = CreateVideoFrame(output_size);
  std::move(renderer).Run(frame.get());
  EXPECT_TRUE(FrameMatchesPNG(*frame, "overlay_full_cover.png"));
}

// Basic test: Render an overlay image that covers the entire video frame and is
// scaled.
TEST_P(VideoCaptureOverlayRenderTest, FullCover_WithScaling) {
  StrictMock<MockFrameSource> frame_source;
  mojo::Remote<mojom::FrameSinkVideoCaptureOverlay> overlay_remote;
  VideoCaptureOverlay overlay(frame_source,
                              overlay_remote.BindNewPipeAndPassReceiver());

  EXPECT_CALL(frame_source, GetSourceSize())
      .WillRepeatedly(Return(kSourceSize));
  EXPECT_CALL(frame_source, InvalidateRect(gfx::Rect())).RetiresOnSaturation();
  EXPECT_CALL(frame_source, InvalidateRect(gfx::Rect(kSourceSize)))
      .RetiresOnSaturation();
  EXPECT_CALL(frame_source, RefreshNow());

  const SkBitmap test_bitmap = MakeTestBitmap(0);
  overlay.SetImageAndBounds(test_bitmap, gfx::RectF(0, 0, 1, 1));
  const gfx::Size output_size(test_bitmap.width() * 4,
                              test_bitmap.height() * 4);
  VideoCaptureOverlay::OnceRenderer renderer =
      overlay.MakeRenderer(VideoCaptureOverlay::CapturedFrameProperties{
          .region_properties =
              CapturableFrameSink::RegionProperties{
                  .root_render_pass_size = output_size,
                  .render_pass_subrect = gfx::Rect(output_size),
              },
          .content_rect = gfx::Rect(output_size),
          .format = pixel_format()});
  ASSERT_TRUE(renderer);
  auto frame = CreateVideoFrame(output_size);
  std::move(renderer).Run(frame.get());
  EXPECT_TRUE(FrameMatchesPNG(*frame, "overlay_full_cover_scaled.png"));
}

// Tests that changing the position of the overlay results in it being rendered
// at different locations in the video frame.
TEST_P(VideoCaptureOverlayRenderTest, MovesAround) {
  NiceMock<MockFrameSource> frame_source;
  EXPECT_CALL(frame_source, GetSourceSize())
      .WillRepeatedly(Return(kSourceSize));
  mojo::Remote<mojom::FrameSinkVideoCaptureOverlay> overlay_remote;
  VideoCaptureOverlay overlay(frame_source,
                              overlay_remote.BindNewPipeAndPassReceiver());

  const SkBitmap test_bitmap = MakeTestBitmap(0);
  const gfx::Size video_frame_size(test_bitmap.width() * 4,
                                   test_bitmap.height() * 4);

  const gfx::RectF relative_image_bounds[6] = {
      gfx::RectF(0.0f, 0.0f, 0.5f, 0.5f),
      gfx::RectF(1.0f / video_frame_size.width(), 0.0f, 0.5f, 0.5f),
      gfx::RectF(2.0f / video_frame_size.width(), 0.0f, 0.5f, 0.5f),
      gfx::RectF(2.0f / video_frame_size.width(),
                 1.0f / video_frame_size.height(), 0.5f, 0.5f),
      gfx::RectF(2.0f / video_frame_size.width(),
                 2.0f / video_frame_size.height(), 0.5f, 0.5f),
      gfx::RectF(0.5f, 0.5f, 0.5f, 0.5f),
  };

  VideoCaptureOverlay::OnceRenderer renderers[6];
  for (int i = 0; i < 6; ++i) {
    if (i == 0) {
      overlay.SetImageAndBounds(test_bitmap, relative_image_bounds[i]);
    } else {
      overlay.SetBounds(relative_image_bounds[i]);
    }
    renderers[i] =
        overlay.MakeRenderer(VideoCaptureOverlay::CapturedFrameProperties{
            .region_properties =
                CapturableFrameSink::RegionProperties{
                    .root_render_pass_size = video_frame_size,
                    .render_pass_subrect = gfx::Rect(video_frame_size),
                },
            .content_rect = gfx::Rect(video_frame_size),
            .format = pixel_format()});
  }

  constexpr std::array<const char*, 6> kGoldenFiles = {
      "overlay_moves_0_0.png", "overlay_moves_1_0.png", "overlay_moves_2_0.png",
      "overlay_moves_2_1.png", "overlay_moves_2_2.png", "overlay_moves_lr.png",
  };

  ExpectRendersAs(renderers, kGoldenFiles.data(), kGoldenFiles.size(),
                  video_frame_size);
}

// Tests that the overlay will be partially rendered (clipped) when any part of
// it extends outside the video frame's content region.
//
// For this test, the content region is a rectangle, centered within the frame
// (e.g., the content is being letterboxed), and the test attempts to locate the
// overlay such that part of it should be clipped. The test succeeds if the
// overlay is clipped to the content region in the center. For example:
//
//    +-------------------------------+
//    |                               |
//    |     ......                    |
//    |     ..****////////////        |  **** the drawn part of the overlay
//    |     ..****CONTENT/////        |
//    |       /////REGION/////        |  .... the clipped part of the overlay
//    |       ////////////////        |       (i.e., not drawn)
//    |                               |
//    |                               |
//    +-------------------------------+
TEST_P(VideoCaptureOverlayRenderTest, ClipsToContentBounds) {
  NiceMock<MockFrameSource> frame_source;
  EXPECT_CALL(frame_source, GetSourceSize())
      .WillRepeatedly(Return(kSourceSize));
  mojo::Remote<mojom::FrameSinkVideoCaptureOverlay> overlay_remote;
  VideoCaptureOverlay overlay(frame_source,
                              overlay_remote.BindNewPipeAndPassReceiver());

  const SkBitmap test_bitmap = MakeTestBitmap(0);
  const gfx::Size video_frame_size(test_bitmap.width() * 4,
                                   test_bitmap.height() * 4);
  const gfx::Rect region_in_frame(test_bitmap.width(), test_bitmap.height(),
                                  test_bitmap.width() * 2,
                                  test_bitmap.height() * 2);

  const gfx::RectF relative_image_bounds[4] = {
      gfx::RectF(-0.25f, -0.25f, 0.5f, 0.5f),
      gfx::RectF(0.75f, -0.25f, 0.5f, 0.5f),
      gfx::RectF(0.75f, 0.75f, 0.5f, 0.5f),
      gfx::RectF(-0.25f, 0.75f, 0.5f, 0.5f),
  };

  VideoCaptureOverlay::OnceRenderer renderers[4];
  for (int i = 0; i < 4; ++i) {
    if (i == 0) {
      overlay.SetImageAndBounds(test_bitmap, relative_image_bounds[i]);
    } else {
      overlay.SetBounds(relative_image_bounds[i]);
    }
    renderers[i] =
        overlay.MakeRenderer(VideoCaptureOverlay::CapturedFrameProperties{
            .region_properties =
                CapturableFrameSink::RegionProperties{
                    .root_render_pass_size = video_frame_size,
                    .render_pass_subrect = gfx::Rect(video_frame_size),
                },
            .content_rect = region_in_frame,
            .format = pixel_format()});
  }

  constexpr std::array<const char*, 4> kGoldenFiles = {
      "overlay_clips_ul.png",
      "overlay_clips_ur.png",
      "overlay_clips_lr.png",
      "overlay_clips_ll.png",
  };

  ExpectRendersAs(renderers, kGoldenFiles.data(), kGoldenFiles.size(),
                  video_frame_size);
}

TEST_P(VideoCaptureOverlayRenderTest, HandlesEmptySubRegion) {
  NiceMock<MockFrameSource> frame_source;
  EXPECT_CALL(frame_source, GetSourceSize())
      .WillRepeatedly(Return(kSourceSize));
  mojo::Remote<mojom::FrameSinkVideoCaptureOverlay> overlay_remote;
  VideoCaptureOverlay overlay(frame_source,
                              overlay_remote.BindNewPipeAndPassReceiver());

  const SkBitmap test_bitmap = MakeTestBitmap(0);
  const gfx::Size frame_size(test_bitmap.width() * 4, test_bitmap.height() * 4);
  const gfx::Rect compositor_frame_subrect;
  const gfx::RectF relative_image_bounds(0.125f, .125f, 0.25f, 0.25f);

  overlay.SetImageAndBounds(test_bitmap, relative_image_bounds);
  auto renderer =
      overlay.MakeRenderer(VideoCaptureOverlay::CapturedFrameProperties{
          .region_properties =
              CapturableFrameSink::RegionProperties{
                  .root_render_pass_size = frame_size,
                  .render_pass_subrect = compositor_frame_subrect,
              },
          .content_rect = gfx::Rect(frame_size),
          .format = pixel_format()});

  // We shouldn't even create a renderer if we aren't capturing any pixels.
  EXPECT_FALSE(renderer);
}

TEST_P(VideoCaptureOverlayRenderTest, ClipsToSubregionBounds) {
  NiceMock<MockFrameSource> frame_source;
  EXPECT_CALL(frame_source, GetSourceSize())
      .WillRepeatedly(Return(kSourceSize));
  mojo::Remote<mojom::FrameSinkVideoCaptureOverlay> overlay_remote;
  VideoCaptureOverlay overlay(frame_source,
                              overlay_remote.BindNewPipeAndPassReceiver());

  const SkBitmap test_bitmap = MakeTestBitmap(0);
  const gfx::Size frame_size(test_bitmap.width() * 4, test_bitmap.height() * 4);
  const gfx::Rect compositor_frame_subrect(
      test_bitmap.width(), test_bitmap.height(), test_bitmap.width() * 2,
      test_bitmap.height() * 2);
  const gfx::RectF relative_image_bounds[4] = {
      gfx::RectF(0.125f, .125f, 0.25f, 0.25f),
      gfx::RectF(0.625f, .125f, 0.25f, 0.25f),
      gfx::RectF(0.625f, 0.625f, 0.25f, 0.25f),
      gfx::RectF(.125f, 0.625f, 0.25f, 0.25f),
  };

  VideoCaptureOverlay::OnceRenderer renderers[4];
  for (int i = 0; i < 4; ++i) {
    if (i == 0) {
      overlay.SetImageAndBounds(test_bitmap, relative_image_bounds[i]);
    } else {
      overlay.SetBounds(relative_image_bounds[i]);
    }
    renderers[i] =
        overlay.MakeRenderer(VideoCaptureOverlay::CapturedFrameProperties{
            .region_properties =
                CapturableFrameSink::RegionProperties{
                    .root_render_pass_size = frame_size,
                    .render_pass_subrect = compositor_frame_subrect,
                },
            .content_rect = gfx::Rect(compositor_frame_subrect.size()),
            .format = pixel_format()});
  }

  constexpr std::array<const char*, 4> kGoldenFiles = {
      "overlay_clips_ul_subregion.png",
      "overlay_clips_ur_subregion.png",
      "overlay_clips_lr_subregion.png",
      "overlay_clips_ll_subregion.png",
  };

  ExpectRendersAs(renderers, kGoldenFiles.data(), kGoldenFiles.size(),
                  compositor_frame_subrect.size());
}

TEST_P(VideoCaptureOverlayRenderTest, ScalesToContentRegion) {
  NiceMock<MockFrameSource> frame_source;
  EXPECT_CALL(frame_source, GetSourceSize())
      .WillRepeatedly(Return(kSourceSize));
  mojo::Remote<mojom::FrameSinkVideoCaptureOverlay> overlay_remote;
  VideoCaptureOverlay overlay(frame_source,
                              overlay_remote.BindNewPipeAndPassReceiver());

  const SkBitmap test_bitmap = MakeTestBitmap(0);
  const gfx::Size video_frame_size(test_bitmap.width() * 4,
                                   test_bitmap.height() * 4);
  const gfx::Rect compositor_frame_subrect(
      test_bitmap.width(), test_bitmap.height(), test_bitmap.width() * 2,
      test_bitmap.height() * 2);
  const gfx::Rect content_rect(
      test_bitmap.width() * 2, test_bitmap.height() * 2,
      test_bitmap.width() * 6, test_bitmap.height() * 6);

  const gfx::RectF relative_image_bounds[4] = {
      gfx::RectF(0.125f, .125f, 0.25f, 0.25f),
      gfx::RectF(0.625f, .125f, 0.25f, 0.25f),
      gfx::RectF(0.625f, 0.625f, 0.25f, 0.25f),
      gfx::RectF(.125f, 0.625f, 0.25f, 0.25f),
  };

  VideoCaptureOverlay::OnceRenderer renderers[4];
  for (int i = 0; i < 4; ++i) {
    if (i == 0) {
      overlay.SetImageAndBounds(test_bitmap, relative_image_bounds[i]);
    } else {
      overlay.SetBounds(relative_image_bounds[i]);
    }
    renderers[i] =
        overlay.MakeRenderer(VideoCaptureOverlay::CapturedFrameProperties{
            .region_properties =
                CapturableFrameSink::RegionProperties{
                    .root_render_pass_size = video_frame_size,
                    .render_pass_subrect = compositor_frame_subrect,
                },
            .content_rect = content_rect,
            .format = pixel_format()});
  }

  constexpr std::array<const char*, 4> kGoldenFiles = {
      "overlay_clips_ul_contentscaled.png",
      "overlay_clips_ur_contentscaled.png",
      "overlay_clips_lr_contentscaled.png",
      "overlay_clips_ll_contentscaled.png",
  };

  ExpectRendersAs(renderers, kGoldenFiles.data(), kGoldenFiles.size(),
                  gfx::Size(content_rect.right(), content_rect.bottom()));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    VideoCaptureOverlayRenderTest,
    testing::Values(VideoCaptureOverlayRenderTest::kARGBFormat,
                    VideoCaptureOverlayRenderTest::kI420Format));

}  // namespace
}  // namespace viz
