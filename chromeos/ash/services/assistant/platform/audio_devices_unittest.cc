// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/platform/audio_devices.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::assistant {

namespace {

using ::testing::_;

constexpr const char kDefaultLocale[] = "en_us";

class FakeAudioDevicesObserver : public AudioDevices::Observer {
 public:
  FakeAudioDevicesObserver() = default;
  FakeAudioDevicesObserver(const FakeAudioDevicesObserver&) = delete;
  FakeAudioDevicesObserver& operator=(const FakeAudioDevicesObserver&) = delete;
  ~FakeAudioDevicesObserver() override = default;

  // AudioDevices::Observer implementation
  void SetDeviceId(const std::optional<std::string>& device_id) override {
    preferred_device_id_ = device_id;
  }
  void SetHotwordDeviceId(
      const std::optional<std::string>& device_id) override {
    hotword_device_id_ = device_id;
  }

  std::string preferred_device_id() {
    return preferred_device_id_.value_or("<none>");
  }

  std::string hotword_device_id() {
    return hotword_device_id_.value_or("<none>");
  }

 private:
  std::optional<std::string> preferred_device_id_;
  std::optional<std::string> hotword_device_id_;
};

class DeviceBuilder {
 public:
  explicit DeviceBuilder(AudioDeviceType type) {
    result_.type = type;
    result_.is_input = true;
  }
  DeviceBuilder(const DeviceBuilder&) = delete;
  DeviceBuilder& operator=(const DeviceBuilder&) = delete;
  ~DeviceBuilder() = default;

  DeviceBuilder& WithId(int id) {
    result_.id = id;
    return *this;
  }

  DeviceBuilder& WithPriority(int priority) {
    result_.priority = priority;
    return *this;
  }

  DeviceBuilder& WithIsInput(bool is_input) {
    result_.is_input = is_input;
    return *this;
  }

  AudioDevice Build() const { return result_; }

 private:
  AudioDevice result_;
};

// Mock for |CrosAudioClient|. This inherits from |FakeCrasAudioClient| so we
// only have to mock the methods we're interested in.
// It will automatically be installed as the global singleton in its
// constructor, and removed in the destructor.
class ScopedCrasAudioClientMock : public FakeCrasAudioClient {
 public:
  ScopedCrasAudioClientMock() = default;
  ScopedCrasAudioClientMock(ScopedCrasAudioClientMock&) = delete;
  ScopedCrasAudioClientMock& operator=(ScopedCrasAudioClientMock&) = delete;
  ~ScopedCrasAudioClientMock() override = default;

  MOCK_METHOD(void,
              SetHotwordModel,
              (uint64_t node_id,
               const std::string& hotword_model,
               chromeos::VoidDBusMethodCallback callback));
};

class ScopedCrasAudioHandler {
 public:
  ScopedCrasAudioHandler() { CrasAudioHandler::InitializeForTesting(); }
  ScopedCrasAudioHandler(const ScopedCrasAudioHandler&) = delete;
  ScopedCrasAudioHandler& operator=(const ScopedCrasAudioHandler&) = delete;
  ~ScopedCrasAudioHandler() { CrasAudioHandler::Shutdown(); }

  CrasAudioHandler* Get() { return CrasAudioHandler::Get(); }
};

}  // namespace

class AssistantAudioDevicesTest : public testing::Test {
 public:
  AssistantAudioDevicesTest()
      : audio_devices_(cras_audio_handler_.Get(), "pref-locale") {
    // Enable DSP feature flag.
    scoped_feature_list_.InitAndEnableFeature(features::kEnableDspHotword);
  }

  AssistantAudioDevicesTest(const AssistantAudioDevicesTest&) = delete;
  AssistantAudioDevicesTest& operator=(const AssistantAudioDevicesTest&) =
      delete;
  ~AssistantAudioDevicesTest() override = default;

  AudioDevices& audio_devices() { return audio_devices_; }

  ScopedCrasAudioClientMock& cras_audio_client_mock() {
    return cras_audio_client_mock_;
  }

