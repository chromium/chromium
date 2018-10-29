// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/audio/cras_audio_handler.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/system/system_monitor.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chromeos/audio/audio_devices_pref_handler.h"
#include "chromeos/audio/audio_devices_pref_handler_stub.h"
#include "chromeos/dbus/audio_node.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cras_audio_client.h"
#include "media/base/video_facing.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
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
const uint64_t kMicJackId = 10010;
const uint64_t kKeyboardMicId = 10011;
const uint64_t kFrontMicId = 10012;
const uint64_t kRearMicId = 10013;
const uint64_t kOtherTypeOutputId = 90001;
const uint64_t kOtherTypeInputId = 90002;
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
};

const AudioNodeInfo kInternalSpeaker[] = {
    {false, kInternalSpeakerId, "Fake Speaker", "INTERNAL_SPEAKER", "Speaker"}};

const AudioNodeInfo kHeadphone[] = {
    {false, kHeadphoneId, "Fake Headphone", "HEADPHONE", "Headphone"}};

const AudioNodeInfo kInternalMic[] = {
    {true, kInternalMicId, "Fake Mic", "INTERNAL_MIC", "Internal Mic"}};

const AudioNodeInfo kMicJack[] = {
    {true, kMicJackId, "Fake Mic Jack", "MIC", "Mic Jack"}};

const AudioNodeInfo kUSBMic1[] = {
    {true, kUSBMicId1, "Fake USB Mic 1", "USB", "USB Microphone 1"}};

const AudioNodeInfo kUSBMic2[] = {
    {true, kUSBMicId2, "Fake USB Mic 2", "USB", "USB Microphone 2"}};

const AudioNodeInfo kKeyboardMic[] = {{true, kKeyboardMicId,
                                       "Fake Keyboard Mic", "KEYBOARD_MIC",
                                       "Keyboard Mic"}};

const AudioNodeInfo kFrontMic[] = {
    {true, kFrontMicId, "Fake Front Mic", "FRONT_MIC", "Front Mic"}};

const AudioNodeInfo kRearMic[] = {
    {true, kRearMicId, "Fake Rear Mic", "REAR_MIC", "Rear Mic"}};

const AudioNodeInfo kOtherTypeOutput[] = {{false, kOtherTypeOutputId,
                                           "Output Device", "SOME_OTHER_TYPE",
                                           "Other Type Output Device"}};

const AudioNodeInfo kOtherTypeInput[] = {{true, kOtherTypeInputId,
                                          "Input Device", "SOME_OTHER_TYPE",
                                          "Other Type Input Device"}};

const AudioNodeInfo kBluetoothHeadset[] = {{false, kBluetoothHeadsetId,
                                            "Bluetooth Headset", "BLUETOOTH",
                                            "Bluetooth Headset 1"}};

const AudioNodeInfo kHDMIOutput[] = {
    {false, kHDMIOutputId, "HDMI output", "HDMI", "HDMI output"}};

const AudioNodeInfo kUSBHeadphone1[] = {
    {false, kUSBHeadphoneId1, "USB Headphone", "USB", "USB Headphone 1"}};

const AudioNodeInfo kUSBHeadphone2[] = {
    {false, kUSBHeadphoneId2, "USB Headphone", "USB", "USB Headphone 1"}};

const AudioNodeInfo kUSBJabraSpeakerOutput1[] = {
    {false, kUSBJabraSpeakerOutputId1, "Jabra Speaker 1", "USB",
     "Jabra Speaker 1"}};

const AudioNodeInfo kUSBJabraSpeakerOutput2[] = {
    {false, kUSBJabraSpeakerOutputId2, "Jabra Speaker 2", "USB",
     "Jabra Speaker 2"}};

const AudioNodeInfo kUSBJabraSpeakerInput1[] = {{true, kUSBJabraSpeakerInputId1,
                                                 "Jabra Speaker 1", "USB",
                                                 "Jabra Speaker"}};

const AudioNodeInfo kUSBJabraSpeakerInput2[] = {{true, kUSBJabraSpeakerInputId2,
                                                 "Jabra Speaker 2", "USB",
                                                 "Jabra Speaker 2"}};

const AudioNodeInfo kUSBCameraInput[] = {
    {true, kUSBCameraInputId, "USB Camera", "USB", "USB Camera"}};

class TestObserver : public chromeos::CrasAudioHandler::AudioObserver {
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

  int audio_nodes_changed_count() const {
    return audio_nodes_changed_count_;
  }

  int output_mute_changed_count() const {
    return output_mute_changed_count_;
  }

  void reset_output_mute_changed_count() { input_mute_changed_count_ = 0; }

  int input_mute_changed_count() const {
    return input_mute_changed_count_;
  }

  int output_volume_changed_count() const {
    return output_volume_changed_count_;
  }

  void reset_output_volume_changed_count() { output_volume_changed_count_ = 0; }

  int input_gain_changed_count() const {
    return input_gain_changed_count_;
  }

  bool output_mute_by_system() const { return output_mute_by_system_; }

  int output_channel_remixing_changed_count() const {
    return output_channel_remixing_changed_count_;
  }

  ~TestObserver() override = default;

 protected:
  // chromeos::CrasAudioHandler::AudioObserver overrides.
  void OnActiveOutputNodeChanged() override {
    ++active_output_node_changed_count_;
  }

  void OnActiveInputNodeChanged() override {
    ++active_input_node_changed_count_;
  }

  void OnAudioNodesChanged() override { ++audio_nodes_changed_count_; }

  void OnOutputMuteChanged(bool /* mute_on */, bool system_adjust) override {
    ++output_mute_changed_count_;
    output_mute_by_system_ = system_adjust;
  }

