// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_devices_pref_handler_impl.h"

#include <stdint.h>

#include <algorithm>
#include <unordered_set>

#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/audio/audio_device.h"
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

// Gets the device id string for storing audio preference. The format of
// device string is a string consisting of 3 parts:
// |version of stable device ID| :
// |integer from lower 32 bit of device id| :
// |0(output device) or 1(input device)|
// If an audio device has both integrated input and output devices, the first 2
// parts of the string could be identical, only the last part will differentiate
// them.
// Note that |version of stable device ID| is present only for devices with
// stable device ID version >= 2. For devices with version 1, the device id
// string contains only latter 2 parts - in order to preserve backward
// compatibility with existing ID from before v2 stable device ID was
// introduced.
std::string GetVersionedDeviceIdString(const AudioDevice& device, int version) {
  CHECK(device.stable_device_id_version >= version);
  DCHECK_GE(device.stable_device_id_version, 1);
  DCHECK_LE(device.stable_device_id_version, 2);

  bool use_deprecated_id = version == 1 && device.stable_device_id_version == 2;
  uint64_t stable_device_id = use_deprecated_id
                                  ? device.deprecated_stable_device_id
                                  : device.stable_device_id;
  std::string version_prefix = version == 2 ? "2 : " : "";
  std::string device_id_string =
      version_prefix +
      base::NumberToString(stable_device_id &
                           static_cast<uint64_t>(0xffffffff)) +
      " : " + (device.is_input ? "1" : "0");
  // Replace any periods from the device id string with a space, since setting
  // names cannot contain periods.
  std::replace(device_id_string.begin(), device_id_string.end(), '.', ' ');
  return device_id_string;
}

std::string GetDeviceIdString(const AudioDevice& device) {
  return GetVersionedDeviceIdString(device, device.stable_device_id_version);
}

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
  absl::optional<base::Value> value = settings->Extract(old_device_id);
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

  absl::optional<bool> active_opt = dict->FindBool(kActiveKey);
  if (!active_opt.has_value()) {
    LOG(ERROR) << "Could not get active value for device:" << device.ToString();
    return false;
  }

  *active = active_opt.value();
  if (!*active)
    return true;

  absl::optional<bool> activate_by_user_opt =
      dict->FindBool(kActivateByUserKey);
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

void AudioDevicesPrefHandlerImpl::DropLeastRecentlySeenDevices(
    const std::vector<AudioDevice>& connected_devices,
    size_t keep_devices) {
  ScopedDictPrefUpdate last_seen_update(local_state_,
                                        prefs::kAudioDevicesLastSeen);
  base::Value::Dict& last_seen = last_seen_update.Get();

  // Set timestamp of connected devices.
  double time = base::Time::Now().ToDoubleT();
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
  if (device.type == AudioDeviceType::kHdmi)
    return kDefaultHdmiOutputVolumePercent;
  else
    return kDefaultOutputVolumePercent;
}

bool AudioDevicesPrefHandlerImpl::GetNoiseCancellationState() {
  return local_state_->GetBoolean(prefs::kInputNoiseCancellationEnabled);
}

void AudioDevicesPrefHandlerImpl::SetNoiseCancellationState(
    bool noise_cancellation_state) {
  local_state_->SetBoolean(prefs::kInputNoiseCancellationEnabled,
                           noise_cancellation_state);
}

AudioDevicesPrefHandlerImpl::AudioDevicesPrefHandlerImpl(
    PrefService* local_state)
    : local_state_(local_state) {
  InitializePrefObservers();

  LoadDevicesMutePref();
  LoadDevicesVolumePref();
  LoadDevicesGainPref();
  LoadDevicesStatePref();
  LoadInputDevicesUserPriorityPref();
  LoadOutputDevicesUserPriorityPref();
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
    // global vloume pref.
    double old_volume = local_state_->GetDouble(prefs::kAudioVolumePercent);
    device_volume_settings_.Set(device_key, old_volume);
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

  // Register the prefs backing the audio muting policies.
  // Policy for audio input is handled by kAudioCaptureAllowed in the Chrome
  // media system.
  registry->RegisterBooleanPref(prefs::kAudioOutputAllowed, true);

  // Register the legacy audio prefs for migration.
  registry->RegisterDoublePref(prefs::kAudioVolumePercent,
                               kDefaultOutputVolumePercent);
  registry->RegisterIntegerPref(prefs::kAudioMute, kPrefMuteOff);

  registry->RegisterDictionaryPref(prefs::kAudioInputDevicesUserPriority);
  registry->RegisterDictionaryPref(prefs::kAudioOutputDevicesUserPriority);

  registry->RegisterDictionaryPref(prefs::kAudioDevicesLastSeen);
}

}  // namespace ash
