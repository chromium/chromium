// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/kiosk/vision/internal/camera_service_connector.h"

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "components/capture_mode/camera_video_frame_handler.h"
#include "content/public/browser/context_factory.h"
#include "content/public/browser/video_capture_service.h"
#include "media/base/video_frame.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-forward.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"
#include "media/capture/video/video_capture_device_info.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"
#include "services/video_capture/public/mojom/video_source.mojom.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

namespace ash::kiosk_vision {

namespace {

mojo::Remote<cros::mojom::CrosCameraService> BindCameraServiceRemote() {
  mojo::Remote<cros::mojom::CrosCameraService> remote;
  ash::mojo_service_manager::GetServiceManagerProxy()->Request(
      chromeos::mojo_services::kCrosCameraService, /*timeout=*/std::nullopt,
      remote.BindNewPipeAndPassReceiver().PassPipe());
  return remote;
}

bool IsAcceptableFormat(media::VideoCaptureFormat format) {
  return (format.frame_rate >= kMinimumFrameRate) &&
         (format.frame_size.width() >= kRequestedFormatWidth) &&
         (format.frame_size.height() >= kRequestedFormatHeight);
}

bool IsCloserValue(int new_value, int current_value, int target) {
  return std::abs(new_value - target) < std::abs(current_value - target);
}

bool IsBetterFormat(media::VideoCaptureFormat new_format,
                    media::VideoCaptureFormat current_format) {
  if (!IsAcceptableFormat(new_format)) {
    return false;
  }
  if (new_format.frame_size.width() != current_format.frame_size.width()) {
    return IsCloserValue(new_format.frame_size.width(),
                         current_format.frame_size.width(),
                         kRequestedFormatWidth);
  }
  return IsCloserValue(new_format.frame_size.height(),
                       current_format.frame_size.height(),
                       kRequestedFormatHeight);
}

media::VideoCaptureFormat GetClosestVideoFormat(
    media::VideoCaptureFormats supported_formats) {
  media::VideoCaptureFormat best_format = supported_formats[0];

  for (auto format : supported_formats) {
    if (IsBetterFormat(format, best_format)) {
      best_format = format;
    }
  }

  return best_format;
}

}  // namespace

CameraServiceConnector::CameraServiceConnector(
    const std::string& dlc_path,
    cros::mojom::KioskVisionObserver* observer)
    : dlc_path_(dlc_path),
      receiver_(observer),
      camera_service_(BindCameraServiceRemote()) {}

CameraServiceConnector::~CameraServiceConnector() {
  if (video_frame_handler_) {
    // Close frame handling and move the object to another thread to allow it
    // to finish processing frames that are in progress. If this isn't done,
    // then allocated buffers can be left dangling until the video stream is
    // stopped.
    auto* handler_ptr = video_frame_handler_.get();
    std::exchange(handler_ptr, nullptr)
        ->Close(base::DoNothingWithBoundArgs(
            std::move(video_source_provider_remote_),
            std::move(video_frame_handler_)));
  }
}

void CameraServiceConnector::Start() {
  status_ = Status::kStarted;
  StartKioskVisionDetection();
  ReconnectToVideoSourceProvider();
}

void CameraServiceConnector::OnCameraVideoFrame(
    scoped_refptr<media::VideoFrame> frame) {
  // We do not use the frames, this function is required to start the video
  // stream.
}

void CameraServiceConnector::OnFatalErrorOrDisconnection() {
  status_ = Status::kFatalErrorOrDisconnection;
  LOG(ERROR) << "Fatal error occurred during the camera video streaming";
  video_frame_handler_.reset();
}

void CameraServiceConnector::ReconnectToVideoSourceProvider() {
  video_source_provider_remote_.reset();
  content::GetVideoCaptureService().ConnectToVideoSourceProvider(
      video_source_provider_remote_.BindNewPipeAndPassReceiver());
  video_source_provider_remote_.set_disconnect_handler(
      base::BindOnce(&CameraServiceConnector::ReconnectToVideoSourceProvider,
                     base::Unretained(this)));
  GetCameraDevices();
}

void CameraServiceConnector::GetCameraDevices() {
  CHECK(video_source_provider_remote_);
  video_source_provider_remote_->GetSourceInfos(
      base::BindOnce(&CameraServiceConnector::OnCameraDevicesReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CameraServiceConnector::OnCameraDevicesReceived(
    video_capture::mojom::VideoSourceProvider::GetSourceInfosResult,
    const std::vector<media::VideoCaptureDeviceInfo>& devices) {
  if (devices.empty()) {
    // TODO(b/339399663): report this error to telemetry API.
    status_ = Status::kCameraNotConnected;
    LOG(ERROR) << "Camera is not connected.";
    return;
  }

  media::VideoCaptureDeviceInfo selected_camera = devices[0];
  std::string device_id = selected_camera.descriptor.device_id;

  mojo::Remote<video_capture::mojom::VideoSource> camera_video_source;
  video_source_provider_remote_->GetVideoSource(
      device_id, camera_video_source.BindNewPipeAndPassReceiver());

  if (selected_camera.supported_formats.empty()) {
    status_ = Status::kCameraHasNoSupportedFormats;
    return;
  }

  media::VideoCaptureFormat video_format =
      GetClosestVideoFormat(selected_camera.supported_formats);
  video_frame_handler_ =
      std::make_unique<capture_mode::CameraVideoFrameHandler>(
          content::GetContextFactory(), std::move(camera_video_source),
          video_format, device_id);
  video_frame_handler_->StartHandlingFrames(/*delegate=*/this);
  status_ = Status::kVideoStreamStarted;
}

void CameraServiceConnector::StartKioskVisionDetection() {
  // TODO(b/335458462) Camera service runs in a separate process and a
  // disconnect may happen when it crashes. Implement a reconnect strategy. Note
  // that `ReconnectToVideoSourceProvider` will be triggered only if the video
  // stream was stopped, but we need to handle mojom disconnections here.
  camera_service_.reset_on_disconnect();
  camera_service_->StartKioskVisionDetection(
      dlc_path_, receiver_.BindNewPipeAndPassRemote());
}

}  // namespace ash::kiosk_vision
