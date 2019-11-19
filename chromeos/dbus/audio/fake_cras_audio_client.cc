// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/audio/fake_cras_audio_client.h"

#include <utility>

namespace chromeos {

namespace {

FakeCrasAudioClient* g_instance = nullptr;

}  // namespace

FakeCrasAudioClient::FakeCrasAudioClient() {
  CHECK(!g_instance);
  g_instance = this;

  VLOG(1) << "FakeCrasAudioClient is created";

  // Fake audio output nodes.
  AudioNode output_1;
  output_1.is_input = false;
  output_1.id = 10001;
  output_1.stable_device_id_v1 = 10001;
  output_1.device_name = "Fake Speaker";
  output_1.type = "INTERNAL_SPEAKER";
  output_1.name = "Speaker";
  node_list_.push_back(output_1);

  AudioNode output_2;
  output_2.is_input = false;
  output_2.id = 10002;
  output_2.stable_device_id_v1 = 10002;
  output_2.device_name = "Fake Headphone";
  output_2.type = "HEADPHONE";
  output_2.name = "Headphone";
  node_list_.push_back(output_2);

  AudioNode output_3;
  output_3.is_input = false;
  output_3.id = 10003;
  output_3.stable_device_id_v1 = 10003;
  output_3.device_name = "Fake Bluetooth Headphone";
  output_3.type = "BLUETOOTH";
  output_3.name = "Headphone";
  node_list_.push_back(output_3);

  AudioNode output_4;
  output_4.is_input = false;
  output_4.id = 10004;
  output_4.stable_device_id_v1 = 10004;
  output_4.device_name = "Fake HDMI Speaker";
  output_4.type = "HDMI";
  output_4.name = "HDMI Speaker";
  node_list_.push_back(output_4);

  // Fake audio input nodes
  AudioNode input_1;
  input_1.is_input = true;
  input_1.id = 20001;
  input_1.stable_device_id_v1 = 20001;
  input_1.device_name = "Fake Internal Mic";
  input_1.type = "INTERNAL_MIC";
  input_1.name = "Internal Mic";
  node_list_.push_back(input_1);

  AudioNode input_2;
  input_2.is_input = true;
  input_2.id = 20002;
  input_2.stable_device_id_v1 = 20002;
  input_2.device_name = "Fake USB Mic";
  input_2.type = "USB";
  input_2.name = "Mic";
  node_list_.push_back(input_2);

  AudioNode input_3;
  input_3.is_input = true;
  input_3.id = 20003;
  input_3.stable_device_id_v1 = 20003;
  input_3.device_name = "Fake Mic Jack";
  input_3.type = "MIC";
  input_3.name = "Some type of Mic";
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
    DBusMethodCallback<VolumeState> callback) {
  std::move(callback).Run(volume_state_);
}

void FakeCrasAudioClient::GetDefaultOutputBufferSize(
    DBusMethodCallback<int> callback) {
  std::move(callback).Run(512);
}

void FakeCrasAudioClient::GetSystemAecSupported(
    DBusMethodCallback<bool> callback) {
  std::move(callback).Run(false);
}

void FakeCrasAudioClient::GetSystemAecGroupId(
    DBusMethodCallback<int32_t> callback) {
  std::move(callback).Run(1);
}

void FakeCrasAudioClient::GetNodes(DBusMethodCallback<AudioNodeList> callback) {
  std::move(callback).Run(node_list_);
}

void FakeCrasAudioClient::GetNumberOfActiveOutputStreams(
    DBusMethodCallback<int> callback) {
  std::move(callback).Run(0);
}

void FakeCrasAudioClient::SetOutputNodeVolume(uint64_t node_id,
                                              int32_t volume) {
  if (!notify_volume_change_with_delay_)
    NotifyOutputNodeVolumeChangedForTesting(node_id, volume);
}

void FakeCrasAudioClient::SetOutputUserMute(bool mute_on) {
  volume_state_.output_user_mute = mute_on;
  for (auto& observer : observers_)
    observer.OutputMuteChanged(volume_state_.output_user_mute);
}

void FakeCrasAudioClient::SetInputNodeGain(uint64_t node_id,
                                           int32_t input_gain) {}

void FakeCrasAudioClient::SetInputMute(bool mute_on) {
  volume_state_.input_mute = mute_on;
  for (auto& observer : observers_)
    observer.InputMuteChanged(volume_state_.input_mute);
}

void FakeCrasAudioClient::SetActiveOutputNode(uint64_t node_id) {
  if (active_output_node_id_ == node_id)
    return;

  for (size_t i = 0; i < node_list_.size(); ++i) {
    if (node_list_[i].id == active_output_node_id_)
      node_list_[i].active = false;
    else if (node_list_[i].id == node_id)
      node_list_[i].active = true;
  }
  active_output_node_id_ = node_id;
  for (auto& observer : observers_)
    observer.ActiveOutputNodeChanged(node_id);
}

void FakeCrasAudioClient::SetActiveInputNode(uint64_t node_id) {
  if (active_input_node_id_ == node_id)
    return;

  for (size_t i = 0; i < node_list_.size(); ++i) {
    if (node_list_[i].id == active_input_node_id_)
      node_list_[i].active = false;
    else if (node_list_[i].id == node_id)
      node_list_[i].active = true;
  }
  active_input_node_id_ = node_id;
  for (auto& observer : observers_)
    observer.ActiveInputNodeChanged(node_id);
}

void FakeCrasAudioClient::SetHotwordModel(uint64_t node_id,
                                          const std::string& hotword_model,
                                          VoidDBusMethodCallback callback) {}

void FakeCrasAudioClient::AddActiveInputNode(uint64_t node_id) {
  for (size_t i = 0; i < node_list_.size(); ++i) {
    if (node_list_[i].id == node_id)
      node_list_[i].active = true;
  }
}

void FakeCrasAudioClient::RemoveActiveInputNode(uint64_t node_id) {
  for (size_t i = 0; i < node_list_.size(); ++i) {
    if (node_list_[i].id == node_id)
      node_list_[i].active = false;
  }
}

void FakeCrasAudioClient::SwapLeftRight(uint64_t node_id, bool swap) {}

void FakeCrasAudioClient::SetGlobalOutputChannelRemix(
    int32_t channels,
    const std::vector<double>& mixer) {}

void FakeCrasAudioClient::AddActiveOutputNode(uint64_t node_id) {
  for (size_t i = 0; i < node_list_.size(); ++i) {
    if (node_list_[i].id == node_id)
      node_list_[i].active = true;
  }
}

void FakeCrasAudioClient::WaitForServiceToBeAvailable(
    WaitForServiceToBeAvailableCallback callback) {
  std::move(callback).Run(true);
}

void FakeCrasAudioClient::RemoveActiveOutputNode(uint64_t node_id) {
  for (size_t i = 0; i < node_list_.size(); ++i) {
    if (node_list_[i].id == node_id)
      node_list_[i].active = false;
  }
}

void FakeCrasAudioClient::InsertAudioNodeToList(const AudioNode& audio_node) {
  auto iter = FindNode(audio_node.id);
  if (iter != node_list_.end())
    (*iter) = audio_node;
  else
    node_list_.push_back(audio_node);
  for (auto& observer : observers_)
    observer.NodesChanged();
}

void FakeCrasAudioClient::RemoveAudioNodeFromList(const uint64_t& node_id) {
  auto iter = FindNode(node_id);
  if (iter != node_list_.end()) {
    node_list_.erase(iter);
    for (auto& observer : observers_)
      observer.NodesChanged();
  }
}

void FakeCrasAudioClient::SetAudioNodesForTesting(
    const AudioNodeList& audio_nodes) {
  node_list_ = audio_nodes;
}

void FakeCrasAudioClient::SetAudioNodesAndNotifyObserversForTesting(
    const AudioNodeList& new_nodes) {
  SetAudioNodesForTesting(new_nodes);
  for (auto& observer : observers_)
    observer.NodesChanged();
}

void FakeCrasAudioClient::NotifyOutputNodeVolumeChangedForTesting(
    uint64_t node_id,
    int volume) {
  for (auto& observer : observers_)
    observer.OutputNodeVolumeChanged(node_id, volume);
}

void FakeCrasAudioClient::NotifyHotwordTriggeredForTesting(uint64_t tv_sec,
                                                           uint64_t tv_nsec) {
  for (auto& observer : observers_)
    observer.HotwordTriggered(tv_sec, tv_nsec);
}

AudioNodeList::iterator FakeCrasAudioClient::FindNode(uint64_t node_id) {
  return std::find_if(
      node_list_.begin(), node_list_.end(),
      [node_id](const AudioNode& node) { return node_id == node.id; });
}

}  // namespace chromeos
