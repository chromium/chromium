// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/video_stream_coordinator.h"

#include <stdlib.h>

#include <utility>

#include "chrome/browser/ui/views/media_preview/camera_preview/preview_badge.h"
#include "chrome/browser/ui/views/media_preview/camera_preview/video_format_comparison.h"
#include "chrome/browser/ui/views/media_preview/camera_preview/video_stream_view.h"
#include "chrome/browser/ui/views/media_preview/media_preview_metrics.h"
#include "content/public/browser/context_factory.h"
#include "media/capture/video_capture_types.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"

VideoStreamCoordinator::VideoStreamCoordinator(
    views::View& parent_view,
    media_preview_metrics::Context metrics_context)
    : metrics_context_(metrics_context),
      video_stream_construction_time_(base::TimeTicks::Now()) {
  auto* container = parent_view.AddChildView(std::make_unique<views::View>());
  container->SetLayoutManager(std::make_unique<views::FillLayout>());

  video_stream_view_ =
      container->AddChildView(std::make_unique<VideoStreamView>());
  scoped_observation_.Observe(video_stream_view_);

  auto* badge_holder =
      container->AddChildView(std::make_unique<views::BoxLayoutView>());
  badge_holder->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  badge_holder->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd);
  badge_holder->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  preview_badge_view_ =
      badge_holder->AddChildView(preview_badge::CreatePreviewBadge());
  preview_badge_view_->SetVisible(false);

  auto* throbber_overlay =
      container->AddChildView(std::make_unique<views::BoxLayoutView>());
  throbber_overlay->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  throbber_overlay->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  const int kThrobberDiameter = 40;
  throbber_ = throbber_overlay->AddChildView(
      std::make_unique<views::Throbber>(kThrobberDiameter));
}

VideoStreamCoordinator::~VideoStreamCoordinator() {
  Stop();

  if (has_requested_any_video_feed_) {
    media_preview_metrics::RecordTotalVisiblePreviewDuration(
        metrics_context_, total_visible_preview_duration_);

    if (video_stream_total_frames_ == 0) {
      // Only records it if never received any frames during dialog life time.
      media_preview_metrics::RecordTimeToActionWithoutPreview(
          metrics_context_,
          time_to_action_without_preview_.value_or(
              base::TimeTicks::Now() - video_stream_construction_time_));
    }
  }
}

void VideoStreamCoordinator::ConnectToDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::Remote<video_capture::mojom::VideoSource> video_source) {
  Stop();

  if (!video_source) {
    // This check is needed, because there is a chance for `video_source` to
    // become unbound, while it is deferred at `connect_to_device_params_`.
    return;
  }

  if (!video_stream_view_) {
    return;
  }

  // Wait till the preview is actually shown.
  if (video_stream_view_->width() == 0) {
    connect_to_device_params_.emplace(device_info, std::move(video_source));
    return;
  }

  // Using double the view width when choosing preferred format. This provides
  // more information to the interpolation algorithm, so scaled images appear
  // sharper.
  int requested_format_width = 2 * video_stream_view_->width();
  video_frame_handler_ =
      std::make_unique<capture_mode::CameraVideoFrameHandler>(
          content::GetContextFactory(), std::move(video_source),
          video_format_comparison::GetClosestVideoFormat(
              device_info.supported_formats, requested_format_width),
          // TODO: Add testing to check that we pass the right device id to
          // CameraVideoFrameHandler.
          device_info.descriptor.device_id);

  video_frame_handler_->StartHandlingFrames(/*delegate=*/this);

  has_requested_any_video_feed_ = true;
  video_stream_request_time_ = base::TimeTicks::Now();

  throbber_->Start();
}

// capture_mode::CameraVideoFrameHandler::Delegate implementation.
void VideoStreamCoordinator::OnCameraVideoFrame(
    scoped_refptr<media::VideoFrame> frame) {
  if (frame_received_callback_for_test_) {
    frame_received_callback_for_test_.Run();
  }

  if (video_stream_view_) {
    video_stream_view_->ScheduleFramePaint(std::move(frame));
    preview_badge_view_->SetVisible(!has_permission_);
  }

  if (!video_stream_start_time_) {
    OnReceivedFirstFrame();
    if (video_stream_view_) {
      throbber_->Stop();
    }
  }
  video_stream_total_frames_++;
}

