// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUDIO_CROS_AUDIO_CONFIG_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_AUDIO_CROS_AUDIO_CONFIG_IMPL_H_

#include "base/component_export.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/audio/cros_audio_config.h"

namespace ash::audio_config {

// This enum is used in histograms, do not remove/renumber entries. If you're
// adding to this enum, update the corresponding enum listing in
// tools/metrics/histograms/enums.xml.
enum class AudioMuteButtonAction {
  kMuted = 0,
  kUnmuted = 1,
  kMaxValue = kUnmuted,
};

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_AUDIO) CrosAudioConfigImpl
    : public CrosAudioConfig,
      public CrasAudioHandler::AudioObserver {
 public:
  CrosAudioConfigImpl();
  ~CrosAudioConfigImpl() override;

 private:
  // CrosAudioConfig:
  uint8_t GetOutputVolumePercent() const override;
  uint8_t GetInputGainPercent() const override;
  mojom::MuteState GetOutputMuteState() const override;
  void GetAudioDevices(
      std::vector<mojom::AudioDevicePtr>* output_devices_out,
      std::vector<mojom::AudioDevicePtr>* input_devices_out) const override;
  mojom::MuteState GetInputMuteState() const override;
  void SetOutputMuted(bool muted) override;
  void SetOutputVolumePercent(int8_t volume) override;
  void SetInputGainPercent(uint8_t gain) override;
  void SetActiveDevice(uint64_t device_id) override;
  void SetInputMuted(bool muted) override;
  void SetNoiseCancellationEnabled(bool enabled) override;

  // CrasAudioHandler::AudioObserver:
  void OnOutputNodeVolumeChanged(uint64_t node_id, int volume) override;
  void OnInputNodeGainChanged(uint64_t node_id, int gain) override;
  void OnOutputMuteChanged(bool mute_on) override;
  void OnAudioNodesChanged() override;
  void OnActiveOutputNodeChanged() override;
  void OnActiveInputNodeChanged() override;
  void OnInputMuteChanged(
      bool mute_on,
      CrasAudioHandler::InputMuteChangeMethod method) override;
  void OnInputMutedByMicrophoneMuteSwitchChanged(bool muted) override;
  void OnNoiseCancellationStateChanged() override;
};

}  // namespace ash::audio_config

#endif  // CHROMEOS_ASH_COMPONENTS_AUDIO_CROS_AUDIO_CONFIG_IMPL_H_
