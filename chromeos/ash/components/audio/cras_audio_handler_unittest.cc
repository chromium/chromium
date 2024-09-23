// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/cras_audio_handler.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/system/system_monitor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/audio/audio_devices_pref_handler.h"
#include "chromeos/ash/components/audio/audio_devices_pref_handler_stub.h"
#include "chromeos/ash/components/audio/audio_selection_notification_handler.h"
#include "chromeos/ash/components/dbus/audio/audio_node.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "media/base/video_facing.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/media_session/public/mojom/media_controller.mojom-test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"

namespace ash {

namespace {

const uint64_t kInternalSpeakerId = 10001;
const uint64_t kHeadphoneId = 10002;
const uint64_t kInternalMicId = 10003;
const uint64_t kUSBMicId1 = 10004;
const uint64_t kUSBMicId2 = 10005;
const uint64_t kBluetoothHeadsetId = 10006;
const uint64_t kHDMIOutputId = 10007;
const uint64_t kUSBHeadphoneId1 = 10008;
const uint64_t kUSBHeadphoneId2 = 10009;
const uint64_t kUSBHeadphoneId3 = 10010;
const uint64_t kMicJackId = 10011;
const uint64_t kKeyboardMicId = 10012;
const uint64_t kFrontMicId = 10013;
const uint64_t kRearMicId = 10014;
const uint64_t kBluetoothNbMicId = 10015;
const uint64_t kOtherId = 10016;
const uint64_t kUSBJabraSpeakerOutputId1 = 90003;
const uint64_t kUSBJabraSpeakerOutputId2 = 90004;
const uint64_t kUSBJabraSpeakerInputId1 = 90005;
const uint64_t kUSBJabraSpeakerInputId2 = 90006;
const uint64_t kUSBCameraInputId = 90007;

struct AudioNodeInfo {
  bool is_input;
  uint64_t id;
  const char* const device_name;
  const char* const type;
  const char* const name;
  const int32_t number_of_volume_steps;
};

const uint32_t kInputMaxSupportedChannels = 1;
const uint32_t kOutputMaxSupportedChannels = 2;

const uint32_t kInputAudioEffect = 1;
const uint32_t kOutputAudioEffect = 0;

const int kStepPercentage = 4;
const int kDefaultUnmuteVolumePercent = 4;

const AudioNodeInfo kInternalSpeaker[] = {{false, kInternalSpeakerId,
                                           "Fake Speaker", "INTERNAL_SPEAKER",
                                           "Speaker", 25}};

const AudioNodeInfo kHeadphone[] = {
    {false, kHeadphoneId, "Fake Headphone", "HEADPHONE", "Headphone", 25}};

const AudioNodeInfo kInternalMic[] = {
    {true, kInternalMicId, "Fake Mic", "INTERNAL_MIC", "Internal Mic", 0}};

const AudioNodeInfo kMicJack[] = {
    {true, kMicJackId, "Fake Mic Jack", "MIC", "Mic Jack", 0}};

const AudioNodeInfo kUSBMic1[] = {
    {true, kUSBMicId1, "Fake USB Mic 1", "USB", "USB Microphone 1", 0}};

const AudioNodeInfo kUSBMic2[] = {
    {true, kUSBMicId2, "Fake USB Mic 2", "USB", "USB Microphone 2", 0}};

const AudioNodeInfo kKeyboardMic[] = {{true, kKeyboardMicId,
                                       "Fake Keyboard Mic", "KEYBOARD_MIC",
                                       "Keyboard Mic", 0}};

const AudioNodeInfo kFrontMic[] = {
    {true, kFrontMicId, "Fake Front Mic", "FRONT_MIC", "Front Mic", 0}};

const AudioNodeInfo kRearMic[] = {
    {true, kRearMicId, "Fake Rear Mic", "REAR_MIC", "Rear Mic", 0}};

const AudioNodeInfo kOther[] = {
    {false, kOtherId, "Non Simple Output Device", "OTHER", "Other", 25}};

const AudioNodeInfo kBluetoothHeadset[] = {{false, kBluetoothHeadsetId,
                                            "Bluetooth Headset", "BLUETOOTH",
                                            "Bluetooth Headset 1", 25}};

const AudioNodeInfo kBluetoothNbMic[] = {{true, kBluetoothNbMicId,
                                          "Fake Bt Mic", "BLUETOOTH_NB_MIC",
                                          "Bluetooth Nb Mic", 0}};

const AudioNodeInfo kHDMIOutput[] = {
    {false, kHDMIOutputId, "HDMI output", "HDMI", "HDMI output", 25}};

const AudioNodeInfo kUSBHeadphone1[] = {
    {false, kUSBHeadphoneId1, "USB Headphone", "USB", "USB Headphone 1", 25}};

const AudioNodeInfo kUSBHeadphone2[] = {
    {false, kUSBHeadphoneId2, "USB Headphone", "USB", "USB Headphone 2", 16}};

const AudioNodeInfo kUSBHeadphone3[] = {
    {false, kUSBHeadphoneId3, "USB Headphone", "USB", "USB Headphone 3", 0}};

const AudioNodeInfo kUSBJabraSpeakerOutput1[] = {
    {false, kUSBJabraSpeakerOutputId1, "Jabra Speaker 1", "USB",
     "Jabra Speaker 1", 16}};

const AudioNodeInfo kUSBJabraSpeakerOutput2[] = {
    {false, kUSBJabraSpeakerOutputId2, "Jabra Speaker 2", "USB",
     "Jabra Speaker 2", 25}};

const AudioNodeInfo kUSBJabraSpeakerInput1[] = {{true, kUSBJabraSpeakerInputId1,
                                                 "Jabra Speaker 1", "USB",
                                                 "Jabra Speaker", 0}};

const AudioNodeInfo kUSBJabraSpeakerInput2[] = {{true, kUSBJabraSpeakerInputId2,
                                                 "Jabra Speaker 2", "USB",
                                                 "Jabra Speaker 2", 0}};

const AudioNodeInfo kUSBCameraInput[] = {
    {true, kUSBCameraInputId, "USB Camera", "USB", "USB Camera", 0}};

class FakeMediaControllerManager
    : public media_session::mojom::MediaControllerManagerInterceptorForTesting {
 public:
  FakeMediaControllerManager() = default;

  FakeMediaControllerManager(const FakeMediaControllerManager&) = delete;
  FakeMediaControllerManager& operator=(const FakeMediaControllerManager&) =
      delete;

  mojo::PendingRemote<media_session::mojom::MediaControllerManager>
  MakeRemote() {
    mojo::PendingRemote<media_session::mojom::MediaControllerManager> remote;
    receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  void CreateActiveMediaController(
      mojo::PendingReceiver<media_session::mojom::MediaController> receiver)
      override {}

  MOCK_METHOD0(SuspendAllSessions, void());

 private:
  // media_session::mojom::MediaControllerManagerInterceptorForTesting:
  MediaControllerManager* GetForwardingInterface() override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  mojo::ReceiverSet<media_session::mojom::MediaControllerManager> receivers_;
};

class TestObserver : public CrasAudioHandler::AudioObserver {
 public:
  TestObserver() = default;

  int active_output_node_changed_count() const {
    return active_output_node_changed_count_;
  }

  void reset_active_output_node_changed_count() {
    active_output_node_changed_count_ = 0;
  }

  int active_input_node_changed_count() const {
    return active_input_node_changed_count_;
  }

  void reset_active_input_node_changed_count() {
    active_input_node_changed_count_ = 0;
  }

  int audio_nodes_changed_count() const { return audio_nodes_changed_count_; }

  int output_mute_changed_count() const { return output_mute_changed_count_; }

  void reset_output_mute_changed_count() { output_mute_changed_count_ = 0; }

  int input_mute_changed_count() const { return input_mute_changed_count_; }

  int output_volume_changed_count() const {
    return output_volume_changed_count_;
  }

  void reset_output_volume_changed_count() { output_volume_changed_count_ = 0; }

  int input_gain_changed_count() const { return input_gain_changed_count_; }

  int output_channel_remixing_changed_count() const {
    return output_channel_remixing_changed_count_;
  }

  int noise_cancellation_state_change_count() const {
    return noise_cancellation_state_change_count_;
  }

  int style_transfer_state_change_count() const {
    return style_transfer_state_change_count_;
  }

  int hfp_mic_sr_state_change_count() const {
    return hfp_mic_sr_state_change_count_;
  }

  int output_started_change_count() const {
    return output_started_change_count_;
  }

  int output_stopped_change_count() const {
    return output_stopped_change_count_;
  }

  int nonchrome_output_started_change_count() const {
    return nonchrome_output_started_change_count_;
  }

  int nonchrome_output_stopped_change_count() const {
    return nonchrome_output_stopped_change_count_;
  }

  int number_of_arc_stream_changed_latest_value() const {
    return number_of_arc_stream_changed_latest_value_;
  }

  int number_of_arc_stream_changed_count() const {
    return number_of_arc_stream_changed_count_;
  }

  int survey_triggerd_count() const { return survey_triggerd_count_; }

  int input_muted_by_security_curtain_changed_count() const {
    return input_muted_by_security_curtain_changed_count_;
  }

  const CrasAudioHandler::AudioSurvey& survey_triggerd_recv() const {
    return survey_triggerd_recv_;
  }

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  ~TestObserver() override = default;

 protected:
  // CrasAudioHandler::AudioObserver overrides.
  void OnActiveOutputNodeChanged() override {
    ++active_output_node_changed_count_;
  }

  void OnActiveInputNodeChanged() override {
    ++active_input_node_changed_count_;
  }

  void OnAudioNodesChanged() override { ++audio_nodes_changed_count_; }

  void OnOutputMuteChanged(bool /* mute_on */) override {
    ++output_mute_changed_count_;
  }

  void OnInputMuteChanged(
      bool /* mute_on */,
      CrasAudioHandler::InputMuteChangeMethod /* method */) override {
    ++input_mute_changed_count_;
  }

  void OnOutputNodeVolumeChanged(uint64_t /* node_id */,
                                 int /* volume */) override {
    ++output_volume_changed_count_;
  }

  void OnInputNodeGainChanged(uint64_t /* node_id */, int /* gain */) override {
    ++input_gain_changed_count_;
  }

  void OnOutputChannelRemixingChanged(bool /* mono_on */) override {
    ++output_channel_remixing_changed_count_;
  }

  void OnNoiseCancellationStateChanged() override {
    ++noise_cancellation_state_change_count_;
  }

  void OnStyleTransferStateChanged() override {
    ++style_transfer_state_change_count_;
  }

  void OnHfpMicSrStateChanged() override { ++hfp_mic_sr_state_change_count_; }

  void OnOutputStarted() override { ++output_started_change_count_; }

  void OnOutputStopped() override { ++output_stopped_change_count_; }

  void OnNonChromeOutputStarted() override {
    ++nonchrome_output_started_change_count_;
  }

  void OnNonChromeOutputStopped() override {
    ++nonchrome_output_stopped_change_count_;
  }

  void OnForceRespectUiGainsStateChanged() override {}

  void OnSurveyTriggered(const CrasAudioHandler::AudioSurvey& survey) override {
    ++survey_triggerd_count_;
    survey_triggerd_recv_.set_type(survey.type());
    survey_triggerd_recv_.clear_data();

    for (const auto& it : survey.data()) {
      survey_triggerd_recv_.AddData(it.first, it.second);
    }
  }

  void OnNumberOfArcStreamsChanged(int32_t num) override {
    ++number_of_arc_stream_changed_count_;
    number_of_arc_stream_changed_latest_value_ = num;
  }

  void OnInputMutedBySecurityCurtainChanged(bool muted) override {
    ++input_muted_by_security_curtain_changed_count_;
  }

 private:
  int active_output_node_changed_count_ = 0;
  int active_input_node_changed_count_ = 0;
  int audio_nodes_changed_count_ = 0;
  int output_mute_changed_count_ = 0;
  int input_mute_changed_count_ = 0;
  int output_volume_changed_count_ = 0;
  int input_gain_changed_count_ = 0;
  int output_channel_remixing_changed_count_ = 0;
  int noise_cancellation_state_change_count_ = 0;
  int style_transfer_state_change_count_ = 0;
  int hfp_mic_sr_state_change_count_ = 0;
  int output_started_change_count_ = 0;
  int output_stopped_change_count_ = 0;
  int nonchrome_output_stopped_change_count_ = 0;
  int nonchrome_output_started_change_count_ = 0;
  int number_of_arc_stream_changed_latest_value_ = 0;
  int number_of_arc_stream_changed_count_ = 0;
  int survey_triggerd_count_ = 0;
  int input_muted_by_security_curtain_changed_count_ = 0;
  CrasAudioHandler::AudioSurvey survey_triggerd_recv_;
};

class SystemMonitorObserver
    : public base::SystemMonitor::DevicesChangedObserver {
 public:
  SystemMonitorObserver() : device_changes_received_(0) {}
  ~SystemMonitorObserver() override = default;

  void OnDevicesChanged(base::SystemMonitor::DeviceType device_type) override {
    if (device_type == base::SystemMonitor::DeviceType::DEVTYPE_AUDIO) {
      device_changes_received_++;
    }
  }

  int device_changes_received() const { return device_changes_received_; }

  void reset_count() { device_changes_received_ = 0; }

 private:
  int device_changes_received_;
};

class FakeVideoCaptureManager {
 public:
  FakeVideoCaptureManager() = default;

  FakeVideoCaptureManager(const FakeVideoCaptureManager&) = delete;
  FakeVideoCaptureManager& operator=(const FakeVideoCaptureManager&) = delete;

  virtual ~FakeVideoCaptureManager() = default;

  void AddObserver(media::VideoCaptureObserver* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveAllObservers() { observers_.Clear(); }

  void NotifyVideoCaptureStarted(media::VideoFacingMode facing) {
    for (auto& observer : observers_) {
      observer.OnVideoCaptureStarted(facing);
    }
  }

  void NotifyVideoCaptureStopped(media::VideoFacingMode facing) {
    for (auto& observer : observers_) {
      observer.OnVideoCaptureStopped(facing);
    }
  }

 private:
  base::ObserverList<media::VideoCaptureObserver>::Unchecked observers_;
};

}  // namespace

// Test param is the version of stabel device id used by audio node.
class CrasAudioHandlerTest : public testing::TestWithParam<int> {
 public:
  CrasAudioHandlerTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME) {}

  CrasAudioHandlerTest(const CrasAudioHandlerTest&) = delete;
  CrasAudioHandlerTest& operator=(const CrasAudioHandlerTest&) = delete;

  ~CrasAudioHandlerTest() override = default;

  void SetUp() override {
    fake_manager_ = std::make_unique<FakeMediaControllerManager>();
    system_monitor_.AddDevicesChangedObserver(&system_monitor_observer_);
    video_capture_manager_ = std::make_unique<FakeVideoCaptureManager>();
    message_center::MessageCenter::Initialize();
  }

  void TearDown() override {
    message_center::MessageCenter::Shutdown();
    system_monitor_.RemoveDevicesChangedObserver(&system_monitor_observer_);
    cras_audio_handler_->RemoveAudioObserver(test_observer_.get());
    test_observer_.reset();
    video_capture_manager_->RemoveAllObservers();
    video_capture_manager_.reset();
    CrasAudioHandler::Shutdown();
    audio_pref_handler_ = nullptr;
    CrasAudioClient::Shutdown();

    // We can't delete the `MicrophoneMuteSwitchMonitor` singleton, so we
    // must reset it to a predictable state to prevent the tests from
    // influencing each other.
    ui::MicrophoneMuteSwitchMonitor::Get()->SetMicrophoneMuteSwitchValue(false);
  }

  AudioNode GenerateAudioNode(const AudioNodeInfo* node_info) {
    uint64_t stable_device_id_v2 = GetParam() == 1 ? 0 : (node_info->id ^ 0xFF);
    uint64_t stable_device_id_v1 = node_info->id;
    return AudioNode(
        node_info->is_input, node_info->id, GetParam() == 2,
        stable_device_id_v1, stable_device_id_v2, node_info->device_name,
        node_info->type, node_info->name, false /* is_active*/,
        0 /* pluged_time */,
        node_info->is_input ? kInputMaxSupportedChannels
                            : kOutputMaxSupportedChannels,
        node_info->is_input ? kInputAudioEffect : kOutputAudioEffect,
        node_info->number_of_volume_steps);
  }

  AudioNodeList GenerateAudioNodeList(
      const std::vector<const AudioNodeInfo*> nodes) {
    AudioNodeList node_list;
    for (auto* node_info : nodes) {
      node_list.push_back(GenerateAudioNode(node_info));
    }
    return node_list;
  }

  void SetUpCrasAudioHandler(const AudioNodeList& audio_nodes) {
    CrasAudioClient::InitializeFake();
    fake_cras_audio_client()->SetAudioNodesForTesting(audio_nodes);
    audio_pref_handler_ = base::MakeRefCounted<AudioDevicesPrefHandlerStub>();
    CrasAudioHandler::Initialize(fake_manager_->MakeRemote(),
                                 audio_pref_handler_);
    cras_audio_handler_ = CrasAudioHandler::Get();
    test_observer_ = std::make_unique<TestObserver>();
    cras_audio_handler_->AddAudioObserver(test_observer_.get());
    video_capture_manager_->AddObserver(cras_audio_handler_);
    base::RunLoop().RunUntilIdle();
  }

  // Set up cras audio handlers with |audio_nodes| and set the active state of
  // |active_device_in_pref| as active and |activate_by_user| in the pref,
  // and rest of nodes in |audio_nodes_in_pref| as inactive.
  void SetupCrasAudioHandlerWithActiveNodeInPref(
      const AudioNodeList& audio_nodes,
      const AudioNodeList& audio_nodes_in_pref,
      const AudioDevice& active_device_in_pref,
      bool activate_by_user) {
    CrasAudioClient::InitializeFake();
    audio_pref_handler_ = base::MakeRefCounted<AudioDevicesPrefHandlerStub>();
    bool active;
    for (const AudioNode& node : audio_nodes_in_pref) {
      active = node.id == active_device_in_pref.id;
      audio_pref_handler_->SetDeviceActive(AudioDevice(node), active,
                                           activate_by_user);
    }

    bool activate_by;
    EXPECT_TRUE(audio_pref_handler_->GetDeviceActive(active_device_in_pref,
                                                     &active, &activate_by));
    EXPECT_TRUE(active);
    EXPECT_EQ(activate_by, activate_by_user);

    fake_cras_audio_client()->SetAudioNodesForTesting(audio_nodes);
    CrasAudioHandler::Initialize(fake_manager_->MakeRemote(),
                                 audio_pref_handler_);

    cras_audio_handler_ = CrasAudioHandler::Get();
    test_observer_ = std::make_unique<TestObserver>();
    cras_audio_handler_->AddAudioObserver(test_observer_.get());
    base::RunLoop().RunUntilIdle();
  }

  void SetUpCrasAudioHandlerWithPrimaryActiveNode(
      const AudioNodeList& audio_nodes,
      const AudioNode& primary_active_node) {
    CrasAudioClient::InitializeFake();
    fake_cras_audio_client()->SetAudioNodesForTesting(audio_nodes);
    fake_cras_audio_client()->SetActiveOutputNode(primary_active_node.id);
    audio_pref_handler_ = base::MakeRefCounted<AudioDevicesPrefHandlerStub>();
    CrasAudioHandler::Initialize(fake_manager_->MakeRemote(),
                                 audio_pref_handler_);
    cras_audio_handler_ = CrasAudioHandler::Get();
    test_observer_ = std::make_unique<TestObserver>();
    cras_audio_handler_->AddAudioObserver(test_observer_.get());
    base::RunLoop().RunUntilIdle();
  }

  void SetUpCrasAudioHandlerWithPrimaryActiveNodeAndNoiseCancellationState(
      const AudioNodeList& audio_nodes,
      const AudioNode& primary_active_node,
      bool noise_cancellation_enabled) {
    CrasAudioClient::InitializeFake();
    fake_cras_audio_client()->SetAudioNodesForTesting(audio_nodes);
    fake_cras_audio_client()->SetActiveOutputNode(primary_active_node.id);
    fake_cras_audio_client()->SetNoiseCancellationSupported(
        /*noise_cancellation_supported=*/true);
    audio_pref_handler_ = base::MakeRefCounted<AudioDevicesPrefHandlerStub>();
    audio_pref_handler_->SetNoiseCancellationState(noise_cancellation_enabled);
    CrasAudioHandler::Initialize(fake_manager_->MakeRemote(),
                                 audio_pref_handler_);
    cras_audio_handler_ = CrasAudioHandler::Get();
    test_observer_ = std::make_unique<TestObserver>();
    cras_audio_handler_->AddAudioObserver(test_observer_.get());
    base::RunLoop().RunUntilIdle();
  }

  void SetUpCrasAudioHandlerWithPrimaryActiveNodeAndStyleTransferState(
      const AudioNodeList& audio_nodes,
      const AudioNode& primary_active_node,
      bool style_transfer_enabled) {
    CrasAudioClient::InitializeFake();
    fake_cras_audio_client()->SetAudioNodesForTesting(audio_nodes);
    fake_cras_audio_client()->SetActiveOutputNode(primary_active_node.id);
    fake_cras_audio_client()->SetStyleTransferSupported(
        /*style_transfer_supported=*/true);
    audio_pref_handler_ = base::MakeRefCounted<AudioDevicesPrefHandlerStub>();
    audio_pref_handler_->SetStyleTransferState(style_transfer_enabled);
    CrasAudioHandler::Initialize(fake_manager_->MakeRemote(),
                                 audio_pref_handler_);
    cras_audio_handler_ = CrasAudioHandler::Get();
    test_observer_ = std::make_unique<TestObserver>();
    cras_audio_handler_->AddAudioObserver(test_observer_.get());
    base::RunLoop().RunUntilIdle();
  }

  void SetUpCrasAudioHandlerWithPrimaryActiveNodeAndHfpMicSrState(
      const AudioNodeList& audio_nodes,
      const AudioNode& primary_active_node,
      bool hfp_mic_sr_enabled) {
    CrasAudioClient::InitializeFake();
    fake_cras_audio_client()->SetAudioNodesForTesting(audio_nodes);
    fake_cras_audio_client()->SetActiveOutputNode(primary_active_node.id);
    fake_cras_audio_client()->SetHfpMicSrSupported(
        /*hfp_mic_sr_supported=*/true);
    audio_pref_handler_ = base::MakeRefCounted<AudioDevicesPrefHandlerStub>();
    audio_pref_handler_->SetHfpMicSrState(hfp_mic_sr_enabled);
    CrasAudioHandler::Initialize(fake_manager_->MakeRemote(),
                                 audio_pref_handler_);
    cras_audio_handler_ = CrasAudioHandler::Get();
    test_observer_ = std::make_unique<TestObserver>();
    cras_audio_handler_->AddAudioObserver(test_observer_.get());
    base::RunLoop().RunUntilIdle();
  }

  void ChangeAudioNodes(const AudioNodeList& audio_nodes) {
    fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
        audio_nodes);
    base::RunLoop().RunUntilIdle();
  }

  void VerifySystemMonitorWasCalled() {
    // System monitor uses PostTask internally, so we have to process messages
    // before verifying the expectation.
    base::RunLoop().RunUntilIdle();
    EXPECT_LE(1, system_monitor_observer_.device_changes_received());
  }

  const AudioDevice* GetDeviceFromId(uint64_t id) {
    return cras_audio_handler_->GetDeviceFromId(id);
  }

  int GetActiveDeviceCount() const {
    int num_active_nodes = 0;
    AudioDeviceList audio_devices;
    cras_audio_handler_->GetAudioDevices(&audio_devices);
    for (size_t i = 0; i < audio_devices.size(); ++i) {
      if (audio_devices[i].active) {
        ++num_active_nodes;
      }
    }
    return num_active_nodes;
  }

  void SetActiveHDMIRediscover() {
    cras_audio_handler_->SetActiveHDMIOutoutRediscoveringIfNecessary(true);
  }

  void SetHDMIRediscoverGracePeriodDuration(int duration_in_ms) {
    cras_audio_handler_->SetHDMIRediscoverGracePeriodForTesting(duration_in_ms);
  }

  bool IsDuringHDMIRediscoverGracePeriod() {
    return cras_audio_handler_->hdmi_rediscovering();
  }

  void RestartAudioClient() {
    cras_audio_handler_->AudioClientRestarted();
    base::RunLoop().RunUntilIdle();
  }

  void StartFrontFacingCamera() {
    video_capture_manager_->NotifyVideoCaptureStarted(
        media::MEDIA_VIDEO_FACING_USER);
  }

  void StopFrontFacingCamera() {
    video_capture_manager_->NotifyVideoCaptureStopped(
        media::MEDIA_VIDEO_FACING_USER);
  }

  void StartRearFacingCamera() {
    video_capture_manager_->NotifyVideoCaptureStarted(
        media::MEDIA_VIDEO_FACING_ENVIRONMENT);
  }

  void StopRearFacingCamera() {
    video_capture_manager_->NotifyVideoCaptureStopped(
        media::MEDIA_VIDEO_FACING_ENVIRONMENT);
  }

  bool output_mono_enabled() const {
    return cras_audio_handler_->output_mono_enabled_;
  }

  const AudioDeviceMap& GetAudioDeviceMap(bool is_current_device) {
    return cras_audio_handler_->GetAudioDevicesMapForTesting(is_current_device);
  }

  // Initialize one or several audio nodes. Expect expected_active_input_node
  // and expected_active_output_node to be activated.
  void SetupAudioNodesAndExpectActiveNodes(
      const std::vector<const AudioNodeInfo*> initial_nodes,
      const AudioNodeInfo* expected_active_input_node,
      const AudioNodeInfo* expected_active_output_node,
      const std::optional<bool> expected_has_alternative_input,
      const std::optional<bool> expected_has_alternative_output) {
    AudioNodeList audio_nodes = GenerateAudioNodeList(initial_nodes);
    SetUpCrasAudioHandler(audio_nodes);

    // Verify the audio devices size.
    AudioDeviceList audio_devices;
    cras_audio_handler_->GetAudioDevices(&audio_devices);
    EXPECT_EQ(audio_nodes.size(), audio_devices.size());

    // Verify expected_active_input_node has been selected as the active output.
    if (expected_active_input_node) {
      AudioDevice active_input;
      EXPECT_TRUE(
          cras_audio_handler_->GetPrimaryActiveInputDevice(&active_input));
      EXPECT_EQ(expected_active_input_node->id, active_input.id);
      EXPECT_EQ(expected_active_input_node->id,
                cras_audio_handler_->GetPrimaryActiveInputNode());
    }
    if (expected_has_alternative_input.has_value()) {
      EXPECT_EQ(expected_has_alternative_input.value(),
                cras_audio_handler_->has_alternative_input());
    }

    // Verify expected_active_output_node has been selected as the active
    // output.
    if (expected_active_output_node) {
      AudioDevice active_output;
      EXPECT_TRUE(
          cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
      EXPECT_EQ(expected_active_output_node->id, active_output.id);
      EXPECT_EQ(expected_active_output_node->id,
                cras_audio_handler_->GetPrimaryActiveOutputNode());
    }
    if (expected_has_alternative_output.has_value()) {
      EXPECT_EQ(expected_has_alternative_output.value(),
                cras_audio_handler_->has_alternative_output());
    }

    EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());
    EXPECT_EQ(0, test_observer_->active_input_node_changed_count());
    EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
    system_monitor_observer_.reset_count();
  }

  // Expect that |expected_active_device| is the current activated device.
  // |has_alternative_device| checks if there is other alternative device.
  void ExpectActiveDevice(bool is_input,
                          const AudioNodeInfo* expected_active_device,
                          bool has_alternative_device) {
    AudioDevice active_device;
    if (is_input) {
      EXPECT_TRUE(
          cras_audio_handler_->GetPrimaryActiveInputDevice(&active_device));
      EXPECT_EQ(expected_active_device->id,
                cras_audio_handler_->GetPrimaryActiveInputNode());
      EXPECT_EQ(has_alternative_device,
                cras_audio_handler_->has_alternative_input());
    } else {
      EXPECT_TRUE(
          cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_device));
      EXPECT_EQ(expected_active_device->id,
                cras_audio_handler_->GetPrimaryActiveOutputNode());
      EXPECT_EQ(has_alternative_device,
                cras_audio_handler_->has_alternative_output());
    }
    EXPECT_EQ(expected_active_device->id, active_device.id);
    EXPECT_TRUE(active_device.active);
  }

  // Get the count of audio selection notification.
  size_t GetNotificationCount() {
    auto* message_center = message_center::MessageCenter::Get();
    return message_center->NotificationCount();
  }

  // Gets the title of audio selection notification. If not found, return
  // std::nullopt.
  const std::optional<std::u16string> GetNotificationTitle() {
    auto* message_center = message_center::MessageCenter::Get();
    message_center::Notification* notification =
        message_center->FindNotificationById(
            AudioSelectionNotificationHandler::kAudioSelectionNotificationId);
    return notification ? std::make_optional(notification->title())
                        : std::nullopt;
  }

  // Helper function to call SyncDevicePrefSetMap.
  void SyncDevicePrefSetMap(bool is_input) {
    cras_audio_handler_->SyncDevicePrefSetMap(is_input);
  }

  // Retrieves input_device_pref_set_map_ or output_device_pref_set_map_.
  const std::map<std::string, std::string>& GetDevicePrefSetMap() {
    return audio_pref_handler_->GetDevicePreferenceSetMap();
  }

  // Mock time fast forward.
  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  // Helper function to call GetDeviceFromStableDeviceId.
  std::optional<AudioDevice> GetDeviceFromStableDeviceId(
      bool is_input,
      uint64_t stable_device_id) {
    const AudioDevice* device =
        cras_audio_handler_->GetDeviceFromStableDeviceId(is_input,
                                                         stable_device_id);
    if (device) {
      return *device;
    } else {
      return std::nullopt;
    }
  }

 protected:
  FakeCrasAudioClient* fake_cras_audio_client() {
    return FakeCrasAudioClient::Get();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::SystemMonitor system_monitor_;
  SystemMonitorObserver system_monitor_observer_;
  raw_ptr<CrasAudioHandler, DanglingUntriaged> cras_audio_handler_ =
      nullptr;  // Not owned.
  std::unique_ptr<TestObserver> test_observer_;
  scoped_refptr<AudioDevicesPrefHandlerStub> audio_pref_handler_;
  std::unique_ptr<FakeMediaControllerManager> fake_manager_;
  std::unique_ptr<FakeVideoCaptureManager> video_capture_manager_;
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class HDMIRediscoverWaiter {
 public:
  HDMIRediscoverWaiter(CrasAudioHandlerTest* cras_audio_handler_test,
                       int grace_period_duration_in_ms)
      : cras_audio_handler_test_(cras_audio_handler_test),
        grace_period_duration_in_ms_(grace_period_duration_in_ms) {}

  HDMIRediscoverWaiter(const HDMIRediscoverWaiter&) = delete;
  HDMIRediscoverWaiter& operator=(const HDMIRediscoverWaiter&) = delete;

  void WaitUntilTimeOut(int wait_duration_in_ms) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::Milliseconds(wait_duration_in_ms));
    run_loop.Run();
  }

  void CheckHDMIRediscoverGracePeriodEnd(base::OnceClosure quit_loop_func) {
    if (!cras_audio_handler_test_->IsDuringHDMIRediscoverGracePeriod()) {
      std::move(quit_loop_func).Run();
      return;
    }
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&HDMIRediscoverWaiter::CheckHDMIRediscoverGracePeriodEnd,
                       base::Unretained(this), std::move(quit_loop_func)),
        base::Milliseconds(grace_period_duration_in_ms_ / 4));
  }

  void WaitUntilHDMIRediscoverGracePeriodEnd() {
    base::RunLoop run_loop;
    CheckHDMIRediscoverGracePeriodEnd(run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  raw_ptr<CrasAudioHandlerTest> cras_audio_handler_test_;  // not owned
  int grace_period_duration_in_ms_;
};

INSTANTIATE_TEST_SUITE_P(StableIdV1, CrasAudioHandlerTest, testing::Values(1));
INSTANTIATE_TEST_SUITE_P(StableIdV2, CrasAudioHandlerTest, testing::Values(2));

TEST_P(CrasAudioHandlerTest, InitializeWithOnlyDefaultAudioDevices) {
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kInternalMic},
      /*expected_active_input_node=*/kInternalMic,
      /*expected_active_output_node=*/kInternalSpeaker,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/false);
}

TEST_P(CrasAudioHandlerTest, InitializeWithAlternativeAudioDevices) {
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kHeadphone, kInternalMic, kUSBMic1},
      /*expected_active_input_node=*/kUSBMic1,
      /*expected_active_output_node=*/kHeadphone,
      /*expected_has_alternative_input=*/true,
      /*expected_has_alternative_output=*/true);
}

TEST_P(CrasAudioHandlerTest, InitializeWithKeyboardMic) {
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kInternalMic, kKeyboardMic},
      /*expected_active_input_node=*/kInternalMic,
      /*expected_active_output_node=*/kInternalSpeaker,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/false);

  // Ensure keyboard_mic is not active.
  const AudioDevice* keyboard_mic = GetDeviceFromId(kKeyboardMic->id);
  EXPECT_FALSE(keyboard_mic->active);
}

TEST_P(CrasAudioHandlerTest, SetKeyboardMicActive) {
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalMic, kKeyboardMic},
      /*expected_active_input_node=*/kInternalMic,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/std::nullopt);

  // Ensure keyboard_mic is not active.
  const AudioDevice* keyboard_mic = GetDeviceFromId(kKeyboardMic->id);
  EXPECT_FALSE(keyboard_mic->active);

  // Make keyboard mic active.
  cras_audio_handler_->SetKeyboardMicActive(true);
  EXPECT_EQ(kInternalMic->id, cras_audio_handler_->GetPrimaryActiveInputNode());
  const AudioDevice* active_keyboard_mic = GetDeviceFromId(kKeyboardMic->id);
  EXPECT_TRUE(active_keyboard_mic->active);

  // Make keyboard mic inactive.
  cras_audio_handler_->SetKeyboardMicActive(false);
  EXPECT_EQ(kInternalMic->id, cras_audio_handler_->GetPrimaryActiveInputNode());
  const AudioDevice* inactive_keyboard_mic = GetDeviceFromId(kKeyboardMic->id);
  EXPECT_FALSE(inactive_keyboard_mic->active);
}

TEST_P(CrasAudioHandlerTest, KeyboardMicNotSetAsPrimaryActive) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kKeyboardMic});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify keyboard mic is not set as primary active input.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_TRUE(cras_audio_handler_->HasKeyboardMic());
  EXPECT_EQ(0u, cras_audio_handler_->GetPrimaryActiveInputNode());

  // Verify the internal mic is set as primary input.
  audio_nodes.push_back(GenerateAudioNode(kInternalMic));
  ChangeAudioNodes(audio_nodes);
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_TRUE(cras_audio_handler_->HasKeyboardMic());
  EXPECT_EQ(kInternalMic->id, cras_audio_handler_->GetPrimaryActiveInputNode());
}

TEST_P(CrasAudioHandlerTest, SwitchActiveOutputDevice) {
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kHeadphone},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kHeadphone,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/true);

  // Switch the active output to internal speaker.
  AudioDevice internal_speaker(GenerateAudioNode(kInternalSpeaker));
  cras_audio_handler_->SwitchToDevice(internal_speaker, true,
                                      DeviceActivateType::kActivateByUser);

  // Verify the active output is switched to internal speaker, and the
  // ActiveOutputNodeChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  VerifySystemMonitorWasCalled();
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/true);
}

