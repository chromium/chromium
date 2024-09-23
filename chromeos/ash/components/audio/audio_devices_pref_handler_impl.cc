// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_devices_pref_handler_impl.h"

#include <stdint.h>

#include <algorithm>
#include <optional>
#include <unordered_set>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/audio_device_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash {
namespace {

// Values used for muted preference.
const int kPrefMuteOff = 0;
const int kPrefMuteOn = 1;

// Prefs keys.
const char kActiveKey[] = "active";
const char kActivateByUserKey[] = "activate_by_user";

// Migrates an entry associated with |device|'s v1 stable device ID in
// |settings| to the key derived from |device|'s v2 stable device ID
// (which is expected to be equal to |intended_key|), if the entry can
// be found.
// Returns whether the migration occurred.
bool MigrateDeviceIdInSettings(base::Value::Dict* settings,
                               const std::string& intended_key,
                               const AudioDevice& device) {
  if (device.stable_device_id_version == 1)
    return false;

  DCHECK_EQ(2, device.stable_device_id_version);

  std::string old_device_id = GetVersionedDeviceIdString(device, 1);
  std::optional<base::Value> value = settings->Extract(old_device_id);
  if (!value)
    return false;

  DCHECK_EQ(intended_key, GetDeviceIdString(device));
  settings->SetByDottedPath(intended_key, std::move(*value));
  return true;
}

}  // namespace

double AudioDevicesPrefHandlerImpl::GetOutputVolumeValue(
    const AudioDevice* device) {
  if (!device)
    return kDefaultOutputVolumePercent;
  else
    return GetOutputVolumePrefValue(*device);
}

double AudioDevicesPrefHandlerImpl::GetInputGainValue(
    const AudioDevice* device) {
  DCHECK(device);
  return GetInputGainPrefValue(*device);
}

void AudioDevicesPrefHandlerImpl::SetVolumeGainValue(const AudioDevice& device,
                                                     double value) {
  // TODO(baileyberro): Refactor public interface to use two explicit methods.
  device.is_input ? SetInputGainPrefValue(device, value)
                  : SetOutputVolumePrefValue(device, value);
}

void AudioDevicesPrefHandlerImpl::SetOutputVolumePrefValue(
    const AudioDevice& device,
    double value) {
  DCHECK(!device.is_input);
  // Use this opportunity to remove device record under deprecated device ID,
  // if one exists.
  if (device.stable_device_id_version == 2) {
    std::string old_device_id = GetVersionedDeviceIdString(device, 1);
    device_volume_settings_.Remove(old_device_id);
  }
  device_volume_settings_.Set(GetDeviceIdString(device), value);

  SaveDevicesVolumePref();
}

void AudioDevicesPrefHandlerImpl::SetInputGainPrefValue(
    const AudioDevice& device,
    double value) {
  DCHECK(device.is_input);

  const std::string device_id = GetDeviceIdString(device);

  // Use this opportunity to remove input device record from
  // |device_volume_settings_|.
  // TODO(baileyberro): Remove this check in M94.
  if (device_volume_settings_.Find(device_id)) {
    device_volume_settings_.Remove(device_id);
    SaveDevicesVolumePref();
  }

  device_gain_settings_.Set(device_id, value);
  SaveDevicesGainPref();
}

bool AudioDevicesPrefHandlerImpl::GetMuteValue(const AudioDevice& device) {
  std::string device_id_str = GetDeviceIdString(device);
  if (!device_mute_settings_.Find(device_id_str))
    MigrateDeviceMuteSettings(device_id_str, device);

  int mute =
      device_mute_settings_.FindInt(device_id_str).value_or(kPrefMuteOff);
  return (mute == kPrefMuteOn);
}

void AudioDevicesPrefHandlerImpl::SetMuteValue(const AudioDevice& device,
                                               bool mute) {
  // Use this opportunity to remove device record under deprecated device ID,
  // if one exists.
  if (device.stable_device_id_version == 2) {
    std::string old_device_id = GetVersionedDeviceIdString(device, 1);
    device_mute_settings_.Remove(old_device_id);
  }
  device_mute_settings_.Set(GetDeviceIdString(device),
                            mute ? kPrefMuteOn : kPrefMuteOff);
  SaveDevicesMutePref();
}

