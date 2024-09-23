// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

namespace {

FakeCrasAudioClient* g_instance = nullptr;

}  // namespace

const uint32_t kInputMaxSupportedChannels = 1;
const uint32_t kOutputMaxSupportedChannels = 2;

const uint32_t kInputAudioEffect = 1;
const uint32_t kOutputAudioEffect = 0;

const int32_t kInputNumberOfVolumeSteps = 0;
const int32_t kOutputNumberOfVolumeSteps = 25;

FakeCrasAudioClient::FakeCrasAudioClient() {
  CHECK(!g_instance);
  g_instance = this;

  VLOG(1) << "FakeCrasAudioClient is created";

  // Fake audio output nodes.
  AudioNode output_1;
  output_1.is_input = false;
  output_1.id = 0x100000001;
  output_1.stable_device_id_v1 = 10001;
  output_1.max_supported_channels = kOutputMaxSupportedChannels;
  output_1.audio_effect = kOutputAudioEffect;
  output_1.device_name = "Fake Speaker";
  output_1.type = "INTERNAL_SPEAKER";
  output_1.name = "Speaker";
  output_1.number_of_volume_steps = kOutputNumberOfVolumeSteps;
  node_list_.push_back(output_1);

  AudioNode output_2;
  output_2.is_input = false;
  output_2.id = 0x200000001;
  output_2.stable_device_id_v1 = 10002;
  output_2.max_supported_channels = kOutputMaxSupportedChannels;
  output_2.audio_effect = kOutputAudioEffect;
  output_2.device_name = "Fake Headphone";
  output_2.type = "HEADPHONE";
  output_2.name = "Headphone";
  output_2.number_of_volume_steps = kOutputNumberOfVolumeSteps;
  node_list_.push_back(output_2);

  AudioNode output_3;
  output_3.is_input = false;
  output_3.id = 0x300000001;
  output_3.stable_device_id_v1 = 10003;
  output_3.max_supported_channels = kOutputMaxSupportedChannels;
  output_3.audio_effect = kOutputAudioEffect;
  output_3.device_name = "Fake Bluetooth Headphone";
  output_3.type = "BLUETOOTH";
  output_3.name = "Headphone";
  output_3.number_of_volume_steps = kOutputNumberOfVolumeSteps;
  node_list_.push_back(output_3);

  AudioNode output_4;
  output_4.is_input = false;
  output_4.id = 0x400000001;
  output_4.stable_device_id_v1 = 10004;
  output_4.max_supported_channels = kOutputMaxSupportedChannels;
  output_4.audio_effect = kOutputAudioEffect;
  output_4.device_name = "Fake HDMI Speaker";
  output_4.type = "HDMI";
  output_4.name = "HDMI Speaker";
  output_4.number_of_volume_steps = kOutputNumberOfVolumeSteps;
  node_list_.push_back(output_4);

  // Fake audio input nodes
  AudioNode input_1;
  input_1.is_input = true;
  input_1.id = 0x100000002;
  input_1.stable_device_id_v1 = 20001;
  input_1.max_supported_channels = kInputMaxSupportedChannels;
  input_1.audio_effect = kInputAudioEffect;
  input_1.device_name = "Fake Internal Mic";
  input_1.type = "INTERNAL_MIC";
  input_1.name = "Internal Mic";
  input_1.number_of_volume_steps = kInputNumberOfVolumeSteps;
  node_list_.push_back(input_1);

  AudioNode input_2;
  input_2.is_input = true;
  input_2.id = 0x200000002;
  input_2.stable_device_id_v1 = 20002;
  input_2.max_supported_channels = kInputMaxSupportedChannels;
  input_2.audio_effect = kInputAudioEffect;
  input_2.device_name = "Fake USB Mic";
  input_2.type = "USB";
  input_2.name = "Mic";
  input_2.number_of_volume_steps = kInputNumberOfVolumeSteps;
  node_list_.push_back(input_2);

  AudioNode input_3;
  input_3.is_input = true;
  input_3.id = 0x300000002;
  input_3.stable_device_id_v1 = 20003;
  input_3.max_supported_channels = kInputMaxSupportedChannels;
  input_3.audio_effect = kInputAudioEffect;
  input_3.device_name = "Fake Mic Jack";
  input_3.type = "MIC";
  input_3.name = "Some type of Mic";
  input_3.number_of_volume_steps = kInputNumberOfVolumeSteps;
  node_list_.push_back(input_3);
}

FakeCrasAudioClient::~FakeCrasAudioClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
FakeCrasAudioClient* FakeCrasAudioClient::Get() {
  return g_instance;
}

void FakeCrasAudioClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeCrasAudioClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool FakeCrasAudioClient::HasObserver(const Observer* observer) const {
  return observers_.HasObserver(observer);
}

void FakeCrasAudioClient::GetVolumeState(
    chromeos::DBusMethodCallback<VolumeState> callback) {
  std::move(callback).Run(volume_state_);
}

void FakeCrasAudioClient::GetDefaultOutputBufferSize(
    chromeos::DBusMethodCallback<int> callback) {
  std::move(callback).Run(512);
}

void FakeCrasAudioClient::GetSystemAecSupported(
    chromeos::DBusMethodCallback<bool> callback) {
  std::move(callback).Run(false);
}

void FakeCrasAudioClient::GetSystemAecGroupId(
    chromeos::DBusMethodCallback<int32_t> callback) {
  std::move(callback).Run(1);
}

void FakeCrasAudioClient::GetSystemNsSupported(
    chromeos::DBusMethodCallback<bool> callback) {
  std::move(callback).Run(false);
}

void FakeCrasAudioClient::GetSystemAgcSupported(
    chromeos::DBusMethodCallback<bool> callback) {
  std::move(callback).Run(false);
}

void FakeCrasAudioClient::GetNodes(
    chromeos::DBusMethodCallback<AudioNodeList> callback) {
  std::move(callback).Run(node_list_);
}

void FakeCrasAudioClient::GetNumberOfNonChromeOutputStreams(
    chromeos::DBusMethodCallback<int32_t> callback) {
  std::move(callback).Run(number_non_chrome_output_streams_);
}

void FakeCrasAudioClient::GetNumberOfActiveOutputStreams(
    chromeos::DBusMethodCallback<int> callback) {
  std::move(callback).Run(0);
}

void FakeCrasAudioClient::GetNumberOfInputStreamsWithPermission(
    chromeos::DBusMethodCallback<ClientTypeToInputStreamCount> callback) {
  std::move(callback).Run(active_input_streams_);
}

void FakeCrasAudioClient::GetSpeakOnMuteDetectionEnabled(
    chromeos::DBusMethodCallback<bool> callback) {
  std::move(callback).Run(false);
}

void FakeCrasAudioClient::SetOutputNodeVolume(uint64_t node_id,
                                              int32_t volume) {
  if (enable_volume_change_events_) {
    if (send_volume_change_events_synchronous_) {
      NotifyOutputNodeVolumeChangedForTesting(node_id, volume);
    } else {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &FakeCrasAudioClient::NotifyOutputNodeVolumeChangedForTesting,
              weak_ptr_factory_.GetWeakPtr(), node_id, volume));
    }
  }
}

void FakeCrasAudioClient::SetOutputUserMute(bool mute_on) {
  volume_state_.output_user_mute = mute_on;
  for (auto& observer : observers_) {
    observer.OutputMuteChanged(volume_state_.output_user_mute);
  }
}

void FakeCrasAudioClient::SetInputNodeGain(uint64_t node_id,
                                           int32_t input_gain) {
  if (enable_gain_change_events_) {
    NotifyInputNodeGainChangedForTesting(node_id, input_gain);
  }
}

void FakeCrasAudioClient::SetInputMute(bool mute_on) {
  volume_state_.input_mute = mute_on;
  for (auto& observer : observers_) {
    observer.InputMuteChanged(volume_state_.input_mute);
  }
}

void FakeCrasAudioClient::SetNoiseCancellationSupported(
    bool noise_cancellation_supported) {
  noise_cancellation_supported_ = noise_cancellation_supported;
}

void FakeCrasAudioClient::SetNoiseCancellationEnabled(
    bool noise_cancellation_on) {
  noise_cancellation_enabled_ = noise_cancellation_on;
  ++noise_cancellation_enabled_counter_;
}

void FakeCrasAudioClient::GetNoiseCancellationSupported(
    chromeos::DBusMethodCallback<bool> callback) {
  std::move(callback).Run(noise_cancellation_supported_);
}

uint32_t FakeCrasAudioClient::GetNoiseCancellationEnabledCount() {
  return noise_cancellation_enabled_counter_;
}

void FakeCrasAudioClient::SetStyleTransferSupported(
    bool style_transfer_supported) {
  style_transfer_supported_ = style_transfer_supported;
}

void FakeCrasAudioClient::SetStyleTransferEnabled(bool style_transfer_on) {
  style_transfer_enabled_ = style_transfer_on;
}

