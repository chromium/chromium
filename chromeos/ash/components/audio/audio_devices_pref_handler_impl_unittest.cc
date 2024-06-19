// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_devices_pref_handler_impl.h"

#include <stdint.h>

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "audio_devices_pref_handler.h"
#include "base/memory/ref_counted.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time_override.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/audio_device_id.h"
#include "chromeos/ash/components/audio/audio_devices_pref_handler.h"
#include "chromeos/ash/components/dbus/audio/audio_node.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using testing::Values;

const uint64_t kPresetInputId = 10001;
const uint64_t kHeadphoneId = 10002;
const uint64_t kInternalMicId = 10003;
const uint64_t kPresetOutputId = 10004;
const uint64_t kUsbOutputId = 10005;
const uint64_t kUsbMicId = 10006;
const uint64_t kHDMIOutputId = 10007;
const uint64_t kBluetoothOutputId = 10008;
const uint64_t kBluetoothMicId = 10009;
const uint64_t kOtherTypeOutputId = 90001;
const uint64_t kOtherTypeInputId = 90002;

const char kPresetInputDeprecatedPrefKey[] = "10001 : 1";
const char kPresetOutputDeprecatedPrefKey[] = "10004 : 0";

const struct {
  bool active;
  bool activate_by_user;
  double sound_level;
  bool mute;
} kPresetState = {true, true, 25.2, true};

struct AudioNodeInfo {
  bool is_input;
  uint64_t id;
  const char* const device_name;
  const char* const type;
  const char* const name;
};

const AudioNodeInfo kPresetInput = {true, kPresetInputId, "Fake input",
                                    "INTERNAL_MIC", "Preset fake input"};

const AudioNodeInfo kInternalMic = {true, kInternalMicId, "Fake Mic",
                                    "INTERNAL_MIC", "Internal Mic"};

const AudioNodeInfo kUsbOutput = {false, kUsbOutputId, "Fake USB Output", "USB",
                                  "USB Output"};

const AudioNodeInfo kUSBMic = {true, kUsbMicId, "Fake USB Mic", "USB",
                               "USB Microphone"};

const AudioNodeInfo kPresetOutput = {false, kPresetOutputId, "Fake output",
                                     "HEADPHONE", "Preset fake output"};

const AudioNodeInfo kHeadphone = {false, kHeadphoneId, "Fake Headphone",
                                  "HEADPHONE", "Headphone"};

const AudioNodeInfo kHDMIOutput = {false, kHDMIOutputId, "HDMI output", "HDMI",
                                   "HDMI output"};

const AudioNodeInfo kBluetoothOutput = {false, kBluetoothOutputId, "BT output",
                                        "BLUETOOTH", "BT output"};

const AudioNodeInfo kBluetoothMic = {true, kBluetoothMicId, "BT mic",
                                     "BLUETOOTH", "BT mic"};

const AudioNodeInfo kInputDeviceWithSpecialCharacters = {
    true, kOtherTypeInputId, "Fake ~!@#$%^&*()_+`-=<>?,./{}|[]\\\\Mic",
    "SOME_OTHER_TYPE", "Other Type Input Device"};

const AudioNodeInfo kOutputDeviceWithSpecialCharacters = {
    false, kOtherTypeOutputId, "Fake ~!@#$%^&*()_+`-=<>?,./{}|[]\\\\Headphone",
    "SOME_OTHER_TYPE", "Other Type Output Device"};

const uint32_t kInputMaxSupportedChannels = 1;
const uint32_t kOutputMaxSupportedChannels = 2;

const uint32_t kInputAudioEffect = 1;
const uint32_t kOutputAudioEffect = 0;
// Does not support getting input step now.
const int32_t kInputNumberOfVolumeSteps = 0;
const int32_t kOutputNumberOfVolumeSteps = 25;

AudioDevice CreateAudioDevice(const AudioNodeInfo& info, int version) {
  return AudioDevice(AudioNode(
      info.is_input, info.id, version == 2, info.id /* stable_device_id_v1 */,
      version == 1 ? 0 : info.id ^ 0xFF /* stable_device_id_v2 */,
      info.device_name, info.type, info.name, false, 0,
      info.is_input ? kInputMaxSupportedChannels : kOutputMaxSupportedChannels,
      info.is_input ? kInputAudioEffect : kOutputAudioEffect,
      info.is_input ? kInputNumberOfVolumeSteps : kOutputNumberOfVolumeSteps));
}

