// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/cros_audio_config_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
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
constexpr uint64_t kInternalMicId = 10060;
constexpr uint64_t kBluetoothNbMicId = 10070;
constexpr uint64_t kInternalMicStyleTransferId = 10080;

constexpr base::TimeDelta kMetricsDelayTimerInterval = base::Seconds(2);

// Histogram names.
constexpr char kOutputMuteChangeHistogramName[] =
    "ChromeOS.CrosAudioConfig.OutputMuteStateChange";
constexpr char kInputMuteChangeHistogramName[] =
    "ChromeOS.CrosAudioConfig.InputMuteStateChange";
constexpr char kNoiseCancellationEnabledHistogramName[] =
    "ChromeOS.CrosAudioConfig.NoiseCancellationEnabled";
constexpr char kOutputVolumeChangeHistogramName[] =
    "ChromeOS.CrosAudioConfig.OutputVolumeSetTo";
constexpr char kInputGainChangeHistogramName[] =
    "ChromeOS.CrosAudioConfig.InputGainSetTo";
constexpr char kAudioDeviceChangeHistogramName[] =
    "ChromeOS.CrosAudioConfig.DeviceChange";
constexpr char kOutputDeviceTypeHistogramName[] =
    "ChromeOS.CrosAudioConfig.OutputDeviceTypeChangedTo";
constexpr char kInputDeviceTypeHistogramName[] =
    "ChromeOS.CrosAudioConfig.InputDeviceTypeChangedTo";

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

constexpr AudioNodeInfo kInternalMic[] = {
    {true, kInternalMicId, "Internal Mic", "INTERNAL_MIC", "InternalMic",
     cras::AudioEffectType::EFFECT_TYPE_NOISE_CANCELLATION}};

constexpr AudioNodeInfo kInternalMicStyleTransfer[] = {
    {true, kInternalMicStyleTransferId, "Internal Mic", "INTERNAL_MIC",
     "InternalMic", cras::AudioEffectType::EFFECT_TYPE_STYLE_TRANSFER}};

constexpr AudioNodeInfo kBluetoothNbMic[] = {
    {true, kBluetoothNbMicId, "Bluetooth Nb Mic", "BLUETOOTH_NB_MIC",
     "BluetoothNbMic", cras::AudioEffectType::EFFECT_TYPE_HFP_MIC_SR}};

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

  mojom::AudioDevicePtr GetInputAudioDevice(size_t index) const {
    DCHECK(last_audio_system_properties_.has_value());
    DCHECK(last_audio_system_properties_.value()->input_devices.size() > index);

    return last_audio_system_properties_.value()->input_devices[index].Clone();
  }

  mojom::AudioDevicePtr GetOutputAudioDevice(size_t index) const {
    DCHECK(last_audio_system_properties_.has_value());
    DCHECK(last_audio_system_properties_.value()->output_devices.size() >
           index);

    return last_audio_system_properties_.value()->output_devices[index].Clone();
  }

  std::optional<mojom::AudioSystemPropertiesPtr> last_audio_system_properties_;
  size_t num_properties_updated_calls_ = 0u;
  mojo::Receiver<mojom::AudioSystemPropertiesObserver> receiver_{this};
};

class CrosAudioConfigImplTest : public testing::Test {
 public:
  CrosAudioConfigImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~CrosAudioConfigImplTest() override = default;

  void SetUp() override {
    CrasAudioClient::InitializeFake();
    fake_cras_audio_client_ = FakeCrasAudioClient::Get();
    CrasAudioHandler::InitializeForTesting();
    cras_audio_handler_ = CrasAudioHandler::Get();
    audio_pref_handler_ = base::MakeRefCounted<AudioDevicesPrefHandlerStub>();
    audio_pref_handler_->SetNoiseCancellationState(
        /*noise_cancellation_state=*/false);
    audio_pref_handler_->SetStyleTransferState(
        /*noise_cancellation_state=*/false);
    audio_pref_handler_->SetForceRespectUiGainsState(
        /*force_respect_ui_gains=*/false);
    audio_pref_handler_->SetHfpMicSrState(
        /*hfp_mic_sr_state=*/false);
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
    // TODO(ashleydp): Replace RunUntilIdle with Run and QuitClosure.
    remote_->SetInputGainPercent(gain_percent);
    base::RunLoop().RunUntilIdle();
  }

  void SimulateSetNoiseCancellationEnabled(bool enabled) {
    // TODO(ashleydp): Replace RunUntilIdle with Run and QuitClosure.
    remote_->SetNoiseCancellationEnabled(enabled);
    base::RunLoop().RunUntilIdle();
  }

  void SimulateSetStyleTransferEnabled(bool enabled) {
    // TODO(ashleydp): Replace RunUntilIdle with Run and QuitClosure.
    remote_->SetStyleTransferEnabled(enabled);
    base::RunLoop().RunUntilIdle();
  }

  void SimulateSetForceRespectUiGainsEnabled(bool enabled) {
    // TODO(eddyhsu): Replace RunUntilIdle with Run and QuitClosure.
    remote_->SetForceRespectUiGainsEnabled(enabled);
    base::RunLoop().RunUntilIdle();
  }

