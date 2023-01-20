// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/cros_audio_config_impl.h"

#include "ash/constants/ash_features.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/audio/audio_devices_pref_handler.h"
#include "chromeos/ash/components/audio/audio_devices_pref_handler_stub.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/audio/cros_audio_config.h"
#include "chromeos/ash/components/audio/public/mojom/cros_audio_config.mojom.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::audio_config {

constexpr uint8_t kTestOutputVolumePercent = 80u;
constexpr uint8_t kTestInputGainPercent = 37u;
constexpr uint8_t kTestUnderMuteThreshholdVolumePercent = 0u;
constexpr uint8_t kTestOverMaxOutputVolumePercent = 105u;
constexpr int8_t kTestUnderMinOutputVolumePercent = -5;

constexpr int8_t kDefaultOutputVolumePercent =
    AudioDevicesPrefHandler::kDefaultOutputVolumePercent;

constexpr int8_t kDefaultInputGainPercent =
    AudioDevicesPrefHandler::kDefaultInputGainPercent;

constexpr uint64_t kInternalSpeakerId = 10001;
constexpr uint64_t kMicJackId = 10010;
constexpr uint64_t kHDMIOutputId = 10020;
constexpr uint64_t kUsbMicId = 10030;
constexpr uint64_t kInternalMicFrontId = 10040;
constexpr uint64_t kInternalMicRearId = 10050;

struct AudioNodeInfo {
  bool is_input;
  uint64_t id;
  const char* const device_name;
  const char* const type;
  const char* const name;
  const uint32_t audio_effect;
};

constexpr uint32_t kInputMaxSupportedChannels = 1;
constexpr uint32_t kOutputMaxSupportedChannels = 2;

constexpr int32_t kInputNumberOfVolumeSteps = 0;
constexpr int32_t kOutputNumberOfVolumeSteps = 25;

constexpr AudioNodeInfo kInternalSpeaker[] = {
    {false, kInternalSpeakerId, "Fake Speaker", "INTERNAL_SPEAKER", "Speaker"}};

constexpr AudioNodeInfo kMicJack[] = {
    {true, kMicJackId, "Fake Mic Jack", "MIC", "Mic Jack"}};

constexpr AudioNodeInfo kHDMIOutput[] = {
    {false, kHDMIOutputId, "HDMI output", "HDMI", "HDMI output"}};

constexpr AudioNodeInfo kUsbMic[] = {
    {true, kUsbMicId, "Fake USB Mic", "USB", "USB Mic"}};

constexpr AudioNodeInfo kInternalMicFront[] = {
    {true, kInternalMicFrontId, "Front Mic", "FRONT_MIC", "FrontMic"}};

constexpr AudioNodeInfo kInternalMicRear[] = {
    {true, kInternalMicRearId, "Rear Mic", "REAR_MIC", "RearMic"}};

class FakeAudioSystemPropertiesObserver
    : public mojom::AudioSystemPropertiesObserver {
 public:
  FakeAudioSystemPropertiesObserver() = default;
  ~FakeAudioSystemPropertiesObserver() override = default;

  mojo::PendingRemote<AudioSystemPropertiesObserver> GeneratePendingRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnPropertiesUpdated(
      mojom::AudioSystemPropertiesPtr properties) override {
    last_audio_system_properties_ = std::move(properties);
    ++num_properties_updated_calls_;
  }

  absl::optional<mojom::AudioSystemPropertiesPtr> last_audio_system_properties_;
  size_t num_properties_updated_calls_ = 0u;
  mojo::Receiver<mojom::AudioSystemPropertiesObserver> receiver_{this};
};