// Test param determines whether the test should test input or output devices
// true -> input devices
// false -> output_devices
class AudioDevicesPrefHandlerTest : public testing::TestWithParam<bool> {
 public:
  AudioDevicesPrefHandlerTest() = default;

  AudioDevicesPrefHandlerTest(const AudioDevicesPrefHandlerTest&) = delete;
  AudioDevicesPrefHandlerTest& operator=(const AudioDevicesPrefHandlerTest&) =
      delete;

  ~AudioDevicesPrefHandlerTest() override = default;

  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    AudioDevicesPrefHandlerImpl::RegisterPrefs(pref_service_->registry());

    // Set the preset pref values directly, to ensure it doesn't depend on pref
    // handler implementation.
    // This has to be done before audio_pref_hander_ is created, so the values
    // are set when pref value sets up its internal state.
    std::string preset_key = GetPresetDeviceDeprecatedPrefKey();
    {
      ScopedDictPrefUpdate update(pref_service_.get(),
                                  prefs::kAudioDevicesState);
      base::Value::Dict& pref = update.Get();
      base::Value::Dict state;
      state.Set("active", kPresetState.active);
      state.Set("activate_by_user", kPresetState.activate_by_user);
      pref.Set(preset_key, std::move(state));
    }

    {
      ScopedDictPrefUpdate update(pref_service_.get(),
                                  prefs::kAudioDevicesVolumePercent);
      base::Value::Dict& pref = update.Get();
      pref.Set(preset_key, kPresetState.sound_level);
    }

    {
      ScopedDictPrefUpdate update(pref_service_.get(),
                                  prefs::kAudioDevicesMute);
      base::Value::Dict& pref = update.Get();
      pref.Set(preset_key, static_cast<int>(kPresetState.mute));
    }

    audio_pref_handler_ = new AudioDevicesPrefHandlerImpl(pref_service_.get());
  }

  void TearDown() override { audio_pref_handler_.reset(); }

  void ResetPrefHandler() {
    audio_pref_handler_.reset();
    audio_pref_handler_ = new AudioDevicesPrefHandlerImpl(pref_service_.get());
  }

 protected:
  void ReloadPrefHandler() {
    audio_pref_handler_ = new AudioDevicesPrefHandlerImpl(pref_service_.get());
  }

  AudioDevice GetDeviceWithVersion(int version) {
    return CreateAudioDevice(IsInputTest() ? kInternalMic : kHeadphone,
                             version);
  }

  std::string GetPresetDeviceDeprecatedPrefKey() {
    return IsInputTest() ? kPresetInputDeprecatedPrefKey
                         : kPresetOutputDeprecatedPrefKey;
  }

  AudioDevice GetPresetDeviceWithVersion(int version) {
    return CreateAudioDevice(IsInputTest() ? kPresetInput : kPresetOutput,
                             version);
  }

  AudioDevice GetSecondaryDeviceWithVersion(int version) {
    return CreateAudioDevice(IsInputTest() ? kUSBMic : kHDMIOutput, version);
  }

  AudioDevice GetDeviceWithSpecialCharactersWithVersion(int version) {
    return CreateAudioDevice(IsInputTest() ? kInputDeviceWithSpecialCharacters
                                           : kOutputDeviceWithSpecialCharacters,
                             version);
  }

  AudioDevice GetBTDeviceWithVersion(int version) {
    return CreateAudioDevice(IsInputTest() ? kBluetoothMic : kBluetoothOutput,
                             version);
  }

  AudioDevice GetUsbDeviceWithVersion(int version) {
    return CreateAudioDevice(IsInputTest() ? kUSBMic : kUsbOutput, version);
  }

  double GetSoundLevelValue(const AudioDevice& device) {
    return IsInputTest() ? audio_pref_handler_->GetInputGainValue(&device)
                         : audio_pref_handler_->GetOutputVolumeValue(&device);
  }

  int GetUserPriority(const AudioDevice& device) {
    return audio_pref_handler_->GetUserPriority(device);
  }

  void SetSoundLevelValue(const AudioDevice& device, double value) {
    return audio_pref_handler_->SetVolumeGainValue(device, value);
  }

  void SetDeviceState(const AudioDevice& device,
                      bool active,
                      bool activated_by_user) {
    audio_pref_handler_->SetDeviceActive(device, active, activated_by_user);
  }

  bool DeviceStateExists(const AudioDevice& device) {
    bool unused;
    return audio_pref_handler_->GetDeviceActive(device, &unused, &unused);
  }

  void ExpectDeviceStateEquals(const AudioDevice& device,
                               bool expect_active,
                               bool expect_activated_by_user) {
    bool active = false;
    bool activated_by_user = false;
    ASSERT_TRUE(audio_pref_handler_->GetDeviceActive(device, &active,
                                                     &activated_by_user))
        << " value for device " << device.id << " not found.";
    EXPECT_EQ(expect_active, active) << " device " << device.id;
    EXPECT_EQ(expect_activated_by_user, activated_by_user)
        << " device " << device.id;
  }

  bool GetMute(const AudioDevice& device) {
    return audio_pref_handler_->GetMuteValue(device);
  }

  void SetMute(const AudioDevice& device, bool value) {
    audio_pref_handler_->SetMuteValue(device, value);
  }

  double GetDefaultSoundLevelValue() {
    return IsInputTest() ? AudioDevicesPrefHandler::kDefaultInputGainPercent
                         : AudioDevicesPrefHandler::kDefaultOutputVolumePercent;
  }

  double GetDeviceDefaultSoundLevelValue(const AudioDevice& device) {
    if (IsInputTest()) {
      return AudioDevicesPrefHandler::kDefaultInputGainPercent;
    }

    switch (device.type) {
      case AudioDeviceType::kBluetooth:
        return AudioDevicesPrefHandler::kDefaultBluetoothOutputVolumePercent;
      case AudioDeviceType::kUsb:
        return AudioDevicesPrefHandler::kDefaultUsbOutputVolumePercent;
      case AudioDeviceType::kHdmi:
        return AudioDevicesPrefHandler::kDefaultHdmiOutputVolumePercent;
      default:
        return AudioDevicesPrefHandler::kDefaultOutputVolumePercent;
    }
  }

  bool IsInputTest() const { return GetParam(); }

  scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(Input, AudioDevicesPrefHandlerTest, Values(true));