  void SimulateSetHfpMicSrEnabled(bool enabled) {
    // TODO(ashleydp): Replace RunUntilIdle with Run and QuitClosure.
    remote_->SetHfpMicSrEnabled(enabled);
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
        NOTREACHED_IN_MIGRATION()
            << "Output audio does not support kMutedExternally.";
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
        NOTREACHED_IN_MIGRATION()
            << "Input audio does not support kMutedByPolicy.";
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

  bool GetNoiseCancellationState() {
    return fake_cras_audio_client_->noise_cancellation_enabled();
  }

  bool GetNoiseCancellationStatePref() {
    return audio_pref_handler_->GetNoiseCancellationState();
  }

  bool GetNoiseCancellationSupported() {
    return cras_audio_handler_->noise_cancellation_supported();
  }

  void SetNoiseCancellationStatePref(bool enabled) {
    audio_pref_handler_->SetNoiseCancellationState(
        /*noise_cancellation_state=*/enabled);
    base::RunLoop().RunUntilIdle();
  }

  void SetNoiseCancellationSupported(bool supported) {
    cras_audio_handler_->SetNoiseCancellationSupportedForTesting(supported);
  }

  void SetNoiseCancellationState(bool noise_cancellation_on) {
    cras_audio_handler_->SetNoiseCancellationState(
        noise_cancellation_on,
        CrasAudioHandler::AudioSettingsChangeSource::kOsSettings);
    base::RunLoop().RunUntilIdle();
  }

  bool GetStyleTransferState() {
    return fake_cras_audio_client_->style_transfer_enabled();
  }

  bool GetStyleTransferStatePref() {
    return audio_pref_handler_->GetStyleTransferState();
  }

  bool GetStyleTransferSupported() {
    return cras_audio_handler_->style_transfer_supported();
  }

  void SetStyleTransferStatePref(bool enabled) {
    audio_pref_handler_->SetStyleTransferState(
        /*style_transfer_state=*/enabled);
    base::RunLoop().RunUntilIdle();
  }

  void SetStyleTransferSupported(bool supported) {
    cras_audio_handler_->SetStyleTransferSupportedForTesting(supported);
  }

  void SetStyleTransferState(bool style_transfer_on) {
    cras_audio_handler_->SetStyleTransferState(style_transfer_on);
    base::RunLoop().RunUntilIdle();
  }

  bool GetForceRespectUiGainsState() {
    return fake_cras_audio_client_->force_respect_ui_gains_enabled();
  }

  bool GetForceRespectUiGainsStatePref() {
    return audio_pref_handler_->GetForceRespectUiGainsState();
  }

  void SetForceRespectUiGainsStatePref(bool enabled) {
    // TODO(eddyhsu): Replace RunUntilIdle with Run and QuitClosure.
    audio_pref_handler_->SetForceRespectUiGainsState(
        /*force_respect_ui_gains=*/enabled);
    base::RunLoop().RunUntilIdle();
  }

  void SetForceRespectUiGainsState(bool force_respect_ui_gains_on) {
    // TODO(eddyhsu): Replace RunUntilIdle with Run and QuitClosure.
    cras_audio_handler_->SetForceRespectUiGainsState(force_respect_ui_gains_on);
    base::RunLoop().RunUntilIdle();
  }

  bool GetHfpMicSrState() {
    return fake_cras_audio_client_->hfp_mic_sr_enabled();
  }

  bool GetHfpMicSrStatePref() {
    return audio_pref_handler_->GetHfpMicSrState();
  }

  bool GetHfpMicSrSupported() {
    return cras_audio_handler_->hfp_mic_sr_supported();
  }

  void SetHfpMicSrStatePref(bool enabled) {
    audio_pref_handler_->SetHfpMicSrState(
        /*hfp_mic_sr_state=*/enabled);
    base::RunLoop().RunUntilIdle();
  }

  void SetHfpMicSrSupported(bool supported) {
    cras_audio_handler_->SetHfpMicSrSupportedForTesting(supported);
  }

  void SetHfpMicSrState(bool hfp_mic_sr_on) {
    cras_audio_handler_->SetHfpMicSrState(
        hfp_mic_sr_on,
        CrasAudioHandler::AudioSettingsChangeSource::kOsSettings);
    base::RunLoop().RunUntilIdle();
  }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  base::HistogramTester histogram_tester_;

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

  raw_ptr<CrasAudioHandler, DanglingUntriaged> cras_audio_handler_ =
      nullptr;  // Not owned.
  std::unique_ptr<CrosAudioConfigImpl> cros_audio_config_;
  mojo::Remote<mojom::CrosAudioConfig> remote_;
  scoped_refptr<AudioDevicesPrefHandlerStub> audio_pref_handler_;
  raw_ptr<FakeCrasAudioClient, DanglingUntriaged> fake_cras_audio_client_;
};

TEST_F(CrosAudioConfigImplTest, HandleExternalInputGainUpdate) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  size_t expected_observer_calls = 1u;
  // |fake_observer| count is first incremented in Observe() method.
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  ASSERT_EQ(
      kDefaultInputGainPercent,
      fake_observer->last_audio_system_properties_.value()->input_gain_percent);

  SetInputGainPercent(kTestInputGainPercent);