  void OnInputMuteChanged(bool /* mute_on */) override {
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

 private:
  int active_output_node_changed_count_ = 0;
  int active_input_node_changed_count_ = 0;
  int audio_nodes_changed_count_ = 0;
  int output_mute_changed_count_ = 0;
  int input_mute_changed_count_ = 0;
  int output_volume_changed_count_ = 0;
  int input_gain_changed_count_ = 0;
  bool output_mute_by_system_ = false;  // output mute state adjusted by system.
  int output_channel_remixing_changed_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class SystemMonitorObserver
    : public base::SystemMonitor::DevicesChangedObserver {
 public:
  SystemMonitorObserver() : device_changes_received_(0) {}
  ~SystemMonitorObserver() override = default;

  void OnDevicesChanged(base::SystemMonitor::DeviceType device_type) override {
    if (device_type == base::SystemMonitor::DeviceType::DEVTYPE_AUDIO)
      device_changes_received_++;
  }

  int device_changes_received() const { return device_changes_received_; }

  void reset_count() { device_changes_received_ = 0; }

 private:
  int device_changes_received_;
};

class FakeVideoCaptureManager {
 public:
  FakeVideoCaptureManager() = default;
  virtual ~FakeVideoCaptureManager() = default;

  void AddObserver(media::VideoCaptureObserver* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveAllObservers() { observers_.Clear(); }

  void NotifyVideoCaptureStarted(media::VideoFacingMode facing) {
    for (auto& observer : observers_)
      observer.OnVideoCaptureStarted(facing);
  }

  void NotifyVideoCaptureStopped(media::VideoFacingMode facing) {
    for (auto& observer : observers_)
      observer.OnVideoCaptureStopped(facing);
  }

 private:
  base::ObserverList<media::VideoCaptureObserver>::Unchecked observers_;
  DISALLOW_COPY_AND_ASSIGN(FakeVideoCaptureManager);
};

}  // namespace

// Test param is the version of stabel device id used by audio node.
class CrasAudioHandlerTest : public testing::TestWithParam<int> {
 public:
  CrasAudioHandlerTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}
  ~CrasAudioHandlerTest() override = default;

  void SetUp() override {
    system_monitor_.AddDevicesChangedObserver(&system_monitor_observer_);
    video_capture_manager_.reset(new FakeVideoCaptureManager);
  }

  void TearDown() override {
    system_monitor_.RemoveDevicesChangedObserver(&system_monitor_observer_);
    cras_audio_handler_->RemoveAudioObserver(test_observer_.get());
    test_observer_.reset();
    video_capture_manager_->RemoveAllObservers();
    video_capture_manager_.reset();
    CrasAudioHandler::Shutdown();
    audio_pref_handler_ = nullptr;
    DBusThreadManager::Shutdown();
  }

  AudioNode GenerateAudioNode(const AudioNodeInfo* node_info) {
    uint64_t stable_device_id_v2 = GetParam() == 1 ? 0 : (node_info->id ^ 0xFF);
    uint64_t stable_device_id_v1 = node_info->id;
    return AudioNode(node_info->is_input, node_info->id, GetParam() == 2,
                     stable_device_id_v1, stable_device_id_v2,
                     node_info->device_name, node_info->type, node_info->name,
                     false /* is_active*/, 0 /* pluged_time */);
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
    DBusThreadManager::Initialize();
    fake_cras_audio_client_ = static_cast<FakeCrasAudioClient*>(
        DBusThreadManager::Get()->GetCrasAudioClient());
    fake_cras_audio_client_->SetAudioNodesForTesting(audio_nodes);
    audio_pref_handler_ = new AudioDevicesPrefHandlerStub();
    CrasAudioHandler::Initialize(audio_pref_handler_);
    cras_audio_handler_ = CrasAudioHandler::Get();
    test_observer_.reset(new TestObserver);
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
    DBusThreadManager::Initialize();
    fake_cras_audio_client_ = static_cast<FakeCrasAudioClient*>(
        DBusThreadManager::Get()->GetCrasAudioClient());
    audio_pref_handler_ = new AudioDevicesPrefHandlerStub();
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

    fake_cras_audio_client_->SetAudioNodesForTesting(audio_nodes);
    CrasAudioHandler::Initialize(audio_pref_handler_);

    cras_audio_handler_ = CrasAudioHandler::Get();
    test_observer_.reset(new TestObserver);
    cras_audio_handler_->AddAudioObserver(test_observer_.get());
    base::RunLoop().RunUntilIdle();
  }

  void SetUpCrasAudioHandlerWithPrimaryActiveNode(
      const AudioNodeList& audio_nodes,
      const AudioNode& primary_active_node) {
    DBusThreadManager::Initialize();
    fake_cras_audio_client_ = static_cast<FakeCrasAudioClient*>(
        DBusThreadManager::Get()->GetCrasAudioClient());
    fake_cras_audio_client_->SetAudioNodesForTesting(audio_nodes);
    fake_cras_audio_client_->SetActiveOutputNode(primary_active_node.id);
    audio_pref_handler_ = new AudioDevicesPrefHandlerStub();
    CrasAudioHandler::Initialize(audio_pref_handler_);
    cras_audio_handler_ = CrasAudioHandler::Get();
    test_observer_.reset(new TestObserver);
    cras_audio_handler_->AddAudioObserver(test_observer_.get());
    base::RunLoop().RunUntilIdle();
  }

  void ChangeAudioNodes(const AudioNodeList& audio_nodes) {
    fake_cras_audio_client_->SetAudioNodesAndNotifyObserversForTesting(
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
      if (audio_devices[i].active)
        ++num_active_nodes;
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

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::SystemMonitor system_monitor_;
  SystemMonitorObserver system_monitor_observer_;
  CrasAudioHandler* cras_audio_handler_ = nullptr;         // Not owned.
  FakeCrasAudioClient* fake_cras_audio_client_ = nullptr;  // Not owned.
  std::unique_ptr<TestObserver> test_observer_;
  scoped_refptr<AudioDevicesPrefHandlerStub> audio_pref_handler_;
  std::unique_ptr<FakeVideoCaptureManager> video_capture_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrasAudioHandlerTest);
};

class HDMIRediscoverWaiter {
 public:
  HDMIRediscoverWaiter(CrasAudioHandlerTest* cras_audio_handler_test,
                       int grace_period_duration_in_ms)
      : cras_audio_handler_test_(cras_audio_handler_test),
        grace_period_duration_in_ms_(grace_period_duration_in_ms) {}

  void WaitUntilTimeOut(int wait_duration_in_ms) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(wait_duration_in_ms));
    run_loop.Run();
  }

  void CheckHDMIRediscoverGracePeriodEnd(const base::Closure& quit_loop_func) {
    if (!cras_audio_handler_test_->IsDuringHDMIRediscoverGracePeriod()) {
      quit_loop_func.Run();
      return;
    }
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&HDMIRediscoverWaiter::CheckHDMIRediscoverGracePeriodEnd,
                       base::Unretained(this), quit_loop_func),
        base::TimeDelta::FromMilliseconds(grace_period_duration_in_ms_ / 4));
  }

  void WaitUntilHDMIRediscoverGracePeriodEnd() {
    base::RunLoop run_loop;
    CheckHDMIRediscoverGracePeriodEnd(run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  CrasAudioHandlerTest* cras_audio_handler_test_;  // not owned
  int grace_period_duration_in_ms_;

  DISALLOW_COPY_AND_ASSIGN(HDMIRediscoverWaiter);
};

INSTANTIATE_TEST_CASE_P(StableIdV1, CrasAudioHandlerTest, testing::Values(1));
INSTANTIATE_TEST_CASE_P(StableIdV2, CrasAudioHandlerTest, testing::Values(2));

TEST_P(CrasAudioHandlerTest, InitializeWithOnlyDefaultAudioDevices) {
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kInternalMic});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the internal speaker has been selected as the active output.
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_output());

  // Ensure the internal microphone has been selected as the active input.
  AudioDevice active_input;
  EXPECT_EQ(kInternalMic->id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_input());
}