void AudioDevicesPrefHandlerImpl::SetDeviceActive(const AudioDevice& device,
                                                  bool active,
                                                  bool activate_by_user) {
  base::Value::Dict dict;
  dict.Set(kActiveKey, active);
  if (active)
    dict.Set(kActivateByUserKey, activate_by_user);

  // Use this opportunity to remove device record under deprecated device ID,
  // if one exists.
  if (device.stable_device_id_version == 2) {
    std::string old_device_id = GetVersionedDeviceIdString(device, 1);
    device_state_settings_.Remove(old_device_id);
  }
  device_state_settings_.SetByDottedPath(GetDeviceIdString(device),
                                         std::move(dict));
  SaveDevicesStatePref();
}

bool AudioDevicesPrefHandlerImpl::GetDeviceActive(const AudioDevice& device,
                                                  bool* active,
                                                  bool* activate_by_user) {
  const std::string device_id_str = GetDeviceIdString(device);
  if (!device_state_settings_.Find(device_id_str) &&
      !MigrateDevicesStatePref(device_id_str, device)) {
    return false;
  }

  base::Value::Dict* dict =
      device_state_settings_.FindDictByDottedPath(device_id_str);
  if (!dict) {
    LOG(ERROR) << "Could not get device state for device:" << device.ToString();
    return false;
  }

  std::optional<bool> active_opt = dict->FindBool(kActiveKey);
  if (!active_opt.has_value()) {
    LOG(ERROR) << "Could not get active value for device:" << device.ToString();
    return false;
  }

  *active = active_opt.value();
  if (!*active)
    return true;

  std::optional<bool> activate_by_user_opt = dict->FindBool(kActivateByUserKey);
  if (!activate_by_user_opt.has_value()) {
    LOG(ERROR) << "Could not get activate_by_user value for previously "
                  "active device:"
               << device.ToString();
    return false;
  }

  *activate_by_user = activate_by_user_opt.value();
  return true;
}

void AudioDevicesPrefHandlerImpl::SetUserPriorityHigherThan(
    const AudioDevice& target,
    const AudioDevice* base) {
  int t = GetUserPriority(target);
  int b = 0;
  if (base) {
    b = GetUserPriority(*base);
  }

  // Don't need to update the user priority of `target` if it's already has
  // higher priority than base.
  if (t > b)
    return;

  auto target_id = GetDeviceIdString(target);
  base::Value::Dict& priority_prefs =
      target.is_input ? input_device_user_priority_settings_
                      : output_device_user_priority_settings_;

  if (t != kUserPriorityNone) {
    // before: [. . . t - - - b . . .]
    // after:  [. . . - - - b t . . .]
    for (auto it : priority_prefs) {
      if (it.second.GetInt() > t && it.second.GetInt() <= b)
        it.second = base::Value(it.second.GetInt() - 1);
    }
    priority_prefs.Set(target_id, b);
  } else {
    // before: [. . . b + + +]
    // after : [. . . b t + + +]
    for (auto it : priority_prefs) {
      if (it.second.GetInt() > b)
        it.second = base::Value(it.second.GetInt() + 1);
    }
    priority_prefs.Set(target_id, b + 1);
  }

  if (target.is_input) {
    SaveInputDevicesUserPriorityPref();
  } else {
    SaveOutputDevicesUserPriorityPref();
  }
}

int AudioDevicesPrefHandlerImpl::GetUserPriority(const AudioDevice& device) {
  if (device.is_input) {
    return input_device_user_priority_settings_
        .FindInt(GetDeviceIdString(device))
        .value_or(kUserPriorityNone);
    ;
  } else {
    return output_device_user_priority_settings_
        .FindInt(GetDeviceIdString(device))
        .value_or(kUserPriorityNone);
    ;
  }
}

const std::optional<uint64_t>
AudioDevicesPrefHandlerImpl::GetPreferredDeviceFromPreferenceSet(
    bool is_input,
    const AudioDeviceList& devices) {
  const base::Value::Dict& device_pref_set =
      is_input ? input_device_preference_set_settings_
               : output_device_preference_set_settings_;
  const std::string ids = GetDeviceSetIdString(devices);
  const std::string* id_string = device_pref_set.FindString(ids);
  return id_string ? ParseDeviceId(*id_string) : std::nullopt;
}