  expected_observer_calls++;
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  ASSERT_EQ(
      kTestInputGainPercent,
      fake_observer->last_audio_system_properties_.value()->input_gain_percent);
}

TEST_F(CrosAudioConfigImplTest, GetSetOutputVolumePercent) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  size_t expected_observer_calls = 1u;
  // |fake_observer| count is first incremented in Observe() method.
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  ASSERT_EQ(kDefaultOutputVolumePercent,
            fake_observer->last_audio_system_properties_.value()
                ->output_volume_percent);

  SetOutputVolumePercent(kTestOutputVolumePercent);
  expected_observer_calls++;
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(kTestOutputVolumePercent,
            fake_observer->last_audio_system_properties_.value()
                ->output_volume_percent);
}

TEST_F(CrosAudioConfigImplTest, SetInputGainPercent) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  // |fake_observer| count is first incremented in Observe() method.
  size_t expected_observer_calls = 1u;
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  ASSERT_EQ(
      kDefaultInputGainPercent,
      fake_observer->last_audio_system_properties_.value()->input_gain_percent);

  SetInputGainPercentFromFrontEnd(kTestInputGainPercent);

  // This check relies on the fact that when CrasAudioHandler receives a call to
  // change the input gain, it will notify all observers, one of which is
  // |fake_observer|.
  expected_observer_calls++;
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(
      kTestInputGainPercent,
      fake_observer->last_audio_system_properties_.value()->input_gain_percent);
}

TEST_F(CrosAudioConfigImplTest, GetSetOutputVolumePercentMuteThresholdTest) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  size_t expected_observer_calls = 1u;

  // |fake_observer| count is first incremented in Observe() method.
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  ASSERT_EQ(kDefaultOutputVolumePercent,
            fake_observer->last_audio_system_properties_.value()
                ->output_volume_percent);

  // Test setting volume over mute threshold when muted.
  SetOutputMuteState(mojom::MuteState::kMutedByUser);
  expected_observer_calls++;
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(
      mojom::MuteState::kMutedByUser,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);

  SetOutputVolumePercent(kDefaultOutputVolumePercent);

  // |fake_observer| should be notified twice due to mute state changing when
  // setting volume over the mute threshold.
  expected_observer_calls += 2u;
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(
      mojom::MuteState::kNotMuted,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);
  EXPECT_EQ(kDefaultOutputVolumePercent,
            fake_observer->last_audio_system_properties_.value()
                ->output_volume_percent);

  // Test setting volume under mute threshold when muted.
  expected_observer_calls++;
  SetOutputMuteState(mojom::MuteState::kMutedByUser);
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(
      mojom::MuteState::kMutedByUser,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);

  SetOutputVolumePercent(kTestUnderMuteThreshholdVolumePercent);
  expected_observer_calls++;
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(
      mojom::MuteState::kMutedByUser,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);
  EXPECT_EQ(kTestUnderMuteThreshholdVolumePercent,
            fake_observer->last_audio_system_properties_.value()
                ->output_volume_percent);
}

TEST_F(CrosAudioConfigImplTest, SetInputGainPercentWhileMuted) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  size_t expected_observer_calls = 1u;

  // |fake_observer| count is first incremented in Observe() method.
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  ASSERT_EQ(
      kDefaultInputGainPercent,
      fake_observer->last_audio_system_properties_.value()->input_gain_percent);

  // Test setting gain when muted.
  SetInputMuteState(mojom::MuteState::kMutedByUser);
  expected_observer_calls++;
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(
      mojom::MuteState::kMutedByUser,
      fake_observer->last_audio_system_properties_.value()->input_mute_state);

  SetInputGainPercentFromFrontEnd(kDefaultInputGainPercent);

  // |fake_observer| should be notified twice due to mute state changing when
  // setting gain.
  expected_observer_calls += 2u;
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(
      mojom::MuteState::kNotMuted,
      fake_observer->last_audio_system_properties_.value()->input_mute_state);
  EXPECT_EQ(
      kDefaultInputGainPercent,
      fake_observer->last_audio_system_properties_.value()->input_gain_percent);
}

TEST_F(CrosAudioConfigImplTest, GetSetOutputVolumePercentVolumeBoundariesTest) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  size_t expected_observer_calls = 1u;

  // |fake_observer| count is first incremented in Observe() method.
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  ASSERT_EQ(kDefaultOutputVolumePercent,
            fake_observer->last_audio_system_properties_.value()
                ->output_volume_percent);

  // Test setting volume over max volume.
  SetOutputVolumePercent(kTestOverMaxOutputVolumePercent);
  expected_observer_calls++;
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(100u, fake_observer->last_audio_system_properties_.value()
                      ->output_volume_percent);

  // Test setting volume under min volume.
  SetOutputVolumePercent(kTestUnderMinOutputVolumePercent);
  expected_observer_calls++;
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(0u, fake_observer->last_audio_system_properties_.value()
                    ->output_volume_percent);
}