TEST_P(CrasAudioHandlerTest, InitializeWithAlternativeAudioDevices) {
  AudioNodeList audio_nodes = GenerateAudioNodeList(
      {kInternalSpeaker, kHeadphone, kInternalMic, kUSBMic1});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the headphone has been selected as the active output.
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHeadphone->id, active_output.id);
  EXPECT_EQ(kHeadphone->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Ensure the USB microphone has been selected as the active input.
  EXPECT_EQ(kUSBMicId1, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());
}

TEST_P(CrasAudioHandlerTest, InitializeWithKeyboardMic) {
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kInternalMic, kKeyboardMic});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_TRUE(cras_audio_handler_->HasKeyboardMic());

  // Verify the internal speaker has been selected as the active output.
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_output());

  // Ensure the internal microphone has been selected as the active input,
  // not affected by keyboard mic.
  AudioDevice active_input;
  EXPECT_EQ(kInternalMic->id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_input());
  const AudioDevice* keyboard_mic = GetDeviceFromId(kKeyboardMic->id);
  EXPECT_FALSE(keyboard_mic->active);
}

TEST_P(CrasAudioHandlerTest, SetKeyboardMicActive) {
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalMic, kKeyboardMic});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_TRUE(cras_audio_handler_->HasKeyboardMic());

  // Ensure the internal microphone has been selected as the active input,
  // not affected by keyboard mic.
  AudioDevice active_input;
  EXPECT_EQ(kInternalMic->id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_input());
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
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kHeadphone});
  SetUpCrasAudioHandler(audio_nodes);

  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  system_monitor_observer_.reset_count();

  // Verify the initial active output device is headphone.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHeadphone->id, active_output.id);
  EXPECT_EQ(kHeadphone->id, cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Switch the active output to internal speaker.
  AudioDevice internal_speaker(GenerateAudioNode(kInternalSpeaker));
  cras_audio_handler_->SwitchToDevice(internal_speaker, true,
                                      CrasAudioHandler::ACTIVATE_BY_USER);

  // Verify the active output is switched to internal speaker, and the
  // ActiveOutputNodeChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  VerifySystemMonitorWasCalled();
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
}

TEST_P(CrasAudioHandlerTest, SwitchActiveInputDevice) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalMic, kUSBMic1});
  SetUpCrasAudioHandler(audio_nodes);

  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the initial active input device is USB mic.
  EXPECT_EQ(0, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kUSBMicId1, cras_audio_handler_->GetPrimaryActiveInputNode());

  // Switch the active input to internal mic.
  AudioDevice internal_mic(GenerateAudioNode(kInternalMic));
  cras_audio_handler_->SwitchToDevice(internal_mic, true,
                                      CrasAudioHandler::ACTIVATE_BY_USER);

  // Verify the active output is switched to internal speaker, and the active
  // ActiveInputNodeChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kInternalMic->id, cras_audio_handler_->GetPrimaryActiveInputNode());
}

TEST_P(CrasAudioHandlerTest, PlugHeadphone) {
  // Set up initial audio devices, only with internal speaker.
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalSpeaker});
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the internal speaker has been selected as the active output.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_output());
  system_monitor_observer_.reset_count();

  // Plug the headphone.
  audio_nodes.clear();
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);
  audio_nodes.push_back(GenerateAudioNode(kHeadphone));
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and new audio device is added.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  VerifySystemMonitorWasCalled();
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size + 1, audio_devices.size());

  // Verify the active output device is switched to headphone and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHeadphone->id, active_output.id);
  EXPECT_EQ(kHeadphone->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());
}

TEST_P(CrasAudioHandlerTest, UnplugHeadphone) {
  // Set up initial audio devices, with internal speaker and headphone.
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kHeadphone});
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the headphone has been selected as the active output.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHeadphone->id, active_output.id);
  EXPECT_EQ(kHeadphone->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());
  system_monitor_observer_.reset_count();

  // Unplug the headphone.
  audio_nodes.clear();
  audio_nodes.push_back(GenerateAudioNode(kInternalSpeaker));
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and one audio device is
  // removed.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  VerifySystemMonitorWasCalled();
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size - 1, audio_devices.size());

  // Verify the active output device is switched to internal speaker and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_output());
}

TEST_P(CrasAudioHandlerTest, InitializeWithBluetoothHeadset) {
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kBluetoothHeadset});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the bluetooth headset has been selected as the active output.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kBluetoothHeadset->id, active_output.id);
  EXPECT_EQ(kBluetoothHeadset->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());
}

TEST_P(CrasAudioHandlerTest, ConnectAndDisconnectBluetoothHeadset) {
  // Initialize with internal speaker and headphone.
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kHeadphone});
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the headphone is selected as the active output initially.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHeadphone->id, active_output.id);
  EXPECT_EQ(kHeadphone->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Connect to bluetooth headset. Since it is plugged in later than
  // headphone, active output should be switched to it.
  system_monitor_observer_.reset_count();
  audio_nodes.clear();
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
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size + 1, audio_devices.size());

  // Verify the active output device is switched to bluetooth headset, and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kBluetoothHeadset->id, active_output.id);
  EXPECT_EQ(kBluetoothHeadset->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

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
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the active output device is switched to headphone, and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(2, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHeadphone->id, active_output.id);
  EXPECT_EQ(kHeadphone->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());
}

TEST_P(CrasAudioHandlerTest, InitializeWithHDMIOutput) {
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kHDMIOutput});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the HDMI device has been selected as the active output.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHDMIOutput->id, active_output.id);
  EXPECT_EQ(kHDMIOutput->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());
}

TEST_P(CrasAudioHandlerTest, ConnectAndDisconnectHDMIOutput) {
  // Initialize with internal speaker.
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalSpeaker});
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the internal speaker is selected as the active output initially.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_output());
  system_monitor_observer_.reset_count();

  // Connect to HDMI output.
  audio_nodes.clear();
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
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size + 1, audio_devices.size());

  // Verify the active output device is switched to hdmi output, and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHDMIOutput->id, active_output.id);
  EXPECT_EQ(kHDMIOutput->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());
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
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the active output device is switched to internal speaker, and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(2, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_output());
}

TEST_P(CrasAudioHandlerTest, HandleHeadphoneAndHDMIOutput) {
  // Initialize with internal speaker, headphone and HDMI output.
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kHeadphone, kHDMIOutput});
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the headphone is selected as the active output initially.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHeadphone->id, active_output.id);
  EXPECT_EQ(kHeadphone->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Disconnect HDMI output.
  audio_nodes.clear();
  audio_nodes.push_back(GenerateAudioNode(kInternalSpeaker));
  audio_nodes.push_back(GenerateAudioNode(kHDMIOutput));
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and one audio device is
  // removed.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size - 1, audio_devices.size());

  // Verify the active output device is switched to HDMI output, and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHDMIOutput->id, active_output.id);
  EXPECT_EQ(kHDMIOutput->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());
}