class CrosAudioConfigImplTest : public testing::Test {
 public:
  CrosAudioConfigImplTest() = default;
  ~CrosAudioConfigImplTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kAudioSettingsPage);
    CrasAudioClient::InitializeFake();
    fake_cras_audio_client_ = FakeCrasAudioClient::Get();
    CrasAudioHandler::InitializeForTesting();
    cras_audio_handler_ = CrasAudioHandler::Get();
    audio_pref_handler_ = base::MakeRefCounted<AudioDevicesPrefHandlerStub>();
    cras_audio_handler_->SetPrefHandlerForTesting(audio_pref_handler_);
    cros_audio_config_ = std::make_unique<CrosAudioConfigImpl>();
    ui::MicrophoneMuteSwitchMonitor::Get()->SetMicrophoneMuteSwitchValue(
        /*switch_on=*/false);
  }

  void TearDown() override {
    CrasAudioHandler::Shutdown();
    CrasAudioClient::Shutdown();
    audio_pref_handler_ = nullptr;
  }

 protected:
  std::unique_ptr<FakeAudioSystemPropertiesObserver> Observe() {
    cros_audio_config_->BindPendingReceiver(
        remote_.BindNewPipeAndPassReceiver());
    auto fake_observer = std::make_unique<FakeAudioSystemPropertiesObserver>();
    remote_->ObserveAudioSystemProperties(
        fake_observer->GeneratePendingRemote());
    base::RunLoop().RunUntilIdle();
    return fake_observer;
  }

  bool GetDeviceMuted(const uint64_t id) {
    return cras_audio_handler_->IsOutputMutedForDevice(id);
  }

  void SimulateSetActiveDevice(const uint64_t& device_id) {
    remote_->SetActiveDevice(device_id);
    base::RunLoop().RunUntilIdle();
  }

  void SimulateSetOutputMuted(bool muted) {
    remote_->SetOutputMuted(muted);
    base::RunLoop().RunUntilIdle();
  }

  void SimulateSetInputMuted(bool muted) {
    remote_->SetInputMuted(muted);
    base::RunLoop().RunUntilIdle();
  }

  void SetOutputVolumePercent(uint8_t volume_percent) {
    remote_->SetOutputVolumePercent(volume_percent);
    base::RunLoop().RunUntilIdle();
  }

  void SetInputGainPercent(int gain_percent) {
    cras_audio_handler_->SetInputGainPercent(gain_percent);
    base::RunLoop().RunUntilIdle();
  }

  void SetInputGainPercentFromFrontEnd(int gain_percent) {
    // TODO(swifton): Replace RunUntilIdle with Run and QuitClosure.
    remote_->SetInputGainPercent(gain_percent);
    base::RunLoop().RunUntilIdle();
  }

  void SetOutputMuteState(mojom::MuteState mute_state) {
    switch (mute_state) {
      case mojom::MuteState::kMutedByUser:
        audio_pref_handler_->SetAudioOutputAllowedValue(true);
        cras_audio_handler_->SetOutputMute(true);
        break;
      case mojom::MuteState::kNotMuted:
        audio_pref_handler_->SetAudioOutputAllowedValue(true);
        cras_audio_handler_->SetOutputMute(false);
        break;
      case mojom::MuteState::kMutedByPolicy:
        // Calling this method does not alert AudioSystemPropertiesObserver.
        audio_pref_handler_->SetAudioOutputAllowedValue(false);
        break;
      case mojom::MuteState::kMutedExternally:
        NOTREACHED() << "Output audio does not support kMutedExternally.";
        break;
    }
    base::RunLoop().RunUntilIdle();
  }

  void SetInputMuteState(mojom::MuteState mute_state, bool switch_on = false) {
    switch (mute_state) {
      case mojom::MuteState::kMutedByUser:
        cras_audio_handler_->SetMuteForDevice(
            cras_audio_handler_->GetPrimaryActiveInputNode(),
            /*mute_on=*/true);
        break;
      case mojom::MuteState::kNotMuted:
        cras_audio_handler_->SetMuteForDevice(
            cras_audio_handler_->GetPrimaryActiveInputNode(),
            /*mute_on=*/false);
        break;
      case mojom::MuteState::kMutedByPolicy:
        NOTREACHED() << "Input audio does not support kMutedByPolicy.";
        break;
      case mojom::MuteState::kMutedExternally:
        ui::MicrophoneMuteSwitchMonitor::Get()->SetMicrophoneMuteSwitchValue(
            switch_on);
        break;
    }
    base::RunLoop().RunUntilIdle();
  }

  void SetActiveInputNodes(const std::vector<uint64_t>& ids) {
    cras_audio_handler_->SetActiveInputNodes(ids);
    base::RunLoop().RunUntilIdle();
  }

  void SetActiveOutputNodes(const std::vector<uint64_t>& ids) {
    cras_audio_handler_->SetActiveOutputNodes(ids);
    base::RunLoop().RunUntilIdle();
  }

  void SetAudioNodes(const std::vector<const AudioNodeInfo*>& nodes) {
    fake_cras_audio_client_->SetAudioNodesAndNotifyObserversForTesting(
        GenerateAudioNodeList(nodes));
    base::RunLoop().RunUntilIdle();
  }

  void RemoveAudioNode(const uint64_t node_id) {
    fake_cras_audio_client_->RemoveAudioNodeFromList(node_id);
    base::RunLoop().RunUntilIdle();
  }

  void InsertAudioNode(const AudioNodeInfo* node_info) {
    fake_cras_audio_client_->InsertAudioNodeToList(
        GenerateAudioNode(node_info));
    base::RunLoop().RunUntilIdle();
  }

 private:
  AudioNode GenerateAudioNode(const AudioNodeInfo* node_info) {
    return AudioNode(node_info->is_input, node_info->id, false, node_info->id,
                     /*stable_device_id_v2=*/0, node_info->device_name,
                     node_info->type, node_info->name, /*is_active=*/false,
                     /* plugged_time=*/0,
                     node_info->is_input ? kInputMaxSupportedChannels
                                         : kOutputMaxSupportedChannels,
                     node_info->audio_effect,
                     /* number_of_volume_steps=*/
                     node_info->is_input ? kInputNumberOfVolumeSteps
                                         : kOutputNumberOfVolumeSteps);
  }

  AudioNodeList GenerateAudioNodeList(
      const std::vector<const AudioNodeInfo*>& nodes) {
    AudioNodeList node_list;
    for (auto* node_info : nodes) {
      node_list.push_back(GenerateAudioNode(node_info));
    }
    return node_list;
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;

  CrasAudioHandler* cras_audio_handler_ = nullptr;  // Not owned.
  std::unique_ptr<CrosAudioConfigImpl> cros_audio_config_;
  mojo::Remote<mojom::CrosAudioConfig> remote_;
  scoped_refptr<AudioDevicesPrefHandlerStub> audio_pref_handler_;
  FakeCrasAudioClient* fake_cras_audio_client_;
};