TEST_P(CrasAudioHandlerTest, SwitchActiveInputDevice) {
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalMic, kUSBMic1},
      /*expected_active_input_node=*/kUSBMic1,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/true,
      /*expected_has_alternative_output=*/std::nullopt);

  // Switch the active input to internal mic.
  AudioDevice internal_mic(GenerateAudioNode(kInternalMic));
  cras_audio_handler_->SwitchToDevice(internal_mic, true,
                                      DeviceActivateType::kActivateByUser);

  // Verify the active output is switched to internal speaker, and the active
  // ActiveInputNodeChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kInternalMic->id, cras_audio_handler_->GetPrimaryActiveInputNode());
}

TEST_P(CrasAudioHandlerTest, PlugHeadphone) {
  // Set up initial audio devices, only with internal speaker.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kInternalSpeaker,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/false);

  // Plug the headphone.
  AudioNodeList audio_nodes;
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);
  audio_nodes.push_back(GenerateAudioNode(kHeadphone));
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and new audio device is added.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  VerifySystemMonitorWasCalled();
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(2u, audio_devices.size());

  // Verify the active output device is switched to headphone and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false, /*expected_active_device=*/kHeadphone,
                     /*has_alternative_device=*/true);
}

TEST_P(CrasAudioHandlerTest, UnplugHeadphone) {
  // Set up initial audio devices, with internal speaker and headphone.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kHeadphone},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kHeadphone,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/true);

  // Unplug the headphone.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(GenerateAudioNode(kInternalSpeaker));
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and one audio device is
  // removed.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  VerifySystemMonitorWasCalled();
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(1u, audio_devices.size());

  // Verify the active output device is switched to internal speaker and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/false);
}

TEST_P(CrasAudioHandlerTest, InitializeWithBluetoothHeadset) {
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kBluetoothHeadset},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kBluetoothHeadset,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/true);
}

TEST_P(CrasAudioHandlerTest, ConnectAndDisconnectBluetoothHeadset) {
  // Initialize with internal speaker and headphone.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kHeadphone},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kHeadphone,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/true);

  // Connect to bluetooth headset. Since it is plugged in later than
  // headphone, active output should be switched to it.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(GenerateAudioNode(kInternalSpeaker));
  AudioNode headphone = GenerateAudioNode(kHeadphone);
  headphone.plugged_time = 80000000;
  headphone.active = true;
  audio_nodes.push_back(headphone);
  AudioNode bluetooth_headset = GenerateAudioNode(kBluetoothHeadset);
  bluetooth_headset.plugged_time = 90000000;
  audio_nodes.push_back(bluetooth_headset);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and new audio device is added.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  VerifySystemMonitorWasCalled();
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(3u, audio_devices.size());

  // Verify the active output device is switched to bluetooth headset, and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kBluetoothHeadset,
                     /*has_alternative_device=*/true);

  // Disconnect bluetooth headset.
  system_monitor_observer_.reset_count();
  audio_nodes.clear();
  audio_nodes.push_back(GenerateAudioNode(kInternalSpeaker));
  headphone.active = false;
  audio_nodes.push_back(headphone);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and one audio device is
  // removed.
  EXPECT_EQ(2, test_observer_->audio_nodes_changed_count());
  VerifySystemMonitorWasCalled();
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(2u, audio_devices.size());

  // Verify the active output device is switched to headphone, and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(2, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false, /*expected_active_device=*/kHeadphone,
                     /*has_alternative_device=*/true);
}

TEST_P(CrasAudioHandlerTest, NumberNonChromeOutputs) {
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kHDMIOutput});
  SetUpCrasAudioHandler(audio_nodes);
  // start at 0.
  EXPECT_EQ(test_observer_->nonchrome_output_started_change_count(), 0);
  EXPECT_EQ(test_observer_->nonchrome_output_stopped_change_count(), 0);
  EXPECT_EQ(cras_audio_handler_->NumberOfNonChromeOutputStreams(), 0);
  fake_cras_audio_client()->SetNumberOfNonChromeOutputStreams(1);
  EXPECT_EQ(test_observer_->nonchrome_output_started_change_count(), 1);
  EXPECT_EQ(test_observer_->nonchrome_output_stopped_change_count(), 0);
  EXPECT_EQ(cras_audio_handler_->NumberOfNonChromeOutputStreams(), 1);
  // And again, to 2. No change expected.
  fake_cras_audio_client()->SetNumberOfNonChromeOutputStreams(2);
  EXPECT_EQ(test_observer_->nonchrome_output_started_change_count(), 1);
  EXPECT_EQ(test_observer_->nonchrome_output_stopped_change_count(), 0);
  EXPECT_EQ(cras_audio_handler_->NumberOfNonChromeOutputStreams(), 2);
  // Down to 0? it gets stopped.
  fake_cras_audio_client()->SetNumberOfNonChromeOutputStreams(0);
  EXPECT_EQ(test_observer_->nonchrome_output_started_change_count(), 1);
  EXPECT_EQ(test_observer_->nonchrome_output_stopped_change_count(), 1);
  EXPECT_EQ(cras_audio_handler_->NumberOfNonChromeOutputStreams(), 0);
  // Down to 0 again for some reason: already stopped.
  fake_cras_audio_client()->SetNumberOfNonChromeOutputStreams(0);
  EXPECT_EQ(test_observer_->nonchrome_output_started_change_count(), 1);
  EXPECT_EQ(test_observer_->nonchrome_output_stopped_change_count(), 1);
  EXPECT_EQ(cras_audio_handler_->NumberOfNonChromeOutputStreams(), 0);
  // And again, to 2. Up we go.
  fake_cras_audio_client()->SetNumberOfNonChromeOutputStreams(2);
  EXPECT_EQ(test_observer_->nonchrome_output_started_change_count(), 2);
  EXPECT_EQ(test_observer_->nonchrome_output_stopped_change_count(), 1);
  EXPECT_EQ(cras_audio_handler_->NumberOfNonChromeOutputStreams(), 2);
}

TEST_P(CrasAudioHandlerTest, NumberArcStreams) {
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kHDMIOutput});
  SetUpCrasAudioHandler(audio_nodes);
  // start at 0.
  EXPECT_EQ(test_observer_->number_of_arc_stream_changed_latest_value(), 0);
  EXPECT_EQ(test_observer_->number_of_arc_stream_changed_count(), 0);
  EXPECT_EQ(cras_audio_handler_->NumberOfArcStreams(), 0);
  // Go up to 1.
  fake_cras_audio_client()->SetNumberOfArcStreams(1);
  EXPECT_EQ(test_observer_->number_of_arc_stream_changed_latest_value(), 1);
  EXPECT_EQ(test_observer_->number_of_arc_stream_changed_count(), 1);
  EXPECT_EQ(cras_audio_handler_->NumberOfArcStreams(), 1);
  // Go up to 2.
  fake_cras_audio_client()->SetNumberOfArcStreams(2);
  EXPECT_EQ(test_observer_->number_of_arc_stream_changed_latest_value(), 2);
  EXPECT_EQ(test_observer_->number_of_arc_stream_changed_count(), 2);
  EXPECT_EQ(cras_audio_handler_->NumberOfArcStreams(), 2);
  // Stay at 2, no callback expected.
  fake_cras_audio_client()->SetNumberOfArcStreams(2);
  EXPECT_EQ(test_observer_->number_of_arc_stream_changed_latest_value(), 2);
  EXPECT_EQ(test_observer_->number_of_arc_stream_changed_count(), 2);
  EXPECT_EQ(cras_audio_handler_->NumberOfArcStreams(), 2);
  // Down to 0
  fake_cras_audio_client()->SetNumberOfArcStreams(0);
  EXPECT_EQ(test_observer_->number_of_arc_stream_changed_latest_value(), 0);
  EXPECT_EQ(test_observer_->number_of_arc_stream_changed_count(), 3);
  EXPECT_EQ(cras_audio_handler_->NumberOfArcStreams(), 0);
}

TEST_P(CrasAudioHandlerTest, InitializeWithHDMIOutput) {
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kHDMIOutput},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kHDMIOutput,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/true);
}

TEST_P(CrasAudioHandlerTest, ConnectAndDisconnectHDMIOutput) {
  // Initialize with internal speaker.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kInternalSpeaker,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/false);

  // Connect to HDMI output.
  AudioNodeList audio_nodes;
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  internal_speaker.active = true;
  internal_speaker.plugged_time = 80000000;
  audio_nodes.push_back(internal_speaker);
  AudioNode hdmi = GenerateAudioNode(kHDMIOutput);
  hdmi.plugged_time = 90000000;
  audio_nodes.push_back(hdmi);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and new audio device is added.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  VerifySystemMonitorWasCalled();
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(2u, audio_devices.size());

  // Verify the active output device is switched to hdmi output, and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false, /*expected_active_device=*/kHDMIOutput,
                     /*has_alternative_device=*/true);
  system_monitor_observer_.reset_count();

  // Disconnect hdmi headset.
  audio_nodes.clear();
  audio_nodes.push_back(GenerateAudioNode(kInternalSpeaker));
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and one audio device is
  // removed.
  EXPECT_EQ(2, test_observer_->audio_nodes_changed_count());
  VerifySystemMonitorWasCalled();
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(1u, audio_devices.size());

  // Verify the active output device is switched to internal speaker, and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(2, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/false);
}

TEST_P(CrasAudioHandlerTest,
       ConnectAndDisconnectHDMIOutput_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  // Initialize with internal speaker.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kInternalSpeaker,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/false);

  // Expect that there is no notification when initializing the audio nodes.
  EXPECT_EQ(0u, GetNotificationCount());

  // Connect to HDMI output.
  AudioNodeList audio_nodes;
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  internal_speaker.active = true;
  internal_speaker.plugged_time = 80000000;
  audio_nodes.push_back(internal_speaker);
  AudioNode hdmi = GenerateAudioNode(kHDMIOutput);
  hdmi.plugged_time = 90000000;
  audio_nodes.push_back(hdmi);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and new audio device is added.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  VerifySystemMonitorWasCalled();
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(2u, audio_devices.size());

  // Verify the active output device is not switched to hdmi output, and
  // ActiveOutputChanged event is not fired.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/true);
  system_monitor_observer_.reset_count();

  // Verify notification shows up for unseen new connected HDMI device.
  FastForwardBy(AudioSelectionNotificationHandler::kDebounceTime);
  EXPECT_EQ(1u, GetNotificationCount());

  // Disconnect hdmi headset.
  audio_nodes.clear();
  audio_nodes.push_back(GenerateAudioNode(kInternalSpeaker));
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and one audio device is
  // removed.
  EXPECT_EQ(2, test_observer_->audio_nodes_changed_count());
  VerifySystemMonitorWasCalled();
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(1u, audio_devices.size());

  // Verify the active output device is still internal speaker, and
  // ActiveOutputChanged event is not fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/false);

  // Verify that the notification is removed because the hotplugged HDMI output
  // is disconnected.
  FastForwardBy(CrasAudioHandler::kRemoveNotificationDelay);
  EXPECT_EQ(0u, GetNotificationCount());
}

TEST_P(CrasAudioHandlerTest, HandleHeadphoneAndHDMIOutput) {
  // Initialize with internal speaker, headphone and HDMI output.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kHeadphone, kHDMIOutput},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kHeadphone,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/true);

  // Disconnect HDMI output.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(GenerateAudioNode(kInternalSpeaker));
  audio_nodes.push_back(GenerateAudioNode(kHDMIOutput));
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and one audio device is
  // removed.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(2u, audio_devices.size());

  // Verify the active output device is switched to HDMI output, and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false, /*expected_active_device=*/kHDMIOutput,
                     /*has_alternative_device=*/true);
}

TEST_P(CrasAudioHandlerTest, InitializeWithUSBHeadphone) {
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kUSBHeadphone1},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kUSBHeadphone1,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/true);
}

TEST_P(CrasAudioHandlerTest, PlugAndUnplugUSBHeadphone) {
  // Initialize with internal speaker.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kInternalSpeaker,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/false);

  // Plug in usb headphone
  AudioNodeList audio_nodes;
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  internal_speaker.active = true;
  internal_speaker.plugged_time = 80000000;
  audio_nodes.push_back(internal_speaker);
  AudioNode usb_headphone = GenerateAudioNode(kUSBHeadphone1);
  usb_headphone.plugged_time = 90000000;
  audio_nodes.push_back(usb_headphone);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and new audio device is added.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  VerifySystemMonitorWasCalled();
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(2u, audio_devices.size());

  // Verify the active output device is switched to usb headphone, and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kUSBHeadphone1,
                     /*has_alternative_device=*/true);
  system_monitor_observer_.reset_count();

  // Unplug usb headphone.
  audio_nodes.clear();
  audio_nodes.push_back(GenerateAudioNode(kInternalSpeaker));
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and one audio device is
  // removed.
  EXPECT_EQ(2, test_observer_->audio_nodes_changed_count());
  VerifySystemMonitorWasCalled();
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(1u, audio_devices.size());

  // Verify the active output device is switched to internal speaker, and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(2, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/false);
}

TEST_P(CrasAudioHandlerTest,
       PlugAndUnplugUSBHeadphone_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  // Initialize with internal speaker.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kInternalSpeaker,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/false);

  // Expect that there is no notification when initializing the audio nodes.
  EXPECT_EQ(0u, GetNotificationCount());

  // Plug in usb headphone
  AudioNodeList audio_nodes;
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  internal_speaker.active = true;
  internal_speaker.plugged_time = 80000000;
  audio_nodes.push_back(internal_speaker);
  AudioNode usb_headphone = GenerateAudioNode(kUSBHeadphone1);
  usb_headphone.plugged_time = 90000000;
  audio_nodes.push_back(usb_headphone);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and new audio device is added.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  VerifySystemMonitorWasCalled();
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(2u, audio_devices.size());

  // Verify the active output device is not switched to usb headphone, and
  // ActiveOutputChanged event is not fired.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/true);
  system_monitor_observer_.reset_count();

  // Verify notification shows up for unseen new connected USB headphone device.
  FastForwardBy(AudioSelectionNotificationHandler::kDebounceTime);
  EXPECT_EQ(1u, GetNotificationCount());

  // Unplug usb headphone.
  audio_nodes.clear();
  audio_nodes.push_back(internal_speaker);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and one audio device is
  // removed.
  EXPECT_EQ(2, test_observer_->audio_nodes_changed_count());
  VerifySystemMonitorWasCalled();
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(1u, audio_devices.size());

  // Verify the active output device is still internal speaker, and
  // ActiveOutputChanged event is not fired.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/false);

  // Verify that the notification is removed because the hotplugged USB output
  // is disconnected.
  FastForwardBy(CrasAudioHandler::kRemoveNotificationDelay);
  EXPECT_EQ(0u, GetNotificationCount());
}

TEST_P(CrasAudioHandlerTest, HandleMultipleUSBHeadphones) {
  // Initialize with internal speaker and one usb headphone.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kUSBHeadphone1},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kUSBHeadphone1,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/true);

  // Plug in another usb headphone.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(GenerateAudioNode(kInternalSpeaker));
  AudioNode usb_headphone_1 = GenerateAudioNode(kUSBHeadphone1);
  usb_headphone_1.active = true;
  usb_headphone_1.plugged_time = 80000000;
  audio_nodes.push_back(usb_headphone_1);
  AudioNode usb_headphone_2 = GenerateAudioNode(kUSBHeadphone2);
  usb_headphone_2.plugged_time = 90000000;
  audio_nodes.push_back(usb_headphone_2);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and new audio device is added.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(3u, audio_devices.size());

  // Verify the active output device is switched to the 2nd usb headphone, which
  // is plugged later, and ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kUSBHeadphone2,
                     /*has_alternative_device=*/true);

  // Unplug the 2nd usb headphone.
  audio_nodes.clear();
  audio_nodes.push_back(GenerateAudioNode(kInternalSpeaker));
  audio_nodes.push_back(GenerateAudioNode(kUSBHeadphone1));
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and one audio device is
  // removed.
  EXPECT_EQ(2, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(2u, audio_devices.size());

  // Verify the active output device is switched to the first usb headphone, and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(2, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kUSBHeadphone1,
                     /*has_alternative_device=*/true);
}

TEST_P(CrasAudioHandlerTest,
       HandleMultipleUSBHeadphones_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  // Initialize with internal speaker and one usb headphone.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kUSBHeadphone1},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kInternalSpeaker,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/true);

  // Expect that notification is displayed since the device set was unseen
  // before.
  FastForwardBy(AudioSelectionNotificationHandler::kDebounceTime);
  EXPECT_EQ(1u, GetNotificationCount());

  // Plug in another usb headphone.
  AudioNodeList audio_nodes;
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);
  AudioNode usb_headphone_1 = GenerateAudioNode(kUSBHeadphone1);
  usb_headphone_1.plugged_time = 80000000;
  audio_nodes.push_back(usb_headphone_1);
  AudioNode usb_headphone_2 = GenerateAudioNode(kUSBHeadphone2);
  usb_headphone_2.plugged_time = 90000000;
  audio_nodes.push_back(usb_headphone_2);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and new audio device is added.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(3u, audio_devices.size());

  // Verify the active output device is not switched to the 2nd usb headphone,
  // which is plugged later, and ActiveOutputChanged event is not fired.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/true);

  // Verify notification shows up for unseen new connected USB headphone device.
  FastForwardBy(AudioSelectionNotificationHandler::kDebounceTime);
  EXPECT_EQ(1u, GetNotificationCount());

  // Unplug the 2nd usb headphone.
  audio_nodes.clear();
  audio_nodes.push_back(internal_speaker);
  audio_nodes.push_back(usb_headphone_1);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and one audio device is
  // removed.
  EXPECT_EQ(2, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(2u, audio_devices.size());

  // Verify the active output device is not changed, and
  // ActiveOutputChanged event is not fired.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/true);

  // Verify that the notification is removed because the hotplugged USB output
  // is disconnected.
  FastForwardBy(CrasAudioHandler::kRemoveNotificationDelay);
  EXPECT_EQ(0u, GetNotificationCount());
}

TEST_P(CrasAudioHandlerTest, UnplugUSBHeadphonesWithActiveSpeaker) {
  // Initialize with internal speaker and one usb headphone.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kUSBHeadphone1},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kUSBHeadphone1,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/true);

  // Plug in the headphone jack.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(GenerateAudioNode(kInternalSpeaker));
  AudioNode usb_headphone_1 = GenerateAudioNode(kUSBHeadphone1);
  usb_headphone_1.active = true;
  usb_headphone_1.plugged_time = 80000000;
  audio_nodes.push_back(usb_headphone_1);
  AudioNode headphone_jack = GenerateAudioNode(kHeadphone);
  headphone_jack.plugged_time = 90000000;
  audio_nodes.push_back(headphone_jack);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and new audio device is added.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(3u, audio_devices.size());

  // Verify the active output device is switched to the headphone jack, which
  // is plugged later, and ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false, /*expected_active_device=*/kHeadphone,
                     /*has_alternative_device=*/true);

  // Select the speaker to be the active output device.
  AudioDevice internal_speaker(GenerateAudioNode(kInternalSpeaker));
  cras_audio_handler_->SwitchToDevice(internal_speaker, true,
                                      DeviceActivateType::kActivateByUser);

  // Verify the active output is switched to internal speaker, and the
  // ActiveOutputNodeChanged event is fired.
  EXPECT_EQ(2, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/true);

  // Unplug the usb headphone.
  audio_nodes.clear();
  AudioNode internal_speaker_node(GenerateAudioNode(kInternalSpeaker));
  internal_speaker_node.active = true;
  internal_speaker_node.plugged_time = 70000000;
  audio_nodes.push_back(internal_speaker_node);
  headphone_jack.active = false;
  audio_nodes.push_back(headphone_jack);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and one audio device is
  // removed.
  EXPECT_EQ(2, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(2u, audio_devices.size());

  // Verify the active output device remains to be speaker.
  EXPECT_EQ(2, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/true);
}

TEST_P(CrasAudioHandlerTest, OneActiveAudioOutputAfterLoginNewUserSession) {
  // This tests the case found with crbug.com/273271.
  // Initialize with internal speaker, bluetooth headphone and headphone jack
  // for a new chrome session after user signs out from the previous session.
  // Headphone jack is plugged in later than bluetooth headphone, but bluetooth
  // headphone is selected as the active output by user from previous user
  // session.
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalSpeaker});
  AudioNode bluetooth_headphone = GenerateAudioNode(kBluetoothHeadset);
  bluetooth_headphone.active = true;
  bluetooth_headphone.plugged_time = 70000000;
  audio_nodes.push_back(bluetooth_headphone);
  AudioNode headphone_jack = GenerateAudioNode(kHeadphone);
  headphone_jack.plugged_time = 80000000;
  audio_nodes.push_back(headphone_jack);
  SetUpCrasAudioHandlerWithPrimaryActiveNode(audio_nodes, bluetooth_headphone);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the headphone jack is selected as the active output and all other
  // audio devices are not active.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false, /*expected_active_device=*/kHeadphone,
                     /*has_alternative_device=*/true);
  for (size_t i = 0; i < audio_devices.size(); ++i) {
    if (audio_devices[i].id != kHeadphone->id) {
      EXPECT_FALSE(audio_devices[i].active);
    }
  }
}

TEST_P(CrasAudioHandlerTest, NoiseCancellationRefreshPrefEnabledNoNC) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({});
  // Set up initial audio devices, only with internal mic.
  AudioNode internalMic = GenerateAudioNode(kInternalMic);
  // Clear the audio effect, no Noise Cancellation supported.
  internalMic.audio_effect = 0u;
  audio_nodes.push_back(internalMic);
  // Simulate enable pref for noise cancellation.
  SetUpCrasAudioHandlerWithPrimaryActiveNodeAndNoiseCancellationState(
      audio_nodes, internalMic, /*noise_cancellation_enabled=*/true);

  // Noise cancellation should still be disabled despite the pref being enabled
  // since the audio_effect of the internal mic is unavailable.
  EXPECT_FALSE(fake_cras_audio_client()->noise_cancellation_enabled());
  EXPECT_TRUE(audio_pref_handler_->GetNoiseCancellationState());
}

TEST_P(CrasAudioHandlerTest, NoiseCancellationRefreshPrefEnabledWithNC) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({});
  // Set up initial audio devices, only with internal mic.
  AudioNode internalMic = GenerateAudioNode(kInternalMic);
  // Enable noise cancellation effect.
  internalMic.audio_effect = cras::EFFECT_TYPE_NOISE_CANCELLATION;
  audio_nodes.push_back(internalMic);
  // Simulate enable pref for noise cancellation.
  SetUpCrasAudioHandlerWithPrimaryActiveNodeAndNoiseCancellationState(
      audio_nodes, internalMic, /*noise_cancellation_enabled=*/true);

  // Noise Cancellation is enabled.
  EXPECT_TRUE(fake_cras_audio_client()->noise_cancellation_enabled());
  EXPECT_TRUE(audio_pref_handler_->GetNoiseCancellationState());
}

TEST_P(CrasAudioHandlerTest, NoiseCancellationRefreshPrefDisableNoNC) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({});
  // Set up initial audio devices, only with internal mic.
  AudioNode internalMic = GenerateAudioNode(kInternalMic);
  // Clear audio effect, no noise cancellation.
  internalMic.audio_effect = 0u;
  audio_nodes.push_back(internalMic);
  // Simulate enable pref for noise cancellation.
  SetUpCrasAudioHandlerWithPrimaryActiveNodeAndNoiseCancellationState(
      audio_nodes, internalMic, /*noise_cancellation_enabled=*/false);

  // Noise cancellation should still be disabled since the pref is disabled.
  EXPECT_FALSE(fake_cras_audio_client()->noise_cancellation_enabled());
  EXPECT_FALSE(audio_pref_handler_->GetNoiseCancellationState());
}

TEST_P(CrasAudioHandlerTest, NoiseCancellationRefreshPrefDisableWithNC) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({});
  // Set up initial audio devices, only with internal mic.
  AudioNode internalMic = GenerateAudioNode(kInternalMic);
  // Enable noise cancellation effect.
  internalMic.audio_effect = cras::EFFECT_TYPE_NOISE_CANCELLATION;
  audio_nodes.push_back(internalMic);
  // Simulate enable pref for noise cancellation.
  SetUpCrasAudioHandlerWithPrimaryActiveNodeAndNoiseCancellationState(
      audio_nodes, internalMic, /*noise_cancellation_enabled=*/false);

  // Noise cancellation should still be disabled since the pref is disabled.
  EXPECT_FALSE(fake_cras_audio_client()->noise_cancellation_enabled());
  EXPECT_FALSE(audio_pref_handler_->GetNoiseCancellationState());
}

TEST_P(CrasAudioHandlerTest, StyleTransferRefreshPrefEnabledNoStyleTransfer) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({});
  // Set up initial audio devices, only with internal mic.
  AudioNode internalMic = GenerateAudioNode(kInternalMic);
  // Clear the audio effect, no style transfer supported.
  internalMic.audio_effect = 0u;
  audio_nodes.push_back(internalMic);
  // Simulate enable pref for style transfer.
  SetUpCrasAudioHandlerWithPrimaryActiveNodeAndStyleTransferState(
      audio_nodes, internalMic, /*style_transfer_enabled=*/true);

  // Style transfer should still be disabled despite the pref being enabled
  // since the audio_effect of the internal mic is unavailable.
  EXPECT_FALSE(fake_cras_audio_client()->style_transfer_enabled());
  EXPECT_TRUE(audio_pref_handler_->GetStyleTransferState());
}

TEST_P(CrasAudioHandlerTest, StyleTransferRefreshPrefEnabledWithStyleTransfer) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({});
  // Set up initial audio devices, only with internal mic.
  AudioNode internalMic = GenerateAudioNode(kInternalMic);
  // Enable style transfer effect.
  internalMic.audio_effect = cras::EFFECT_TYPE_STYLE_TRANSFER;
  audio_nodes.push_back(internalMic);
  // Simulate enable pref for style transfer.
  SetUpCrasAudioHandlerWithPrimaryActiveNodeAndStyleTransferState(
      audio_nodes, internalMic, /*style_transfer_enabled=*/true);

  // Style transfer is enabled.
  EXPECT_TRUE(fake_cras_audio_client()->style_transfer_enabled());
  EXPECT_TRUE(audio_pref_handler_->GetStyleTransferState());
}

TEST_P(CrasAudioHandlerTest, StyleTransferRefreshPrefDisableNoStyleTransfer) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({});
  // Set up initial audio devices, only with internal mic.
  AudioNode internalMic = GenerateAudioNode(kInternalMic);
  // Clear audio effect, no style transfer.
  internalMic.audio_effect = 0u;
  audio_nodes.push_back(internalMic);
  // Simulate enable pref for style transfer.
  SetUpCrasAudioHandlerWithPrimaryActiveNodeAndStyleTransferState(
      audio_nodes, internalMic, /*style_transfer_enabled=*/false);

  // Style transfer should still be disabled since the pref is disabled.
  EXPECT_FALSE(fake_cras_audio_client()->style_transfer_enabled());
  EXPECT_FALSE(audio_pref_handler_->GetStyleTransferState());
}

TEST_P(CrasAudioHandlerTest, StyleTransferRefreshPrefDisableWithStyleTransfer) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({});
  // Set up initial audio devices, only with internal mic.
  AudioNode internalMic = GenerateAudioNode(kInternalMic);
  // Enable style transfer effect.
  internalMic.audio_effect = cras::EFFECT_TYPE_STYLE_TRANSFER;
  audio_nodes.push_back(internalMic);
  // Simulate enable pref for style transfer.
  SetUpCrasAudioHandlerWithPrimaryActiveNodeAndStyleTransferState(
      audio_nodes, internalMic, /*style_transfer_enabled=*/false);

  // Style transfer should still be disabled since the pref is disabled.
  EXPECT_FALSE(fake_cras_audio_client()->style_transfer_enabled());
  EXPECT_FALSE(audio_pref_handler_->GetStyleTransferState());
}

TEST_P(CrasAudioHandlerTest, HfpMicSrRefreshPrefEnabledNoHfpMicSr) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({});
  // Set up initial audio devices, only with bluetooth mic.
  AudioNode bt_nb_mic = GenerateAudioNode(kBluetoothNbMic);
  // Clear the audio effect, no hfp_mic_sr supported.
  bt_nb_mic.audio_effect = 0u;
  audio_nodes.push_back(bt_nb_mic);
  // Simulate enable pref for hfp_mic_sr.
  SetUpCrasAudioHandlerWithPrimaryActiveNodeAndHfpMicSrState(
      audio_nodes, bt_nb_mic, /*hfp_mic_sr_enabled=*/true);

  // hfp_mic_sr should still be disabled despite the pref being enabled
  // since the audio_effect of the hfp mic sr is unavailable.
  EXPECT_FALSE(fake_cras_audio_client()->hfp_mic_sr_enabled());
  EXPECT_TRUE(audio_pref_handler_->GetHfpMicSrState());
}

TEST_P(CrasAudioHandlerTest, HfpMicSrRefreshPrefEnabledWithHfpMicSr) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({});
  // Set up initial audio devices, only with bluetooth mic.
  AudioNode bt_nb_mic = GenerateAudioNode(kBluetoothNbMic);
  // Enable hfp_mic_sr effect.
  bt_nb_mic.audio_effect = cras::EFFECT_TYPE_HFP_MIC_SR;
  audio_nodes.push_back(bt_nb_mic);
  // Simulate enable pref for hfp_mic_sr.
  SetUpCrasAudioHandlerWithPrimaryActiveNodeAndHfpMicSrState(
      audio_nodes, bt_nb_mic, /*hfp_mic_sr_enabled=*/true);

  // hfp_mic_sr is enabled.
  EXPECT_TRUE(fake_cras_audio_client()->hfp_mic_sr_enabled());
  EXPECT_TRUE(audio_pref_handler_->GetHfpMicSrState());
}

TEST_P(CrasAudioHandlerTest, HfpMicSrRefreshPrefDisableNoHfpMicSr) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({});
  // Set up initial audio devices, only with bluetooth mic.
  AudioNode bt_nb_mic = GenerateAudioNode(kBluetoothNbMic);
  // Clear audio effect, no hfp_mic_sr.
  bt_nb_mic.audio_effect = 0u;
  audio_nodes.push_back(bt_nb_mic);
  // Simulate enable pref for hfp_mic_sr.
  SetUpCrasAudioHandlerWithPrimaryActiveNodeAndHfpMicSrState(
      audio_nodes, bt_nb_mic, /*hfp_mic_sr_enabled=*/false);

  // hfp_mic_sr should still be disabled since the pref is disabled.
  EXPECT_FALSE(fake_cras_audio_client()->hfp_mic_sr_enabled());
  EXPECT_FALSE(audio_pref_handler_->GetHfpMicSrState());
}

TEST_P(CrasAudioHandlerTest, HfpMicSrRefreshPrefDisableWithHfpMicSr) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({});
  // Set up initial audio devices, only with bluetooth mic.
  AudioNode bt_nb_mic = GenerateAudioNode(kBluetoothNbMic);
  // Enable hfp_mic_sr effect.
  bt_nb_mic.audio_effect = cras::EFFECT_TYPE_HFP_MIC_SR;
  audio_nodes.push_back(bt_nb_mic);
  // Simulate enable pref for hfp_mic_sr.
  SetUpCrasAudioHandlerWithPrimaryActiveNodeAndHfpMicSrState(
      audio_nodes, bt_nb_mic, /*hfp_mic_sr_enabled=*/false);

  // hfp_mic_sr should still be disabled since the pref is disabled.
  EXPECT_FALSE(fake_cras_audio_client()->hfp_mic_sr_enabled());
  EXPECT_FALSE(audio_pref_handler_->GetHfpMicSrState());
}

TEST_P(CrasAudioHandlerTest, BluetoothSpeakerIdChangedOnFly) {
  // Initialize with internal speaker and bluetooth headset.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kBluetoothHeadset},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kBluetoothHeadset,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/true);

  // Cras changes the bluetooth headset's id on the fly.
  AudioNodeList audio_nodes;
  AudioNode internal_speaker(GenerateAudioNode(kInternalSpeaker));
  internal_speaker.active = false;
  audio_nodes.push_back(internal_speaker);
  AudioNode bluetooth_headphone = GenerateAudioNode(kBluetoothHeadset);
  // Change bluetooth headphone id.
  bluetooth_headphone.id = kBluetoothHeadsetId + 20000;
  bluetooth_headphone.active = false;
  audio_nodes.push_back(bluetooth_headphone);
  ChangeAudioNodes(audio_nodes);

  // Verify NodesChanged event is fired, and the audio devices size is not
  // changed.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(2u, audio_devices.size());
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());

  // Verify ActiveOutputNodeChanged event is fired, and active device should be
  // bluetooth headphone.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(bluetooth_headphone.id, active_output.id);
}

TEST_P(CrasAudioHandlerTest, PlugUSBMic) {
  // Set up initial audio devices, only with internal mic.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalMic},
      /*expected_active_input_node=*/kInternalMic,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/std::nullopt);

  // Plug the USB Mic.
  AudioNodeList audio_nodes;
  AudioNode internal_mic(GenerateAudioNode(kInternalMic));
  internal_mic.active = true;
  audio_nodes.push_back(internal_mic);
  audio_nodes.push_back(GenerateAudioNode(kUSBMic1));
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and new audio device is added.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  VerifySystemMonitorWasCalled();
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(2u, audio_devices.size());

  // Verify the active input device is switched to USB mic and
  // and ActiveInputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kUSBMicId1, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());
}

TEST_P(CrasAudioHandlerTest, PlugUSBMic_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  // Set up initial audio devices, only with internal mic.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalMic},
      /*expected_active_input_node=*/kInternalMic,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/std::nullopt);

  // Expect that there is no notification when initializing the audio nodes.
  EXPECT_EQ(0u, GetNotificationCount());

  // Plug the USB Mic.
  AudioNodeList audio_nodes;
  AudioNode internal_mic(GenerateAudioNode(kInternalMic));
  internal_mic.active = true;
  audio_nodes.push_back(internal_mic);
  audio_nodes.push_back(GenerateAudioNode(kUSBMic1));
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and new audio device is added.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  VerifySystemMonitorWasCalled();
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(2u, audio_devices.size());

  // Verify the active input device is not switched to USB mic and
  // and ActiveInputChanged event is not fired.
  EXPECT_EQ(0, test_observer_->active_input_node_changed_count());
  EXPECT_NE(kUSBMicId1, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());

  // Verify notification shows up for unseen new connected USB mic device.
  FastForwardBy(AudioSelectionNotificationHandler::kDebounceTime);
  EXPECT_EQ(1u, GetNotificationCount());
}

TEST_P(CrasAudioHandlerTest, UnplugUSBMic) {
  // Set up initial audio devices, with internal mic and USB Mic.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalMic, kUSBMic1},
      /*expected_active_input_node=*/kUSBMic1,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/true,
      /*expected_has_alternative_output=*/std::nullopt);

  // Unplug the USB Mic.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(GenerateAudioNode(kInternalMic));
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired, and one audio device is
  // removed.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  VerifySystemMonitorWasCalled();
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(1u, audio_devices.size());

  // Verify the active input device is switched to internal mic, and
  // and ActiveInputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kInternalMic->id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_input());
}

TEST_P(CrasAudioHandlerTest, PlugUSBMicNotAffectActiveOutput) {
  // Set up initial audio devices.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kHeadphone, kInternalMic},
      /*expected_active_input_node=*/kInternalMic,
      /*expected_active_output_node=*/kHeadphone,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/true);

  // Switch the active output to internal speaker.
  AudioDevice internal_speaker(GenerateAudioNode(kInternalSpeaker));
  cras_audio_handler_->SwitchToDevice(internal_speaker, true,
                                      DeviceActivateType::kActivateByUser);

  // Verify the active output is switched to internal speaker, and the
  // ActiveOutputNodeChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/true);

  // Plug the USB Mic.
  AudioNodeList audio_nodes;
  AudioNode internal_speaker_node = GenerateAudioNode(kInternalSpeaker);
  internal_speaker_node.active = true;
  audio_nodes.push_back(internal_speaker_node);
  audio_nodes.push_back(GenerateAudioNode(kHeadphone));
  AudioNode internal_mic = GenerateAudioNode(kInternalMic);
  internal_mic.active = true;
  audio_nodes.push_back(internal_mic);
  audio_nodes.push_back(GenerateAudioNode(kUSBMic1));
  system_monitor_observer_.reset_count();
  ChangeAudioNodes(audio_nodes);
  LOG(INFO) << system_monitor_observer_.device_changes_received();

  // Verify the AudioNodesChanged event is fired, one new device is added.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(4u, audio_devices.size());

  // Verify the active input device is switched to USB mic, and
  // and ActiveInputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kUSBMic1->id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());

  // Verify the active output device is not changed.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/true);
}