TEST_P(CrasAudioHandlerTest, InitializeWithUSBHeadphone) {
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kUSBHeadphone1});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the usb headphone has been selected as the active output.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kUSBHeadphone1->id, active_output.id);
  EXPECT_EQ(kUSBHeadphone1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());
}

TEST_P(CrasAudioHandlerTest, PlugAndUnplugUSBHeadphone) {
  // Initialize with internal speaker.
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalSpeaker});
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the internal speaker is selected as the active output initially.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_output());
  system_monitor_observer_.reset_count();

  // Plug in usb headphone
  audio_nodes.clear();
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
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size + 1, audio_devices.size());

  // Verify the active output device is switched to usb headphone, and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kUSBHeadphone1->id, active_output.id);
  EXPECT_EQ(kUSBHeadphone1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());
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
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the active output device is switched to internal speaker, and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(2, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_output());
}

TEST_P(CrasAudioHandlerTest, HandleMultipleUSBHeadphones) {
  // Initialize with internal speaker and one usb headphone.
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kUSBHeadphone1});
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the usb headphone is selected as the active output initially.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kUSBHeadphone1->id, active_output.id);
  EXPECT_EQ(kUSBHeadphone1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Plug in another usb headphone.
  audio_nodes.clear();
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
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size + 1, audio_devices.size());

  // Verify the active output device is switched to the 2nd usb headphone, which
  // is plugged later, and ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kUSBHeadphone2->id, active_output.id);
  EXPECT_EQ(kUSBHeadphone2->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Unplug the 2nd usb headphone.
  audio_nodes.clear();
  audio_nodes.push_back(GenerateAudioNode(kInternalSpeaker));
  audio_nodes.push_back(GenerateAudioNode(kUSBHeadphone1));
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and one audio device is
  // removed.
  EXPECT_EQ(2, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the active output device is switched to the first usb headphone, and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(2, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kUSBHeadphone1->id, active_output.id);
  EXPECT_EQ(kUSBHeadphone1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());
}

TEST_P(CrasAudioHandlerTest, UnplugUSBHeadphonesWithActiveSpeaker) {
  // Initialize with internal speaker and one usb headphone.
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kUSBHeadphone1});
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the usb headphone is selected as the active output initially.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kUSBHeadphone1->id, active_output.id);
  EXPECT_EQ(kUSBHeadphone1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Plug in the headphone jack.
  audio_nodes.clear();
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
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size + 1, audio_devices.size());

  // Verify the active output device is switched to the headphone jack, which
  // is plugged later, and ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHeadphone->id, active_output.id);
  EXPECT_EQ(kHeadphone->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Select the speaker to be the active output device.
  AudioDevice internal_speaker(GenerateAudioNode(kInternalSpeaker));
  cras_audio_handler_->SwitchToDevice(internal_speaker, true,
                                      CrasAudioHandler::ACTIVATE_BY_USER);

  // Verify the active output is switched to internal speaker, and the
  // ActiveOutputNodeChanged event is fired.
  EXPECT_EQ(2, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

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
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the active output device remains to be speaker.
  EXPECT_EQ(2, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());
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
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHeadphone->id, active_output.id);
  EXPECT_EQ(kHeadphone->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());
  for (size_t i = 0; i < audio_devices.size(); ++i) {
    if (audio_devices[i].id != kHeadphone->id)
      EXPECT_FALSE(audio_devices[i].active);
  }
}

TEST_P(CrasAudioHandlerTest, BluetoothSpeakerIdChangedOnFly) {
  // Initialize with internal speaker and bluetooth headset.
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kBluetoothHeadset});
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the bluetooth headset is selected as the active output and all other
  // audio devices are not active.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kBluetoothHeadset->id, active_output.id);
  EXPECT_EQ(kBluetoothHeadset->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Cras changes the bluetooth headset's id on the fly.
  audio_nodes.clear();
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
  audio_devices.clear();
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());

  // Verify ActiveOutputNodeChanged event is fired, and active device should be
  // bluetooth headphone.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(bluetooth_headphone.id, active_output.id);
}

TEST_P(CrasAudioHandlerTest, PlugUSBMic) {
  // Set up initial audio devices, only with internal mic.
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalMic});
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the internal mic is selected as the active input.
  EXPECT_EQ(0, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kInternalMic->id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_input());
  system_monitor_observer_.reset_count();

  // Plug the USB Mic.
  audio_nodes.clear();
  AudioNode internal_mic(GenerateAudioNode(kInternalMic));
  internal_mic.active = true;
  audio_nodes.push_back(internal_mic);
  audio_nodes.push_back(GenerateAudioNode(kUSBMic1));
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and new audio device is added.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  VerifySystemMonitorWasCalled();
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size + 1, audio_devices.size());

  // Verify the active input device is switched to USB mic and
  // and ActiveInputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kUSBMicId1, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());
}

TEST_P(CrasAudioHandlerTest, UnplugUSBMic) {
  // Set up initial audio devices, with internal mic and USB Mic.
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalMic, kUSBMic1});
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the USB mic is selected as the active output.
  EXPECT_EQ(0, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kUSBMicId1, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());
  system_monitor_observer_.reset_count();

  // Unplug the USB Mic.
  audio_nodes.clear();
  audio_nodes.push_back(GenerateAudioNode(kInternalMic));
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired, and one audio device is
  // removed.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  VerifySystemMonitorWasCalled();
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size - 1, audio_devices.size());

  // Verify the active input device is switched to internal mic, and
  // and ActiveInputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kInternalMic->id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_input());
}

TEST_P(CrasAudioHandlerTest, PlugUSBMicNotAffectActiveOutput) {
  // Set up initial audio devices.
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kHeadphone, kInternalMic});
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the internal mic is selected as the active input.
  EXPECT_EQ(0, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kInternalMicId, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_input());

  // Verify the headphone is selected as the active output.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  EXPECT_EQ(kHeadphoneId, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Switch the active output to internal speaker.
  AudioDevice internal_speaker(GenerateAudioNode(kInternalSpeaker));
  cras_audio_handler_->SwitchToDevice(internal_speaker, true,
                                      CrasAudioHandler::ACTIVATE_BY_USER);

  // Verify the active output is switched to internal speaker, and the
  // ActiveOutputNodeChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Plug the USB Mic.
  audio_nodes.clear();
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
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size + 1, audio_devices.size());

  // Verify the active input device is switched to USB mic, and
  // and ActiveInputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kUSBMic1->id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());

  // Verify the active output device is not changed.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
}