TEST_F(CrosAudioConfigImplTest, HandleExternalInputGainUpdate) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  // |fake_observer| count is first incremented in Observe() method.
  ASSERT_EQ(1u, fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  ASSERT_EQ(
      kDefaultInputGainPercent,
      fake_observer->last_audio_system_properties_.value()->input_gain_percent);

  SetInputGainPercent(kTestInputGainPercent);

  ASSERT_EQ(2u, fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  ASSERT_EQ(
      kTestInputGainPercent,
      fake_observer->last_audio_system_properties_.value()->input_gain_percent);
}

TEST_F(CrosAudioConfigImplTest, GetSetOutputVolumePercent) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  // |fake_observer| count is first incremented in Observe() method.
  ASSERT_EQ(1u, fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  ASSERT_EQ(kDefaultOutputVolumePercent,
            fake_observer->last_audio_system_properties_.value()
                ->output_volume_percent);

  SetOutputVolumePercent(kTestOutputVolumePercent);
  ASSERT_EQ(2u, fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(kTestOutputVolumePercent,
            fake_observer->last_audio_system_properties_.value()
                ->output_volume_percent);
}

TEST_F(CrosAudioConfigImplTest, SetInputGainPercent) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  // |fake_observer| count is first incremented in Observe() method.
  ASSERT_EQ(1u, fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  ASSERT_EQ(
      kDefaultInputGainPercent,
      fake_observer->last_audio_system_properties_.value()->input_gain_percent);

  SetInputGainPercentFromFrontEnd(kTestInputGainPercent);

  // This check relies on the fact that when CrasAudioHandler receives a call to
  // change the input gain, it will notify all observers, one of which is
  // |fake_observer|.
  ASSERT_EQ(2u, fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(
      kTestInputGainPercent,
      fake_observer->last_audio_system_properties_.value()->input_gain_percent);
}

TEST_F(CrosAudioConfigImplTest, GetSetOutputVolumePercentMuteThresholdTest) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();

  // |fake_observer| count is first incremented in Observe() method.
  ASSERT_EQ(1u, fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  ASSERT_EQ(kDefaultOutputVolumePercent,
            fake_observer->last_audio_system_properties_.value()
                ->output_volume_percent);

  // Test setting volume over mute threshold when muted.
  SetOutputMuteState(mojom::MuteState::kMutedByUser);
  ASSERT_EQ(2u, fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(
      mojom::MuteState::kMutedByUser,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);

  SetOutputVolumePercent(kDefaultOutputVolumePercent);

  // |fake_observer| should be notified twice due to mute state changing when
  // setting volume over the mute threshold.
  ASSERT_EQ(4u, fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(
      mojom::MuteState::kNotMuted,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);
  EXPECT_EQ(kDefaultOutputVolumePercent,
            fake_observer->last_audio_system_properties_.value()
                ->output_volume_percent);

  // Test setting volume under mute threshold when muted.
  SetOutputMuteState(mojom::MuteState::kMutedByUser);
  ASSERT_EQ(5u, fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(
      mojom::MuteState::kMutedByUser,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);

  SetOutputVolumePercent(kTestUnderMuteThreshholdVolumePercent);
  ASSERT_EQ(6u, fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(
      mojom::MuteState::kMutedByUser,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);
  EXPECT_EQ(kTestUnderMuteThreshholdVolumePercent,
            fake_observer->last_audio_system_properties_.value()
                ->output_volume_percent);
}

TEST_F(CrosAudioConfigImplTest, SetInputGainPercentWhileMuted) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();

  // |fake_observer| count is first incremented in Observe() method.
  ASSERT_EQ(1u, fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  ASSERT_EQ(
      kDefaultInputGainPercent,
      fake_observer->last_audio_system_properties_.value()->input_gain_percent);

  // Test setting gain when muted.
  SetInputMuteState(mojom::MuteState::kMutedByUser);
  ASSERT_EQ(2u, fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(
      mojom::MuteState::kMutedByUser,
      fake_observer->last_audio_system_properties_.value()->input_mute_state);

  SetInputGainPercentFromFrontEnd(kDefaultInputGainPercent);

  // |fake_observer| should be notified twice due to mute state changing when
  // setting gain.
  ASSERT_EQ(4u, fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(
      mojom::MuteState::kNotMuted,
      fake_observer->last_audio_system_properties_.value()->input_mute_state);
  EXPECT_EQ(
      kDefaultInputGainPercent,
      fake_observer->last_audio_system_properties_.value()->input_gain_percent);
}

TEST_F(CrosAudioConfigImplTest, GetSetOutputVolumePercentVolumeBoundariesTest) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();

  // |fake_observer| count is first incremented in Observe() method.
  ASSERT_EQ(1u, fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  ASSERT_EQ(kDefaultOutputVolumePercent,
            fake_observer->last_audio_system_properties_.value()
                ->output_volume_percent);

  // Test setting volume over max volume.
  SetOutputVolumePercent(kTestOverMaxOutputVolumePercent);
  ASSERT_EQ(2u, fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(100u, fake_observer->last_audio_system_properties_.value()
                      ->output_volume_percent);

  // Test setting volume under min volume.
  SetOutputVolumePercent(kTestUnderMinOutputVolumePercent);
  ASSERT_EQ(3u, fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(0u, fake_observer->last_audio_system_properties_.value()
                    ->output_volume_percent);
}

TEST_F(CrosAudioConfigImplTest, GetOutputMuteState) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  ASSERT_EQ(1u, fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  EXPECT_EQ(
      mojom::MuteState::kNotMuted,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);

  SetOutputMuteState(mojom::MuteState::kMutedByUser);
  ASSERT_EQ(2u, fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(
      mojom::MuteState::kMutedByUser,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);

  SetOutputMuteState(mojom::MuteState::kNotMuted);
  ASSERT_EQ(3u, fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(
      mojom::MuteState::kNotMuted,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);
}

TEST_F(CrosAudioConfigImplTest, HandleOutputMuteStateMutedByPolicy) {
  SetOutputMuteState(mojom::MuteState::kMutedByPolicy);
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  ASSERT_EQ(1u, fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  EXPECT_EQ(
      mojom::MuteState::kMutedByPolicy,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);

  // Simulate attempting to change mute state while policy is enabled.
  SimulateSetOutputMuted(/*muted=*/true);
  SimulateSetOutputMuted(/*muted=*/false);
  ASSERT_EQ(1u, fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  EXPECT_EQ(
      mojom::MuteState::kMutedByPolicy,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);
}

TEST_F(CrosAudioConfigImplTest, GetOutputAudioDevices) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  size_t expected_observer_calls = 1u;
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);

  // Test default audio node list, which includes one input and one output node.
  SetAudioNodes({kInternalSpeaker, kMicJack});
  // Multiple calls to observer triggered by setting active nodes triggered by
  // AudioObserver events volume, active output, and nodes changed.
  expected_observer_calls += 4u;

  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  EXPECT_EQ(1u, fake_observer->last_audio_system_properties_.value()
                    ->output_devices.size());
  EXPECT_EQ(kInternalSpeakerId,
            fake_observer->last_audio_system_properties_.value()
                ->output_devices[0]
                ->id);

  // Test removing output device.
  RemoveAudioNode(kInternalSpeakerId);
  // Multiple calls to observer triggered by setting active nodes triggered by
  // AudioObserver events volume, active output, and nodes changed.
  expected_observer_calls += 2u;
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(0u, fake_observer->last_audio_system_properties_.value()
                    ->output_devices.size());

  // Test inserting inactive output device.
  InsertAudioNode(kInternalSpeaker);
  // Multiple calls to observer triggered by setting active nodes triggered by
  // AudioObserver events volume, active output, and nodes changed.
  expected_observer_calls += 3u;
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(1u, fake_observer->last_audio_system_properties_.value()
                    ->output_devices.size());
  EXPECT_EQ(kInternalSpeakerId,
            fake_observer->last_audio_system_properties_.value()
                ->output_devices[0]
                ->id);
}

TEST_F(CrosAudioConfigImplTest, GetInputAudioDevices) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  size_t expected_observer_calls = 1u;
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);

  // Consfigure initial node set for test.
  SetAudioNodes({kInternalSpeaker});
  // Multiple calls to observer triggered by setting active nodes triggered by
  // AudioObserver events volume, active output, and nodes changed.
  expected_observer_calls += 4u;

  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  EXPECT_EQ(0u, fake_observer->last_audio_system_properties_.value()
                    ->input_devices.size());

  InsertAudioNode(kMicJack);
  // Multiple calls to observer triggered by setting active nodes triggered by
  // AudioObserver events active input and nodes changed.
  expected_observer_calls += 2;

  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  EXPECT_EQ(1u, fake_observer->last_audio_system_properties_.value()
                    ->input_devices.size());
  EXPECT_EQ(kMicJackId, fake_observer->last_audio_system_properties_.value()
                            ->input_devices[0]
                            ->id);

  RemoveAudioNode(kMicJackId);
  // Multiple calls to observer triggered by setting active nodes triggered by
  // AudioObserver events active input and nodes changed.
  expected_observer_calls += 2;

  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  EXPECT_EQ(0u, fake_observer->last_audio_system_properties_.value()
                    ->input_devices.size());
}

TEST_F(CrosAudioConfigImplTest, HandleExternalActiveOutputDeviceUpdate) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  // Setup test with two output and one input device. CrasAudioHandler will set
  // the first device to active.
  SetAudioNodes({kHDMIOutput, kInternalSpeaker, kMicJack});
  SetActiveOutputNodes({kHDMIOutputId});

  ASSERT_FALSE(fake_observer->last_audio_system_properties_.value()
                   ->output_devices[0]
                   ->is_active);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.value()
                  ->output_devices[1]
                  ->is_active);
  ASSERT_EQ(kHDMIOutputId, fake_observer->last_audio_system_properties_.value()
                               ->output_devices[1]
                               ->id);

  SetActiveOutputNodes({kInternalSpeakerId});

  ASSERT_TRUE(fake_observer->last_audio_system_properties_.value()
                  ->output_devices[0]
                  ->is_active);
  ASSERT_EQ(kInternalSpeakerId,
            fake_observer->last_audio_system_properties_.value()
                ->output_devices[0]
                ->id);
  ASSERT_FALSE(fake_observer->last_audio_system_properties_.value()
                   ->output_devices[1]
                   ->is_active);
}

TEST_F(CrosAudioConfigImplTest, HandleExternalActiveInputDeviceUpdate) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  // Setup test with two input and one output device. CrasAudioHandler will set
  // the first device to active.
  SetAudioNodes({kInternalSpeaker, kMicJack, kUsbMic});
  SetActiveInputNodes({kUsbMicId});

  ASSERT_FALSE(fake_observer->last_audio_system_properties_.value()
                   ->input_devices[0]
                   ->is_active);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.value()
                  ->input_devices[1]
                  ->is_active);
  ASSERT_EQ(kUsbMicId, fake_observer->last_audio_system_properties_.value()
                           ->input_devices[1]
                           ->id);

  SetActiveInputNodes({kMicJackId});

  ASSERT_TRUE(fake_observer->last_audio_system_properties_.value()
                  ->input_devices[0]
                  ->is_active);
  ASSERT_EQ(kMicJackId, fake_observer->last_audio_system_properties_.value()
                            ->input_devices[0]
                            ->id);
  ASSERT_FALSE(fake_observer->last_audio_system_properties_.value()
                   ->input_devices[1]
                   ->is_active);
}

TEST_F(CrosAudioConfigImplTest, SetActiveOutputDevice) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();

  // Test default audio node list, with two output and one input node.
  SetAudioNodes({kInternalSpeaker, kHDMIOutput, kMicJack});
  // Set active output node for test.
  SetActiveOutputNodes({kInternalSpeakerId});
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.value()
                  ->output_devices[0]
                  ->is_active);
  ASSERT_FALSE(fake_observer->last_audio_system_properties_.value()
                   ->output_devices[1]
                   ->is_active);

  SimulateSetActiveDevice(kHDMIOutputId);

  ASSERT_FALSE(fake_observer->last_audio_system_properties_.value()
                   ->output_devices[0]
                   ->is_active);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.value()
                  ->output_devices[1]
                  ->is_active);
}

TEST_F(CrosAudioConfigImplTest, SetActiveInputDevice) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();

  // Test default audio node list, with two input and one output node.
  SetAudioNodes({kInternalSpeaker, kMicJack, kUsbMic});
  // Set active output node for test.
  SetActiveInputNodes({kMicJackId});
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.value()
                  ->input_devices[0]
                  ->is_active);
  ASSERT_FALSE(fake_observer->last_audio_system_properties_.value()
                   ->input_devices[1]
                   ->is_active);

  SimulateSetActiveDevice(kUsbMicId);

  ASSERT_FALSE(fake_observer->last_audio_system_properties_.value()
                   ->input_devices[0]
                   ->is_active);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.value()
                  ->input_devices[1]
                  ->is_active);
}

TEST_F(CrosAudioConfigImplTest, HandleInputMuteState) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  SetAudioNodes({kInternalSpeaker, kMicJack});
  ASSERT_EQ(
      mojom::MuteState::kNotMuted,
      fake_observer->last_audio_system_properties_.value()->input_mute_state);

  SetInputMuteState(mojom::MuteState::kMutedByUser);
  ASSERT_EQ(
      mojom::MuteState::kMutedByUser,
      fake_observer->last_audio_system_properties_.value()->input_mute_state);

  SetInputMuteState(mojom::MuteState::kNotMuted);
  ASSERT_EQ(
      mojom::MuteState::kNotMuted,
      fake_observer->last_audio_system_properties_.value()->input_mute_state);

  // Simulate turning physical switch on.
  SetInputMuteState(mojom::MuteState::kMutedExternally, true);
  ASSERT_EQ(
      mojom::MuteState::kMutedExternally,
      fake_observer->last_audio_system_properties_.value()->input_mute_state);

  // Simulate turning physical switch off.
  SetInputMuteState(mojom::MuteState::kMutedExternally, false);
  ASSERT_EQ(
      mojom::MuteState::kNotMuted,
      fake_observer->last_audio_system_properties_.value()->input_mute_state);
}

