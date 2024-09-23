// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_CAMERA_MEDIATOR_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_CAMERA_MEDIATOR_H_

#include <string>
#include <vector>

#include "base/scoped_observation.h"
#include "components/media_effects/media_device_info.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"

// Handles interactions with the backend services for the coordinator.
class CameraMediator : public media_effects::MediaDeviceInfo::Observer {
 public:
  using DevicesChangedCallback = base::RepeatingCallback<void(
      const std::vector<media::VideoCaptureDeviceInfo>& device_infos)>;

  explicit CameraMediator(PrefService& prefs,
                          DevicesChangedCallback devices_changed_callback);
  CameraMediator(const CameraMediator&) = delete;
  CameraMediator& operator=(const CameraMediator&) = delete;
  ~CameraMediator() override;

  // Connects VideoSource receiver to a particular camera. This connection is
  // used later to subscribe to the camera feed.
  void BindVideoSource(
      const std::string& device_id,
      mojo::PendingReceiver<video_capture::mojom::VideoSource> source_receiver);

  void InitializeDeviceList();

  // Passes ownership of the `video_source_provider_` to the caller.
  mojo::Remote<video_capture::mojom::VideoSourceProvider>
  TakeVideoSourceProvider() {
    return std::move(video_source_provider_);
  }

  bool IsDeviceListInitialized() const { return is_device_list_initialized_; }

 private:
  // media_effects::MediaDeviceInfo::Observer overrides.
  void OnVideoDevicesChanged(
      const std::optional<std::vector<media::VideoCaptureDeviceInfo>>&
          device_infos) override;

  mojo::Remote<video_capture::mojom::VideoSourceProvider>
      video_source_provider_;

  raw_ptr<PrefService> prefs_;
  DevicesChangedCallback devices_changed_callback_;
  base::ScopedObservation<media_effects::MediaDeviceInfo, CameraMediator>
      devices_observer_{this};

  bool is_device_list_initialized_ = false;

  base::WeakPtrFactory<CameraMediator> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_CAMERA_MEDIATOR_H_
