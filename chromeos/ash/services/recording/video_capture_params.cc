// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/recording/video_capture_params.h"

#include "base/check.h"
#include "chromeos/ash/services/recording/recording_service_constants.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "media/base/video_types.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace recording {

namespace {

// Returns a rect that is the result of intersecting the given two rects.
gfx::Rect GetIntersectionRect(const gfx::Rect& rect_a,
                              const gfx::Rect& rect_b) {
  auto result = rect_a;
  result.Intersect(rect_b);
  return result;
}

inline gfx::Size GetSizeInPixels(const gfx::Size& size_dips, float dsf) {
  return gfx::ToFlooredSize(gfx::ConvertSizeToPixels(size_dips, dsf));
}

// -----------------------------------------------------------------------------
// FullscreenCaptureParams:

class FullscreenCaptureParams : public VideoCaptureParams {
 public:
  FullscreenCaptureParams(viz::FrameSinkId frame_sink_id,
                          const gfx::Size& frame_sink_size_dip,
                          float device_scale_factor)
      : VideoCaptureParams(frame_sink_id,
                           viz::SubtreeCaptureId(),
                           frame_sink_size_dip,
                           device_scale_factor),
        initial_frame_sink_size_pixels_(current_frame_sink_size_pixels_) {}
  FullscreenCaptureParams(const FullscreenCaptureParams&) = delete;
  FullscreenCaptureParams& operator=(const FullscreenCaptureParams&) = delete;
  ~FullscreenCaptureParams() override = default;

  // VideoCaptureParams:
  gfx::Size GetVideoSize() const override {
    return initial_frame_sink_size_pixels_;
  }

 protected:
  void SetCapturerResolutionConstraints(
      mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer)
      const override {
    DCHECK(capturer);

    capturer->SetResolutionConstraints(
        /*min_size=*/initial_frame_sink_size_pixels_,
        /*max_size=*/initial_frame_sink_size_pixels_,
        /*use_fixed_aspect_ratio=*/true);
  }

  bool OnVideoSizeMayHaveChanged(
      mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer) override {
    // We override the default behavior, as we want the video size to remain at
    // the original requested size. This gives a nice indication of display
    // rotations. The new video frames will letterbox to adhere to
    // the original requested resolution constraints.
    return false;
  }

 private:
  // We chose not to change the video size for fullscreen recording and let the
  // capturer letterbox the content in the initial size we gave it. This gives
  // a nice effect especially when rotating the display. See
  // OnVideoSizeMayHaveChanged() above.
  const gfx::Size initial_frame_sink_size_pixels_;
};

// -----------------------------------------------------------------------------
// WindowCaptureParams:

class WindowCaptureParams : public VideoCaptureParams {
 public:
  WindowCaptureParams(viz::FrameSinkId frame_sink_id,
                      viz::SubtreeCaptureId subtree_capture_id,
                      const gfx::Size& frame_sink_size_dip,
                      float device_scale_factor,
                      const gfx::Size& window_size_dip)
      : VideoCaptureParams(frame_sink_id,
                           subtree_capture_id,
                           frame_sink_size_dip,
                           device_scale_factor),
        current_window_size_dips_(window_size_dip),
        current_window_size_pixels_(CalculateWindowSizeInPixels()) {}
  WindowCaptureParams(const WindowCaptureParams&) = delete;
  WindowCaptureParams& operator=(const WindowCaptureParams&) = delete;
  ~WindowCaptureParams() override = default;

  // VideoCaptureParams:
  gfx::Rect GetVideoFrameVisibleRect(
      const gfx::Rect& original_frame_visible_rect_pixels) const override {
    return GetIntersectionRect(original_frame_visible_rect_pixels,
                               gfx::Rect(current_window_size_pixels_));
  }

  gfx::Size GetVideoSize() const override {
    return current_window_size_pixels_;
  }

  bool OnRecordedWindowChangingRoot(
      mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer,
      viz::FrameSinkId new_frame_sink_id,
      const gfx::Size& new_frame_sink_size_dip,
      float new_device_scale_factor) override {
    DCHECK(new_frame_sink_id.is_valid());
    DCHECK_NE(frame_sink_id_, new_frame_sink_id);

    frame_sink_id_ = new_frame_sink_id;
    capturer->ChangeTarget(
        viz::VideoCaptureTarget(frame_sink_id_, subtree_capture_id_),
        /*sub_capture_target_version=*/0);

    // If the movement to another display results in changes in the frame sink
    // size or DSF, OnVideoSizeMayHaveChanged() will be called by the below
    // OnFrameSinkSizeChanged().
    return OnFrameSinkSizeChanged(capturer, new_frame_sink_size_dip,
                                  new_device_scale_factor);
  }