TEST_P(CrasAudioHandlerTest,
       PlugUSBMicNotAffectActiveOutput_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  // Set up initial audio devices.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kHeadphone, kInternalMic},
      /*expected_active_input_node=*/kInternalMic,
      /*expected_active_output_node=*/kHeadphone,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/true);

  // Switch the active output to internal speaker.
  AudioDevice internal_speaker(GenerateAudioNode(kInternalSpeaker));
  cras_audio_handler_->SwitchToDevice(internal_speaker, true,
                                      DeviceActivateType::kActivateByUser);

  // Verify the active output is switched to internal speaker, and the
  // ActiveOutputNodeChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/true);

  // Expect that there is no notification when initializing the audio nodes.
  EXPECT_EQ(0u, GetNotificationCount());

  // Plug the USB Mic.
  AudioNodeList audio_nodes;
  AudioNode internal_speaker_node = GenerateAudioNode(kInternalSpeaker);
  internal_speaker_node.active = true;
  audio_nodes.push_back(internal_speaker_node);
  audio_nodes.push_back(GenerateAudioNode(kHeadphone));
  AudioNode internal_mic = GenerateAudioNode(kInternalMic);
  internal_mic.active = true;
  audio_nodes.push_back(internal_mic);
  audio_nodes.push_back(GenerateAudioNode(kUSBMic1));
  system_monitor_observer_.reset_count();
  ChangeAudioNodes(audio_nodes);
  LOG(INFO) << system_monitor_observer_.device_changes_received();

  // Verify the AudioNodesChanged event is fired, one new device is added.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(4u, audio_devices.size());

  // Verify the active input device is not switched to USB mic, and
  // and ActiveInputChanged event is not fired.
  EXPECT_EQ(0, test_observer_->active_input_node_changed_count());
  EXPECT_NE(kUSBMic1->id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());

  // Verify notification shows up for unseen new connected USB mic device.
  FastForwardBy(AudioSelectionNotificationHandler::kDebounceTime);
  EXPECT_EQ(1u, GetNotificationCount());

  // Verify the active output device is not changed.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/true);
}

TEST_P(CrasAudioHandlerTest, PlugHeadphoneAutoUnplugSpeakerWithActiveUSB) {
  // Set up initial audio devices.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kUSBHeadphone1, kInternalSpeaker, kInternalMic},
      /*expected_active_input_node=*/kInternalMic,
      /*expected_active_output_node=*/kUSBHeadphone1,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/true);

  // Plug the headphone and auto-unplug internal speaker.
  AudioNodeList audio_nodes;
  AudioNode usb_headphone_node = GenerateAudioNode(kUSBHeadphone1);
  usb_headphone_node.active = true;
  audio_nodes.push_back(usb_headphone_node);
  AudioNode headphone_node = GenerateAudioNode(kHeadphone);
  headphone_node.plugged_time = 1000;
  audio_nodes.push_back(headphone_node);
  AudioNode internal_mic = GenerateAudioNode(kInternalMic);
  internal_mic.active = true;
  audio_nodes.push_back(internal_mic);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired, with nodes count unchanged.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(3u, audio_devices.size());

  // Verify the active output device is switched to headphone, and
  // an ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false, /*expected_active_device=*/kHeadphone,
                     /*has_alternative_device=*/true);

  // Unplug the headphone and internal speaker auto-plugs back.
  audio_nodes.clear();
  audio_nodes.push_back(GenerateAudioNode(kUSBHeadphone1));
  AudioNode internal_speaker_node = GenerateAudioNode(kInternalSpeaker);
  internal_speaker_node.plugged_time = 2000;
  audio_nodes.push_back(internal_speaker_node);
  audio_nodes.push_back(internal_mic);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired, with nodes count unchanged.
  EXPECT_EQ(2, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(3u, audio_devices.size());

  // Verify the active output device is switched back to USB, and
  // an ActiveOutputChanged event is fired.
  EXPECT_EQ(2, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kUSBHeadphone1,
                     /*has_alternative_device=*/true);

  // Verify the active input device is not changed.
  EXPECT_EQ(0, test_observer_->active_input_node_changed_count());
  ExpectActiveDevice(/*is_input=*/true, /*expected_active_device=*/kInternalMic,
                     /*has_alternative_device=*/false);
}

TEST_P(CrasAudioHandlerTest, PlugMicAutoUnplugInternalMicWithActiveUSB) {
  // Set up initial audio devices.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kUSBHeadphone1, kInternalSpeaker, kUSBMic1,
                         kInternalMic},
      /*expected_active_input_node=*/kUSBMic1,
      /*expected_active_output_node=*/kUSBHeadphone1,
      /*expected_has_alternative_input=*/true,
      /*expected_has_alternative_output=*/true);

  // Plug the headphone and mic, auto-unplug internal mic and speaker.
  AudioNodeList audio_nodes;
  AudioNode usb_headphone_node = GenerateAudioNode(kUSBHeadphone1);
  usb_headphone_node.active = true;
  audio_nodes.push_back(usb_headphone_node);
  AudioNode headphone_node = GenerateAudioNode(kHeadphone);
  headphone_node.plugged_time = 1000;
  audio_nodes.push_back(headphone_node);
  AudioNode usb_mic = GenerateAudioNode(kUSBMic1);
  usb_mic.active = true;
  audio_nodes.push_back(usb_mic);
  AudioNode mic_jack = GenerateAudioNode(kMicJack);
  mic_jack.plugged_time = 1000;
  audio_nodes.push_back(mic_jack);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired, with nodes count unchanged.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(4u, audio_devices.size());

  // Verify the active output device is switched to headphone, and
  // an ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false, /*expected_active_device=*/kHeadphone,
                     /*has_alternative_device=*/true);

  // Verify the active input device is switched to mic jack, and
  // an ActiveInputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kMicJack->id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());

  // Unplug the headphone and internal speaker auto-plugs back.
  audio_nodes.clear();
  audio_nodes.push_back(GenerateAudioNode(kUSBHeadphone1));
  AudioNode internal_speaker_node = GenerateAudioNode(kInternalSpeaker);
  internal_speaker_node.plugged_time = 2000;
  audio_nodes.push_back(internal_speaker_node);
  audio_nodes.push_back(GenerateAudioNode(kUSBMic1));
  AudioNode internal_mic = GenerateAudioNode(kInternalMic);
  internal_mic.plugged_time = 2000;
  audio_nodes.push_back(internal_mic);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired, with nodes count unchanged.
  EXPECT_EQ(2, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(4u, audio_devices.size());

  // Verify the active output device is switched back to USB, and
  // an ActiveOutputChanged event is fired.
  EXPECT_EQ(2, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kUSBHeadphone1,
                     /*has_alternative_device=*/true);

  // Verify the active input device is switched back to USB mic, and
  // an ActiveInputChanged event is fired.
  EXPECT_EQ(2, test_observer_->active_input_node_changed_count());
  ExpectActiveDevice(/*is_input=*/true, /*expected_active_device=*/kUSBMic1,
                     /*has_alternative_device=*/true);
}

TEST_P(CrasAudioHandlerTest, MultipleNodesChangedSignalsOnPlugInHeadphone) {
  // Set up initial audio devices.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kBluetoothHeadset},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kBluetoothHeadset,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/true);

  // Plug in headphone, but fire NodesChanged signal twice.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(GenerateAudioNode(kInternalSpeaker));
  AudioNode bluetooth_headset = GenerateAudioNode(kBluetoothHeadset);
  bluetooth_headset.plugged_time = 1000;
  bluetooth_headset.active = true;
  audio_nodes.push_back(bluetooth_headset);
  AudioNode headphone = GenerateAudioNode(kHeadphone);
  headphone.active = false;
  headphone.plugged_time = 2000;
  audio_nodes.push_back(headphone);
  ChangeAudioNodes(audio_nodes);
  ChangeAudioNodes(audio_nodes);

  // Verify the active output device is set to headphone.
  EXPECT_EQ(2, test_observer_->audio_nodes_changed_count());
  EXPECT_LE(1, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false, /*expected_active_device=*/kHeadphone,
                     /*has_alternative_device=*/true);

  // Verify the audio devices data is consistent, i.e., the active output device
  // should be headphone.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(3u, audio_devices.size());
  for (size_t i = 0; i < audio_devices.size(); ++i) {
    if (audio_devices[i].id == kInternalSpeaker->id) {
      EXPECT_FALSE(audio_devices[i].active);
    } else if (audio_devices[i].id == bluetooth_headset.id) {
      EXPECT_FALSE(audio_devices[i].active);
    } else if (audio_devices[i].id == headphone.id) {
      EXPECT_TRUE(audio_devices[i].active);
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }
}

TEST_P(CrasAudioHandlerTest, MultipleNodesChangedSignalsOnPlugInUSBMic) {
  // Set up initial audio devices.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalMic},
      /*expected_active_input_node=*/kInternalMic,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/std::nullopt);

  // Plug in usb mic, but fire NodesChanged signal twice.
  AudioNodeList audio_nodes;
  AudioNode internal_mic = GenerateAudioNode(kInternalMic);
  internal_mic.active = true;
  internal_mic.plugged_time = 1000;
  audio_nodes.push_back(internal_mic);
  AudioNode usb_mic = GenerateAudioNode(kUSBMic1);
  usb_mic.active = false;
  usb_mic.plugged_time = 2000;
  audio_nodes.push_back(usb_mic);
  ChangeAudioNodes(audio_nodes);
  ChangeAudioNodes(audio_nodes);

  // Verify the active output device is set to headphone.
  EXPECT_EQ(2, test_observer_->audio_nodes_changed_count());
  EXPECT_LE(1, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(usb_mic.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());

  // Verify the audio devices data is consistent, i.e., the active input device
  // should be usb mic.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(2u, audio_devices.size());
  for (size_t i = 0; i < audio_devices.size(); ++i) {
    if (audio_devices[i].id == kInternalMic->id) {
      EXPECT_FALSE(audio_devices[i].active);
    } else if (audio_devices[i].id == usb_mic.id) {
      EXPECT_TRUE(audio_devices[i].active);
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }
}

TEST_P(
    CrasAudioHandlerTest,
    MultipleNodesChangedSignalsOnPlugInUSBMic_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  // Set up initial audio devices.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalMic},
      /*expected_active_input_node=*/kInternalMic,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/std::nullopt);

  // Plug in usb mic, but fire NodesChanged signal twice.
  AudioNodeList audio_nodes;
  AudioNode internal_mic = GenerateAudioNode(kInternalMic);
  internal_mic.active = true;
  internal_mic.plugged_time = 1000;
  audio_nodes.push_back(internal_mic);
  AudioNode usb_mic = GenerateAudioNode(kUSBMic1);
  usb_mic.active = false;
  usb_mic.plugged_time = 2000;
  audio_nodes.push_back(usb_mic);
  ChangeAudioNodes(audio_nodes);
  ChangeAudioNodes(audio_nodes);

  // Verify the active output device is not set to usb mic.
  EXPECT_EQ(2, test_observer_->audio_nodes_changed_count());
  EXPECT_EQ(0, test_observer_->active_input_node_changed_count());
  EXPECT_NE(usb_mic.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());

  // Verify the audio devices data is consistent, i.e., the active input device
  // should be internal mic.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(2u, audio_devices.size());
  for (size_t i = 0; i < audio_devices.size(); ++i) {
    if (audio_devices[i].id == kInternalMic->id) {
      EXPECT_TRUE(audio_devices[i].active);
    } else if (audio_devices[i].id == usb_mic.id) {
      EXPECT_FALSE(audio_devices[i].active);
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }
}

// This is the case of crbug.com/291303.
TEST_P(CrasAudioHandlerTest, MultipleNodesChangedSignalsOnSystemBoot) {
  // Set up audio handler with empty audio_nodes.
  AudioNodeList audio_nodes;
  SetUpCrasAudioHandler(audio_nodes);

  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  internal_speaker.active = false;
  AudioNode headphone = GenerateAudioNode(kHeadphone);
  headphone.active = false;
  AudioNode internal_mic = GenerateAudioNode(kInternalMic);
  internal_mic.active = false;
  audio_nodes.push_back(internal_speaker);
  audio_nodes.push_back(headphone);
  audio_nodes.push_back(internal_mic);
  const size_t init_nodes_size = audio_nodes.size();

  // Simulate AudioNodesChanged signal being fired twice during system boot.
  ChangeAudioNodes(audio_nodes);
  ChangeAudioNodes(audio_nodes);

  // Verify the active output device is set to headphone.
  EXPECT_EQ(2, test_observer_->audio_nodes_changed_count());
  EXPECT_LE(1, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false, /*expected_active_device=*/kHeadphone,
                     /*has_alternative_device=*/true);

  // Verify the active input device id is set to internal mic.
  EXPECT_EQ(internal_mic.id, cras_audio_handler_->GetPrimaryActiveInputNode());

  // Verify the audio devices data is consistent, i.e., the active output device
  // should be headphone, and the active input device should internal mic.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  for (size_t i = 0; i < audio_devices.size(); ++i) {
    if (audio_devices[i].id == internal_speaker.id) {
      EXPECT_FALSE(audio_devices[i].active);
    } else if (audio_devices[i].id == headphone.id) {
      EXPECT_TRUE(audio_devices[i].active);
    } else if (audio_devices[i].id == internal_mic.id) {
      EXPECT_TRUE(audio_devices[i].active);
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }
}

// This is the case of crbug.com/448924.
TEST_P(CrasAudioHandlerTest,
       TwoNodesChangedSignalsForLosingTowNodesOnOneUnplug) {
  // Set up audio handler with 4 audio_nodes.
  AudioNodeList audio_nodes;
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  internal_speaker.active = false;
  AudioNode headphone = GenerateAudioNode(kHeadphone);
  headphone.active = false;
  AudioNode internal_mic = GenerateAudioNode(kInternalMic);
  internal_mic.active = false;
  AudioNode micJack = GenerateAudioNode(kMicJack);
  micJack.active = false;
  audio_nodes.push_back(internal_speaker);
  audio_nodes.push_back(headphone);
  audio_nodes.push_back(internal_mic);
  audio_nodes.push_back(micJack);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the headphone has been selected as the active output.
  ExpectActiveDevice(/*is_input=*/false, /*expected_active_device=*/kHeadphone,
                     /*has_alternative_device=*/true);

  // Verify the mic Jack has been selected as the active input.
  EXPECT_EQ(micJack.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  const AudioDevice* active_input = GetDeviceFromId(micJack.id);
  EXPECT_TRUE(active_input->active);
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());

  // Simulate the nodes list in first NodesChanged signal, only headphone is
  // removed, other nodes remains the same.
  AudioNodeList changed_nodes_1;
  internal_speaker.active = false;
  changed_nodes_1.push_back(internal_speaker);
  internal_mic.active = false;
  changed_nodes_1.push_back(internal_mic);
  micJack.active = true;
  changed_nodes_1.push_back(micJack);

  // Simulate the nodes list in second NodesChanged signal, the micJac is
  // removed, but the internal_mic is inactive, which does not reflect the
  // active status set from the first NodesChanged signal since this was sent
  // before cras receives the SetActiveOutputNode from the first NodesChanged
  // handling.
  AudioNodeList changed_nodes_2;
  changed_nodes_2.push_back(internal_speaker);
  changed_nodes_2.push_back(internal_mic);

  // Simulate AudioNodesChanged signal being fired twice for unplug an audio
  // device with both input and output nodes on it.
  ChangeAudioNodes(changed_nodes_1);
  ChangeAudioNodes(changed_nodes_2);

  AudioDeviceList changed_devices;
  cras_audio_handler_->GetAudioDevices(&changed_devices);
  EXPECT_EQ(2u, changed_devices.size());

  // Verify the active output device is set to internal speaker.
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/false);

  // Verify the active input device id is set to internal mic.
  EXPECT_EQ(internal_mic.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  const AudioDevice* changed_active_input = GetDeviceFromId(internal_mic.id);
  EXPECT_TRUE(changed_active_input->active);
}

TEST_P(CrasAudioHandlerTest, SetOutputMonoEnabled) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kHeadphone});
  SetUpCrasAudioHandler(audio_nodes);
  EXPECT_EQ(0, test_observer_->output_channel_remixing_changed_count());

  // Set output mono
  cras_audio_handler_->SetOutputMonoEnabled(true);

  // Verify the output is in mono mode, OnOuputChannelRemixingChanged event
  // is fired.
  EXPECT_TRUE(output_mono_enabled());
  EXPECT_EQ(1, test_observer_->output_channel_remixing_changed_count());

  // Set output stereo
  cras_audio_handler_->SetOutputMonoEnabled(false);
  EXPECT_FALSE(output_mono_enabled());
  EXPECT_EQ(2, test_observer_->output_channel_remixing_changed_count());
}

TEST_P(CrasAudioHandlerTest, SetOutputMute) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalSpeaker});
  SetUpCrasAudioHandler(audio_nodes);
  EXPECT_EQ(0, test_observer_->output_mute_changed_count());

  // Mute the device.
  cras_audio_handler_->SetOutputMute(true);

  // Verify the output is muted, OnOutputMuteChanged event is fired,
  // and mute value is saved in the preferences.
  EXPECT_TRUE(cras_audio_handler_->IsOutputMuted());
  EXPECT_EQ(1, test_observer_->output_mute_changed_count());
  AudioDevice speaker(GenerateAudioNode(kInternalSpeaker));
  EXPECT_TRUE(audio_pref_handler_->GetMuteValue(speaker));

  // Unmute the device.
  cras_audio_handler_->SetOutputMute(false);

  // Verify the output is unmuted, OnOutputMuteChanged event is fired,
  // and mute value is saved in the preferences.
  EXPECT_FALSE(cras_audio_handler_->IsOutputMuted());
  EXPECT_EQ(2, test_observer_->output_mute_changed_count());
  EXPECT_FALSE(audio_pref_handler_->GetMuteValue(speaker));
}

TEST_P(CrasAudioHandlerTest, SetOutputMuteWithSource) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalSpeaker});
  SetUpCrasAudioHandler(audio_nodes);
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kOutputVolumeMuteSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kSystemTray,
      /*expected_count=*/0);

  // Mute the device.
  cras_audio_handler_->SetOutputMute(
      true, CrasAudioHandler::AudioSettingsChangeSource::kSystemTray);

  // Verify mute source is recorded.
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kOutputVolumeMuteSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kSystemTray,
      /*expected_count=*/1);
}

TEST_P(CrasAudioHandlerTest, SetInputMute) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalMic});
  SetUpCrasAudioHandler(audio_nodes);
  EXPECT_EQ(0, test_observer_->input_mute_changed_count());

  // Mute the device.
  cras_audio_handler_->SetInputMute(
      true, CrasAudioHandler::InputMuteChangeMethod::kOther);

  // Verify the input is muted, OnInputMuteChanged event is fired.
  EXPECT_TRUE(cras_audio_handler_->IsInputMuted());
  EXPECT_EQ(1, test_observer_->input_mute_changed_count());

  // Unmute the device.
  cras_audio_handler_->SetInputMute(
      false, CrasAudioHandler::InputMuteChangeMethod::kOther);

  // Verify the input is unmuted, OnInputMuteChanged event is fired.
  EXPECT_FALSE(cras_audio_handler_->IsInputMuted());
  EXPECT_EQ(2, test_observer_->input_mute_changed_count());
}

TEST_P(CrasAudioHandlerTest, SetInputMuteWithSource) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalMic});
  SetUpCrasAudioHandler(audio_nodes);
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kInputGainMuteSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kSystemTray,
      /*expected_count=*/0);

  // Mute the device.
  cras_audio_handler_->SetInputMute(
      true, CrasAudioHandler::InputMuteChangeMethod::kOther,
      CrasAudioHandler::AudioSettingsChangeSource::kSystemTray);

  // Verify mute source is recorded.
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kInputGainMuteSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kSystemTray,
      /*expected_count=*/1);
}

TEST_P(CrasAudioHandlerTest, SetOutputVolumePercent) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalSpeaker});
  SetUpCrasAudioHandler(audio_nodes);
  EXPECT_EQ(0, test_observer_->output_volume_changed_count());

  const int kVolume = 60;
  cras_audio_handler_->SetOutputVolumePercent(kVolume);

  // Verify the output volume is changed to the designated value,
  // OnOutputNodeVolumeChanged event is fired, and the device volume value
  // is saved in the preferences.
  EXPECT_EQ(kVolume, cras_audio_handler_->GetOutputVolumePercent());
  EXPECT_EQ(1, test_observer_->output_volume_changed_count());
  AudioDevice device;
  EXPECT_TRUE(cras_audio_handler_->GetPrimaryActiveOutputDevice(&device));
  EXPECT_EQ(device.id, kInternalSpeaker->id);
  EXPECT_EQ(kVolume, audio_pref_handler_->GetOutputVolumeValue(&device));
}

TEST_P(CrasAudioHandlerTest, IncreaseOutputVolumeByOneStepOther) {
  AudioNodeList audio_nodes = GenerateAudioNodeList(
      {kInternalSpeaker, kHeadphone, kOther, kBluetoothHeadset, kHDMIOutput});
  SetUpCrasAudioHandler(audio_nodes);
  for (const auto& audio_node : audio_nodes) {
    cras_audio_handler_->ChangeActiveNodes({audio_node.id});
    cras_audio_handler_->SetOutputVolumePercent(50);
    cras_audio_handler_->IncreaseOutputVolumeByOneStep(kStepPercentage);
    EXPECT_EQ(50 + kStepPercentage,
              cras_audio_handler_->GetOutputVolumePercent());
  }
}

TEST_P(CrasAudioHandlerTest, IncreaseOutputVolumeByOneStepUSB) {
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kUSBHeadphone1, kUSBHeadphone2});
  AudioNode invalid_steps_auido_node = GenerateAudioNode(kUSBHeadphone3);
  invalid_steps_auido_node.number_of_volume_steps = 0;
  audio_nodes.push_back(invalid_steps_auido_node);
  SetUpCrasAudioHandler(audio_nodes);
  // USB 1 have 25 steps, mean we increase 100/25=4 % of volume per step.
  // USB 1 start from volume 0 and increase one step expect increase to 4.
  cras_audio_handler_->ChangeActiveNodes({kUSBHeadphone1->id});
  cras_audio_handler_->SetOutputVolumePercent(0);
  cras_audio_handler_->IncreaseOutputVolumeByOneStep(kStepPercentage);
  EXPECT_EQ(4, cras_audio_handler_->GetOutputVolumePercent());
  // 0   -> step0
  // 1-4 -> step1
  // 5-8 -> step2
  // Inorder to let user feel volume change, increase step1 to step2.
  // USB 1 start from volume 2 and increase one step, expect increase to 8.
  cras_audio_handler_->SetOutputVolumePercent(2);
  cras_audio_handler_->IncreaseOutputVolumeByOneStep(kStepPercentage);
  EXPECT_EQ(8, cras_audio_handler_->GetOutputVolumePercent());
  // 100 is max volume
  cras_audio_handler_->SetOutputVolumePercent(100);
  cras_audio_handler_->IncreaseOutputVolumeByOneStep(kStepPercentage);
  EXPECT_EQ(100, cras_audio_handler_->GetOutputVolumePercent());
  // can increase from 0 to 100
  cras_audio_handler_->SetOutputVolumePercent(0);
  for (int32_t i = 0; i < kUSBHeadphone1->number_of_volume_steps; ++i) {
    cras_audio_handler_->IncreaseOutputVolumeByOneStep(kStepPercentage);
  }
  EXPECT_EQ(100, cras_audio_handler_->GetOutputVolumePercent());
  // USB 2 have 16 steps, mean we increase 100/16=6.25 % of volume per step.
  // USB 2 start from volume 0 and increase one step expect increase to 6;
  cras_audio_handler_->ChangeActiveNodes({kUSBHeadphone2->id});
  cras_audio_handler_->SetOutputVolumePercent(0);
  cras_audio_handler_->IncreaseOutputVolumeByOneStep(kStepPercentage);
  EXPECT_EQ(6, cras_audio_handler_->GetOutputVolumePercent());
  // 0    -> step0
  // 1-6  -> step1
  // 7-12 -> step2
  // Inorder to let user feel volume change, increase step1 to step2.
  // USB 2 start from volume 4 and increase one step, expect increase to 12
  cras_audio_handler_->SetOutputVolumePercent(4);
  cras_audio_handler_->IncreaseOutputVolumeByOneStep(kStepPercentage);
  EXPECT_EQ(12, cras_audio_handler_->GetOutputVolumePercent());
  // USB 2 start from volume 0 and increase 4 step, expect increase to
  // 25(6.25*4=25)
  cras_audio_handler_->SetOutputVolumePercent(0);
  for (int32_t i = 0; i < 4; ++i) {
    cras_audio_handler_->IncreaseOutputVolumeByOneStep(kStepPercentage);
  }
  EXPECT_EQ(25, cras_audio_handler_->GetOutputVolumePercent());
  // 100 is max
  cras_audio_handler_->SetOutputVolumePercent(100);
  cras_audio_handler_->IncreaseOutputVolumeByOneStep(kStepPercentage);
  EXPECT_EQ(100, cras_audio_handler_->GetOutputVolumePercent());
  // can increase from 0 to 100
  cras_audio_handler_->SetOutputVolumePercent(0);
  for (uint32_t i = 0; i < kUSBHeadphone2->number_of_volume_steps; ++i) {
    cras_audio_handler_->IncreaseOutputVolumeByOneStep(kStepPercentage);
  }
  EXPECT_EQ(100, cras_audio_handler_->GetOutputVolumePercent());
  // USB 3 have 0 steps, this is invalid case, so we will fallback to use 25
  // steps. USB 3 start from volume 0 and increase one step expect increase
  // to 4.
  cras_audio_handler_->ChangeActiveNodes({kUSBHeadphone3->id});
  cras_audio_handler_->SetOutputVolumePercent(0);
  cras_audio_handler_->IncreaseOutputVolumeByOneStep(kStepPercentage);
  EXPECT_EQ(4, cras_audio_handler_->GetOutputVolumePercent());
  // 0   -> step0
  // 1-4 -> step1
  // 5-8 -> step2
  // Inorder to let user feel volume change, increase step1 to step2.
  // USB 1 start from volume 2 and increase one step, expect increase to 8.
  cras_audio_handler_->SetOutputVolumePercent(2);
  cras_audio_handler_->IncreaseOutputVolumeByOneStep(kStepPercentage);
  EXPECT_EQ(8, cras_audio_handler_->GetOutputVolumePercent());
  // 100 is max volume
  cras_audio_handler_->SetOutputVolumePercent(100);
  cras_audio_handler_->IncreaseOutputVolumeByOneStep(kStepPercentage);
  EXPECT_EQ(100, cras_audio_handler_->GetOutputVolumePercent());
  // can increase from 0 to 100
  cras_audio_handler_->SetOutputVolumePercent(0);
  for (int32_t i = 0; i < NUMBER_OF_VOLUME_STEPS_DEFAULT; ++i) {
    cras_audio_handler_->IncreaseOutputVolumeByOneStep(kStepPercentage);
  }
  EXPECT_EQ(100, cras_audio_handler_->GetOutputVolumePercent());
}

TEST_P(CrasAudioHandlerTest, DecreaseOutputVolumeByOneStepOther) {
  AudioNodeList audio_nodes = GenerateAudioNodeList(
      {kInternalSpeaker, kHeadphone, kOther, kBluetoothHeadset, kHDMIOutput});
  SetUpCrasAudioHandler(audio_nodes);
  for (const auto& audio_node : audio_nodes) {
    cras_audio_handler_->ChangeActiveNodes({audio_node.id});
    cras_audio_handler_->SetOutputVolumePercent(50);
    cras_audio_handler_->DecreaseOutputVolumeByOneStep(kStepPercentage);
    EXPECT_EQ(50 - kStepPercentage,
              cras_audio_handler_->GetOutputVolumePercent());
  }
}

TEST_P(CrasAudioHandlerTest, DecreaseOutputVolumeByOneStepUSB) {
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kUSBHeadphone1, kUSBHeadphone2});
  AudioNode invalid_steps_auido_node = GenerateAudioNode(kUSBHeadphone3);
  invalid_steps_auido_node.number_of_volume_steps = 0;
  audio_nodes.push_back(invalid_steps_auido_node);
  SetUpCrasAudioHandler(audio_nodes);
  // USB 1 have 25 steps, mean we decrease 100/25=4 % of volume per step.
  // USB 1 start from volume 4 and decrease one step expect decrease to 0;
  cras_audio_handler_->ChangeActiveNodes({kUSBHeadphone1->id});
  cras_audio_handler_->SetOutputVolumePercent(4);
  cras_audio_handler_->DecreaseOutputVolumeByOneStep(kStepPercentage);
  EXPECT_EQ(0, cras_audio_handler_->GetOutputVolumePercent());
  // 0   -> step0
  // 1-4 -> step1
  // 4-8 -> step2
  // Inorder to let user feel volume change, decrease step2 to step1.
  // USB 1 start from volume 6 and decrease one step, expect decrease to 4
  cras_audio_handler_->SetOutputVolumePercent(6);
  cras_audio_handler_->DecreaseOutputVolumeByOneStep(kStepPercentage);
  EXPECT_EQ(4, cras_audio_handler_->GetOutputVolumePercent());
  // 0 is min volume
  cras_audio_handler_->SetOutputVolumePercent(0);
  cras_audio_handler_->DecreaseOutputVolumeByOneStep(kStepPercentage);
  EXPECT_EQ(0, cras_audio_handler_->GetOutputVolumePercent());
  // can decrease from 100 to 0
  cras_audio_handler_->SetOutputVolumePercent(100);
  for (int32_t i = 0; i < kUSBHeadphone1->number_of_volume_steps; ++i) {
    cras_audio_handler_->DecreaseOutputVolumeByOneStep(kStepPercentage);
  }
  EXPECT_EQ(0, cras_audio_handler_->GetOutputVolumePercent());
  // USB 2 have 16 steps, mean we decrease 100/16=6.25 % of volume per step.
  // USB 2 start from volume 12 and decrease one step expect decrease to 6;
  cras_audio_handler_->ChangeActiveNodes({kUSBHeadphone2->id});
  cras_audio_handler_->SetOutputVolumePercent(12);
  cras_audio_handler_->DecreaseOutputVolumeByOneStep(kStepPercentage);
  EXPECT_EQ(6, cras_audio_handler_->GetOutputVolumePercent());
  // 0    -> step0
  // 1-6  -> step1
  // 7-12 -> step2
  // Inorder to let user feel volume change, decrease step2 to step1.
  // USB 2 start from volume 10 and decrease one step, expect decrease to 6
  cras_audio_handler_->SetOutputVolumePercent(10);
  cras_audio_handler_->DecreaseOutputVolumeByOneStep(kStepPercentage);
  EXPECT_EQ(6, cras_audio_handler_->GetOutputVolumePercent());
  // USB 2 start from volume 25 and decrease 4 step, expect decrease to 0
  cras_audio_handler_->SetOutputVolumePercent(25);
  for (int32_t i = 0; i < 4; ++i) {
    cras_audio_handler_->DecreaseOutputVolumeByOneStep(kStepPercentage);
  }
  EXPECT_EQ(0, cras_audio_handler_->GetOutputVolumePercent());
  // 0 is min volume
  cras_audio_handler_->SetOutputVolumePercent(0);
  cras_audio_handler_->DecreaseOutputVolumeByOneStep(kStepPercentage);
  EXPECT_EQ(0, cras_audio_handler_->GetOutputVolumePercent());
  // can decrease from 100 to 0
  cras_audio_handler_->SetOutputVolumePercent(100);
  for (int32_t i = 0; i < kUSBHeadphone2->number_of_volume_steps; ++i) {
    cras_audio_handler_->DecreaseOutputVolumeByOneStep(kStepPercentage);
  }
  EXPECT_EQ(0, cras_audio_handler_->GetOutputVolumePercent());
  // USB 3 have 0 steps, this is invalid case, so we will fallback to use 25
  // steps. USB 3 start from volume 4 and decrease one step expect decrease to
  // 0;
  cras_audio_handler_->ChangeActiveNodes({kUSBHeadphone3->id});
  cras_audio_handler_->SetOutputVolumePercent(4);
  cras_audio_handler_->DecreaseOutputVolumeByOneStep(kStepPercentage);
  EXPECT_EQ(0, cras_audio_handler_->GetOutputVolumePercent());
  // 0   -> step0
  // 1-4 -> step1
  // 4-8 -> step2
  // Inorder to let user feel volume change, decrease step2 to step1.
  // USB 3 start from volume 6 and decrease one step, expect decrease to 4
  cras_audio_handler_->SetOutputVolumePercent(6);
  cras_audio_handler_->DecreaseOutputVolumeByOneStep(kStepPercentage);
  EXPECT_EQ(4, cras_audio_handler_->GetOutputVolumePercent());
  // 0 is min volume
  cras_audio_handler_->SetOutputVolumePercent(0);
  cras_audio_handler_->DecreaseOutputVolumeByOneStep(kStepPercentage);
  EXPECT_EQ(0, cras_audio_handler_->GetOutputVolumePercent());
  // can decrease from 100 to 0
  cras_audio_handler_->SetOutputVolumePercent(100);
  for (int32_t i = 0; i < NUMBER_OF_VOLUME_STEPS_DEFAULT; ++i) {
    cras_audio_handler_->DecreaseOutputVolumeByOneStep(kStepPercentage);
  }
  EXPECT_EQ(0, cras_audio_handler_->GetOutputVolumePercent());
}

TEST_P(CrasAudioHandlerTest, AdjustOutputVolumeToAudibleLevelUSB) {
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kUSBHeadphone1, kUSBHeadphone2});
  SetUpCrasAudioHandler(audio_nodes);
  cras_audio_handler_->ChangeActiveNodes({kUSBHeadphone1->id});
  cras_audio_handler_->SetOutputVolumePercent(0);
  cras_audio_handler_->AdjustOutputVolumeToAudibleLevel();
  EXPECT_EQ(4, cras_audio_handler_->GetOutputVolumePercent());

  cras_audio_handler_->ChangeActiveNodes({kUSBHeadphone2->id});
  cras_audio_handler_->SetOutputVolumePercent(0);
  cras_audio_handler_->AdjustOutputVolumeToAudibleLevel();
  EXPECT_EQ(6, cras_audio_handler_->GetOutputVolumePercent());
  // If volume is not under audible level, don't change the volume.
  cras_audio_handler_->ChangeActiveNodes({kUSBHeadphone1->id});
  cras_audio_handler_->SetOutputVolumePercent(50);
  cras_audio_handler_->AdjustOutputVolumeToAudibleLevel();
  EXPECT_EQ(50, cras_audio_handler_->GetOutputVolumePercent());

  cras_audio_handler_->ChangeActiveNodes({kUSBHeadphone2->id});
  cras_audio_handler_->SetOutputVolumePercent(50);
  cras_audio_handler_->AdjustOutputVolumeToAudibleLevel();
  EXPECT_EQ(50, cras_audio_handler_->GetOutputVolumePercent());
}