TEST_P(CrasAudioHandlerTest, PlugHeadphoneAutoUnplugSpeakerWithActiveUSB) {
  // Set up initial audio devices.
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kUSBHeadphone1, kInternalSpeaker, kInternalMic});
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the internal mic is selected as the active input.
  EXPECT_EQ(0, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kInternalMicId, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_input());

  // Verify the USB headphone is selected as the active output.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  EXPECT_EQ(kUSBHeadphoneId1,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Plug the headphone and auto-unplug internal speaker.
  audio_nodes.clear();
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
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the active output device is switched to headphone, and
  // an ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_EQ(kHeadphone->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

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
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the active output device is switched back to USB, and
  // an ActiveOutputChanged event is fired.
  EXPECT_EQ(2, test_observer_->active_output_node_changed_count());
  EXPECT_EQ(kUSBHeadphone1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Verify the active input device is not changed.
  EXPECT_EQ(0, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kInternalMic->id, cras_audio_handler_->GetPrimaryActiveInputNode());
}

TEST_P(CrasAudioHandlerTest, PlugMicAutoUnplugInternalMicWithActiveUSB) {
  // Set up initial audio devices.
  AudioNodeList audio_nodes = GenerateAudioNodeList(
      {kUSBHeadphone1, kInternalSpeaker, kUSBMic1, kInternalMic});
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the internal mic is selected as the active input.
  EXPECT_EQ(0, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kUSBMicId1, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());

  // Verify the internal speaker is selected as the active output.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  EXPECT_EQ(kUSBHeadphoneId1,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Plug the headphone and mic, auto-unplug internal mic and speaker.
  audio_nodes.clear();
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
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the active output device is switched to headphone, and
  // an ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_EQ(kHeadphone->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

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
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the active output device is switched back to USB, and
  // an ActiveOutputChanged event is fired.
  EXPECT_EQ(2, test_observer_->active_output_node_changed_count());
  EXPECT_EQ(kUSBHeadphone1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Verify the active input device is switched back to USB mic, and
  // an ActiveInputChanged event is fired.
  EXPECT_EQ(2, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kUSBMic1->id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());
}

TEST_P(CrasAudioHandlerTest, MultipleNodesChangedSignalsOnPlugInHeadphone) {
  // Set up initial audio devices.
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kBluetoothHeadset});
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the bluetooth headset is selected as the active output.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  EXPECT_EQ(kBluetoothHeadsetId,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Plug in headphone, but fire NodesChanged signal twice.
  audio_nodes.clear();
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
  EXPECT_EQ(headphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(headphone.id, active_output.id);

  // Verfiy the audio devices data is consistent, i.e., the active output device
  // should be headphone.
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size + 1, audio_devices.size());
  for (size_t i = 0; i < audio_devices.size(); ++i) {
    if (audio_devices[i].id == kInternalSpeaker->id)
      EXPECT_FALSE(audio_devices[i].active);
    else if (audio_devices[i].id == bluetooth_headset.id)
      EXPECT_FALSE(audio_devices[i].active);
    else if (audio_devices[i].id == headphone.id)
      EXPECT_TRUE(audio_devices[i].active);
    else
      NOTREACHED();
  }
}

