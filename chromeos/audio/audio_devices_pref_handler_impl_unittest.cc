// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/audio/audio_devices_pref_handler_impl.h"

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chromeos/audio/audio_device.h"
#include "chromeos/audio/audio_devices_pref_handler.h"
#include "chromeos/constants/chromeos_pref_names.h"
#include "chromeos/dbus/audio/audio_node.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

using testing::Values;

const uint64_t kPresetInputId = 10001;
const uint64_t kHeadphoneId = 10002;
const uint64_t kInternalMicId = 10003;
const uint64_t kPresetOutputId = 10004;
const uint64_t kUSBMicId = 10005;
const uint64_t kHDMIOutputId = 10006;
const uint64_t kOtherTypeOutputId = 90001;
const uint64_t kOtherTypeInputId = 90002;

const char kPresetInputDeprecatedPrefKey[] = "10001 : 1";
const char kPresetOutputDeprecatedPrefKey[] = "10004 : 0";

const double kDefaultSoundLevel = 75.0;

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

const AudioNodeInfo kUSBMic = {true, kUSBMicId, "Fake USB Mic", "USB",
                               "USB Microphone"};

const AudioNodeInfo kPresetOutput = {false, kPresetOutputId, "Fake output",
                                     "HEADPHONE", "Preset fake output"};

const AudioNodeInfo kHeadphone = {false, kHeadphoneId, "Fake Headphone",
                                  "HEADPHONE", "Headphone"};

const AudioNodeInfo kHDMIOutput = {false, kHDMIOutputId, "HDMI output", "HDMI",
                                   "HDMI output"};

const AudioNodeInfo kInputDeviceWithSpecialCharacters = {
    true, kOtherTypeInputId, "Fake ~!@#$%^&*()_+`-=<>?,./{}|[]\\\\Mic",
    "SOME_OTHER_TYPE", "Other Type Input Device"};

const AudioNodeInfo kOutputDeviceWithSpecialCharacters = {
    false, kOtherTypeOutputId, "Fake ~!@#$%^&*()_+`-=<>?,./{}|[]\\\\Headphone",
    "SOME_OTHER_TYPE", "Other Type Output Device"};

AudioDevice CreateAudioDevice(const AudioNodeInfo& info, int version) {
  return AudioDevice(AudioNode(
      info.is_input, info.id, version == 2, info.id /* stable_device_id_v1 */,
      version == 1 ? 0 : info.id ^ 0xFF /* stable_device_id_v2 */,
      info.device_name, info.type, info.name, false, 0));
}

// Test param determines whether the test should test input or output devices
// true -> input devices
// false -> output_devices
class AudioDevicesPrefHandlerTest : public testing::TestWithParam<bool> {
 public:
  AudioDevicesPrefHandlerTest() = default;
  ~AudioDevicesPrefHandlerTest() override = default;

  void SetUp() override {
    pref_service_.reset(new TestingPrefServiceSimple());
    AudioDevicesPrefHandlerImpl::RegisterPrefs(pref_service_->registry());

    // Set the preset pref values directly, to ensure it doesn't depend on pref
    // handler implementation.
    // This has to be done before audio_pref_hander_ is created, so the values
    // are set when pref value sets up its internal state.
    std::string preset_key = GetPresetDeviceDeprecatedPrefKey();
    {
      DictionaryPrefUpdate update(pref_service_.get(),
                                  prefs::kAudioDevicesState);
      base::DictionaryValue* pref = update.Get();
      std::unique_ptr<base::DictionaryValue> state(new base::DictionaryValue());
      state->SetBoolean("active", kPresetState.active);
      state->SetBoolean("activate_by_user", kPresetState.activate_by_user);
      pref->Set(preset_key, std::move(state));
    }

    {
      DictionaryPrefUpdate update(pref_service_.get(),
                                  prefs::kAudioDevicesVolumePercent);
      base::DictionaryValue* pref = update.Get();
      pref->SetDouble(preset_key, kPresetState.sound_level);
    }

    {
      DictionaryPrefUpdate update(pref_service_.get(),
                                  prefs::kAudioDevicesMute);
      base::DictionaryValue* pref = update.Get();
      pref->SetInteger(preset_key, kPresetState.mute ? 1 : 0);
    }

    audio_pref_handler_ = new AudioDevicesPrefHandlerImpl(pref_service_.get());
  }

