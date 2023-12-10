// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_CAMERA_MEDIATOR_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_CAMERA_MEDIATOR_H_

#include <string>
#include <vector>

#include "base/system/system_monitor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"

// Handles interactions with the backend services for the coordinator.
class CameraMediator : public base::SystemMonitor::DevicesChangedObserver {
 public:
  using DevicesChangedCallback = base::RepeatingCallback<void(
      const std::vector<media::VideoCaptureDeviceInfo>& device_infos)>;

  explicit CameraMediator(DevicesChangedCallback devices_changed_callback);
  CameraMediator(const CameraMediator&) = delete;
  CameraMediator& operator=(const CameraMediator&) = delete;
  ~CameraMediator() override;

  // Connects VideoSource receiver to a particular camera. This connection is
  // used later to subscribe to the camera feed.
  void BindVideoSource(
      const std::string& device_id,
      mojo::PendingReceiver<video_capture::mojom::VideoSource> source_receiver);

  // base::SystemMonitor::DevicesChangedObserver.
  void OnDevicesChanged(base::SystemMonitor::DeviceType device_type) override;

  // Passes ownership of the `video_source_provider_` to the caller.
  mojo::Remote<video_capture::mojom::VideoSourceProvider>
  TakeVideoSourceProvider() {
    return std::move(video_source_provider_);
  }

 private:
  void OnVideoSourceInfosReceived(
      const std::vector<media::VideoCaptureDeviceInfo>& device_infos);

  mojo::Remote<video_capture::mojom::VideoSourceProvider>
      video_source_provider_;

  DevicesChangedCallback devices_changed_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_CAMERA_MEDIATOR_H_
