// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/video_stream_coordinator.h"

#include <stdlib.h>

#include <utility>

#include "chrome/browser/ui/views/media_preview/camera_preview/video_format_comparison.h"
#include "chrome/browser/ui/views/media_preview/camera_preview/video_stream_view.h"
#include "content/public/browser/context_factory.h"
#include "media/capture/video_capture_types.h"
#include "ui/compositor/compositor.h"

VideoStreamCoordinator::VideoStreamCoordinator(views::View& parent_view) {
  auto* video_stream_view =
      parent_view.AddChildView(std::make_unique<VideoStreamView>());

  video_stream_view->SetRasterContextProvider(
      content::GetContextFactory()->SharedMainThreadRasterContextProvider());

  video_stream_view_tracker_.SetView(video_stream_view);
}

VideoStreamCoordinator::~VideoStreamCoordinator() {
  Stop();
}

void VideoStreamCoordinator::ConnectToDevice(
    mojo::Remote<video_capture::mojom::VideoSource> video_source,
    const std::vector<media::VideoCaptureFormat>& supported_formats) {
  Stop();
  if (auto* view = GetVideoStreamView(); view) {
    video_frame_handler_ =
        std::make_unique<capture_mode::CameraVideoFrameHandler>(
            content::GetContextFactory(), std::move(video_source),
            video_format_comparison::GetClosestVideoFormat(supported_formats,
                                                           view->width()));

    video_frame_handler_->StartHandlingFrames(/*delegate=*/this);
  }
}

// capture_mode::CameraVideoFrameHandler::Delegate implementation.
void VideoStreamCoordinator::OnCameraVideoFrame(
    scoped_refptr<media::VideoFrame> frame) {
  if (frame_received_callback_for_test_) {
    frame_received_callback_for_test_.Run();
  }

  if (auto* view = GetVideoStreamView(); view) {
    view->ScheduleFramePaint(std::move(frame));
  }
}

void VideoStreamCoordinator::OnFatalErrorOrDisconnection() {
  // When called, `video_frame_handler_` is no longer valid.
  video_frame_handler_.reset();
  if (auto* view = GetVideoStreamView(); view) {
    view->ClearFrame();
  }
}

void VideoStreamCoordinator::StopAndCleanup(
    mojo::Remote<video_capture::mojom::VideoSourceProvider>
        video_source_provider) {
  StopInternal(std::move(video_source_provider));
}

void VideoStreamCoordinator::Stop() {
  StopInternal();
}

void VideoStreamCoordinator::StopInternal(
    mojo::Remote<video_capture::mojom::VideoSourceProvider>
        video_source_provider) {
  if (video_frame_handler_) {
    // Close frame handling and move the object to another thread to allow it
    // to finish processing frames that are in progress. If this isn't done,
    // then allocated buffers can be left dangling until the video stream is
    // stopped.
    auto* handler_ptr = video_frame_handler_.get();
    std::exchange(handler_ptr, nullptr)
        ->Close(base::DoNothingWithBoundArgs(std::move(video_source_provider),
                                             std::move(video_frame_handler_)));
  }

  if (auto* view = GetVideoStreamView(); view) {
    view->ClearFrame();
  }
}

VideoStreamView* VideoStreamCoordinator::GetVideoStreamView() {
  auto* view = video_stream_view_tracker_.view();
  return view ? static_cast<VideoStreamView*>(view) : nullptr;
}
