// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_RECORDING_VIDEO_CAPTURE_PARAMS_H_
#define CHROMEOS_ASH_SERVICES_RECORDING_VIDEO_CAPTURE_PARAMS_H_

#include <memory>

#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "media/base/video_types.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom-forward.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace recording {

// Encapsulates the parameters for an ongoing video capture, and knows how to
// initialize a video capturer according to the requested capture source
// (fullscreen, window, or region).
class VideoCaptureParams {
 public:
  VideoCaptureParams(const VideoCaptureParams&) = delete;
  VideoCaptureParams& operator=(const VideoCaptureParams&) = delete;
  virtual ~VideoCaptureParams() = default;

  // Returns a capture params instance for a fullscreen recording of a root
  // window which has the given |frame_sink_id|. Using the given
  // |frame_sink_size_dip| and |device_scale_factor|, the resulting video will
  // have a resolution equal to the pixel size of the recorded frame sink.
  // |frame_sink_id| must be valid.
  static std::unique_ptr<VideoCaptureParams> CreateForFullscreenCapture(
      viz::FrameSinkId frame_sink_id,
      const gfx::Size& frame_sink_size_dip,
      float device_scale_factor);

  // Returns a capture params instance for a recording of a window. The given
  // |frame_sink_id| is either of that window (if it submits compositor frames
  // independently), or of the root window it descends from (if it doesn't
  // submit its compositor frames). In the latter case, the window must be
  // identifiable by a valid |subtree_capture_id| (created by calling
  // aura::window::MakeWindowCapturable() before recording starts).
  // |window_size_dip| is the initial size of the recorded window, and
  // |frame_sink_size_dip| is the current size of the frame sink.
  // |device_scale_factor| will be used to compute and perform the capture at
  // the pixel size of the window.
  // |frame_sink_id| must be valid.
  static std::unique_ptr<VideoCaptureParams> CreateForWindowCapture(
      viz::FrameSinkId frame_sink_id,
      viz::SubtreeCaptureId subtree_capture_id,
      const gfx::Size& frame_sink_size_dip,
      float device_scale_factor,
      const gfx::Size& window_size_dip);

  // Returns a capture params instance for a recording of a partial region of a
  // root window which has the given |frame_sink_id|. Using the given
  // |frame_sink_size_dip| and |device_scale_factor|, the video will be captured
  // at a resolution equal to the size of the frame sink in pixels, but the
  // resulting video frames will be cropped to the pixel bounds that corresponds
  // to the given |crop_region_dip|. |frame_sink_id| must be valid.
  static std::unique_ptr<VideoCaptureParams> CreateForRegionCapture(
      viz::FrameSinkId frame_sink_id,
      const gfx::Size& frame_sink_size_dip,
      float device_scale_factor,
      const gfx::Rect& crop_region_dip);

  const viz::FrameSinkId& frame_sink_id() const { return frame_sink_id_; }
  float current_device_scale_factor() const {
    return current_device_scale_factor_;
  }
  gfx::Size current_frame_sink_size_pixels() const {
    return current_frame_sink_size_pixels_;
  }

  // Initializes the given `capturer` (passed by ref) according to the capture
  // parameters. The given `capturer` must be bound before calling this.
  // The given `supported_pixel_format` will be used to initialize `capturer`.
  void InitializeVideoCapturer(
      mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer,
      media::VideoPixelFormat supported_pixel_format) const;

  // Returns the bounds to which a video frame, whose
  // |original_frame_visible_rect_pixels| is given, should be cropped. If no
  // cropping is desired, |original_frame_visible_rect_pixels| is returned.
  virtual gfx::Rect GetVideoFrameVisibleRect(
      const gfx::Rect& original_frame_visible_rect_pixels) const;

  // Returns the size in pixels with which the video encoder will be
  // initialized.
  virtual gfx::Size GetVideoSize() const = 0;

  // Called when a window, being recorded by the given |capturer|, is moved to
  // a different display whose root window has the given |new_frame_sink_id|,
  // |new_frame_sink_size_dip|, and |new_device_scale_factor|.
  // The default implementation is to *crash* the service, as this is only valid
  // when recording a window.
  // Returns true if the video encoder needs to be reconfigured, which happens
  // when the pixel size of the window changes, resulting in a change in the
  // size of the video. Returns false otherwise.
  [[nodiscard]] virtual bool OnRecordedWindowChangingRoot(
      mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer,
      viz::FrameSinkId new_frame_sink_id,
      const gfx::Size& new_frame_sink_size_dip,
      float new_device_scale_factor);

  // Called when a window being recorded by the given |capturer| is resized
  // (e.g. due to snapping, maximizing, user resizing, ... etc.) to
  // |new_window_size_dip|.
  // The default implementation is to *crash* the service, as this is only valid
  // when recording a window.
  // Returns true if the video encoder needs to be reconfigured, indicating that
  // there's a change in the pixel size of the recorded window, resulting in a
  // change in the video size. False otherwise.
  [[nodiscard]] virtual bool OnRecordedWindowSizeChanged(
      mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer,
      const gfx::Size& new_window_size_dip);

  // Called when the frame sink being recorded changes its size or device scale
  // factor to |new_frame_sink_size_dip| or |new_device_scale_factor|
  // respectively. The |current_frame_sink_size_pixels_| will be updated, and
  // OnVideoSizeMayHaveChanged() will be called. Subclasses should implement
  // OnVideoSizeMayHaveChanged() to handle possible changes in the pixel size
  // of the recorded surface, and hence a change in the output video size, which
  // would require a video encoder reconfiguration.
  // Returns true if the video encoder needs to be reconfigured. False
  // otherwise.
  [[nodiscard]] bool OnFrameSinkSizeChanged(
      mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer,
      const gfx::Size& new_frame_sink_size_dip,
      float new_device_scale_factor);

 protected:
  VideoCaptureParams(viz::FrameSinkId frame_sink_id,
                     viz::SubtreeCaptureId subtree_capture_id,
                     const gfx::Size& current_frame_sink_size,
                     float device_scale_factor);

  // Sets the desired resolution constraints on the given |capturer|. Subclasses
  // should override this to request a resolution from |capturer| that matches
  // the pixel size of the recorded surface.
  virtual void SetCapturerResolutionConstraints(
      mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer) const = 0;

  // Called when an event occurs that may lead to changing the size of the
  // video (such as changing the device scale factor, or resizing a recorded
  // window). Implementations should recompute the video size, and return true
  // if there was actually a change in the video size that the video encoder
  // needs to be reconfigured. Returns false otherwise.
  virtual bool OnVideoSizeMayHaveChanged(
      mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer) = 0;

  // Computes and returns the pixel size of the frame sink according to the
  // current values of |current_frame_sink_size_dips_| and
  // |current_device_scale_factor_|.
  gfx::Size CalculateFrameSinkSizeInPixels() const;

  viz::FrameSinkId frame_sink_id_;
  const viz::SubtreeCaptureId subtree_capture_id_;
  gfx::Size current_frame_sink_size_dips_;
  float current_device_scale_factor_;
  gfx::Size current_frame_sink_size_pixels_;
};

}  // namespace recording

#endif  // CHROMEOS_ASH_SERVICES_RECORDING_VIDEO_CAPTURE_PARAMS_H_