void AudioDevicesPrefHandlerImpl::UpdateDevicePreferenceSet(
    const AudioDeviceList& devices,
    const AudioDevice& preferred_device) {
  // Double check that |preferred_device| exists in |devices|.
  auto it = std::find_if(
      devices.begin(), devices.end(), [&](const AudioDevice& device) {
        return device.stable_device_id == preferred_device.stable_device_id;
      });

  if (it == devices.end()) {
    LOG(ERROR)
        << "The preferred_device does not exist in the given device list. "
        << preferred_device.ToString();
    return;
  }

  bool is_input = preferred_device.is_input;
  base::Value::Dict& device_pref_set =
      is_input ? input_device_preference_set_settings_
               : output_device_preference_set_settings_;
  device_pref_set.Set(GetDeviceSetIdString(devices),
                      GetDeviceIdString(preferred_device));

  if (is_input) {
    SaveInputDevicePreferenceSetPref();
  } else {
    SaveOutputDevicePreferenceSetPref();
  }
}

const base::Value::List&
AudioDevicesPrefHandlerImpl::GetMostRecentActivatedDeviceIdList(bool is_input) {
  return is_input ? most_recent_activated_input_device_ids_
                  : most_recent_activated_output_device_ids_;
}

void AudioDevicesPrefHandlerImpl::UpdateMostRecentActivatedDeviceIdList(
    const AudioDevice& device) {
  base::Value::List& ids = device.is_input
                               ? most_recent_activated_input_device_ids_
                               : most_recent_activated_output_device_ids_;
  std::string target_device_id = GetDeviceIdString(device);
  // Find if this device is already in the list, remove it if so.
  for (auto it = ids.begin(); it != ids.end(); it++) {
    if (target_device_id == *it) {
      ids.erase(it);
      break;
    }
  }

  // Add this device to the end of the list.
  ids.Append(target_device_id);

  if (device.is_input) {
    SaveMostRecentActivatedInputDeviceIdsPref();
  } else {
    SaveMostRecentActivatedOutputDeviceIdsPref();
  }
}

void AudioDevicesPrefHandlerImpl::DropLeastRecentlySeenDevices(
    const std::vector<AudioDevice>& connected_devices,
    size_t keep_devices) {
  ScopedDictPrefUpdate last_seen_update(local_state_,
                                        prefs::kAudioDevicesLastSeen);
  base::Value::Dict& last_seen = last_seen_update.Get();

  // Set timestamp of connected devices.
  double time = base::Time::Now().InSecondsFSinceUnixEpoch();
  for (AudioDevice device : connected_devices) {
    last_seen.Set(GetDeviceIdString(device), time);
  }

  // Order devices by last seen timestamp.
  std::vector<std::string> recently_seen_ids;
  for (auto device : last_seen) {
    recently_seen_ids.push_back(device.first);
  }
  std::sort(recently_seen_ids.begin(), recently_seen_ids.end(),
            [&](const std::string& i, const std::string& j) {
              // More recent device first.
              return last_seen.FindDouble(i).value_or(0) >
                     last_seen.FindDouble(j).value_or(0);
            });

  // Keep `keep_devices` recently seen devices.
  while (recently_seen_ids.size() > keep_devices &&
         last_seen.FindDouble(recently_seen_ids.back()).value_or(0) != time) {
    last_seen.Remove(recently_seen_ids.back());
    recently_seen_ids.pop_back();
  }

  // Remove preferences if not seen recently, keeping the most recent
  // `keep_devices` devices.
  // TODO(aaronyu): Consider also remove volume/mute/gain preferences.
  for (base::Value::Dict& settings :
       std::initializer_list<std::reference_wrapper<base::Value::Dict>>{
           input_device_user_priority_settings_,
           output_device_user_priority_settings_}) {
    std::vector<std::string> to_remove;
    for (auto entry : settings) {
      const std::string& id = entry.first;
      if (last_seen.Find(id) == nullptr) {
        to_remove.push_back(id);
      }
    }
    for (const std::string& id : to_remove) {
      settings.Remove(id);
    }
  }
  SaveInputDevicesUserPriorityPref();
  SaveOutputDevicesUserPriorityPref();
}

bool AudioDevicesPrefHandlerImpl::GetAudioOutputAllowedValue() const {
  return local_state_->GetBoolean(prefs::kAudioOutputAllowed);
}

void AudioDevicesPrefHandlerImpl::AddAudioPrefObserver(
    AudioPrefObserver* observer) {
  observers_.AddObserver(observer);
}

void AudioDevicesPrefHandlerImpl::RemoveAudioPrefObserver(
    AudioPrefObserver* observer) {
  observers_.RemoveObserver(observer);
}