TEST_P(CrasAudioHandlerTest, AdjustOutputVolumeToAudibleLevelOther) {
  AudioNodeList audio_nodes = GenerateAudioNodeList(
      {kInternalSpeaker, kHeadphone, kOther, kBluetoothHeadset, kHDMIOutput});
  SetUpCrasAudioHandler(audio_nodes);
  for (const auto& audio_node : audio_nodes) {
    cras_audio_handler_->ChangeActiveNodes({audio_node.id});
    cras_audio_handler_->SetOutputVolumePercent(0);
    cras_audio_handler_->AdjustOutputVolumeToAudibleLevel();
    EXPECT_EQ(kDefaultUnmuteVolumePercent,
              cras_audio_handler_->GetOutputVolumePercent());
  }
  // If volume is not under audible level, don't change the volume.
  for (const auto& audio_node : audio_nodes) {
    cras_audio_handler_->ChangeActiveNodes({audio_node.id});
    cras_audio_handler_->SetOutputVolumePercent(50);
    cras_audio_handler_->AdjustOutputVolumeToAudibleLevel();
    EXPECT_EQ(50, cras_audio_handler_->GetOutputVolumePercent());
  }
}

TEST_P(CrasAudioHandlerTest, RejectInvalidForOutputNodeVolumeChanged) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalSpeaker});
  SetUpCrasAudioHandler(audio_nodes);
  EXPECT_EQ(0, test_observer_->output_volume_changed_count());
  const std::vector<int> invalid_volumes = {-1, 101};

  for (const int kVolume : invalid_volumes) {
    fake_cras_audio_client()->NotifyOutputNodeVolumeChangedForTesting(
        kInternalSpeaker->id, kVolume);
    EXPECT_EQ(0, test_observer_->output_volume_changed_count());
  }
}

TEST_P(CrasAudioHandlerTest, RestartAudioClientWithCrasReady) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalSpeaker});
  SetUpCrasAudioHandler(audio_nodes);
  EXPECT_EQ(0, test_observer_->output_volume_changed_count());

  const int kDefaultVolume = cras_audio_handler_->GetOutputVolumePercent();
  // Disable the auto OutputNodeVolumeChanged signal.
  fake_cras_audio_client()->disable_volume_change_events();

  fake_cras_audio_client()->SetAudioNodesForTesting(audio_nodes);
  RestartAudioClient();
  EXPECT_EQ(0, test_observer_->output_volume_changed_count());
  EXPECT_EQ(kDefaultVolume, cras_audio_handler_->GetOutputVolumePercent());

  // The correct initialization OutputNodeVolumeChanged event is fired. We
  // should avoid notifying observers.
  fake_cras_audio_client()->NotifyOutputNodeVolumeChangedForTesting(
      kInternalSpeaker->id, kDefaultVolume);
  EXPECT_EQ(0, test_observer_->output_volume_changed_count());
  EXPECT_EQ(kDefaultVolume, cras_audio_handler_->GetOutputVolumePercent());

  // The later OutputNodeVolumeChanged event after initialization should notify
  // observers.
  const int kVolume = 60;
  fake_cras_audio_client()->NotifyOutputNodeVolumeChangedForTesting(
      kInternalSpeaker->id, kVolume);
  EXPECT_EQ(1, test_observer_->output_volume_changed_count());
  EXPECT_EQ(kVolume, cras_audio_handler_->GetOutputVolumePercent());
}

TEST_P(CrasAudioHandlerTest, RestartAudioClientWithCrasDropRequest) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalSpeaker});
  SetUpCrasAudioHandler(audio_nodes);
  EXPECT_EQ(0, test_observer_->output_volume_changed_count());

  const int kDefaultVolume = cras_audio_handler_->GetOutputVolumePercent();
  // Disable the auto OutputNodeVolumeChanged signal.
  fake_cras_audio_client()->disable_volume_change_events();

  fake_cras_audio_client()->SetAudioNodesForTesting(audio_nodes);
  RestartAudioClient();
  EXPECT_EQ(0, test_observer_->output_volume_changed_count());
  EXPECT_EQ(kDefaultVolume, cras_audio_handler_->GetOutputVolumePercent());

  // A wrong initialization OutputNodeVolumeChanged event is fired. This may
  // happen when Cras is not ready and drops request. The approach we use is
  // to log warning message, clear the pending automated volume change reasons,
  // and notify observers about this change.
  const int kVolume1 = 30;
  fake_cras_audio_client()->NotifyOutputNodeVolumeChangedForTesting(
      kInternalSpeaker->id, kVolume1);
  EXPECT_EQ(1, test_observer_->output_volume_changed_count());
  EXPECT_EQ(kVolume1, cras_audio_handler_->GetOutputVolumePercent());

  // The later OutputNodeVolumeChanged event should notify observers.
  const int kVolume2 = 60;
  fake_cras_audio_client()->NotifyOutputNodeVolumeChangedForTesting(
      kInternalSpeaker->id, kVolume2);
  EXPECT_EQ(2, test_observer_->output_volume_changed_count());
  EXPECT_EQ(kVolume2, cras_audio_handler_->GetOutputVolumePercent());
}

TEST_P(CrasAudioHandlerTest, SetOutputVolumeWithDelayedSignal) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalSpeaker});
  SetUpCrasAudioHandler(audio_nodes);
  EXPECT_EQ(0, test_observer_->output_volume_changed_count());

  const int kDefaultVolume = 75;
  EXPECT_EQ(kDefaultVolume, cras_audio_handler_->GetOutputVolumePercent());

  // Disable the auto OutputNodeVolumeChanged signal.
  fake_cras_audio_client()->disable_volume_change_events();

  // Verify the volume state is not changed before OutputNodeVolumeChanged
  // signal fires.
  const int kVolume = 60;
  cras_audio_handler_->SetOutputVolumePercent(kVolume);
  EXPECT_EQ(0, test_observer_->output_volume_changed_count());
  EXPECT_EQ(kDefaultVolume, cras_audio_handler_->GetOutputVolumePercent());

  // Verify the output volume is changed to the designated value after
  // OnOutputNodeVolumeChanged cras signal fires, and the volume change event
  // has been fired to notify the observers.
  fake_cras_audio_client()->NotifyOutputNodeVolumeChangedForTesting(
      kInternalSpeaker->id, kVolume);
  EXPECT_EQ(1, test_observer_->output_volume_changed_count());
  EXPECT_EQ(kVolume, cras_audio_handler_->GetOutputVolumePercent());
  AudioDevice device;
  EXPECT_TRUE(cras_audio_handler_->GetPrimaryActiveOutputDevice(&device));
  EXPECT_EQ(device.id, kInternalSpeaker->id);
  EXPECT_EQ(kVolume, audio_pref_handler_->GetOutputVolumeValue(&device));
}

TEST_P(CrasAudioHandlerTest,
       ChangeOutputVolumesWithDelayedSignalForSingleActiveDevice) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalSpeaker});
  SetUpCrasAudioHandler(audio_nodes);
  EXPECT_EQ(0, test_observer_->output_volume_changed_count());

  const int kDefaultVolume = 75;
  EXPECT_EQ(kDefaultVolume, cras_audio_handler_->GetOutputVolumePercent());

  // Disable the auto OutputNodeVolumeChanged signal.
  fake_cras_audio_client()->disable_volume_change_events();

  // Verify the volume state is not changed before OutputNodeVolumeChanged
  // signal fires.
  const int kVolume1 = 50;
  const int kVolume2 = 60;
  cras_audio_handler_->SetOutputVolumePercent(kVolume1);
  cras_audio_handler_->SetOutputVolumePercent(kVolume2);
  EXPECT_EQ(0, test_observer_->output_volume_changed_count());
  EXPECT_EQ(kDefaultVolume, cras_audio_handler_->GetOutputVolumePercent());

  // Simulate OutputNodeVolumeChanged signal fired with big latency that
  // it lags behind the SetOutputNodeVolume requests. Chrome sets the volume
  // to 50 then 60, but the volume changed signal for 50 comes back after
  // chrome sets the volume to 60. Verify chrome will sync to the designated
  // volume level after all signals arrive.
  fake_cras_audio_client()->NotifyOutputNodeVolumeChangedForTesting(
      kInternalSpeaker->id, kVolume1);
  EXPECT_EQ(1, test_observer_->output_volume_changed_count());
  EXPECT_EQ(kVolume1, cras_audio_handler_->GetOutputVolumePercent());

  fake_cras_audio_client()->NotifyOutputNodeVolumeChangedForTesting(
      kInternalSpeaker->id, kVolume2);
  EXPECT_EQ(2, test_observer_->output_volume_changed_count());
  EXPECT_EQ(kVolume2, cras_audio_handler_->GetOutputVolumePercent());
}

TEST_P(CrasAudioHandlerTest,
       ChangeOutputVolumeFromNonChromeSourceSingleActiveDevice) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalSpeaker});
  SetUpCrasAudioHandler(audio_nodes);
  EXPECT_EQ(0, test_observer_->output_volume_changed_count());

  const int kDefaultVolume = 75;
  EXPECT_EQ(0, test_observer_->output_volume_changed_count());
  EXPECT_EQ(kDefaultVolume, cras_audio_handler_->GetOutputVolumePercent());

  // Simulate OutputNodeVolumeChanged signal fired by a non-chrome source.
  // Verify chrome will sync its volume state to the volume from the signal,
  // and notify its observers for the volume change event.
  const int kVolume = 20;
  fake_cras_audio_client()->NotifyOutputNodeVolumeChangedForTesting(
      kInternalSpeaker->id, kVolume);
  EXPECT_EQ(1, test_observer_->output_volume_changed_count());
  EXPECT_EQ(kVolume, cras_audio_handler_->GetOutputVolumePercent());
  AudioDevice device;
  EXPECT_TRUE(cras_audio_handler_->GetPrimaryActiveOutputDevice(&device));
  EXPECT_EQ(device.id, kInternalSpeaker->id);
  EXPECT_EQ(kVolume, audio_pref_handler_->GetOutputVolumeValue(&device));
}

TEST_P(CrasAudioHandlerTest,
       ChangeOutputVolumeFromNonChromeSourceNonActiveDevice) {
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kHeadphone});
  SetupCrasAudioHandlerWithActiveNodeInPref(
      audio_nodes, audio_nodes,
      AudioDevice(GenerateAudioNode(kInternalSpeaker)), true);
  EXPECT_EQ(0, test_observer_->output_volume_changed_count());

  const AudioDevice* device = GetDeviceFromId(kHeadphone->id);
  const int kDefaultVolume = 75;
  EXPECT_EQ(0, test_observer_->output_volume_changed_count());
  EXPECT_EQ(kDefaultVolume, audio_pref_handler_->GetOutputVolumeValue(device));

  const int kVolume = 20;
  fake_cras_audio_client()->NotifyOutputNodeVolumeChangedForTesting(
      kHeadphone->id, kVolume);
  EXPECT_EQ(1, test_observer_->output_volume_changed_count());
  // Since the device is not active,
  EXPECT_EQ(kDefaultVolume, cras_audio_handler_->GetOutputVolumePercent());
  EXPECT_EQ(kVolume, audio_pref_handler_->GetOutputVolumeValue(device));
}

TEST_P(CrasAudioHandlerTest, SetInputGainPercent) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalMic});
  SetUpCrasAudioHandler(audio_nodes);
  EXPECT_EQ(0, test_observer_->input_gain_changed_count());

  cras_audio_handler_->SetInputGainPercent(60);

  // Verify the input gain changed to the designated value,
  // OnInputNodeGainChanged event is fired, and the device gain value
  // is saved in the preferences.
  const int kGain = 60;
  EXPECT_EQ(kGain, cras_audio_handler_->GetInputGainPercent());
  EXPECT_EQ(1, test_observer_->input_gain_changed_count());
  AudioDevice internal_mic(GenerateAudioNode(kInternalMic));
  EXPECT_EQ(kGain, audio_pref_handler_->GetInputGainValue(&internal_mic));
}

TEST_P(CrasAudioHandlerTest, SetInputGainWithDelayedSignal) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalMic});
  SetUpCrasAudioHandler(audio_nodes);
  EXPECT_EQ(0, test_observer_->input_gain_changed_count());

  const int kDefaultGain = cras_audio_handler_->GetInputGainPercent();

  // Disable the auto InputNodeGainChanged signal.
  fake_cras_audio_client()->disable_gain_change_events();

  // Verify the gain state is not changed before InputNodeGainChanged
  // signal fires.
  const int kGain = 60;
  cras_audio_handler_->SetInputGainPercent(kGain);
  EXPECT_EQ(0, test_observer_->input_gain_changed_count());
  EXPECT_EQ(kDefaultGain, cras_audio_handler_->GetInputGainPercent());

  // Verify the output gain is changed to the designated value after
  // OnInputNodeGainChanged cras signal fires, and the gain change event
  // has been fired to notify the observers.
  fake_cras_audio_client()->NotifyInputNodeGainChangedForTesting(
      kInternalMic->id, kGain);
  EXPECT_EQ(1, test_observer_->input_gain_changed_count());
  EXPECT_EQ(kGain, cras_audio_handler_->GetInputGainPercent());
  AudioDevice device;
  EXPECT_TRUE(cras_audio_handler_->GetPrimaryActiveInputDevice(&device));
  EXPECT_EQ(device.id, kInternalMic->id);
  EXPECT_EQ(kGain, audio_pref_handler_->GetInputGainValue(&device));
}

TEST_P(CrasAudioHandlerTest,
       ChangeInputGainsWithDelayedSignalForSingleActiveDevice) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalMic});
  SetUpCrasAudioHandler(audio_nodes);
  EXPECT_EQ(0, test_observer_->input_gain_changed_count());

  const int kDefaultGain = cras_audio_handler_->GetInputGainPercent();

  // Disable the auto InputNodeGainChanged signal.
  fake_cras_audio_client()->disable_gain_change_events();

  // Verify the gain state is not changed before InputNodeGainChanged
  // signal fires.
  EXPECT_EQ(kDefaultGain, cras_audio_handler_->GetInputGainPercent());
  const int kGain1 = 10;
  const int kGain2 = 90;
  cras_audio_handler_->SetInputGainPercent(kGain1);
  EXPECT_EQ(0, test_observer_->input_gain_changed_count());
  EXPECT_EQ(kDefaultGain, cras_audio_handler_->GetInputGainPercent());
  cras_audio_handler_->SetInputGainPercent(kGain2);
  EXPECT_EQ(0, test_observer_->input_gain_changed_count());
  EXPECT_EQ(kDefaultGain, cras_audio_handler_->GetInputGainPercent());

  // Simulate InputNodeGainChanged signal fired with big latency that
  // it lags behind the SetInputNodeGain requests. Chrome sets the gain
  // to 50 then 60, but the gain changed signal for 50 comes back after
  // chrome sets the gain to 60. Verify chrome will sync to the designated
  // gain level after all signals arrive.
  fake_cras_audio_client()->NotifyInputNodeGainChangedForTesting(
      kInternalMic->id, kGain1);
  EXPECT_EQ(1, test_observer_->input_gain_changed_count());
  EXPECT_EQ(kGain1, cras_audio_handler_->GetInputGainPercent());

  fake_cras_audio_client()->NotifyInputNodeGainChangedForTesting(
      kInternalMic->id, kGain2);
  EXPECT_EQ(2, test_observer_->input_gain_changed_count());
  EXPECT_EQ(kGain2, cras_audio_handler_->GetInputGainPercent());
}

TEST_P(CrasAudioHandlerTest,
       ChangeInputGainFromNonChromeSourceSingleActiveDevice) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalMic});
  SetUpCrasAudioHandler(audio_nodes);
  EXPECT_EQ(0, test_observer_->input_gain_changed_count());

  // Simulate InputNodeGainChanged signal fired by a non-chrome source.
  // Verify chrome will sync its gain state to the gain from the signal,
  // and notify its observers for the gain change event.
  const int kGain = 20;
  fake_cras_audio_client()->NotifyInputNodeGainChangedForTesting(
      kInternalMic->id, kGain);
  EXPECT_EQ(1, test_observer_->input_gain_changed_count());
  EXPECT_EQ(kGain, cras_audio_handler_->GetInputGainPercent());
  AudioDevice device;
  EXPECT_TRUE(cras_audio_handler_->GetPrimaryActiveInputDevice(&device));
  EXPECT_EQ(device.id, kInternalMic->id);
  EXPECT_EQ(kGain, audio_pref_handler_->GetInputGainValue(&device));
}

TEST_P(CrasAudioHandlerTest, SetMuteForDevice) {
  AudioNodeList audio_nodes = GenerateAudioNodeList(
      {kInternalSpeaker, kHeadphone, kInternalMic, kUSBMic1});
  SetUpCrasAudioHandler(audio_nodes);

  // Mute the active output device.
  ExpectActiveDevice(/*is_input=*/false, /*expected_active_device=*/kHeadphone,
                     /*has_alternative_device=*/true);
  cras_audio_handler_->SetMuteForDevice(kHeadphone->id, true);

  // Verify the headphone is muted and mute value is saved in the preferences.
  EXPECT_TRUE(cras_audio_handler_->IsOutputMutedForDevice(kHeadphone->id));
  AudioDevice headphone(GenerateAudioNode(kHeadphone));
  EXPECT_TRUE(audio_pref_handler_->GetMuteValue(headphone));

  // Mute the non-active output device.
  cras_audio_handler_->SetMuteForDevice(kInternalSpeaker->id, true);

  // Verify the internal speaker is muted and mute value is saved in the
  // preferences.
  EXPECT_TRUE(
      cras_audio_handler_->IsOutputMutedForDevice(kInternalSpeaker->id));
  AudioDevice internal_speaker(GenerateAudioNode(kInternalSpeaker));
  EXPECT_TRUE(audio_pref_handler_->GetMuteValue(internal_speaker));

  // Mute the active input device.
  EXPECT_EQ(kUSBMic1->id, cras_audio_handler_->GetPrimaryActiveInputNode());
  cras_audio_handler_->SetMuteForDevice(kUSBMic1->id, true);

  // Verify the USB Mic is muted.
  EXPECT_TRUE(cras_audio_handler_->IsInputMutedForDevice(kUSBMic1->id));

  // Mute the non-active input device should be a no-op, see crbug.com/365050.
  cras_audio_handler_->SetMuteForDevice(kInternalMic->id, true);

  // Verify IsInputMutedForDevice returns false for non-active input device.
  EXPECT_FALSE(cras_audio_handler_->IsInputMutedForDevice(kInternalMic->id));
}

TEST_P(CrasAudioHandlerTest, SetVolumeGainPercentForDevice) {
  AudioNodeList audio_nodes = GenerateAudioNodeList(
      {kInternalSpeaker, kHeadphone, kInternalMic, kUSBMic1});
  SetUpCrasAudioHandler(audio_nodes);

  // Set volume percent for active output device.
  const int kHeadphoneVolume = 30;
  ExpectActiveDevice(/*is_input=*/false, /*expected_active_device=*/kHeadphone,
                     /*has_alternative_device=*/true);
  cras_audio_handler_->SetVolumeGainPercentForDevice(kHeadphone->id,
                                                     kHeadphoneVolume);

  // Verify the volume percent of headphone is set, and saved in preferences.
  EXPECT_EQ(
      kHeadphoneVolume,
      cras_audio_handler_->GetOutputVolumePercentForDevice(kHeadphone->id));
  AudioDevice headphone(GenerateAudioNode(kHeadphone));
  EXPECT_EQ(kHeadphoneVolume,
            audio_pref_handler_->GetOutputVolumeValue(&headphone));

  // Set volume percent for non-active output device.
  const int kSpeakerVolume = 60;
  cras_audio_handler_->SetVolumeGainPercentForDevice(kInternalSpeaker->id,
                                                     kSpeakerVolume);

  // Verify the volume percent of speaker is set, and saved in preferences.
  EXPECT_EQ(kSpeakerVolume,
            cras_audio_handler_->GetOutputVolumePercentForDevice(
                kInternalSpeaker->id));
  AudioDevice speaker(GenerateAudioNode(kInternalSpeaker));
  EXPECT_EQ(kSpeakerVolume,
            audio_pref_handler_->GetOutputVolumeValue(&speaker));

  // Set gain percent for active input device.
  const int kUSBMicGain = 30;
  EXPECT_EQ(kUSBMic1->id, cras_audio_handler_->GetPrimaryActiveInputNode());
  cras_audio_handler_->SetVolumeGainPercentForDevice(kUSBMic1->id, kUSBMicGain);

  // Verify the gain percent of USB mic is set, and saved in preferences.
  EXPECT_EQ(kUSBMicGain,
            cras_audio_handler_->GetOutputVolumePercentForDevice(kUSBMic1->id));
  AudioDevice usb_mic(GenerateAudioNode(kHeadphone));
  EXPECT_EQ(kUSBMicGain, audio_pref_handler_->GetInputGainValue(&usb_mic));

  // Set gain percent for non-active input device.
  const int kInternalMicGain = 60;
  cras_audio_handler_->SetVolumeGainPercentForDevice(kInternalMic->id,
                                                     kInternalMicGain);

  // Verify the gain percent of internal mic is set, and saved in preferences.
  EXPECT_EQ(
      kInternalMicGain,
      cras_audio_handler_->GetOutputVolumePercentForDevice(kInternalMic->id));
  AudioDevice internal_mic(GenerateAudioNode(kInternalMic));
  EXPECT_EQ(kInternalMicGain,
            audio_pref_handler_->GetInputGainValue(&internal_mic));
}

TEST_P(CrasAudioHandlerTest, TreatDualInternalMicNotAsAlternativeDevice) {
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kFrontMic, kRearMic},
      /*expected_active_input_node=*/kFrontMic,
      /*expected_active_output_node=*/kInternalSpeaker,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/false);
}

TEST_P(CrasAudioHandlerTest, ActiveDeviceSelectionWithStableDeviceId) {
  AudioNodeList audio_nodes;
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  audio_nodes.push_back(internal_speaker);
  AudioNode usb_headset = GenerateAudioNode(kUSBHeadphone1);
  usb_headset.plugged_time = 80000000;
  audio_nodes.push_back(usb_headset);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Initially active node is selected base on priority, so USB headphone
  // is selected.
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kUSBHeadphone1,
                     /*has_alternative_device=*/true);

  // Change the active device to internal speaker, now internal speaker has
  // higher preference priority than USB headphone.
  AudioDevice speaker(GenerateAudioNode(kInternalSpeaker));
  cras_audio_handler_->SwitchToDevice(speaker, true,
                                      DeviceActivateType::kActivateByUser);
  EXPECT_NE(kUSBHeadphone1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Unplug USB headset.
  audio_nodes.clear();
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);
  ChangeAudioNodes(audio_nodes);
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/false);

  // Plug the same USB headset back, id is different, but stable_device_id
  // remains the same.
  usb_headset.active = false;
  usb_headset.id = 98765;
  audio_nodes.push_back(usb_headset);
  ChangeAudioNodes(audio_nodes);

  // Since internal speaker has higher preference priority than USB headphone,
  // it won't be selected as active after it's plugged in again.
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/true);

  // Plug the second USB headset.
  AudioNode usb_headset2 = GenerateAudioNode(kUSBHeadphone2);
  usb_headset2.plugged_time = 80000001;
  audio_nodes.push_back(usb_headset2);
  ChangeAudioNodes(audio_nodes);

  // Since the second USB device is new, it's selected as the active device
  // by its priority.
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kUSBHeadphone2,
                     /*has_alternative_device=*/true);

  // Unplug the second USB headset.
  audio_nodes.clear();
  internal_speaker.active = false;
  audio_nodes.push_back(internal_speaker);
  audio_nodes.push_back(usb_headset);
  ChangeAudioNodes(audio_nodes);

  // There is no active node after USB2 unplugged, the internal speaker got
  // selected by its preference priority.
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/true);

  audio_nodes.clear();
  internal_speaker.active = false;
  audio_nodes.push_back(internal_speaker);
  usb_headset.active = true;
  audio_nodes.push_back(usb_headset);
  usb_headset2.active = false;
  usb_headset2.plugged_time = 80000002;
  audio_nodes.push_back(usb_headset2);
  ChangeAudioNodes(audio_nodes);

  // Plug the second USB again. Since it was the active node before it got
  // unplugged, it is now selected as the active node.
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kUSBHeadphone2,
                     /*has_alternative_device=*/true);
}

TEST_P(
    CrasAudioHandlerTest,
    ActiveDeviceSelectionWithStableDeviceId_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  AudioNodeList audio_nodes;
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  audio_nodes.push_back(internal_speaker);
  AudioNode usb_headset = GenerateAudioNode(kUSBHeadphone1);
  usb_headset.plugged_time = 80000000;
  audio_nodes.push_back(usb_headset);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // The device set of kInternalSpeaker and kUSBHeadphone1 was un seen before.
  // No most recently active device list is available. Activate the
  // kInternalSpeaker first.
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/true);

  // Change the active device to kInternalSpeaker, now kInternalSpeaker is the
  // preferred device in the device set of kInternalSpeaker and kUSBHeadphone1.
  AudioDevice speaker(GenerateAudioNode(kInternalSpeaker));
  cras_audio_handler_->SwitchToDevice(speaker, true,
                                      DeviceActivateType::kActivateByUser);
  EXPECT_NE(kUSBHeadphone1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Unplug USB headset.
  audio_nodes.clear();
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);
  ChangeAudioNodes(audio_nodes);
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/false);

  // Plug the same USB headset back, id is different, but stable_device_id
  // remains the same.
  usb_headset.active = false;
  usb_headset.id = 98765;
  audio_nodes.push_back(usb_headset);
  ChangeAudioNodes(audio_nodes);

  // Since kInternalSpeaker is the preferred device,
  // kUSBHeadphone1 won't be selected as active after it's plugged in again.
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/true);

  // Plug the second USB headset.
  AudioNode usb_headset2 = GenerateAudioNode(kUSBHeadphone2);
  usb_headset2.plugged_time = 80000001;
  audio_nodes.push_back(usb_headset2);
  ChangeAudioNodes(audio_nodes);

  // Since the second USB device is new, it's not selected as the active device.
  EXPECT_NE(kUSBHeadphone2->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Change the active device to the second USB device, now the second USB
  // device has higher preference priority.
  AudioDevice usb_headset2_device(GenerateAudioNode(kUSBHeadphone2));
  cras_audio_handler_->SwitchToDevice(usb_headset2_device, true,
                                      DeviceActivateType::kActivateByUser);
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kUSBHeadphone2,
                     /*has_alternative_device=*/true);

  // Unplug the second USB headset.
  audio_nodes.clear();
  internal_speaker.active = false;
  audio_nodes.push_back(internal_speaker);
  audio_nodes.push_back(usb_headset);
  ChangeAudioNodes(audio_nodes);

  // The internal speaker gets selected based on previous priority.
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/true);

  audio_nodes.clear();
  internal_speaker.active = false;
  audio_nodes.push_back(internal_speaker);
  usb_headset.active = true;
  audio_nodes.push_back(usb_headset);
  usb_headset2.active = false;
  usb_headset2.plugged_time = 80000002;
  audio_nodes.push_back(usb_headset2);
  ChangeAudioNodes(audio_nodes);

  // Plug the second USB again. Since it was the active node before it got
  // unplugged, it is now selected as the active node.
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kUSBHeadphone2,
                     /*has_alternative_device=*/true);
}

// Test the device new session case, either via reboot or logout, if there
// is an active device in the previous session, that device should still
// be set as active after the new session starts.
TEST_P(CrasAudioHandlerTest, PersistActiveDeviceAcrossSession) {
  // Set the active device to internal speaker before the session starts.
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kHeadphone});
  SetupCrasAudioHandlerWithActiveNodeInPref(
      audio_nodes, audio_nodes,
      AudioDevice(GenerateAudioNode(kInternalSpeaker)), true);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the active device is the internal speaker, which is of a lower
  // priority, but selected as active since it was the active device previously.
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/true);
}

TEST_P(CrasAudioHandlerTest, PersistActiveSpeakerAcrossReboot) {
  // Simulates the device was shut down with three audio devices, and
  // internal speaker being the active one selected by user.
  AudioNodeList audio_nodes_in_pref =
      GenerateAudioNodeList({kInternalSpeaker, kHeadphone, kUSBHeadphone1});

  // Simulate the first NodesChanged signal coming with only one node.
  AudioNodeList audio_nodes = GenerateAudioNodeList({kUSBHeadphone1});

  SetupCrasAudioHandlerWithActiveNodeInPref(
      audio_nodes, audio_nodes_in_pref,
      AudioDevice(GenerateAudioNode(kInternalSpeaker)), true);

  // Verify the usb headphone has been made active.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kUSBHeadphone1,
                     /*has_alternative_device=*/false);

  // Simulate another NodesChanged signal coming later with all ndoes.
  audio_nodes.push_back(GenerateAudioNode(kInternalSpeaker));
  audio_nodes.push_back(GenerateAudioNode(kHeadphone));
  ChangeAudioNodes(audio_nodes);

  // Verify the active output has been restored to internal speaker.
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/true);
}

// crbug.com/698809. User plug in USB speaker, then unplug it, leave
// internal speaker as active device. Power down, plug in USB speaker again.
// When the device powers up again, the first NodesChanged signal comes with
// only USB speaker; followed by another NodesChanged signal with internal
// speaker added.
TEST_P(CrasAudioHandlerTest, USBShouldBeActiveAfterReboot) {
  // Start with both interanl speaker and USB speaker.
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kUSBHeadphone1});

  SetUpCrasAudioHandler(audio_nodes);

  // Verify the usb headphone has been made active by priority.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_EQ(kUSBHeadphone1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Remove USB headphone.
  audio_nodes.clear();
  audio_nodes.push_back(GenerateAudioNode(kInternalSpeaker));
  ChangeAudioNodes(audio_nodes);
  // Verify the internal speaker becomes the active device by priority.
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Simulate after power off, plug in usb header phone, then power on.
  // The first NodesChanged signal sends usb headphone only.
  audio_nodes.clear();
  audio_nodes.push_back(GenerateAudioNode(kUSBHeadphone1));
  ChangeAudioNodes(audio_nodes);
  // Verify the usb headerphone becomes the active device by priority.
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_EQ(kUSBHeadphone1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Simulate the second NodesChanged signal comes with internal speaker added.
  audio_nodes.clear();
  AudioNode usb_headphone = GenerateAudioNode(kUSBHeadphone1);
  usb_headphone.active = true;
  audio_nodes.push_back(usb_headphone);
  audio_nodes.push_back(GenerateAudioNode(kInternalSpeaker));
  ChangeAudioNodes(audio_nodes);
  // Verify the usb headerphone is still the active device.
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_EQ(kUSBHeadphone1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
}

// Test the corner case that headphone is plugged in for the first time on
// a cros device after the device is shutdown.
// crbug.com/622045.
TEST_P(CrasAudioHandlerTest, PlugInHeadphoneFirstTimeAfterPowerDown) {
  // Simulate plugging headphone for the first on a cros device after it is
  // powered down. Internal speaker is set up in audio prefs as active
  // before the new cros session starts.
  AudioNodeList audio_nodes_in_pref = GenerateAudioNodeList({kInternalSpeaker});

  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kHeadphone});

  SetupCrasAudioHandlerWithActiveNodeInPref(
      audio_nodes, audio_nodes_in_pref,
      AudioDevice(GenerateAudioNode(kInternalSpeaker)), false);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify headphone becomes the active output.
  EXPECT_EQ(kHeadphone->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
}

TEST_P(CrasAudioHandlerTest,
       PersistActiveUsbHeadphoneAcrossRebootUsbComeFirst) {
  // Simulates the device was shut down with three audio devices, and
  // usb headphone being the active one selected by priority.
  AudioNodeList audio_nodes_in_pref =
      GenerateAudioNodeList({kInternalSpeaker, kHeadphone, kUSBHeadphone1});

  // Simulate the first NodesChanged signal coming with only internal speaker
  // and the USB headphone.
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kUSBHeadphone1});

  SetupCrasAudioHandlerWithActiveNodeInPref(
      audio_nodes, audio_nodes_in_pref,
      AudioDevice(GenerateAudioNode(kUSBHeadphone1)), false);

  // Verify the USB headphone has been made active.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_EQ(kUSBHeadphone1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Simulate another NodesChanged signal coming later with all ndoes.
  AudioNode headphone_node = GenerateAudioNode(kHeadphone);
  headphone_node.plugged_time = 80000000;
  audio_nodes.push_back(headphone_node);
  ChangeAudioNodes(audio_nodes);

  // Verify the active output has been restored to USB headphone.
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_EQ(kUSBHeadphone1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
}

// This covers the crbug.com/586026. Cras lost the active state of the internal
// speaker when user unplugs the headphone, which is a bug in cras. However,
// chrome code is still resilient and set the internal speaker back to active.
TEST_P(CrasAudioHandlerTest, UnplugHeadphoneLostActiveInternalSpeakerByCras) {
  // Set up with three nodes.
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kHeadphone, kUSBHeadphone1});
  SetUpCrasAudioHandler(audio_nodes);

  // Switch the active output to internal speaker.
  cras_audio_handler_->SwitchToDevice(
      AudioDevice(GenerateAudioNode(kInternalSpeaker)), true,
      DeviceActivateType::kActivateByUser);

  // Verify internal speaker has been made active.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Simulate unplug the headphone. Cras sends NodesChanged signal with
  // both internal speaker and usb headphone being inactive.
  audio_nodes.clear();
  audio_nodes.push_back(GenerateAudioNode(kInternalSpeaker));
  audio_nodes.push_back(GenerateAudioNode(kUSBHeadphone1));
  for (const auto& node : audio_nodes) {
    ASSERT_FALSE(node.active) << node.id << " expexted to be inactive";
  }
  ChangeAudioNodes(audio_nodes);

  // Verify the active output is set back to internal speaker.
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
}

TEST_P(CrasAudioHandlerTest, RemoveNonActiveDevice) {
  // Set up with three nodes.
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kHeadphone, kUSBHeadphone1});
  SetUpCrasAudioHandler(audio_nodes);

  // Switch the active output to internal speaker.
  cras_audio_handler_->SwitchToDevice(
      AudioDevice(GenerateAudioNode(kInternalSpeaker)), true,
      DeviceActivateType::kActivateByUser);

  // Verify internal speaker has been made active.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Remove headphone, which is an non-active device.
  audio_nodes.clear();
  AudioNode speaker = GenerateAudioNode(kInternalSpeaker);
  speaker.active = true;
  audio_nodes.push_back(speaker);
  AudioNode usb_headphone = GenerateAudioNode(kUSBHeadphone1);
  usb_headphone.active = false;
  audio_nodes.push_back(usb_headphone);

  ChangeAudioNodes(audio_nodes);

  // Verify the active output remains as internal speaker.
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
}