  void TearDown() override { audio_pref_handler_.reset(); }

 protected:
  void ReloadPrefHandler() {
    audio_pref_handler_ = new AudioDevicesPrefHandlerImpl(pref_service_.get());
  }

  AudioDevice GetDeviceWithVersion(int version) {
    return CreateAudioDevice(GetParam() ? kInternalMic : kHeadphone, version);
  }

  std::string GetPresetDeviceDeprecatedPrefKey() {
    return GetParam() ? kPresetInputDeprecatedPrefKey
                      : kPresetOutputDeprecatedPrefKey;
  }

  AudioDevice GetPresetDeviceWithVersion(int version) {
    return CreateAudioDevice(GetParam() ? kPresetInput : kPresetOutput,
                             version);
  }

  AudioDevice GetSecondaryDeviceWithVersion(int version) {
    return CreateAudioDevice(GetParam() ? kUSBMic : kHDMIOutput, version);
  }

  AudioDevice GetDeviceWithSpecialCharactersWithVersion(int version) {
    return CreateAudioDevice(GetParam() ? kInputDeviceWithSpecialCharacters
                                        : kOutputDeviceWithSpecialCharacters,
                             version);
  }

  double GetSoundLevelValue(const AudioDevice& device) {
    return GetParam() ? audio_pref_handler_->GetInputGainValue(&device)
                      : audio_pref_handler_->GetOutputVolumeValue(&device);
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
    EXPECT_EQ(expect_activated_by_user, activated_by_user) << " device "
                                                           << device.id;
  }

  bool GetMute(const AudioDevice& device) {
    return audio_pref_handler_->GetMuteValue(device);
  }

  void SetMute(const AudioDevice& device, bool value) {
    audio_pref_handler_->SetMuteValue(device, value);
  }

  scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioDevicesPrefHandlerTest);
};

INSTANTIATE_TEST_SUITE_P(Input, AudioDevicesPrefHandlerTest, Values(true));
INSTANTIATE_TEST_SUITE_P(Output, AudioDevicesPrefHandlerTest, Values(false));

TEST_P(AudioDevicesPrefHandlerTest, TestDefaultValuesV1) {
  AudioDevice device = GetDeviceWithVersion(1);
  AudioDevice secondary_device = GetSecondaryDeviceWithVersion(1);

  // TODO(rkc): Once the bug with default preferences is fixed, fix this test
  // also. http://crbug.com/442489
  EXPECT_EQ(kDefaultSoundLevel, GetSoundLevelValue(device));
  EXPECT_EQ(kDefaultSoundLevel, GetSoundLevelValue(secondary_device));

  EXPECT_FALSE(DeviceStateExists(device));
  EXPECT_FALSE(DeviceStateExists(secondary_device));

  EXPECT_FALSE(GetMute(device));
  EXPECT_FALSE(GetMute(secondary_device));
}

TEST_P(AudioDevicesPrefHandlerTest, TestDefaultValuesV2) {
  AudioDevice device = GetDeviceWithVersion(2);
  AudioDevice secondary_device = GetSecondaryDeviceWithVersion(2);

  // TODO(rkc): Once the bug with default preferences is fixed, fix this test
  // also. http://crbug.com/442489
  EXPECT_EQ(kDefaultSoundLevel, GetSoundLevelValue(device));
  EXPECT_EQ(kDefaultSoundLevel, GetSoundLevelValue(secondary_device));

  EXPECT_FALSE(DeviceStateExists(device));
  EXPECT_FALSE(DeviceStateExists(secondary_device));

  EXPECT_FALSE(GetMute(device));
  EXPECT_FALSE(GetMute(secondary_device));
}