double AudioDevicesPrefHandlerImpl::GetOutputVolumePrefValue(
    const AudioDevice& device) {
  DCHECK(!device.is_input);
  std::string device_id_str = GetDeviceIdString(device);
  if (!device_volume_settings_.Find(device_id_str))
    MigrateDeviceVolumeGainSettings(device_id_str, device);
  return *device_volume_settings_.FindDouble(device_id_str);
}

double AudioDevicesPrefHandlerImpl::GetInputGainPrefValue(
    const AudioDevice& device) {
  DCHECK(device.is_input);
  std::string device_id_str = GetDeviceIdString(device);
  if (!device_gain_settings_.Find(device_id_str))
    SetInputGainPrefValue(device, kDefaultInputGainPercent);
  return *device_gain_settings_.FindDouble(device_id_str);
}

double AudioDevicesPrefHandlerImpl::GetDeviceDefaultOutputVolume(
    const AudioDevice& device) {
  switch (device.type) {
    case AudioDeviceType::kBluetooth:
      return kDefaultBluetoothOutputVolumePercent;
    case AudioDeviceType::kUsb:
      return kDefaultUsbOutputVolumePercent;
    case AudioDeviceType::kHdmi:
      return kDefaultHdmiOutputVolumePercent;
    default:
      return kDefaultOutputVolumePercent;
  }
}

bool AudioDevicesPrefHandlerImpl::GetNoiseCancellationState() {
  return local_state_->GetBoolean(prefs::kInputNoiseCancellationEnabled);
}

void AudioDevicesPrefHandlerImpl::SetNoiseCancellationState(
    bool noise_cancellation_state) {
  local_state_->SetBoolean(prefs::kInputNoiseCancellationEnabled,
                           noise_cancellation_state);
}

bool AudioDevicesPrefHandlerImpl::GetStyleTransferState() const {
  return local_state_->GetBoolean(prefs::kInputStyleTransferEnabled);
}

void AudioDevicesPrefHandlerImpl::SetStyleTransferState(
    bool style_transfer_state) {
  local_state_->SetBoolean(prefs::kInputStyleTransferEnabled,
                           style_transfer_state);
}

bool AudioDevicesPrefHandlerImpl::GetForceRespectUiGainsState() {
  return local_state_->GetBoolean(prefs::kInputForceRespectUiGainsEnabled);
}

void AudioDevicesPrefHandlerImpl::SetForceRespectUiGainsState(
    bool force_respect_ui_gains_state) {
  local_state_->SetBoolean(prefs::kInputForceRespectUiGainsEnabled,
                           force_respect_ui_gains_state);
}

bool AudioDevicesPrefHandlerImpl::GetHfpMicSrState() {
  return local_state_->GetBoolean(prefs::kHandsFreeProfileInputSuperResolution);
}

void AudioDevicesPrefHandlerImpl::SetHfpMicSrState(bool hfp_mic_sr_state) {
  local_state_->SetBoolean(prefs::kHandsFreeProfileInputSuperResolution,
                           hfp_mic_sr_state);
}

AudioDevicesPrefHandlerImpl::AudioDevicesPrefHandlerImpl(
    PrefService* local_state)
    : local_state_(local_state) {
  InitializePrefObservers();

  LoadDevicesMutePref();
  LoadDevicesVolumePref();
  LoadDevicesGainPref();
  LoadInputDevicesUserPriorityPref();
  LoadOutputDevicesUserPriorityPref();

  // Reset set-based audio selection preference pref for testing purpose.
  if (features::IsResetAudioSelectionImprovementPrefEnabled()) {
    SaveDevicesStatePref();
    SaveInputDevicePreferenceSetPref();
    SaveOutputDevicePreferenceSetPref();
    SaveMostRecentActivatedInputDeviceIdsPref();
    SaveMostRecentActivatedOutputDeviceIdsPref();
  } else {
    LoadDevicesStatePref();
    LoadInputDevicePreferenceSetPref();
    LoadOutputDevicePreferenceSetPref();
    LoadMostRecentActivatedInputDeviceIdsPref();
    LoadMostRecentActivatedOutputDeviceIdsPref();
  }
}

AudioDevicesPrefHandlerImpl::~AudioDevicesPrefHandlerImpl() = default;

