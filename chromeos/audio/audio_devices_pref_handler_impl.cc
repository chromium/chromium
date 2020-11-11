// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/audio/audio_devices_pref_handler_impl.h"

#include <stdint.h>

#include <algorithm>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/audio/audio_device.h"
#include "chromeos/constants/chromeos_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

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
std::string GetVersionedDeviceIdString(const chromeos::AudioDevice& device,
                                       int version) {
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

std::string GetDeviceIdString(const chromeos::AudioDevice& device) {
  return GetVersionedDeviceIdString(device, device.stable_device_id_version);
}

// Migrates an entry associated with |device|'s v1 stable device ID in
// |settings| to the key derived from |device|'s v2 stable device ID
// (which is expected to be equal to |intended_key|), if the entry can
// be found.
// Returns whether the migration occurred.
bool MigrateDeviceIdInSettings(base::DictionaryValue* settings,
                               const std::string& intended_key,
                               const chromeos::AudioDevice& device) {
  if (device.stable_device_id_version == 1)
    return false;

  DCHECK_EQ(2, device.stable_device_id_version);

  std::string old_device_id = GetVersionedDeviceIdString(device, 1);
  std::unique_ptr<base::Value> value;
  if (!settings->Remove(old_device_id, &value))
    return false;

  DCHECK_EQ(intended_key, GetDeviceIdString(device));
  settings->Set(intended_key, std::move(value));
  return true;
}

}  // namespace

namespace chromeos {

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

void AudioDevicesPrefHandlerImpl::SetVolumeGainValue(
    const AudioDevice& device, double value) {
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
    device_volume_settings_->Remove(old_device_id, nullptr);
  }
  device_volume_settings_->SetDouble(GetDeviceIdString(device), value);

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
  if (device_volume_settings_->HasKey(device_id)) {
    device_volume_settings_->Remove(device_id, nullptr);
    SaveDevicesVolumePref();
  }

  device_gain_settings_->SetDouble(device_id, value);
  SaveDevicesGainPref();
}

bool AudioDevicesPrefHandlerImpl::GetMuteValue(const AudioDevice& device) {
  std::string device_id_str = GetDeviceIdString(device);
  if (!device_mute_settings_->HasKey(device_id_str))
    MigrateDeviceMuteSettings(device_id_str, device);

  int mute = kPrefMuteOff;
  device_mute_settings_->GetInteger(device_id_str, &mute);

  return (mute == kPrefMuteOn);
}

void AudioDevicesPrefHandlerImpl::SetMuteValue(const AudioDevice& device,
                                               bool mute) {
  // Use this opportunity to remove device record under deprecated device ID,
  // if one exists.
  if (device.stable_device_id_version == 2) {
    std::string old_device_id = GetVersionedDeviceIdString(device, 1);
    device_mute_settings_->Remove(old_device_id, nullptr);
  }
  device_mute_settings_->SetInteger(GetDeviceIdString(device),
                                    mute ? kPrefMuteOn : kPrefMuteOff);
  SaveDevicesMutePref();
}

void AudioDevicesPrefHandlerImpl::SetDeviceActive(const AudioDevice& device,
                                                  bool active,
                                                  bool activate_by_user) {
  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetBoolean(kActiveKey, active);
  if (active)
    dict->SetBoolean(kActivateByUserKey, activate_by_user);

  // Use this opportunity to remove device record under deprecated device ID,
  // if one exists.
  if (device.stable_device_id_version == 2) {
    std::string old_device_id = GetVersionedDeviceIdString(device, 1);
    device_state_settings_->Remove(old_device_id, nullptr);
  }
  device_state_settings_->Set(GetDeviceIdString(device), std::move(dict));
  SaveDevicesStatePref();
}

bool AudioDevicesPrefHandlerImpl::GetDeviceActive(const AudioDevice& device,
                                                  bool* active,
                                                  bool* activate_by_user) {
  const std::string device_id_str = GetDeviceIdString(device);
  if (!device_state_settings_->HasKey(device_id_str) &&
      !MigrateDevicesStatePref(device_id_str, device)) {
    return false;
  }

  base::DictionaryValue* dict = NULL;
  if (!device_state_settings_->GetDictionary(device_id_str, &dict)) {
    LOG(ERROR) << "Could not get device state for device:" << device.ToString();
    return false;
  }
  if (!dict->GetBoolean(kActiveKey, active)) {
    LOG(ERROR) << "Could not get active value for device:" << device.ToString();
    return false;
  }

  if (*active && !dict->GetBoolean(kActivateByUserKey, activate_by_user)) {
    LOG(ERROR) << "Could not get activate_by_user value for previously "
                  "active device:"
               << device.ToString();
    return false;
  }

  return true;
}

bool AudioDevicesPrefHandlerImpl::GetAudioOutputAllowedValue() {
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
  if (!device_volume_settings_->HasKey(device_id_str))
    MigrateDeviceVolumeGainSettings(device_id_str, device);

  double value;
  device_volume_settings_->GetDouble(device_id_str, &value);

  return value;
}

double AudioDevicesPrefHandlerImpl::GetInputGainPrefValue(
    const AudioDevice& device) {
  DCHECK(device.is_input);
  std::string device_id_str = GetDeviceIdString(device);
  if (!device_gain_settings_->HasKey(device_id_str))
    SetInputGainPrefValue(device, kDefaultInputGainPercent);

  double value;
  device_gain_settings_->GetDouble(device_id_str, &value);

  return value;
}