TEST_P(AudioDevicesPrefHandlerTest, PrefsRegistered) {
  // The standard audio prefs are registered.
  EXPECT_TRUE(pref_service_->FindPreference(prefs::kAudioDevicesVolumePercent));
  EXPECT_TRUE(pref_service_->FindPreference(prefs::kAudioDevicesMute));
  EXPECT_TRUE(pref_service_->FindPreference(prefs::kAudioOutputAllowed));
  EXPECT_TRUE(pref_service_->FindPreference(prefs::kAudioVolumePercent));
  EXPECT_TRUE(pref_service_->FindPreference(prefs::kAudioMute));
  EXPECT_TRUE(pref_service_->FindPreference(prefs::kAudioDevicesState));
}

TEST_P(AudioDevicesPrefHandlerTest, SoundLevel) {
  AudioDevice device = GetDeviceWithVersion(2);
  SetSoundLevelValue(device, 13.37);
  EXPECT_EQ(13.37, GetSoundLevelValue(device));
}

TEST_P(AudioDevicesPrefHandlerTest, SoundLevelMigratedFromV1StableId) {
  AudioDevice device_v1 = GetPresetDeviceWithVersion(1);
  AudioDevice device_v2 = GetPresetDeviceWithVersion(2);

  // Sanity check for test params - preset state should be different than the
  // default one in order for this test to make sense.
  ASSERT_NE(kDefaultSoundLevel, kPresetState.sound_level);

  EXPECT_EQ(kPresetState.sound_level, GetSoundLevelValue(device_v1));
  EXPECT_EQ(kPresetState.sound_level, GetSoundLevelValue(device_v2));
  // Test that v1 entry does not exist after migration - the method should
  // return default value.
  EXPECT_EQ(kDefaultSoundLevel, GetSoundLevelValue(device_v1));

  // Test that values are persisted when audio pref handler is reset.
  ReloadPrefHandler();
  EXPECT_EQ(kDefaultSoundLevel, GetSoundLevelValue(device_v1));
  EXPECT_EQ(kPresetState.sound_level, GetSoundLevelValue(device_v2));
}

TEST_P(AudioDevicesPrefHandlerTest, SettingV2DeviceSoundLevelRemovesV1Entry) {
  AudioDevice device_v1 = GetDeviceWithVersion(1);
  AudioDevice device_v2 = GetDeviceWithVersion(2);

  SetSoundLevelValue(device_v1, 13.37);
  EXPECT_EQ(13.37, GetSoundLevelValue(device_v1));

  SetSoundLevelValue(device_v2, 13.38);
  EXPECT_EQ(kDefaultSoundLevel, GetSoundLevelValue(device_v1));
  EXPECT_EQ(13.38, GetSoundLevelValue(device_v2));

  // Test that values are persisted when audio pref handler is reset.
  ReloadPrefHandler();
  EXPECT_EQ(kDefaultSoundLevel, GetSoundLevelValue(device_v1));
  EXPECT_EQ(13.38, GetSoundLevelValue(device_v2));
}

TEST_P(AudioDevicesPrefHandlerTest, MigrateFromGlobalSoundLevelPref) {
  pref_service_->SetDouble(prefs::kAudioVolumePercent, 13.37);

  // For devices with v1 stable device id.
  EXPECT_EQ(13.37, GetSoundLevelValue(GetDeviceWithVersion(1)));
  EXPECT_EQ(13.37, GetSoundLevelValue(GetDeviceWithVersion(2)));

  // For devices with v2 stable id.
  EXPECT_EQ(13.37, GetSoundLevelValue(GetSecondaryDeviceWithVersion(2)));
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

}  // namespace chromeos