  bool OnRecordedWindowSizeChanged(
      mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer,
      const gfx::Size& new_window_size_dip) override {
    current_window_size_dips_ = new_window_size_dip;
    return OnVideoSizeMayHaveChanged(capturer);
  }

 protected:
  void SetCapturerResolutionConstraints(
      mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer)
      const override {
    DCHECK(capturer);

    // To avoid receiving letterboxed video frames from the capturer, we ask it
    // to give us an exact resolution matching the window's size in pixels.
    capturer->SetResolutionConstraints(/*min_size=*/current_window_size_pixels_,
                                       /*max_size=*/current_window_size_pixels_,
                                       /*use_fixed_aspect_ratio=*/true);
  }

  bool OnVideoSizeMayHaveChanged(
      mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer) override {
    const auto new_window_size_pixels = CalculateWindowSizeInPixels();
    if (new_window_size_pixels == current_window_size_pixels_)
      return false;
    current_window_size_pixels_ = new_window_size_pixels;
    SetCapturerResolutionConstraints(capturer);
    return true;
  }

 private:
  gfx::Size CalculateWindowSizeInPixels() const {
    return GetSizeInPixels(current_window_size_dips_,
                           current_device_scale_factor_);
  }

  gfx::Size current_window_size_dips_;
  gfx::Size current_window_size_pixels_;
};

// -----------------------------------------------------------------------------
// RegionCaptureParams:

class RegionCaptureParams : public VideoCaptureParams {
 public:
  RegionCaptureParams(viz::FrameSinkId frame_sink_id,
                      const gfx::Size& frame_sink_size_dip,
                      float device_scale_factor,
                      const gfx::Rect& crop_region_dip)
      : VideoCaptureParams(frame_sink_id,
                           viz::SubtreeCaptureId(),
                           frame_sink_size_dip,
                           device_scale_factor),
        crop_region_dips_(crop_region_dip),
        crop_region_pixels_(CalculateCropRegionInPixels()) {
    DCHECK(!crop_region_dips_.IsEmpty());
    DCHECK(!crop_region_pixels_.IsEmpty());
  }
  RegionCaptureParams(const RegionCaptureParams&) = delete;
  RegionCaptureParams& operator=(const RegionCaptureParams&) = delete;
  ~RegionCaptureParams() override = default;

  // VideoCaptureParams:
  gfx::Rect GetVideoFrameVisibleRect(
      const gfx::Rect& original_frame_visible_rect_pixels) const override {
    // We can't crop the video frame by an invalid bounds. The crop bounds must
    // be contained within the original frame bounds.
    gfx::Rect visible_rect = crop_region_pixels_;
    visible_rect.AdjustToFit(original_frame_visible_rect_pixels);
    return visible_rect;
  }

  gfx::Size GetVideoSize() const override {
    return GetVideoFrameVisibleRect(gfx::Rect(current_frame_sink_size_pixels_))
        .size();
  }

 protected:
  void SetCapturerResolutionConstraints(
      mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer)
      const override {
    DCHECK(capturer);

    capturer->SetResolutionConstraints(
        /*min_size=*/current_frame_sink_size_pixels_,
        /*max_size=*/current_frame_sink_size_pixels_,
        /*use_fixed_aspect_ratio=*/true);
  }

  bool OnVideoSizeMayHaveChanged(
      mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer) override {
    SetCapturerResolutionConstraints(capturer);
    const auto new_crop_region_pixels = CalculateCropRegionInPixels();
    if (new_crop_region_pixels == crop_region_pixels_)
      return false;
    crop_region_pixels_ = new_crop_region_pixels;
    return true;
  }

 private:
  // Computes and returns the crop region bounds in pixels, according to the
  // |crop_region_dips_| and the |current_device_scale_factor_|.
  gfx::Rect CalculateCropRegionInPixels() const {
    return gfx::ToRoundedRect(gfx::ConvertRectToPixels(
        crop_region_dips_, current_device_scale_factor_));
  }

  const gfx::Rect crop_region_dips_;
  gfx::Rect crop_region_pixels_;
};

}  // namespace

