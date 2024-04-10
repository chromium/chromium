// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_device_selection_test_base.h"

#include "base/strings/stringprintf.h"
#include "chromeos/ash/components/audio/audio_devices_pref_handler_impl.h"

namespace ash {

class ActiveNodeObserver : public CrasAudioClient::Observer {
 protected:
  void ActiveInputNodeChanged(uint64_t node_id) override {
    active_input_node_id_ = node_id;
  }
  void ActiveOutputNodeChanged(uint64_t node_id) override {
    active_output_node_id_ = node_id;
  }

 public:
  uint64_t GetActiveInputNodeId() { return active_input_node_id_; }
  uint64_t GetActiveOutputNodeId() { return active_output_node_id_; }
  void Reset() {
    active_input_node_id_ = 0;
    active_output_node_id_ = 0;
  }

 private:
  uint64_t active_input_node_id_ = 0;
  uint64_t active_output_node_id_ = 0;
};

AudioDeviceSelectionTestBase::AudioDeviceSelectionTestBase()
    : active_node_observer_(std::make_unique<ActiveNodeObserver>()) {}

AudioDeviceSelectionTestBase::~AudioDeviceSelectionTestBase() = default;

void AudioDeviceSelectionTestBase::SetUp() {
  node_count_ = 0;
  plugged_time_ = 0;

  pref_service_ = std::make_unique<TestingPrefServiceSimple>();
  AudioDevicesPrefHandlerImpl::RegisterPrefs(pref_service_->registry());
  audio_pref_handler_ = new AudioDevicesPrefHandlerImpl(pref_service_.get());

  CrasAudioClient::InitializeFake();
  fake_cras_audio_client_ = FakeCrasAudioClient::Get();
  // Delete audio nodes created in FakeCrasAudioClient::FakeCrasAudioClient()
  fake_cras_audio_client_->SetAudioNodesForTesting({});
  active_node_observer_->Reset();
  fake_cras_audio_client_->AddObserver(active_node_observer_.get());

  CrasAudioHandler::Initialize(mojo::NullRemote(), audio_pref_handler_);
  cras_audio_handler_ = CrasAudioHandler::Get();

  base::RunLoop().RunUntilIdle();
}

void AudioDeviceSelectionTestBase::TearDown() {
  CrasAudioHandler::Shutdown();
  audio_pref_handler_ = nullptr;
  CrasAudioClient::Shutdown();
  pref_service_.reset();
}

void AudioDeviceSelectionTestBase::Plug(AudioNode node) {
  node.plugged_time = ++plugged_time_;
  fake_cras_audio_client_->InsertAudioNodeToList(node);
}

void AudioDeviceSelectionTestBase::Unplug(const AudioNode& node) {
  fake_cras_audio_client_->RemoveAudioNodeFromList(node.id);
}

void AudioDeviceSelectionTestBase::Select(const AudioNode& node) {
  if (node.is_input) {
    ASSERT_TRUE(cras_audio_handler_->SetActiveInputNodes({node.id}));
  } else {
    ASSERT_TRUE(cras_audio_handler_->SetActiveOutputNodes({node.id}));
  }
}

void AudioDeviceSelectionTestBase::SystemBootsWith(
    const AudioNodeList& new_nodes) {
  fake_cras_audio_client_->SetAudioNodesAndNotifyObserversForTesting(new_nodes);
}

uint64_t AudioDeviceSelectionTestBase::ActiveInputNodeId() {
  return active_node_observer_->GetActiveInputNodeId();
}

uint64_t AudioDeviceSelectionTestBase::ActiveOutputNodeId() {
  return active_node_observer_->GetActiveOutputNodeId();
}

AudioNode AudioDeviceSelectionTestBase::NewNodeWithName(
    bool is_input,
    const std::string& type,
    const std::string& name) {
  ++node_count_;
  return AudioNode(
      is_input, /*id=*/node_count_, /*has_v2_stable_device_id=*/true,
      /*stable_device_id_v1=*/node_count_, /*stable_device_id_v2=*/node_count_,
      /*device_name=*/name, type, name, /*active=*/false, /*plugged_time=*/0,
      /*max_supported_channels=*/2, /*audio_effect=*/0,
      /*number_of_volume_steps=*/0);
}

AudioNode AudioDeviceSelectionTestBase::NewNode(bool is_input,
                                                const std::string& type) {
  ++node_count_;
  std::string name =
      base::StringPrintf("%s-%" PRIu64, type.c_str(), node_count_);
  return NewNodeWithName(is_input, type, name);
}

}  // namespace ash
