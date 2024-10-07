// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_VIDEO_STREAM_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_VIDEO_STREAM_COORDINATOR_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/media_preview/media_preview_metrics.h"
#include "components/capture_mode/camera_video_frame_handler.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"
#include "ui/views/view_observer.h"

class VideoStreamView;

namespace views {
class Throbber;
}  // namespace views

// Sets up, updates and maintains the lifetime of the VideoStreamView.
// The view controller layer would be very thin so it is combined with the
// coordinator for the VideoStreamView.
class VideoStreamCoordinator
    : public capture_mode::CameraVideoFrameHandler::Delegate,
      views::ViewObserver {
 public:
  // VideoStreamView is added to `parent_view` children list.
  explicit VideoStreamCoordinator(
      views::View& parent_view,
      media_preview_metrics::Context metrics_context);
  VideoStreamCoordinator(const VideoStreamCoordinator&) = delete;
  VideoStreamCoordinator& operator=(const VideoStreamCoordinator&) = delete;
  ~VideoStreamCoordinator() override;

  // Initializes CameraVideoFrameHandler, and request to start receiving frames.
  void ConnectToDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojo::Remote<video_capture::mojom::VideoSource> video_source);

  // Stops active VideoSource connection.
  void Stop();

  // Stops active VideoSource connection. As to guarantee that
  // VideoSourceProvider outlive its VideoSource connection, it is passed in
  // here to protect from destruction.
  void StopAndCleanup(mojo::Remote<video_capture::mojom::VideoSourceProvider>
                          video_source_provider);

  // capture_mode::CameraVideoFrameHandler::Delegate implementation.
  void OnCameraVideoFrame(scoped_refptr<media::VideoFrame> frame) override;
  void OnFatalErrorOrDisconnection() override;

  void OnPermissionChange(bool has_permission);

  void SetFrameReceivedCallbackForTest(base::RepeatingClosure callback) {
    frame_received_callback_for_test_ = std::move(callback);
  }

  VideoStreamView* GetVideoStreamView() { return video_stream_view_; }

  // ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override;
  void OnViewBoundsChanged(views::View* observed_view) override;

  // Called when the preview is about to close. This happens in:
  // (1) Permission prompt when the user allows camera permission.
  // (2) Page info camera subpage on destruction when there are no active
  // devices.
  void OnClosing();

 private:
  // Input parameters for ConnectToDevice() method.
  struct ConnectToDeviceParams {
    ConnectToDeviceParams(
        const media::VideoCaptureDeviceInfo& device_info,
        mojo::Remote<video_capture::mojom::VideoSource> video_source);
    ~ConnectToDeviceParams();

    ConnectToDeviceParams(const ConnectToDeviceParams&) = delete;
    ConnectToDeviceParams& operator=(const ConnectToDeviceParams& rules) =
        delete;

    const media::VideoCaptureDeviceInfo device_info;
    mojo::Remote<video_capture::mojom::VideoSource> video_source;
  };

  void StopInternal(mojo::Remote<video_capture::mojom::VideoSourceProvider>
                        video_source_provider = {});

  void OnReceivedFirstFrame();

  void RecordVideoStreamMetrics(size_t rendered_frame_count);

  raw_ptr<VideoStreamView> video_stream_view_;
  raw_ptr<views::View> preview_badge_view_;
  raw_ptr<views::Throbber> throbber_;
  std::unique_ptr<capture_mode::CameraVideoFrameHandler> video_frame_handler_;

  // Runs when a new frame is received. Used for testing.
  base::RepeatingClosure frame_received_callback_for_test_;

  const media_preview_metrics::Context metrics_context_;
  size_t video_stream_total_frames_ = 0;
  const base::TimeTicks video_stream_construction_time_;
  std::optional<base::TimeTicks> video_stream_request_time_;
  std::optional<base::TimeTicks> video_stream_start_time_;
  base::TimeDelta total_visible_preview_duration_;
  std::optional<base::TimeDelta> time_to_action_without_preview_;

  bool has_permission_ = false;
  bool has_requested_any_video_feed_ = false;

  std::optional<ConnectToDeviceParams> connect_to_device_params_;
  base::ScopedObservation<views::View, views::ViewObserver> scoped_observation_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_VIDEO_STREAM_COORDINATOR_H_