INSTANTIATE_TEST_SUITE_P(Output, AudioDevicesPrefHandlerTest, Values(false));

TEST_P(AudioDevicesPrefHandlerTest, TestDefaultValuesV1) {
  AudioDevice device = GetDeviceWithVersion(1);
  AudioDevice secondary_device = GetSecondaryDeviceWithVersion(1);
  AudioDevice bt_device = GetBTDeviceWithVersion(1);
  AudioDevice usb_device = GetUsbDeviceWithVersion(1);

  EXPECT_EQ(GetDeviceDefaultSoundLevelValue(device),
            GetSoundLevelValue(device));
  EXPECT_EQ(GetDeviceDefaultSoundLevelValue(secondary_device),
            GetSoundLevelValue(secondary_device));
  EXPECT_EQ(GetDeviceDefaultSoundLevelValue(bt_device),
            GetSoundLevelValue(bt_device));
  EXPECT_EQ(GetDeviceDefaultSoundLevelValue(usb_device),
            GetSoundLevelValue(usb_device));

  EXPECT_FALSE(DeviceStateExists(device));
  EXPECT_FALSE(DeviceStateExists(secondary_device));
  EXPECT_FALSE(DeviceStateExists(bt_device));

  EXPECT_FALSE(GetMute(device));
  EXPECT_FALSE(GetMute(secondary_device));
  EXPECT_FALSE(GetMute(bt_device));

  EXPECT_EQ(0, GetUserPriority(device));
  EXPECT_EQ(0, GetUserPriority(secondary_device));
  EXPECT_EQ(0, GetUserPriority(bt_device));
}

TEST_P(AudioDevicesPrefHandlerTest, TestDefaultValuesV2) {
  AudioDevice device = GetDeviceWithVersion(2);
  AudioDevice secondary_device = GetSecondaryDeviceWithVersion(2);
  AudioDevice bt_device = GetBTDeviceWithVersion(2);

  EXPECT_EQ(GetDeviceDefaultSoundLevelValue(device),
            GetSoundLevelValue(device));
  EXPECT_EQ(GetDeviceDefaultSoundLevelValue(secondary_device),
            GetSoundLevelValue(secondary_device));
  EXPECT_EQ(GetDeviceDefaultSoundLevelValue(bt_device),
            GetSoundLevelValue(bt_device));

  EXPECT_FALSE(DeviceStateExists(device));
  EXPECT_FALSE(DeviceStateExists(secondary_device));
  EXPECT_FALSE(DeviceStateExists(bt_device));

  EXPECT_FALSE(GetMute(device));
  EXPECT_FALSE(GetMute(secondary_device));
  EXPECT_FALSE(GetMute(bt_device));

  EXPECT_EQ(0, GetUserPriority(device));
  EXPECT_EQ(0, GetUserPriority(secondary_device));
  EXPECT_EQ(0, GetUserPriority(bt_device));
}

