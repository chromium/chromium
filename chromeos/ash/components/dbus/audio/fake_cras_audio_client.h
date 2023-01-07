// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_AUDIO_FAKE_CRAS_AUDIO_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_AUDIO_FAKE_CRAS_AUDIO_CLIENT_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/audio/cras_audio_client.h"

namespace ash {

// The CrasAudioClient implementation used on Linux desktop.
class COMPONENT_EXPORT(DBUS_AUDIO) FakeCrasAudioClient
    : public CrasAudioClient {
 public:
  using ClientTypeToInputStreamCount = base::flat_map<std::string, uint32_t>;

  FakeCrasAudioClient();

  FakeCrasAudioClient(const FakeCrasAudioClient&) = delete;
  FakeCrasAudioClient& operator=(const FakeCrasAudioClient&) = delete;

  ~FakeCrasAudioClient() override;

  static FakeCrasAudioClient* Get();

  void SetNoiseCancellationSupported(bool noise_cancellation_supported);
  uint32_t GetNoiseCancellationEnabledCount();

  // CrasAudioClient overrides:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool HasObserver(const Observer* observer) const override;
  void GetVolumeState(
      chromeos::DBusMethodCallback<VolumeState> callback) override;
  void GetDefaultOutputBufferSize(
      chromeos::DBusMethodCallback<int> callback) override;
  void GetSystemAecSupported(
      chromeos::DBusMethodCallback<bool> callback) override;
  void GetSystemAecGroupId(
      chromeos::DBusMethodCallback<int32_t> callback) override;
  void GetSystemNsSupported(
      chromeos::DBusMethodCallback<bool> callback) override;
  void GetSystemAgcSupported(
      chromeos::DBusMethodCallback<bool> callback) override;
  void GetNodes(chromeos::DBusMethodCallback<AudioNodeList> callback) override;
  void GetNumberOfActiveOutputStreams(
      chromeos::DBusMethodCallback<int> callback) override;
  void GetNumberOfInputStreamsWithPermission(
      chromeos::DBusMethodCallback<ClientTypeToInputStreamCount>) override;
  void GetDeprioritizeBtWbsMic(
      chromeos::DBusMethodCallback<bool> callback) override;
  void SetOutputNodeVolume(uint64_t node_id, int32_t volume) override;
  void SetOutputUserMute(bool mute_on) override;
  void SetInputNodeGain(uint64_t node_id, int32_t gain) override;
  void SetInputMute(bool mute_on) override;
  void SetNoiseCancellationEnabled(bool noise_cancellation_on) override;
  void GetNoiseCancellationSupported(
      chromeos::DBusMethodCallback<bool> callback) override;
  void SetActiveOutputNode(uint64_t node_id) override;
  void SetActiveInputNode(uint64_t node_id) override;
  void SetHotwordModel(uint64_t node_id,
                       const std::string& hotword_model,
                       chromeos::VoidDBusMethodCallback callback) override;
  void SetFixA2dpPacketSize(bool enabled) override;
  void SetFlossEnabled(bool enabled) override;
  void AddActiveInputNode(uint64_t node_id) override;
  void RemoveActiveInputNode(uint64_t node_id) override;
  void AddActiveOutputNode(uint64_t node_id) override;
  void RemoveActiveOutputNode(uint64_t node_id) override;
  void SwapLeftRight(uint64_t node_id, bool swap) override;
  void SetDisplayRotation(uint64_t node_id,
                          cras::DisplayRotation rotation) override;
  void SetGlobalOutputChannelRemix(int32_t channels,
                                   const std::vector<double>& mixer) override;
  void SetPlayerPlaybackStatus(const std::string& playback_status) override;
  void SetPlayerIdentity(const std::string& playback_identity) override;
  void SetPlayerPosition(const int64_t& position) override;
  void SetPlayerDuration(const int64_t& duration) override;
  void SetPlayerMetadata(
      const std::map<std::string, std::string>& metadata) override;
  void ResendBluetoothBattery() override;
  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override;

  // Modifies an AudioNode from |node_list_| based on |audio_node.id|.
  // if the |audio_node.id| cannot be found in list, Add an
  // AudioNode to |node_list_|
  void InsertAudioNodeToList(const AudioNode& audio_node);

  // Removes an AudioNode from |node_list_| based on |node_id|.
  void RemoveAudioNodeFromList(const uint64_t& node_id);

  // Updates |node_list_| to contain |audio_nodes|.
  void SetAudioNodesForTesting(const AudioNodeList& audio_nodes);

  // Calls SetAudioNodesForTesting() and additionally notifies |observers_|.
  void SetAudioNodesAndNotifyObserversForTesting(
      const AudioNodeList& new_nodes);

  // Generates fake signal for OutputNodeVolumeChanged.
  void NotifyOutputNodeVolumeChangedForTesting(uint64_t node_id, int volume);

  // Generates fake hotword signal for HotwordTriggered.
  void NotifyHotwordTriggeredForTesting(uint64_t tv_sec, uint64_t tv_nsec);

  // Set a mock battery level for ResendBatteryLevel.
  void SetBluetoothBattteryLevelForTesting(uint32_t level);

  // Sets a mock mapping from client type to number of active input streams per
  // the client type.
  void SetActiveInputStreamsWithPermission(
      const ClientTypeToInputStreamCount& input_streams);

  const AudioNodeList& node_list() const { return node_list_; }
  const uint64_t& active_input_node_id() const { return active_input_node_id_; }
  const uint64_t& active_output_node_id() const {
    return active_output_node_id_;
  }
  void set_notify_volume_change_with_delay(bool notify_with_delay) {
    notify_volume_change_with_delay_ = notify_with_delay;
  }

  bool noise_cancellation_enabled() const {
    return noise_cancellation_enabled_;
  }

 private:
  // Finds a node in the list based on the id.
  AudioNodeList::iterator FindNode(uint64_t node_id);

  VolumeState volume_state_;
  AudioNodeList node_list_;
  uint64_t active_input_node_id_ = 0;
  uint64_t active_output_node_id_ = 0;
  // By default, immediately sends OutputNodeVolumeChange signal following the
  // SetOutputNodeVolume fake dbus call.
  bool notify_volume_change_with_delay_ = false;
  bool noise_cancellation_supported_ = false;
  uint32_t battery_level_ = 0;
  uint32_t noise_cancellation_enabled_counter_ = 0;
  bool noise_cancellation_enabled_ = false;
  // Maps audio client type to the number of active input streams for clients
  // with the type specified
  ClientTypeToInputStreamCount active_input_streams_;

  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when the migration is finished.
namespace chromeos {
using ::ash::FakeCrasAudioClient;
}

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_AUDIO_FAKE_CRAS_AUDIO_CLIENT_H_
