// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_AUDIO_CRAS_AUDIO_CLIENT_H_
#define CHROMEOS_DBUS_AUDIO_CRAS_AUDIO_CLIENT_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/dbus/audio/audio_node.h"
#include "chromeos/dbus/audio/volume_state.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace dbus {
class Bus;
}

namespace chromeos {

// CrasAudioClient is used to communicate with the cras audio dbus interface.
class COMPONENT_EXPORT(DBUS_AUDIO) CrasAudioClient {
 public:
  // Interface for observing changes from the cras audio changes.
  class Observer {
   public:
    // Called when cras audio client starts or re-starts, which happens when
    // cros device powers up or restarted.
    virtual void AudioClientRestarted();

    // Called when audio output mute state changed to new state of |mute_on|.
    virtual void OutputMuteChanged(bool mute_on);

    // Called when audio input mute state changed to new state of |mute_on|.
    virtual void InputMuteChanged(bool mute_on);

    // Called when audio nodes change.
    virtual void NodesChanged();

    // Called when active audio output node changed to new node with |node_id|.
    virtual void ActiveOutputNodeChanged(uint64_t node_id);

    // Called when active audio input node changed to new node with |node_id|.
    virtual void ActiveInputNodeChanged(uint64_t node_id);

    // Called when output node's volume changed.
    virtual void OutputNodeVolumeChanged(uint64_t node_id, int volume);

    // Called when hotword is triggered.
    virtual void HotwordTriggered(uint64_t tv_sec, uint64_t tv_nsec);

    // Called when the number of active output streams has changed.
    virtual void NumberOfActiveStreamsChanged();

   protected:
    virtual ~Observer();
  };

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static CrasAudioClient* Get();

  // Adds and removes the observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  // Returns true if this object has the given observer.
  virtual bool HasObserver(const Observer* observer) const = 0;

  // Gets the volume state, asynchronously.
  virtual void GetVolumeState(DBusMethodCallback<VolumeState> callback) = 0;

  // Gets the default output buffer size in frames.
  virtual void GetDefaultOutputBufferSize(DBusMethodCallback<int> callback) = 0;

  // Gets if system AEC is supported.
  virtual void GetSystemAecSupported(DBusMethodCallback<bool> callback) = 0;

  // Gets any available group ID for the system AEC
  virtual void GetSystemAecGroupId(DBusMethodCallback<int32_t> callback) = 0;

  // Gets an array of audio input and output nodes.
  virtual void GetNodes(DBusMethodCallback<AudioNodeList> callback) = 0;

  // Gets the number of active output streams.
  virtual void GetNumberOfActiveOutputStreams(
      DBusMethodCallback<int> callback) = 0;

  // Sets output volume of the given |node_id| to |volume|, in the rage of
  // [0, 100].
  virtual void SetOutputNodeVolume(uint64_t node_id, int32_t volume) = 0;

  // Sets output mute from user action.
  virtual void SetOutputUserMute(bool mute_on) = 0;

  // Sets input gain of the given |node_id| to |gain|, in the range of
  // [0, 100].
  virtual void SetInputNodeGain(uint64_t node_id, int32_t gain) = 0;

  // Sets input mute state to |mute_on| value.
  virtual void SetInputMute(bool mute_on) = 0;

  // Sets the active output node to |node_id|.
  virtual void SetActiveOutputNode(uint64_t node_id) = 0;

  // Sets the primary active input node to |node_id|.
  virtual void SetActiveInputNode(uint64_t node_id) = 0;

  // Sets |hotword_model| for the given |node_id|.
  // |hotword_model| is expected to be in format <language>_<region> with lower
  // cases. E.g., "en_us".
  // The callback will receive a boolean which indicates if the hotword model is
  // successfully set.
  virtual void SetHotwordModel(uint64_t node_id,
                               const std::string& hotword_model,
                               VoidDBusMethodCallback callback) = 0;

  // Adds input node |node_id| to the active input list. This is used to add
  // an additional active input node besides the one set by SetActiveInputNode.
  // Note that this action will not trigger an ActiveInputNodeChanged event and
  // nothing will happen if the |node_id| has already been set as active.
  virtual void AddActiveInputNode(uint64_t node_id) = 0;

  // Removes input node |node_id| from the active input list. This is used for
  // removing an active input node added by AddActiveInputNode.
  virtual void RemoveActiveInputNode(uint64_t node_id) = 0;

  // Adds input node |node_id| to the active outputs list. This is used to add
  // an additional active output node besides the one set by SetActiveInputNode.
  // Note that this action will not trigger an ActiveOutputNodeChanged event
  // and nothing will happen if the |node_id| has already been set as active.
  virtual void AddActiveOutputNode(uint64_t node_id) = 0;

  // Removes output node |node_id| from the active output list. This is used for
  // removing an active output node added by AddActiveOutputNode.
  virtual void RemoveActiveOutputNode(uint64_t node_id) = 0;

  // Swaps the left and right channel of the primary active output device.
  // Swap the left and right channel if |swap| is true; otherwise, swap the left
  // and right channel back to the normal mode.
  // The dbus message will be dropped if this feature is not supported on the
  // |node_id|.
  virtual void SwapLeftRight(uint64_t node_id, bool swap) = 0;

  virtual void SetGlobalOutputChannelRemix(
      int32_t channels,
      const std::vector<double>& mixer) = 0;

  // Runs the callback as soon as the service becomes available.
  virtual void WaitForServiceToBeAvailable(
      WaitForServiceToBeAvailableCallback callback) = 0;

 protected:
  friend class CrasAudioClientTest;

  CrasAudioClient();
  virtual ~CrasAudioClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(CrasAudioClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_AUDIO_CRAS_AUDIO_CLIENT_H_