TEST_P(AudioDevicesPrefHandlerTest, PrefsRegistered) {
  // The standard audio prefs are registered.
  EXPECT_TRUE(pref_service_->FindPreference(prefs::kAudioDevicesVolumePercent));
  EXPECT_TRUE(pref_service_->FindPreference(prefs::kAudioDevicesMute));
  EXPECT_TRUE(pref_service_->FindPreference(prefs::kAudioOutputAllowed));
  EXPECT_TRUE(pref_service_->FindPreference(prefs::kAudioMute));
  EXPECT_TRUE(pref_service_->FindPreference(prefs::kAudioDevicesState));
  EXPECT_TRUE(
      pref_service_->FindPreference(prefs::kAudioInputDevicesUserPriority));
  EXPECT_TRUE(
      pref_service_->FindPreference(prefs::kAudioOutputDevicesUserPriority));
  EXPECT_TRUE(
      pref_service_->FindPreference(prefs::kAudioInputDevicePreferenceSet));
  EXPECT_TRUE(
      pref_service_->FindPreference(prefs::kAudioOutputDevicePreferenceSet));
}

TEST_P(AudioDevicesPrefHandlerTest, SoundLevel) {
  AudioDevice device = GetDeviceWithVersion(2);
  SetSoundLevelValue(device, 13.37);
  EXPECT_EQ(13.37, GetSoundLevelValue(device));
}

TEST_P(AudioDevicesPrefHandlerTest, SoundLevelMigratedFromV1StableId) {
  if (IsInputTest()) {
    // Input gain does not need to be migrated since a new pref is used.
    return;
  }

  AudioDevice device_v1 = GetPresetDeviceWithVersion(1);
  AudioDevice device_v2 = GetPresetDeviceWithVersion(2);

  // Sanity check for test params - preset state should be different than the
  // default one in order for this test to make sense.
  ASSERT_NE(GetDefaultSoundLevelValue(), kPresetState.sound_level);

  EXPECT_EQ(kPresetState.sound_level, GetSoundLevelValue(device_v1));
  EXPECT_EQ(kPresetState.sound_level, GetSoundLevelValue(device_v2));
  // Test that v1 entry does not exist after migration - the method should
  // return default value.
  EXPECT_EQ(GetDefaultSoundLevelValue(), GetSoundLevelValue(device_v1));

  // Test that values are persisted when audio pref handler is reset.
  ReloadPrefHandler();
  EXPECT_EQ(GetDefaultSoundLevelValue(), GetSoundLevelValue(device_v1));
  EXPECT_EQ(kPresetState.sound_level, GetSoundLevelValue(device_v2));
}

TEST_P(AudioDevicesPrefHandlerTest, SettingV2DeviceSoundLevelRemovesV1Entry) {
  if (IsInputTest()) {
    // Input gain does not need to be migrated since a new pref is used.
    return;
  }
  AudioDevice device_v1 = GetDeviceWithVersion(1);
  AudioDevice device_v2 = GetDeviceWithVersion(2);

  SetSoundLevelValue(device_v1, 13.37);
  EXPECT_EQ(13.37, GetSoundLevelValue(device_v1));

  SetSoundLevelValue(device_v2, 13.38);
  EXPECT_EQ(GetDefaultSoundLevelValue(), GetSoundLevelValue(device_v1));
  EXPECT_EQ(13.38, GetSoundLevelValue(device_v2));

  // Test that values are persisted when audio pref handler is reset.
  ReloadPrefHandler();
  EXPECT_EQ(GetDefaultSoundLevelValue(), GetSoundLevelValue(device_v1));
  EXPECT_EQ(13.38, GetSoundLevelValue(device_v2));
}

TEST_P(AudioDevicesPrefHandlerTest, Mute) {
  AudioDevice device = GetDeviceWithVersion(2);
  SetMute(device, true);
  EXPECT_TRUE(GetMute(device));

  SetMute(device, false);
  EXPECT_FALSE(GetMute(device));
}