TEST_P(CrasAudioHandlerTest, MultipleNodesChangedSignalsOnPlugInUSBMic) {
  // Set up initial audio devices.
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalMic});
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the internal mic is selected as the active output.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  EXPECT_EQ(kInternalMic->id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_output());
  EXPECT_TRUE(audio_devices[0].active);

  // Plug in usb mic, but fire NodesChanged signal twice.
  audio_nodes.clear();
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

  // Verfiy the audio devices data is consistent, i.e., the active input device
  // should be usb mic.
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size + 1, audio_devices.size());
  for (size_t i = 0; i < audio_devices.size(); ++i) {
    if (audio_devices[i].id == kInternalMic->id)
      EXPECT_FALSE(audio_devices[i].active);
    else if (audio_devices[i].id == usb_mic.id)
      EXPECT_TRUE(audio_devices[i].active);
    else
      NOTREACHED();
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
  EXPECT_EQ(headphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(headphone.id, active_output.id);

  // Verify the active input device id is set to internal mic.
  EXPECT_EQ(internal_mic.id, cras_audio_handler_->GetPrimaryActiveInputNode());

  // Verfiy the audio devices data is consistent, i.e., the active output device
  // should be headphone, and the active input device should internal mic.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  for (size_t i = 0; i < audio_devices.size(); ++i) {
    if (audio_devices[i].id == internal_speaker.id)
      EXPECT_FALSE(audio_devices[i].active);
    else if (audio_devices[i].id == headphone.id)
      EXPECT_TRUE(audio_devices[i].active);
    else if (audio_devices[i].id == internal_mic.id)
      EXPECT_TRUE(audio_devices[i].active);
    else
      NOTREACHED();
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
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHeadphone->id, active_output.id);
  EXPECT_EQ(kHeadphone->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(active_output.active);
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

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
  EXPECT_EQ(internal_speaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(internal_speaker.id, active_output.id);
  EXPECT_TRUE(active_output.active);

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

TEST_P(CrasAudioHandlerTest, SetInputMute) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalMic});
  SetUpCrasAudioHandler(audio_nodes);
  EXPECT_EQ(0, test_observer_->input_mute_changed_count());

  // Mute the device.
  cras_audio_handler_->SetInputMute(true);

  // Verify the input is muted, OnInputMuteChanged event is fired.
  EXPECT_TRUE(cras_audio_handler_->IsInputMuted());
  EXPECT_EQ(1, test_observer_->input_mute_changed_count());

  // Unmute the device.
  cras_audio_handler_->SetInputMute(false);

  // Verify the input is unmuted, OnInputMuteChanged event is fired.
  EXPECT_FALSE(cras_audio_handler_->IsInputMuted());
  EXPECT_EQ(2, test_observer_->input_mute_changed_count());
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

TEST_P(CrasAudioHandlerTest, RestartAudioClientWithCrasReady) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalSpeaker});
  SetUpCrasAudioHandler(audio_nodes);
  EXPECT_EQ(0, test_observer_->output_volume_changed_count());

  const int kDefaultVolume = cras_audio_handler_->GetOutputVolumePercent();
  // Disable the auto OutputNodeVolumeChanged signal.
  fake_cras_audio_client_->set_notify_volume_change_with_delay(true);

  fake_cras_audio_client_->SetAudioNodesForTesting(audio_nodes);
  RestartAudioClient();
  EXPECT_EQ(0, test_observer_->output_volume_changed_count());
  EXPECT_EQ(kDefaultVolume, cras_audio_handler_->GetOutputVolumePercent());

  // The correct initialization OutputNodeVolumeChanged event is fired. We
  // should avoid notifying observers.
  fake_cras_audio_client_->NotifyOutputNodeVolumeChangedForTesting(
      kInternalSpeaker->id, kDefaultVolume);
  EXPECT_EQ(0, test_observer_->output_volume_changed_count());
  EXPECT_EQ(kDefaultVolume, cras_audio_handler_->GetOutputVolumePercent());

  // The later OutputNodeVolumeChanged event after initialization should notify
  // observers.
  const int kVolume = 60;
  fake_cras_audio_client_->NotifyOutputNodeVolumeChangedForTesting(
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
  fake_cras_audio_client_->set_notify_volume_change_with_delay(true);

  fake_cras_audio_client_->SetAudioNodesForTesting(audio_nodes);
  RestartAudioClient();
  EXPECT_EQ(0, test_observer_->output_volume_changed_count());
  EXPECT_EQ(kDefaultVolume, cras_audio_handler_->GetOutputVolumePercent());

  // A wrong initialization OutputNodeVolumeChanged event is fired. This may
  // happen when Cras is not ready and drops request. The approach we use is
  // to log warning message, clear the pending automated volume change reasons,
  // and notify observers about this change.
  const int kVolume1 = 30;
  fake_cras_audio_client_->NotifyOutputNodeVolumeChangedForTesting(
      kInternalSpeaker->id, kVolume1);
  EXPECT_EQ(1, test_observer_->output_volume_changed_count());
  EXPECT_EQ(kVolume1, cras_audio_handler_->GetOutputVolumePercent());

  // The later OutputNodeVolumeChanged event should notify observers.
  const int kVolume2 = 60;
  fake_cras_audio_client_->NotifyOutputNodeVolumeChangedForTesting(
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
  fake_cras_audio_client_->set_notify_volume_change_with_delay(true);

  // Verify the volume state is not changed before OutputNodeVolumeChanged
  // signal fires.
  const int kVolume = 60;
  cras_audio_handler_->SetOutputVolumePercent(kVolume);
  EXPECT_EQ(0, test_observer_->output_volume_changed_count());
  EXPECT_EQ(kDefaultVolume, cras_audio_handler_->GetOutputVolumePercent());

  // Verify the output volume is changed to the designated value after
  // OnOutputNodeVolumeChanged cras signal fires, and the volume change event
  // has been fired to notify the observers.
  fake_cras_audio_client_->NotifyOutputNodeVolumeChangedForTesting(
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
  fake_cras_audio_client_->set_notify_volume_change_with_delay(true);

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
  fake_cras_audio_client_->NotifyOutputNodeVolumeChangedForTesting(
      kInternalSpeaker->id, kVolume1);
  EXPECT_EQ(1, test_observer_->output_volume_changed_count());
  EXPECT_EQ(kVolume1, cras_audio_handler_->GetOutputVolumePercent());

  fake_cras_audio_client_->NotifyOutputNodeVolumeChangedForTesting(
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
  fake_cras_audio_client_->NotifyOutputNodeVolumeChangedForTesting(
      kInternalSpeaker->id, kVolume);
  EXPECT_EQ(1, test_observer_->output_volume_changed_count());
  EXPECT_EQ(kVolume, cras_audio_handler_->GetOutputVolumePercent());
  AudioDevice device;
  EXPECT_TRUE(cras_audio_handler_->GetPrimaryActiveOutputDevice(&device));
  EXPECT_EQ(device.id, kInternalSpeaker->id);
  EXPECT_EQ(kVolume, audio_pref_handler_->GetOutputVolumeValue(&device));
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

TEST_P(CrasAudioHandlerTest, SetMuteForDevice) {
  AudioNodeList audio_nodes = GenerateAudioNodeList(
      {kInternalSpeaker, kHeadphone, kInternalMic, kUSBMic1});
  SetUpCrasAudioHandler(audio_nodes);

  // Mute the active output device.
  EXPECT_EQ(kHeadphone->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
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
  EXPECT_EQ(kHeadphone->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
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
  EXPECT_EQ(kUSBMicGain,
            audio_pref_handler_->GetInputGainValue(&usb_mic));

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

TEST_P(CrasAudioHandlerTest, HandleOtherDeviceType) {
  const size_t kNumValidAudioDevices = 4;
  AudioNodeList audio_nodes = GenerateAudioNodeList(
      {kInternalSpeaker, kOtherTypeOutput, kInternalMic, kOtherTypeInput});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(kNumValidAudioDevices, audio_devices.size());

  // Verify the internal speaker has been selected as the active output,
  // and the output device with some randown unknown type is handled gracefully.
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Ensure the internal microphone has been selected as the active input,
  // and the input device with some random unknown type is handled gracefully.
  AudioDevice active_input;
  EXPECT_EQ(kInternalMic->id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());
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
  EXPECT_EQ(kUSBHeadphone1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Change the active device to internal speaker, now USB headphone becomes
  // inactive.
  AudioDevice speaker(GenerateAudioNode(kInternalSpeaker));
  cras_audio_handler_->SwitchToDevice(speaker, true,
                                      CrasAudioHandler::ACTIVATE_BY_USER);
  EXPECT_NE(kUSBHeadphone1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Unplug USB headset.
  audio_nodes.clear();
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);
  ChangeAudioNodes(audio_nodes);
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Plug the same USB headset back, id is different, but stable_device_id
  // remains the same.
  usb_headset.active = false;
  usb_headset.id = 98765;
  audio_nodes.push_back(usb_headset);
  ChangeAudioNodes(audio_nodes);

  // Since USB headset was inactive before it was unplugged, it won't be
  // selected as active after it's plugged in again.
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Plug the second USB headset.
  AudioNode usb_headset2 = GenerateAudioNode(kUSBHeadphone2);
  usb_headset2.plugged_time = 80000001;
  audio_nodes.push_back(usb_headset2);
  ChangeAudioNodes(audio_nodes);

  // Since the second USB device is new, it's selected as the active device
  // by its priority.
  EXPECT_EQ(kUSBHeadphone2->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Unplug the second USB headset.
  audio_nodes.clear();
  internal_speaker.active = false;
  audio_nodes.push_back(internal_speaker);
  audio_nodes.push_back(usb_headset);
  ChangeAudioNodes(audio_nodes);

  // There is no active node after USB2 unplugged, the 1st USB got selected
  // by its priority.
  EXPECT_EQ(usb_headset.id, cras_audio_handler_->GetPrimaryActiveOutputNode());

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
  EXPECT_EQ(usb_headset2.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
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
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
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
  EXPECT_EQ(kUSBHeadphone1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Simulate another NodesChanged signal coming later with all ndoes.
  audio_nodes.push_back(GenerateAudioNode(kInternalSpeaker));
  audio_nodes.push_back(GenerateAudioNode(kHeadphone));
  ChangeAudioNodes(audio_nodes);

  // Verify the active output has been restored to internal speaker.
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
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
       PersistActiveUsbHeadphoneAcrossRebootUsbComeLater) {
  // Simulates the device was shut down with three audio devices, and
  // usb headphone being the active one selected by priority.
  AudioNodeList audio_nodes_in_pref =
      GenerateAudioNodeList({kInternalSpeaker, kHeadphone, kUSBHeadphone1});

  // Simulate the first NodesChanged signal coming with only internal speaker
  // and the headphone.
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kHeadphone});

  SetupCrasAudioHandlerWithActiveNodeInPref(
      audio_nodes, audio_nodes_in_pref,
      AudioDevice(GenerateAudioNode(kUSBHeadphone1)), false);

  // Verify the headphone has been made active.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_EQ(kHeadphone->id, cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Simulate USB node comes later with all ndoes.
  AudioNode usb_node = GenerateAudioNode(kUSBHeadphone1);
  usb_node.plugged_time = 80000000;
  audio_nodes.push_back(usb_node);
  ChangeAudioNodes(audio_nodes);

  // Verify the active output has been restored to usb headphone.
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_EQ(kUSBHeadphone1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
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
      CrasAudioHandler::ACTIVATE_BY_USER);

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
  for (const auto& node : audio_nodes)
    ASSERT_FALSE(node.active) << node.id << " expexted to be inactive";
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
      CrasAudioHandler::ACTIVATE_BY_USER);

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
  AudioNodeList audio_nodes = GenerateAudioNodeList(
      {kHDMIOutput, kUSBJabraSpeakerOutput1, kUSBJabraSpeakerOutput2,
       kUSBJabraSpeakerInput1, kUSBJabraSpeakerInput2, kUSBCameraInput});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify only the 1st jabra speaker's output and input are selected as active
  // nodes by CrasAudioHandler.
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(2, GetActiveDeviceCount());
  AudioDevice primary_active_device;
  EXPECT_TRUE(cras_audio_handler_->GetPrimaryActiveOutputDevice(
      &primary_active_device));
  EXPECT_EQ(kUSBJabraSpeakerOutput1->id, primary_active_device.id);
  EXPECT_EQ(kUSBJabraSpeakerInput1->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

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
  AudioNodeList audio_nodes = GenerateAudioNodeList(
      {kHDMIOutput, kUSBJabraSpeakerOutput1, kUSBJabraSpeakerOutput2,
       kUSBJabraSpeakerInput1, kUSBJabraSpeakerInput2, kUSBCameraInput});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify only the 1st jabra speaker's output and input are selected as active
  // nodes by CrasAudioHandler.
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(2, GetActiveDeviceCount());
  AudioDevice primary_active_device;
  EXPECT_TRUE(cras_audio_handler_->GetPrimaryActiveOutputDevice(
      &primary_active_device));
  EXPECT_EQ(kUSBJabraSpeakerOutput1->id, primary_active_device.id);
  EXPECT_EQ(kUSBJabraSpeakerInput1->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

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
  fake_cras_audio_client_->set_notify_volume_change_with_delay(true);
  test_observer_->reset_output_volume_changed_count();

  // Adjust the volume of output devices continuously.
  cras_audio_handler_->SetOutputVolumePercent(20);
  cras_audio_handler_->SetOutputVolumePercent(30);

  // Sends delayed OutputNodeVolumeChanged signals.
  fake_cras_audio_client_->NotifyOutputNodeVolumeChangedForTesting(
      kUSBJabraSpeakerOutput2->id, 20);
  fake_cras_audio_client_->NotifyOutputNodeVolumeChangedForTesting(
      kUSBJabraSpeakerOutput1->id, 20);
  fake_cras_audio_client_->NotifyOutputNodeVolumeChangedForTesting(
      kUSBJabraSpeakerOutput2->id, 30);
  fake_cras_audio_client_->NotifyOutputNodeVolumeChangedForTesting(
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
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kHDMIOutput, kUSBJabraSpeakerOutput1,
                             kUSBJabraSpeakerInput1, kUSBCameraInput});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the jabra speaker's output and input are selected as active nodes
  // by CrasAudioHandler.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerOutput1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBJabraSpeakerInput1->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());
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
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kHDMIOutput, kUSBJabraSpeakerOutput1,
                             kUSBJabraSpeakerInput1, kUSBCameraInput});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the jabra speaker's output and input are selected as active output
  // by CrasAudioHandler.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerOutput1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBJabraSpeakerInput1->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

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
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kHDMIOutput, kUSBJabraSpeakerOutput1,
                             kUSBJabraSpeakerInput1, kUSBCameraInput});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the jabra speaker's output and input are selected as active output
  // by CrasAudioHandler.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerOutput1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBJabraSpeakerInput1->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

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
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kHDMIOutput, kUSBJabraSpeakerOutput1,
                             kUSBJabraSpeakerInput1, kUSBCameraInput});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the jabra speaker's output and input are selected as active output
  // by CrasAudioHandler.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerOutput1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBJabraSpeakerInput1->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

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
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kHDMIOutput, kUSBJabraSpeakerOutput1,
                             kUSBJabraSpeakerInput1, kUSBCameraInput});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the jabra speaker's output and input are selected as active output
  // by CrasAudioHandler.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerOutput1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBJabraSpeakerInput1->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

  cras_audio_handler_->SetActiveOutputNodes(CrasAudioHandler::NodeIdList());

  // Verify the jabra's input is selected as active input, and that there are
  // no active outputs.
  EXPECT_EQ(1, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerInput1->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_EQ(0u, cras_audio_handler_->GetPrimaryActiveOutputNode());
}

TEST_P(CrasAudioHandlerTest, SetEmptyActiveInputNodes) {
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kHDMIOutput, kUSBJabraSpeakerOutput1,
                             kUSBJabraSpeakerInput1, kUSBCameraInput});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the jabra speaker's output and input are selected as active output
  // by CrasAudioHandler.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerOutput1->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBJabraSpeakerInput1->id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

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
  AudioNodeList audio_nodes;
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  audio_nodes.push_back(internal_speaker);
  AudioNode internal_mic = GenerateAudioNode(kInternalMic);
  audio_nodes.push_back(internal_mic);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(2u, audio_devices.size());

  // Verify internal speaker is selected as active output by default.
  EXPECT_EQ(internal_speaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  // Verify internal mic is selected as active input by default.
  EXPECT_EQ(internal_mic.id, cras_audio_handler_->GetPrimaryActiveInputNode());

  // Hotplug the 35mm headset with both headphone and mic.
  audio_nodes.clear();
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);
  AudioNode headphone = GenerateAudioNode(kHeadphone);
  headphone.active = false;
  headphone.plugged_time = 50000000;
  audio_nodes.push_back(headphone);
  internal_mic.active = true;
  audio_nodes.push_back(internal_mic);
  AudioNode mic = GenerateAudioNode(kMicJack);
  mic.active = false;
  mic.plugged_time = 50000000;
  audio_nodes.push_back(mic);
  ChangeAudioNodes(audio_nodes);

  // Verify 35mm headphone is selected as active output.
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(4u, audio_devices.size());
  EXPECT_EQ(headphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  // Verify 35mm mic is selected as active input.
  EXPECT_EQ(mic.id, cras_audio_handler_->GetPrimaryActiveInputNode());

  // Manually select internal speaker as active output.
  AudioDevice internal_output(internal_speaker);
  cras_audio_handler_->SwitchToDevice(internal_output, true,
                                      CrasAudioHandler::ACTIVATE_BY_USER);
  // Manually select internal mic as active input.
  AudioDevice internal_input(internal_mic);
  cras_audio_handler_->SwitchToDevice(internal_input, true,
                                      CrasAudioHandler::ACTIVATE_BY_USER);

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

// Test the case in which an HDMI output is plugged in with other higher
// priority
// output devices already plugged and user has manually selected an active
// output.
// The hotplug of hdmi output should not change user's selection of active
// device.
// crbug.com/447826.
TEST_P(CrasAudioHandlerTest, HotPlugHDMINotChangeActiveOutput) {
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
                                      CrasAudioHandler::ACTIVATE_BY_USER);

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

  // The active output should not change.
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
}

// Test the case in which the active device was set to inactive from cras after
// resuming from suspension state. See crbug.com/478968.
TEST_P(CrasAudioHandlerTest, ActiveNodeLostAfterResume) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kHeadphone, kHDMIOutput});
  for (const auto& node : audio_nodes)
    ASSERT_FALSE(node.active) << node.id << " expected to be inactive";
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
  AudioNodeList audio_nodes;
  AudioNode internal_speaker = GenerateAudioNode(kInternalSpeaker);
  audio_nodes.push_back(internal_speaker);
  AudioNode hdmi_output = GenerateAudioNode(kHDMIOutput);
  audio_nodes.push_back(hdmi_output);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the hdmi is selected as the active output since it has a higher
  // priority.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_EQ(hdmi_output.id, cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Manually set the active output to internal speaker.
  cras_audio_handler_->SwitchToDevice(AudioDevice(internal_speaker), true,
                                      CrasAudioHandler::ACTIVATE_BY_USER);
  EXPECT_EQ(internal_speaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Simulate the suspend and resume of the device during mirror mode. The HDMI
  // node will be lost first.
  audio_nodes.clear();
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);
  ChangeAudioNodes(audio_nodes);

  // Verify the HDMI node is lost, and internal speaker is still the active
  // node.
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(1u, audio_devices.size());
  EXPECT_EQ(internal_speaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Simulate the re-appearing of the hdmi node, which comes with a new id,
  // but the same stable device id.
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
  for (const auto& node : audio_nodes)
    ASSERT_FALSE(node.active) << node.id << " expected to be inactive";
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
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kHDMIOutput});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the HDMI device has been selected as the active output, and audio
  // output is not muted.
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHDMIOutput->id, active_output.id);
  EXPECT_EQ(kHDMIOutput->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());
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
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_FALSE(
      cras_audio_handler_->IsOutputMutedForDevice(kInternalSpeaker->id));
  EXPECT_TRUE(cras_audio_handler_->IsOutputMuted());

  // Re-attach the HDMI device after a little delay.
  HDMIRediscoverWaiter waiter(this, grace_period_in_ms);
  waiter.WaitUntilTimeOut(grace_period_in_ms / 4);
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

