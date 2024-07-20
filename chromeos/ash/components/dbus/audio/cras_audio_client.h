// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_AUDIO_CRAS_AUDIO_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_AUDIO_CRAS_AUDIO_CLIENT_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/dbus/audio/audio_node.h"
#include "chromeos/ash/components/dbus/audio/volume_state.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace dbus {
class Bus;
}

namespace ash {

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

    // Called when input node's gain changed.
    virtual void InputNodeGainChanged(uint64_t node_id, int volume);

    // Called when hotword is triggered.
    virtual void HotwordTriggered(uint64_t tv_sec, uint64_t tv_nsec);

    // Called when the number of active output streams has changed.
    virtual void NumberOfActiveStreamsChanged();

    // Called when the battery level for a Bluetooth headset changed.
    virtual void BluetoothBatteryChanged(const std::string& address,
                                         uint32_t level);

    // Called when the number of input streams with permission per client type
    // changed.
    virtual void NumberOfInputStreamsWithPermissionChanged(
        const base::flat_map<std::string, uint32_t>& num_input_streams);

    // Called when an audio survey should be triggered.
    virtual void SurveyTriggered(
        const base::flat_map<std::string, std::string>& survey_specific_data);

    // Called when a new speak-on-mute signal is detected.
    virtual void SpeakOnMuteDetected();

    // Called when ewma power reported by cras.
    virtual void EwmaPowerReported(double power);

    // Called when NumberOfNonChromeOutputStreamsChanged is detected.
    virtual void NumberOfNonChromeOutputStreamsChanged();

    // Called when num-stream-ignore-ui-gains is changed.
    virtual void NumStreamIgnoreUiGains(int32_t num);

    // Called when NumberOfArcStreamsChanged is detected.
    virtual void NumberOfArcStreamsChanged();

    // Called when there is a new active node to indicate whether sidetone is
    // supported.
    virtual void SidetoneSupportedChanged(bool supported);

