// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_AUDIO_AUDIO_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_AUDIO_AUDIO_HANDLER_H_

#include <tuple>

#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/ash/audio/audio.mojom.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

class AudioHandler : public audio::mojom::PageHandler,
                     public CrasAudioHandler::AudioObserver {
 public:
  AudioHandler(mojo::PendingReceiver<audio::mojom::PageHandler> receiver,
               mojo::PendingRemote<audio::mojom::Page> page);
  AudioHandler(const AudioHandler&) = delete;
  AudioHandler& operator=(const AudioHandler&) = delete;
  ~AudioHandler() override;

  void GetAudioDeviceInfo() override;

  void GetActiveOutputDeviceName(
      GetActiveOutputDeviceNameCallback callback) override;

  void GetActiveInputDeviceName(
      GetActiveInputDeviceNameCallback callback) override;

  void OpenFeedbackDialog() override;

  void OnAudioNodesChanged() override;

  void OnOutputNodeVolumeChanged(uint64_t node_id, int volume) override;

  void OnInputNodeGainChanged(uint64_t node_id, int gain) override;

  void OnOutputMuteChanged(bool mute) override;

  void OnInputMuteChanged(
      bool mute_on,
      CrasAudioHandler::InputMuteChangeMethod method) override;

  void OnInputMutedByMicrophoneMuteSwitchChanged(bool mute) override;

  void OnActiveOutputNodeChanged() override;

  void OnActiveInputNodeChanged() override;

 private:
  void UpdateAudioDeviceInfo();

  base::ScopedObservation<CrasAudioHandler, AudioObserver> observation_{this};

  audio::mojom::DeviceDataPtr CreateDeviceData(const AudioDevice* item) const;

  std::tuple<int, bool> GetDeviceVolGain(uint64_t id, bool is_input) const;

  mojo::Remote<audio::mojom::Page> page_;
  mojo::Receiver<audio::mojom::PageHandler> receiver_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_AUDIO_AUDIO_HANDLER_H_