TEST_P(CrasAudioHandlerTest, ChangeActiveNodesHotrodInit) {
  // This simulates a typical hotrod audio device configuration.
  // Verify only the 1st jabra speaker's output and input are selected as active
  // nodes by CrasAudioHandler.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kHDMIOutput, kUSBJabraSpeakerOutput1,
                         kUSBJabraSpeakerOutput2, kUSBJabraSpeakerInput1,
                         kUSBJabraSpeakerInput2, kUSBCameraInput},
      /*expected_active_input_node=*/kUSBJabraSpeakerInput1,
      /*expected_active_output_node=*/kUSBJabraSpeakerOutput1,
      /*expected_has_alternative_input=*/true,
      /*expected_has_alternative_output=*/true);

  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(2, GetActiveDeviceCount());

  // Set both jabra speakers's input and output nodes to active, this simulate
  // the call sent by hotrod initialization process.
  test_observer_->reset_active_output_node_changed_count();
  test_observer_->reset_active_input_node_changed_count();
  cras_audio_handler_->ChangeActiveNodes(
      {kUSBJabraSpeakerOutput1->id, kUSBJabraSpeakerOutput2->id,
       kUSBJabraSpeakerInput1->id, kUSBJabraSpeakerInput2->id});

  // Verify both jabra speakers' input/output nodes are made active.
  // num_active_nodes = GetActiveDeviceCount();
  EXPECT_EQ(4, GetActiveDeviceCount());
  const AudioDevice* active_output_1 =
      GetDeviceFromId(kUSBJabraSpeakerOutput1->id);
  EXPECT_TRUE(active_output_1->active);
  const AudioDevice* active_output_2 =
      GetDeviceFromId(kUSBJabraSpeakerOutput2->id);
  EXPECT_TRUE(active_output_2->active);
  AudioDevice primary_active_device;
  EXPECT_TRUE(cras_audio_handler_->GetPrimaryActiveOutputDevice(
      &primary_active_device));
  EXPECT_EQ(kUSBJabraSpeakerOutput1->id, primary_active_device.id);
  const AudioDevice* active_input_1 =
      GetDeviceFromId(kUSBJabraSpeakerInput1->id);
  EXPECT_TRUE(active_input_1->active);
  const AudioDevice* active_input_2 =
      GetDeviceFromId(kUSBJabraSpeakerInput2->id);
  EXPECT_TRUE(active_input_2->active);
  EXPECT_EQ(kUSBJabraSpeakerInput1->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

  // Verify only 1 ActiveOutputNodeChanged notification has been sent out
  // by calling ChangeActiveNodes.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_EQ(1, test_observer_->active_input_node_changed_count());

  // Verify all active devices are the not muted and their volume values are
  // the same.
  EXPECT_FALSE(cras_audio_handler_->IsOutputMuted());
  EXPECT_FALSE(
      cras_audio_handler_->IsOutputMutedForDevice(kUSBJabraSpeakerOutput1->id));
  EXPECT_FALSE(
      cras_audio_handler_->IsOutputMutedForDevice(kUSBJabraSpeakerOutput2->id));
  EXPECT_EQ(cras_audio_handler_->GetOutputVolumePercent(),
            cras_audio_handler_->GetOutputVolumePercentForDevice(
                kUSBJabraSpeakerOutput1->id));
  EXPECT_EQ(cras_audio_handler_->GetOutputVolumePercent(),
            cras_audio_handler_->GetOutputVolumePercentForDevice(
                kUSBJabraSpeakerOutput2->id));

  // Adjust the volume of output devices, verify all active nodes are set to
  // the same volume.
  cras_audio_handler_->SetOutputVolumePercent(25);
  EXPECT_EQ(25, cras_audio_handler_->GetOutputVolumePercent());
  EXPECT_EQ(25, cras_audio_handler_->GetOutputVolumePercentForDevice(
                    kUSBJabraSpeakerOutput1->id));
  EXPECT_EQ(25, cras_audio_handler_->GetOutputVolumePercentForDevice(
                    kUSBJabraSpeakerOutput2->id));
}

TEST_P(CrasAudioHandlerTest, SetActiveNodesHotrodInit) {
  // This simulates a typical hotrod audio device configuration.
  // Verify only the 1st jabra speaker's output and input are selected as active
  // nodes by CrasAudioHandler.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kHDMIOutput, kUSBJabraSpeakerOutput1,
                         kUSBJabraSpeakerOutput2, kUSBJabraSpeakerInput1,
                         kUSBJabraSpeakerInput2, kUSBCameraInput},
      /*expected_active_input_node=*/kUSBJabraSpeakerInput1,
      /*expected_active_output_node=*/kUSBJabraSpeakerOutput1,
      /*expected_has_alternative_input=*/true,
      /*expected_has_alternative_output=*/true);

  // Set both jabra speakers's input and output nodes to active, this simulate
  // the call sent by hotrod initialization process.
  test_observer_->reset_active_output_node_changed_count();
  test_observer_->reset_active_input_node_changed_count();

  cras_audio_handler_->SetActiveInputNodes(
      {kUSBJabraSpeakerInput1->id, kUSBJabraSpeakerInput2->id});

  cras_audio_handler_->SetActiveOutputNodes(
      {kUSBJabraSpeakerOutput1->id, kUSBJabraSpeakerOutput2->id});

  // Verify both jabra speakers' input/output nodes are made active.
  // num_active_nodes = GetActiveDeviceCount();
  EXPECT_EQ(4, GetActiveDeviceCount());
  const AudioDevice* active_output_1 =
      GetDeviceFromId(kUSBJabraSpeakerOutput1->id);
  EXPECT_TRUE(active_output_1->active);
  const AudioDevice* active_output_2 =
      GetDeviceFromId(kUSBJabraSpeakerOutput2->id);
  EXPECT_TRUE(active_output_2->active);
  AudioDevice primary_active_device;
  EXPECT_TRUE(cras_audio_handler_->GetPrimaryActiveOutputDevice(
      &primary_active_device));
  EXPECT_EQ(kUSBJabraSpeakerOutput1->id, primary_active_device.id);
  const AudioDevice* active_input_1 =
      GetDeviceFromId(kUSBJabraSpeakerInput1->id);
  EXPECT_TRUE(active_input_1->active);
  const AudioDevice* active_input_2 =
      GetDeviceFromId(kUSBJabraSpeakerInput2->id);
  EXPECT_TRUE(active_input_2->active);
  EXPECT_EQ(kUSBJabraSpeakerInput1->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

  // Verify only 1 ActiveOutputNodeChanged notification has been sent out
  // by calling SetActiveNodes.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_EQ(1, test_observer_->active_input_node_changed_count());

  // Verify all active devices are the not muted and their volume values are
  // the same.
  EXPECT_FALSE(cras_audio_handler_->IsOutputMuted());
  EXPECT_FALSE(
      cras_audio_handler_->IsOutputMutedForDevice(kUSBJabraSpeakerOutput1->id));
  EXPECT_FALSE(
      cras_audio_handler_->IsOutputMutedForDevice(kUSBJabraSpeakerOutput2->id));
  EXPECT_EQ(cras_audio_handler_->GetOutputVolumePercent(),
            cras_audio_handler_->GetOutputVolumePercentForDevice(
                kUSBJabraSpeakerOutput1->id));
  EXPECT_EQ(cras_audio_handler_->GetOutputVolumePercent(),
            cras_audio_handler_->GetOutputVolumePercentForDevice(
                kUSBJabraSpeakerOutput2->id));

  // Adjust the volume of output devices, verify all active nodes are set to
  // the same volume.
  cras_audio_handler_->SetOutputVolumePercent(25);
  EXPECT_EQ(25, cras_audio_handler_->GetOutputVolumePercent());
  EXPECT_EQ(25, cras_audio_handler_->GetOutputVolumePercentForDevice(
                    kUSBJabraSpeakerOutput1->id));
  EXPECT_EQ(25, cras_audio_handler_->GetOutputVolumePercentForDevice(
                    kUSBJabraSpeakerOutput2->id));
}

TEST_P(CrasAudioHandlerTest, ChangeVolumeHotrodDualSpeakersWithDelayedSignals) {
  AudioNodeList audio_nodes = GenerateAudioNodeList(
      {kHDMIOutput, kUSBJabraSpeakerOutput1, kUSBJabraSpeakerOutput2});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Set both jabra speakers nodes to active, this simulate
  // the call sent by hotrod initialization process.
  test_observer_->reset_active_output_node_changed_count();
  cras_audio_handler_->ChangeActiveNodes(
      {kUSBJabraSpeakerOutput1->id, kUSBJabraSpeakerOutput2->id});

  // Verify both jabra speakers are made active.
  EXPECT_EQ(2, GetActiveDeviceCount());
  const AudioDevice* active_output_1 =
      GetDeviceFromId(kUSBJabraSpeakerOutput1->id);
  EXPECT_TRUE(active_output_1->active);
  const AudioDevice* active_output_2 =
      GetDeviceFromId(kUSBJabraSpeakerOutput2->id);
  EXPECT_TRUE(active_output_2->active);

  // Verify all active devices are the not muted and their volume values are
  // the same.
  EXPECT_FALSE(cras_audio_handler_->IsOutputMuted());
  EXPECT_FALSE(
      cras_audio_handler_->IsOutputMutedForDevice(kUSBJabraSpeakerOutput1->id));
  EXPECT_FALSE(
      cras_audio_handler_->IsOutputMutedForDevice(kUSBJabraSpeakerOutput2->id));
  EXPECT_EQ(cras_audio_handler_->GetOutputVolumePercent(),
            cras_audio_handler_->GetOutputVolumePercentForDevice(
                kUSBJabraSpeakerOutput1->id));
  EXPECT_EQ(cras_audio_handler_->GetOutputVolumePercent(),
            cras_audio_handler_->GetOutputVolumePercentForDevice(
                kUSBJabraSpeakerOutput2->id));
  const int kDefaultVolume = 75;
  EXPECT_EQ(kDefaultVolume, cras_audio_handler_->GetOutputVolumePercent());

  // Disable the auto OutputNodeVolumeChanged signal.
  fake_cras_audio_client()->disable_volume_change_events();
  test_observer_->reset_output_volume_changed_count();

  // Adjust the volume of output devices continuously.
  cras_audio_handler_->SetOutputVolumePercent(20);
  cras_audio_handler_->SetOutputVolumePercent(30);

  // Sends delayed OutputNodeVolumeChanged signals.
  fake_cras_audio_client()->NotifyOutputNodeVolumeChangedForTesting(
      kUSBJabraSpeakerOutput2->id, 20);
  fake_cras_audio_client()->NotifyOutputNodeVolumeChangedForTesting(
      kUSBJabraSpeakerOutput1->id, 20);
  fake_cras_audio_client()->NotifyOutputNodeVolumeChangedForTesting(
      kUSBJabraSpeakerOutput2->id, 30);
  fake_cras_audio_client()->NotifyOutputNodeVolumeChangedForTesting(
      kUSBJabraSpeakerOutput1->id, 30);

  // Verify that both speakers are set to the designated volume level after
  // receiving all delayed signals.
  EXPECT_EQ(4, test_observer_->output_volume_changed_count());
  EXPECT_EQ(30, cras_audio_handler_->GetOutputVolumePercent());
  EXPECT_EQ(30, cras_audio_handler_->GetOutputVolumePercentForDevice(
                    kUSBJabraSpeakerOutput1->id));
  EXPECT_EQ(30, cras_audio_handler_->GetOutputVolumePercentForDevice(
                    kUSBJabraSpeakerOutput2->id));
}

TEST_P(CrasAudioHandlerTest, ChangeActiveNodesHotrodInitWithCameraInputActive) {
  AudioNodeList audio_nodes = GenerateAudioNodeList(
      {kHDMIOutput, kUSBJabraSpeakerOutput1, kUSBJabraSpeakerOutput2,
       kUSBJabraSpeakerInput1, kUSBJabraSpeakerInput2});
  // Make the camera input to be plugged in later than jabra's input.
  AudioNode usb_camera = GenerateAudioNode(kUSBCameraInput);
  usb_camera.plugged_time = 10000000;
  audio_nodes.push_back(usb_camera);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the 1st jabra speaker's output is selected as active output
  // node and camera's input is selected active input by CrasAudioHandler.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerOutput1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBCameraInput->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

  // Set both jabra speakers's input and output nodes to active, this simulates
  // the call sent by hotrod initialization process.
  test_observer_->reset_active_output_node_changed_count();
  test_observer_->reset_active_input_node_changed_count();
  cras_audio_handler_->ChangeActiveNodes(
      {kUSBJabraSpeakerOutput1->id, kUSBJabraSpeakerOutput2->id,
       kUSBJabraSpeakerInput1->id, kUSBJabraSpeakerInput2->id});

  // Verify both jabra speakers' input/output nodes are made active.
  // num_active_nodes = GetActiveDeviceCount();
  EXPECT_EQ(4, GetActiveDeviceCount());
  const AudioDevice* active_output_1 =
      GetDeviceFromId(kUSBJabraSpeakerOutput1->id);
  EXPECT_TRUE(active_output_1->active);
  const AudioDevice* active_output_2 =
      GetDeviceFromId(kUSBJabraSpeakerOutput2->id);
  EXPECT_TRUE(active_output_2->active);
  EXPECT_EQ(kUSBJabraSpeakerOutput1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  const AudioDevice* active_input_1 =
      GetDeviceFromId(kUSBJabraSpeakerInput1->id);
  EXPECT_TRUE(active_input_1->active);
  const AudioDevice* active_input_2 =
      GetDeviceFromId(kUSBJabraSpeakerInput2->id);
  EXPECT_TRUE(active_input_2->active);
  EXPECT_EQ(kUSBJabraSpeakerInput1->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

  // Verify only 1 ActiveOutputNodeChanged notification has been sent out
  // by calling ChangeActiveNodes.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_EQ(1, test_observer_->active_input_node_changed_count());
}

TEST_P(CrasAudioHandlerTest, SetActiveNodesHotrodInitWithCameraInputActive) {
  AudioNodeList audio_nodes = GenerateAudioNodeList(
      {kHDMIOutput, kUSBJabraSpeakerOutput1, kUSBJabraSpeakerOutput2,
       kUSBJabraSpeakerInput1, kUSBJabraSpeakerInput2});
  // Make the camera input to be plugged in later than jabra's input.
  AudioNode usb_camera = GenerateAudioNode(kUSBCameraInput);
  usb_camera.plugged_time = 10000000;
  audio_nodes.push_back(usb_camera);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the 1st jabra speaker's output is selected as active output
  // node and camera's input is selected active input by CrasAudioHandler.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerOutput1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBCameraInput->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

  // Set both jabra speakers's input and output nodes to active, this simulates
  // the call sent by hotrod initialization process.
  test_observer_->reset_active_output_node_changed_count();
  test_observer_->reset_active_input_node_changed_count();

  cras_audio_handler_->SetActiveOutputNodes(
      {kUSBJabraSpeakerOutput1->id, kUSBJabraSpeakerOutput2->id});

  cras_audio_handler_->SetActiveInputNodes(
      {kUSBJabraSpeakerInput1->id, kUSBJabraSpeakerInput2->id});

  // Verify both jabra speakers' input/output nodes are made active.
  // num_active_nodes = GetActiveDeviceCount();
  EXPECT_EQ(4, GetActiveDeviceCount());
  const AudioDevice* active_output_1 =
      GetDeviceFromId(kUSBJabraSpeakerOutput1->id);
  EXPECT_TRUE(active_output_1->active);
  const AudioDevice* active_output_2 =
      GetDeviceFromId(kUSBJabraSpeakerOutput2->id);
  EXPECT_TRUE(active_output_2->active);
  EXPECT_EQ(kUSBJabraSpeakerOutput1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  const AudioDevice* active_input_1 =
      GetDeviceFromId(kUSBJabraSpeakerInput1->id);
  EXPECT_TRUE(active_input_1->active);
  const AudioDevice* active_input_2 =
      GetDeviceFromId(kUSBJabraSpeakerInput2->id);
  EXPECT_TRUE(active_input_2->active);
  EXPECT_EQ(kUSBJabraSpeakerInput1->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

  // Verify only 1 ActiveOutputNodeChanged notification has been sent out
  // by calling ChangeActiveNodes.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_EQ(1, test_observer_->active_input_node_changed_count());
}

TEST_P(CrasAudioHandlerTest, ChangeActiveNodesWithFewerActives) {
  AudioNodeList audio_nodes = GenerateAudioNodeList(
      {kHDMIOutput, kUSBJabraSpeakerOutput1, kUSBJabraSpeakerOutput2});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Set all three nodes to be active.
  cras_audio_handler_->ChangeActiveNodes({kHDMIOutput->id,
                                          kUSBJabraSpeakerOutput1->id,
                                          kUSBJabraSpeakerOutput2->id});

  // Verify all three nodes are active.
  EXPECT_EQ(3, GetActiveDeviceCount());
  const AudioDevice* active_output_1 = GetDeviceFromId(kHDMIOutput->id);
  EXPECT_TRUE(active_output_1->active);
  const AudioDevice* active_output_2 =
      GetDeviceFromId(kUSBJabraSpeakerOutput1->id);
  EXPECT_TRUE(active_output_2->active);
  const AudioDevice* active_output_3 =
      GetDeviceFromId(kUSBJabraSpeakerOutput2->id);
  EXPECT_TRUE(active_output_3->active);

  // Now call ChangeActiveDevices with only 2 nodes.
  cras_audio_handler_->ChangeActiveNodes(
      {kUSBJabraSpeakerOutput1->id, kUSBJabraSpeakerOutput2->id});

  // Verify only 2 nodes are active.
  EXPECT_EQ(2, GetActiveDeviceCount());
  const AudioDevice* output_1 = GetDeviceFromId(kHDMIOutput->id);
  EXPECT_FALSE(output_1->active);
  const AudioDevice* output_2 = GetDeviceFromId(kUSBJabraSpeakerOutput1->id);
  EXPECT_TRUE(output_2->active);
  const AudioDevice* output_3 = GetDeviceFromId(kUSBJabraSpeakerOutput2->id);
  EXPECT_TRUE(output_3->active);
}

TEST_P(CrasAudioHandlerTest, SetActiveNodesWithFewerActives) {
  AudioNodeList audio_nodes = GenerateAudioNodeList(
      {kHDMIOutput, kUSBJabraSpeakerOutput1, kUSBJabraSpeakerOutput2});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Set all three nodes to be active.
  cras_audio_handler_->SetActiveOutputNodes({kHDMIOutput->id,
                                             kUSBJabraSpeakerOutput1->id,
                                             kUSBJabraSpeakerOutput2->id});

  // Verify all three nodes are active.
  EXPECT_EQ(3, GetActiveDeviceCount());
  const AudioDevice* active_output_1 = GetDeviceFromId(kHDMIOutput->id);
  EXPECT_TRUE(active_output_1->active);
  const AudioDevice* active_output_2 =
      GetDeviceFromId(kUSBJabraSpeakerOutput1->id);
  EXPECT_TRUE(active_output_2->active);
  const AudioDevice* active_output_3 =
      GetDeviceFromId(kUSBJabraSpeakerOutput2->id);
  EXPECT_TRUE(active_output_3->active);

  // Now call SetActiveOutputNodes with only 2 nodes.
  cras_audio_handler_->SetActiveOutputNodes(
      {kUSBJabraSpeakerOutput1->id, kUSBJabraSpeakerOutput2->id});

  // Verify only 2 nodes are active.
  EXPECT_EQ(2, GetActiveDeviceCount());
  const AudioDevice* output_1 = GetDeviceFromId(kHDMIOutput->id);
  EXPECT_FALSE(output_1->active);
  const AudioDevice* output_2 = GetDeviceFromId(kUSBJabraSpeakerOutput1->id);
  EXPECT_TRUE(output_2->active);
  const AudioDevice* output_3 = GetDeviceFromId(kUSBJabraSpeakerOutput2->id);
  EXPECT_TRUE(output_3->active);
}

TEST_P(CrasAudioHandlerTest, HotrodInitWithSingleJabra) {
  // Simulates the hotrod initializated with a single jabra device and
  // CrasAudioHandler selected jabra input/output as active devices.
  // Verify the jabra speaker's output and input are selected as active nodes
  // by CrasAudioHandler.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kHDMIOutput, kUSBJabraSpeakerOutput1,
                         kUSBJabraSpeakerInput1, kUSBCameraInput},
      /*expected_active_input_node=*/kUSBJabraSpeakerInput1,
      /*expected_active_output_node=*/kUSBJabraSpeakerOutput1,
      /*expected_has_alternative_input=*/true,
      /*expected_has_alternative_output=*/true);
}

TEST_P(CrasAudioHandlerTest,
       ChangeActiveNodesHotrodInitWithSingleJabraCameraPlugInLater) {
  AudioNodeList audio_nodes = GenerateAudioNodeList(
      {kHDMIOutput, kUSBJabraSpeakerOutput1, kUSBJabraSpeakerInput1});
  AudioNode usb_camera = GenerateAudioNode(kUSBCameraInput);
  usb_camera.plugged_time = 10000000;
  audio_nodes.push_back(usb_camera);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the jabra speaker's output is selected as active output, and
  // camera's input is selected as active input by CrasAudioHandler
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerOutput1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBCameraInput->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

  // Simulate hotrod app call to set jabra input as active device with only
  // jabra input node in the active node list, which does not conform to the
  // new SetActiveDevices protocol, but just show we can still handle it if
  // this happens.
  cras_audio_handler_->ChangeActiveNodes(
      {kUSBJabraSpeakerOutput1->id, kUSBJabraSpeakerInput1->id});

  // Verify the jabra speaker's output is selected as active output, and
  // jabra's input is selected as active input.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerOutput1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBJabraSpeakerInput1->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());
}

TEST_P(CrasAudioHandlerTest,
       SetActiveNodesHotrodInitWithSingleJabraCameraPlugInLater) {
  AudioNodeList audio_nodes = GenerateAudioNodeList(
      {kHDMIOutput, kUSBJabraSpeakerOutput1, kUSBJabraSpeakerInput1});
  AudioNode usb_camera = GenerateAudioNode(kUSBCameraInput);
  usb_camera.plugged_time = 10000000;
  audio_nodes.push_back(usb_camera);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the jabra speaker's output is selected as active output, and
  // camera's input is selected as active input by CrasAudioHandler
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerOutput1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBCameraInput->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

  // Simulate hotrod app call to set jabra input as active device with only
  // jabra input node in the active node list, which does not conform to the
  // new SetActiveDevices protocol, but just show we can still handle it if
  // this happens.
  cras_audio_handler_->SetActiveOutputNodes({kUSBJabraSpeakerOutput1->id});

  cras_audio_handler_->SetActiveInputNodes({kUSBJabraSpeakerInput1->id});

  // Verify the jabra speaker's output is selected as active output, and
  // jabra's input is selected as active input.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerOutput1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBJabraSpeakerInput1->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());
}

TEST_P(CrasAudioHandlerTest, ChangeActiveNodesDeactivatePrimaryActiveNode) {
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kUSBJabraSpeakerInput1, kUSBJabraSpeakerInput2});
  AudioNode usb_camera = GenerateAudioNode(kUSBCameraInput);
  usb_camera.plugged_time = 10000000;
  audio_nodes.push_back(usb_camera);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the camera's input is selected as active input by CrasAudioHandler
  EXPECT_EQ(1, GetActiveDeviceCount());
  EXPECT_EQ(kUSBCameraInput->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

  // Simulate hotrod app call to set jabra input as active device with only
  // jabra input node in the active node list, which does not conform to the
  // new SetActiveDevices protocol, but just show we can still handle it if
  // this happens.
  cras_audio_handler_->ChangeActiveNodes(
      {kUSBJabraSpeakerInput1->id, kUSBCameraInput->id});

  // Verify active input devices are set as expected, with primary active input
  // staying the same.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBCameraInput->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

  const AudioDevice* additional_speaker =
      cras_audio_handler_->GetDeviceFromId(kUSBJabraSpeakerInput1->id);
  ASSERT_TRUE(additional_speaker);
  EXPECT_TRUE(additional_speaker->active);

  // Update active device list so previously primary active device is not
  // active anymore.
  cras_audio_handler_->ChangeActiveNodes(
      {kUSBJabraSpeakerInput1->id, kUSBJabraSpeakerInput2->id});

  // Verify that list of active devices is correctly set, and that a new primary
  // active input is selected.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerInput1->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

  additional_speaker =
      cras_audio_handler_->GetDeviceFromId(kUSBJabraSpeakerInput2->id);
  ASSERT_TRUE(additional_speaker);
  EXPECT_TRUE(additional_speaker->active);
}

TEST_P(CrasAudioHandlerTest, SetActiveNodesDeactivatePrimaryActiveNode) {
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kUSBJabraSpeakerInput1, kUSBJabraSpeakerInput2});
  AudioNode usb_camera = GenerateAudioNode(kUSBCameraInput);
  usb_camera.plugged_time = 10000000;
  audio_nodes.push_back(usb_camera);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the camera's input is selected as active input by CrasAudioHandler.
  EXPECT_EQ(1, GetActiveDeviceCount());
  EXPECT_EQ(kUSBCameraInput->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

  // Add another device to active input device list.
  cras_audio_handler_->SetActiveInputNodes(
      {kUSBJabraSpeakerInput1->id, kUSBCameraInput->id});

  // Verify active input devices are set as expected, with primary active input
  // staying the same.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBCameraInput->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

  const AudioDevice* additional_speaker =
      cras_audio_handler_->GetDeviceFromId(kUSBJabraSpeakerInput1->id);
  ASSERT_TRUE(additional_speaker);
  EXPECT_TRUE(additional_speaker->active);

  // Update active device list so previously primary active device is not
  // active anymore.
  cras_audio_handler_->SetActiveInputNodes(
      {kUSBJabraSpeakerInput1->id, kUSBJabraSpeakerInput2->id});

  // Verify that list of active devices is correctly set, and that a new primary
  // active input is selected.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerInput1->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

  additional_speaker =
      cras_audio_handler_->GetDeviceFromId(kUSBJabraSpeakerInput2->id);
  ASSERT_TRUE(additional_speaker);
  EXPECT_TRUE(additional_speaker->active);
}

TEST_P(CrasAudioHandlerTest,
       ChangeActiveNodesHotrodInitWithSingleJabraCameraPlugInLaterOldCall) {
  AudioNodeList audio_nodes = GenerateAudioNodeList(
      {kHDMIOutput, kUSBJabraSpeakerOutput1, kUSBJabraSpeakerInput1});
  AudioNode usb_camera = GenerateAudioNode(kUSBCameraInput);
  usb_camera.plugged_time = 10000000;
  audio_nodes.push_back(usb_camera);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the jabra speaker's output is selected as active output, and
  // camera's input is selected as active input by CrasAudioHandler
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerOutput1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBCameraInput->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

  // Simulate hotrod app call to set jabra input as active device with only
  // jabra input node in the active node list, which does not conform to the
  // new SetActiveDevices protocol, but just show we can still handle it if
  // this happens.
  cras_audio_handler_->ChangeActiveNodes({kUSBJabraSpeakerInput1->id});

  // Verify the jabra speaker's output is selected as active output, and
  // jabra's input is selected as active input.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerOutput1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBJabraSpeakerInput1->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());
}

TEST_P(CrasAudioHandlerTest,
       SetActiveNodesHotrodInitWithSingleJabraCameraPlugInLaterOldCall) {
  AudioNodeList audio_nodes = GenerateAudioNodeList(
      {kHDMIOutput, kUSBJabraSpeakerOutput1, kUSBJabraSpeakerInput1});
  AudioNode usb_camera = GenerateAudioNode(kUSBCameraInput);
  usb_camera.plugged_time = 10000000;
  audio_nodes.push_back(usb_camera);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the jabra speaker's output is selected as active output, and
  // camera's input is selected as active input by CrasAudioHandler
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerOutput1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBCameraInput->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

  // Simulate hotrod app call to set jabra input as active device with only
  // jabra input node in the active node list, which does not conform to the
  // new SetActiveDevices protocol, but just show we can still handle it if
  // this happens.
  cras_audio_handler_->SetActiveInputNodes({kUSBJabraSpeakerInput1->id});

  // Verify the jabra speaker's output is selected as active output, and
  // jabra's input is selected as active input.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerOutput1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBJabraSpeakerInput1->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());
}

TEST_P(CrasAudioHandlerTest,
       ChangeActiveNodesHotrodInitWithSingleJabraChangeOutput) {
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kHDMIOutput, kUSBJabraSpeakerOutput1,
                         kUSBJabraSpeakerInput1, kUSBCameraInput},
      /*expected_active_input_node=*/kUSBJabraSpeakerInput1,
      /*expected_active_output_node=*/kUSBJabraSpeakerOutput1,
      /*expected_has_alternative_input=*/true,
      /*expected_has_alternative_output=*/true);

  // Simulate hotrod app call SetActiveDevices to change active output
  // with only complete list of active nodes passed in, which is the new
  // way of hotrod app.
  cras_audio_handler_->ChangeActiveNodes(
      {kHDMIOutput->id, kUSBJabraSpeakerInput1->id});

  // Verify the jabra speaker's output is selected as active output, and
  // jabra's input is selected as active input.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kHDMIOutput->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBJabraSpeakerInput1->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());
}

TEST_P(CrasAudioHandlerTest,
       SetActiveNodesHotrodInitWithSingleJabraChangeOutput) {
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kHDMIOutput, kUSBJabraSpeakerOutput1,
                         kUSBJabraSpeakerInput1, kUSBCameraInput},
      /*expected_active_input_node=*/kUSBJabraSpeakerInput1,
      /*expected_active_output_node=*/kUSBJabraSpeakerOutput1,
      /*expected_has_alternative_input=*/true,
      /*expected_has_alternative_output=*/true);

  // Simulate hotrod app calling SetActiveDeviceLists to change active input
  // and output with complete list of active nodes passed in.
  cras_audio_handler_->SetActiveOutputNodes({kHDMIOutput->id});
  cras_audio_handler_->SetActiveInputNodes({kUSBJabraSpeakerInput1->id});

  // Verify the jabra speaker's output is selected as active output, and
  // jabra's input is selected as active input.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kHDMIOutput->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBJabraSpeakerInput1->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());
}

TEST_P(CrasAudioHandlerTest,
       ChangeActiveNodesHotrodInitWithSingleJabraChangeOutputOldCall) {
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kHDMIOutput, kUSBJabraSpeakerOutput1,
                         kUSBJabraSpeakerInput1, kUSBCameraInput},
      /*expected_active_input_node=*/kUSBJabraSpeakerInput1,
      /*expected_active_output_node=*/kUSBJabraSpeakerOutput1,
      /*expected_has_alternative_input=*/true,
      /*expected_has_alternative_output=*/true);

  // Simulate hotrod app call SetActiveDevices to change active output
  // with only a single active output nodes passed in, which is the old
  // way of hotrod app.
  cras_audio_handler_->ChangeActiveNodes({kHDMIOutput->id});

  // Verify the jabra speaker's output is selected as active output, and
  // jabra's input is selected as active input.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kHDMIOutput->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBJabraSpeakerInput1->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());
}

TEST_P(CrasAudioHandlerTest, SetEmptyActiveOutputNodes) {
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kHDMIOutput, kUSBJabraSpeakerOutput1,
                         kUSBJabraSpeakerInput1, kUSBCameraInput},
      /*expected_active_input_node=*/kUSBJabraSpeakerInput1,
      /*expected_active_output_node=*/kUSBJabraSpeakerOutput1,
      /*expected_has_alternative_input=*/true,
      /*expected_has_alternative_output=*/true);

  cras_audio_handler_->SetActiveOutputNodes(CrasAudioHandler::NodeIdList());

  // Verify the jabra's input is selected as active input, and that there are
  // no active outputs.
  EXPECT_EQ(1, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerInput1->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_EQ(0u, cras_audio_handler_->GetPrimaryActiveOutputNode());
}

TEST_P(CrasAudioHandlerTest, SetEmptyActiveInputNodes) {
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kHDMIOutput, kUSBJabraSpeakerOutput1,
                         kUSBJabraSpeakerInput1, kUSBCameraInput},
      /*expected_active_input_node=*/kUSBJabraSpeakerInput1,
      /*expected_active_output_node=*/kUSBJabraSpeakerOutput1,
      /*expected_has_alternative_input=*/true,
      /*expected_has_alternative_output=*/true);

  cras_audio_handler_->SetActiveInputNodes(CrasAudioHandler::NodeIdList());

  // Verify the jabra speaker's output is selected as active output, and
  // there are no active inputs..
  EXPECT_EQ(1, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerOutput1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(0u, cras_audio_handler_->GetPrimaryActiveInputNode());
}