TEST_P(AudioDevicesPrefHandlerTest, MuteMigratedFromV1StableId) {
  AudioDevice device_v1 = GetPresetDeviceWithVersion(1);
  AudioDevice device_v2 = GetPresetDeviceWithVersion(2);

  // Sanity check for test params - preset state should be different than the
  // default one (mute = false) in order for this test to make sense.
  ASSERT_TRUE(kPresetState.mute);

  EXPECT_EQ(kPresetState.mute, GetMute(device_v1));
  EXPECT_EQ(kPresetState.mute, GetMute(device_v2));
  // Test that v1 entry does not exist after migration - the method should
  // return default value
  EXPECT_FALSE(GetMute(device_v1));

  // Test that values are persisted when audio pref handler is reset.
  ReloadPrefHandler();
  EXPECT_FALSE(GetMute(device_v1));
  EXPECT_EQ(kPresetState.mute, GetMute(device_v2));
}

TEST_P(AudioDevicesPrefHandlerTest, SettingV2DeviceMuteRemovesV1Entry) {
  AudioDevice device_v1 = GetDeviceWithVersion(1);
  AudioDevice device_v2 = GetDeviceWithVersion(2);

  SetMute(device_v1, true);
  EXPECT_TRUE(GetMute(device_v1));

  SetMute(device_v2, true);
  EXPECT_FALSE(GetMute(device_v1));
  EXPECT_TRUE(GetMute(device_v2));

  // Test that values are persisted when audio pref handler is reset.
  ReloadPrefHandler();
  EXPECT_FALSE(GetMute(device_v1));
  EXPECT_TRUE(GetMute(device_v2));
}

TEST_P(AudioDevicesPrefHandlerTest, MigrateFromGlobalMutePref) {
  pref_service_->SetInteger(prefs::kAudioMute, true);

  // For devices with v1 stable device id.
  EXPECT_TRUE(GetMute(GetDeviceWithVersion(1)));
  EXPECT_TRUE(GetMute(GetDeviceWithVersion(2)));

  // For devices with v2 stable id.
  EXPECT_TRUE(GetMute(GetSecondaryDeviceWithVersion(2)));
}

TEST_P(AudioDevicesPrefHandlerTest, TestSpecialCharactersInDeviceNames) {
  AudioDevice device = GetDeviceWithSpecialCharactersWithVersion(2);
  SetSoundLevelValue(device, 73.31);
  EXPECT_EQ(73.31, GetSoundLevelValue(device));

  SetMute(device, true);
  EXPECT_TRUE(GetMute(device));

  SetDeviceState(device, true, true);
  ExpectDeviceStateEquals(device, true, true);
}

TEST_P(AudioDevicesPrefHandlerTest, TestDeviceStates) {
  AudioDevice device = GetDeviceWithVersion(2);
  SetDeviceState(device, true, true);
  ExpectDeviceStateEquals(device, true, true);

  SetDeviceState(device, true, false);
  ExpectDeviceStateEquals(device, true, false);

  SetDeviceState(device, false, false);
  ExpectDeviceStateEquals(device, false, false);

  AudioDevice secondary_device = GetSecondaryDeviceWithVersion(2);
  EXPECT_FALSE(DeviceStateExists(secondary_device));
}

TEST_P(AudioDevicesPrefHandlerTest, TestDeviceStatesMigrateFromV1StableId) {
  AudioDevice device_v1 = GetPresetDeviceWithVersion(1);
  AudioDevice device_v2 = GetPresetDeviceWithVersion(2);

  ExpectDeviceStateEquals(device_v1, kPresetState.active,
                          kPresetState.activate_by_user);
  ExpectDeviceStateEquals(device_v2, kPresetState.active,
                          kPresetState.activate_by_user);
  EXPECT_FALSE(DeviceStateExists(device_v1));

  // Test that values are persisted when audio pref handler is reset.
  ReloadPrefHandler();
  EXPECT_FALSE(DeviceStateExists(device_v1));
  ExpectDeviceStateEquals(device_v2, kPresetState.active,
                          kPresetState.activate_by_user);
}