TEST_F(CrosAudioConfigImplTest, GetOutputMuteState) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  size_t expected_observer_calls = 1u;
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  EXPECT_EQ(
      mojom::MuteState::kNotMuted,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);

  SetOutputMuteState(mojom::MuteState::kMutedByUser);
  expected_observer_calls++;
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(
      mojom::MuteState::kMutedByUser,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);

  SetOutputMuteState(mojom::MuteState::kNotMuted);
  expected_observer_calls++;
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(
      mojom::MuteState::kNotMuted,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);
}

TEST_F(CrosAudioConfigImplTest, HandleOutputMuteStateMutedByPolicy) {
  SetOutputMuteState(mojom::MuteState::kMutedByPolicy);
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  size_t expected_observer_calls = 1u;
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  EXPECT_EQ(
      mojom::MuteState::kMutedByPolicy,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);

  // Simulate attempting to change mute state while policy is enabled.
  SimulateSetOutputMuted(/*muted=*/true);
  SimulateSetOutputMuted(/*muted=*/false);
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  EXPECT_EQ(
      mojom::MuteState::kMutedByPolicy,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);
}

TEST_F(CrosAudioConfigImplTest, SetNoiseCancellationState) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();

  // By default noise cancellation is disabled and not supported in this test.
  ASSERT_FALSE(GetNoiseCancellationSupported());
  ASSERT_FALSE(GetNoiseCancellationState());
  ASSERT_FALSE(GetNoiseCancellationStatePref());

  // Simulate trying to set noise cancellation.
  SimulateSetNoiseCancellationEnabled(/*enabled=*/true);

  // Since noise cancellation is not supported, nothing is set.
  ASSERT_FALSE(GetNoiseCancellationState());
  bool expect_noise_cancellation_enabled = true;
  histogram_tester_.ExpectBucketCount(kNoiseCancellationEnabledHistogramName,
                                      expect_noise_cancellation_enabled, 0);
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kNoiseCancellationEnabledSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kOsSettings, 0);

  // Turn on noise cancellation support.
  SetNoiseCancellationSupported(/*supported=*/true);
  ASSERT_TRUE(GetNoiseCancellationSupported());

  // Now turning on noise cancellation should work.
  SimulateSetNoiseCancellationEnabled(/*enabled=*/true);
  histogram_tester_.ExpectBucketCount(kNoiseCancellationEnabledHistogramName,
                                      expect_noise_cancellation_enabled, 1);
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kNoiseCancellationEnabledSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kOsSettings, 1);

  // Add input audio nodes.
  SetAudioNodes({kInternalMic, kUsbMic});
  SetActiveInputNodes({kInternalMicId});

  ASSERT_TRUE(GetNoiseCancellationState());
  ASSERT_TRUE(GetNoiseCancellationStatePref());
  ASSERT_EQ(mojom::AudioEffectState::kEnabled,
            fake_observer->GetInputAudioDevice(1)->noise_cancellation_state);

  // Change active node does not change noise cancellation state.
  SetActiveInputNodes({kUsbMicId});

  ASSERT_EQ(mojom::AudioEffectState::kEnabled,
            fake_observer->GetInputAudioDevice(1)->noise_cancellation_state);

  // Frontend call to turn off noise cancellation ignored when active input node
  // does not support noise cancellation.
  SimulateSetNoiseCancellationEnabled(/*enabled=*/false);

  ASSERT_TRUE(GetNoiseCancellationState());
  ASSERT_TRUE(GetNoiseCancellationStatePref());
  expect_noise_cancellation_enabled = false;
  histogram_tester_.ExpectBucketCount(kNoiseCancellationEnabledHistogramName,
                                      expect_noise_cancellation_enabled, 0);
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kNoiseCancellationEnabledSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kOsSettings, 1);

  // Turn noise cancellation off with active input device that supports noise
  // cancellation.
  SetActiveInputNodes({kInternalMicId});
  SimulateSetNoiseCancellationEnabled(/*enabled=*/false);

  ASSERT_FALSE(GetNoiseCancellationState());
  ASSERT_FALSE(GetNoiseCancellationStatePref());
  ASSERT_EQ(mojom::AudioEffectState::kNotEnabled,
            fake_observer->GetInputAudioDevice(1)->noise_cancellation_state);
  histogram_tester_.ExpectBucketCount(kNoiseCancellationEnabledHistogramName,
                                      expect_noise_cancellation_enabled, 1);
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kNoiseCancellationEnabledSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kOsSettings, 2);
}

