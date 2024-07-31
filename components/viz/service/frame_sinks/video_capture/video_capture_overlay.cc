// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/viz/service/frame_sinks/video_capture/video_capture_overlay.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/base/video_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

using media::VideoFrame;
using media::VideoPixelFormat;

namespace viz {

VideoCaptureOverlay::FrameSource::~FrameSource() = default;

VideoCaptureOverlay::VideoCaptureOverlay(
    FrameSource& frame_source,
    mojo::PendingReceiver<mojom::FrameSinkVideoCaptureOverlay> receiver)
    : frame_source_(frame_source), receiver_(this, std::move(receiver)) {
  receiver_.set_disconnect_handler(
      base::BindOnce(&FrameSource::OnOverlayConnectionLost,
                     base::Unretained(frame_source_), this));
}

VideoCaptureOverlay::~VideoCaptureOverlay() = default;

void VideoCaptureOverlay::SetImageAndBounds(const SkBitmap& image,
                                            const gfx::RectF& bounds) {
  const gfx::Rect old_rect = ComputeSourceMutationRect();

  image_ = image;
  bounds_ = bounds;

  image_.setImmutable();

  // Reset the cached sprite since the source image has been changed.
  sprite_ = nullptr;

  const gfx::Rect new_rect = ComputeSourceMutationRect();
  if (!new_rect.IsEmpty() || !old_rect.IsEmpty()) {
    frame_source_->InvalidateRect(old_rect);
    frame_source_->InvalidateRect(new_rect);
    frame_source_->RefreshNow();
  }
}

void VideoCaptureOverlay::SetBounds(const gfx::RectF& bounds) {
  if (bounds_ != bounds) {
    const gfx::Rect old_rect = ComputeSourceMutationRect();
    bounds_ = bounds;
    const gfx::Rect new_rect = ComputeSourceMutationRect();
    if (!new_rect.IsEmpty() || !old_rect.IsEmpty()) {
      frame_source_->InvalidateRect(old_rect);
      frame_source_->InvalidateRect(new_rect);
      frame_source_->RefreshNow();
    }
  }
}

namespace {

// Scales a |relative| rect having coordinates in the range [0.0,1.0) by the
// given |span|, snapping all coordinates to even numbers.
gfx::Rect ToAbsoluteBoundsForI420(const gfx::RectF& relative,
                                  const gfx::Rect& span) {
  const float absolute_left = std::fma(relative.x(), span.width(), span.x());
  const float absolute_top = std::fma(relative.y(), span.height(), span.y());
  const float absolute_right =
      std::fma(relative.right(), span.width(), span.x());
  const float absolute_bottom =
      std::fma(relative.bottom(), span.height(), span.y());

  // Compute the largest I420-friendly Rect that is fully-enclosed by the
  // absolute rect. Use saturated_cast<> to restrict all extreme results [and
  // Inf and NaN] to a safe range of integers.
  const int snapped_left =
      base::saturated_cast<int16_t>(std::ceil(absolute_left / 2.0f)) * 2;
  const int snapped_top =
      base::saturated_cast<int16_t>(std::ceil(absolute_top / 2.0f)) * 2;
  const int snapped_right =
      base::saturated_cast<int16_t>(std::floor(absolute_right / 2.0f)) * 2;
  const int snapped_bottom =
      base::saturated_cast<int16_t>(std::floor(absolute_bottom / 2.0f)) * 2;
  return gfx::Rect(snapped_left, snapped_top,
                   std::max(0, snapped_right - snapped_left),
                   std::max(0, snapped_bottom - snapped_top));
}

// Uses the mapping of a region R that exists in coordinate system A
// as |from_region| and in coordinate system B as |to_region|. The |source|
// rectangle is in coordinate system A and mapped to coordinate system B
// in three steps:
//   1. translate to remove the origin of the old coordinate space.
//   2. scale values to the new space.
//   3. translate to add the origin of the new coordinate space.
gfx::Rect Transform(const gfx::Rect& source,
                    const gfx::Rect& from_region,
                    const gfx::Rect& to_region) {
  // Transforming from or to a zero space is undefined behavior.
  if (from_region.IsEmpty() || to_region.IsEmpty())
    return {};

  const gfx::Vector2dF scale{static_cast<float>(to_region.width()) /
                                 static_cast<float>(from_region.width()),
                             static_cast<float>(to_region.height()) /
                                 static_cast<float>(from_region.height())};

  const gfx::Rect old_translated =
      gfx::Rect(source.x() - from_region.x(), source.y() - from_region.y(),
                source.width(), source.height());
  const gfx::Rect scaled =
      gfx::ScaleToEnclosingRect(old_translated, scale.x(), scale.y());
  const gfx::Rect new_translated =
      gfx::Rect(scaled.x() + to_region.x(), scaled.y() + to_region.y(),
                scaled.width(), scaled.height());

  return media::MinimallyShrinkRectForI420(new_translated);
}

}  // namespace

std::string VideoCaptureOverlay::CapturedFrameProperties::ToString() const {
  return base::StringPrintf(
      "%s from %s into %s via transform %s, format %s",
      region_properties.render_pass_subrect.ToString().c_str(),
      region_properties.root_render_pass_size.ToString().c_str(),
      content_rect.ToString().c_str(),
      region_properties.transform_to_root.ToString().c_str(),
      media::VideoPixelFormatToString(format).c_str());
}

std::string VideoCaptureOverlay::BlendInformation::ToString() const {
  return base::StringPrintf(
      "source_region=%s, source_region_scaled=%s, "
      "destination_region_content=%s",
      source_region.ToString().c_str(), source_region_scaled.ToString().c_str(),
      destination_region_content.ToString().c_str());
}

std::optional<VideoCaptureOverlay::BlendInformation>
VideoCaptureOverlay::CalculateBlendInformation(
    const CapturedFrameProperties& properties) const {
  const auto& compositor_frame_rect =
      gfx::Rect(properties.region_properties.root_render_pass_size);
  const gfx::Rect compositor_frame_subrect =
      properties.region_properties.transform_to_root.MapRect(
          properties.region_properties.render_pass_subrect);

  // The sub region should always be a subset of the frame region.
  CHECK(compositor_frame_rect.Contains(compositor_frame_subrect));

  // If there's no image set yet, punt.
  if (image_.drawsNothing() || bounds_.IsEmpty()) {
    return std::nullopt;
  }

  // Determine the bounds of the sprite to be blended onto the video frame. The
  // calculations here align to the 2x2 pixel-quads, since dealing with
  // fractions or partial I420 chroma plane alpha-blending would greatly
  // complexify the blitting algorithm later on. This introduces a little
  // inaccuracy in the size and position of the overlay in the final result, but
  // should be an acceptable trade-off for all use cases.
  //
  // Rescale the relative bounds (scoped between [0, 1]) to absolute bounds
  // based on the entire region of the frame sink being captured. This allows
  // for calculations such as mouse cursor position (which is retrieved in
  // relationship to the entire tab or window) to be scaled properly.
  const gfx::Rect bounds_in_compositor_space =
      ToAbsoluteBoundsForI420(bounds_, compositor_frame_rect);

  // If the sprite that we want to render does not fall within the subregion
  // that we are capturing, punt.
  if (!bounds_in_compositor_space.Intersects(compositor_frame_subrect)) {
    return std::nullopt;
  }

  // The bounds are currently in the coordinate space of the captured compositor
  // frame, however blending may be done in the coordinate space of the
  // outputted video frame and must be scaled and translated.
  const gfx::Rect bounds_in_content_space =
      Transform(bounds_in_compositor_space, compositor_frame_subrect,
                properties.content_rect);

  // If the sprite's size will be unreasonably large, punt.
  if (bounds_in_content_space.width() > media::limits::kMaxDimension ||
      bounds_in_content_space.height() > media::limits::kMaxDimension) {
    return std::nullopt;
  }

  // Now let's see where the scaled sprite will be placed in the video frame.
  // By intersecting, we will check if the entire sprite fits in the frame,
  // and if not, we will calculate which part of the sprite will be blended.
  // |blit_rect| is the region of the video frame that we will write into.
  const gfx::Rect blit_rect =
      gfx::IntersectRects(bounds_in_content_space, properties.content_rect);

  // If the scaled sprite's size is empty, punt.
  if (blit_rect.IsEmpty()) {
    return std::nullopt;
  }

  // Compute the left-most and top-most pixel to source from the transformed
  // image. This is usually (0,0) unless only part of the sprite is being
  // blended (i.e., cropped at the edge(s) of the video frame):
  const gfx::Rect source_region_scaled =
      gfx::Rect(blit_rect.origin() - bounds_in_content_space.OffsetFromOrigin(),
                blit_rect.size());

  // Scaling is determined by the ratio of the |image_| size to
  // |bounds_in_content_space| size - we know the size of the scaled region, so
  // use the ratio to compute the unscaled region:
  float scale_x = static_cast<float>(image_.dimensions().width()) /
                  bounds_in_content_space.width();
  float scale_y = static_cast<float>(image_.dimensions().height()) /
                  bounds_in_content_space.height();
  const gfx::Rect source_region =
      gfx::ScaleToEnclosingRect(source_region_scaled, scale_x, scale_y);

  // If the unscaled source region is empty, punt.
  if (source_region.IsEmpty()) {
    return std::nullopt;
  }

  return BlendInformation{source_region, source_region_scaled,
                          bounds_in_content_space};
}

VideoCaptureOverlay::OnceRenderer VideoCaptureOverlay::MakeRenderer(
    const CapturedFrameProperties& properties) {
  std::optional<VideoCaptureOverlay::BlendInformation> blend_information =
      CalculateBlendInformation(properties);
  if (!blend_information) {
    return {};
  }

  // Sprite cares about scaled source region, as it will blend from a
  // transformed image:
  gfx::Rect src_rect = blend_information->source_region_scaled;
  // Sprite cares about content's destination region, as it will blend into the
  // video frame:
  gfx::Rect dst_rect = blend_information->destination_region_content;

  // If the cached sprite does not match the computed scaled size and/or
  // pixel format, create a new instance for this (and future) renderers.
  if (!sprite_ || sprite_->size() != dst_rect.size() ||
      sprite_->format() != properties.format) {
    sprite_ = base::MakeRefCounted<Sprite>(image_, dst_rect.size(),
                                           properties.format);
  }

  dst_rect.Intersect(properties.content_rect);
  if (dst_rect.IsEmpty())
    return {};

  return base::BindOnce(&Sprite::Blend, sprite_, src_rect, dst_rect);
}

// static
VideoCaptureOverlay::OnceRenderer VideoCaptureOverlay::MakeCombinedRenderer(
    const std::vector<VideoCaptureOverlay*>& overlays,
    const CapturedFrameProperties& properties) {
  if (overlays.empty())
    return {};

  std::vector<OnceRenderer> renderers;
  for (VideoCaptureOverlay* overlay : overlays) {
    renderers.emplace_back(overlay->MakeRenderer(properties));
    if (renderers.back().is_null()) {
      renderers.pop_back();
    }
  }

  if (renderers.empty())
    return {};

  return base::BindOnce(
      [](std::vector<OnceRenderer> renderers, VideoFrame* frame) {
        for (OnceRenderer& renderer : renderers) {
          std::move(renderer).Run(frame);
        }
      },
      std::move(renderers));
}

gfx::Rect VideoCaptureOverlay::ComputeSourceMutationRect() const {
  if (!image_.drawsNothing() && !bounds_.IsEmpty()) {
    const gfx::Size& source_size = frame_source_->GetSourceSize();
    gfx::Rect result = gfx::ToEnclosingRect(
        gfx::ScaleRect(bounds_, source_size.width(), source_size.height()));
    result.Intersect(gfx::Rect(source_size));
    return result;
  }
  return {};
}

VideoCaptureOverlay::Sprite::Sprite(const SkBitmap& image,
                                    const gfx::Size& size,
                                    const VideoPixelFormat format)
    : image_(image), size_(size), format_(format) {
  CHECK(!image_.isNull());
}

VideoCaptureOverlay::Sprite::~Sprite() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

namespace {

// Returns the pointer to the element at the |offset| position, given a pointer
// to the element for (0,0) in a row-major image plane.
template <typename Pointer>
Pointer PositionPointerInPlane(Pointer plane_begin,
                               int stride,
                               const gfx::Point& offset) {
  return plane_begin + (offset.y() * stride) + offset.x();
}

// Returns the pointer to the element at the |offset| position, given a pointer
// to the element for (0,0) in a row-major bitmap with 4 elements per pixel.
template <typename Pointer>
Pointer PositionPointerARGB(Pointer pixels_begin,
                            int stride,
                            const gfx::Point& offset) {
  return pixels_begin + (offset.y() * stride) + (4 * offset.x());
}

// Transforms the lower 8 bits of |value| from the [0,255] range to the
// normalized floating-point [0.0,1.0] range.
float From255(uint8_t value) {
  return value / 255.0f;
}

// Transforms the value from the normalized floating-point [0.0,1.0] range to an
// unsigned int in the [0,255] range, capping any out-of-range values.
uint32_t ToClamped255(float value) {
  value = std::fma(value, 255.0f, 0.5f /* rounding */);
  return base::saturated_cast<uint8_t>(value);
}

}  // namespace

void VideoCaptureOverlay::Sprite::Blend(const gfx::Rect& src_rect,
                                        const gfx::Rect& dst_rect,
                                        VideoFrame* frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(frame);
  CHECK(frame->visible_rect().Contains(dst_rect))
      << "frame->visible_rect()=" << frame->visible_rect().ToString()
      << ", dst_rect=" << dst_rect.ToString();
  CHECK(gfx::Rect(size_).Contains(src_rect))
      << "size_=" << size_.ToString() << ", src_rect=" << src_rect.ToString();

  CHECK_EQ(format_, frame->format());
  CHECK(!dst_rect.IsEmpty())
      << ": frame->visible_rect()=" << frame->visible_rect().ToString()
      << ", dst_rect=" << dst_rect.ToString();
  CHECK(frame->ColorSpace().IsValid());

  TRACE_EVENT("gpu.capture", "VideoCaptureOverlay::Sprite::Blend", "x",
              dst_rect.x(), "y", dst_rect.y());

  if (!transformed_image_ || color_space_ != frame->ColorSpace()) {
    color_space_ = frame->ColorSpace();
    TransformImage();
  }

  gfx::Point src_origin = src_rect.origin();

  // Blit the sprite (src) onto the video frame (dest). One of two algorithms is
  // used, depending on the video frame's format, as the blending calculations
  // and data layout/format are different.
  switch (frame->format()) {
    case media::PIXEL_FORMAT_I420: {
      // Core assumption: All coordinates are aligned to even-numbered
      // coordinates.
      CHECK_EQ(src_origin.x() % 2, 0);
      CHECK_EQ(src_origin.y() % 2, 0);
      CHECK_EQ(dst_rect.x() % 2, 0);
      CHECK_EQ(dst_rect.y() % 2, 0);
      CHECK_EQ(dst_rect.width() % 2, 0);
      CHECK_EQ(dst_rect.height() % 2, 0);

      // Helper function to execute a "SrcOver" blit from |src| to |dst|, and
      // store the results back in |dst|.
      const auto BlitOntoPlane = [](const gfx::Size& blit_size, int src_stride,
                                    const float* src, const float* under_weight,
                                    int dst_stride, uint8_t* dst) {
        for (int row = 0; row < blit_size.height(); ++row, src += src_stride,
                 under_weight += src_stride, dst += dst_stride) {
          for (int col = 0; col < blit_size.width(); ++col) {
            dst[col] = base::saturated_cast<uint8_t>(
                dst[col] * under_weight[col] + 255.0f * src[col] + 0.5f);
          }
        }
      };

      // Blit the Y plane: |src| points to the pre-multiplied luma values, while
      // |under_weight| points to the "one minus src alpha" values. Both have
      // the same stride, |src_stride|.
      int src_stride = size_.width();
      const float* under_weight = PositionPointerInPlane(
          transformed_image_.get(), src_stride, src_origin);
      const int num_pixels = size_.GetArea();
      const float* src = under_weight + num_pixels;
      // Likewise, start |dst| at the upper-left-most pixel within the video
      // frame's Y plane that will be SrcOver'ed.
      int dst_stride = frame->stride(VideoFrame::Plane::kY);
      uint8_t* dst = PositionPointerInPlane(
          frame->GetWritableVisibleData(VideoFrame::Plane::kY), dst_stride,
          dst_rect.origin());
      BlitOntoPlane(dst_rect.size(), src_stride, src, under_weight, dst_stride,
                    dst);

      // Blit the U and V planes similarly to the Y plane, but reduce all
      // coordinates by 2x2.
      src_stride = size_.width() / 2;
      src_origin = gfx::Point(src_origin.x() / 2, src_origin.y() / 2);
      under_weight = PositionPointerInPlane(
          transformed_image_.get() + 2 * num_pixels, src_stride, src_origin);
      const int num_chroma_pixels = size_.GetArea() / 4;
      src = under_weight + num_chroma_pixels;
      dst_stride = frame->stride(VideoFrame::Plane::kU);
      const gfx::Rect chroma_blit_rect(dst_rect.x() / 2, dst_rect.y() / 2,
                                       dst_rect.width() / 2,
                                       dst_rect.height() / 2);
      dst = PositionPointerInPlane(
          frame->GetWritableVisibleData(VideoFrame::Plane::kU), dst_stride,
          chroma_blit_rect.origin());
      BlitOntoPlane(chroma_blit_rect.size(), src_stride, src, under_weight,
                    dst_stride, dst);
      src += num_chroma_pixels;
      dst_stride = frame->stride(VideoFrame::Plane::kV);
      dst = PositionPointerInPlane(
          frame->GetWritableVisibleData(VideoFrame::Plane::kV), dst_stride,
          chroma_blit_rect.origin());
      BlitOntoPlane(chroma_blit_rect.size(), src_stride, src, under_weight,
                    dst_stride, dst);

      break;
    }

    case media::PIXEL_FORMAT_ARGB: {
      // Start |src| at the upper-left-most pixel within |transformed_image_|
      // that will be blitted.
      const int src_stride = size_.width() * 4;
      const float* src =
          PositionPointerARGB(transformed_image_.get(), src_stride, src_origin);

      // Likewise, start |dst| at the upper-left-most pixel within the video
      // frame that will be SrcOver'ed.
      const int dst_stride = frame->stride(VideoFrame::Plane::kARGB);
      CHECK_EQ(dst_stride % sizeof(uint32_t), 0u);
      uint8_t* dst = PositionPointerARGB(
          frame->GetWritableVisibleData(VideoFrame::Plane::kARGB), dst_stride,
          dst_rect.origin());
      CHECK_EQ((dst - frame->visible_data(VideoFrame::Plane::kARGB)) %
                   sizeof(uint32_t),
               0u);

      // Blend each sprite pixel over the corresponding pixel in the video
      // frame, and store the result back in the video frame. Note that the
      // video frame format does NOT have color values pre-multiplied by the
      // alpha.
      for (int row = 0; row < dst_rect.height();
           ++row, src += src_stride, dst += dst_stride) {
        uint32_t* dst_pixel = reinterpret_cast<uint32_t*>(dst);
        for (int col = 0; col < dst_rect.width(); ++col) {
          const int src_idx = 4 * col;
          const float src_alpha = src[src_idx];
          const float dst_weight =
              From255(dst_pixel[col] >> 24) * (1.0f - src_alpha);
          const float out_alpha = src_alpha + dst_weight;
          float out_red = std::fma(From255(dst_pixel[col] >> 16), dst_weight,
                                   src[src_idx + 1]);
          float out_green = std::fma(From255(dst_pixel[col] >> 8), dst_weight,
                                     src[src_idx + 2]);
          float out_blue = std::fma(From255(dst_pixel[col] >> 0), dst_weight,
                                    src[src_idx + 3]);
          if (out_alpha != 0.0f) {
            out_red /= out_alpha;
            out_green /= out_alpha;
            out_blue /= out_alpha;
          }
          dst_pixel[col] =
              ((ToClamped255(out_alpha) << 24) | (ToClamped255(out_red) << 16) |
               (ToClamped255(out_green) << 8) | (ToClamped255(out_blue) << 0));
        }
      }

      break;
    }

    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void VideoCaptureOverlay::Sprite::TransformImage() {
  TRACE_EVENT("gpu.capture", "VideoCaptureOverlay::Sprite::TransformImage",
              "width", size_.width(), "height", size_.height());

  // Scale the source |image_| to match the format and size required. For the
  // purposes of color space conversion, the alpha must not be pre-multiplied.
  const SkImageInfo scaled_image_format =
      SkImageInfo::Make(size_.width(), size_.height(), kN32_SkColorType,
                        kUnpremul_SkAlphaType, image_.refColorSpace());
  SkBitmap scaled_image;
  if (image_.info() == scaled_image_format) {
    scaled_image = image_;
  } else {
    if (scaled_image.tryAllocPixels(scaled_image_format) &&
        image_.pixmap().scalePixels(
            scaled_image.pixmap(),
            SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNearest))) {
      // Cache the scaled image, to avoid needing to re-scale in future calls to
      // this method.
      image_ = scaled_image;
    } else {
      // If the allocation, format conversion and/or scaling failed, just reset
      // the |scaled_image|. This will be checked below.
      scaled_image.reset();
    }
  }

  // Populate |colors| and |alphas| from the |scaled_image|. If the image
  // scaling operation failed, this sprite should draw nothing, and so fully
  // transparent pixels will be generated instead.
  const int num_pixels = size_.GetArea();
  auto alphas = base::HeapArray<float>::Uninit(num_pixels);
  auto colors =
      base::HeapArray<gfx::ColorTransform::TriStim>::WithSize(num_pixels);
  if (scaled_image.drawsNothing()) {
    std::fill(alphas.begin(), alphas.end(), 0.0f);
    std::fill(colors.begin(), colors.end(), gfx::ColorTransform::TriStim());
  } else {
    int pos = 0;
    for (int y = 0; y < size_.height(); ++y) {
      const uint32_t* src = scaled_image.getAddr32(0, y);
      for (int x = 0; x < size_.width(); ++x) {
        const uint32_t pixel = src[x];
        alphas[pos] = ((pixel >> SK_A32_SHIFT) & 0xff) / 255.0f;
        colors[pos].SetPoint(((pixel >> SK_R32_SHIFT) & 0xff) / 255.0f,
                             ((pixel >> SK_G32_SHIFT) & 0xff) / 255.0f,
                             ((pixel >> SK_B32_SHIFT) & 0xff) / 255.0f);
        ++pos;
      }
    }
  }

  // Transform the colors, if needed. This may perform RGB→YUV conversion.
  gfx::ColorSpace image_color_space;
  if (scaled_image.colorSpace()) {
    image_color_space = gfx::ColorSpace(*scaled_image.colorSpace());
  }
  if (!image_color_space.IsValid()) {
    // Assume a default linear color space, if no color space was provided.
    image_color_space = gfx::ColorSpace(
        gfx::ColorSpace::PrimaryID::BT709, gfx::ColorSpace::TransferID::LINEAR,
        gfx::ColorSpace::MatrixID::RGB, gfx::ColorSpace::RangeID::FULL);
  }
  if (image_color_space != color_space_) {
    const auto color_transform =
        gfx::ColorTransform::NewColorTransform(image_color_space, color_space_);
    color_transform->Transform(colors.data(), num_pixels);
  }

  switch (format_) {
    case media::PIXEL_FORMAT_I420: {
      // Produce 5 planes of data: The "one minus alpha" plane, the Y plane, the
      // subsampled "one minus alpha" plane, the U plane, and the V plane.
      // Pre-multiply the colors by the alpha to prevent extra work in multiple
      // later Blit() calls.
      CHECK_EQ(size_.width() % 2, 0);
      CHECK_EQ(size_.height() % 2, 0);
      const int num_chroma_pixels = size_.GetArea() / 4;
      transformed_image_.reset(
          new float[num_pixels * 2 + num_chroma_pixels * 3]);

      // Copy the alpha values, and pre-multiply the luma values by the alpha.
      float* out_1_minus_alpha = transformed_image_.get();
      float* out_luma = out_1_minus_alpha + num_pixels;
      for (int i = 0; i < num_pixels; ++i) {
        const float alpha = alphas[i];
        out_1_minus_alpha[i] = 1.0f - alpha;
        out_luma[i] = colors[i].x() * alpha;
      }

      // Downscale the alpha, U, and V planes by 2x2, and pre-multiply the
      // chroma values by the alpha.
      float* out_uv_1_minus_alpha = out_luma + num_pixels;
      float* out_u = out_uv_1_minus_alpha + num_chroma_pixels;
      float* out_v = out_u + num_chroma_pixels;
      auto alpha_row0 = alphas.begin();
      auto alpha_row_end = alphas.end();
      auto color_row0 = colors.begin();
      while (alpha_row0 < alpha_row_end) {
        const auto alpha_row1 = alpha_row0 + size_.width();
        const auto color_row1 = color_row0 + size_.width();
        for (int col = 0; col < size_.width(); col += 2) {
          // First, the downscaled alpha is the average of the four original
          // alpha values:
          //
          //     sum_of_alphas = a[r,c] + a[r,c+1] + a[r+1,c] + a[r+1,c+1];
          //     average_alpha = sum_of_alphas / 4
          *(out_uv_1_minus_alpha++) =
              std::fma(alpha_row0[col] + alpha_row0[col + 1] + alpha_row1[col] +
                           alpha_row1[col + 1],
                       -1.0f / 4.0f, 1.0f);
          // Then, the downscaled chroma values are the weighted average of the
          // four original chroma values (weighed by alpha):
          //
          //   weighted_sum_of_chromas =
          //       c[r,c]*a[r,c] + c[r,c+1]*a[r,c+1] +
          //           c[r+1,c]*a[r+1,c] + c[r+1,c+1]*a[r+1,c+1]
          //   sum_of_weights = sum_of_alphas;
          //   average_chroma = weighted_sum_of_chromas / sum_of_weights
          //
          // But then, because the chroma is to be pre-multiplied by the alpha,
          // the calculations simplify, as follows:
          //
          //   premul_chroma = average_chroma * average_alpha
          //                 = (weighted_sum_of_chromas / sum_of_alphas) *
          //                       (sum_of_alphas / 4)
          //                 = weighted_sum_of_chromas / 4
          //
          // This also automatically solves a special case, when sum_of_alphas
          // is zero: With the simplified calculations, there is no longer a
          // "divide-by-zero guard" needed; and the result in this case will be
          // a zero chroma, which is perfectly acceptable behavior.
          *(out_u++) = ((color_row0[col].y() * alpha_row0[col]) +
                        (color_row0[col + 1].y() * alpha_row0[col + 1]) +
                        (color_row1[col].y() * alpha_row1[col]) +
                        (color_row1[col + 1].y() * alpha_row1[col + 1])) /
                       4.0f;
          *(out_v++) = ((color_row0[col].z() * alpha_row0[col]) +
                        (color_row0[col + 1].z() * alpha_row0[col + 1]) +
                        (color_row1[col].z() * alpha_row1[col]) +
                        (color_row1[col + 1].z() * alpha_row1[col + 1])) /
                       4.0f;
        }
        alpha_row0 = alpha_row1 + size_.width();
        color_row0 = color_row1 + size_.width();
      }

      break;
    }

    case media::PIXEL_FORMAT_ARGB: {
      // Produce ARGB pixels from |colors| and |alphas|. Pre-multiply the colors
      // by the alpha to prevent extra work in multiple later Blit() calls.
      transformed_image_.reset(new float[num_pixels * 4]);
      float* out = transformed_image_.get();
      for (int i = 0; i < num_pixels; ++i) {
        const float alpha = alphas[i];
        *(out++) = alpha;
        *(out++) = colors[i].x() * alpha;
        *(out++) = colors[i].y() * alpha;
        *(out++) = colors[i].z() * alpha;
      }
      break;
    }

    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

}  // namespace viz
