// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUDIO_CRAS_AUDIO_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_AUDIO_CRAS_AUDIO_HANDLER_H_

#include <stddef.h>
#include <stdint.h>

#include <cstdint>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/observer_list.h"
#include "base/scoped_observation_traits.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/audio_device_metrics_handler.h"
#include "chromeos/ash/components/audio/audio_devices_pref_handler.h"
#include "chromeos/ash/components/audio/audio_pref_observer.h"
#include "chromeos/ash/components/audio/audio_selection_notification_handler.h"
#include "chromeos/ash/components/dbus/audio/audio_node.h"
#include "chromeos/ash/components/dbus/audio/cras_audio_client.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "chromeos/ash/components/dbus/audio/volume_state.h"
#include "media/base/video_facing.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/events/devices/microphone_mute_switch_monitor.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace ash {

class AudioDevicesPrefHandler;

// Callback to handle response of methods without result.
// |result| is true if the method call is successfully completed, otherwise
// false.
using VoidCrasAudioHandlerCallback = base::OnceCallback<void(bool result)>;

// Callback to handle the dbus message for whether noise cancellation is
// supported by the board.
using OnNoiseCancellationSupportedCallback = base::OnceCallback<void()>;

// Callback to handle the dbus message for whether style transfer is
// supported by the board.
using OnStyleTransferSupportedCallback = base::OnceCallback<void()>;

// Callback to handle the dbus message for whether hfp_mic_sr is
// supported by the board.
using OnHfpMicSrSupportedCallback = base::OnceCallback<void()>;