TEST_F(CrosAudioConfigImplTest, SetStyleTransferState) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();

  // Add input audio nodes.
  SetAudioNodes({kInternalMicStyleTransfer});
  SetActiveInputNodes({kInternalMicStyleTransferId});

  // By default style transfer is disabled and not supported in this test.
  ASSERT_FALSE(GetStyleTransferSupported());
  ASSERT_FALSE(GetStyleTransferState());
  ASSERT_FALSE(GetStyleTransferStatePref());

  // Simulate trying to set style transfer.
  SimulateSetStyleTransferEnabled(/*enabled=*/true);

  // Since style transfer is not supported, nothing is set.
  ASSERT_FALSE(GetStyleTransferState());

  // Turn on style transfer support.
  SetStyleTransferSupported(/*supported=*/true);
  ASSERT_TRUE(GetStyleTransferSupported());

  // Now turning on style transfer should work.
  SimulateSetStyleTransferEnabled(/*enabled=*/true);
  ASSERT_TRUE(GetStyleTransferState());

  // Add input audio nodes.
  SetAudioNodes({kInternalMicStyleTransfer, kUsbMic});
  SetActiveInputNodes({kInternalMicStyleTransferId});

  ASSERT_TRUE(GetStyleTransferState());
  ASSERT_TRUE(GetStyleTransferStatePref());
  ASSERT_EQ(mojom::AudioEffectState::kEnabled,
            fake_observer->GetInputAudioDevice(1)->style_transfer_state);

  // Change active node does not change style transfer state.
  SetActiveInputNodes({kUsbMicId});

  ASSERT_EQ(mojom::AudioEffectState::kEnabled,
            fake_observer->GetInputAudioDevice(1)->style_transfer_state);

  // Frontend call to turn off style transfer ignored when active input node
  // does not support style transfer.
  SimulateSetStyleTransferEnabled(/*enabled=*/false);

  ASSERT_TRUE(GetStyleTransferState());
  ASSERT_TRUE(GetStyleTransferStatePref());

  // Turn style transfer off with active input device that supports style
  // transfer.
  SetActiveInputNodes({kInternalMicStyleTransferId});
  SimulateSetStyleTransferEnabled(/*enabled=*/false);

  ASSERT_FALSE(GetStyleTransferState());
  ASSERT_FALSE(GetStyleTransferStatePref());
  ASSERT_EQ(mojom::AudioEffectState::kNotEnabled,
            fake_observer->GetInputAudioDevice(1)->style_transfer_state);
}

TEST_F(CrosAudioConfigImplTest, SetForceRespectUiGainsState) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();

  // By default force respect ui gains is disabled in this test.
  ASSERT_FALSE(GetForceRespectUiGainsState());
  ASSERT_FALSE(GetForceRespectUiGainsStatePref());

  // Simulate trying to set force respect ui gains.
  SimulateSetForceRespectUiGainsEnabled(/*enabled=*/true);

  // Add input audio nodes.
  SetAudioNodes({kInternalMic, kUsbMic});
  SetActiveInputNodes({kInternalMicId});

  ASSERT_TRUE(GetForceRespectUiGainsState());
  ASSERT_TRUE(GetForceRespectUiGainsStatePref());
  ASSERT_EQ(
      mojom::AudioEffectState::kEnabled,
      fake_observer->GetInputAudioDevice(1)->force_respect_ui_gains_state);

  // Change active node does not change force respect ui gains state.
  SetActiveInputNodes({kUsbMicId});
  ASSERT_EQ(
      mojom::AudioEffectState::kEnabled,
      fake_observer->GetInputAudioDevice(1)->force_respect_ui_gains_state);

  // Turn force respect ui gains off.
  SetActiveInputNodes({kInternalMicId});
  SimulateSetForceRespectUiGainsEnabled(/*enabled=*/false);

  ASSERT_FALSE(GetForceRespectUiGainsState());
  ASSERT_FALSE(GetForceRespectUiGainsStatePref());
  ASSERT_EQ(
      mojom::AudioEffectState::kNotEnabled,
      fake_observer->GetInputAudioDevice(1)->force_respect_ui_gains_state);
}

TEST_F(CrosAudioConfigImplTest, SetHfpMicSrState) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();

  // By default hfp_mic_sr is disabled and not supported in this test.
  ASSERT_FALSE(GetHfpMicSrSupported());
  ASSERT_FALSE(GetHfpMicSrState());
  ASSERT_FALSE(GetHfpMicSrStatePref());

  // Simulate trying to set hfp-mic-sr.
  SimulateSetHfpMicSrEnabled(/*enabled=*/true);

  // Since hfp_mic_sr is not supported, nothing is set.
  ASSERT_FALSE(GetHfpMicSrState());

  // Turn on hfp_mic_sr support.
  SetHfpMicSrSupported(/*supported=*/true);
  ASSERT_TRUE(GetHfpMicSrSupported());

  // Add input audio nodes.
  SetAudioNodes({kBluetoothNbMic, kUsbMic});
  SetActiveInputNodes({kBluetoothNbMicId});

  // Now turning on hfp_mic_sr should work.
  SimulateSetHfpMicSrEnabled(/*enabled=*/true);

  ASSERT_TRUE(GetHfpMicSrState());
  ASSERT_TRUE(GetHfpMicSrStatePref());
  ASSERT_EQ(mojom::AudioEffectState::kEnabled,
            fake_observer->GetInputAudioDevice(1)->hfp_mic_sr_state);

  // Change active node does not change hfp_mic_sr state.
  SetActiveInputNodes({kUsbMicId});

  ASSERT_EQ(mojom::AudioEffectState::kEnabled,
            fake_observer->GetInputAudioDevice(1)->hfp_mic_sr_state);

  // Frontend call to turn off hfp_mic_sr ignored when active input node
  // does not support hfp_mic_sr.
  SimulateSetHfpMicSrEnabled(/*enabled=*/false);

  ASSERT_TRUE(GetHfpMicSrState());
  ASSERT_TRUE(GetHfpMicSrStatePref());

  // Turn hfp_mic_sr off with active input device that supports hfp_mic_sr.
  SetActiveInputNodes({kBluetoothNbMicId});
  SimulateSetHfpMicSrEnabled(/*enabled=*/false);

  ASSERT_FALSE(GetHfpMicSrState());
  ASSERT_FALSE(GetHfpMicSrStatePref());
  ASSERT_EQ(mojom::AudioEffectState::kNotEnabled,
            fake_observer->GetInputAudioDevice(1)->hfp_mic_sr_state);
}