double AudioDevicesPrefHandlerImpl::GetDeviceDefaultOutputVolume(
    const AudioDevice& device) {
  if (device.type == AUDIO_TYPE_HDMI)
    return kDefaultHdmiOutputVolumePercent;
  else
    return kDefaultOutputVolumePercent;
}

AudioDevicesPrefHandlerImpl::AudioDevicesPrefHandlerImpl(
    PrefService* local_state)
    : device_mute_settings_(std::make_unique<base::DictionaryValue>()),
      device_volume_settings_(std::make_unique<base::DictionaryValue>()),
      device_gain_settings_(std::make_unique<base::DictionaryValue>()),
      device_state_settings_(std::make_unique<base::DictionaryValue>()),
      local_state_(local_state) {
  InitializePrefObservers();

  LoadDevicesMutePref();
  LoadDevicesVolumePref();
  LoadDevicesGainPref();
  LoadDevicesStatePref();
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
  const base::DictionaryValue* mute_prefs =
      local_state_->GetDictionary(prefs::kAudioDevicesMute);
  if (mute_prefs)
    device_mute_settings_.reset(mute_prefs->DeepCopy());
}

void AudioDevicesPrefHandlerImpl::SaveDevicesMutePref() {
  DictionaryPrefUpdate dict_update(local_state_, prefs::kAudioDevicesMute);
  dict_update->Clear();
  dict_update->MergeDictionary(device_mute_settings_.get());
}

void AudioDevicesPrefHandlerImpl::LoadDevicesVolumePref() {
  const base::DictionaryValue* volume_prefs =
      local_state_->GetDictionary(prefs::kAudioDevicesVolumePercent);
  if (volume_prefs)
    device_volume_settings_.reset(volume_prefs->DeepCopy());
}

void AudioDevicesPrefHandlerImpl::SaveDevicesVolumePref() {
  DictionaryPrefUpdate dict_update(local_state_,
                                   prefs::kAudioDevicesVolumePercent);
  dict_update->Clear();
  dict_update->MergeDictionary(device_volume_settings_.get());
}

void AudioDevicesPrefHandlerImpl::LoadDevicesGainPref() {
  const base::DictionaryValue* gain_prefs =
      local_state_->GetDictionary(prefs::kAudioDevicesGainPercent);
  if (gain_prefs)
    device_gain_settings_.reset(gain_prefs->DeepCopy());
}

void AudioDevicesPrefHandlerImpl::SaveDevicesGainPref() {
  DictionaryPrefUpdate dict_update(local_state_,
                                   prefs::kAudioDevicesGainPercent);
  dict_update->Clear();
  dict_update->MergeDictionary(device_gain_settings_.get());
}

void AudioDevicesPrefHandlerImpl::LoadDevicesStatePref() {
  const base::DictionaryValue* state_prefs =
      local_state_->GetDictionary(prefs::kAudioDevicesState);
  if (state_prefs)
    device_state_settings_.reset(state_prefs->DeepCopy());
}

void AudioDevicesPrefHandlerImpl::SaveDevicesStatePref() {
  DictionaryPrefUpdate dict_update(local_state_, prefs::kAudioDevicesState);
  dict_update->Clear();
  dict_update->MergeDictionary(device_state_settings_.get());
}

bool AudioDevicesPrefHandlerImpl::MigrateDevicesStatePref(
    const std::string& device_key,
    const AudioDevice& device) {
  if (!MigrateDeviceIdInSettings(device_state_settings_.get(), device_key,
                                 device)) {
    return false;
  }

  SaveDevicesStatePref();
  return true;
}

void AudioDevicesPrefHandlerImpl::MigrateDeviceMuteSettings(
    const std::string& device_key,
    const AudioDevice& device) {
  if (!MigrateDeviceIdInSettings(device_mute_settings_.get(), device_key,
                                 device)) {
    // If there was no recorded value for deprecated device ID, use value from
    // global mute pref.
    int old_mute = local_state_->GetInteger(prefs::kAudioMute);
    device_mute_settings_->SetInteger(device_key, old_mute);
  }
  SaveDevicesMutePref();
}

void AudioDevicesPrefHandlerImpl::MigrateDeviceVolumeGainSettings(
    const std::string& device_key,
    const AudioDevice& device) {
  DCHECK(!device.is_input);
  if (!MigrateDeviceIdInSettings(device_volume_settings_.get(), device_key,
                                 device)) {
    // If there was no recorded value for deprecated device ID, use value from
    // global vloume pref.
    double old_volume = local_state_->GetDouble(prefs::kAudioVolumePercent);
    device_volume_settings_->SetDouble(device_key, old_volume);
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

  // Register the prefs backing the audio muting policies.
  // Policy for audio input is handled by kAudioCaptureAllowed in the Chrome
  // media system.
  registry->RegisterBooleanPref(prefs::kAudioOutputAllowed, true);

  // Register the legacy audio prefs for migration.
  registry->RegisterDoublePref(prefs::kAudioVolumePercent,
                               kDefaultOutputVolumePercent);
  registry->RegisterIntegerPref(prefs::kAudioMute, kPrefMuteOff);
}

}  // namespace chromeos