TEST_P(CrasAudioHandlerTest, NoMoreAudioInputDevices) {
  // Some device like chromebox does not have the internal input device. The
  // active devices should be reset when the user plugs a device and then
  // unplugs it to such device.
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalSpeaker});
  SetUpCrasAudioHandler(audio_nodes);

  EXPECT_EQ(0ULL, cras_audio_handler_->GetPrimaryActiveInputNode());

  audio_nodes.push_back(GenerateAudioNode(kMicJack));
  ChangeAudioNodes(audio_nodes);

  EXPECT_EQ(kMicJack->id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_EQ(1, test_observer_->active_input_node_changed_count());
  test_observer_->reset_active_input_node_changed_count();

  audio_nodes.pop_back();
  ChangeAudioNodes(audio_nodes);
  EXPECT_EQ(0ULL, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_EQ(1, test_observer_->active_input_node_changed_count());
}

// Test the case hot plugging 35mm headphone and mic. Both 35mm headphone and
// and mic should always be selected as active output and input devices.
TEST_P(CrasAudioHandlerTest, HotPlug35mmHeadphoneAndMic) {
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kInternalMic},
      /*expected_active_input_node=*/kInternalMic,
      /*expected_active_output_node=*/kInternalSpeaker,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/false);

  // Hotplug the 35mm headset with both headphone and mic.
  AudioNodeList audio_nodes;
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);
  AudioNode headphone = GenerateAudioNode(kHeadphone);
  headphone.active = false;
  headphone.plugged_time = 50000000;
  audio_nodes.push_back(headphone);
  AudioNode internal_mic = GenerateAudioNode(kInternalMic);
  internal_mic.active = true;
  audio_nodes.push_back(internal_mic);
  AudioNode mic = GenerateAudioNode(kMicJack);
  mic.active = false;
  mic.plugged_time = 50000000;
  audio_nodes.push_back(mic);
  ChangeAudioNodes(audio_nodes);

  // Verify 35mm headphone is selected as active output.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(4u, audio_devices.size());
  EXPECT_EQ(headphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  // Verify 35mm mic is selected as active input.
  EXPECT_EQ(mic.id, cras_audio_handler_->GetPrimaryActiveInputNode());

  // Manually select internal speaker as active output.
  AudioDevice internal_output(internal_speaker);
  cras_audio_handler_->SwitchToDevice(internal_output, true,
                                      DeviceActivateType::kActivateByUser);
  // Manually select internal mic as active input.
  AudioDevice internal_input(internal_mic);
  cras_audio_handler_->SwitchToDevice(internal_input, true,
                                      DeviceActivateType::kActivateByUser);

  // Verify the active output is switched to internal speaker.
  EXPECT_EQ(internal_speaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_LT(internal_speaker.plugged_time, headphone.plugged_time);
  // Verify the active input is switched to internal mic.
  EXPECT_EQ(internal_mic.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_LT(internal_mic.plugged_time, mic.plugged_time);

  // Unplug 35mm headphone and mic.
  audio_nodes.clear();
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);
  internal_mic.active = true;
  audio_nodes.push_back(internal_mic);
  ChangeAudioNodes(audio_nodes);

  // Verify internal speaker remains as active output.
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(2u, audio_devices.size());
  EXPECT_EQ(internal_speaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  // Verify internal mic remains as active input.
  EXPECT_EQ(internal_mic.id, cras_audio_handler_->GetPrimaryActiveInputNode());

  // Hotplug 35mm headset again.
  headphone.active = false;
  headphone.plugged_time = 90000000;
  audio_nodes.push_back(headphone);
  mic.active = false;
  mic.plugged_time = 90000000;
  audio_nodes.push_back(mic);
  ChangeAudioNodes(audio_nodes);

  // Verify 35mm headphone is active again.
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(4u, audio_devices.size());
  EXPECT_EQ(headphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_LT(internal_speaker.plugged_time, headphone.plugged_time);
  // Verify 35mm mic is active again.
  EXPECT_EQ(mic.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_LT(internal_mic.plugged_time, mic.plugged_time);
}

// Test the case where 3.5mm headphone and mic will be activated automatically
// when being hot plugged, under the condition that the
// kAudioSelectionImprovement flag is on.
TEST_P(CrasAudioHandlerTest,
       HotPlug35mmHeadphoneAndMic_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kInternalMic},
      /*expected_active_input_node=*/kInternalMic,
      /*expected_active_output_node=*/kInternalSpeaker,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/false);

  // No exception rule metrics are recorded before plugging 3.5mm headset.
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kInputRule1HotPlugPrivilegedDevice,
      /*expected_count=*/0);
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kOutputRule1HotPlugPrivilegedDevice,
      /*expected_count=*/0);

  // Hotplug the 35mm headset with both headphone and mic.
  AudioNodeList audio_nodes;
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);
  AudioNode headphone = GenerateAudioNode(kHeadphone);
  headphone.active = false;
  headphone.plugged_time = 50000000;
  audio_nodes.push_back(headphone);
  AudioNode internal_mic = GenerateAudioNode(kInternalMic);
  internal_mic.active = true;
  audio_nodes.push_back(internal_mic);
  AudioNode mic = GenerateAudioNode(kMicJack);
  mic.active = false;
  mic.plugged_time = 50000000;
  audio_nodes.push_back(mic);
  ChangeAudioNodes(audio_nodes);

  // Verify 35mm headphone is selected as active output.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(4u, audio_devices.size());
  EXPECT_EQ(headphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  // Verify 35mm mic is selected as active input.
  EXPECT_EQ(mic.id, cras_audio_handler_->GetPrimaryActiveInputNode());

  // Exception rule metrics are recorded after plugging 3.5mm headset.
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kInputRule1HotPlugPrivilegedDevice,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kOutputRule1HotPlugPrivilegedDevice,
      /*expected_count=*/1);

  // Manually select internal speaker as active output.
  AudioDevice internal_output(internal_speaker);
  cras_audio_handler_->SwitchToDevice(internal_output, true,
                                      DeviceActivateType::kActivateByUser);
  // Manually select internal mic as active input.
  AudioDevice internal_input(internal_mic);
  cras_audio_handler_->SwitchToDevice(internal_input, true,
                                      DeviceActivateType::kActivateByUser);

  // Verify the active output is switched to internal speaker.
  EXPECT_EQ(internal_speaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_LT(internal_speaker.plugged_time, headphone.plugged_time);
  // Verify the active input is switched to internal mic.
  EXPECT_EQ(internal_mic.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_LT(internal_mic.plugged_time, mic.plugged_time);

  // Unplug 35mm headphone and mic.
  audio_nodes.clear();
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);
  internal_mic.active = true;
  audio_nodes.push_back(internal_mic);
  ChangeAudioNodes(audio_nodes);

  // Verify internal speaker remains as active output.
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(2u, audio_devices.size());
  EXPECT_EQ(internal_speaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  // Verify internal mic remains as active input.
  EXPECT_EQ(internal_mic.id, cras_audio_handler_->GetPrimaryActiveInputNode());

  // Hotplug 35mm headset again.
  headphone.active = false;
  headphone.plugged_time = 90000000;
  audio_nodes.push_back(headphone);
  mic.active = false;
  mic.plugged_time = 90000000;
  audio_nodes.push_back(mic);
  ChangeAudioNodes(audio_nodes);

  // Verify 35mm headphone is active again.
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(4u, audio_devices.size());
  EXPECT_EQ(headphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  // Make sure headphone is plugged after internal speaker so that headphone is
  // being hot plugged.
  EXPECT_LT(internal_speaker.plugged_time, headphone.plugged_time);
  // Verify 35mm mic is active again.
  EXPECT_EQ(mic.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_LT(internal_mic.plugged_time, mic.plugged_time);
}

// Test the case where bluetooth headphone and mic will be activated
// automatically when being hot plugged, under the condition that the
// kAudioSelectionImprovement flag is on.
TEST_P(CrasAudioHandlerTest,
       HotPlugBluetoothDevices_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kInternalMic},
      /*expected_active_input_node=*/kInternalMic,
      /*expected_active_output_node=*/kInternalSpeaker,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/false);

  // No exception rule metrics are recorded before plugging bluetooth device.
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kInputRule1HotPlugPrivilegedDevice,
      /*expected_count=*/0);
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kOutputRule1HotPlugPrivilegedDevice,
      /*expected_count=*/0);

  // Hotplug the bluetooth devices.
  AudioNodeList audio_nodes;
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);
  AudioNode bluetooth_output = GenerateAudioNode(kBluetoothHeadset);
  bluetooth_output.active = false;
  bluetooth_output.plugged_time = 50000000;
  audio_nodes.push_back(bluetooth_output);
  AudioNode internal_mic = GenerateAudioNode(kInternalMic);
  internal_mic.active = true;
  audio_nodes.push_back(internal_mic);
  AudioNode bluetooth_input = GenerateAudioNode(kBluetoothNbMic);
  bluetooth_input.active = false;
  bluetooth_input.plugged_time = 50000000;
  audio_nodes.push_back(bluetooth_input);
  ChangeAudioNodes(audio_nodes);

  // Verify bluetooth_output is selected as active output.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(4u, audio_devices.size());
  EXPECT_EQ(bluetooth_output.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  // Verify bluetooth_input is selected as active input.
  EXPECT_EQ(bluetooth_input.id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

  // Exception rule metrics are recorded after plugging bluetooth device.
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kInputRule1HotPlugPrivilegedDevice,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kOutputRule1HotPlugPrivilegedDevice,
      /*expected_count=*/1);

  // Manually select internal speaker as active output.
  AudioDevice internal_output(internal_speaker);
  cras_audio_handler_->SwitchToDevice(internal_output, true,
                                      DeviceActivateType::kActivateByUser);
  // Manually select internal mic as active input.
  AudioDevice internal_input(internal_mic);
  cras_audio_handler_->SwitchToDevice(internal_input, true,
                                      DeviceActivateType::kActivateByUser);

  // Verify the active output is switched to internal speaker.
  EXPECT_EQ(internal_speaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_LT(internal_speaker.plugged_time, bluetooth_output.plugged_time);
  // Verify the active input is switched to internal mic.
  EXPECT_EQ(internal_mic.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_LT(internal_mic.plugged_time, bluetooth_input.plugged_time);

  // Unplug bluetooth_output and bluetooth_input.
  audio_nodes.clear();
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);
  internal_mic.active = true;
  audio_nodes.push_back(internal_mic);
  ChangeAudioNodes(audio_nodes);

  // Verify internal speaker remains as active output.
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(2u, audio_devices.size());
  EXPECT_EQ(internal_speaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  // Verify internal mic remains as active input.
  EXPECT_EQ(internal_mic.id, cras_audio_handler_->GetPrimaryActiveInputNode());

  // Hotplug bluetooth devices again.
  bluetooth_output.active = false;
  bluetooth_output.plugged_time = 90000000;
  audio_nodes.push_back(bluetooth_output);
  bluetooth_input.active = false;
  bluetooth_input.plugged_time = 90000000;
  audio_nodes.push_back(bluetooth_input);
  ChangeAudioNodes(audio_nodes);

  // Verify bluetooth_output is active again.
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(4u, audio_devices.size());
  EXPECT_EQ(bluetooth_output.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_LT(internal_speaker.plugged_time, bluetooth_output.plugged_time);
  // Verify bluetooth_input is active again.
  EXPECT_EQ(bluetooth_input.id,
            cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_LT(internal_mic.plugged_time, bluetooth_input.plugged_time);
}

// Test the case where 3.5mm headphone will be activated automatically
// when being hot plugged, and exception rule #1 will be fired, under the
// condition that the kAudioSelectionImprovement flag is on.
TEST_P(CrasAudioHandlerTest,
       HotPlug35mmHeadphone_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kInternalSpeaker,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/false);

  // No exception rule metrics are recorded before plugging 3.5mm headset.
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kInputRule1HotPlugPrivilegedDevice,
      /*expected_count=*/0);
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kOutputRule1HotPlugPrivilegedDevice,
      /*expected_count=*/0);

  // Hotplug the 35mm headset with only headphone.
  AudioNodeList audio_nodes;
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);
  AudioNode headphone = GenerateAudioNode(kHeadphone);
  headphone.active = false;
  headphone.plugged_time = 50000000;
  audio_nodes.push_back(headphone);
  ChangeAudioNodes(audio_nodes);

  // Verify 35mm headphone is selected as active output.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(2u, audio_devices.size());
  EXPECT_EQ(headphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Exception rule metric for output is recorded after plugging 3.5mm
  // headphone.
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kInputRule1HotPlugPrivilegedDevice,
      /*expected_count=*/0);
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kOutputRule1HotPlugPrivilegedDevice,
      /*expected_count=*/1);
}

// Test the case where 3.5mm mic will be activated automatically
// when being hot plugged, and exception rule #1 will be fired, under the
// condition that the kAudioSelectionImprovement flag is on.
TEST_P(CrasAudioHandlerTest, HotPlug35mmMic_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalMic},
      /*expected_active_input_node=*/kInternalMic,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/false);

  // No exception rule metrics are recorded before plugging 3.5mm headset.
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kInputRule1HotPlugPrivilegedDevice,
      /*expected_count=*/0);
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kOutputRule1HotPlugPrivilegedDevice,
      /*expected_count=*/0);

  // Hotplug the 35mm headset with both only mic.
  AudioNodeList audio_nodes;
  AudioNode internal_mic = GenerateAudioNode(kInternalMic);
  internal_mic.active = true;
  audio_nodes.push_back(internal_mic);
  AudioNode mic = GenerateAudioNode(kMicJack);
  mic.active = false;
  mic.plugged_time = 50000000;
  audio_nodes.push_back(mic);
  ChangeAudioNodes(audio_nodes);

  // Verify 35mm mic is selected as active input.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(2u, audio_devices.size());
  EXPECT_EQ(mic.id, cras_audio_handler_->GetPrimaryActiveInputNode());

  // Exception rule metric for input is recorded after plugging 3.5mm mic.
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kInputRule1HotPlugPrivilegedDevice,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kOutputRule1HotPlugPrivilegedDevice,
      /*expected_count=*/0);
}

// Test the case in which an HDMI output is plugged in with other higher
// priority
// output devices already plugged and user has manually selected an active
// output.
TEST_P(CrasAudioHandlerTest, HotPlugHDMIChangeActiveOutput) {
  AudioNodeList audio_nodes;
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  audio_nodes.push_back(internal_speaker);
  AudioNode usb_headset = GenerateAudioNode(kUSBHeadphone1);
  usb_headset.plugged_time = 80000000;
  audio_nodes.push_back(usb_headset);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the USB headset is selected as active output by default.
  EXPECT_EQ(usb_headset.id, cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Manually set the active output to internal speaker.
  AudioDevice internal_output(GenerateAudioNode(kInternalSpeaker));
  cras_audio_handler_->SwitchToDevice(internal_output, true,
                                      DeviceActivateType::kActivateByUser);

  // Verify the active output is switched to internal speaker.
  EXPECT_EQ(internal_speaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_LT(internal_speaker.plugged_time, usb_headset.plugged_time);
  const AudioDevice* usb_device = GetDeviceFromId(usb_headset.id);
  EXPECT_FALSE(usb_device->active);

  // Plug in HDMI output.
  audio_nodes.clear();
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);
  usb_headset.active = false;
  audio_nodes.push_back(usb_headset);
  AudioNode hdmi = GenerateAudioNode(kHDMIOutput);
  hdmi.plugged_time = 90000000;
  audio_nodes.push_back(hdmi);
  ChangeAudioNodes(audio_nodes);

  // The active output change to hdmi as it has higher built-in priority than
  // the internal speaker.
  EXPECT_EQ(kHDMIOutputId, cras_audio_handler_->GetPrimaryActiveOutputNode());
}

TEST_P(CrasAudioHandlerTest,
       HotPlugHDMIChangeActiveOutput_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  AudioNodeList audio_nodes;
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  audio_nodes.push_back(internal_speaker);
  AudioNode usb_headset = GenerateAudioNode(kUSBHeadphone1);
  usb_headset.plugged_time = 80000000;
  audio_nodes.push_back(usb_headset);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the internal_speaker is selected since the device set was not seen
  // before based on system boots case.
  EXPECT_EQ(internal_speaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Plug in HDMI output.
  audio_nodes.clear();
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);
  usb_headset.active = false;
  audio_nodes.push_back(usb_headset);
  AudioNode hdmi = GenerateAudioNode(kHDMIOutput);
  hdmi.plugged_time = 90000000;
  audio_nodes.push_back(hdmi);
  ChangeAudioNodes(audio_nodes);

  // The active output does not change to hdmi since it's a new device unseen
  // before.
  EXPECT_NE(kHDMIOutputId, cras_audio_handler_->GetPrimaryActiveOutputNode());
}

// Test the case in which the active device was set to inactive from cras after
// resuming from suspension state. See crbug.com/478968.
TEST_P(CrasAudioHandlerTest, ActiveNodeLostAfterResume) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kHeadphone, kHDMIOutput});
  for (const auto& node : audio_nodes) {
    ASSERT_FALSE(node.active) << node.id << " expected to be inactive";
  }
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the headphone is selected as the active output.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_EQ(kHeadphone->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  const AudioDevice* active_headphone = GetDeviceFromId(kHeadphone->id);
  EXPECT_EQ(kHeadphone->id, active_headphone->id);
  EXPECT_TRUE(active_headphone->active);

  // Simulate NodesChanged signal with headphone turning into inactive state,
  // and HDMI node removed.
  audio_nodes.clear();
  audio_nodes.push_back(GenerateAudioNode(kHeadphone));
  ChangeAudioNodes(audio_nodes);

  // Verify the headphone is set to active again.
  EXPECT_EQ(kHeadphone->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  const AudioDevice* headphone_resumed = GetDeviceFromId(kHeadphone->id);
  EXPECT_EQ(kHeadphone->id, headphone_resumed->id);
  EXPECT_TRUE(headphone_resumed->active);
}

// In the mirror mode, when the device resumes after being suspended, the hmdi
// node will be lost first, then re-appear with a different node id, but with
// the same stable id. If it is set as the non-active node  by user before
// suspend/resume, it should remain inactive after the device resumes even
// if it has a higher priority than the current active node.
// crbug.com/443014.
TEST_P(CrasAudioHandlerTest, HDMIRemainInactiveAfterSuspendResume) {
  // Verify the hdmi is selected as the active output since it has a higher
  // priority.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kHDMIOutput},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kHDMIOutput,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/true);

  // Manually set the active output to internal speaker.
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  cras_audio_handler_->SwitchToDevice(AudioDevice(internal_speaker), true,
                                      DeviceActivateType::kActivateByUser);
  EXPECT_EQ(internal_speaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Simulate the suspend and resume of the device during mirror mode. The HDMI
  // node will be lost first.
  AudioNodeList audio_nodes;
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);
  ChangeAudioNodes(audio_nodes);

  // Verify the HDMI node is lost, and internal speaker is still the active
  // node.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(1u, audio_devices.size());
  EXPECT_EQ(internal_speaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Simulate the re-appearing of the hdmi node, which comes with a new id,
  // but the same stable device id.
  AudioNode hdmi_output = GenerateAudioNode(kHDMIOutput);
  AudioNode hdmi_output_2(hdmi_output);
  hdmi_output_2.id = 20006;
  hdmi_output_2.plugged_time = internal_speaker.plugged_time + 100;
  audio_nodes.push_back(hdmi_output_2);
  ASSERT_NE(hdmi_output.id, hdmi_output_2.id);
  ASSERT_EQ(hdmi_output.StableDeviceId(), hdmi_output_2.StableDeviceId());
  ChangeAudioNodes(audio_nodes);

  // Verify the hdmi node is not set the active, and the current active node
  // , the internal speaker, remains active.
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(2u, audio_devices.size());
  EXPECT_EQ(internal_speaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
}

// Test the case in which there are two NodesChanged signal for discovering
// output devices, and there is race between NodesChange and SetActiveOutput
// during this process. See crbug.com/478968.
TEST_P(CrasAudioHandlerTest, ActiveNodeLostDuringLoginSession) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kHeadphone});
  for (const auto& node : audio_nodes) {
    ASSERT_FALSE(node.active) << node.id << " expected to be inactive";
  }
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the headphone is selected as the active output.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_EQ(kHeadphone->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  const AudioDevice* active_headphone = GetDeviceFromId(kHeadphone->id);
  EXPECT_EQ(kHeadphone->id, active_headphone->id);
  EXPECT_TRUE(active_headphone->active);

  // Simulate NodesChanged signal with headphone turning into inactive state,
  // and add a new HDMI output node.
  audio_nodes.clear();
  audio_nodes.push_back(GenerateAudioNode(kHeadphone));
  audio_nodes.push_back(GenerateAudioNode(kHDMIOutput));
  ChangeAudioNodes(audio_nodes);

  // Verify the headphone is set to active again.
  EXPECT_EQ(kHeadphone->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  const AudioDevice* headphone_resumed = GetDeviceFromId(kHeadphone->id);
  EXPECT_EQ(kHeadphone->id, headphone_resumed->id);
  EXPECT_TRUE(headphone_resumed->active);
}

// This test HDMI output rediscovering case in crbug.com/503667.
TEST_P(CrasAudioHandlerTest, HDMIOutputRediscover) {
  // Verify the HDMI device has been selected as the active output, and audio
  // output is not muted.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kHDMIOutput},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kHDMIOutput,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/true);

  EXPECT_FALSE(cras_audio_handler_->IsOutputMuted());

  // Trigger HDMI rediscovering grace period, and remove the HDMI node.
  const int grace_period_in_ms = 200;
  SetHDMIRediscoverGracePeriodDuration(grace_period_in_ms);
  SetActiveHDMIRediscover();
  AudioNodeList audio_nodes_lost_hdmi =
      GenerateAudioNodeList({kInternalSpeaker});
  ChangeAudioNodes(audio_nodes_lost_hdmi);

  // Verify the active output is switched to internal speaker, it is not muted
  // by preference, but the system output is muted during the grace period.
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_FALSE(
      cras_audio_handler_->IsOutputMutedForDevice(kInternalSpeaker->id));
  EXPECT_TRUE(cras_audio_handler_->IsOutputMuted());

  // Re-attach the HDMI device after a little delay.
  HDMIRediscoverWaiter waiter(this, grace_period_in_ms);
  waiter.WaitUntilTimeOut(grace_period_in_ms / 4);
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kHDMIOutput});
  ChangeAudioNodes(audio_nodes);

  // After HDMI re-discover grace period, verify HDMI output is selected as the
  // active device and not muted.
  waiter.WaitUntilHDMIRediscoverGracePeriodEnd();
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHDMIOutput->id, active_output.id);
  EXPECT_EQ(kHDMIOutput->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_FALSE(cras_audio_handler_->IsOutputMuted());
}

// This tests the case of output unmuting event is not notified after the hdmi
// output re-discover grace period ends.
TEST_P(CrasAudioHandlerTest, HDMIOutputUnplugDuringSuspension) {
  // Verify the HDMI device has been selected as the active output, and audio
  // output is not muted.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kHDMIOutput},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kHDMIOutput,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/true);

  EXPECT_FALSE(cras_audio_handler_->IsOutputMuted());

  // Trigger HDMI rediscovering grace period, and remove the HDMI node.
  const int grace_period_in_ms = 200;
  SetHDMIRediscoverGracePeriodDuration(grace_period_in_ms);
  SetActiveHDMIRediscover();
  AudioNodeList audio_nodes_lost_hdmi =
      GenerateAudioNodeList({kInternalSpeaker});
  ChangeAudioNodes(audio_nodes_lost_hdmi);

  // Verify the active output is switched to internal speaker, it is not muted
  // by preference, but the system output is muted during the grace period.
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_FALSE(
      cras_audio_handler_->IsOutputMutedForDevice(kInternalSpeaker->id));
  EXPECT_TRUE(cras_audio_handler_->IsOutputMuted());

  // After HDMI re-discover grace period, verify internal speaker is still the
  // active output and not muted.
  test_observer_->reset_output_mute_changed_count();
  HDMIRediscoverWaiter waiter(this, grace_period_in_ms);
  waiter.WaitUntilHDMIRediscoverGracePeriodEnd();
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_FALSE(cras_audio_handler_->IsOutputMuted());
  EXPECT_EQ(0, test_observer_->output_mute_changed_count());
}

TEST_P(CrasAudioHandlerTest, FrontCameraStartStop) {
  // Verify the front mic has been selected as the active input.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kFrontMic, kRearMic},
      /*expected_active_input_node=*/kFrontMic,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/std::nullopt);

  EXPECT_TRUE(cras_audio_handler_->HasDualInternalMic());

  // Start the front facing camera.
  StartFrontFacingCamera();
  // Verify the front mic has been selected as the active input.
  EXPECT_EQ(kFrontMicId, cras_audio_handler_->GetPrimaryActiveInputNode());

  // Stop the front facing camera.
  StopFrontFacingCamera();
  // Verify the front mic has been selected as the active input.
  EXPECT_EQ(kFrontMicId, cras_audio_handler_->GetPrimaryActiveInputNode());
}

TEST_P(CrasAudioHandlerTest, RearCameraStartStop) {
  // Verify the front mic has been selected as the active input.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kFrontMic, kRearMic},
      /*expected_active_input_node=*/kFrontMic,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/std::nullopt);

  EXPECT_TRUE(cras_audio_handler_->HasDualInternalMic());

  // Start the rear facing camera.
  StartRearFacingCamera();
  // Verify the active input is switched to the rear mic.
  EXPECT_EQ(kRearMicId, cras_audio_handler_->GetPrimaryActiveInputNode());

  // Stop the rear facing camera.
  StopRearFacingCamera();
  // Verify the active input is switched back to front mic.
  EXPECT_EQ(kFrontMicId, cras_audio_handler_->GetPrimaryActiveInputNode());
}

TEST_P(CrasAudioHandlerTest, SwitchFrontRearCamera) {
  // Verify the front mic has been selected as the active input.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kFrontMic, kRearMic},
      /*expected_active_input_node=*/kFrontMic,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/std::nullopt);

  EXPECT_TRUE(cras_audio_handler_->HasDualInternalMic());

  // Start the front facing camera.
  StartFrontFacingCamera();
  // Verify the front mic has been selected as the active input.
  EXPECT_EQ(kFrontMicId, cras_audio_handler_->GetPrimaryActiveInputNode());

  // Simulates the camera app switching from front camera to rear camera.
  StopFrontFacingCamera();
  StartRearFacingCamera();

  // Verify the rear mic has been selected as the active input.
  EXPECT_EQ(kRearMicId, cras_audio_handler_->GetPrimaryActiveInputNode());
}

TEST_P(CrasAudioHandlerTest, StartFrontCameraWithActiveExternalInput) {
  // Verify the mic Jack has been selected as the active input.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kFrontMic, kRearMic, kMicJack},
      /*expected_active_input_node=*/kMicJack,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/true,
      /*expected_has_alternative_output=*/std::nullopt);

  EXPECT_TRUE(cras_audio_handler_->HasDualInternalMic());

  // Start the front facing camera.
  StartFrontFacingCamera();
  // Verify the mic Jack has been selected as the active input.
  EXPECT_EQ(kMicJackId, cras_audio_handler_->GetPrimaryActiveInputNode());

  // Stop the front facing camera.
  StopFrontFacingCamera();
  // Verify the mic Jack remains as the active input.
  EXPECT_EQ(kMicJackId, cras_audio_handler_->GetPrimaryActiveInputNode());
}

TEST_P(CrasAudioHandlerTest, StartFrontCameraWithInactiveExternalInput) {
  // Verify the mic Jack has been selected as the active input.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kFrontMic, kRearMic, kMicJack},
      /*expected_active_input_node=*/kMicJack,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/true,
      /*expected_has_alternative_output=*/std::nullopt);

  EXPECT_TRUE(cras_audio_handler_->HasDualInternalMic());

  // Change the active input to internal mic.
  cras_audio_handler_->SwitchToFrontOrRearMic();
  // Verify the active input has been switched to front mic.
  EXPECT_EQ(kFrontMicId, cras_audio_handler_->GetPrimaryActiveInputNode());

  // Start the front facing camera.
  StartFrontFacingCamera();
  // Verify the front mic has been selected as the active input.
  EXPECT_EQ(kFrontMicId, cras_audio_handler_->GetPrimaryActiveInputNode());

  // Stop the front facing camera.
  StopFrontFacingCamera();
  // Verify the active input remains as front mic.
  EXPECT_EQ(kFrontMicId, cras_audio_handler_->GetPrimaryActiveInputNode());
}

TEST_P(CrasAudioHandlerTest, StartFrontCameraWithoutDualMic) {
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalMic},
      /*expected_active_input_node=*/kInternalMic,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/std::nullopt);

  EXPECT_FALSE(cras_audio_handler_->HasDualInternalMic());

  // Start the front facing camera.
  StartFrontFacingCamera();
  // Verify the internal mic has been selected as the active input.
  EXPECT_EQ(kInternalMicId, cras_audio_handler_->GetPrimaryActiveInputNode());

  // Stop the front facing camera.
  StopFrontFacingCamera();
  // Verify the active input remains as interanl mic.
  EXPECT_EQ(kInternalMicId, cras_audio_handler_->GetPrimaryActiveInputNode());
}

TEST_P(CrasAudioHandlerTest, FrontRearCameraBothOn) {
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kFrontMic, kRearMic},
      /*expected_active_input_node=*/kFrontMic,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/std::nullopt);

  EXPECT_TRUE(cras_audio_handler_->HasDualInternalMic());

  // Start the rear facing camera.
  StartRearFacingCamera();
  // Verify the rear mic has been selected as the active input.
  EXPECT_EQ(kRearMicId, cras_audio_handler_->GetPrimaryActiveInputNode());

  // Start front camera without stopping the front camera.
  StartFrontFacingCamera();
  // Verify the active microphone does not change.
  EXPECT_EQ(kRearMicId, cras_audio_handler_->GetPrimaryActiveInputNode());

  // Stop the front mic.
  StopFrontFacingCamera();
  // Verity the active mic does not change when there is still camera on.
  EXPECT_EQ(kRearMicId, cras_audio_handler_->GetPrimaryActiveInputNode());

  // Stop the rear mic.
  StopRearFacingCamera();
  // Verify the actice mic changes to front mic after both cameras stop.
  EXPECT_EQ(kFrontMicId, cras_audio_handler_->GetPrimaryActiveInputNode());
}

TEST_P(CrasAudioHandlerTest,
       PlugUSBMicWhichIsInactiveInPrefsWithAnAlreadyActiveUSBMic) {
  // Set up initial audio devices.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kUSBMic1, kInternalMic},
      /*expected_active_input_node=*/kUSBMic1,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/true,
      /*expected_has_alternative_output=*/std::nullopt);

  // Plug the second USB mic.
  AudioNodeList audio_nodes;
  AudioNode internal_mic(GenerateAudioNode(kInternalMic));
  AudioNode usb_mic1(GenerateAudioNode(kUSBMic1));
  usb_mic1.active = true;
  usb_mic1.plugged_time = 1000;
  AudioNode usb_mic2 = GenerateAudioNode(kUSBMic2);
  audio_pref_handler_->SetDeviceActive(AudioDevice(usb_mic2), false, false);
  usb_mic2.active = false;
  usb_mic2.plugged_time = 2000;
  audio_nodes.push_back(internal_mic);
  audio_nodes.push_back(usb_mic1);
  audio_nodes.push_back(usb_mic2);
  ChangeAudioNodes(audio_nodes);

  // Verify that system monitor is notified, since we must update the cache of
  // enumerated devices even if the active device isn't changed.
  VerifySystemMonitorWasCalled();

  // Verify the AudioNodesChanged event is fired.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(3u, audio_devices.size());

  // Verify the active input device is changed to usb mic 2.
  EXPECT_EQ(1, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kUSBMicId2, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());
}

TEST_P(
    CrasAudioHandlerTest,
    PlugUSBMicWhichIsInactiveInPrefsWithAnAlreadyActiveUSBMic_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  // Set up initial audio devices.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kUSBMic1, kInternalMic},
      /*expected_active_input_node=*/kInternalMic,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/true,
      /*expected_has_alternative_output=*/std::nullopt);

  // Plug the second USB mic.
  AudioNodeList audio_nodes;
  AudioNode internal_mic(GenerateAudioNode(kInternalMic));
  internal_mic.active = true;
  AudioNode usb_mic1(GenerateAudioNode(kUSBMic1));
  usb_mic1.plugged_time = 1000;
  AudioNode usb_mic2 = GenerateAudioNode(kUSBMic2);
  audio_pref_handler_->SetDeviceActive(AudioDevice(usb_mic2), false, false);
  usb_mic2.active = false;
  usb_mic2.plugged_time = 2000;
  audio_nodes.push_back(internal_mic);
  audio_nodes.push_back(usb_mic1);
  audio_nodes.push_back(usb_mic2);
  ChangeAudioNodes(audio_nodes);

  // Verify that system monitor is notified, since we must update the cache of
  // enumerated devices even if the active device isn't changed.
  VerifySystemMonitorWasCalled();

  // Verify the AudioNodesChanged event is fired.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(3u, audio_devices.size());

  // Verify the active input device is not changed to usb mic 2.
  EXPECT_EQ(0, test_observer_->active_input_node_changed_count());
  EXPECT_NE(kUSBMicId2, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());
}

TEST_P(CrasAudioHandlerTest, PlugInUSBHeadphoneAfterLastUnplugNotActive) {
  // Set up initial audio devices.
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kHeadphone, kUSBHeadphone1});
  SetUpCrasAudioHandler(audio_nodes);

  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(3u, audio_devices.size());
  // 35mm Headphone is active, but USB headphone is not.
  EXPECT_EQ(kHeadphone->id, cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Unplug both 35mm headphone and USB headphone.
  audio_nodes.clear();
  audio_nodes.push_back(GenerateAudioNode(kInternalSpeaker));
  ChangeAudioNodes(audio_nodes);
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(1u, audio_devices.size());
  // Internal speaker is active.
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Plug in USB headphone.
  audio_nodes.clear();
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);
  AudioNode usb_headphone = GenerateAudioNode(kUSBHeadphone1);
  usb_headphone.plugged_time = 80000000;
  audio_nodes.push_back(usb_headphone);
  ChangeAudioNodes(audio_nodes);
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(2u, audio_devices.size());
  // USB headphone is active.
  EXPECT_EQ(kUSBHeadphone1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
}

TEST_P(
    CrasAudioHandlerTest,
    PlugInUSBHeadphoneAfterLastUnplugNotActive_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  // Set up initial audio devices.
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kHeadphone, kUSBHeadphone1});
  SetUpCrasAudioHandler(audio_nodes);

  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(3u, audio_devices.size());
  // 35mm Headphone is active, but USB headphone is not.
  EXPECT_EQ(kHeadphone->id, cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Unplug both 35mm headphone and USB headphone.
  audio_nodes.clear();
  audio_nodes.push_back(GenerateAudioNode(kInternalSpeaker));
  ChangeAudioNodes(audio_nodes);

  // Internal speaker is active.
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(1u, audio_devices.size());
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Plug in USB headphone.
  audio_nodes.clear();
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);
  AudioNode usb_headphone = GenerateAudioNode(kUSBHeadphone1);
  usb_headphone.plugged_time = 80000000;
  audio_nodes.push_back(usb_headphone);
  ChangeAudioNodes(audio_nodes);

  // USB headphone is not active since the set of USB headphone and internal
  // speaker is not seen before.
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(2u, audio_devices.size());
  EXPECT_NE(kUSBHeadphone1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
}

TEST_P(CrasAudioHandlerTest, SuspendAllSessionsForOutput) {
  // Set up initial output audio devices, with internal speaker, headphone and
  // USBHeadphone.
  // Verify the headphone has been selected as the active output.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kHeadphone, kUSBHeadphone1},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kHeadphone,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/true);

  // Unplug USBHeadphone should not suspend media sessions.
  EXPECT_CALL(*fake_manager_.get(), SuspendAllSessions).Times(0);
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kHeadphone});
  ChangeAudioNodes(audio_nodes);

  // Unplug active output device should suspend all media sessions.
  EXPECT_CALL(*fake_manager_.get(), SuspendAllSessions).Times(1);
  audio_nodes.clear();
  audio_nodes.push_back(GenerateAudioNode(kInternalSpeaker));
  ChangeAudioNodes(audio_nodes);
}

TEST_P(CrasAudioHandlerTest, SuspendAllSessionsForInput) {
  // Set up initial input audio devices, with internal mic and mic jack.
  // Verify the mic jack has been selected as the active input.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalMic, kMicJack},
      /*expected_active_input_node=*/kMicJack,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/true,
      /*expected_has_alternative_output=*/std::nullopt);

  // Unplug active input device should not suspend media sessions.
  EXPECT_CALL(*fake_manager_.get(), SuspendAllSessions).Times(0);
  AudioNodeList audio_nodes;
  audio_nodes.push_back(GenerateAudioNode(kInternalMic));
  ChangeAudioNodes(audio_nodes);
}

TEST_P(CrasAudioHandlerTest, MicrophoneMuteHwSwitchMutesInput) {
  // Set up initial input audio devices, with internal mic and mic jack.
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalMic, kMicJack});
  SetUpCrasAudioHandler(audio_nodes);

  // Simulate hw microphone mute switch toggle.
  ui::MicrophoneMuteSwitchMonitor::Get()->SetMicrophoneMuteSwitchValue(
      /*muted=*/true);

  // Verify the input is muted, OnInputMuteChanged event is fired.
  EXPECT_TRUE(cras_audio_handler_->IsInputMuted());
  EXPECT_EQ(1, test_observer_->input_mute_changed_count());

  // Unmuting input while the hw mute switch is on should fail.
  cras_audio_handler_->SetInputMute(
      false, CrasAudioHandler::InputMuteChangeMethod::kOther);

  EXPECT_TRUE(cras_audio_handler_->IsInputMuted());
  EXPECT_EQ(1, test_observer_->input_mute_changed_count());

  // Verify the input is unmuted if the hw mute switch is toggled again.
  ui::MicrophoneMuteSwitchMonitor::Get()->SetMicrophoneMuteSwitchValue(
      /*muted=*/false);
  EXPECT_FALSE(cras_audio_handler_->IsInputMuted());
  EXPECT_EQ(2, test_observer_->input_mute_changed_count());
}

TEST_P(CrasAudioHandlerTest, MicrophoneMuteSwitchToggledBeforeHandlerSetup) {
  // Simulate hw microphone mute switch toggle.
  ui::MicrophoneMuteSwitchMonitor::Get()->SetMicrophoneMuteSwitchValue(
      /*muted=*/true);

  // Set up initial input audio devices, with internal mic and mic jack.
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalMic, kMicJack});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the input is muted, OnInputMuteChanged event is fired.
  EXPECT_TRUE(cras_audio_handler_->IsInputMuted());
  EXPECT_EQ(0, test_observer_->input_mute_changed_count());

  // Unmuting input while the hw mute switch is on should fail.
  cras_audio_handler_->SetInputMute(
      false, CrasAudioHandler::InputMuteChangeMethod::kOther);

  EXPECT_TRUE(cras_audio_handler_->IsInputMuted());
  EXPECT_EQ(0, test_observer_->input_mute_changed_count());

  // Verify the input is unmuted if the hw mute switch is toggled again.
  ui::MicrophoneMuteSwitchMonitor::Get()->SetMicrophoneMuteSwitchValue(
      /*muted=*/false);
  EXPECT_FALSE(cras_audio_handler_->IsInputMuted());
  EXPECT_EQ(1, test_observer_->input_mute_changed_count());
}

