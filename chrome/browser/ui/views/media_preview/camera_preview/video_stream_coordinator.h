// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_VIDEO_STREAM_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_VIDEO_STREAM_COORDINATOR_H_

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "components/capture_mode/camera_video_frame_handler.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"
#include "ui/views/view_tracker.h"

class VideoStreamView;

// Sets up, updates and maintains the lifetime of the VideoStreamView.
// The view controller layer would be very thin so it is combined with the
// coordinator for the VideoStreamView.
class VideoStreamCoordinator
    : public capture_mode::CameraVideoFrameHandler::Delegate {
 public:
  // VideoStreamView is added to `parent_view` children list.
  explicit VideoStreamCoordinator(views::View& parent_view);
  VideoStreamCoordinator(const VideoStreamCoordinator&) = delete;
  VideoStreamCoordinator& operator=(const VideoStreamCoordinator&) = delete;
  ~VideoStreamCoordinator() override;

  // Initializes CameraVideoFrameHandler, and request to start receiving frames.
  void ConnectToDevice(
      mojo::Remote<video_capture::mojom::VideoSource> video_source,
      const std::vector<media::VideoCaptureFormat>& supported_formats);

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

  void SetFrameReceivedCallbackForTest(base::RepeatingClosure callback) {
    frame_received_callback_for_test_ = std::move(callback);
  }

  const media::VideoCaptureFormat GetClosestVideoFormatForTest(
      const std::vector<media::VideoCaptureFormat>& formats,
      const int view_width,
      const float minimum_frame_rate);

 private:
  void StopInternal(mojo::Remote<video_capture::mojom::VideoSourceProvider>
                        video_source_provider = {});

  VideoStreamView* GetVideoStreamView();

  views::ViewTracker video_stream_view_tracker_;
  std::unique_ptr<capture_mode::CameraVideoFrameHandler> video_frame_handler_;

  // Runs when a new frame is received. Used for testing.
  base::RepeatingClosure frame_received_callback_for_test_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_VIDEO_STREAM_COORDINATOR_H_