TEST_F(CrosAudioConfigImplTest, GetOutputAudioDevices) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  size_t expected_observer_calls = 1u;
  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);

  // Test default audio node list, which includes one input and one output node.
  SetAudioNodes({kInternalSpeaker, kMicJack});
  // Multiple calls to observer triggered by setting active nodes triggered by
  // AudioObserver events volume, gain, active output, active input, and nodes
  // changed
  expected_observer_calls += 5u;

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
  // AudioObserver events volume, active input(observer is still called if
  // there's no input device), active output, and nodes changed.
  expected_observer_calls += 4u;

  ASSERT_EQ(expected_observer_calls,
            fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  EXPECT_EQ(0u, fake_observer->last_audio_system_properties_.value()
                    ->input_devices.size());

  InsertAudioNode(kMicJack);
  // Multiple calls to observer triggered by setting active nodes triggered by
  // AudioObserver events gain, active input and nodes changed.
  expected_observer_calls += 3u;

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
  expected_observer_calls += 2u;

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
  histogram_tester_.ExpectBucketCount(kAudioDeviceChangeHistogramName,
                                      AudioDeviceChange::kOutputDevice, 1);
  histogram_tester_.ExpectBucketCount(kAudioDeviceChangeHistogramName,
                                      AudioDeviceChange::kInputDevice, 0);
  histogram_tester_.ExpectBucketCount(kOutputDeviceTypeHistogramName,
                                      AudioDeviceType::kHdmi, 1);
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
  histogram_tester_.ExpectBucketCount(kAudioDeviceChangeHistogramName,
                                      AudioDeviceChange::kOutputDevice, 0);
  histogram_tester_.ExpectBucketCount(kAudioDeviceChangeHistogramName,
                                      AudioDeviceChange::kInputDevice, 1);
  histogram_tester_.ExpectBucketCount(kInputDeviceTypeHistogramName,
                                      AudioDeviceType::kUsb, 1);
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
  histogram_tester_.ExpectBucketCount(kOutputMuteChangeHistogramName,
                                      AudioMuteButtonAction::kMuted, 1);
  histogram_tester_.ExpectBucketCount(kOutputMuteChangeHistogramName,
                                      AudioMuteButtonAction::kUnmuted, 0);

  SimulateSetOutputMuted(/*muted=*/false);
  EXPECT_EQ(
      mojom::MuteState::kNotMuted,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);
  EXPECT_FALSE(GetDeviceMuted(kInternalSpeakerId));
  EXPECT_FALSE(GetDeviceMuted(kHDMIOutputId));
  histogram_tester_.ExpectBucketCount(kOutputMuteChangeHistogramName,
                                      AudioMuteButtonAction::kMuted, 1);
  histogram_tester_.ExpectBucketCount(kOutputMuteChangeHistogramName,
                                      AudioMuteButtonAction::kUnmuted, 1);
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kOutputVolumeMuteSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kOsSettings, 2);
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
  histogram_tester_.ExpectBucketCount(kInputMuteChangeHistogramName,
                                      AudioMuteButtonAction::kMuted, 1);
  histogram_tester_.ExpectBucketCount(kInputMuteChangeHistogramName,
                                      AudioMuteButtonAction::kUnmuted, 0);

  SimulateSetInputMuted(/*muted=*/false);
  ASSERT_EQ(
      mojom::MuteState::kNotMuted,
      fake_observer->last_audio_system_properties_.value()->input_mute_state);
  histogram_tester_.ExpectBucketCount(kInputMuteChangeHistogramName,
                                      AudioMuteButtonAction::kMuted, 1);
  histogram_tester_.ExpectBucketCount(kInputMuteChangeHistogramName,
                                      AudioMuteButtonAction::kUnmuted, 1);
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kInputGainMuteSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kOsSettings, 2);

  // Simulate turning physical switch on.
  SetInputMuteState(mojom::MuteState::kMutedExternally, /*switch_on=*/true);

  // Simulate changing mute state while physical switch on.
  SimulateSetInputMuted(/*muted=*/true);
  ASSERT_EQ(
      mojom::MuteState::kMutedExternally,
      fake_observer->last_audio_system_properties_.value()->input_mute_state);
  histogram_tester_.ExpectBucketCount(kInputMuteChangeHistogramName,
                                      AudioMuteButtonAction::kMuted, 1);
  histogram_tester_.ExpectBucketCount(kInputMuteChangeHistogramName,
                                      AudioMuteButtonAction::kUnmuted, 1);

  SimulateSetInputMuted(/*muted=*/false);
  ASSERT_EQ(
      mojom::MuteState::kMutedExternally,
      fake_observer->last_audio_system_properties_.value()->input_mute_state);
  histogram_tester_.ExpectBucketCount(kInputMuteChangeHistogramName,
                                      AudioMuteButtonAction::kMuted, 1);
  histogram_tester_.ExpectBucketCount(kInputMuteChangeHistogramName,
                                      AudioMuteButtonAction::kUnmuted, 1);
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kInputGainMuteSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kOsSettings, 2);
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
  EXPECT_EQ(mojom::AudioDeviceType::kInternalMic, internal_mic->device_type);
  EXPECT_FALSE(internal_mic->is_active);

  SimulateSetActiveDevice(kInternalMicFrontId);

  EXPECT_EQ(expected_input_device_count,
            fake_observer->last_audio_system_properties_.value()
                ->input_devices.size());
  internal_mic = fake_observer->last_audio_system_properties_.value()
                     ->input_devices[1]
                     .Clone();
  EXPECT_EQ(mojom::AudioDeviceType::kInternalMic, internal_mic->device_type);
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
  EXPECT_EQ(mojom::AudioDeviceType::kInternalMic, internal_mic->device_type);
  EXPECT_TRUE(internal_mic->is_active);
}