void VideoStreamCoordinator::OnFatalErrorOrDisconnection() {
  // When called, `video_frame_handler_` is no longer valid.
  video_frame_handler_.reset();
  video_stream_start_time_.reset();
  if (video_stream_view_) {
    video_stream_view_->ClearFrame();
    preview_badge_view_->SetVisible(false);
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
  size_t rendered_frame_count = 0;
  if (video_stream_view_) {
    rendered_frame_count = video_stream_view_->GetRenderedFrameCount();
    // ClearFrame() should be called before CameraVideoFrameHandler::Close().
    // This order is needed as to clear all frame references before resetting
    // the buffers which happens within Close().
    video_stream_view_->ClearFrame();
    preview_badge_view_->SetVisible(false);
  }

  if (video_frame_handler_) {
    RecordVideoStreamMetrics(rendered_frame_count);
    // Close frame handling and move the object to another thread to allow it
    // to finish processing frames that are in progress. If this isn't done,
    // then allocated buffers can be left dangling until the video stream is
    // stopped.
    auto* handler_ptr = video_frame_handler_.get();
    std::exchange(handler_ptr, nullptr)
        ->Close(base::DoNothingWithBoundArgs(std::move(video_source_provider),
                                             std::move(video_frame_handler_)));
  }
}

void VideoStreamCoordinator::OnViewIsDeleting(views::View* observed_view) {
  CHECK(scoped_observation_.IsObservingSource(observed_view));
  scoped_observation_.Reset();
  video_stream_view_ = nullptr;
}

VideoStreamCoordinator::ConnectToDeviceParams::ConnectToDeviceParams(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::Remote<video_capture::mojom::VideoSource> video_source)
    : device_info(device_info), video_source(std::move(video_source)) {}

VideoStreamCoordinator::ConnectToDeviceParams::~ConnectToDeviceParams() =
    default;

void VideoStreamCoordinator::OnViewBoundsChanged(views::View* observed_view) {
  CHECK(scoped_observation_.IsObservingSource(observed_view));
  if (observed_view->width() > 0 && connect_to_device_params_) {
    const auto device_info = std::move(connect_to_device_params_->device_info);
    auto remote = std::move(connect_to_device_params_->video_source);
    connect_to_device_params_.reset();
    ConnectToDevice(device_info, std::move(remote));
  }
}

void VideoStreamCoordinator::OnPermissionChange(bool has_permission) {
  has_permission_ = has_permission;
}

void VideoStreamCoordinator::OnClosing() {
  // Stop the video feed once a decision is made, as to record the correct
  // duration of the feed. If we wait till destruction instead, the duration
  // will not be accurate due to some async calls.
  Stop();
  time_to_action_without_preview_ =
      base::TimeTicks::Now() - video_stream_construction_time_;
}

void VideoStreamCoordinator::OnReceivedFirstFrame() {
  CHECK(!video_stream_start_time_);
  video_stream_start_time_ = base::TimeTicks::Now();
  video_stream_total_frames_ = 0;

  CHECK(video_stream_request_time_);
  const auto preview_delay_time =
      *video_stream_start_time_ - *video_stream_request_time_;
  media_preview_metrics::RecordPreviewDelayTime(metrics_context_,
                                                preview_delay_time);
  video_stream_request_time_.reset();
}

void VideoStreamCoordinator::RecordVideoStreamMetrics(
    size_t rendered_frame_count) {
  CHECK(video_frame_handler_);
  // Retrieve the settings actually being sent by the handler. If something
  // else has the stream open when the media preview requests it, then the
  // requested settings are ignored and the existing settings are used
  // instead.
  std::optional<media::VideoCaptureParams> actual_params =
      video_frame_handler_->GetActualParams();
  if (actual_params) {
    media_preview_metrics::RecordPreviewCameraPixelHeight(
        metrics_context_, actual_params->requested_format.frame_size.height());

    media_preview_metrics::RecordPreviewVideoExpectedFPS(
        metrics_context_, actual_params->requested_format.frame_rate);
  }
  if (video_stream_start_time_) {
    const auto duration = base::TimeTicks::Now() - *video_stream_start_time_;
    total_visible_preview_duration_ += duration;
    int actual_fps = video_stream_total_frames_ / duration.InSecondsF();
    media_preview_metrics::RecordPreviewVideoActualFPS(metrics_context_,
                                                       actual_fps);
    video_stream_start_time_.reset();

    if (video_stream_view_) {
      float rendered_percent = static_cast<double>(rendered_frame_count) /
                               video_stream_total_frames_;
      media_preview_metrics::RecordPreviewVideoFramesRenderedPercent(
          metrics_context_, rendered_percent);
    }
  }
}