// This tests the case of output unmuting event is notified after the hdmi
// output re-discover grace period ends, see crbug.com/512601.
TEST_P(CrasAudioHandlerTest, HDMIOutputUnplugDuringSuspension) {
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kInternalSpeaker, kHDMIOutput});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the HDMI device has been selected as the active output, and audio
  // output is not muted.
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHDMIOutput->id, active_output.id);
  EXPECT_EQ(kHDMIOutput->id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());
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
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_FALSE(
      cras_audio_handler_->IsOutputMutedForDevice(kInternalSpeaker->id));
  EXPECT_TRUE(cras_audio_handler_->IsOutputMuted());

  // After HDMI re-discover grace period, verify internal speaker is still the
  // active output and not muted, and unmute event by system is notified.
  test_observer_->reset_output_mute_changed_count();
  HDMIRediscoverWaiter waiter(this, grace_period_in_ms);
  waiter.WaitUntilHDMIRediscoverGracePeriodEnd();
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker->id, active_output.id);
  EXPECT_EQ(kInternalSpeaker->id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_FALSE(cras_audio_handler_->IsOutputMuted());
  EXPECT_EQ(1, test_observer_->output_mute_changed_count());
  EXPECT_TRUE(test_observer_->output_mute_by_system());
}

TEST_P(CrasAudioHandlerTest, FrontCameraStartStop) {
  AudioNodeList audio_nodes = GenerateAudioNodeList({kFrontMic, kRearMic});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the front mic has been selected as the active input.
  EXPECT_EQ(kFrontMicId, cras_audio_handler_->GetPrimaryActiveInputNode());
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
  AudioNodeList audio_nodes = GenerateAudioNodeList({kFrontMic, kRearMic});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the front mic has been selected as the active input.
  EXPECT_EQ(kFrontMicId, cras_audio_handler_->GetPrimaryActiveInputNode());
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
  AudioNodeList audio_nodes = GenerateAudioNodeList({kFrontMic, kRearMic});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the front mic has been selected as the active input.
  EXPECT_EQ(kFrontMicId, cras_audio_handler_->GetPrimaryActiveInputNode());
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
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kFrontMic, kRearMic, kMicJack});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the mic Jack has been selected as the active input.
  EXPECT_EQ(kMicJackId, cras_audio_handler_->GetPrimaryActiveInputNode());
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
  AudioNodeList audio_nodes =
      GenerateAudioNodeList({kFrontMic, kRearMic, kMicJack});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the mic Jack has been selected as the active input.
  EXPECT_EQ(kMicJackId, cras_audio_handler_->GetPrimaryActiveInputNode());
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
  AudioNodeList audio_nodes = GenerateAudioNodeList({kInternalMic});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the mic Jack has been selected as the active input.
  EXPECT_EQ(kInternalMicId, cras_audio_handler_->GetPrimaryActiveInputNode());
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
  AudioNodeList audio_nodes = GenerateAudioNodeList({kFrontMic, kRearMic});
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the front mic has been selected as the active input.
  EXPECT_EQ(kFrontMicId, cras_audio_handler_->GetPrimaryActiveInputNode());
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
  AudioNodeList audio_nodes = GenerateAudioNodeList({kUSBMic1, kInternalMic});
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the USB mic is selected as the active input.
  EXPECT_EQ(0, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kUSBMicId1, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());

  system_monitor_observer_.reset_count();

  // Plug the second USB mic.
  audio_nodes.clear();
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
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size + 1, audio_devices.size());

  // Verify the active input device is changed to usb mic 2.
  EXPECT_EQ(1, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kUSBMicId2, cras_audio_handler_->GetPrimaryActiveInputNode());
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

}  // namespace chromeos
