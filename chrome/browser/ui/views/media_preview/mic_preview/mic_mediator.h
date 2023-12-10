// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MIC_PREVIEW_MIC_MEDIATOR_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MIC_PREVIEW_MIC_MEDIATOR_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/system/system_monitor.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/mojom/system_info.mojom.h"

// Handles interactions with the backend services for the coordinator.
class MicMediator : public base::SystemMonitor::DevicesChangedObserver {
 public:
  using DevicesChangedCallback = base::RepeatingCallback<void(
      const std::vector<media::AudioDeviceDescription>& device_infos)>;

  explicit MicMediator(DevicesChangedCallback devices_changed_callback);
  MicMediator(const MicMediator&) = delete;
  MicMediator& operator=(const MicMediator&) = delete;
  ~MicMediator() override;

  // Used to get mic format info (i.e sample rate), which is needed for the live
  // feed.
  void GetAudioInputDeviceFormats(
      const std::string& device_id,
      audio::mojom::SystemInfo::GetInputStreamParametersCallback callback);

  // Connects AudioStreamFactory receiver to AudioService.
  void BindAudioStreamFactory(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory>
          audio_stream_factory);

  // base::SystemMonitor::DevicesChangedObserver.
  void OnDevicesChanged(base::SystemMonitor::DeviceType device_type) override;

 private:
  void OnAudioSourceInfosReceived(
      const std::vector<media::AudioDeviceDescription> device_infos);

  mojo::Remote<audio::mojom::SystemInfo> system_info_;

  DevicesChangedCallback devices_changed_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MIC_PREVIEW_MIC_MEDIATOR_H_