// This class is not thread safe. The public functions should be called on
// browser main thread.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_AUDIO) CrasAudioHandler
    : public CrasAudioClient::Observer,
      public ui::MicrophoneMuteSwitchMonitor::Observer,
      public AudioPrefObserver,
      public media::VideoCaptureObserver,
      public media_session::mojom::MediaControllerObserver {
 public:
  typedef std::vector<uint64_t> NodeIdList;

  enum class SurveyType {
    kGeneral,
    kBluetooth,
    kOutputProc,
  };

  static constexpr char kSurveyNameKey[] = "SurveyName";
  static constexpr char kSurveyNameGeneral[] = "GENERAL";
  static constexpr char kSurveyNameBluetooth[] = "BLUETOOTH";
  static constexpr char kSurveyNameOutputProc[] = "OUTPUTPROC";

  // Key-value mapping type for audio survey specific data.
  // For audio satisfaction survey, it contains
  //  - StreamType: Usage of the stream, e.g., multimedia.
  //  - ClientType: Client of the stream, e.g., Chrome or ARC++.
  //  - NodeType: Pair of the active input/output device types, e.g., USB_USB.
  // The content can be extended when other types of survey is added.
  typedef base::flat_map<std::string, std::string> AudioSurveyData;
  class AudioSurvey {
   public:
    AudioSurvey();
    ~AudioSurvey();
    AudioSurvey(const AudioSurvey&) = delete;
    AudioSurvey& operator=(const AudioSurvey&) = delete;

    SurveyType type() const { return type_; }
    AudioSurveyData data() const { return data_; }

    void set_type(SurveyType type) { type_ = type; }
    void clear_data() { data_.clear(); }
    void AddData(std::string key, std::string value) {
      data_.emplace(key, value);
    }

   private:
    SurveyType type_;
    AudioSurveyData data_;
  };

  // A Delegate class to expose chrome browser functionality to ash.
  class Delegate {
   public:
    Delegate() = default;

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate() = default;

    // Opens the OS Settings audio page.
    virtual void OpenSettingsAudioPage() const = 0;
  };

  static constexpr int32_t kSystemAecGroupIdNotAvailable = -1;

  // Grace period for removing notification.
  static constexpr base::TimeDelta kRemoveNotificationDelay =
      base::Milliseconds(5000);

  enum class InputMuteChangeMethod {
    kKeyboardButton,
    kPhysicalShutter,
    kOther,
  };

  // This enum is used to record UMA histogram values and should not be
  // reordered. Please keep in sync with `AudioSettingsChangeSource` in
  // src/tools/metrics/histograms/enums.xml.
  enum class AudioSettingsChangeSource {
    kSystemTray = 0,
    kOsSettings,
    kAccelerator,
    kVideoConferenceTray,
    kMaxValue = kVideoConferenceTray,
  };

  static constexpr base::TimeDelta kMetricsDelayTimerInterval =
      base::Seconds(2);
  static constexpr char kForceRespectUiGainsHistogramName[] =
      "Cras.ForceRespectUiGains";
  static constexpr char kInputGainChangedSourceHistogramName[] =
      "Cras.InputGainChangedSource";
  static constexpr char kInputGainChangedHistogramName[] =
      "Cras.InputGainChanged";
  static constexpr char kInputGainMuteSourceHistogramName[] =
      "Cras.InputGainMutedSource";
  static constexpr char kOutputVolumeChangedSourceHistogramName[] =
      "Cras.OutputVolumeChangedSource";
  static constexpr char kOutputVolumeMuteSourceHistogramName[] =
      "Cras.OutputVolumeMutedSource";
  static constexpr char kNoiseCancellationEnabledSourceHistogramName[] =
      "Cras.NoiseCancellationEnabledSource";

  class AudioObserver {
   public:
    AudioObserver(const AudioObserver&) = delete;
    AudioObserver& operator=(const AudioObserver&) = delete;

    // Called when an active output volume changed.
    virtual void OnOutputNodeVolumeChanged(uint64_t node_id, int volume);

    // Called when output mute state changed.
    virtual void OnOutputMuteChanged(bool mute_on);

    // Called when active input node's gain changed.
    virtual void OnInputNodeGainChanged(uint64_t node_id, int gain);

    // Called when input mute state changed.
    virtual void OnInputMuteChanged(bool mute_on, InputMuteChangeMethod method);

    // Called when the state of input mute hw switch state changes.
    virtual void OnInputMutedByMicrophoneMuteSwitchChanged(bool muted);

    // Called when the state of input mute by security curtain changes.
    virtual void OnInputMutedBySecurityCurtainChanged(bool muted);

    // Called when audio nodes changed.
    virtual void OnAudioNodesChanged();

    // Called when active audio node changed.
    virtual void OnActiveOutputNodeChanged();

    // Called when active audio input node changed.
    virtual void OnActiveInputNodeChanged();

    // Called when output channel remixing changed.
    virtual void OnOutputChannelRemixingChanged(bool mono_on);

    // Called when noise cancellation state changed.
    virtual void OnNoiseCancellationStateChanged();

    // Called when style transfer state changed.
    virtual void OnStyleTransferStateChanged();

    // Called when force respect ui gains state changed.
    virtual void OnForceRespectUiGainsStateChanged();

    // Called when hfp_mic_sr state changed.
    virtual void OnHfpMicSrStateChanged();

    // Called when hotword is detected.
    virtual void OnHotwordTriggered(uint64_t tv_sec, uint64_t tv_nsec);

    // Called when the battery level change is reported over the Hands-Free
    // Profile for a Bluetooth headset.
    // The address is a Bluetooth address as 6 bytes written in hexadecimal and
    // separated by colons. Example: 00:11:22:33:44:FF
    // The level ranges from 0 to 100. Erroneous value reported by the headset
    // will be ignored and won't trigger this callback.
    virtual void OnBluetoothBatteryChanged(const std::string& address,
                                           uint32_t level);

    // Called when the number of input streams with permission per client type
    // changed.
    virtual void OnNumberOfInputStreamsWithPermissionChanged();

    // Called when an initial output stream is opened.
    virtual void OnOutputStarted();

    // Called when the last output stream is closed.
    virtual void OnOutputStopped();

    // Called when an initial output stream, not in chrome, is opened.
    virtual void OnNonChromeOutputStarted();

    // Called when the last output stream is closed, not in chrome.
    virtual void OnNonChromeOutputStopped();

    // Called when the audio survey like to trigger an audio survey.
    // CRAS owns the trigger to send out an audio survey as opposed to trigger
    // from any Chrome/UI elements as CRAS has the most context to determine
    // when to sent out audio survey, for example when an input stream living
    // for >= 30 seconds is closed.
    // CRAS also has full control on what data to send to Chrome. These survey
    // specific data will be attached with each survey response for analysis.
    // Currently this supports general audio and Bluetooth audio surveys.
    // The survey to trigger is determined by the type of the `AudioSurvey`
    // passed in.
    virtual void OnSurveyTriggered(const AudioSurvey& survey);

    // Called when a speak-on-mute is detected.
    virtual void OnSpeakOnMuteDetected();

    // Called when num-stream-ignore-ui-gains state is changed.
    virtual void OnNumStreamIgnoreUiGainsChanged(int32_t num);

    // Called when number of ARC streams is changed.
    virtual void OnNumberOfArcStreamsChanged(int32_t num);

   protected:
    AudioObserver();
    virtual ~AudioObserver();
  };

  enum class ClientType {
    CHROME = 0,
    ARC,
    VM_TERMINA,
    VM_PLUGIN,
    VM_BOREALIS,
    LACROS,
    UNKNOWN,
  };

  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize(
      mojo::PendingRemote<media_session::mojom::MediaControllerManager>
          media_controller_manager,
      scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler);

  // Same as Initialize function but also initializing a delegate class.
  static void InitializeDelegate(
      mojo::PendingRemote<media_session::mojom::MediaControllerManager>
          media_controller_manager,
      scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler,
      std::unique_ptr<Delegate> delegate);

  // Sets the global instance for testing.
  // Consider using |ScopedCrasAudioHandlerForTesting| instead, as that
  // guarantees things get properly cleaned up.
  static void InitializeForTesting();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize must be called first.
  static CrasAudioHandler* Get();

  CrasAudioHandler(const CrasAudioHandler&) = delete;
  CrasAudioHandler& operator=(const CrasAudioHandler&) = delete;

  // Overrides media::VideoCaptureObserver.
  void OnVideoCaptureStarted(media::VideoFacingMode facing) override;
  void OnVideoCaptureStopped(media::VideoFacingMode facing) override;

  // Overrides media_session::mojom::MediaControllerObserver.
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override;
  void MediaSessionMetadataChanged(
      const std::optional<media_session::MediaMetadata>& metadata) override;
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& actions)
      override {}
  void MediaSessionChanged(
      const std::optional<base::UnguessableToken>& request_id) override {}
  void MediaSessionPositionChanged(
      const std::optional<media_session::MediaPosition>& position) override;

  // ui::MicrophoneMuteSwitchMonitor::Observer:
  void OnMicrophoneMuteSwitchValueChanged(bool muted) override;

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

  // Returns true if audio output is muted for the system by policy.
  bool IsOutputMutedByPolicy();

  // Returns true if audio output is muted for the system by security curtain.
  bool IsOutputMutedBySecurityCurtain();

  // Returns true if audio input is muted.
  bool IsInputMuted();

  // Returns true if audio input is muted for the system by security curtain.
  bool IsInputMutedBySecurityCurtain();

  // Returns true if audio input is muted for a device.
  bool IsInputMutedForDevice(uint64_t device_id);

  // Returns true if the output volume is below the default mute volume level.
  bool IsOutputVolumeBelowDefaultMuteLevel();

  // Returns volume level in 0-100% range at which the volume should be muted.
  int GetOutputDefaultVolumeMuteThreshold() const;

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

  // Gets the simple usage input or output audio devices.
  static AudioDeviceList GetSimpleUsageAudioDevices(
      const AudioDeviceMap& audio_devices,
      bool is_input);

  const AudioDeviceMap& GetAudioDevicesMapForTesting(
      bool is_current_device) const;

  // Gets the primary active output device in |device|.
  // Returns true if the primary active output device is successfully obtained.
  // Returns false if no active device is obtained or |device| is null.
  bool GetPrimaryActiveOutputDevice(AudioDevice* device) const;

  // Gets the primary active input device in |device|.
  // Returns true if the primary active input device is successfully obtained.
  // Returns false if no active device is obtained or |device| is null.
  bool GetPrimaryActiveInputDevice(AudioDevice* device) const;

  // Returns the device matched with |type|. Assuming there is only one device
  // matched the |type|, if there is more than one matched devices, it will
  // return the first one found.
  const AudioDevice* GetDeviceByType(AudioDeviceType type);

  // Returns a map contains number of input streams with permission per
  // ClientType. If a ClientType is not in the map, we assume the number is 0.
  base::flat_map<ClientType, uint32_t> GetNumberOfInputStreamsWithPermission()
      const;

  // Gets the default output buffer size in frames.
  void GetDefaultOutputBufferSize(int32_t* buffer_size) const;

  // Returns noise cancellation supported if:
  // - Overall board/device supports noise cancellation
  // - Audio device has bit for Noise Cancellation set in `audio_effect`.
  bool IsNoiseCancellationSupportedForDevice(uint64_t device_id);

  // Gets the pref state of input noise cancellation.
  bool GetNoiseCancellationState() const;

  // Refreshes the input device noise cancellation state.
  void RefreshNoiseCancellationState();

  // Updates noise cancellation state in `CrasAudioClient` and
  // `AudioDevicesPrefHandler` to the provided value. `source` records to
  // metrics who changed the noise cancellation state.
  void SetNoiseCancellationState(bool noise_cancellation_on,
                                 AudioSettingsChangeSource source);

  // Get if noise cancellation is supported by the board.
  void RequestNoiseCancellationSupported(
      OnNoiseCancellationSupportedCallback callback);

  // Simulate noise cancellation support in a test.
  void SetNoiseCancellationSupportedForTesting(bool supported);

  // Returns style transfer supported if:
  // - Overall board/device supports style transfer
  // - Audio device has bit for style transfer set in `audio_effect`.
  bool IsStyleTransferSupportedForDevice(uint64_t device_id);

  // Gets the pref state of input style transfer.
  bool GetStyleTransferState() const;

  // Refreshes the input device style transfer state.
  void RefreshStyleTransferState();

  // Updates style transfer state in `CrasAudioClient` and
  // `AudioDevicesPrefHandler` to the provided value.
  void SetStyleTransferState(bool style_transfer_on);

  // Get if style transfer is supported by the board.
  void RequestStyleTransferSupported(OnStyleTransferSupportedCallback callback);

  // Simulate style transfer support in a test.
  void SetStyleTransferSupportedForTesting(bool supported);

  // Gets the state of input force respect ui gains state.
  bool GetForceRespectUiGainsState() const;

  // Refreshes the input device force respect ui gains state.
  void RefreshForceRespectUiGainsState();

  // Makes a DBus call to set the state of input force respect ui gains.
  void SetForceRespectUiGainsState(bool state);

  // Returns hfp_mic_sr supported.
  bool IsHfpMicSrSupportedForDevice(uint64_t device_id);

  // Gets if hfp_mic_sr is supported by the board.
  void RequestHfpMicSrSupported(OnHfpMicSrSupportedCallback callback);

  // Simulates hfp_mic_sr support in a test.
  void SetHfpMicSrSupportedForTesting(bool supported);

  // Gets the pref state of hfp_mic_sr.
  bool GetHfpMicSrState() const;

  // Refreshes the input device hfp_mic_sr state.
  void RefreshHfpMicSrState();

  // Updates hfp_mic_sr state in `CrasAudioClient` and
  // `AudioDevicesPrefHandler` to the provided value. `source` records to
  // metrics who changed the hfp_mic_sr state.
  void SetHfpMicSrState(bool hfp_mic_sr_on, AudioSettingsChangeSource source);

  // Whether there is alternative input/output audio device.
  bool has_alternative_input() const;
  bool has_alternative_output() const;

  // Sets the current display |rotation| to CrasAudioHandler and updates the
  // |rotation| to the internal speaker.
  void SetDisplayRotation(cras::DisplayRotation rotation);

  // Sets all active output devices' volume levels to |volume_percent|, whose
  // range is from 0-100%.
  void SetOutputVolumePercent(int volume_percent);

  // Sets all active input devices' gain level to |gain_percent|, whose range is
  // from 0-100%.
  void SetInputGainPercent(int gain_percent);

  // Adjusts all active output devices' volume up (positive percentage) or down
  // (negative percentage).
  void AdjustOutputVolumeByPercent(int adjust_by_percent);

  // Adjusts all active output devices' volume higher by one volume step.
  void IncreaseOutputVolumeByOneStep(int one_step_percent);

  // Adjusts all active output devices' volume lower by one volume step
  void DecreaseOutputVolumeByOneStep(int one_step_percent);

  // Adjusts all active output devices' volume to a minimum audible level if it
  // is too low.
  void AdjustOutputVolumeToAudibleLevel();

  // Mutes or unmutes audio output device.
  void SetOutputMute(bool mute_on);

  // Mutes or unmutes audio output device including the `source` to record to
  // metrics.
  void SetOutputMute(bool mute_on,
                     CrasAudioHandler::AudioSettingsChangeSource source);

  // Mutes or unmutes audio output device by security curtain
  void SetOutputMuteLockedBySecurityCurtain(bool mute_on);

  // Mutes or unmutes audio input device.
  void SetInputMute(bool mute_on, InputMuteChangeMethod method);

  // Mutes or unmutes audio input device including the `source` to record to
  // metrics.
  void SetInputMute(bool mute_on,
                    InputMuteChangeMethod method,
                    CrasAudioHandler::AudioSettingsChangeSource source);

  // Mutes or unmutes audio input device by security curtain
  void SetInputMuteLockedBySecurityCurtain(bool mute);

  // Switches active audio device to |device|. |activate_by| indicates why
  // the device is switched to active: by user's manual choice, by priority,
  // or by restoring to its previous active state.
  void SwitchToDevice(const AudioDevice& device,
                      bool notify,
                      DeviceActivateType activate_by);

  // Opens the OS Settings audio page.
  void OpenSettingsAudioPage();

  // Sets volume/gain level for a device.
  void SetVolumeGainPercentForDevice(uint64_t device_id, int value);

  // Sets the mute for device.
  void SetMuteForDevice(uint64_t device_id, bool mute_on);

  // Sets the mute for device including the `source` to record to metrics.
  void SetMuteForDevice(uint64_t device_id,
                        bool mute_on,
                        CrasAudioHandler::AudioSettingsChangeSource source);

  // Activates or deactivates keyboard mic if there's one.
  void SetKeyboardMicActive(bool active);

  // Enables or disables the speak-on-mute detection.
  void SetSpeakOnMuteDetection(bool som_on);

  // Enable or disable sidetone;
  void SetSidetoneEnabled(bool enabled);

  // Enable or disable input stream ewma power report.
  void SetEwmaPowerReportEnabled(bool enabled);

  // Returns the last reported ewma power.
  double GetEwmaPower();

  // Get whether the sidetone is enabled or not
  bool GetSidetoneEnabled() const;

  // Based on the output device, get whether sidetone is available or not.
  bool IsSidetoneSupported() const;

  // Request to CRAS to check whether the current device support sidetone or
  // not.
  void UpdateSidetoneSupportedState();

  // Handles dbus callback for GetNodes.
  void HandleGetSidetoneSupported(std::optional<bool> available);

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

  // There are some audio devices which are not meant for simple usage. Keyboard
  // mic is an example of a non simple device. There are cases when we need to
  // know programmatically if there is an audio input device for simple usage
  // available. Checking `active_input_node_id_` being non zero is not
  // sufficient in this case. A non simple device can be the active input node
  // during cras initialization depeneding the audio device enumeration process.
  // This function returns true if an input device for simple usage is
  // available.
  bool HasActiveInputDeviceForSimpleUsage() const;

  // Returns true if |device| is front or rear microphone.
  bool IsFrontOrRearMic(const AudioDevice& device) const;

  // Switches to either front or rear microphone depending on the
  // the use case. It should be called from a user initiated action.
  void SwitchToFrontOrRearMic();

  bool input_muted_by_microphone_mute_switch() const {
    return input_muted_by_microphone_mute_switch_;
  }

  // Returns if system AEC is supported in CRAS or not.
  bool system_aec_supported() const;

  // Returns if noise cancellation is supported in CRAS or not.
  bool noise_cancellation_supported() const;

  // Returns if style transfer is supported in CRAS or not.
  bool style_transfer_supported() const;

  // Returns if hfp_mic_sr is supported in CRAS or not.
  bool hfp_mic_sr_supported() const;

  // Returns the system AEC group ID. If no group ID is specified, -1 is
  // returned.
  int32_t system_aec_group_id() const;

  // Returns if system NS is supported in CRAS or not.
  bool system_ns_supported() const;

  // Returns if system AGC is supported in CRAS or not.
  bool system_agc_supported() const;

  // Returns number of streams ignoring UI gains.
  int32_t num_stream_ignore_ui_gains() const;

  // Asks  CRAS to resend BluetoothBatteryChanged signal, used in cases when
  // Chrome cleans up the stored battery information but still has the device
  // connected afterward. For example: User logout.
  void ResendBluetoothBattery();

  void SetPrefHandlerForTesting(
      scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler);

  int32_t NumberOfNonChromeOutputStreams() const;
  int32_t NumberOfChromeOutputStreams() const;
  int32_t NumberOfArcStreams() const;

  // Simulate number of ARC streams changing in a test.
  void SetNumberOfArcStreamsForTesting(int32_t num);

 protected:
  CrasAudioHandler(
      mojo::PendingRemote<media_session::mojom::MediaControllerManager>
          media_controller_manager,
      scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler);
  CrasAudioHandler(
      mojo::PendingRemote<media_session::mojom::MediaControllerManager>
          media_controller_manager,
      scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler,
      std::unique_ptr<Delegate> delegate);
  ~CrasAudioHandler() override;

 private:
  friend class CrasAudioHandlerTest;

  // A helper function to set up CrasAudioHandler.
  void SetupCrasAudioHandler(
      scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler);

  // CrasAudioClient::Observer overrides.
  void AudioClientRestarted() override;
  void NodesChanged() override;
  void ActiveOutputNodeChanged(uint64_t node_id) override;
  void ActiveInputNodeChanged(uint64_t node_id) override;
  void OutputNodeVolumeChanged(uint64_t node_id, int volume) override;
  void InputNodeGainChanged(uint64_t node_id, int gain) override;
  void HotwordTriggered(uint64_t tv_sec, uint64_t tv_nsec) override;
  void BluetoothBatteryChanged(const std::string& address,
                               uint32_t level) override;
  void NumberOfInputStreamsWithPermissionChanged(
      const base::flat_map<std::string, uint32_t>& num_input_streams) override;
  void NumberOfActiveStreamsChanged() override;
  void SurveyTriggered(const base::flat_map<std::string, std::string>&
                           survey_specific_data) override;
  void SpeakOnMuteDetected() override;
  void EwmaPowerReported(double power) override;
  void NumberOfNonChromeOutputStreamsChanged() override;
  void NumStreamIgnoreUiGains(int32_t num) override;
  void NumberOfArcStreamsChanged() override;
  void SidetoneSupportedChanged(bool supported) override;

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

  AudioDevice ConvertAudioNodeWithModifiedPriority(const AudioNode& node);

  const AudioDevice* GetDeviceFromStableDeviceId(
      bool is_input,
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

  // Helper method to apply the conditional audio output mute change.
  void UpdateAudioOutputMute();

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

  void GetNumberOfNonChromeOutputStreams();

  // Calls CRAS over D-Bus to get the number of ARC streams.
  void GetNumberOfArcStreams();

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
                       AudioDeviceList* new_discovered,
                       AudioDeviceList* removed_devices,
                       bool* active_device_removed);

  // Handles dbus callback for GetNodes.
  void HandleGetNodes(std::optional<AudioNodeList> node_list);

  void HandleGetNumActiveOutputStreams(
      std::optional<int> num_active_output_streams);

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
  bool ShouldSwitchToHotPlugDevice(const AudioDevice& hotplug_device) const;

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
                               const AudioDeviceList& devices,
                               const AudioDeviceList& hotplug_nodes,
                               bool has_device_change,
                               bool has_device_removed,
                               bool active_device_removed);

  // Handles non-hotplug nodes change cases.
  void HandleNonHotplugNodesChange(bool is_input,
                                   const AudioDeviceList& devices,
                                   const AudioDeviceList& hotplug_nodes,
                                   bool has_device_change,
                                   bool has_device_removed,
                                   bool active_device_removed);

  // Handles the regular user hotplug case with user priority.
  void HandleHotPlugDeviceByUserPriority(const AudioDevice& hotplug_device);

  // Switch to the top priority device in |devices|. |devices| must be a list
  // of devices with the same direction.
  void SwitchToTopPriorityDevice(const AudioDeviceList& devices);

  // Switch to previous active device if it is found, otherwise, switch
  // to the top priority device.
  void SwitchToPreviousActiveDeviceIfAvailable(bool is_input,
                                               const AudioDeviceList& devices);

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
  void HandleGetDefaultOutputBufferSize(std::optional<int> buffer_size);

  // Calling dbus to get current number of input streams with permission and
  // storing the result in number_of_input_streams_with_permission_.
  void GetNumberOfInputStreamsWithPermissionInternal();

  // Static function converts |client_type_str| to ClientType which used by
  // HandleGetNumberOfInputStreamsWithPermission.
  static ClientType ConvertClientTypeStringToEnum(std::string client_type_str);

  // Handle dbus callback for GetNumberOfInputStreamsWithPermission.
  void HandleGetNumberOfInputStreamsWithPermission(
      std::optional<base::flat_map<std::string, uint32_t>> num_input_streams);
  void HandleGetNumberOfNonChromeOutputStreams(
      std::optional<int32_t> num_output_streams);

  // Handle dbus callback for GetNumberOfArcStreams.
  void HandleGetNumberOfArcStreams(std::optional<int32_t> num_arc_streams);

  // Calling dbus to get system AEC supported flag.
  void GetSystemAecSupported();

  // Handle dbus callback for GetSystemNoiseCancellationSupported.
  void HandleGetNoiseCancellationSupported(
      OnNoiseCancellationSupportedCallback callback,
      std::optional<bool> system_noise_cancellation_supported);

  // Handle dbus callback for GetSystemStyleTransferSupported.
  void HandleGetStyleTransferSupported(
      OnStyleTransferSupportedCallback callback,
      std::optional<bool> system_style_transfer_supported);

  // Handle dbus callback for IsHfpMicSrSupported.
  void HandleGetHfpMicSrSupported(OnHfpMicSrSupportedCallback callback,
                                  std::optional<bool> hfp_mic_sr_supported);

  // Handle dbus callback for GetSystemAecSupported.
  void HandleGetSystemAecSupported(std::optional<bool> system_aec_supported);

  // Calling dbus to get the system AEC group id if available.
  void GetSystemAecGroupId();

  // Handle dbus callback for GetSystemAecGroupId.
  void HandleGetSystemAecGroupId(std::optional<int32_t> system_aec_group_id);

  // Calling dbus to get system NS supported flag.
  void GetSystemNsSupported();

  // Handle dbus callback for GetSystemNsSupported.
  void HandleGetSystemNsSupported(std::optional<bool> system_ns_supported);

  // Calling dbus to get system AGC supported flag.
  void GetSystemAgcSupported();

  // Handle dbus callback for GetSystemAgcSupported.
  void HandleGetSystemAgcSupported(std::optional<bool> system_agc_supported);

  void OnVideoCaptureStartedOnMainThread(media::VideoFacingMode facing);
  void OnVideoCaptureStoppedOnMainThread(media::VideoFacingMode facing);

  void BindMediaControllerObserver();

  // Handle null Metadata from MediaSession.
  void HandleMediaSessionMetadataReset();

  // Calls CRAS over D-Bus to get the number of streams ignoring Ui Gains.
  void GetNumStreamIgnoreUiGains();

  // Handle dbus callback for GetNumStreamIgnoreUiGains.
  void HandleGetNumStreamIgnoreUiGains(
      std::optional<int32_t> num_stream_ignore_ui_gains);

  // Checks if a given set of devices is seen before. If seen, return the
  // preferred device.
  const std::optional<AudioDevice> GetPreferredDeviceIfDeviceSetSeenBefore(
      bool is_input,
      const AudioDeviceList& devices) const;

  // Syncs device preference set map with currently activated device, called
  // whenever user perefences have changed.
  void SyncDevicePrefSetMap(bool is_input);

  // Handles the regular user hotplug case. Show notification for unseen device
  // set.
  void HandleHotPlugDeviceWithNotification(const AudioDevice& hotplug_device);

  // Handles the audio selection notification being removed.
  void HandleRemoveNotification(bool is_input);

  // Handles the system boots or restarts case.
  // - If the device boots with only one device, activate it automatically.
  // - If the device set was seen before, activate the preferred one.
  // - Otherwise if a 3.5mm headphone is connected, activate it and don't show
  // notification.
  // - Otherwise activate the most recent activated device and show
  // notification.
  // - Otherwise if there is an internal device, activate it and show
  // notification.
  // - Otherwise when no most recent activated device and no internal device (a
  // brand new chromebox), activate the highest priority device based on the
  // pre-determined priority list and show notification.
  void HandleSystemBoots(bool is_input, const AudioDeviceList& devices);

  // Activates the most recently active device. Return false if no device in the
  // most recently active device list is currently connected, otherwise
  // return true.
  bool ActivateMostRecentActiveDevice(bool is_input);

  // Static helper function to abstract the |AudioSurvey| from input
  // |survey_specific_data|.
  static std::unique_ptr<CrasAudioHandler::AudioSurvey> AbstractAudioSurvey(
      const base::flat_map<std::string, std::string>& survey_specific_data);

  mojo::Remote<media_session::mojom::MediaControllerManager>
      media_controller_manager_;

  mojo::Remote<media_session::mojom::MediaController>
      media_session_controller_remote_;

  mojo::Receiver<media_session::mojom::MediaControllerObserver>
      media_controller_observer_receiver_{this};

  scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler_;
  base::ObserverList<AudioObserver>::Unchecked observers_;

  // Handles firing of audio selection related metrics.
  AudioDeviceMetricsHandler audio_device_metrics_handler_;

  // Handles creation and display of audio selection notification.
  AudioSelectionNotificationHandler audio_selection_notification_handler_;

  // Timer for remove notification.
  base::OneShotTimer remove_notification_timer_for_input_;
  base::OneShotTimer remove_notification_timer_for_output_;

  // Audio data and state.
  AudioDeviceMap audio_devices_;

  // Previous audio devices before new device is connected or current device is
  // disconnected. Used for histogram purpose to understand the context when the
  // system makes a switch or not switch decision.
  AudioDeviceMap previous_audio_devices_;

  bool output_mute_on_ = false;
  bool input_mute_on_ = false;
  int output_volume_ = 0;
  int input_gain_ = 0;
  uint64_t active_output_node_id_ = 0;
  uint64_t active_input_node_id_ = 0;
  bool has_alternative_input_ = false;
  bool has_alternative_output_ = false;

  bool output_mute_forced_by_policy_ = false;
  bool output_mute_forced_by_security_curtain_ = false;
  bool input_mute_forced_by_security_curtain_ = false;

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

  base::flat_map<ClientType, uint32_t> number_of_input_streams_with_permission_;

  bool system_aec_supported_ = false;
  bool noise_cancellation_supported_ = false;
  bool style_transfer_supported_ = false;
  int32_t system_aec_group_id_ = kSystemAecGroupIdNotAvailable;
  bool system_ns_supported_ = false;
  bool system_agc_supported_ = false;
  bool hfp_mic_sr_supported_ = false;

  int num_active_output_streams_ = 0;
  int32_t num_active_nonchrome_output_streams_ = 0;

  bool fetch_media_session_duration_ = false;

  // Whether the audio input is muted because the microphone mute switch is on.
  // In this case, input mute changes will be disabled.
  bool input_muted_by_microphone_mute_switch_ = false;

  // Whether the speak-on-mute detection is enabled in CRAS.
  bool speak_on_mute_detection_on_ = false;

  // Whether the sidetone is enabled in CRAS.
  bool sidetone_enabled_ = false;

  // Whether the current has sidetone available or not.
  bool sidetone_supported_ = false;

  // Whether the ewma power report is enabled in CRAS.
  bool ewma_power_report_enabled_ = false;

  // The last reported ewma power.
  double ewma_power_ = 0;

  // Indicates whether the audio selection notification should be displayed.
  bool should_show_notification_ = false;

  // Task runner of browser main thread. All member variables should be accessed
  // on this thread.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  cras::DisplayRotation display_rotation_ = cras::DisplayRotation::ROTATE_0;

  int num_stream_ignore_ui_gains_ = 0;

  int32_t num_arc_streams_ = 0;

  std::unique_ptr<Delegate> delegate_;

  base::WeakPtrFactory<CrasAudioHandler> weak_ptr_factory_{this};
};