TEST_F(CrosAudioConfigImplTest, NoiseCancellationAudioStateConfigured) {
  SetNoiseCancellationSupported(true);
  SetNoiseCancellationStatePref(false);
  SetAudioNodes({kInternalSpeaker, kInternalMic, kUsbMic});
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();

  // Noise cancellation supported by laptop and disabled in device wide
  // preference.
  EXPECT_EQ(mojom::AudioEffectState::kNotSupported,
            fake_observer->GetOutputAudioDevice(/*index=*/0)
                ->noise_cancellation_state);
  EXPECT_EQ(mojom::AudioEffectState::kNotEnabled,
            fake_observer->GetInputAudioDevice(/*index=*/1)
                ->noise_cancellation_state);
  EXPECT_EQ(mojom::AudioEffectState::kNotSupported,
            fake_observer->GetInputAudioDevice(/*index=*/0)
                ->noise_cancellation_state);

  // Set noise cancellation preference to enabled and force observer update
  // using `SetAudioNodes`.
  SetNoiseCancellationStatePref(true);
  // TODO(b/260277007): Remove calls to `SetAudioNodes` when observer for
  // noise cancellation state changed added available.
  SetAudioNodes({kInternalSpeaker, kInternalMic, kUsbMic});

  EXPECT_EQ(mojom::AudioEffectState::kNotSupported,
            fake_observer->GetOutputAudioDevice(/*index=*/0)
                ->noise_cancellation_state);
  EXPECT_EQ(mojom::AudioEffectState::kEnabled,
            fake_observer->GetInputAudioDevice(/*index=*/1)
                ->noise_cancellation_state);
  EXPECT_EQ(mojom::AudioEffectState::kNotSupported,
            fake_observer->GetInputAudioDevice(/*index=*/0)
                ->noise_cancellation_state);

  // Change overall device to not support noise cancellation and force observer
  // update using `SetAudioNodes`.
  SetNoiseCancellationSupported(false);
  SetAudioNodes({kInternalSpeaker, kInternalMic, kUsbMic});

  EXPECT_EQ(mojom::AudioEffectState::kNotSupported,
            fake_observer->GetOutputAudioDevice(/*index=*/0)
                ->noise_cancellation_state);
  EXPECT_EQ(mojom::AudioEffectState::kNotSupported,
            fake_observer->GetInputAudioDevice(/*index=*/0)
                ->noise_cancellation_state);
  EXPECT_EQ(mojom::AudioEffectState::kNotSupported,
            fake_observer->GetInputAudioDevice(/*index=*/1)
                ->noise_cancellation_state);
}

TEST_F(CrosAudioConfigImplTest, StyleTransferAudioStateConfigured) {
  SetStyleTransferSupported(true);
  SetStyleTransferStatePref(false);
  SetAudioNodes({kInternalSpeaker, kInternalMicStyleTransfer, kUsbMic});
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();

  // Style transfer supported by laptop and disabled in device wide
  // preference.
  EXPECT_EQ(
      mojom::AudioEffectState::kNotSupported,
      fake_observer->GetOutputAudioDevice(/*index=*/0)->style_transfer_state);
  EXPECT_EQ(
      mojom::AudioEffectState::kNotEnabled,
      fake_observer->GetInputAudioDevice(/*index=*/1)->style_transfer_state);
  EXPECT_EQ(
      mojom::AudioEffectState::kNotSupported,
      fake_observer->GetInputAudioDevice(/*index=*/0)->style_transfer_state);

  // Set style transfer preference to enabled and force observer update
  // using `SetAudioNodes`.
  SetStyleTransferStatePref(true);
  SetAudioNodes({kInternalSpeaker, kInternalMicStyleTransfer, kUsbMic});

  EXPECT_EQ(
      mojom::AudioEffectState::kNotSupported,
      fake_observer->GetOutputAudioDevice(/*index=*/0)->style_transfer_state);
  EXPECT_EQ(
      mojom::AudioEffectState::kEnabled,
      fake_observer->GetInputAudioDevice(/*index=*/1)->style_transfer_state);
  EXPECT_EQ(
      mojom::AudioEffectState::kNotSupported,
      fake_observer->GetInputAudioDevice(/*index=*/0)->style_transfer_state);

  // Change overall device to not support style transfer and force observer
  // update using `SetAudioNodes`.
  SetStyleTransferSupported(false);
  SetAudioNodes({kInternalSpeaker, kInternalMicStyleTransfer, kUsbMic});

  EXPECT_EQ(
      mojom::AudioEffectState::kNotSupported,
      fake_observer->GetOutputAudioDevice(/*index=*/0)->style_transfer_state);
  EXPECT_EQ(
      mojom::AudioEffectState::kNotSupported,
      fake_observer->GetInputAudioDevice(/*index=*/0)->style_transfer_state);
  EXPECT_EQ(
      mojom::AudioEffectState::kNotSupported,
      fake_observer->GetInputAudioDevice(/*index=*/1)->style_transfer_state);
}