void AudioDevicesPrefHandlerImpl::InitializePrefObservers() {
  pref_change_registrar_.Init(local_state_);
  base::RepeatingClosure callback =
      base::BindRepeating(&AudioDevicesPrefHandlerImpl::NotifyAudioPolicyChange,
                          base::Unretained(this));
  pref_change_registrar_.Add(prefs::kAudioOutputAllowed, callback);
}

void AudioDevicesPrefHandlerImpl::LoadDevicesMutePref() {
  const base::Value::Dict& mute_prefs =
      local_state_->GetDict(prefs::kAudioDevicesMute);
  device_mute_settings_ = mute_prefs.Clone();
}

void AudioDevicesPrefHandlerImpl::SaveDevicesMutePref() {
  local_state_->SetDict(prefs::kAudioDevicesMute,
                        device_mute_settings_.Clone());
}

void AudioDevicesPrefHandlerImpl::LoadDevicesVolumePref() {
  const base::Value::Dict& volume_prefs =
      local_state_->GetDict(prefs::kAudioDevicesVolumePercent);
  device_volume_settings_ = volume_prefs.Clone();
}

void AudioDevicesPrefHandlerImpl::SaveDevicesVolumePref() {
  local_state_->SetDict(prefs::kAudioDevicesVolumePercent,
                        device_volume_settings_.Clone());
}

void AudioDevicesPrefHandlerImpl::LoadDevicesGainPref() {
  const base::Value::Dict& gain_prefs =
      local_state_->GetDict(prefs::kAudioDevicesGainPercent);
  device_gain_settings_ = gain_prefs.Clone();
}

void AudioDevicesPrefHandlerImpl::SaveDevicesGainPref() {
  local_state_->SetDict(prefs::kAudioDevicesGainPercent,
                        device_gain_settings_.Clone());
}

void AudioDevicesPrefHandlerImpl::LoadDevicesStatePref() {
  const base::Value::Dict& state_prefs =
      local_state_->GetDict(prefs::kAudioDevicesState);
  device_state_settings_ = state_prefs.Clone();
}

void AudioDevicesPrefHandlerImpl::SaveDevicesStatePref() {
  local_state_->SetDict(prefs::kAudioDevicesState,
                        device_state_settings_.Clone());
}

void AudioDevicesPrefHandlerImpl::LoadInputDevicesUserPriorityPref() {
  const base::Value::Dict& priority_prefs =
      local_state_->GetDict(prefs::kAudioInputDevicesUserPriority);
  input_device_user_priority_settings_ = priority_prefs.Clone();
}

void AudioDevicesPrefHandlerImpl::SaveInputDevicesUserPriorityPref() {
  local_state_->SetDict(prefs::kAudioInputDevicesUserPriority,
                        input_device_user_priority_settings_.Clone());
}

void AudioDevicesPrefHandlerImpl::LoadOutputDevicesUserPriorityPref() {
  const base::Value::Dict& priority_prefs =
      local_state_->GetDict(prefs::kAudioOutputDevicesUserPriority);
  output_device_user_priority_settings_ = priority_prefs.Clone();
}

void AudioDevicesPrefHandlerImpl::SaveOutputDevicesUserPriorityPref() {
  local_state_->SetDict(prefs::kAudioOutputDevicesUserPriority,
                        output_device_user_priority_settings_.Clone());
}

void AudioDevicesPrefHandlerImpl::LoadInputDevicePreferenceSetPref() {
  const base::Value::Dict& preference_set_prefs =
      local_state_->GetDict(prefs::kAudioInputDevicePreferenceSet);
  input_device_preference_set_settings_ = preference_set_prefs.Clone();
}

void AudioDevicesPrefHandlerImpl::SaveInputDevicePreferenceSetPref() {
  local_state_->SetDict(prefs::kAudioInputDevicePreferenceSet,
                        input_device_preference_set_settings_.Clone());
}

void AudioDevicesPrefHandlerImpl::LoadOutputDevicePreferenceSetPref() {
  const base::Value::Dict& preference_set_prefs =
      local_state_->GetDict(prefs::kAudioOutputDevicePreferenceSet);
  output_device_preference_set_settings_ = preference_set_prefs.Clone();
}

void AudioDevicesPrefHandlerImpl::SaveMostRecentActivatedInputDeviceIdsPref() {
  local_state_->SetList(prefs::kAudioMostRecentActivatedInputDeviceIds,
                        most_recent_activated_input_device_ids_.Clone());
}