   protected:
    virtual ~Observer();
  };

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance.
  //
  // Note:
  // `InitializeFake` does not shutdown `CrasAudioClient` automatically and it
  // can cause an unexpected side effect for other tests in automated tests.
  //
  // e.g.
  // A test leaves a client without shutdown. A following test expect that a
  // client does not exist.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static CrasAudioClient* Get();

  CrasAudioClient(const CrasAudioClient&) = delete;
  CrasAudioClient& operator=(const CrasAudioClient&) = delete;

  // Adds and removes the observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  // Returns true if this object has the given observer.
  virtual bool HasObserver(const Observer* observer) const = 0;

  // Gets the volume state, asynchronously.
  virtual void GetVolumeState(
      chromeos::DBusMethodCallback<VolumeState> callback) = 0;

  // Gets the default output buffer size in frames.
  virtual void GetDefaultOutputBufferSize(
      chromeos::DBusMethodCallback<int> callback) = 0;

  // Gets if system AEC is supported.
  virtual void GetSystemAecSupported(
      chromeos::DBusMethodCallback<bool> callback) = 0;

  // Gets any available group ID for the system AEC
  virtual void GetSystemAecGroupId(
      chromeos::DBusMethodCallback<int32_t> callback) = 0;

  // Gets if system NS is supported.
  virtual void GetSystemNsSupported(
      chromeos::DBusMethodCallback<bool> callback) = 0;

  // Gets if system AGC is supported.
  virtual void GetSystemAgcSupported(
      chromeos::DBusMethodCallback<bool> callback) = 0;

  // Gets an array of audio input and output nodes.
  virtual void GetNodes(
      chromeos::DBusMethodCallback<AudioNodeList> callback) = 0;

  // Gets the number of active output streams.
  virtual void GetNumberOfActiveOutputStreams(
      chromeos::DBusMethodCallback<int> callback) = 0;

  // Gets the number of input streams with permission per client type.
  virtual void GetNumberOfInputStreamsWithPermission(
      chromeos::DBusMethodCallback<base::flat_map<std::string, uint32_t>>) = 0;

  // Get the number of active non-chrome output streams.
  virtual void GetNumberOfNonChromeOutputStreams(
      chromeos::DBusMethodCallback<int32_t> callback) = 0;

  // Gets if speak-on-mute detection is enabled.
  virtual void GetSpeakOnMuteDetectionEnabled(
      chromeos::DBusMethodCallback<bool> callback) = 0;

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

  // Sets input noise cancellation state to |noise_cancellation_on| value.
  virtual void SetNoiseCancellationEnabled(bool noise_cancellation_on) = 0;

  // Gets if Noise Cancellation is supported.
  virtual void GetNoiseCancellationSupported(
      chromeos::DBusMethodCallback<bool> callback) = 0;

  // Sets input style transfer state to |style_transfer_on| value.
  virtual void SetStyleTransferEnabled(bool style_transfer_on) = 0;

  // Gets if style transfer is supported.
  virtual void GetStyleTransferSupported(
      chromeos::DBusMethodCallback<bool> callback) = 0;

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
                               chromeos::VoidDBusMethodCallback callback) = 0;

  // Enables or disables the usage of fixed A2DP packet size in CRAS.
  virtual void SetFixA2dpPacketSize(bool enabled) = 0;

  // Enables or disables CRAS to use Floss as the Bluetooth stack.
  virtual void SetFlossEnabled(bool enabled) = 0;

  // Enables or disables CRAS to use speak-on-mute detection.
  virtual void SetSpeakOnMuteDetection(bool enabled) = 0;

  virtual void SetEwmaPowerReportEnabled(bool enabled) = 0;

  virtual void SetSidetoneEnabled(bool enabled) = 0;

  // Gets the number of active output streams.
  virtual void GetSidetoneSupported(
      chromeos::DBusMethodCallback<bool> callback) = 0;

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

  // Sets the display |rotation| attribute of the primary active output device.
  // The dbus message will be dropped if this feature is not supported on the
  // |node_id|.
  virtual void SetDisplayRotation(uint64_t node_id,
                                  cras::DisplayRotation rotation) = 0;

  virtual void SetGlobalOutputChannelRemix(
      int32_t channels,
      const std::vector<double>& mixer) = 0;

  // Sets the player playback status. Possible status are "Playing", "Paused" or
  // "Stopped".
  virtual void SetPlayerPlaybackStatus(const std::string& playback_status) = 0;

  // Sets the player identity. Identity is a human readable title for the source
  // of the media player. This could be the name of the app or the name of the
  // site playing media.
  virtual void SetPlayerIdentity(const std::string& playback_identity) = 0;

  // Sets the current track position for the player in microseconds
  virtual void SetPlayerPosition(const int64_t& position) = 0;

  // Sets the current track duration for the player in microseconds
  virtual void SetPlayerDuration(const int64_t& duration) = 0;

  // Sets the current media metadata including Title, Album, and Artist.
  virtual void SetPlayerMetadata(
      const std::map<std::string, std::string>& metadata) = 0;

  // Asks Cras to resend battery level for Bluetooth device if exists .
  virtual void ResendBluetoothBattery() = 0;

  // Runs the callback as soon as the service becomes available.
  virtual void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) = 0;

  // Sets input force respect ui gains state to |force_repsect_ui_gains| value.
  virtual void SetForceRespectUiGains(bool force_respect_ui_gains) = 0;

  // Gets the number of streams ignoring UI Gains.
  virtual void GetNumStreamIgnoreUiGains(
      chromeos::DBusMethodCallback<int> callback) = 0;

  // Sets hfp_mic_sr state to |hfp_mic_sr_on| value.
  virtual void SetHfpMicSrEnabled(bool hfp_mic_sr_on) = 0;

  // Gets if hfp_mic_sr is supported.
  virtual void GetHfpMicSrSupported(
      chromeos::DBusMethodCallback<bool> callback) = 0;

  // Gets the number of active ARC streams.
  virtual void GetNumberOfArcStreams(
      chromeos::DBusMethodCallback<int32_t> callback) = 0;

 protected:
  friend class CrasAudioClientTest;

  CrasAudioClient();
  virtual ~CrasAudioClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_AUDIO_CRAS_AUDIO_CLIENT_H_
