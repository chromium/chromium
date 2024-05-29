// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_IMPL_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chromeos/ash/components/audio/audio_devices_pref_handler.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

// Class which implements AudioDevicesPrefHandler interface and register audio
// preferences as well.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_AUDIO)
    AudioDevicesPrefHandlerImpl : public AudioDevicesPrefHandler {
 public:
  // |local_state| is the device-wide preference service.
  explicit AudioDevicesPrefHandlerImpl(PrefService* local_state);

  AudioDevicesPrefHandlerImpl(const AudioDevicesPrefHandlerImpl&) = delete;
  AudioDevicesPrefHandlerImpl& operator=(const AudioDevicesPrefHandlerImpl&) =
      delete;

  // Overridden from AudioDevicesPrefHandler.
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
  int GetUserPriority(const AudioDevice& device) override;

  const std::optional<uint64_t> GetPreferredDeviceFromPreferenceSet(
      bool is_input,
      const AudioDeviceList& devices) override;

  void UpdateDevicePreferenceSet(const AudioDeviceList& devices,
                                 const AudioDevice& preferred_device) override;

  const base::Value::List& GetMostRecentActivatedDeviceIdList(
      bool is_input) override;

  void UpdateMostRecentActivatedDeviceIdList(
      const AudioDevice& device) override;

  void DropLeastRecentlySeenDevices(
      const std::vector<AudioDevice>& connected_devices,
      size_t keep_devices) override;

  bool GetNoiseCancellationState() override;
  void SetNoiseCancellationState(bool noise_cancellation_state) override;

  bool GetStyleTransferState() const override;
  void SetStyleTransferState(bool style_transfer_state) override;

  bool GetAudioOutputAllowedValue() const override;

  void AddAudioPrefObserver(AudioPrefObserver* observer) override;
  void RemoveAudioPrefObserver(AudioPrefObserver* observer) override;

  bool GetForceRespectUiGainsState() override;
  void SetForceRespectUiGainsState(bool force_respect_ui_gains) override;

  bool GetHfpMicSrState() override;
  void SetHfpMicSrState(bool hfp_mic_sr_state) override;

  // Registers volume and mute preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 protected:
  ~AudioDevicesPrefHandlerImpl() override;

 private:
  // Initializes the observers for the policy prefs.
  void InitializePrefObservers();

  // Load and save methods for the mute preferences for all devices.
  void LoadDevicesMutePref();
  void SaveDevicesMutePref();

  // Load and save methods for the gain preferences for all devices.
  void LoadDevicesGainPref();
  void SaveDevicesGainPref();

  // Load and save methods for the volume preferences for all devices.
  void LoadDevicesVolumePref();
  void SaveDevicesVolumePref();

  // Load and save methods for the active state for all devices.
  void LoadDevicesStatePref();
  void SaveDevicesStatePref();

  // Load and save methods for the user priority for all input devices.
  void LoadInputDevicesUserPriorityPref();
  void SaveInputDevicesUserPriorityPref();

  // Load and save methods for the user priority for all output devices.
  void LoadOutputDevicesUserPriorityPref();
  void SaveOutputDevicesUserPriorityPref();

  // Load and save methods for the preference set for all input devices.
  void LoadInputDevicePreferenceSetPref();
  void SaveInputDevicePreferenceSetPref();

  // Load and save methods for the preference set for all output devices.
  void LoadOutputDevicePreferenceSetPref();
  void SaveOutputDevicePreferenceSetPref();

  // Load and save methods for the most recently activated input device id list.
  void LoadMostRecentActivatedInputDeviceIdsPref();
  void SaveMostRecentActivatedInputDeviceIdsPref();

  // Load and save methods for the most recently activated output device id
  // list.
  void LoadMostRecentActivatedOutputDeviceIdsPref();
  void SaveMostRecentActivatedOutputDeviceIdsPref();

  double GetOutputVolumePrefValue(const AudioDevice& device);
  double GetInputGainPrefValue(const AudioDevice& device);
  double GetDeviceDefaultOutputVolume(const AudioDevice& device);

  void SetOutputVolumePrefValue(const AudioDevice& device, double value);
  void SetInputGainPrefValue(const AudioDevice& device, double value);

  // Migrates devices state pref for an audio device. Device settings are
  // saved under device stable device ID - this method migrates device state
  // for a device that is saved under key derived from v1 stable device ID to
  // |device_key|. Note that |device_key| should be the key derived from
  // |device|'s v2 stable device ID.
  bool MigrateDevicesStatePref(const std::string& device_key,
                               const AudioDevice& device);

  // Methods to migrate the mute and volume settings for an audio device.
  // Migration is done in the folowing way:
  //   1. If there is a setting for the device under |device_key|, do nothing.
  //      (Note that |device_key| is expected to be the key derived from
  //       |device's| v2 stable device ID).
  //   2. If there is a setting for the device under the key derived from
  //      |device|'s v1 stable device ID, move the value to |device_key|.
  //   3. If a previous global pref value exists, move it to the per device
  //      setting (under |device_key|).
  //   4. If a previous global setting is not set, use default values of
  //      mute = off and volume = 75%.
  void MigrateDeviceMuteSettings(const std::string& device_key,
                                 const AudioDevice& device);
  void MigrateDeviceVolumeGainSettings(const std::string& device_key,
                                       const AudioDevice& device);

  // Notifies the AudioPrefObserver for audio policy pref changes.
  void NotifyAudioPolicyChange();

  base::Value::Dict device_mute_settings_;
  base::Value::Dict device_volume_settings_;
  base::Value::Dict device_gain_settings_;
  base::Value::Dict device_state_settings_;
  base::Value::Dict input_device_user_priority_settings_;
  base::Value::Dict output_device_user_priority_settings_;
  base::Value::Dict input_device_preference_set_settings_;
  base::Value::Dict output_device_preference_set_settings_;
  base::Value::List most_recent_activated_input_device_ids_;
  base::Value::List most_recent_activated_output_device_ids_;

  raw_ptr<PrefService> local_state_;  // not owned

  PrefChangeRegistrar pref_change_registrar_;
  base::ObserverList<AudioPrefObserver>::Unchecked observers_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_IMPL_H_