void FakeCrasAudioClient::GetStyleTransferSupported(
    chromeos::DBusMethodCallback<bool> callback) {
  std::move(callback).Run(style_transfer_supported_);
}

bool FakeCrasAudioClient::GetStyleTransferEnabled() {
  return style_transfer_enabled_;
}

void FakeCrasAudioClient::SetNumberOfNonChromeOutputStreams(int32_t streams) {
  number_non_chrome_output_streams_ = streams;
  for (auto& observer : observers_) {
    observer.NumberOfNonChromeOutputStreamsChanged();
  }
}

void FakeCrasAudioClient::SetActiveOutputNode(uint64_t node_id) {
  if (active_output_node_id_ == node_id) {
    return;
  }

  for (size_t i = 0; i < node_list_.size(); ++i) {
    if (node_list_[i].id == active_output_node_id_) {
      node_list_[i].active = false;
    } else if (node_list_[i].id == node_id) {
      node_list_[i].active = true;
    }
  }
  active_output_node_id_ = node_id;
  for (auto& observer : observers_) {
    observer.ActiveOutputNodeChanged(node_id);
  }
}

void FakeCrasAudioClient::SetActiveInputNode(uint64_t node_id) {
  if (active_input_node_id_ == node_id) {
    return;
  }

  for (size_t i = 0; i < node_list_.size(); ++i) {
    if (node_list_[i].id == active_input_node_id_) {
      node_list_[i].active = false;
    } else if (node_list_[i].id == node_id) {
      node_list_[i].active = true;
    }
  }
  active_input_node_id_ = node_id;
  for (auto& observer : observers_) {
    observer.ActiveInputNodeChanged(node_id);
  }
}

void FakeCrasAudioClient::SetHotwordModel(
    uint64_t node_id,
    const std::string& hotword_model,
    chromeos::VoidDBusMethodCallback callback) {}

void FakeCrasAudioClient::SetFixA2dpPacketSize(bool enabled) {}

void FakeCrasAudioClient::SetFlossEnabled(bool enabled) {}

void FakeCrasAudioClient::SetSpeakOnMuteDetection(bool enabled) {
  speak_on_mute_detection_enabled_ = enabled;
}

void FakeCrasAudioClient::SetEwmaPowerReportEnabled(bool enabled) {
  ewma_power_report_enabled_ = enabled;
}

void FakeCrasAudioClient::SetSidetoneEnabled(bool enabled) {
  sidetone_enabled_ = enabled;
}

void FakeCrasAudioClient::GetSidetoneSupported(
    chromeos::DBusMethodCallback<bool> callback) {
  std::move(callback).Run(sidetone_supported_);
}

void FakeCrasAudioClient::AddActiveInputNode(uint64_t node_id) {
  for (size_t i = 0; i < node_list_.size(); ++i) {
    if (node_list_[i].id == node_id) {
      node_list_[i].active = true;
    }
  }
}

void FakeCrasAudioClient::RemoveActiveInputNode(uint64_t node_id) {
  for (size_t i = 0; i < node_list_.size(); ++i) {
    if (node_list_[i].id == node_id) {
      node_list_[i].active = false;
    }
  }
}

void FakeCrasAudioClient::SwapLeftRight(uint64_t node_id, bool swap) {}

void FakeCrasAudioClient::SetDisplayRotation(uint64_t node_id,
                                             cras::DisplayRotation rotation) {}

void FakeCrasAudioClient::SetGlobalOutputChannelRemix(
    int32_t channels,
    const std::vector<double>& mixer) {}

void FakeCrasAudioClient::SetPlayerPlaybackStatus(
    const std::string& playback_status) {}

void FakeCrasAudioClient::SetPlayerIdentity(
    const std::string& playback_identity) {}

void FakeCrasAudioClient::SetPlayerPosition(const int64_t& position) {}

void FakeCrasAudioClient::SetPlayerDuration(const int64_t& duration) {}

void FakeCrasAudioClient::SetPlayerMetadata(
    const std::map<std::string, std::string>& metadata) {}

void FakeCrasAudioClient::AddActiveOutputNode(uint64_t node_id) {
  for (size_t i = 0; i < node_list_.size(); ++i) {
    if (node_list_[i].id == node_id) {
      node_list_[i].active = true;
    }
  }
}

void FakeCrasAudioClient::ResendBluetoothBattery() {
  for (auto& observer : observers_) {
    observer.BluetoothBatteryChanged("11:22:33:44:55:66", battery_level_);
  }
}

void FakeCrasAudioClient::WaitForServiceToBeAvailable(
    chromeos::WaitForServiceToBeAvailableCallback callback) {
  std::move(callback).Run(true);
}