TEST_P(CrasAudioHandlerTest, HasActiveInputDeviceForSimpleUsage) {
  using TestCaseWithDescription =
      std::map<std::vector<const AudioNodeInfo*>, const char*>;

  const TestCaseWithDescription positive_cases{
      {{kMicJack}, "A simple input node connected."},
      {{kMicJack, kInternalSpeaker},
       "A simple input node and a simple output node connected."},
      {{kKeyboardMic, kMicJack},
       "A non simple input node and a simple input node connected."},
      {{kInternalSpeaker, kOther, kMicJack, kKeyboardMic},
       "All types of audio nodes connected."}};
  const TestCaseWithDescription negative_cases{
      {{kInternalSpeaker}, "A simple output node connected."},
      {{kKeyboardMic}, "A non simple input node connected."},
      {{kInternalSpeaker, kKeyboardMic},
       "A simple output node and a non simple input node connected."},
      {{kOther}, "A non simple output node connected."}};

  ASSERT_FALSE(AudioDevice(GenerateAudioNode(kOther)).is_for_simple_usage());

  // Set up audio handler with empty audio_nodes.
  AudioNodeList audio_nodes;
  SetUpCrasAudioHandler(audio_nodes);

  EXPECT_FALSE(cras_audio_handler_->HasActiveInputDeviceForSimpleUsage())
      << "No audio nodes connected.";

  for (const auto& [audio_node_infos, description] : positive_cases) {
    audio_nodes = GenerateAudioNodeList(audio_node_infos);
    ChangeAudioNodes(audio_nodes);
    EXPECT_TRUE(cras_audio_handler_->HasActiveInputDeviceForSimpleUsage())
        << description;
  }

  for (const auto& [audio_node_infos, description] : negative_cases) {
    audio_nodes = GenerateAudioNodeList(audio_node_infos);
    ChangeAudioNodes(audio_nodes);
    EXPECT_FALSE(cras_audio_handler_->HasActiveInputDeviceForSimpleUsage())
        << description;
  }
}

TEST_P(CrasAudioHandlerTest, ShouldBeForcefullyMutedByAudioPolicy) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalSpeaker});
  SetUpCrasAudioHandler(audio_nodes);

  for (bool previous_value : {true, false}) {
    cras_audio_handler_->SetOutputMute(previous_value);

    audio_pref_handler_->SetAudioOutputAllowedValue(false);
    EXPECT_TRUE(cras_audio_handler_->IsOutputMutedByPolicy());
    EXPECT_TRUE(cras_audio_handler_->IsOutputMuted());

    audio_pref_handler_->SetAudioOutputAllowedValue(true);
    EXPECT_FALSE(cras_audio_handler_->IsOutputMutedByPolicy());
    EXPECT_EQ(cras_audio_handler_->IsOutputMuted(), previous_value);
  }
}

TEST_P(CrasAudioHandlerTest, ShouldBeForcefullyMutedBySecurityCurtainMode) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalSpeaker});
  SetUpCrasAudioHandler(audio_nodes);

  for (bool previous_value : {true, false}) {
    cras_audio_handler_->SetOutputMute(previous_value);

    cras_audio_handler_->SetOutputMuteLockedBySecurityCurtain(true);
    EXPECT_TRUE(cras_audio_handler_->IsOutputMutedBySecurityCurtain());
    EXPECT_TRUE(cras_audio_handler_->IsOutputMuted());

    cras_audio_handler_->SetOutputMuteLockedBySecurityCurtain(false);
    EXPECT_FALSE(cras_audio_handler_->IsOutputMutedBySecurityCurtain());
    EXPECT_EQ(cras_audio_handler_->IsOutputMuted(), previous_value);
  }
}

TEST_P(CrasAudioHandlerTest,
       ShouldNotBreakPolicyMutingByDisablingSecurityCurtain) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalSpeaker});
  SetUpCrasAudioHandler(audio_nodes);
  cras_audio_handler_->SetOutputMute(false);

  // Forced mute through a policy
  audio_pref_handler_->SetAudioOutputAllowedValue(false);

  // Then enable and disable forced mute through the security curtain.
  cras_audio_handler_->SetOutputMuteLockedBySecurityCurtain(true);
  cras_audio_handler_->SetOutputMuteLockedBySecurityCurtain(false);

  // The force mute through the policy should still be in effect.
  EXPECT_TRUE(cras_audio_handler_->IsOutputMuted());
  EXPECT_TRUE(cras_audio_handler_->IsOutputMutedByPolicy());
}

TEST_P(CrasAudioHandlerTest,
       ShouldNotBreakSecurityCurtainMutingByAudioPolicyChange) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalSpeaker});
  SetUpCrasAudioHandler(audio_nodes);
  cras_audio_handler_->SetOutputMute(false);

  // Forced mute by security curtain
  cras_audio_handler_->SetOutputMuteLockedBySecurityCurtain(true);

  // Then enable and disable mute through audio policy
  audio_pref_handler_->SetAudioOutputAllowedValue(false);
  audio_pref_handler_->SetAudioOutputAllowedValue(true);

  // The force mute through the policy should still be in effect.
  EXPECT_TRUE(cras_audio_handler_->IsOutputMuted());
  EXPECT_TRUE(cras_audio_handler_->IsOutputMutedBySecurityCurtain());
}

TEST_P(CrasAudioHandlerTest, MicrophoneMuteKeyboardSwitchTest) {
  // Set up initial input audio devices, with internal mic and mic jack.
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalMic, kMicJack});
  SetUpCrasAudioHandler(audio_nodes);

  // Setting the hw toggle to off first.
  ui::MicrophoneMuteSwitchMonitor::Get()->SetMicrophoneMuteSwitchValue(
      /*switch_on=*/false);
  EXPECT_FALSE(cras_audio_handler_->IsInputMuted());

  int input_mute_changed_counter = test_observer_->input_mute_changed_count();

  // This is similar to pressing the keyboard button for toggling the microphone
  // mute state.
  cras_audio_handler_->SetInputMute(
      !cras_audio_handler_->IsInputMuted(),
      CrasAudioHandler::InputMuteChangeMethod::kKeyboardButton);

  // Verify the input is muted, OnInputMuteChanged event is fired.
  EXPECT_TRUE(cras_audio_handler_->IsInputMuted());
  EXPECT_EQ(++input_mute_changed_counter,
            test_observer_->input_mute_changed_count());

  // Setting the hw toggle on. Pressing the keyboard switch should not be able
  // to change the system input mute state as long as the hw switch is on.
  ui::MicrophoneMuteSwitchMonitor::Get()->SetMicrophoneMuteSwitchValue(
      /*switch_on=*/true);

  // Input should still be muted as it was before toggling the hw switch.
  // OnInputMuteChanged event should not be fired.
  EXPECT_TRUE(cras_audio_handler_->IsInputMuted());
  EXPECT_EQ(input_mute_changed_counter,
            test_observer_->input_mute_changed_count());

  // Trying to toggle system input mute state using the keyboard switch.
  cras_audio_handler_->SetInputMute(
      !cras_audio_handler_->IsInputMuted(),
      CrasAudioHandler::InputMuteChangeMethod::kKeyboardButton);

  // Input should still be muted. OnInputMuteChanged event should not be fired.
  EXPECT_TRUE(cras_audio_handler_->IsInputMuted());
  EXPECT_EQ(input_mute_changed_counter,
            test_observer_->input_mute_changed_count());
}

TEST_P(CrasAudioHandlerTest,
       InputShouldBeForcefullyMutedBySecurityCurtainMode) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalMic, kMicJack});
  SetUpCrasAudioHandler(audio_nodes);

  for (bool previous_value : {true, false}) {
    cras_audio_handler_->SetInputMute(
        previous_value,
        CrasAudioHandler::InputMuteChangeMethod::kKeyboardButton);

    // Force enable input muting.
    cras_audio_handler_->SetInputMuteLockedBySecurityCurtain(true);
    EXPECT_TRUE(cras_audio_handler_->IsInputMutedBySecurityCurtain());
    EXPECT_TRUE(cras_audio_handler_->IsInputMuted());

    // Trying to unmute should now be blocked.
    cras_audio_handler_->SetInputMute(
        false, CrasAudioHandler::InputMuteChangeMethod::kKeyboardButton);
    EXPECT_TRUE(cras_audio_handler_->IsInputMutedBySecurityCurtain());
    EXPECT_TRUE(cras_audio_handler_->IsInputMuted());

    // Stop force enabling input muting - the device should go to unmuted state.
    cras_audio_handler_->SetInputMuteLockedBySecurityCurtain(false);
    EXPECT_FALSE(cras_audio_handler_->IsInputMutedBySecurityCurtain());
    EXPECT_FALSE(cras_audio_handler_->IsInputMuted());
  }
}

TEST_P(CrasAudioHandlerTest,
       HwSwitchInputMutingShouldNotBeBrokenBySecurityCurtain) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalMic, kMicJack});
  SetUpCrasAudioHandler(audio_nodes);

  // Simulate hw microphone mute switch toggle.
  ui::MicrophoneMuteSwitchMonitor::Get()->SetMicrophoneMuteSwitchValue(true);

  // Now enabling and disabling security input muting should not unmute the
  // device

  cras_audio_handler_->SetInputMuteLockedBySecurityCurtain(true);
  EXPECT_TRUE(cras_audio_handler_->IsInputMuted());

  cras_audio_handler_->SetInputMuteLockedBySecurityCurtain(false);
  EXPECT_TRUE(cras_audio_handler_->IsInputMuted());
}

TEST_P(CrasAudioHandlerTest,
       SecurityCurtainInputMutingShouldNotBeBrokenByHwSwitch) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalMic, kMicJack});
  SetUpCrasAudioHandler(audio_nodes);

  // Force enable input muting through security curtain
  cras_audio_handler_->SetInputMuteLockedBySecurityCurtain(true);

  // Now toggling the hw microphone mute switch should not unmute the system.
  ui::MicrophoneMuteSwitchMonitor::Get()->SetMicrophoneMuteSwitchValue(true);
  EXPECT_TRUE(cras_audio_handler_->IsInputMuted());

  ui::MicrophoneMuteSwitchMonitor::Get()->SetMicrophoneMuteSwitchValue(false);
  EXPECT_TRUE(cras_audio_handler_->IsInputMuted());
}

TEST_P(CrasAudioHandlerTest, IsInputMutedBySecurityCurtainChangeObserver) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalMic, kMicJack});
  SetUpCrasAudioHandler(audio_nodes);

  EXPECT_EQ(0, test_observer_->input_muted_by_security_curtain_changed_count());

  cras_audio_handler_->SetInputMuteLockedBySecurityCurtain(true);
  EXPECT_EQ(1, test_observer_->input_muted_by_security_curtain_changed_count());

  // Security curtain is already on. Trying to turn it on again should be no-op.
  cras_audio_handler_->SetInputMuteLockedBySecurityCurtain(true);
  EXPECT_EQ(1, test_observer_->input_muted_by_security_curtain_changed_count());

  cras_audio_handler_->SetInputMuteLockedBySecurityCurtain(false);
  EXPECT_EQ(2, test_observer_->input_muted_by_security_curtain_changed_count());
}

TEST_P(CrasAudioHandlerTest, IsNoiseCancellationSupportedForDeviceNoNC) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({});
  // Set up initial audio devices, only with internal mic.
  AudioNode internalMic = GenerateAudioNode(kInternalMic);
  // Clear the audio effect, no Noise Cancellation supported.
  internalMic.audio_effect = 0u;
  audio_nodes.push_back(internalMic);
  // Disable noise cancellation for board.
  SetUpCrasAudioHandlerWithPrimaryActiveNodeAndNoiseCancellationState(
      audio_nodes, /*primary_active_node=*/audio_nodes[0],
      /*noise_cancellation_enabled=*/false);

  EXPECT_FALSE(cras_audio_handler_->IsNoiseCancellationSupportedForDevice(
      kInternalMicId));
}

TEST_P(CrasAudioHandlerTest, IsNoiseCancellationSupportedForDeviceWithNC) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({});
  AudioNode internalMic = GenerateAudioNode(kInternalMic);
  // Set the audio effect, Noise Cancellation supported.
  internalMic.audio_effect = cras::EFFECT_TYPE_NOISE_CANCELLATION;
  audio_nodes.push_back(internalMic);
  AudioNode micJack = GenerateAudioNode(kMicJack);
  // Clear the audio effect, no Noise Cancellation supported.
  micJack.audio_effect = 0u;
  audio_nodes.push_back(micJack);
  AudioNode internalSpeaker = GenerateAudioNode(kInternalSpeaker);
  // Force audio_effect value to verify output nodes always return false.
  internalSpeaker.audio_effect = cras::EFFECT_TYPE_NOISE_CANCELLATION;
  audio_nodes.push_back(internalSpeaker);

  // Enable noise cancellation for board.
  SetUpCrasAudioHandlerWithPrimaryActiveNodeAndNoiseCancellationState(
      audio_nodes, /*primary_active_node=*/audio_nodes[0],
      /*noise_cancellation_enabled=*/true);

  EXPECT_TRUE(cras_audio_handler_->IsNoiseCancellationSupportedForDevice(
      kInternalMicId));
  EXPECT_FALSE(
      cras_audio_handler_->IsNoiseCancellationSupportedForDevice(kMicJackId));
  EXPECT_FALSE(cras_audio_handler_->IsNoiseCancellationSupportedForDevice(
      kInternalSpeakerId));
}

TEST_P(CrasAudioHandlerTest,
       SetNoiseCancellationStateUpdatesAudioPrefAndClient) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({});
  AudioNode internalMic = GenerateAudioNode(kInternalMic);
  // Set the audio effect, Noise Cancellation supported.
  internalMic.audio_effect = cras::EFFECT_TYPE_NOISE_CANCELLATION;
  audio_nodes.push_back(internalMic);

  // Enable noise cancellation for board.
  SetUpCrasAudioHandlerWithPrimaryActiveNodeAndNoiseCancellationState(
      audio_nodes, /*primary_active_node=*/audio_nodes[0],
      /*noise_cancellation_enabled=*/true);

  EXPECT_TRUE(audio_pref_handler_->GetNoiseCancellationState());
  EXPECT_TRUE(fake_cras_audio_client()->noise_cancellation_enabled());

  // Turn off noise cancellation.
  cras_audio_handler_->SetNoiseCancellationState(
      /*noise_cancellation_on=*/false,
      CrasAudioHandler::AudioSettingsChangeSource::kSystemTray);

  EXPECT_FALSE(audio_pref_handler_->GetNoiseCancellationState());
  EXPECT_FALSE(fake_cras_audio_client()->noise_cancellation_enabled());
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kNoiseCancellationEnabledSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kSystemTray, 1);

  // Turn on noise cancellation.
  cras_audio_handler_->SetNoiseCancellationState(
      /*noise_cancellation_on=*/true,
      CrasAudioHandler::AudioSettingsChangeSource::kSystemTray);

  EXPECT_TRUE(audio_pref_handler_->GetNoiseCancellationState());
  EXPECT_TRUE(fake_cras_audio_client()->noise_cancellation_enabled());
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kNoiseCancellationEnabledSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kSystemTray, 2);
}

TEST_P(CrasAudioHandlerTest, SetNoiseCancellationStateObserver) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({});
  AudioNode internalMic = GenerateAudioNode(kInternalMic);
  // Set the audio effect, Noise Cancellation supported.
  internalMic.audio_effect = cras::EFFECT_TYPE_NOISE_CANCELLATION;
  audio_nodes.push_back(internalMic);

  // Enable noise cancellation for board.
  SetUpCrasAudioHandlerWithPrimaryActiveNodeAndNoiseCancellationState(
      audio_nodes, /*primary_active_node=*/audio_nodes[0],
      /*noise_cancellation_enabled=*/true);

  EXPECT_EQ(0, test_observer_->noise_cancellation_state_change_count());

  // Change noise cancellation state to trigger observer.
  cras_audio_handler_->SetNoiseCancellationState(
      false, CrasAudioHandler::AudioSettingsChangeSource::kSystemTray);

  EXPECT_EQ(1, test_observer_->noise_cancellation_state_change_count());
}

TEST_P(CrasAudioHandlerTest, IsStyleTransferSupportedForDevice) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({});

  AudioNode internalMic = GenerateAudioNode(kInternalMic);
  internalMic.audio_effect = cras::EFFECT_TYPE_STYLE_TRANSFER;
  audio_nodes.push_back(internalMic);

  AudioNode micJack = GenerateAudioNode(kMicJack);
  micJack.audio_effect = 0u;  // no style transfer supported.
  audio_nodes.push_back(micJack);

  SetUpCrasAudioHandlerWithPrimaryActiveNodeAndStyleTransferState(
      audio_nodes, /*primary_active_node=*/audio_nodes[0],
      /*style_transfer_enabled=*/true);

  EXPECT_TRUE(
      cras_audio_handler_->IsStyleTransferSupportedForDevice(kInternalMicId));
  EXPECT_FALSE(
      cras_audio_handler_->IsStyleTransferSupportedForDevice(kMicJackId));
}

TEST_P(CrasAudioHandlerTest, SetStyleTransferStateUpdatesAudioPrefAndClient) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({});

  AudioNode internalMic = GenerateAudioNode(kInternalMic);
  internalMic.audio_effect = cras::EFFECT_TYPE_STYLE_TRANSFER;
  audio_nodes.push_back(internalMic);

  // On.
  SetUpCrasAudioHandlerWithPrimaryActiveNodeAndStyleTransferState(
      audio_nodes, /*primary_active_node=*/audio_nodes[0],
      /*style_transfer_enabled=*/true);

  EXPECT_TRUE(audio_pref_handler_->GetStyleTransferState());
  EXPECT_TRUE(fake_cras_audio_client()->style_transfer_enabled());

  // Off.
  cras_audio_handler_->SetStyleTransferState(/*style_transfer_on=*/false);

  EXPECT_FALSE(audio_pref_handler_->GetStyleTransferState());
  EXPECT_FALSE(fake_cras_audio_client()->style_transfer_enabled());

  // On.
  cras_audio_handler_->SetStyleTransferState(/*style_transfer_on=*/true);

  EXPECT_TRUE(audio_pref_handler_->GetStyleTransferState());
  EXPECT_TRUE(fake_cras_audio_client()->style_transfer_enabled());
}

TEST_P(CrasAudioHandlerTest, SetStyleTransferStateObserver) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({});

  AudioNode internalMic = GenerateAudioNode(kInternalMic);
  internalMic.audio_effect = cras::EFFECT_TYPE_STYLE_TRANSFER;
  audio_nodes.push_back(internalMic);

  SetUpCrasAudioHandlerWithPrimaryActiveNodeAndStyleTransferState(
      audio_nodes, /*primary_active_node=*/audio_nodes[0],
      /*style_transfer_enabled=*/true);

  EXPECT_EQ(0, test_observer_->style_transfer_state_change_count());

  // Change style transfer state to trigger observer.
  cras_audio_handler_->SetStyleTransferState(false);

  EXPECT_EQ(1, test_observer_->style_transfer_state_change_count());
}

TEST_P(CrasAudioHandlerTest, IsHfpMicSrSupportedForDeviceNoHfpMicSr) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({});
  // Set up initial audio devices, only with bluetooth mic.
  AudioNode bt_nb_mic = GenerateAudioNode(kBluetoothNbMic);
  // Clear the audio effect, no hfp_mic_sr supported.
  bt_nb_mic.audio_effect = 0u;
  audio_nodes.push_back(bt_nb_mic);
  // Disable hfp_mic_sr for board.
  SetUpCrasAudioHandlerWithPrimaryActiveNodeAndHfpMicSrState(
      audio_nodes, /*primary_active_node=*/audio_nodes[0],
      /*hfp_mic_sr_enabled=*/false);

  EXPECT_FALSE(
      cras_audio_handler_->IsHfpMicSrSupportedForDevice(kBluetoothNbMicId));
}

TEST_P(CrasAudioHandlerTest, IsHfpMicSrSupportedForDeviceWithHfpMicSr) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({});
  AudioNode bt_nb_mic = GenerateAudioNode(kBluetoothNbMic);
  // Set the audio effect, hfp_mic_sr supported.
  bt_nb_mic.audio_effect = cras::EFFECT_TYPE_HFP_MIC_SR;
  audio_nodes.push_back(bt_nb_mic);
  AudioNode micJack = GenerateAudioNode(kMicJack);
  // Clear the audio effect, no hfp_mic_sr supported.
  micJack.audio_effect = 0u;
  audio_nodes.push_back(micJack);
  AudioNode internalSpeaker = GenerateAudioNode(kInternalSpeaker);
  // Force audio_effect value to verify output nodes always return false.
  internalSpeaker.audio_effect = cras::EFFECT_TYPE_HFP_MIC_SR;
  audio_nodes.push_back(internalSpeaker);

  // Enable hfp_mic_sr for board.
  SetUpCrasAudioHandlerWithPrimaryActiveNodeAndHfpMicSrState(
      audio_nodes, /*primary_active_node=*/audio_nodes[0],
      /*hfp_mic_sr_enabled=*/true);

  EXPECT_TRUE(
      cras_audio_handler_->IsHfpMicSrSupportedForDevice(kBluetoothNbMicId));
  EXPECT_FALSE(cras_audio_handler_->IsHfpMicSrSupportedForDevice(kMicJackId));
  EXPECT_FALSE(
      cras_audio_handler_->IsHfpMicSrSupportedForDevice(kInternalSpeakerId));
}

TEST_P(CrasAudioHandlerTest, SetHfpMicSrStateUpdatesAudioPrefAndClient) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({});
  AudioNode bt_nb_mic = GenerateAudioNode(kBluetoothNbMic);
  // Set the audio effect, hfp_mic_sr supported.
  bt_nb_mic.audio_effect = cras::EFFECT_TYPE_HFP_MIC_SR;
  audio_nodes.push_back(bt_nb_mic);

  // Enable hfp_mic_sr for board.
  SetUpCrasAudioHandlerWithPrimaryActiveNodeAndHfpMicSrState(
      audio_nodes, /*primary_active_node=*/audio_nodes[0],
      /*hfp_mic_sr_enabled=*/true);

  EXPECT_TRUE(audio_pref_handler_->GetHfpMicSrState());
  EXPECT_TRUE(fake_cras_audio_client()->hfp_mic_sr_enabled());

  // Turn off hfp_mic_sr.
  cras_audio_handler_->SetHfpMicSrState(
      /*hfp_mic_sr_on=*/false,
      CrasAudioHandler::AudioSettingsChangeSource::kSystemTray);

  EXPECT_FALSE(audio_pref_handler_->GetHfpMicSrState());
  EXPECT_FALSE(fake_cras_audio_client()->hfp_mic_sr_enabled());

  // Turn on hfp_mic_sr.
  cras_audio_handler_->SetHfpMicSrState(
      /*hfp_mic_sr_on=*/true,
      CrasAudioHandler::AudioSettingsChangeSource::kSystemTray);

  EXPECT_TRUE(audio_pref_handler_->GetHfpMicSrState());
  EXPECT_TRUE(fake_cras_audio_client()->hfp_mic_sr_enabled());
}

TEST_P(CrasAudioHandlerTest, SetHfpMicSrStateObserver) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({});
  AudioNode bt_nb_mic = GenerateAudioNode(kBluetoothNbMic);
  // Set the audio effect, hfp_mic_sr supported.
  bt_nb_mic.audio_effect = cras::EFFECT_TYPE_HFP_MIC_SR;
  audio_nodes.push_back(bt_nb_mic);

  // Enable hfp_mic_sr for board.
  SetUpCrasAudioHandlerWithPrimaryActiveNodeAndHfpMicSrState(
      audio_nodes, /*primary_active_node=*/audio_nodes[0],
      /*hfp_mic_sr_enabled=*/true);

  EXPECT_EQ(0, test_observer_->hfp_mic_sr_state_change_count());

  // Change hfp_mic_sr state to trigger observer.
  cras_audio_handler_->SetHfpMicSrState(
      false, CrasAudioHandler::AudioSettingsChangeSource::kSystemTray);

  EXPECT_EQ(1, test_observer_->hfp_mic_sr_state_change_count());
}

TEST_P(CrasAudioHandlerTest, SpeakOnMuteDetectionSwitchTest) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({});
  // Set up initial audio devices, only with internal mic.
  AudioNode internalMic = GenerateAudioNode(kInternalMic);
  audio_nodes.push_back(internalMic);
  SetUpCrasAudioHandlerWithPrimaryActiveNode(audio_nodes, internalMic);

  // Speak-on-mute detection should still be disabled since it is not set during
  // the initialization.
  EXPECT_FALSE(fake_cras_audio_client()->speak_on_mute_detection_enabled());

  // Simulate enable speak-on-mute detection from handler, which should enable
  // speak-on-mute detection in the client.
  cras_audio_handler_->SetSpeakOnMuteDetection(/*som_on=*/true);
  EXPECT_TRUE(fake_cras_audio_client()->speak_on_mute_detection_enabled());

  // Simulate disable speak-on-mute detection from handler, which should disable
  // speak-on-mute detection in the client.
  cras_audio_handler_->SetSpeakOnMuteDetection(/*som_on=*/false);
  EXPECT_FALSE(fake_cras_audio_client()->speak_on_mute_detection_enabled());
}

TEST_P(CrasAudioHandlerTest, AudioSurveyDefault) {
  AudioNodeList audio_nodes = GenerateAudioNodeList(
      {kInternalSpeaker, kHeadphone, kInternalMic, kUSBMic1});
  base::flat_map<std::string, std::string> survey_specific_data = {
      {"key", "value"}};

  SetUpCrasAudioHandler(audio_nodes);

  // Simulate an audio survey gets triggered.
  fake_cras_audio_client()->NotifySurveyTriggered(survey_specific_data);

  EXPECT_EQ(test_observer_->survey_triggerd_count(), 1);
  EXPECT_EQ(test_observer_->survey_triggerd_recv().type(),
            CrasAudioHandler::SurveyType::kGeneral);
  EXPECT_THAT(test_observer_->survey_triggerd_recv().data(),
              testing::ElementsAre(std::make_pair("key", "value")));
}

TEST_P(CrasAudioHandlerTest, AudioSurveyBluetooth) {
  AudioNodeList audio_nodes = GenerateAudioNodeList(
      {kInternalSpeaker, kHeadphone, kInternalMic, kUSBMic1});
  base::flat_map<std::string, std::string> survey_specific_data = {
      {CrasAudioHandler::kSurveyNameKey,
       CrasAudioHandler::kSurveyNameBluetooth}};

  SetUpCrasAudioHandler(audio_nodes);

  // Simulate an audio survey gets triggered.
  fake_cras_audio_client()->NotifySurveyTriggered(survey_specific_data);

  EXPECT_EQ(test_observer_->survey_triggerd_count(), 1);
  EXPECT_EQ(test_observer_->survey_triggerd_recv().type(),
            CrasAudioHandler::SurveyType::kBluetooth);
  EXPECT_EQ(test_observer_->survey_triggerd_recv().data().size(), 0u);
}

TEST_P(CrasAudioHandlerTest, SimpleUsageAudioDevices) {
  // Set up initial audio devices, only with internal speaker and mic.
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kInternalMic});
  SetUpCrasAudioHandler(audio_nodes);

  uint32_t previous_input_count = 1u;
  uint32_t previous_output_count = 1u;
  uint32_t input_count = 1u;
  uint32_t output_count = 1u;

  // Verify the audio devices size.
  AudioDeviceList input_devices =
      cras_audio_handler_->GetSimpleUsageAudioDevices(
          GetAudioDeviceMap(/*is_current_device=*/true),
          /*is_input=*/true);
  AudioDeviceList output_devices =
      cras_audio_handler_->GetSimpleUsageAudioDevices(
          GetAudioDeviceMap(/*is_current_device=*/true),
          /*is_input=*/false);
  EXPECT_EQ(input_count, input_devices.size());
  EXPECT_EQ(output_count, output_devices.size());

  // Plug the headphone.
  audio_nodes = GenerateAudioNodeList(
      {kInternalSpeaker, kInternalMic, kHeadphone, kMicJack});
  ChangeAudioNodes(audio_nodes);

  // Verify the audio devices size.
  input_devices = cras_audio_handler_->GetSimpleUsageAudioDevices(
      GetAudioDeviceMap(/*is_current_device=*/true),
      /*is_input=*/true);
  output_devices = cras_audio_handler_->GetSimpleUsageAudioDevices(
      GetAudioDeviceMap(/*is_current_device=*/true),
      /*is_input=*/false);
  EXPECT_EQ(++input_count, input_devices.size());
  EXPECT_EQ(++output_count, output_devices.size());

  AudioDeviceList previous_input_devices =
      cras_audio_handler_->GetSimpleUsageAudioDevices(
          GetAudioDeviceMap(/*is_current_device=*/false),
          /*is_input=*/true);
  AudioDeviceList previous_output_devices =
      cras_audio_handler_->GetSimpleUsageAudioDevices(
          GetAudioDeviceMap(/*is_current_device=*/false),
          /*is_input=*/false);
  EXPECT_EQ(previous_input_count, previous_input_devices.size());
  EXPECT_EQ(previous_output_count, previous_output_devices.size());

  // Unplug the headphone.
  audio_nodes = GenerateAudioNodeList({kInternalSpeaker, kInternalMic});
  ChangeAudioNodes(audio_nodes);

  // Verify the audio devices size.
  input_devices = cras_audio_handler_->GetSimpleUsageAudioDevices(
      GetAudioDeviceMap(/*is_current_device=*/true),
      /*is_input=*/true);
  output_devices = cras_audio_handler_->GetSimpleUsageAudioDevices(
      GetAudioDeviceMap(/*is_current_device=*/true),
      /*is_input=*/false);
  EXPECT_EQ(--input_count, input_devices.size());
  EXPECT_EQ(--output_count, output_devices.size());

  previous_input_devices = cras_audio_handler_->GetSimpleUsageAudioDevices(
      GetAudioDeviceMap(/*is_current_device=*/false),
      /*is_input=*/true);
  previous_output_devices = cras_audio_handler_->GetSimpleUsageAudioDevices(
      GetAudioDeviceMap(/*is_current_device=*/false),
      /*is_input=*/false);
  EXPECT_EQ(++previous_input_count, previous_input_devices.size());
  EXPECT_EQ(++previous_output_count, previous_output_devices.size());

  // Plug a non simple usage device, which should not be counted.
  audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kInternalMic, kKeyboardMic});
  ChangeAudioNodes(audio_nodes);

  // Verify the audio devices size.
  input_devices = cras_audio_handler_->GetSimpleUsageAudioDevices(
      GetAudioDeviceMap(/*is_current_device=*/true),
      /*is_input=*/true);
  output_devices = cras_audio_handler_->GetSimpleUsageAudioDevices(
      GetAudioDeviceMap(/*is_current_device=*/true),
      /*is_input=*/false);
  EXPECT_EQ(input_count, input_devices.size());
  EXPECT_EQ(output_count, output_devices.size());

  previous_input_devices = cras_audio_handler_->GetSimpleUsageAudioDevices(
      GetAudioDeviceMap(/*is_current_device=*/false),
      /*is_input=*/true);
  previous_output_devices = cras_audio_handler_->GetSimpleUsageAudioDevices(
      GetAudioDeviceMap(/*is_current_device=*/false),
      /*is_input=*/false);
  EXPECT_EQ(--previous_input_count, previous_input_devices.size());
  EXPECT_EQ(--previous_output_count, previous_output_devices.size());
}

// Tests audio selection exception rule #3 metrics are fired that hot plugging
// an unpreferred device keeps current active device unchanged.
TEST_P(
    CrasAudioHandlerTest,
    AudioSelectionExceptionRule3MetricsFired_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  // Set up initial audio devices.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kInternalMic},
      /*expected_active_input_node=*/kInternalMic,
      /*expected_active_output_node=*/kInternalSpeaker,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/false);

  // Plug a HDMI output device. Expect active device remains unchanged as
  // internal speaker.
  AudioNodeList audio_nodes;
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);

  AudioNode internal_mic = GenerateAudioNode(kInternalMic);
  internal_mic.active = true;
  audio_nodes.push_back(internal_mic);

  AudioNode hdmi_output = GenerateAudioNode(kHDMIOutput);
  audio_nodes.push_back(hdmi_output);
  system_monitor_observer_.reset_count();
  ChangeAudioNodes(audio_nodes);

  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Plug a USB output device. Expect active device remains unchanged as
  // internal speaker.
  AudioNode usb_output = GenerateAudioNode(kUSBHeadphone1);
  audio_nodes.push_back(usb_output);
  ChangeAudioNodes(audio_nodes);

  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Activate USB output device. Expect active device is the USB output device.
  // Now USB is the preferred device among USB, internal and HDMI device.
  AudioDevice usb_output_device(usb_output);
  cras_audio_handler_->SwitchToDevice(usb_output_device, true,
                                      DeviceActivateType::kActivateByUser);

  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kUSBHeadphone1->id, active_output.id);
  EXPECT_EQ(kUSBHeadphone1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Unplug HDMI output device.
  audio_nodes.clear();
  audio_nodes.push_back(internal_speaker);
  audio_nodes.push_back(internal_mic);
  audio_nodes.push_back(usb_output);
  ChangeAudioNodes(audio_nodes);

  EXPECT_EQ(2, test_observer_->active_output_node_changed_count());

  // Activate internal speaker. Expect active device is the internal speaker.
  AudioDevice internal_speaker_device(internal_speaker);
  cras_audio_handler_->SwitchToDevice(internal_speaker_device, true,
                                      DeviceActivateType::kActivateByUser);

  EXPECT_EQ(3, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Plug HDMI output device. Expect active device remains unchanged even though
  // USB is the preferred device among USB,internal and HDMI device. Expect
  // exception rule #3 metric is fired.
  audio_nodes.push_back(hdmi_output);
  ChangeAudioNodes(audio_nodes);

  EXPECT_EQ(3, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kOutputRule3HotPlugUnpreferredDevice,
      /*expected_count=*/1);
}

// Tests audio selection performance metrics of system not switching output
// device are fired when hot plugging an unpreferred device.
TEST_P(
    CrasAudioHandlerTest,
    AudioSelectionPerformanceSystemNotSwitchingOutput_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  // Set up initial audio devices.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kInternalSpeaker,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/false);

  // Plug a HDMI output device. Expect active device remains unchanged as
  // internal speaker.
  AudioNodeList audio_nodes;
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);

  AudioNode hdmi_output = GenerateAudioNode(kHDMIOutput);
  audio_nodes.push_back(hdmi_output);
  ChangeAudioNodes(audio_nodes);

  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // At this moment, system has seen the device set of internal speaker and HDMI
  // output, and the preferable device is internal speaker.
  // Unplug HDMI output device.
  audio_nodes.clear();
  audio_nodes.push_back(internal_speaker);
  ChangeAudioNodes(audio_nodes);

  // No audio selection performance metrics are fired.
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionPerformance,
      AudioDeviceMetricsHandler::AudioSelectionEvents::kSystemNotSwitchOutput,
      /*expected_count=*/0);
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionPerformance,
      AudioDeviceMetricsHandler::AudioSelectionEvents::
          kSystemNotSwitchOutputNonChromeRestart,
      /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionPerformance,
      /*expected_count=*/0);

  // Plug HDMI output device. Expect active device remains unchanged. Expect
  // audio selection performance metric of system not switching device is fired.
  audio_nodes.push_back(hdmi_output);
  ChangeAudioNodes(audio_nodes);

  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionPerformance,
      AudioDeviceMetricsHandler::AudioSelectionEvents::kSystemNotSwitchOutput,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionPerformance,
      AudioDeviceMetricsHandler::AudioSelectionEvents::
          kSystemNotSwitchOutputNonChromeRestart,
      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionPerformance,
      /*expected_count=*/2);
}

