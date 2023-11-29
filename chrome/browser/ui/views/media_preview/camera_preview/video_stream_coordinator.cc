// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/video_stream_coordinator.h"

#include <stdlib.h>

#include <utility>

#include "chrome/browser/ui/views/media_preview/camera_preview/video_stream_view.h"
#include "content/public/browser/context_factory.h"
#include "media/capture/video_capture_types.h"
#include "ui/compositor/compositor.h"

namespace {

constexpr float kDefaultFrameRate = 24.0f;
constexpr float kDefaultAspectRatio = 16.0 / 9.0;
// Used to exclude vertical video.
constexpr float kLeastAcceptableAspectRatio = 0.99;

float GetFrameAspectRatio(const gfx::Size& frame_size) {
  return frame_size.width() / static_cast<float>(frame_size.height());
}

bool IsAcceptableFormat(const media::VideoCaptureFormat& format,
                        const float minimum_frame_rate,
                        const int view_width) {
  return format.frame_rate >= minimum_frame_rate &&
         format.frame_size.width() >= view_width &&
         GetFrameAspectRatio(format.frame_size) >= kLeastAcceptableAspectRatio;
}

// Returns true if `v2` suits more than `v1`. If both values are larger than or
// equal the `least_acceptable`, then the lower among the two values would be
// more suitable. On the other hand, if one or both the values are less than
// `least_acceptable`, then the higher among the two values would be more
// suitable.
bool SuitsMore(const float v1, const float v2, const float least_acceptable) {
  const auto lower_v = std::min(v1, v2);
  return lower_v >= least_acceptable ? v2 == lower_v : v1 == lower_v;
}

// Returns true if `other` format is better than `cur` format.
// Better here means: (1) If one format is acceptable and the other is not, then
// the acceptable is better. (2) If both formats are acceptable or both are
// unacceptable, then check their frame rate, width, and aspect ratio in that
// order, and decide which suits more.
// For suits more definition, check `SuitsMore(..)`.
bool IsBetterFormat(const media::VideoCaptureFormat& cur,
                    const media::VideoCaptureFormat& other,
                    const int view_width,
                    const float minimum_frame_rate) {
  const bool is_other_acceptable =
      IsAcceptableFormat(other, minimum_frame_rate, view_width);
  if (is_other_acceptable !=
      IsAcceptableFormat(cur, minimum_frame_rate, view_width)) {
    return is_other_acceptable;
  }

  if (cur.frame_rate != other.frame_rate) {
    return SuitsMore(cur.frame_rate, other.frame_rate, minimum_frame_rate);
  }
  if (cur.frame_size.width() != other.frame_size.width()) {
    return SuitsMore(cur.frame_size.width(), other.frame_size.width(),
                     view_width);
  }
  return SuitsMore(GetFrameAspectRatio(cur.frame_size),
                   GetFrameAspectRatio(other.frame_size), kDefaultAspectRatio);
}

// Given a list of supported formats, return the least taxing acceptable format
// if exist. If no acceptable format exist, then return the closest that exist.
// For acceptable format definition, check `IsAcceptableFormat(...)`.
// For more info about formats comparison (i.e. to decide which is more
// taxable), check `IsBetterFormat(...)`.
media::VideoCaptureFormat GetClosestVideoFormat(
    const std::vector<media::VideoCaptureFormat>& formats,
    const int view_width,
    const float minimum_frame_rate = kDefaultFrameRate) {
  const media::VideoCaptureFormat* chosen_format = nullptr;
  for (const auto& format : formats) {
    if (!chosen_format || IsBetterFormat(*chosen_format, format, view_width,
                                         minimum_frame_rate)) {
      chosen_format = &format;
    }
  }
  return chosen_format ? *chosen_format : media::VideoCaptureFormat();
}

}  // namespace

VideoStreamCoordinator::VideoStreamCoordinator(views::View& parent_view) {
  auto* video_stream_view = parent_view.AddChildView(
      std::make_unique<VideoStreamView>(kDefaultAspectRatio));

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
            GetClosestVideoFormat(supported_formats, view->width()));

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

const media::VideoCaptureFormat
VideoStreamCoordinator::GetClosestVideoFormatForTest(  // IN-TEST
    const std::vector<media::VideoCaptureFormat>& formats,
    const int view_width,
    const float minimum_frame_rate) {
  return GetClosestVideoFormat(formats, view_width, minimum_frame_rate);
}