// Helper class that will initialize the `CrasAudioHandler` for testing in its
// constructor, and clean things up in its destructor.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_AUDIO)
    ScopedCrasAudioHandlerForTesting {
 public:
  // `ScopedCrasAudioHandlerForTesting` expects that there is no audio client
  // running. This class starts and shuts down an audio client automatically at
  // its constructor and destructor.
  ScopedCrasAudioHandlerForTesting();
  ScopedCrasAudioHandlerForTesting(const ScopedCrasAudioHandlerForTesting&) =
      delete;
  ScopedCrasAudioHandlerForTesting& operator=(
      const ScopedCrasAudioHandlerForTesting&) = delete;
  ~ScopedCrasAudioHandlerForTesting();

  CrasAudioHandler& Get();

 private:
  std::unique_ptr<FakeCrasAudioClient> fake_cras_audio_client_;
};

}  // namespace ash

namespace base {

template <>
struct ScopedObservationTraits<ash::CrasAudioHandler,
                               ash::CrasAudioHandler::AudioObserver> {
  static void AddObserver(ash::CrasAudioHandler* source,
                          ash::CrasAudioHandler::AudioObserver* observer) {
    source->AddAudioObserver(observer);
  }
  static void RemoveObserver(ash::CrasAudioHandler* source,
                             ash::CrasAudioHandler::AudioObserver* observer) {
    source->RemoveAudioObserver(observer);
  }
};

}  // namespace base

#endif  // CHROMEOS_ASH_COMPONENTS_AUDIO_CRAS_AUDIO_HANDLER_H_
