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

const uint8_t kTestOutputVolumePercent = 80u;
const uint8_t kTestUnderMuteThreshholdVolumePercent = 0u;
const uint8_t kTestOverMaxOutputVolumePercent = 105u;
const int8_t kTestUnderMinOutputVolumePercent = -5;

const int8_t kDefaultOutputVolumePercent =
    AudioDevicesPrefHandler::kDefaultOutputVolumePercent;

const uint64_t kInternalSpeakerId = 10001;
const uint64_t kMicJackId = 10010;

struct AudioNodeInfo {
  bool is_input;
  uint64_t id;
  const char* const device_name;
  const char* const type;
  const char* const name;
  const uint32_t audio_effect;
};

const uint32_t kInputMaxSupportedChannels = 1;
const uint32_t kOutputMaxSupportedChannels = 2;

const int32_t kInputNumberOfVolumeSteps = 0;
const int32_t kOutputNumberOfVolumeSteps = 25;

const AudioNodeInfo kInternalSpeaker[] = {
    {false, kInternalSpeakerId, "Fake Speaker", "INTERNAL_SPEAKER", "Speaker"}};

const AudioNodeInfo kMicJack[] = {
    {true, kMicJackId, "Fake Mic Jack", "MIC", "Mic Jack"}};

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

  void SetOutputVolumePercent(uint8_t volume_percent) {
    remote_->SetOutputVolumePercent(volume_percent);
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
    }
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

TEST_F(CrosAudioConfigImplTest, GetOutputMuteStateMutedByPolicy) {
  SetOutputMuteState(mojom::MuteState::kMutedByPolicy);
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  ASSERT_EQ(1u, fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  EXPECT_EQ(
      mojom::MuteState::kMutedByPolicy,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);
}

TEST_F(CrosAudioConfigImplTest, GetOutputAudioDevices) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  ASSERT_EQ(1u, fake_observer->num_properties_updated_calls_);

  // Test default audio node list, which includes one input and one output node.
  SetAudioNodes({kInternalSpeaker, kMicJack});

  // |fake_observer| is called two times because OutputNodeVolume changes.
  ASSERT_EQ(3u, fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  EXPECT_EQ(1u, fake_observer->last_audio_system_properties_.value()
                    ->output_devices.size());
  EXPECT_EQ(kInternalSpeakerId,
            fake_observer->last_audio_system_properties_.value()
                ->output_devices[0]
                ->id);

  // Test removing output device.
  RemoveAudioNode(kInternalSpeakerId);
  ASSERT_EQ(4u, fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(0u, fake_observer->last_audio_system_properties_.value()
                    ->output_devices.size());

  // Test inserting output device.
  InsertAudioNode(kInternalSpeaker);
  ASSERT_EQ(6u, fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(1u, fake_observer->last_audio_system_properties_.value()
                    ->output_devices.size());
  EXPECT_EQ(kInternalSpeakerId,
            fake_observer->last_audio_system_properties_.value()
                ->output_devices[0]
                ->id);
}

}  // namespace ash::audio_config
