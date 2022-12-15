// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUDIO_CROS_AUDIO_CONFIG_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_AUDIO_CROS_AUDIO_CONFIG_IMPL_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/audio/cros_audio_config.h"

namespace ash::audio_config {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_AUDIO) CrosAudioConfigImpl
    : public CrosAudioConfig,
      public CrasAudioHandler::AudioObserver {
 public:
  CrosAudioConfigImpl();
  ~CrosAudioConfigImpl() override;

 private:
  // CrosAudioConfig:
  uint8_t GetOutputVolumePercent() const override;
  mojom::MuteState GetOutputMuteState() const override;
  void GetAudioDevices(
      std::vector<mojom::AudioDevicePtr>* output_devices_out) const override;
  void SetOutputVolumePercent(int8_t volume) override;

  // CrasAudioHandler::AudioObserver:
  void OnOutputNodeVolumeChanged(uint64_t node_id, int volume) override;
  void OnOutputMuteChanged(bool mute_on) override;
  void OnAudioNodesChanged() override;

  base::raw_ptr<CrasAudioHandler> audio_handler_;
};

}  // namespace ash::audio_config

#endif  // CHROMEOS_ASH_COMPONENTS_AUDIO_CROS_AUDIO_CONFIG_IMPL_H_