TEST_P(AudioDevicesPrefHandlerTest, TestSettingV2DeviceStateRemovesV1Entry) {
  AudioDevice device_v1 = GetDeviceWithVersion(1);
  AudioDevice device_v2 = GetDeviceWithVersion(2);

  SetDeviceState(device_v1, true, true);
  ExpectDeviceStateEquals(device_v1, true, true);

  SetDeviceState(device_v2, false, false);
  EXPECT_FALSE(DeviceStateExists(device_v1));
  ExpectDeviceStateEquals(device_v2, false, false);

  // Test that values are persisted when audio pref handler is reset.
  ReloadPrefHandler();
  EXPECT_FALSE(DeviceStateExists(device_v1));
  ExpectDeviceStateEquals(device_v2, false, false);
}

TEST_P(AudioDevicesPrefHandlerTest, InputNoiseCancellationPrefRegistered) {
  EXPECT_FALSE(audio_pref_handler_->GetNoiseCancellationState());
  audio_pref_handler_->SetNoiseCancellationState(true);
  EXPECT_TRUE(audio_pref_handler_->GetNoiseCancellationState());
}

TEST_P(AudioDevicesPrefHandlerTest, InputStyleTransferPrefRegistered) {
  EXPECT_FALSE(audio_pref_handler_->GetStyleTransferState());
  audio_pref_handler_->SetStyleTransferState(true);
  EXPECT_TRUE(audio_pref_handler_->GetStyleTransferState());
}

TEST_P(AudioDevicesPrefHandlerTest, HfpMicSrPrefRegistered) {
  EXPECT_FALSE(audio_pref_handler_->GetHfpMicSrState());
  audio_pref_handler_->SetHfpMicSrState(true);
  EXPECT_TRUE(audio_pref_handler_->GetHfpMicSrState());
}

TEST_P(AudioDevicesPrefHandlerTest, UserPriority) {
  AudioDevice device = GetDeviceWithVersion(2);
  EXPECT_EQ(kUserPriorityNone, GetUserPriority(device));

  AudioDevice device2 = GetSecondaryDeviceWithVersion(2);
  audio_pref_handler_->SetUserPriorityHigherThan(device2, &device);
  EXPECT_EQ(kUserPriorityNone, GetUserPriority(device));
  EXPECT_EQ(kUserPriorityMin, GetUserPriority(device2));

  audio_pref_handler_->SetUserPriorityHigherThan(device, &device2);
  EXPECT_EQ(2, GetUserPriority(device));
  EXPECT_EQ(kUserPriorityMin, GetUserPriority(device2));

  AudioDevice device3 = GetDeviceWithSpecialCharactersWithVersion(2);

  audio_pref_handler_->SetUserPriorityHigherThan(device3, &device2);
  EXPECT_EQ(2, GetUserPriority(device3));
  EXPECT_EQ(3, GetUserPriority(device));
  EXPECT_EQ(kUserPriorityMin, GetUserPriority(device2));

  audio_pref_handler_->SetUserPriorityHigherThan(device, &device3);
  EXPECT_EQ(2, GetUserPriority(device3));
  EXPECT_EQ(3, GetUserPriority(device));
  EXPECT_EQ(kUserPriorityMin, GetUserPriority(device2));

  audio_pref_handler_->SetUserPriorityHigherThan(device3, &device);
  EXPECT_EQ(3, GetUserPriority(device3));
  EXPECT_EQ(2, GetUserPriority(device));
  EXPECT_EQ(kUserPriorityMin, GetUserPriority(device2));
}

TEST_P(AudioDevicesPrefHandlerTest, UserPrioritySingle) {
  AudioDevice device = GetDeviceWithVersion(2);
  AudioDevice device2 = GetSecondaryDeviceWithVersion(2);
  audio_pref_handler_->SetUserPriorityHigherThan(device, nullptr);
  EXPECT_EQ(kUserPriorityMin, GetUserPriority(device));

  audio_pref_handler_->SetUserPriorityHigherThan(device2, nullptr);
  EXPECT_LT(GetUserPriority(device2), GetUserPriority(device));
  EXPECT_NE(kUserPriorityNone, GetUserPriority(device2));
  EXPECT_NE(kUserPriorityNone, GetUserPriority(device));
}