// -----------------------------------------------------------------------------
// VideoCaptureParams:

// static
std::unique_ptr<VideoCaptureParams>
VideoCaptureParams::CreateForFullscreenCapture(
    viz::FrameSinkId frame_sink_id,
    const gfx::Size& frame_sink_size_dip,
    float device_scale_factor) {
  return std::make_unique<FullscreenCaptureParams>(
      frame_sink_id, frame_sink_size_dip, device_scale_factor);
}

// static
std::unique_ptr<VideoCaptureParams> VideoCaptureParams::CreateForWindowCapture(
    viz::FrameSinkId frame_sink_id,
    viz::SubtreeCaptureId subtree_capture_id,
    const gfx::Size& frame_sink_size_dip,
    float device_scale_factor,
    const gfx::Size& window_size_dip) {
  return std::make_unique<WindowCaptureParams>(
      frame_sink_id, subtree_capture_id, frame_sink_size_dip,
      device_scale_factor, window_size_dip);
}

// static
std::unique_ptr<VideoCaptureParams> VideoCaptureParams::CreateForRegionCapture(
    viz::FrameSinkId frame_sink_id,
    const gfx::Size& full_capture_size,
    float device_scale_factor,
    const gfx::Rect& crop_region_dip) {
  return std::make_unique<RegionCaptureParams>(
      frame_sink_id, full_capture_size, device_scale_factor, crop_region_dip);
}

void VideoCaptureParams::InitializeVideoCapturer(
    mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer,
    media::VideoPixelFormat supported_pixel_format) const {
  DCHECK(capturer);

  capturer->SetMinCapturePeriod(kMinCapturePeriod);
  capturer->SetMinSizeChangePeriod(kMinPeriodForResizeThrottling);
  SetCapturerResolutionConstraints(capturer);
  capturer->SetAutoThrottlingEnabled(false);
  // TODO(afakhry): Discuss with //media/ team the implications of color space
  // conversions.
  capturer->SetFormat(supported_pixel_format);
  capturer->ChangeTarget(
      viz::VideoCaptureTarget(frame_sink_id_, subtree_capture_id_),
      /*sub_capture_target_version=*/0);
}

gfx::Rect VideoCaptureParams::GetVideoFrameVisibleRect(
    const gfx::Rect& original_frame_visible_rect_pixels) const {
  return original_frame_visible_rect_pixels;
}

bool VideoCaptureParams::OnRecordedWindowChangingRoot(
    mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer,
    viz::FrameSinkId new_frame_sink_id,
    const gfx::Size& new_frame_sink_size_dip,
    float new_device_scale_factor) {
  CHECK(false) << "This can only be called when recording a window";
  return false;
}

bool VideoCaptureParams::OnRecordedWindowSizeChanged(
    mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer,
    const gfx::Size& new_window_size_dip) {
  CHECK(false) << "This can only be called when recording a window";
  return false;
}

bool VideoCaptureParams::OnFrameSinkSizeChanged(
    mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer,
    const gfx::Size& new_frame_sink_size_dip,
    float new_device_scale_factor) {
  if (current_frame_sink_size_dips_ == new_frame_sink_size_dip &&
      current_device_scale_factor_ == new_device_scale_factor) {
    return false;
  }

  current_frame_sink_size_dips_ = new_frame_sink_size_dip;
  current_device_scale_factor_ = new_device_scale_factor;
  current_frame_sink_size_pixels_ = CalculateFrameSinkSizeInPixels();

  return OnVideoSizeMayHaveChanged(capturer);
}

VideoCaptureParams::VideoCaptureParams(viz::FrameSinkId frame_sink_id,
                                       viz::SubtreeCaptureId subtree_capture_id,
                                       const gfx::Size& current_frame_sink_size,
                                       float device_scale_factor)
    : frame_sink_id_(frame_sink_id),
      subtree_capture_id_(subtree_capture_id),
      current_frame_sink_size_dips_(current_frame_sink_size),
      current_device_scale_factor_(device_scale_factor),
      current_frame_sink_size_pixels_(CalculateFrameSinkSizeInPixels()) {
  DCHECK(frame_sink_id_.is_valid());
}

gfx::Size VideoCaptureParams::CalculateFrameSinkSizeInPixels() const {
  return GetSizeInPixels(current_frame_sink_size_dips_,
                         current_device_scale_factor_);
}

}  // namespace recording