// Tests audio selection performance metrics of system not switching input
// device are fired when hot plugging an unpreferred device.
TEST_P(
    CrasAudioHandlerTest,
    AudioSelectionPerformanceSystemNotSwitchingInput_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  // Set up initial audio devices.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalMic},
      /*expected_active_input_node=*/kInternalMic,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/false);

  // Plug a USB input device. Expect active device remains unchanged as
  // internal mic.
  AudioNodeList audio_nodes;
  AudioNode internal_mic = GenerateAudioNode(kInternalMic);
  internal_mic.active = true;
  audio_nodes.push_back(internal_mic);

  AudioNode usb_input = GenerateAudioNode(kUSBMic1);
  audio_nodes.push_back(usb_input);
  ChangeAudioNodes(audio_nodes);

  EXPECT_EQ(0, test_observer_->active_input_node_changed_count());
  ExpectActiveDevice(/*is_input=*/true,
                     /*expected_active_device=*/kInternalMic,
                     /*has_alternative_device=*/true);

  // At this moment, system has seen the device set of internal mic and HDMI
  // output, and the preferable device is internal mic.
  // Unplug HDMI output device.
  audio_nodes.clear();
  audio_nodes.push_back(internal_mic);
  ChangeAudioNodes(audio_nodes);

  // No audio selection performance metrics are fired.
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionPerformance,
      AudioDeviceMetricsHandler::AudioSelectionEvents::kSystemNotSwitchInput,
      /*expected_count=*/0);
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionPerformance,
      AudioDeviceMetricsHandler::AudioSelectionEvents::
          kSystemNotSwitchInputNonChromeRestart,
      /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionPerformance,
      /*expected_count=*/0);

  // Plug the USB input device. Expect active device remains unchanged. Expect
  // audio selection performance metric of system not switching device is fired.
  audio_nodes.push_back(usb_input);
  ChangeAudioNodes(audio_nodes);

  EXPECT_EQ(0, test_observer_->active_input_node_changed_count());
  ExpectActiveDevice(/*is_input=*/true,
                     /*expected_active_device=*/kInternalMic,
                     /*has_alternative_device=*/true);

  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionPerformance,
      AudioDeviceMetricsHandler::AudioSelectionEvents::kSystemNotSwitchInput,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionPerformance,
      AudioDeviceMetricsHandler::AudioSelectionEvents::
          kSystemNotSwitchInputNonChromeRestart,
      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionPerformance,
      /*expected_count=*/2);
}

// Tests audio selection exception rule #2 metric is recorded when unplugging a
// non active device and the currently active device is not the preferred device
// in the new device set.
TEST_P(CrasAudioHandlerTest,
       AudioSelectionExceptionRule2Output_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  // Set up initial audio devices.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kInternalSpeaker,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/false);

  // Plug a HDMI output device. Expect active device remains unchanged as
  // internal speaker.
  AudioNodeList audio_nodes;
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);

  AudioNode hdmi_output = GenerateAudioNode(kHDMIOutput);
  audio_nodes.push_back(hdmi_output);
  ChangeAudioNodes(audio_nodes);

  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // At this moment, system has seen the device set of internal speaker and HDMI
  // output, and the preferable device is internal speaker.

  // Plug a USB output device. Expect active device remains unchanged as
  // internal speaker.
  AudioNode usb_output = GenerateAudioNode(kUSBHeadphone1);
  audio_nodes.push_back(usb_output);
  ChangeAudioNodes(audio_nodes);

  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Activate HDMI output device, expect HDMI output device becomes active.
  AudioDevice hdmi_output_device(hdmi_output);
  cras_audio_handler_->SwitchToDevice(hdmi_output_device, true,
                                      DeviceActivateType::kActivateByUser);

  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHDMIOutput->id, active_output.id);
  EXPECT_EQ(kHDMIOutput->id, cras_audio_handler_->GetPrimaryActiveOutputNode());

  // No metrics are fired before unplugging.
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kOutputRule2UnplugNonActiveDevice,
      /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      /*expected_count=*/0);

  // Unplug the USB device
  audio_nodes.clear();
  audio_nodes.push_back(internal_speaker);
  hdmi_output.active = true;
  audio_nodes.push_back(hdmi_output);
  ChangeAudioNodes(audio_nodes);

  // Expect active device remains HDMI, disregarding that the internal speaker
  // is the preferable device in the device set of internal speaker and HDMI
  // output device.
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHDMIOutput->id, active_output.id);
  EXPECT_EQ(kHDMIOutput->id, cras_audio_handler_->GetPrimaryActiveOutputNode());

  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kOutputRule2UnplugNonActiveDevice,
      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      /*expected_count=*/1);
}

// Tests audio selection exception rule #2 metric is recorded when unplugging a
// non active device and the currently active device is not the preferred device
// in the new device set.
TEST_P(CrasAudioHandlerTest,
       AudioSelectionExceptionRule2Input_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  // Set up initial audio devices.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalMic},
      /*expected_active_input_node=*/kInternalMic,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/false);

  // Plug a USB input1 device. Expect active device remains unchanged as
  // internal speaker.
  AudioNodeList audio_nodes;
  AudioNode internal_mic = GenerateAudioNode(kInternalMic);
  internal_mic.active = true;
  audio_nodes.push_back(internal_mic);

  AudioNode usb_input1 = GenerateAudioNode(kUSBMic1);
  audio_nodes.push_back(usb_input1);
  ChangeAudioNodes(audio_nodes);

  EXPECT_EQ(0, test_observer_->active_input_node_changed_count());
  ExpectActiveDevice(/*is_input=*/true,
                     /*expected_active_device=*/kInternalMic,
                     /*has_alternative_device=*/true);

  // At this moment, system has seen the device set of internal mic and USB
  // input1, and the preferable device is internal mic.

  // Plug a USB input2 device. Expect active device remains unchanged as
  // internal mic.
  AudioNode usb_input2 = GenerateAudioNode(kUSBMic2);
  audio_nodes.push_back(usb_input2);
  ChangeAudioNodes(audio_nodes);

  ExpectActiveDevice(/*is_input=*/true,
                     /*expected_active_device=*/kInternalMic,
                     /*has_alternative_device=*/true);

  // Activate USB input1 device, expect USB input1 device becomes active.
  AudioDevice usb_input1_device(usb_input1);
  cras_audio_handler_->SwitchToDevice(usb_input1_device, true,
                                      DeviceActivateType::kActivateByUser);

  ExpectActiveDevice(/*is_input=*/true, /*expected_active_device=*/kUSBMic1,
                     /*has_alternative_device=*/true);

  // No metrics are fired before unplugging.
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kInputRule2UnplugNonActiveDevice,
      /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      /*expected_count=*/0);

  // Unplug the USB2 device
  audio_nodes.clear();
  audio_nodes.push_back(internal_mic);
  usb_input1.active = true;
  audio_nodes.push_back(usb_input1);
  ChangeAudioNodes(audio_nodes);

  // Expect active device remains USB1 input device, disregarding that the
  // internal mic is the preferable device in the device set of internal mic and
  // USB1 input device.
  ExpectActiveDevice(/*is_input=*/true, /*expected_active_device=*/kUSBMic1,
                     /*has_alternative_device=*/true);

  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kInputRule2UnplugNonActiveDevice,
      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      /*expected_count=*/1);
}

TEST_P(CrasAudioHandlerTest, AudioSurveyOutputProc) {
  AudioNodeList audio_nodes = GenerateAudioNodeList(
      {kInternalSpeaker, kHeadphone, kInternalMic, kUSBMic1});
  base::flat_map<std::string, std::string> survey_specific_data = {
      {CrasAudioHandler::kSurveyNameKey,
       CrasAudioHandler::kSurveyNameOutputProc}};

  SetUpCrasAudioHandler(audio_nodes);

  // Simulate an audio survey gets triggered.
  fake_cras_audio_client()->NotifySurveyTriggered(survey_specific_data);

  EXPECT_EQ(test_observer_->survey_triggerd_count(), 1);
  EXPECT_EQ(test_observer_->survey_triggerd_recv().type(),
            CrasAudioHandler::SurveyType::kOutputProc);
  EXPECT_EQ(test_observer_->survey_triggerd_recv().data().size(), 0u);
}

// Tests that two internal input devices don't set alternative_device to be
// true.
TEST_P(CrasAudioHandlerTest,
       AlternativeInputDeviceWithTwoInternalInputDevices) {
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalMic, kFrontMic},
      /*expected_active_input_node=*/kInternalMic,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/std::nullopt);
}

// Tests that three internal input devices don't set alternative_device to be
// true.
TEST_P(CrasAudioHandlerTest,
       AlternativeInputDeviceWithThreeInternalInputDevices) {
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalMic, kFrontMic, kRearMic},
      /*expected_active_input_node=*/kInternalMic,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/std::nullopt);
}

// Tests that one external input devices doesn't set alternative_device to be
// true.
TEST_P(CrasAudioHandlerTest, AlternativeInputDeviceWithOneExternalInputDevice) {
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kUSBMic1},
      /*expected_active_input_node=*/kUSBMic1,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/std::nullopt);
}

// Tests that two external input devices set alternative_device to be
// true.
TEST_P(CrasAudioHandlerTest,
       AlternativeInputDeviceWithTwoExternalInputDevices) {
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kUSBMic1, kUSBMic2},
      /*expected_active_input_node=*/kUSBMic1,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/true,
      /*expected_has_alternative_output=*/std::nullopt);
}

// Tests that one external output devices doesn't set alternative_device to be
// true.
TEST_P(CrasAudioHandlerTest,
       AlternativeInputDeviceWithOneExternalOutputDevice) {
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kUSBHeadphone1},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kUSBHeadphone1,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/false);
}

// Tests that two external output devices set alternative_device to be true.
TEST_P(CrasAudioHandlerTest,
       AlternativeInputDeviceWithTwoExternalOutputDevices) {
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kUSBHeadphone1, kHDMIOutput},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kUSBHeadphone1,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/true);
}

// Tests that one internal and one external output devices set
// alternative_device to be true.
TEST_P(CrasAudioHandlerTest,
       AlternativeInputDeviceWithOneInternalAndOneExternalOutputDevice) {
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kHDMIOutput},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kHDMIOutput,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/true);
}

// Tests that one internal and one external input devices set alternative_device
// to be true.
TEST_P(CrasAudioHandlerTest,
       AlternativeInputDeviceWithOneInternalAndOneExternalInputDevice) {
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kUSBMic1, kInternalMic},
      /*expected_active_input_node=*/kUSBMic1,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/true,
      /*expected_has_alternative_output=*/std::nullopt);
}

// Unplug a device with only one device left, activate that remaining device.
TEST_P(CrasAudioHandlerTest,
       UnplugDeviceWithOneDeviceLeft_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  // Set up initial audio devices, with internal speaker and headphone.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kHeadphone},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kHeadphone,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/true);

  // Unplug the headphone.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(GenerateAudioNode(kInternalSpeaker));
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and one audio device is
  // removed.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  VerifySystemMonitorWasCalled();
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(1u, audio_devices.size());

  // Verify the active output device is switched to internal speaker and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/false);
}

// Unplug an active device and the remaining device set was seen before, expect
// that the preferred device is activated.
TEST_P(
    CrasAudioHandlerTest,
    UnplugActiveDeviceWithRemainingSetSeenBefore_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  // Set up initial audio devices, with kInternalSpeaker, kUSBHeadphone1 and
  // kUSBHeadphone2.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kUSBHeadphone1, kUSBHeadphone2},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kInternalSpeaker,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/true);

  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  AudioNode usb_headphone_1 = GenerateAudioNode(kUSBHeadphone1);
  AudioNode usb_headphone_2 = GenerateAudioNode(kUSBHeadphone2);
  AudioNode hdmi_output = GenerateAudioNode(kHDMIOutput);

  // Activate kUSBHeadphone2, now that kUSBHeadphone2 is the preferred device
  // among kInternalSpeaker, kUSBHeadphone1 and kUSBHeadphone2.
  AudioDevice usb_headphone_device_2(usb_headphone_2);
  cras_audio_handler_->SwitchToDevice(usb_headphone_device_2, true,
                                      DeviceActivateType::kActivateByUser);
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kUSBHeadphone2,
                     /*has_alternative_device=*/true);

  // Plug kHDMIOutput and activate it.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(internal_speaker);
  audio_nodes.push_back(usb_headphone_1);
  audio_nodes.push_back(usb_headphone_2);
  audio_nodes.push_back(hdmi_output);
  ChangeAudioNodes(audio_nodes);

  AudioDevice hdmi_output_device(hdmi_output);
  cras_audio_handler_->SwitchToDevice(hdmi_output_device, true,
                                      DeviceActivateType::kActivateByUser);
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kHDMIOutput,
                     /*has_alternative_device=*/true);

  // Unplug the kHDMIOutput, expect that kUSBHeadphone2 is acivated as the
  // preferred device among remaining device set.
  audio_nodes.clear();
  audio_nodes.push_back(internal_speaker);
  audio_nodes.push_back(usb_headphone_1);
  audio_nodes.push_back(usb_headphone_2);
  ChangeAudioNodes(audio_nodes);

  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kUSBHeadphone2,
                     /*has_alternative_device=*/true);
}

// Unplug an active device and the remaining device set was not seen before,
// expect that the most recently activated device is activated.
TEST_P(
    CrasAudioHandlerTest,
    UnplugActiveDeviceWithRemainingSetUnseenBefore1_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  // Set up initial audio devices, with kInternalSpeaker, kUSBHeadphone1 and
  // kUSBHeadphone2.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker, kUSBHeadphone1, kUSBHeadphone2},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kInternalSpeaker,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/true);

  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  AudioNode usb_headphone_1 = GenerateAudioNode(kUSBHeadphone1);
  AudioNode usb_headphone_2 = GenerateAudioNode(kUSBHeadphone2);
  AudioNode hdmi_output = GenerateAudioNode(kHDMIOutput);

  // Activate kUSBHeadphone2. Now that kUSBHeadphone2 the most recently active
  // device.
  AudioDevice usb_headphone_device_2(usb_headphone_2);
  cras_audio_handler_->SwitchToDevice(usb_headphone_device_2, true,
                                      DeviceActivateType::kActivateByUser);
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kUSBHeadphone2,
                     /*has_alternative_device=*/true);

  // Plug kHDMIOutput and activate it. Now that kHDMIOutputis the most recently
  // active device.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(internal_speaker);
  audio_nodes.push_back(usb_headphone_1);
  audio_nodes.push_back(usb_headphone_2);
  audio_nodes.push_back(hdmi_output);
  ChangeAudioNodes(audio_nodes);

  AudioDevice hdmi_output_device(hdmi_output);
  cras_audio_handler_->SwitchToDevice(hdmi_output_device, true,
                                      DeviceActivateType::kActivateByUser);
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kHDMIOutput,
                     /*has_alternative_device=*/true);

  // Activate kUSBHeadphone1 and Unplug it, remaining device set
  // kInternalSpeaker, kUSBHeadphone2, kHDMIOutputis was not seen before. Expect
  // that kHDMIOutput is acivated as the most recently active device.
  AudioDevice usb_headphone_device_1(usb_headphone_1);
  cras_audio_handler_->SwitchToDevice(usb_headphone_device_1, true,
                                      DeviceActivateType::kActivateByUser);

  // No metrics are fired before unplugging the active device.
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kOutputRule4UnplugDeviceCausesUnseenSet,
      /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      /*expected_count=*/0);

  audio_nodes.clear();
  audio_nodes.push_back(internal_speaker);
  audio_nodes.push_back(usb_headphone_2);
  audio_nodes.push_back(hdmi_output);
  ChangeAudioNodes(audio_nodes);

  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kHDMIOutput,
                     /*has_alternative_device=*/true);

  // Exception rule #4 metrics are fired after unplugging the active device and
  // remaining device set was not seen before.
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kOutputRule4UnplugDeviceCausesUnseenSet,
      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      /*expected_count=*/1);
}

// Unplug an active device and the remaining device set was not seen before.
// There is no most recently active device available, fall back to previous
// approach.
TEST_P(
    CrasAudioHandlerTest,
    UnplugActiveDeviceWithRemainingSetUnseenBefore2_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  // Set up initial audio devices, with kHDMIOutput, kUSBHeadphone1 and
  // kUSBHeadphone2.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kHDMIOutput, kUSBHeadphone1, kUSBHeadphone2},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kUSBHeadphone1,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/true);

  AudioNode hdmi_output = GenerateAudioNode(kHDMIOutput);
  AudioNode usb_headphone_1 = GenerateAudioNode(kUSBHeadphone1);
  AudioNode usb_headphone_2 = GenerateAudioNode(kUSBHeadphone2);

  // Activate kUSBHeadphone1 and Unplug it, remaining device set
  // kHDMIOutput, kUSBHeadphone2 was not seen before. There is no most
  // recently active device available. Fall back to previous approach and expect
  // that kUSBHeadphone2 is acivated since it has higher default priority than
  // kHDMIOutput.
  AudioDevice usb_headphone_device_1(usb_headphone_1);
  cras_audio_handler_->SwitchToDevice(usb_headphone_device_1, true,
                                      DeviceActivateType::kActivateByUser);

  // No metrics are fired before unplugging the active device.
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kOutputRule4UnplugDeviceCausesUnseenSet,
      /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      /*expected_count=*/0);

  AudioNodeList audio_nodes;
  audio_nodes.push_back(hdmi_output);
  audio_nodes.push_back(usb_headphone_2);
  ChangeAudioNodes(audio_nodes);

  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kUSBHeadphone2,
                     /*has_alternative_device=*/true);

  // Exception rule #4 metrics are fired after unplugging the active device and
  // remaining device set was not seen before.
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kOutputRule4UnplugDeviceCausesUnseenSet,
      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      /*expected_count=*/1);
}

// Unplug an input active device and the remaining device set was not seen
// before. Exception rule #4 metrics are fired.
TEST_P(
    CrasAudioHandlerTest,
    UnplugActiveDeviceWithRemainingSetUnseenBefore4_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  // Set up initial audio devices, with kBluetoothNbMic, kUSBMic1 and
  // kUSBMic2.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kBluetoothNbMic, kUSBMic1, kUSBMic2},
      /*expected_active_input_node=*/kUSBMic1,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/true,
      /*expected_has_alternative_output=*/std::nullopt);

  AudioNode bluetooth_input = GenerateAudioNode(kBluetoothNbMic);
  AudioNode usb_mic_1 = GenerateAudioNode(kUSBMic1);
  AudioNode usb_mic_2 = GenerateAudioNode(kUSBMic2);

  // Activate kUSBMic1 and Unplug it, remaining device set
  // kBluetoothNbMic, kUSBMic2 was not seen before. There is no most
  // recently active device available. Fall back to previous approach and expect
  // that kUSBMic2 is acivated since it has higher default priority than
  // kBluetoothNbMic.
  AudioDevice usb_mic_device_1(usb_mic_1);
  cras_audio_handler_->SwitchToDevice(usb_mic_device_1, true,
                                      DeviceActivateType::kActivateByUser);

  // No metrics are fired before unplugging the active device.
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kInputRule4UnplugDeviceCausesUnseenSet,
      /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      /*expected_count=*/0);

  AudioNodeList audio_nodes;
  audio_nodes.push_back(bluetooth_input);
  audio_nodes.push_back(usb_mic_2);
  ChangeAudioNodes(audio_nodes);

  ExpectActiveDevice(/*is_input=*/true,
                     /*expected_active_device=*/kUSBMic2,
                     /*has_alternative_device=*/true);

  // Exception rule #4 metrics are fired after unplugging the active device and
  // remaining device set was not seen before.
  histogram_tester_.ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
          kInputRule4UnplugDeviceCausesUnseenSet,
      /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionExceptionRuleMetrics,
      /*expected_count=*/1);
}

// Tests system boots with only one device kHDMIOutput, kHDMIOutput is expected
// to be activated and no notification is shown.
TEST_P(CrasAudioHandlerTest,
       SystemBootsOnlyOneDevice_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kHDMIOutput},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kHDMIOutput,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/false);

  EXPECT_EQ(0u, GetNotificationCount());
}

// Tests system boots with kInternalSpeaker and 3.5mm kHeadphone, kHeadphone is
// expected to be activated and no notification is shown.
TEST_P(CrasAudioHandlerTest,
       SystemBootsWith35mmHeadphone_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kHDMIOutput, kHeadphone},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kHeadphone,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/true);

  EXPECT_EQ(0u, GetNotificationCount());
}

// Tests system boots with kInternalSpeaker and kHDMIOutput, no most current
// activated device list, kInternalSpeaker is expected to be activated and
// notification will be shown.
TEST_P(CrasAudioHandlerTest,
       SystemBootsWithInternalAndExternal_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kHDMIOutput, kInternalSpeaker},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kInternalSpeaker,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/true);

  FastForwardBy(AudioSelectionNotificationHandler::kDebounceTime);
  EXPECT_EQ(1u, GetNotificationCount());
}

// Tests system boots with kUSBHeadphone1 and kHDMIOutput, no most current
// activated device list, kUSBHeadphone1 is expected to be activated since it
// has higher built-in priority, notification will be shown.
TEST_P(CrasAudioHandlerTest,
       SystemBootsWithTwoExternal_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kHDMIOutput, kUSBHeadphone1},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kUSBHeadphone1,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/true);

  FastForwardBy(AudioSelectionNotificationHandler::kDebounceTime);
  EXPECT_EQ(1u, GetNotificationCount());
}

// Tests system boots with device set that was seen before. Activate the
// preferred device among the device set and no notification is shown.
TEST_P(CrasAudioHandlerTest,
       SystemBootsWithSeenDeviceSet_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kHDMIOutput, kInternalSpeaker, kHeadphone},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kHeadphone,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/true);

  EXPECT_EQ(0u, GetNotificationCount());

  AudioNode hdmi_output = GenerateAudioNode(kHDMIOutput);
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  AudioNode headphone = GenerateAudioNode(kHeadphone);

  // Activate kHDMIOutput.
  AudioDevice hdmi_output_device(hdmi_output);
  cras_audio_handler_->SwitchToDevice(hdmi_output_device, true,
                                      DeviceActivateType::kActivateByUser);

  // Reset audio nodes.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(internal_speaker);
  ChangeAudioNodes(audio_nodes);

  // kHDMIOutput is not activated at this moment.
  EXPECT_NE(kHDMIOutput->id, cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Mock system reboots.
  audio_nodes.clear();
  audio_nodes.push_back(hdmi_output);
  audio_nodes.push_back(internal_speaker);
  audio_nodes.push_back(headphone);
  ChangeAudioNodes(audio_nodes);

  // Expect kHDMIOutput is activated as the preferred device.
  ExpectActiveDevice(/*is_input=*/false, /*expected_active_device=*/kHDMIOutput,
                     /*has_alternative_device=*/true);
}

// Tests non simple usage devices are ignored.
TEST_P(CrasAudioHandlerTest,
       IgnoreNonSimpleUsageDevices_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  AudioNodeList audio_nodes;
  audio_nodes.push_back(GenerateAudioNode(kMicJack));
  audio_nodes.push_back(GenerateAudioNode(kKeyboardMic));
  SetUpCrasAudioHandler(audio_nodes);

  // kKeyboardMic is a simple usage device, it should be ignored.
  // The devices size is expected to be 1.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(1u, audio_devices.size());
}

// Tests calling SyncDevicePrefSetMap with no active device will early return
// and no crashes.
TEST_P(CrasAudioHandlerTest, SyncDevicePrefSetMap) {
  SetUpCrasAudioHandler({});
  const std::map<std::string, std::string>& device_pref_set_map =
      GetDevicePrefSetMap();
  EXPECT_TRUE(device_pref_set_map.empty());

  SyncDevicePrefSetMap(/*is_input=*/true);
  SyncDevicePrefSetMap(/*is_input=*/false);
  EXPECT_TRUE(device_pref_set_map.empty());
}

// Tests that showing notification is debounced for audio output device.
TEST_P(CrasAudioHandlerTest,
       DebounceNotificationForOutput_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  // Initialize with internal speaker.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kInternalSpeaker,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/false);

  // Expect that notification is not displayed since there is only one output
  // device connected.
  EXPECT_EQ(0u, GetNotificationCount());

  // Plug in a usb headphone without fast forward time.
  AudioNodeList audio_nodes;
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);
  AudioNode usb_headphone_1 = GenerateAudioNode(kUSBHeadphone1);
  audio_nodes.push_back(usb_headphone_1);
  ChangeAudioNodes(audio_nodes);

  // Although notification is supposed to show up for unseen new connected USB
  // headphone device, the debounce function will delay the display of
  // notification.
  EXPECT_EQ(0u, GetNotificationCount());

  // Plug in another usb headphone with fast forward time.
  AudioNode usb_headphone_2 = GenerateAudioNode(kUSBHeadphone2);
  audio_nodes.push_back(usb_headphone_2);
  ChangeAudioNodes(audio_nodes);

  // Verify the active output device is not switched because the new connected
  // device is not seen before.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/true);

  // Verify notification is displayed at this moment.
  FastForwardBy(AudioSelectionNotificationHandler::kDebounceTime);
  EXPECT_EQ(1u, GetNotificationCount());

  // Verify that the notification is for multiple audio sources detected.
  // when two new devices are hot plugged quickly, due to the debounce
  // mechanism, rather than showing different notifications for each of them
  // (with the first one being overridden quickly), we show notification to
  // handle both of them together.
  std::optional<std::u16string> title = GetNotificationTitle();
  EXPECT_TRUE(title.has_value());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_MULTIPLE_DEVICES_TITLE),
      title.value());
}

// Tests that showing notification is debounced for audio input device.
TEST_P(CrasAudioHandlerTest,
       DebounceNotificationForInput_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  // Initialize with internal mic.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalMic},
      /*expected_active_input_node=*/kInternalMic,
      /*expected_active_output_node=*/nullptr,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/std::nullopt);

  // Expect that notification is not displayed since there is only one input
  // device connected.
  EXPECT_EQ(0u, GetNotificationCount());

  // Plug in a usb mic without fast forward time.
  AudioNodeList audio_nodes;
  AudioNode inernal_mic = GenerateAudioNode(kInternalMic);
  inernal_mic.active = true;
  audio_nodes.push_back(inernal_mic);
  AudioNode usb_mic_1 = GenerateAudioNode(kUSBMic1);
  audio_nodes.push_back(usb_mic_1);
  ChangeAudioNodes(audio_nodes);

  // Although notification is supposed to show up for unseen new connected USB
  // mic device, the debounce function will delay the display of
  // notification.
  EXPECT_EQ(0u, GetNotificationCount());

  // Plug in another usb mic with fast forward time.
  AudioNode usb_mic_2 = GenerateAudioNode(kUSBMic2);
  audio_nodes.push_back(usb_mic_2);
  ChangeAudioNodes(audio_nodes);

  // Verify the active output device is not switched because the new connected
  // device is not seen before.
  EXPECT_EQ(0, test_observer_->active_input_node_changed_count());
  ExpectActiveDevice(/*is_input=*/true,
                     /*expected_active_device=*/kInternalMic,
                     /*has_alternative_device=*/true);

  // Verify notification is displayed at this moment.
  FastForwardBy(AudioSelectionNotificationHandler::kDebounceTime);
  EXPECT_EQ(1u, GetNotificationCount());

  // Verify that the notification is for multiple audio sources detected.
  // when two new devices are hot plugged quickly, due to the debounce
  // mechanism, rather than showing different notifications for each of them
  // (with the first one being overridden quickly), we show notification to
  // handle both of them together.
  std::optional<std::u16string> title = GetNotificationTitle();
  EXPECT_TRUE(title.has_value());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_MULTIPLE_DEVICES_TITLE),
      title.value());
}

// Tests that in a rare case where both input and output have the same stable
// id, manually switch output device should keep the input device unchanged.
TEST_P(CrasAudioHandlerTest,
       SwitchOutputShouldKeepInputUnchanged_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  // Initialize with internal devices.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalMic, kInternalSpeaker},
      /*expected_active_input_node=*/kInternalMic,
      /*expected_active_output_node=*/kInternalSpeaker,
      /*expected_has_alternative_input=*/false,
      /*expected_has_alternative_output=*/false);

  // Plug USB input and output devices with the same stable id.
  AudioNodeList audio_nodes;
  AudioNode inernal_mic = GenerateAudioNode(kInternalMic);
  inernal_mic.active = true;
  audio_nodes.push_back(inernal_mic);
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);

  uint64_t id = 100;
  uint64_t stable_device_id_v1 = 12345;
  uint64_t stable_device_id_v2 = 6789;
  AudioNode usb_input = AudioNode(
      /*is_input=*/true, id, /*has_v2_stable_device_id=*/true,
      stable_device_id_v1, stable_device_id_v2,
      /*device_name=*/"usb_input",
      /*type=*/"USB", /*name=*/"usb_input", false /* is_active*/,
      /*plugged_time=*/100, kInputMaxSupportedChannels, kInputAudioEffect,
      /*number_of_volume_steps=*/0);
  audio_nodes.push_back(usb_input);
  AudioNode usb_output = AudioNode(
      /*is_input=*/false, id, /*has_v2_stable_device_id=*/true,
      stable_device_id_v1, stable_device_id_v2,
      /*device_name=*/"usb_output",
      /*type=*/"USB", /*name=*/"usb_output", false /* is_active*/,
      /*plugged_time=*/100, kOutputMaxSupportedChannels, kOutputAudioEffect,
      /*number_of_volume_steps=*/0);
  audio_nodes.push_back(usb_output);
  ChangeAudioNodes(audio_nodes);

  // Verify internal device is still active.
  ExpectActiveDevice(/*is_input=*/true,
                     /*expected_active_device=*/kInternalMic,
                     /*has_alternative_device=*/true);
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/true);

  // Switch to usb_output.
  AudioDevice usb_output_device(usb_output);
  cras_audio_handler_->SwitchToDevice(usb_output_device, true,
                                      DeviceActivateType::kActivateByUser);
  ChangeAudioNodes(audio_nodes);

  // Verify output is switched to usb_output, input statys the same.
  ExpectActiveDevice(/*is_input=*/true,
                     /*expected_active_device=*/kInternalMic,
                     /*has_alternative_device=*/true);
  EXPECT_EQ(usb_output_device.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
}

// Tests that GetDeviceFromStableDeviceId can get the correct device when an
// input and an output device have the same stable id.
TEST_P(CrasAudioHandlerTest,
       GetDeviceFromStableDeviceId_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  // Plug USB input and output devices with the same stable id.
  AudioNodeList audio_nodes;
  uint64_t stable_device_id = 12345;
  std::string input_device_name = "usb_input";
  std::string output_device_name = "usb_output";
  AudioNode usb_input = AudioNode(
      /*is_input=*/true, /*id=*/800, /*has_v2_stable_device_id=*/true,
      stable_device_id, stable_device_id,
      /*device_name=*/input_device_name,
      /*type=*/"USB", /*name=*/input_device_name, /*active=*/false,
      /*plugged_time=*/100, kInputMaxSupportedChannels, kInputAudioEffect,
      /*number_of_volume_steps=*/0);

  AudioNode usb_output = AudioNode(
      /*is_input=*/false, /*id=*/900, /*has_v2_stable_device_id=*/true,
      stable_device_id, stable_device_id,
      /*device_name=*/output_device_name,
      /*type=*/"USB", /*name=*/output_device_name, /*active=*/false,
      /*plugged_time=*/100, kOutputMaxSupportedChannels, kOutputAudioEffect,
      /*number_of_volume_steps=*/0);
  audio_nodes.push_back(usb_input);
  audio_nodes.push_back(usb_output);
  SetUpCrasAudioHandler(audio_nodes);

  std::optional<AudioDevice> device1 =
      GetDeviceFromStableDeviceId(/*is_input=*/false, stable_device_id);
  EXPECT_TRUE(device1.has_value());
  EXPECT_FALSE(device1->is_input);
  EXPECT_EQ(device1->device_name, output_device_name);

  std::optional<AudioDevice> device2 =
      GetDeviceFromStableDeviceId(/*is_input=*/true, stable_device_id);
  EXPECT_TRUE(device2.has_value());
  EXPECT_TRUE(device2->is_input);
  EXPECT_EQ(device2->device_name, input_device_name);
}

// Tests that notification is removed if the hot plugged device that triggered
// the notification has already been activated via settings or quick settings.
TEST_P(
    CrasAudioHandlerTest,
    RemoveNotificationIfHotPluggedDeviceHasBeenActivated_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  // Initialize with internal speaker.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kInternalSpeaker,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/false);

  // Expect that notification is not displayed since there is only one output
  // device connected.
  EXPECT_EQ(0u, GetNotificationCount());

  // Plug in a usb headphone.
  AudioNodeList audio_nodes;
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);
  AudioNode usb_headphone_1 = GenerateAudioNode(kUSBHeadphone1);
  audio_nodes.push_back(usb_headphone_1);
  ChangeAudioNodes(audio_nodes);

  // Verify the active output device is not switched because the new connected
  // device is not seen before.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/true);

  // Verify notification is displayed.
  FastForwardBy(AudioSelectionNotificationHandler::kDebounceTime);
  EXPECT_EQ(1u, GetNotificationCount());

  // Activate the kUSBHeadphone1 by system, expect that notification is not
  // removed.
  AudioDevice usb_headphone_device(usb_headphone_1);
  cras_audio_handler_->SwitchToDevice(usb_headphone_device, true,
                                      DeviceActivateType::kActivateByPriority);
  EXPECT_EQ(1u, GetNotificationCount());

  // Switch back to internal speaker.
  AudioDevice internal_speaker_device(internal_speaker);
  cras_audio_handler_->SwitchToDevice(internal_speaker_device, true,
                                      DeviceActivateType::kActivateByPriority);

  // Manually activate the kUSBHeadphone1, expect that notification is removed.
  cras_audio_handler_->SwitchToDevice(usb_headphone_device, true,
                                      DeviceActivateType::kActivateByUser);
  EXPECT_EQ(0u, GetNotificationCount());
}

// Tests that notification is not removed if the hot plugged device that
// triggered the notification was disconnected and reconnected within grace
// period.
TEST_P(
    CrasAudioHandlerTest,
    DoNotRemoveNotificationIfHotPluggedDeviceWasDiconnectedAndReconnectedQuickly_AudioSelectionImprovementFlagOn) {
  scoped_feature_list_.InitAndEnableFeature(
      ash::features::kAudioSelectionImprovement);

  // Initialize with internal speaker.
  SetupAudioNodesAndExpectActiveNodes(
      /*initial_nodes=*/{kInternalSpeaker},
      /*expected_active_input_node=*/nullptr,
      /*expected_active_output_node=*/kInternalSpeaker,
      /*expected_has_alternative_input=*/std::nullopt,
      /*expected_has_alternative_output=*/false);

  // Expect that notification is not displayed since there is only one output
  // device connected.
  EXPECT_EQ(0u, GetNotificationCount());

  // Plug in a usb headphone.
  AudioNodeList audio_nodes;
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);
  AudioNode usb_headphone_1 = GenerateAudioNode(kUSBHeadphone1);
  audio_nodes.push_back(usb_headphone_1);
  ChangeAudioNodes(audio_nodes);

  // Verify the active output device is not switched because the new connected
  // device is not seen before.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  ExpectActiveDevice(/*is_input=*/false,
                     /*expected_active_device=*/kInternalSpeaker,
                     /*has_alternative_device=*/true);

  // Verify notification is displayed.
  FastForwardBy(AudioSelectionNotificationHandler::kDebounceTime);
  EXPECT_EQ(1u, GetNotificationCount());

  // Disconnect the usb headphone.
  audio_nodes.clear();
  audio_nodes.push_back(internal_speaker);
  ChangeAudioNodes(audio_nodes);

  // Notification still shows within grace period.
  FastForwardBy(base::Milliseconds(2000));
  EXPECT_EQ(1u, GetNotificationCount());

  // Reconnect the usb headphone within grace period.
  audio_nodes.clear();
  audio_nodes.push_back(internal_speaker);
  audio_nodes.push_back(usb_headphone_1);
  ChangeAudioNodes(audio_nodes);

  // Verify that notification still displays.
  FastForwardBy(base::Milliseconds(2000));
  EXPECT_EQ(1u, GetNotificationCount());

  // Verify that notification after grace period.
  FastForwardBy(CrasAudioHandler::kRemoveNotificationDelay);
  EXPECT_EQ(1u, GetNotificationCount());

  // Remove the usb headphone again.
  audio_nodes.clear();
  audio_nodes.push_back(internal_speaker);
  ChangeAudioNodes(audio_nodes);

  // Notification is removed after grace period.
  FastForwardBy(CrasAudioHandler::kRemoveNotificationDelay);
  EXPECT_EQ(0u, GetNotificationCount());
}

}  // namespace ash