TEST_P(AudioDevicesPrefHandlerTest, DropLeastRecentlySeenDevices) {
  base::subtle::ScopedTimeClockOverrides time_override(
      []() {
        static int i = 0;
        return base::Time::FromSecondsSinceUnixEpoch(i++);
      },
      nullptr, nullptr);

  AudioDevice d[3] = {
      GetDeviceWithVersion(2),
      GetSecondaryDeviceWithVersion(2),
      GetDeviceWithSpecialCharactersWithVersion(2),
  };

  audio_pref_handler_->SetUserPriorityHigherThan(d[0], nullptr);
  audio_pref_handler_->SetUserPriorityHigherThan(d[1], &d[0]);
  audio_pref_handler_->SetUserPriorityHigherThan(d[2], &d[1]);

  // All devices should have priorities assigned.
  ASSERT_NE(kUserPriorityNone, GetUserPriority(d[0]));
  ASSERT_NE(kUserPriorityNone, GetUserPriority(d[1]));
  ASSERT_NE(kUserPriorityNone, GetUserPriority(d[2]));

  audio_pref_handler_->DropLeastRecentlySeenDevices(
      /*connected_devices=*/{d[0], d[1], d[2]}, 3);
  // Keep 2 devices. Only the most recently seen d[0], d[2] should be left.
  audio_pref_handler_->DropLeastRecentlySeenDevices(
      /*connected_devices=*/{d[0], d[2]}, 2);
  EXPECT_NE(kUserPriorityNone, GetUserPriority(d[0]));
  EXPECT_EQ(kUserPriorityNone, GetUserPriority(d[1]));
  EXPECT_NE(kUserPriorityNone, GetUserPriority(d[2]));

  // Request to keep 1 device. But connected devices are always kept.
  audio_pref_handler_->DropLeastRecentlySeenDevices(
      /*connected_devices=*/{d[0], d[2]}, 1);
  EXPECT_NE(kUserPriorityNone, GetUserPriority(d[0]));
  EXPECT_EQ(kUserPriorityNone, GetUserPriority(d[1]));
  EXPECT_NE(kUserPriorityNone, GetUserPriority(d[2]));

  // Keep 1 devices. Only the most recently seen d[2] should be left.
  audio_pref_handler_->DropLeastRecentlySeenDevices(
      /*connected_devices=*/{d[2]}, 1);
  EXPECT_EQ(kUserPriorityNone, GetUserPriority(d[0]));
  EXPECT_EQ(kUserPriorityNone, GetUserPriority(d[1]));
  EXPECT_NE(kUserPriorityNone, GetUserPriority(d[2]));
}

// Tests read and write audio device preference set pref.
TEST_P(AudioDevicesPrefHandlerTest, PreferenceSet) {
  AudioDevice device = GetDeviceWithVersion(2);
  AudioDevice device2 = GetSecondaryDeviceWithVersion(2);
  AudioDeviceList devices = {device, device2};

  // No preferred device among this set of devices yet.
  EXPECT_EQ(std::nullopt,
            audio_pref_handler_->GetPreferredDeviceFromPreferenceSet(
                device.is_input, devices));

  // Set preferred device and verify.
  audio_pref_handler_->UpdateDevicePreferenceSet(devices, device);
  EXPECT_EQ(device.stable_device_id,
            audio_pref_handler_->GetPreferredDeviceFromPreferenceSet(
                device.is_input, devices));

  // Set a different preferred device and verify.
  audio_pref_handler_->UpdateDevicePreferenceSet(devices, device2);
  EXPECT_EQ(device2.stable_device_id,
            audio_pref_handler_->GetPreferredDeviceFromPreferenceSet(
                device.is_input, devices));

  // Set back to the first device and verify.
  audio_pref_handler_->UpdateDevicePreferenceSet(devices, device);
  EXPECT_EQ(device.stable_device_id,
            audio_pref_handler_->GetPreferredDeviceFromPreferenceSet(
                device.is_input, devices));
}

// Tests preference set pref shouldn't be updated if the preferred device does
// not exist in the given device set.
TEST_P(AudioDevicesPrefHandlerTest, PreferredDeviceNotExist) {
  AudioDevice device = GetDeviceWithVersion(2);
  AudioDevice device2 = GetSecondaryDeviceWithVersion(2);
  AudioDevice device3 = GetBTDeviceWithVersion(2);
  AudioDeviceList devices = {device, device2};

  // No preferred device among this set of devices yet.
  EXPECT_EQ(std::nullopt,
            audio_pref_handler_->GetPreferredDeviceFromPreferenceSet(
                device.is_input, devices));

  // Set a preferred device which is not in the device set.
  audio_pref_handler_->UpdateDevicePreferenceSet(devices, device3);

  // Expect that no preferred device is set.
  EXPECT_EQ(std::nullopt,
            audio_pref_handler_->GetPreferredDeviceFromPreferenceSet(
                device.is_input, devices));
}