TEST_F(CrosAudioConfigImplTest, SetOutputMuted) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();

  // Test default audio node list.
  SetAudioNodes({kInternalSpeaker, kHDMIOutput});
  SetActiveOutputNodes({kInternalSpeakerId});
  EXPECT_EQ(
      mojom::MuteState::kNotMuted,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);
  EXPECT_FALSE(GetDeviceMuted(kInternalSpeakerId));
  EXPECT_FALSE(GetDeviceMuted(kHDMIOutputId));

  SimulateSetOutputMuted(/*muted=*/true);
  EXPECT_EQ(
      mojom::MuteState::kMutedByUser,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);
  EXPECT_TRUE(GetDeviceMuted(kInternalSpeakerId));
  EXPECT_FALSE(GetDeviceMuted(kHDMIOutputId));

  SimulateSetOutputMuted(/*muted=*/false);
  EXPECT_EQ(
      mojom::MuteState::kNotMuted,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);
  EXPECT_FALSE(GetDeviceMuted(kInternalSpeakerId));
  EXPECT_FALSE(GetDeviceMuted(kHDMIOutputId));
}

TEST_F(CrosAudioConfigImplTest, SetInputMuted) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  SetAudioNodes({kInternalSpeaker, kMicJack});
  ASSERT_EQ(
      mojom::MuteState::kNotMuted,
      fake_observer->last_audio_system_properties_.value()->input_mute_state);

  SimulateSetInputMuted(/*muted=*/true);
  ASSERT_EQ(
      mojom::MuteState::kMutedByUser,
      fake_observer->last_audio_system_properties_.value()->input_mute_state);

  SimulateSetInputMuted(/*muted=*/false);
  ASSERT_EQ(
      mojom::MuteState::kNotMuted,
      fake_observer->last_audio_system_properties_.value()->input_mute_state);

  // Simulate turning physical switch on.
  SetInputMuteState(mojom::MuteState::kMutedExternally, /*switch_on=*/true);

  // Simulate changing mute state while physical switch on.
  SimulateSetInputMuted(/*muted=*/true);
  ASSERT_EQ(
      mojom::MuteState::kMutedExternally,
      fake_observer->last_audio_system_properties_.value()->input_mute_state);

  SimulateSetInputMuted(/*muted=*/false);
  ASSERT_EQ(
      mojom::MuteState::kMutedExternally,
      fake_observer->last_audio_system_properties_.value()->input_mute_state);
}