void FakeCrasAudioClient::RemoveActiveOutputNode(uint64_t node_id) {
  for (size_t i = 0; i < node_list_.size(); ++i) {
    if (node_list_[i].id == node_id) {
      node_list_[i].active = false;
    }
  }
}

void FakeCrasAudioClient::InsertAudioNodeToList(const AudioNode& audio_node) {
  auto iter = FindNode(audio_node.id);
  if (iter != node_list_.end()) {
    (*iter) = audio_node;
  } else {
    node_list_.push_back(audio_node);
  }
  for (auto& observer : observers_) {
    observer.NodesChanged();
  }
}

void FakeCrasAudioClient::RemoveAudioNodeFromList(const uint64_t& node_id) {
  auto iter = FindNode(node_id);
  if (iter != node_list_.end()) {
    node_list_.erase(iter);
    for (auto& observer : observers_) {
      observer.NodesChanged();
    }
  }
}

void FakeCrasAudioClient::SetAudioNodesForTesting(
    const AudioNodeList& audio_nodes) {
  node_list_ = audio_nodes;
}

void FakeCrasAudioClient::SetAudioNodesAndNotifyObserversForTesting(
    const AudioNodeList& new_nodes) {
  SetAudioNodesForTesting(new_nodes);
  for (auto& observer : observers_) {
    observer.NodesChanged();
  }
}

void FakeCrasAudioClient::NotifyOutputNodeVolumeChangedForTesting(
    uint64_t node_id,
    int volume) {
  for (auto& observer : observers_) {
    observer.OutputNodeVolumeChanged(node_id, volume);
  }
}

void FakeCrasAudioClient::NotifyInputNodeGainChangedForTesting(uint64_t node_id,
                                                               int gain) {
  for (auto& observer : observers_) {
    observer.InputNodeGainChanged(node_id, gain);
  }
}

void FakeCrasAudioClient::NotifyHotwordTriggeredForTesting(uint64_t tv_sec,
                                                           uint64_t tv_nsec) {
  for (auto& observer : observers_) {
    observer.HotwordTriggered(tv_sec, tv_nsec);
  }
}

void FakeCrasAudioClient::SetBluetoothBattteryLevelForTesting(uint32_t level) {
  battery_level_ = level;
}

void FakeCrasAudioClient::SetActiveInputStreamsWithPermission(
    const ClientTypeToInputStreamCount& input_streams) {
  active_input_streams_ = input_streams;
  for (auto& observer : observers_) {
    observer.NumberOfInputStreamsWithPermissionChanged(active_input_streams_);
  }
}

void FakeCrasAudioClient::NotifySurveyTriggered(
    const base::flat_map<std::string, std::string>& survey_specific_data) {
  for (auto& observer : observers_) {
    observer.SurveyTriggered(survey_specific_data);
  }
}

AudioNodeList::iterator FakeCrasAudioClient::FindNode(uint64_t node_id) {
  return base::ranges::find(node_list_, node_id, &AudioNode::id);
}

void FakeCrasAudioClient::SetForceRespectUiGains(
    bool force_respect_ui_gains_enabled) {
  force_respect_ui_gains_enabled_ = force_respect_ui_gains_enabled;
}

void FakeCrasAudioClient::GetNumStreamIgnoreUiGains(
    chromeos::DBusMethodCallback<int> callback) {
  std::move(callback).Run(false);
}

void FakeCrasAudioClient::GetHfpMicSrSupported(
    chromeos::DBusMethodCallback<bool> callback) {
  std::move(callback).Run(hfp_mic_sr_supported_);
}

void FakeCrasAudioClient::SetHfpMicSrSupported(bool hfp_mic_sr_supported) {
  hfp_mic_sr_supported_ = hfp_mic_sr_supported;
}

uint32_t FakeCrasAudioClient::GetHfpMicSrEnabled() {
  return hfp_mic_sr_enabled_;
}

void FakeCrasAudioClient::SetHfpMicSrEnabled(bool hfp_mic_sr_on) {
  hfp_mic_sr_enabled_ = hfp_mic_sr_on;
}

void FakeCrasAudioClient::SetNumberOfArcStreams(int32_t streams) {
  number_arc_streams_ = streams;
  for (auto& observer : observers_) {
    observer.NumberOfArcStreamsChanged();
  }
}

void FakeCrasAudioClient::GetNumberOfArcStreams(
    chromeos::DBusMethodCallback<int32_t> callback) {
  std::move(callback).Run(number_arc_streams_);
}

}  // namespace ash