TEST_F(CrosAudioConfigImplTest,
       ExternalUpdatesToNoiseCancellationStateObserved) {
  SetAudioNodes({kInternalMic, kUsbMic});
  SetActiveInputNodes({kInternalMicId});

  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  size_t expected_call_count = 1;
  EXPECT_EQ(expected_call_count, fake_observer->num_properties_updated_calls_);

  SetNoiseCancellationState(/*noise_cancellation_on=*/true);
  expected_call_count++;

  EXPECT_EQ(expected_call_count, fake_observer->num_properties_updated_calls_);
  expected_call_count++;

  SetNoiseCancellationState(/*noise_cancellation_on=*/false);

  EXPECT_EQ(expected_call_count, fake_observer->num_properties_updated_calls_);
}

TEST_F(CrosAudioConfigImplTest, SetOutputVolumeHistogram) {
  // Bind mojo remote.
  Observe();

  // Move the output volume up slider 3 times. Move the slider at half of the
  // delay interval time so each change shouldn't be recorded.
  SetOutputVolumePercent(/*volume_percent=*/10);
  task_environment()->FastForwardBy(kMetricsDelayTimerInterval / 2);
  SetOutputVolumePercent(/*volume_percent=*/20);
  task_environment()->FastForwardBy(kMetricsDelayTimerInterval / 2);
  SetOutputVolumePercent(/*volume_percent=*/30);

  task_environment()->FastForwardBy(kMetricsDelayTimerInterval);
  histogram_tester_.ExpectBucketCount(kOutputVolumeChangeHistogramName, 10, 0);
  histogram_tester_.ExpectBucketCount(kOutputVolumeChangeHistogramName, 20, 0);
  histogram_tester_.ExpectBucketCount(kOutputVolumeChangeHistogramName, 30, 1);
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kOutputVolumeChangedSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kOsSettings, 1);

  // Move the output volume up slider 2 times. Move the slider at half of the
  // delay interval time so each change shouldn't be recorded.
  SetOutputVolumePercent(/*volume_percent=*/50);
  task_environment()->FastForwardBy(kMetricsDelayTimerInterval / 2);
  SetOutputVolumePercent(/*volume_percent=*/100);

  task_environment()->FastForwardBy(kMetricsDelayTimerInterval);
  histogram_tester_.ExpectBucketCount(kOutputVolumeChangeHistogramName, 50, 0);
  histogram_tester_.ExpectBucketCount(kOutputVolumeChangeHistogramName, 100, 1);
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kOutputVolumeChangedSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kOsSettings, 2);
}

TEST_F(CrosAudioConfigImplTest, SetInputGainHistogram) {
  // Bind mojo remote.
  Observe();

  // Move the input gain up slider 3 times. Move the slider at half of the
  // delay interval time so each change shouldn't be recorded.
  SetInputGainPercentFromFrontEnd(/*gain_percent=*/10);
  task_environment()->FastForwardBy(kMetricsDelayTimerInterval / 2);
  SetInputGainPercentFromFrontEnd(/*gain_percent=*/20);
  task_environment()->FastForwardBy(kMetricsDelayTimerInterval / 2);
  SetInputGainPercentFromFrontEnd(/*gain_percent=*/30);

  task_environment()->FastForwardBy(kMetricsDelayTimerInterval);
  histogram_tester_.ExpectBucketCount(kInputGainChangeHistogramName, 10, 0);
  histogram_tester_.ExpectBucketCount(kInputGainChangeHistogramName, 20, 0);
  histogram_tester_.ExpectBucketCount(kInputGainChangeHistogramName, 30, 1);
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kInputGainChangedSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kOsSettings, 1);

  // Move the input gain up slider 2 times. Move the slider at half of the
  // delay interval time so each change shouldn't be recorded.
  SetInputGainPercentFromFrontEnd(/*gain_percent=*/50);
  task_environment()->FastForwardBy(kMetricsDelayTimerInterval / 2);
  SetInputGainPercentFromFrontEnd(/*gain_percent=*/100);

  task_environment()->FastForwardBy(kMetricsDelayTimerInterval);
  histogram_tester_.ExpectBucketCount(kInputGainChangeHistogramName, 50, 0);
  histogram_tester_.ExpectBucketCount(kInputGainChangeHistogramName, 100, 1);
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kInputGainChangedSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kOsSettings, 2);
}

}  // namespace ash::audio_config