// Verify merging front and rear mic into a single device returns expected
// device and updates correctly.
TEST_F(CrosAudioConfigImplTest, StubInternalMicHandlesDualMicUpdates) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  SetAudioNodes(
      {kInternalSpeaker, kInternalMicFront, kInternalMicRear, kUsbMic});
  SetActiveInputNodes({kUsbMicId});

  size_t expected_input_device_count = 2u;
  EXPECT_EQ(expected_input_device_count,
            fake_observer->last_audio_system_properties_.value()
                ->input_devices.size());
  mojom::AudioDevicePtr internal_mic =
      fake_observer->last_audio_system_properties_.value()
          ->input_devices[1]
          .Clone();
  const std::string expected_display_name = "Internal Mic";
  EXPECT_EQ(expected_display_name, internal_mic->display_name);
  EXPECT_FALSE(internal_mic->is_active);

  SimulateSetActiveDevice(kInternalMicFrontId);

  EXPECT_EQ(expected_input_device_count,
            fake_observer->last_audio_system_properties_.value()
                ->input_devices.size());
  internal_mic = fake_observer->last_audio_system_properties_.value()
                     ->input_devices[1]
                     .Clone();
  EXPECT_EQ(expected_display_name, internal_mic->display_name);
  EXPECT_TRUE(internal_mic->is_active);

  // Verify rear device ID will also set internal mic to active.
  SetActiveInputNodes({kUsbMicId});
  SimulateSetActiveDevice(kInternalMicRearId);

  EXPECT_EQ(expected_input_device_count,
            fake_observer->last_audio_system_properties_.value()
                ->input_devices.size());
  internal_mic = fake_observer->last_audio_system_properties_.value()
                     ->input_devices[1]
                     .Clone();
  EXPECT_EQ(expected_display_name, internal_mic->display_name);
  EXPECT_TRUE(internal_mic->is_active);
}

}  // namespace ash::audio_config
