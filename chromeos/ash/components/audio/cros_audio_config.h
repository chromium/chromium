// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUDIO_CROS_AUDIO_CONFIG_H_
#define CHROMEOS_ASH_COMPONENTS_AUDIO_CROS_AUDIO_CONFIG_H_

#include "base/component_export.h"
#include "chromeos/ash/components/audio/public/mojom/cros_audio_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::audio_config {

// Implements the CrosAudioConfig API, which will support Audio system UI on
// Chrome OS, providing and allowing configuration of system audio settings.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_AUDIO) CrosAudioConfig
    : public mojom::CrosAudioConfig {
 public:
  CrosAudioConfig(const CrosAudioConfig&) = delete;
  CrosAudioConfig& operator=(const CrosAudioConfig&) = delete;
  ~CrosAudioConfig() override;

  void BindPendingReceiver(
      mojo::PendingReceiver<mojom::CrosAudioConfig> pending_receiver);

 protected:
  CrosAudioConfig();

  void NotifyObserversAudioSystemPropertiesChanged();

  virtual uint8_t GetOutputVolumePercent() const = 0;
  virtual uint8_t GetInputGainPercent() const = 0;
  virtual mojom::MuteState GetOutputMuteState() const = 0;
  virtual void GetAudioDevices(
      std::vector<mojom::AudioDevicePtr>* output_devices_out,
      std::vector<mojom::AudioDevicePtr>* input_devices_out) const = 0;
  virtual mojom::MuteState GetInputMuteState() const = 0;

 private:
  // mojom::CrosAudioConfig:
  void ObserveAudioSystemProperties(
      mojo::PendingRemote<mojom::AudioSystemPropertiesObserver> observer)
      override;

  mojom::AudioSystemPropertiesPtr GetAudioSystemProperties();

  mojo::ReceiverSet<mojom::CrosAudioConfig> receivers_;
  mojo::RemoteSet<mojom::AudioSystemPropertiesObserver> observers_;
};

}  // namespace ash::audio_config

#endif  // CHROMEOS_ASH_COMPONENTS_AUDIO_CROS_AUDIO_CONFIG_H_