// Tests read and write most recent activated device id list pref.
TEST_P(AudioDevicesPrefHandlerTest, MostRecentActivatedDeviceIdList) {
  AudioDevice device = GetDeviceWithVersion(2);
  AudioDevice device2 = GetSecondaryDeviceWithVersion(2);

  // No activated device yet.
  EXPECT_TRUE(
      audio_pref_handler_->GetMostRecentActivatedDeviceIdList(device.is_input)
          .empty());

  // Activate first device.
  audio_pref_handler_->UpdateMostRecentActivatedDeviceIdList(device);
  EXPECT_EQ(1u, audio_pref_handler_
                    ->GetMostRecentActivatedDeviceIdList(device.is_input)
                    .size());
  EXPECT_EQ(
      GetDeviceIdString(device),
      audio_pref_handler_->GetMostRecentActivatedDeviceIdList(device.is_input)
          .back());

  // Activate second device.
  audio_pref_handler_->UpdateMostRecentActivatedDeviceIdList(device2);
  EXPECT_EQ(2u, audio_pref_handler_
                    ->GetMostRecentActivatedDeviceIdList(device2.is_input)
                    .size());
  EXPECT_EQ(
      GetDeviceIdString(device2),
      audio_pref_handler_->GetMostRecentActivatedDeviceIdList(device2.is_input)
          .back());

  // Activate the first device again. Expect there are still two devices in the
  // list. Expect this device is now in the end of the list.
  audio_pref_handler_->UpdateMostRecentActivatedDeviceIdList(device);
  EXPECT_EQ(2u, audio_pref_handler_
                    ->GetMostRecentActivatedDeviceIdList(device.is_input)
                    .size());
  EXPECT_EQ(
      GetDeviceIdString(device),
      audio_pref_handler_->GetMostRecentActivatedDeviceIdList(device.is_input)
          .back());
}

// Tests set-based audio selection preference is reset when flag is on.
TEST_P(AudioDevicesPrefHandlerTest, ResetAudioSelectionPrefFlagOn) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{ash::features::kResetAudioSelectionImprovementPref},
      /*disabled_features=*/{});

  AudioDevice device = GetDeviceWithVersion(2);
  AudioDevice device2 = GetSecondaryDeviceWithVersion(2);
  AudioDeviceList devices = {device, device2};

  // No preferred device among this set of devices yet.
  EXPECT_EQ(std::nullopt,
            audio_pref_handler_->GetPreferredDeviceFromPreferenceSet(
                device.is_input, devices));

  // Set preferred device and verify.
  audio_pref_handler_->UpdateDevicePreferenceSet(devices, device);
  EXPECT_EQ(device.stable_device_id,
            audio_pref_handler_->GetPreferredDeviceFromPreferenceSet(
                device.is_input, devices));

  ResetPrefHandler();
  // Expect that no preferred device is set.
  EXPECT_EQ(std::nullopt,
            audio_pref_handler_->GetPreferredDeviceFromPreferenceSet(
                device.is_input, devices));
}

// Tests set-based audio selection preference is not reset when flag is off.
TEST_P(AudioDevicesPrefHandlerTest, ResetAudioSelectionPrefFlagOff) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{
          ash::features::kResetAudioSelectionImprovementPref});

  AudioDevice device = GetDeviceWithVersion(2);
  AudioDevice device2 = GetSecondaryDeviceWithVersion(2);
  AudioDeviceList devices = {device, device2};

  // No preferred device among this set of devices yet.
  EXPECT_EQ(std::nullopt,
            audio_pref_handler_->GetPreferredDeviceFromPreferenceSet(
                device.is_input, devices));

  // Set preferred device and verify.
  audio_pref_handler_->UpdateDevicePreferenceSet(devices, device);
  EXPECT_EQ(device.stable_device_id,
            audio_pref_handler_->GetPreferredDeviceFromPreferenceSet(
                device.is_input, devices));

  ResetPrefHandler();
  // Expect that preferred device is set.
  EXPECT_EQ(device.stable_device_id,
            audio_pref_handler_->GetPreferredDeviceFromPreferenceSet(
                device.is_input, devices));
}

}  // namespace ash
