// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_INTERNAL_CAMERA_SERVICE_CONNECTOR_H_
#define CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_INTERNAL_CAMERA_SERVICE_CONNECTOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/capture_mode/camera_video_frame_handler.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"
#include "media/capture/video/video_capture_device_info.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"

namespace ash::kiosk_vision {

inline constexpr int kRequestedFormatWidth = 569;
inline constexpr int kRequestedFormatHeight = 320;
inline constexpr float kMinimumFrameRate = 6.0f;

// Binds a remote to `CrosCameraService` and  starts the camera video stream to
// be able to use the camera. When the video stream is establish, the class
// registers the given `observer` for detection events.
class CameraServiceConnector
    : public capture_mode::CameraVideoFrameHandler::Delegate {
 public:
  enum Status {
    kNotStarted = 0,
    kStarted = 1,
    kCameraNotConnected = 2,
    kCameraHasNoSupportedFormats = 3,
    kVideoStreamStarted = 4,
    kFatalErrorOrDisconnection = 5,
    kMaxValue = kFatalErrorOrDisconnection,
  };

  // To start the video stream and detections, call `Start()`.
  CameraServiceConnector(const std::string& dlc_path,
                         cros::mojom::KioskVisionObserver* observer);
  CameraServiceConnector(const CameraServiceConnector&) = delete;
  CameraServiceConnector& operator=(const CameraServiceConnector&) = delete;
  ~CameraServiceConnector() override;

  Status status() const { return status_; }

  void Start();

  // capture_mode::CameraVideoFrameHandler::Delegate:
  void OnCameraVideoFrame(scoped_refptr<media::VideoFrame> frame) override;
  void OnFatalErrorOrDisconnection() override;

 private:
  // Asks ChromeOS Camera service to start the kiosk vision detections.
  void StartKioskVisionDetection();

  // Called to connect to the video capture services's video source provider for
  // the first time, or when the connection to it is lost. It also queries the
  // list of currently available cameras by calling the below
  // `GetCameraDevices()`.
  void ReconnectToVideoSourceProvider();

  // Retrieves the list of currently available cameras from
  // `video_source_provider_remote_`.
  void GetCameraDevices();

  // Called back asynchronously by the video source provider to give us the list
  // of currently available camera `devices`.
  void OnCameraDevicesReceived(
      video_capture::mojom::VideoSourceProvider::GetSourceInfosResult,
      const std::vector<media::VideoCaptureDeviceInfo>& devices);

  const std::string dlc_path_;
  mojo::Receiver<cros::mojom::KioskVisionObserver> receiver_;
  mojo::Remote<cros::mojom::CrosCameraService> camera_service_;

  // The remote end to the video source provider that exists in the video
  // capture service.
  mojo::Remote<video_capture::mojom::VideoSourceProvider>
      video_source_provider_remote_;
  std::unique_ptr<capture_mode::CameraVideoFrameHandler> video_frame_handler_;
  Status status_ = kNotStarted;

  // `base::WeakPtrFactory` must be the last field so it's destroyed first.
  base::WeakPtrFactory<CameraServiceConnector> weak_ptr_factory_{this};
};

}  // namespace ash::kiosk_vision

#endif  // CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_INTERNAL_CAMERA_SERVICE_CONNECTOR_H_