  void UpdateDeviceList(const AudioDeviceList& devices) {
    audio_devices().SetAudioDevicesForTest(devices);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  testing::NiceMock<ScopedCrasAudioClientMock> cras_audio_client_mock_;
  ScopedCrasAudioHandler cras_audio_handler_;
  AudioDevices audio_devices_;
};

TEST_F(AssistantAudioDevicesTest, ShouldSendHotwordDeviceToObserver) {
  FakeAudioDevicesObserver observer;
  audio_devices().AddAndFireObserver(&observer);

  UpdateDeviceList(
      {DeviceBuilder(AudioDeviceType::kHotword).WithId(111).Build()});

  EXPECT_EQ("111", observer.hotword_device_id());
  EXPECT_EQ("<none>", observer.preferred_device_id());
}

TEST_F(AssistantAudioDevicesTest, ShouldSendMicDeviceToObserver) {
  FakeAudioDevicesObserver observer;
  audio_devices().AddAndFireObserver(&observer);

  UpdateDeviceList({DeviceBuilder(AudioDeviceType::kMic).WithId(221).Build()});

  EXPECT_EQ("<none>", observer.hotword_device_id());
  EXPECT_EQ("221", observer.preferred_device_id());
}

TEST_F(AssistantAudioDevicesTest, ShouldSendUsbDeviceToObserver) {
  FakeAudioDevicesObserver observer;
  audio_devices().AddAndFireObserver(&observer);

  UpdateDeviceList({DeviceBuilder(AudioDeviceType::kUsb).WithId(222).Build()});

  EXPECT_EQ("<none>", observer.hotword_device_id());
  EXPECT_EQ("222", observer.preferred_device_id());
}

TEST_F(AssistantAudioDevicesTest, ShouldSendHeadphonesDeviceToObserver) {
  FakeAudioDevicesObserver observer;
  audio_devices().AddAndFireObserver(&observer);

  UpdateDeviceList(
      {DeviceBuilder(AudioDeviceType::kHeadphone).WithId(333).Build()});

  EXPECT_EQ("<none>", observer.hotword_device_id());
  EXPECT_EQ("333", observer.preferred_device_id());
}

TEST_F(AssistantAudioDevicesTest, ShouldSendInternalMicDeviceToObserver) {
  FakeAudioDevicesObserver observer;
  audio_devices().AddAndFireObserver(&observer);

  UpdateDeviceList(
      {DeviceBuilder(AudioDeviceType::kInternalMic).WithId(444).Build()});

  EXPECT_EQ("<none>", observer.hotword_device_id());
  EXPECT_EQ("444", observer.preferred_device_id());
}

TEST_F(AssistantAudioDevicesTest, ShouldSendFrontMicDeviceToObserver) {
  FakeAudioDevicesObserver observer;
  audio_devices().AddAndFireObserver(&observer);

  UpdateDeviceList(
      {DeviceBuilder(AudioDeviceType::kFrontMic).WithId(555).Build()});

  EXPECT_EQ("<none>", observer.hotword_device_id());
  EXPECT_EQ("555", observer.preferred_device_id());
}

TEST_F(AssistantAudioDevicesTest, ShouldSendRearMicDeviceToObserver) {
  FakeAudioDevicesObserver observer;
  audio_devices().AddAndFireObserver(&observer);

  UpdateDeviceList(
      {DeviceBuilder(AudioDeviceType::kRearMic).WithId(666).Build()});

  EXPECT_EQ("<none>", observer.hotword_device_id());
  EXPECT_EQ("666", observer.preferred_device_id());
}

TEST_F(AssistantAudioDevicesTest, ShouldSendKeyboardMicDeviceToObserver) {
  FakeAudioDevicesObserver observer;
  audio_devices().AddAndFireObserver(&observer);

  UpdateDeviceList(
      {DeviceBuilder(AudioDeviceType::kKeyboardMic).WithId(777).Build()});

  EXPECT_EQ("<none>", observer.hotword_device_id());
  EXPECT_EQ("777", observer.preferred_device_id());
}

TEST_F(AssistantAudioDevicesTest, ShouldUseHighestPriorityHotwordDevice) {
  FakeAudioDevicesObserver observer;
  audio_devices().AddAndFireObserver(&observer);

  UpdateDeviceList({
      DeviceBuilder(AudioDeviceType::kHotword)
          .WithId(111)
          .WithPriority(1)
          .Build(),
      DeviceBuilder(AudioDeviceType::kHotword)
          .WithId(555)
          .WithPriority(5)
          .Build(),
      DeviceBuilder(AudioDeviceType::kHotword)
          .WithId(222)
          .WithPriority(2)
          .Build(),
  });

  EXPECT_EQ("555", observer.hotword_device_id());
  EXPECT_EQ("<none>", observer.preferred_device_id());
}

TEST_F(AssistantAudioDevicesTest, ShouldIgnoreNonInputHotwordDevices) {
  FakeAudioDevicesObserver observer;
  audio_devices().AddAndFireObserver(&observer);

  UpdateDeviceList({
      DeviceBuilder(AudioDeviceType::kHotword)
          .WithId(111)
          .WithIsInput(false)
          .Build(),
      DeviceBuilder(AudioDeviceType::kHotword)
          .WithId(222)
          .WithIsInput(true)
          .Build(),
      DeviceBuilder(AudioDeviceType::kHotword)
          .WithId(333)
          .WithIsInput(false)
          .Build(),
  });

  EXPECT_EQ("222", observer.hotword_device_id());
  EXPECT_EQ("<none>", observer.preferred_device_id());
}

TEST_F(AssistantAudioDevicesTest, ShouldUseHighestPriorityDevice) {
  FakeAudioDevicesObserver observer;
  audio_devices().AddAndFireObserver(&observer);

  UpdateDeviceList({
      DeviceBuilder(AudioDeviceType::kUsb).WithId(111).WithPriority(1).Build(),
      DeviceBuilder(AudioDeviceType::kUsb).WithId(555).WithPriority(5).Build(),
      DeviceBuilder(AudioDeviceType::kUsb).WithId(222).WithPriority(2).Build(),
  });

  EXPECT_EQ("<none>", observer.hotword_device_id());
  EXPECT_EQ("555", observer.preferred_device_id());
}

TEST_F(AssistantAudioDevicesTest, ShouldIgnoreNonInputDevices) {
  FakeAudioDevicesObserver observer;
  audio_devices().AddAndFireObserver(&observer);

  UpdateDeviceList({
      DeviceBuilder(AudioDeviceType::kUsb)
          .WithId(111)
          .WithIsInput(false)
          .Build(),
      DeviceBuilder(AudioDeviceType::kUsb)
          .WithId(222)
          .WithIsInput(true)
          .Build(),
      DeviceBuilder(AudioDeviceType::kUsb)
          .WithId(333)
          .WithIsInput(false)
          .Build(),
  });

  EXPECT_EQ("<none>", observer.hotword_device_id());
  EXPECT_EQ("222", observer.preferred_device_id());
}

TEST_F(AssistantAudioDevicesTest, ShouldIgnoreUnsupportedDeviceTypes) {
  FakeAudioDevicesObserver observer;
  audio_devices().AddAndFireObserver(&observer);

  UpdateDeviceList({
      DeviceBuilder(AudioDeviceType::kBluetooth).WithId(2).Build(),
      DeviceBuilder(AudioDeviceType::kBluetoothNbMic).WithId(3).Build(),
      DeviceBuilder(AudioDeviceType::kHdmi).WithId(4).Build(),
      DeviceBuilder(AudioDeviceType::kInternalSpeaker).WithId(5).Build(),
      DeviceBuilder(AudioDeviceType::kLineout).WithId(8).Build(),
      DeviceBuilder(AudioDeviceType::kPostMixLoopback).WithId(9).Build(),
      DeviceBuilder(AudioDeviceType::kPostDspLoopback).WithId(10).Build(),
      DeviceBuilder(AudioDeviceType::kAlsaLoopback).WithId(11).Build(),
      DeviceBuilder(AudioDeviceType::kOther).WithId(12).Build(),
  });

  EXPECT_EQ("<none>", observer.hotword_device_id());
  EXPECT_EQ("<none>", observer.preferred_device_id());
}

TEST_F(AssistantAudioDevicesTest, ShouldFireObserverWhenAdded) {
  UpdateDeviceList({
      DeviceBuilder(AudioDeviceType::kHotword).WithId(111).Build(),
      DeviceBuilder(AudioDeviceType::kUsb).WithId(222).Build(),
  });

  FakeAudioDevicesObserver observer;
  audio_devices().AddAndFireObserver(&observer);

  EXPECT_EQ("111", observer.hotword_device_id());
  EXPECT_EQ("222", observer.preferred_device_id());
}

TEST_F(AssistantAudioDevicesTest, ShouldNotFireObserverAfterItsRemoved) {
  FakeAudioDevicesObserver observer;
  audio_devices().AddAndFireObserver(&observer);
  audio_devices().RemoveObserver(&observer);

  UpdateDeviceList({
      DeviceBuilder(AudioDeviceType::kHotword).WithId(111).Build(),
      DeviceBuilder(AudioDeviceType::kUsb).WithId(222).Build(),
  });

  EXPECT_EQ("<none>", observer.hotword_device_id());
  EXPECT_EQ("<none>", observer.preferred_device_id());
}

TEST_F(AssistantAudioDevicesTest,
       ShouldUpdateHotwordModelWhenHotwordDeviceIsAdded) {
  EXPECT_CALL(cras_audio_client_mock(), SetHotwordModel);

  UpdateDeviceList({DeviceBuilder(AudioDeviceType::kHotword).Build()});
}

TEST_F(AssistantAudioDevicesTest, ShouldFormatLocaleToHotwordModel) {
  UpdateDeviceList(
      {DeviceBuilder(AudioDeviceType::kHotword).WithId(111).Build()});

  // Normal case
  EXPECT_CALL(cras_audio_client_mock(), SetHotwordModel(111, "nl_be", _));
  audio_devices().SetLocale("nl-BE");
  base::RunLoop().RunUntilIdle();

  // Handle the case where country code and language code are the same
  EXPECT_CALL(cras_audio_client_mock(), SetHotwordModel(111, "fr_fr", _));
  audio_devices().SetLocale("fr");
  base::RunLoop().RunUntilIdle();

  // use "en_all" for all english locales
  EXPECT_CALL(cras_audio_client_mock(), SetHotwordModel(111, "en_all", _));
  audio_devices().SetLocale("en-US");
  base::RunLoop().RunUntilIdle();
}

TEST_F(AssistantAudioDevicesTest, ShouldUseDefaultLocaleIfUserPrefIsRejected) {
  UpdateDeviceList(
      {DeviceBuilder(AudioDeviceType::kHotword).WithId(222).Build()});

  EXPECT_CALL(cras_audio_client_mock(),
              SetHotwordModel(_, "rejected_locale", _))
      .WillOnce([](uint64_t node_id, const std::string&,
                   chromeos::VoidDBusMethodCallback callback) {
        // Report failure to change the locale
        std::move(callback).Run(/*success=*/false);
      });

  EXPECT_CALL(cras_audio_client_mock(),
              SetHotwordModel(222, kDefaultLocale, _));

  audio_devices().SetLocale("rejected-LOCALE");
}

TEST_F(AssistantAudioDevicesTest, ShouldUseDefaultLocaleIfUserPrefIsEmpty) {
  UpdateDeviceList({DeviceBuilder(AudioDeviceType::kHotword).Build()});

  EXPECT_CALL(cras_audio_client_mock(), SetHotwordModel(_, kDefaultLocale, _));

  audio_devices().SetLocale("");
}

TEST_F(AssistantAudioDevicesTest, ShouldDoNothingIfUserPrefIsAccepted) {
  UpdateDeviceList({DeviceBuilder(AudioDeviceType::kHotword).Build()});

  EXPECT_CALL(cras_audio_client_mock(),
              SetHotwordModel(_, "accepted_locale", _))
      .WillOnce([](uint64_t node_id, const std::string&,
                   chromeos::VoidDBusMethodCallback callback) {
        // Accept the change to the locale.
        std::move(callback).Run(/*success=*/true);
      });

  // Do not expect a second call if change of locale is accepted
  EXPECT_CALL(cras_audio_client_mock(), SetHotwordModel(_, kDefaultLocale, _))
      .Times(0);

  audio_devices().SetLocale("accepted-LOCALE");
}

}  // namespace ash::assistant
