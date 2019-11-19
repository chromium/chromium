// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_AUDIO_CRAS_AUDIO_HANDLER_H_
#define CHROMEOS_AUDIO_CRAS_AUDIO_HANDLER_H_

#include <stddef.h>
#include <stdint.h>

#include <queue>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "chromeos/audio/audio_device.h"
#include "chromeos/audio/audio_devices_pref_handler.h"
#include "chromeos/audio/audio_pref_observer.h"
#include "chromeos/dbus/audio/audio_node.h"
#include "chromeos/dbus/audio/cras_audio_client.h"
#include "chromeos/dbus/audio/volume_state.h"
#include "media/base/video_facing.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace service_manager {
class Connector;
}

namespace chromeos {

class AudioDevicesPrefHandler;

// Callback to handle response of methods without result.
// |result| is true if the method call is successfully completed, otherwise
// false.
using VoidCrasAudioHandlerCallback = base::OnceCallback<void(bool result)>;

// This class is not thread safe. The public functions should be called on
// browser main thread.
class COMPONENT_EXPORT(CHROMEOS_AUDIO) CrasAudioHandler
    : public CrasAudioClient::Observer,
      public AudioPrefObserver,
      public media::VideoCaptureObserver {
 public:
  typedef std::
      priority_queue<AudioDevice, std::vector<AudioDevice>, AudioDeviceCompare>
          AudioDevicePriorityQueue;
  typedef std::vector<uint64_t> NodeIdList;
  static constexpr int32_t kSystemAecGroupIdNotAvailable = -1;

  class AudioObserver {
   public:
    // Called when an active output volume changed.
    virtual void OnOutputNodeVolumeChanged(uint64_t node_id, int volume);

    // Called when output mute state changed.
    virtual void OnOutputMuteChanged(bool mute_on);

    // Called when active input node's gain changed.
    virtual void OnInputNodeGainChanged(uint64_t node_id, int gain);

    // Called when input mute state changed.
    virtual void OnInputMuteChanged(bool mute_on);

    // Called when audio nodes changed.
    virtual void OnAudioNodesChanged();

    // Called when active audio node changed.
    virtual void OnActiveOutputNodeChanged();

    // Called when active audio input node changed.
    virtual void OnActiveInputNodeChanged();

    // Called when output channel remixing changed.
    virtual void OnOutputChannelRemixingChanged(bool mono_on);

    // Called when hotword is detected.
    virtual void OnHotwordTriggered(uint64_t tv_sec, uint64_t tv_nsec);

    // Called when an initial output stream is opened.
    virtual void OnOutputStarted();

    // Called when the last output stream is closed.
    virtual void OnOutputStopped();

   protected:
    AudioObserver();
    virtual ~AudioObserver();
    DISALLOW_COPY_AND_ASSIGN(AudioObserver);
  };

  enum DeviceActivateType {
    ACTIVATE_BY_PRIORITY = 0,
    ACTIVATE_BY_USER,
    ACTIVATE_BY_RESTORE_PREVIOUS_STATE,
    ACTIVATE_BY_CAMERA
  };

  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize(
      service_manager::Connector* connector,
      scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler);

  // Sets the global instance for testing.
  static void InitializeForTesting();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize must be called first.
  static CrasAudioHandler* Get();

  // Overrides media::VideoCaptureObserver.
  void OnVideoCaptureStarted(media::VideoFacingMode facing) override;
  void OnVideoCaptureStopped(media::VideoFacingMode facing) override;

  // Adds an audio observer.
  void AddAudioObserver(AudioObserver* observer);

  // Removes an audio observer.
  void RemoveAudioObserver(AudioObserver* observer);

  // Returns true if keyboard mic exists.
  bool HasKeyboardMic();

  // Returns true if hotword input device exists.
  bool HasHotwordDevice();

  // Returns true if audio output is muted for the system.
  bool IsOutputMuted();

  // Returns true if audio output is muted for a device.
  bool IsOutputMutedForDevice(uint64_t device_id);

  // Returns true if audio input is muted.
  bool IsInputMuted();

  // Returns true if audio input is muted for a device.
  bool IsInputMutedForDevice(uint64_t device_id);

  // Returns true if the output volume is below the default mute volume level.
  bool IsOutputVolumeBelowDefaultMuteLevel();

  // Returns volume level in 0-100% range at which the volume should be muted.
  int GetOutputDefaultVolumeMuteThreshold();

  // Gets volume level in 0-100% range (0 being pure silence) for the current
  // active node.
  int GetOutputVolumePercent();

  // Gets volume level in 0-100% range (0 being pure silence) for a device.
  int GetOutputVolumePercentForDevice(uint64_t device_id);

  // Gets gain level in 0-100% range (0 being pure silence) for the current
  // active node.
  int GetInputGainPercent();

  // Gets volume level in 0-100% range (0 being pure silence) for a device.
  int GetInputGainPercentForDevice(uint64_t device_id);

  // Returns node_id of the primary active output node.
  uint64_t GetPrimaryActiveOutputNode() const;

  // Returns the node_id of the primary active input node.
  uint64_t GetPrimaryActiveInputNode() const;

  // Gets the audio devices back in |device_list|.
  void GetAudioDevices(AudioDeviceList* device_list) const;

  bool GetPrimaryActiveOutputDevice(AudioDevice* device) const;

  // Returns the device matched with |type|. Assuming there is only one device
  // matched the |type|, if there is more than one matched devices, it will
  // return the first one found.
  const AudioDevice* GetDeviceByType(AudioDeviceType type);

  // Gets the default output buffer size in frames.
  void GetDefaultOutputBufferSize(int32_t* buffer_size) const;

  // Whether there is alternative input/output audio device.
  bool has_alternative_input() const;
  bool has_alternative_output() const;

  // Sets all active output devices' volume levels to |volume_percent|, whose
  // range is from 0-100%.
  void SetOutputVolumePercent(int volume_percent);

  // Sets all active input devices' gain level to |gain_percent|, whose range is
  // from 0-100%.
  void SetInputGainPercent(int gain_percent);

  // Adjusts all active output devices' volume up (positive percentage) or down
  // (negative percentage).
  void AdjustOutputVolumeByPercent(int adjust_by_percent);

  // Adjusts all active output devices' volume to a minimum audible level if it
  // is too low.
  void AdjustOutputVolumeToAudibleLevel();

  // Mutes or unmutes audio output device.
  void SetOutputMute(bool mute_on);

  // Mutes or unmutes audio input device.
  void SetInputMute(bool mute_on);

  // Switches active audio device to |device|. |activate_by| indicates why
  // the device is switched to active: by user's manual choice, by priority,
  // or by restoring to its previous active state.
  void SwitchToDevice(const AudioDevice& device,
                      bool notify,
                      DeviceActivateType activate_by);

  // Sets volume/gain level for a device.
  void SetVolumeGainPercentForDevice(uint64_t device_id, int value);

  // Sets the mute for device.
  void SetMuteForDevice(uint64_t device_id, bool mute_on);

  // Activates or deactivates keyboard mic if there's one.
  void SetKeyboardMicActive(bool active);

  // Changes the active nodes to the nodes specified by |new_active_ids|.
  // The caller can pass in the "complete" active node list of either input
  // nodes, or output nodes, or both. If only input nodes are passed in,
  // it will only change the input nodes' active status, output nodes will NOT
  // be changed; similarly for the case if only output nodes are passed.
  // If the nodes specified in |new_active_ids| are already active, they will
  // remain active. Otherwise, the old active nodes will be de-activated before
  // we activate the new nodes with the same type(input/output).
  // DEPRECATED in favor of |SetActiveInputNodes| and |SetActiveOutputNodes|.
  void ChangeActiveNodes(const NodeIdList& new_active_ids);

  // Sets the set of active input nodes. Empty |node_ids| will deactivate all
  // input devices.
  // |node_ids| is expected to contain only existing input node IDs - the
  // method will fail if this is not the case.
  // Returns whether the acive nodes were successfully set.
  bool SetActiveInputNodes(const NodeIdList& node_ids);

  // Sets the set of active output nodes. Empty |node_ids| will deactivate all
  // output devices.
  // |node_ids| is expected to contain only existing output node IDs - the
  // method will fail if this is not the case.
  // Returns whether the acive nodes were successfully set.
  bool SetActiveOutputNodes(const NodeIdList& node_ids);

  // Sets |hotword_model| to the given |node_id|.
  // |hotword_model| is expected to be in format <language>_<region> with lower
  // cases. E.g., "en_us".
  // The callback will receive a boolean which indicates if the hotword model is
  // successfully set.
  void SetHotwordModel(uint64_t node_id,
                       const std::string& hotword_model,
                       VoidCrasAudioHandlerCallback callback);

  // Swaps the left and right channel of the internal speaker.
  // Swap the left and right channel if |swap| is true; otherwise, swap the left
  // and right channel back to the normal mode.
  // If the feature is not supported on the device, nothing happens.
  void SwapInternalSpeakerLeftRightChannel(bool swap);

  // Accessibility mono audio setting: sets the output mono or not.
  void SetOutputMonoEnabled(bool enabled);

  // If necessary, sets the starting point for re-discovering the active HDMI
  // output device caused by device entering/exiting docking mode, HDMI display
  // changing resolution, or chromeos device suspend/resume. If
  // |force_rediscovering| is true, it will force to set the starting point for
  // re-discovering the active HDMI output device again if it has been in the
  // middle of rediscovering the HDMI active output device.
  void SetActiveHDMIOutoutRediscoveringIfNecessary(bool force_rediscovering);

  const AudioDevice* GetDeviceFromId(uint64_t device_id) const;

  // Returns true the device has dual internal microphones(front and rear).
  bool HasDualInternalMic() const;

  // Returns true if |device| is front or rear microphone.
  bool IsFrontOrRearMic(const AudioDevice& device) const;

  // Switches to either front or rear microphone depending on the
  // the use case. It should be called from a user initiated action.
  void SwitchToFrontOrRearMic();

  // Returns if system AEC is supported in CRAS.
  bool system_aec_supported() const;

  // Returns the system AEC group ID. If no group ID is specified, -1 is
  // returned.
  int32_t system_aec_group_id() const;

 protected:
  explicit CrasAudioHandler(
      service_manager::Connector* connector,
      scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler);
  ~CrasAudioHandler() override;

 private:
  friend class CrasAudioHandlerTest;

  // CrasAudioClient::Observer overrides.
  void AudioClientRestarted() override;
  void NodesChanged() override;
  void ActiveOutputNodeChanged(uint64_t node_id) override;
  void ActiveInputNodeChanged(uint64_t node_id) override;
  void OutputNodeVolumeChanged(uint64_t node_id, int volume) override;
  void HotwordTriggered(uint64_t tv_sec, uint64_t tv_nsec) override;
  void NumberOfActiveStreamsChanged() override;

  // AudioPrefObserver overrides.
  void OnAudioPolicyPrefChanged() override;

  // Sets the |active_device| to be active.
  // If |notify|, notifies Active*NodeChange.
  // Saves device active states in prefs. |activate_by| indicates how
  // the device was activated.
  void SetActiveDevice(const AudioDevice& active_device,
                       bool notify,
                       DeviceActivateType activate_by);

  // Shared implementation for |SetActiveInputNodes| and |SetActiveOutputNodes|.
  bool SetActiveNodes(const NodeIdList& node_ids, bool is_input);

  // Sets list of active input or output nodes to |devices|.
  // If |is_input| is set, active input nodes will be set, otherwise active
  // output nodes will be set.
  // For each device in |devices| it is expected device.is_input == is_input.
  void SetActiveDevices(const AudioDeviceList& devices, bool is_input);

  // Saves |device|'s state in pref. If |active| is true, |activate_by|
  // indicates how |device| is activated.
  void SaveDeviceState(const AudioDevice& device,
                       bool active,
                       DeviceActivateType activate_by);

  // Sets up the audio device state based on audio policy and audio settings
  // saved in prefs.
  void SetupAudioInputState();
  void SetupAudioOutputState();

  // Sets up the additional active audio node's state.
  void SetupAdditionalActiveAudioNodeState(uint64_t node_id);

  const AudioDevice* GetDeviceFromStableDeviceId(
      uint64_t stable_device_id) const;
  const AudioDevice* GetKeyboardMic() const;

  const AudioDevice* GetHotwordDevice() const;

  // Initializes audio state, which should only be called when CrasAudioHandler
  // is created or cras audio client is restarted.
  void InitializeAudioState();

  void InitializeAudioAfterCrasServiceAvailable(bool service_is_available);

  // Applies the audio muting policies whenever the user logs in or policy
  // change notification is received.
  void ApplyAudioPolicy();

  // Sets output volume of |node_id| to |volume|.
  void SetOutputNodeVolume(uint64_t node_id, int volume);

  void SetOutputNodeVolumePercent(uint64_t node_id, int volume_percent);

  // Sets output mute state to |mute_on| internally, returns true if output mute
  // is set.
  bool SetOutputMuteInternal(bool mute_on);

  // Sets input gain of |node_id| to |gain|.
  void SetInputNodeGain(uint64_t node_id, int gain);

  void SetInputNodeGainPercent(uint64_t node_id, int gain_percent);

  // Sets input mute state to |mute_on| internally.
  void SetInputMuteInternal(bool mute_on);

  // Calls CRAS over D-Bus to get nodes data.
  void GetNodes();

  // Calls CRAS over D-Bus to get the number of active output streams.
  void GetNumberOfOutputStreams();

  // Updates the current audio nodes list and switches the active device
  // if needed.
  void UpdateDevicesAndSwitchActive(const AudioNodeList& nodes);

  // Returns true if the current active device is changed to
  // |new_active_device|.
  bool ChangeActiveDevice(const AudioDevice& new_active_device);

  // Returns true if there are any device changes for input or output
  // specified by |is_input|, by comparing |audio_devices_| with |new_nodes|.
  // Passes the new nodes discovered in *|new_discovered|.
  // *|device_removed| indicates if any devices have been removed.
  // *|active_device_removed| indicates if the current active device has been
  // removed.
  bool HasDeviceChange(const AudioNodeList& new_nodes,
                       bool is_input,
                       AudioDevicePriorityQueue* new_discovered,
                       bool* device_removed,
                       bool* active_device_removed);

  // Handles dbus callback for GetNodes.
  void HandleGetNodes(base::Optional<chromeos::AudioNodeList> node_list);

  void HandleGetNumActiveOutputStreams(
      base::Optional<int> num_active_output_streams);

  // Adds an active node.
  // If there is no active node, |node_id| will be switched to become the
  // primary active node. Otherwise, it will be added as an additional active
  // node.
  void AddActiveNode(uint64_t node_id, bool notify);

  // Adds |node_id| into additional active nodes.
  void AddAdditionalActiveNode(uint64_t node_id, bool notify);

  // Removes |node_id| from additional active nodes.
  void RemoveActiveNodeInternal(uint64_t node_id, bool notify);

  void UpdateAudioAfterHDMIRediscoverGracePeriod();

  bool IsHDMIPrimaryOutputDevice() const;

  void StartHDMIRediscoverGracePeriod();

  bool hdmi_rediscovering() const { return hdmi_rediscovering_; }

  void SetHDMIRediscoverGracePeriodForTesting(int duration_in_ms);

  enum DeviceStatus {
    OLD_DEVICE,
    NEW_DEVICE,
    CHANGED_DEVICE,
  };

  // Checks if |device| is a newly discovered, changed, or existing device for
  // the nodes sent from NodesChanged signal.
  DeviceStatus CheckDeviceStatus(const AudioDevice& device);

  void NotifyActiveNodeChanged(bool is_input);

  // Returns true if it retrieves an active audio device from user preference
  // among the current |audio_devices_|.
  bool GetActiveDeviceFromUserPref(bool is_input, AudioDevice* device);

  // Pauses all active streams.
  void PauseAllStreams();

  // Handles either input or output device changes, specified by |is_input|.
  void HandleAudioDeviceChange(bool is_input,
                               const AudioDevicePriorityQueue& devices_pq,
                               const AudioDevicePriorityQueue& hotplug_nodes,
                               bool has_device_change,
                               bool has_device_removed,
                               bool active_device_removed);

  // Handles non-hotplug nodes change cases.
  void HandleNonHotplugNodesChange(
      bool is_input,
      const AudioDevicePriorityQueue& hotplug_nodes,
      bool has_device_change,
      bool has_device_removed,
      bool active_device_removed);

  // Handles the regular user hotplug case.
  void HandleHotPlugDevice(
      const AudioDevice& hotplug_device,
      const AudioDevicePriorityQueue& device_priority_queue);

  void SwitchToTopPriorityDevice(bool is_input);

  // Switch to previous active device if it is found, otherwise, switch
  // to the top priority device.
  void SwitchToPreviousActiveDeviceIfAvailable(bool is_input);

  // Activates the internal mic attached with the camera specified by
  // |camera_facing|.
  void ActivateMicForCamera(media::VideoFacingMode camera_facing);

  // Activates the front or rear mic that is consistent with the active camera.
  // Note: This should only be called for the dural camera/mic use case.
  void ActivateInternalMicForActiveCamera();

  // Returns the microphone for the camera with |camera_facing|.
  const AudioDevice* GetMicForCamera(media::VideoFacingMode camera_facing);

  bool IsCameraOn() const;

  // Returns true if there are any external devices.
  bool HasExternalDevice(bool is_input) const;

  // Calling dbus to get default output buffer size.
  void GetDefaultOutputBufferSizeInternal();

  // Handle dbus callback for GetDefaultOutputBufferSize.
  void HandleGetDefaultOutputBufferSize(base::Optional<int> buffer_size);

  // Calling dbus to get system AEC supported flag.
  void GetSystemAecSupported();

  // Calling dbus to get system AEC supported flag on main thread.
  void GetSystemAecSupportedOnMainThread();

  // Handle dbus callback for GetSystemAecSupported.
  void HandleGetSystemAecSupported(base::Optional<bool> system_aec_supported);

  // Calling dbus to get the system AEC group id if available.
  void GetSystemAecGroupId();

  // Calling dbus to get any available system AEC group id on main thread.
  void GetSystemAecGroupIdOnMainThread();

  // Handle dbus callback for GetSystemAecGroupId.
  void HandleGetSystemAecGroupId(base::Optional<int32_t> system_aec_group_id);

  void OnVideoCaptureStartedOnMainThread(media::VideoFacingMode facing);
  void OnVideoCaptureStoppedOnMainThread(media::VideoFacingMode facing);

  service_manager::Connector* const connector_;

  scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler_;
  base::ObserverList<AudioObserver>::Unchecked observers_;

  // Audio data and state.
  AudioDeviceMap audio_devices_;

  AudioDevicePriorityQueue input_devices_pq_;
  AudioDevicePriorityQueue output_devices_pq_;

  bool output_mute_on_ = false;
  bool input_mute_on_ = false;
  int output_volume_ = 0;
  int input_gain_ = 0;
  uint64_t active_output_node_id_ = 0;
  uint64_t active_input_node_id_ = 0;
  bool has_alternative_input_ = false;
  bool has_alternative_output_ = false;

  bool output_mute_locked_ = false;

  // Audio output channel counts.
  int32_t output_channels_ = 2;
  bool output_mono_enabled_ = false;

  // Timer for HDMI re-discovering grace period.
  base::OneShotTimer hdmi_rediscover_timer_;
  int hdmi_rediscover_grace_period_duration_in_ms_ = 2000;
  bool hdmi_rediscovering_ = false;

  bool cras_service_available_ = false;

  bool initializing_audio_state_ = false;
  int init_volume_;
  uint64_t init_node_id_;
  int init_volume_count_ = 0;

  bool front_camera_on_ = false;
  bool rear_camera_on_ = false;

  // Default output buffer size in frames.
  int32_t default_output_buffer_size_ = 512;

  bool system_aec_supported_ = false;
  int32_t system_aec_group_id_ = kSystemAecGroupIdNotAvailable;

  int num_active_output_streams_ = 0;

  // Task runner of browser main thread. All member variables should be accessed
  // on this thread.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  base::WeakPtrFactory<CrasAudioHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrasAudioHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_AUDIO_CRAS_AUDIO_HANDLER_H_