void AudioDevicesPrefHandlerImpl::LoadMostRecentActivatedInputDeviceIdsPref() {
  const base::Value::List& id_list_pref =
      local_state_->GetList(prefs::kAudioMostRecentActivatedInputDeviceIds);
  most_recent_activated_input_device_ids_ = id_list_pref.Clone();
}

void AudioDevicesPrefHandlerImpl::SaveMostRecentActivatedOutputDeviceIdsPref() {
  local_state_->SetList(prefs::kAudioMostRecentActivatedOutputDeviceIds,
                        most_recent_activated_output_device_ids_.Clone());
}

void AudioDevicesPrefHandlerImpl::LoadMostRecentActivatedOutputDeviceIdsPref() {
  const base::Value::List& id_list_pref =
      local_state_->GetList(prefs::kAudioMostRecentActivatedOutputDeviceIds);
  most_recent_activated_output_device_ids_ = id_list_pref.Clone();
}

void AudioDevicesPrefHandlerImpl::SaveOutputDevicePreferenceSetPref() {
  local_state_->SetDict(prefs::kAudioOutputDevicePreferenceSet,
                        output_device_preference_set_settings_.Clone());
}

bool AudioDevicesPrefHandlerImpl::MigrateDevicesStatePref(
    const std::string& device_key,
    const AudioDevice& device) {
  if (!MigrateDeviceIdInSettings(&device_state_settings_, device_key, device)) {
    return false;
  }

  SaveDevicesStatePref();
  return true;
}

void AudioDevicesPrefHandlerImpl::MigrateDeviceMuteSettings(
    const std::string& device_key,
    const AudioDevice& device) {
  if (!MigrateDeviceIdInSettings(&device_mute_settings_, device_key, device)) {
    // If there was no recorded value for deprecated device ID, use value from
    // global mute pref.
    int old_mute = local_state_->GetInteger(prefs::kAudioMute);
    device_mute_settings_.Set(device_key, old_mute);
  }
  SaveDevicesMutePref();
}

void AudioDevicesPrefHandlerImpl::MigrateDeviceVolumeGainSettings(
    const std::string& device_key,
    const AudioDevice& device) {
  DCHECK(!device.is_input);
  if (!MigrateDeviceIdInSettings(&device_volume_settings_, device_key,
                                 device)) {
    // If there was no recorded value for deprecated device ID, use value from
    // default volume associated to device type.
    double default_volume = GetDeviceDefaultOutputVolume(device);
    device_volume_settings_.Set(device_key, default_volume);
  }
  SaveDevicesVolumePref();
}

void AudioDevicesPrefHandlerImpl::NotifyAudioPolicyChange() {
  for (auto& observer : observers_)
    observer.OnAudioPolicyPrefChanged();
}

// static
void AudioDevicesPrefHandlerImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kAudioDevicesVolumePercent);
  registry->RegisterDictionaryPref(prefs::kAudioDevicesGainPercent);
  registry->RegisterDictionaryPref(prefs::kAudioDevicesMute);
  registry->RegisterDictionaryPref(prefs::kAudioDevicesState);
  registry->RegisterBooleanPref(prefs::kInputNoiseCancellationEnabled, false);
  registry->RegisterBooleanPref(prefs::kInputStyleTransferEnabled, false);
  registry->RegisterBooleanPref(prefs::kHandsFreeProfileInputSuperResolution,
                                false);

  // Register the prefs backing the audio muting policies.
  // Policy for audio input is handled by kAudioCaptureAllowed in the Chrome
  // media system.
  registry->RegisterBooleanPref(prefs::kAudioOutputAllowed, true);

  registry->RegisterIntegerPref(prefs::kAudioMute, kPrefMuteOff);

  registry->RegisterDictionaryPref(prefs::kAudioInputDevicesUserPriority);
  registry->RegisterDictionaryPref(prefs::kAudioOutputDevicesUserPriority);

  registry->RegisterDictionaryPref(prefs::kAudioInputDevicePreferenceSet);
  registry->RegisterDictionaryPref(prefs::kAudioOutputDevicePreferenceSet);

  registry->RegisterListPref(prefs::kAudioMostRecentActivatedInputDeviceIds);
  registry->RegisterListPref(prefs::kAudioMostRecentActivatedOutputDeviceIds);

  registry->RegisterDictionaryPref(prefs::kAudioDevicesLastSeen);

  registry->RegisterBooleanPref(prefs::kInputForceRespectUiGainsEnabled, false);
}

}  // namespace ash
