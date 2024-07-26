// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MIC_PREVIEW_MIC_MEDIATOR_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MIC_PREVIEW_MIC_MEDIATOR_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "components/media_effects/media_device_info.h"
#include "components/prefs/pref_service.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"

// Handles interactions with the backend services for the coordinator.
class MicMediator : public media_effects::MediaDeviceInfo::Observer {
 public:
  using DevicesChangedCallback = base::RepeatingCallback<void(
      const std::vector<media::AudioDeviceDescription>& device_infos)>;

  explicit MicMediator(PrefService& prefs,
                       DevicesChangedCallback devices_changed_callback);
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

  void InitializeDeviceList();

  bool IsDeviceListInitialized() const { return is_device_list_initialized_; }

 private:
  // media_effects::MediaDeviceInfo::Observer overrides.
  void OnAudioDevicesChanged(
      const std::optional<std::vector<media::AudioDeviceDescription>>&
          device_infos) override;

  raw_ptr<PrefService> prefs_;
  DevicesChangedCallback devices_changed_callback_;
  base::ScopedObservation<media_effects::MediaDeviceInfo, MicMediator>
      devices_observer_{this};

  bool is_device_list_initialized_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MIC_PREVIEW_MIC_MEDIATOR_H_
