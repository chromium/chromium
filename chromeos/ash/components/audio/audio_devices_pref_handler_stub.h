// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_STUB_H_
#define CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_STUB_H_

#include <stdint.h>

#include <map>

#include "base/component_export.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/audio/audio_devices_pref_handler.h"

namespace ash {

// Stub class for AudioDevicesPrefHandler, used for testing.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_AUDIO)
    AudioDevicesPrefHandlerStub : public AudioDevicesPrefHandler {
 public:
  struct DeviceState {
    bool active;
    bool activate_by_user;
  };

  using AudioDeviceMute = std::map<uint64_t, bool>;
  using AudioDeviceVolumeGain = std::map<uint64_t, int>;
  using AudioDeviceStateMap = std::map<uint64_t, DeviceState>;
  using AudioDeviceUserPriority = std::map<uint64_t, int>;
  using AudioDevicePreferenceSetMap = std::map<std::string, std::string>;
  using MostRecentActivatedDeviceIdList = base::Value::List;

  AudioDevicesPrefHandlerStub();

  AudioDevicesPrefHandlerStub(const AudioDevicesPrefHandlerStub&) = delete;
  AudioDevicesPrefHandlerStub& operator=(const AudioDevicesPrefHandlerStub&) =
      delete;

  // AudioDevicesPrefHandler:
  double GetOutputVolumeValue(const AudioDevice* device) override;
  double GetInputGainValue(const AudioDevice* device) override;
  void SetVolumeGainValue(const AudioDevice& device, double value) override;
  bool GetMuteValue(const AudioDevice& device) override;
  void SetMuteValue(const AudioDevice& device, bool mute_on) override;
  void SetDeviceActive(const AudioDevice& device,
                       bool active,
                       bool activate_by_user) override;
  bool GetDeviceActive(const AudioDevice& device,
                       bool* active,
                       bool* activate_by_user) override;
  void SetUserPriorityHigherThan(const AudioDevice& target,
                                 const AudioDevice* base) override;
  int32_t GetUserPriority(const AudioDevice& device) override;
  void DropLeastRecentlySeenDevices(
      const std::vector<AudioDevice>& connected_devices,
      size_t keep_devices) override;
  bool GetAudioOutputAllowedValue() const override;
  void AddAudioPrefObserver(AudioPrefObserver* observer) override;
  void RemoveAudioPrefObserver(AudioPrefObserver* observer) override;

  bool GetNoiseCancellationState() override;
  void SetNoiseCancellationState(bool noise_cancellation_state) override;

  bool GetStyleTransferState() const override;
  void SetStyleTransferState(bool style_transfer_state) override;

  void SetAudioOutputAllowedValue(bool is_audio_output_allowed);

  bool GetForceRespectUiGainsState() override;
  void SetForceRespectUiGainsState(bool force_respect_ui_gains) override;
  bool GetHfpMicSrState() override;
  void SetHfpMicSrState(bool hfp_mic_sr_state) override;

  const std::optional<uint64_t> GetPreferredDeviceFromPreferenceSet(
      bool is_input,
      const AudioDeviceList& devices) override;
  void UpdateDevicePreferenceSet(const AudioDeviceList& devices,
                                 const AudioDevice& preferred_device) override;

  const base::Value::List& GetMostRecentActivatedDeviceIdList(
      bool is_input) override;
  void UpdateMostRecentActivatedDeviceIdList(
      const AudioDevice& device) override;

  const AudioDevicePreferenceSetMap& GetDevicePreferenceSetMap();

 protected:
  ~AudioDevicesPrefHandlerStub() override;

 private:
  AudioDeviceMute audio_device_mute_map_;
  AudioDeviceVolumeGain audio_device_volume_gain_map_;
  AudioDeviceStateMap audio_device_state_map_;
  AudioDeviceUserPriority user_priority_map_;
  AudioDevicePreferenceSetMap device_preference_set_map_;
  MostRecentActivatedDeviceIdList most_recent_activated_device_id_list;

  base::ObserverList<AudioPrefObserver>::Unchecked observers_;

  bool is_audio_output_allowed_ = true;
  bool noise_cancellation_state_ = true;
  bool style_transfer_state_ = false;
  bool force_respect_ui_gains_ = false;
  bool hfp_mic_sr_ = false;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_STUB_H_
