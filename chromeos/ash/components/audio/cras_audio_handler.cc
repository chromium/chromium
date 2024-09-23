// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/cras_audio_handler.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/system/system_monitor.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/audio_device_encoding.h"
#include "chromeos/ash/components/audio/audio_device_id.h"
#include "chromeos/ash/components/audio/audio_devices_pref_handler_stub.h"
#include "chromeos/ash/components/dbus/audio/cras_audio_client.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "chromeos/ash/components/dbus/audio/floss_media_client.h"
#include "device/bluetooth/floss/floss_features.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {
namespace {

using ::std::max;
using ::std::min;

// Default value for unmuting, as a percent in the range [0, 100].
// Used when sound is unmuted, but volume was less than kMuteThresholdPercent.
const int kDefaultUnmuteVolumePercent = 4;

// Volume value which should be considered as muted in range [0, 100].
const int kMuteThresholdPercent = 1;

// Mixer matrix, [0.5, 0.5; 0.5, 0.5]
const double kStereoToMono[] = {0.5, 0.5, 0.5, 0.5};
// Mixer matrix, [1, 0; 0, 1]
const double kStereoToStereo[] = {1, 0, 0, 1};

// Number of entries we're willing to store in preferences.
const int kMaxDeviceStoredInPref = 100;

CrasAudioHandler* g_cras_audio_handler = nullptr;

bool IsSameAudioDevice(const AudioDevice& a, const AudioDevice& b) {
  return a.stable_device_id == b.stable_device_id && a.is_input == b.is_input &&
         a.type == b.type && a.device_name == b.device_name;
}

bool IsDeviceInList(const AudioDevice& device, const AudioNodeList& node_list) {
  for (const AudioNode& node : node_list) {
    if (device.stable_device_id == node.StableDeviceId()) {
      return true;
    }
  }
  return false;
}

// Gets the current state of the microphone mute switch. If the switch is on,
// cras will be kept in the muted state. The switch disables the internal audio
// input.
bool IsMicrophoneMuteSwitchOn() {
  return ui::MicrophoneMuteSwitchMonitor::Get()->microphone_mute_switch_on();
}

}  // namespace

// TODO(b/277300962): Clean up the default value and handle the unset case.
CrasAudioHandler::AudioSurvey::AudioSurvey() : type_(SurveyType::kGeneral) {}

CrasAudioHandler::AudioSurvey::~AudioSurvey() = default;

CrasAudioHandler::AudioObserver::AudioObserver() = default;

CrasAudioHandler::AudioObserver::~AudioObserver() = default;

void CrasAudioHandler::AudioObserver::OnOutputNodeVolumeChanged(
    uint64_t /* node_id */,
    int /* volume */) {}

void CrasAudioHandler::AudioObserver::OnInputNodeGainChanged(
    uint64_t /* node_id */,
    int /* gain */) {}

void CrasAudioHandler::AudioObserver::OnOutputMuteChanged(bool /* mute_on */) {}

void CrasAudioHandler::AudioObserver::OnInputMuteChanged(
    bool /* mute_on */,
    InputMuteChangeMethod /* method */) {}

void CrasAudioHandler::AudioObserver::OnInputMutedByMicrophoneMuteSwitchChanged(
    bool /* muted */) {}

void CrasAudioHandler::AudioObserver::OnInputMutedBySecurityCurtainChanged(
    bool /* muted */) {}

void CrasAudioHandler::AudioObserver::OnAudioNodesChanged() {}

void CrasAudioHandler::AudioObserver::OnActiveOutputNodeChanged() {}

void CrasAudioHandler::AudioObserver::OnActiveInputNodeChanged() {}

void CrasAudioHandler::AudioObserver::OnOutputChannelRemixingChanged(
    bool /* mono_on */) {}

void CrasAudioHandler::AudioObserver::OnNoiseCancellationStateChanged() {}

void CrasAudioHandler::AudioObserver::OnStyleTransferStateChanged() {}

void CrasAudioHandler::AudioObserver::OnForceRespectUiGainsStateChanged() {}

void CrasAudioHandler::AudioObserver::OnHfpMicSrStateChanged() {}

void CrasAudioHandler::AudioObserver::OnHotwordTriggered(
    uint64_t /* tv_sec */,
    uint64_t /* tv_nsec */) {}

void CrasAudioHandler::AudioObserver::OnBluetoothBatteryChanged(
    const std::string& /* address */,
    uint32_t /* level */) {}

void CrasAudioHandler::AudioObserver::
    OnNumberOfInputStreamsWithPermissionChanged() {}

void CrasAudioHandler::AudioObserver::OnOutputStarted() {}

void CrasAudioHandler::AudioObserver::OnOutputStopped() {}

void CrasAudioHandler::AudioObserver::OnNonChromeOutputStarted() {}

void CrasAudioHandler::AudioObserver::OnNonChromeOutputStopped() {}

void CrasAudioHandler::AudioObserver::OnSurveyTriggered(
    const AudioSurvey& /*survey*/) {}

void CrasAudioHandler::AudioObserver::OnSpeakOnMuteDetected() {}

void CrasAudioHandler::AudioObserver::OnNumStreamIgnoreUiGainsChanged(
    int32_t num) {}

void CrasAudioHandler::AudioObserver::OnNumberOfArcStreamsChanged(int32_t num) {
}

void CrasAudioHandler::NumberOfNonChromeOutputStreamsChanged() {
  GetNumberOfNonChromeOutputStreams();
}

void CrasAudioHandler::NumberOfArcStreamsChanged() {
  GetNumberOfArcStreams();
}

// static
void CrasAudioHandler::Initialize(
    mojo::PendingRemote<media_session::mojom::MediaControllerManager>
        media_controller_manager,
    scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler) {
  g_cras_audio_handler = new CrasAudioHandler(
      std::move(media_controller_manager), audio_pref_handler);
}

// static
void CrasAudioHandler::InitializeDelegate(
    mojo::PendingRemote<media_session::mojom::MediaControllerManager>
        media_controller_manager,
    scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler,
    std::unique_ptr<Delegate> delegate) {
  g_cras_audio_handler =
      new CrasAudioHandler(std::move(media_controller_manager),
                           audio_pref_handler, std::move(delegate));
}

// static
void CrasAudioHandler::InitializeForTesting() {
  CHECK(CrasAudioClient::Get()) << "CrasAudioClient must be initialized.";

  // Make sure FlossMediaClient has been initialized.
  // TODO(b/228608730): Remove this after Floss bypasses CRAS to receive media
  // information directly from the provider.
  if (floss::features::IsFlossEnabled() && !FlossMediaClient::Get()) {
    FlossMediaClient::InitializeFake();
  }
  CrasAudioHandler::Initialize(mojo::NullRemote(),
                               new AudioDevicesPrefHandlerStub());
}

// static
void CrasAudioHandler::Shutdown() {
  delete g_cras_audio_handler;
  g_cras_audio_handler = nullptr;
}

// static
CrasAudioHandler* CrasAudioHandler::Get() {
  return g_cras_audio_handler;
}

void CrasAudioHandler::OnVideoCaptureStarted(media::VideoFacingMode facing) {
  DCHECK(main_task_runner_);
  if (!main_task_runner_->BelongsToCurrentThread()) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CrasAudioHandler::OnVideoCaptureStartedOnMainThread,
                       weak_ptr_factory_.GetWeakPtr(), facing));
    return;
  }
  // Unittest may call this from the main thread.
  OnVideoCaptureStartedOnMainThread(facing);
}

void CrasAudioHandler::OnVideoCaptureStopped(media::VideoFacingMode facing) {
  DCHECK(main_task_runner_);
  if (!main_task_runner_->BelongsToCurrentThread()) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CrasAudioHandler::OnVideoCaptureStoppedOnMainThread,
                       weak_ptr_factory_.GetWeakPtr(), facing));
    return;
  }
  // Unittest may call this from the main thread.
  OnVideoCaptureStoppedOnMainThread(facing);
}

void CrasAudioHandler::OnVideoCaptureStartedOnMainThread(
    media::VideoFacingMode facing) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  // Do nothing if the device doesn't have both front and rear microphones.
  if (!HasDualInternalMic()) {
    return;
  }

  bool camera_is_already_on = IsCameraOn();
  switch (facing) {
    case media::MEDIA_VIDEO_FACING_USER:
      front_camera_on_ = true;
      break;
    case media::MEDIA_VIDEO_FACING_ENVIRONMENT:
      rear_camera_on_ = true;
      break;
    default:
      LOG_IF(WARNING, facing == media::NUM_MEDIA_VIDEO_FACING_MODES)
          << "On the device with dual microphone, got video capture "
          << "notification with invalid camera facing mode value";
      return;
  }

  // If the camera is already on before this notification, don't change active
  // input. In the case that both cameras are turned on at the same time, we
  // won't change the active input after the first camera is turned on. We only
  // support the use case of one camera on at a time. The third party
  // developer can turn on/off both microphones with extension api if they like
  // to.
  if (camera_is_already_on) {
    return;
  }

  // If the current active input is an external device, keep it.
  const AudioDevice* active_input = GetDeviceFromId(active_input_node_id_);
  if (active_input && active_input->IsExternalDevice()) {
    return;
  }

  // Activate the correct mic for the current active camera.
  ActivateMicForCamera(facing);
}

void CrasAudioHandler::OnVideoCaptureStoppedOnMainThread(
    media::VideoFacingMode facing) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  // Do nothing if the device doesn't have both front and rear microphones.
  if (!HasDualInternalMic()) {
    return;
  }

  switch (facing) {
    case media::MEDIA_VIDEO_FACING_USER:
      front_camera_on_ = false;
      break;
    case media::MEDIA_VIDEO_FACING_ENVIRONMENT:
      rear_camera_on_ = false;
      break;
    default:
      LOG_IF(WARNING, facing == media::NUM_MEDIA_VIDEO_FACING_MODES)
          << "On the device with dual microphone, got video capture "
          << "notification with invalid camera facing mode value";
      return;
  }

  // If not all cameras are turned off, don't change active input. In the case
  // that both cameras are turned on at the same time before one of them is
  // stopped, we won't change active input until all of them are stopped.
  // We only support the use case of one camera on at a time. The third party
  // developer can turn on/off both microphones with extension api if they like
  // to.
  if (IsCameraOn()) {
    return;
  }

  // If the current active input is an external device, keep it.
  const AudioDevice* active_input = GetDeviceFromId(active_input_node_id_);
  if (active_input && active_input->IsExternalDevice()) {
    return;
  }

  // Switch to front mic properly.
  DeviceActivateType activated_by =
      HasExternalDevice(true) ? DeviceActivateType::kActivateByUser
                              : DeviceActivateType::kActivateByPriority;
  SwitchToDevice(*GetDeviceByType(AudioDeviceType::kFrontMic), true,
                 activated_by);
}

void CrasAudioHandler::HandleMediaSessionMetadataReset() {
  const std::map<std::string, std::string> empty_metadata_map = {
      {"title", ""}, {"artist", ""}, {"album", ""}};

  // TODO(b/228608730): Remove this after Floss bypasses CRAS to receive media
  // information directly from the provider.
  if (floss::features::IsFlossEnabled()) {
    FlossMediaClient::Get()->SetPlayerMetadata(empty_metadata_map);
    FlossMediaClient::Get()->SetPlayerIdentity("");
    FlossMediaClient::Get()->SetPlayerPlaybackStatus("stopped");
  } else {
    CrasAudioClient::Get()->SetPlayerMetadata(empty_metadata_map);
    CrasAudioClient::Get()->SetPlayerIdentity("");
    CrasAudioClient::Get()->SetPlayerPlaybackStatus("stopped");
  }
}

void CrasAudioHandler::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  if (!session_info) {
    return;
  }

  std::string state = session_info->playback_state ==
                              media_session::mojom::MediaPlaybackState::kPlaying
                          ? "playing"
                          : "paused";

  // TODO(b/228608730): Remove this after Floss bypasses CRAS to receive media
  // information directly from the provider.
  if (floss::features::IsFlossEnabled()) {
    FlossMediaClient::Get()->SetPlayerPlaybackStatus(state);
  } else {
    CrasAudioClient::Get()->SetPlayerPlaybackStatus(state);
  }
}

void CrasAudioHandler::MediaSessionMetadataChanged(
    const std::optional<media_session::MediaMetadata>& metadata) {
  if (!metadata || metadata->IsEmpty()) {
    HandleMediaSessionMetadataReset();
    return;
  }

  const std::map<std::string, std::string> metadata_map = {
      {"title", base::UTF16ToUTF8(metadata->title)},
      {"artist", base::UTF16ToUTF8(metadata->artist)},
      {"album", base::UTF16ToUTF8(metadata->album)}};
  const std::string source_title = base::UTF16ToUTF8(metadata->source_title);

  // Assume media duration/length should always change with new metadata.
  fetch_media_session_duration_ = true;

  // TODO(b/228608730): Remove this after Floss bypasses CRAS to receive media
  // information directly from the provider.
  if (floss::features::IsFlossEnabled()) {
    FlossMediaClient::Get()->SetPlayerMetadata(metadata_map);
    FlossMediaClient::Get()->SetPlayerIdentity(source_title);
  } else {
    CrasAudioClient::Get()->SetPlayerMetadata(metadata_map);
    CrasAudioClient::Get()->SetPlayerIdentity(source_title);
  }
}

void CrasAudioHandler::MediaSessionPositionChanged(
    const std::optional<media_session::MediaPosition>& position) {
  if (!position) {
    return;
  }

  int64_t duration = 0;
  if (fetch_media_session_duration_) {
    duration = position->duration().InMicroseconds();
    if (duration > 0) {
      // TODO(b/228608730): Remove this after Floss bypasses the media
      // information from CRAS.
      if (floss::features::IsFlossEnabled()) {
        FlossMediaClient::Get()->SetPlayerDuration(duration);
      } else {
        CrasAudioClient::Get()->SetPlayerDuration(duration);
      }
      fetch_media_session_duration_ = false;
    }
  }

  int64_t current_position = position->GetPosition().InMicroseconds();
  if (current_position < 0 || (duration > 0 && current_position > duration)) {
    return;
  }

  // TODO(b/228608730): Remove this after Floss bypasses CRAS to receive media
  // information directly from the provider.
  if (floss::features::IsFlossEnabled()) {
    FlossMediaClient::Get()->SetPlayerPosition(current_position);
  } else {
    CrasAudioClient::Get()->SetPlayerPosition(current_position);
  }
}

void CrasAudioHandler::OnMicrophoneMuteSwitchValueChanged(bool muted) {
  input_muted_by_microphone_mute_switch_ = muted;
  SetInputMute(muted, InputMuteChangeMethod::kPhysicalShutter);

  for (auto& observer : observers_) {
    observer.OnInputMutedByMicrophoneMuteSwitchChanged(muted);
  }
}

void CrasAudioHandler::AddAudioObserver(AudioObserver* observer) {
  observers_.AddObserver(observer);
}

void CrasAudioHandler::RemoveAudioObserver(AudioObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool CrasAudioHandler::HasKeyboardMic() {
  return GetKeyboardMic() != nullptr;
}

bool CrasAudioHandler::HasHotwordDevice() {
  return GetHotwordDevice() != nullptr;
}

bool CrasAudioHandler::IsOutputMuted() {
  return output_mute_on_;
}

bool CrasAudioHandler::IsOutputMutedForDevice(uint64_t device_id) {
  const AudioDevice* device = GetDeviceFromId(device_id);
  if (!device) {
    return false;
  }
  DCHECK(!device->is_input);
  return audio_pref_handler_->GetMuteValue(*device);
}

bool CrasAudioHandler::IsOutputMutedByPolicy() {
  return output_mute_forced_by_policy_;
}

bool CrasAudioHandler::IsOutputMutedBySecurityCurtain() {
  return output_mute_forced_by_security_curtain_;
}

bool CrasAudioHandler::IsOutputVolumeBelowDefaultMuteLevel() {
  return output_volume_ <= kMuteThresholdPercent;
}

bool CrasAudioHandler::IsInputMuted() {
  return input_mute_on_;
}

bool CrasAudioHandler::IsInputMutedBySecurityCurtain() {
  return input_mute_forced_by_security_curtain_;
}

bool CrasAudioHandler::IsInputMutedForDevice(uint64_t device_id) {
  const AudioDevice* device = GetDeviceFromId(device_id);
  if (!device) {
    return false;
  }
  DCHECK(device->is_input);
  // We don't record input mute state for each device in the prefs,
  // for any non-active input device, we assume mute is off.
  if (device->id == active_input_node_id_) {
    return input_mute_on_;
  }
  return false;
}

int CrasAudioHandler::GetOutputDefaultVolumeMuteThreshold() const {
  return kMuteThresholdPercent;
}

int CrasAudioHandler::GetOutputVolumePercent() {
  return output_volume_;
}

int CrasAudioHandler::GetOutputVolumePercentForDevice(uint64_t device_id) {
  if (device_id == active_output_node_id_) {
    return output_volume_;
  }
  const AudioDevice* device = GetDeviceFromId(device_id);
  return static_cast<int>(audio_pref_handler_->GetOutputVolumeValue(device));
}

int CrasAudioHandler::GetInputGainPercent() {
  return input_gain_;
}

int CrasAudioHandler::GetInputGainPercentForDevice(uint64_t device_id) {
  if (device_id == active_input_node_id_) {
    return input_gain_;
  }
  const AudioDevice* device = GetDeviceFromId(device_id);
  return static_cast<int>(audio_pref_handler_->GetInputGainValue(device));
}

uint64_t CrasAudioHandler::GetPrimaryActiveOutputNode() const {
  return active_output_node_id_;
}

uint64_t CrasAudioHandler::GetPrimaryActiveInputNode() const {
  return active_input_node_id_;
}

void CrasAudioHandler::GetAudioDevices(AudioDeviceList* device_list) const {
  device_list->clear();
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    device_list->push_back(device);
  }
}

// static.
AudioDeviceList CrasAudioHandler::GetSimpleUsageAudioDevices(
    const AudioDeviceMap& audio_devices,
    bool is_input) {
  AudioDeviceList device_list;
  for (const auto& item : audio_devices) {
    const AudioDevice& device = item.second;
    // Do not count non simple usage devices.
    if (!device.is_for_simple_usage()) {
      continue;
    }
    if (device.is_input == is_input) {
      device_list.push_back(device);
    }
  }
  return device_list;
}

const AudioDeviceMap& CrasAudioHandler::GetAudioDevicesMapForTesting(
    bool is_current_device) const {
  return is_current_device ? audio_devices_ : previous_audio_devices_;
}

bool CrasAudioHandler::GetPrimaryActiveOutputDevice(AudioDevice* device) const {
  const AudioDevice* active_device = GetDeviceFromId(active_output_node_id_);
  if (!active_device || !device) {
    return false;
  }
  *device = *active_device;
  return true;
}

bool CrasAudioHandler::GetPrimaryActiveInputDevice(AudioDevice* device) const {
  const AudioDevice* active_device = GetDeviceFromId(active_input_node_id_);
  if (!active_device || !device) {
    return false;
  }
  *device = *active_device;
  return true;
}

const AudioDevice* CrasAudioHandler::GetDeviceByType(AudioDeviceType type) {
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (device.type == type) {
      return &device;
    }
  }
  return nullptr;
}

base::flat_map<CrasAudioHandler::ClientType, uint32_t>
CrasAudioHandler::GetNumberOfInputStreamsWithPermission() const {
  return number_of_input_streams_with_permission_;
}

void CrasAudioHandler::GetDefaultOutputBufferSize(int32_t* buffer_size) const {
  *buffer_size = default_output_buffer_size_;
}

bool CrasAudioHandler::IsNoiseCancellationSupportedForDevice(
    uint64_t device_id) {
  if (!noise_cancellation_supported()) {
    return false;
  }

  const AudioDevice* device = GetDeviceFromId(device_id);
  if (!device) {
    return false;
  }

  if (!device->is_input) {
    return false;
  }

  return device->audio_effect & cras::EFFECT_TYPE_NOISE_CANCELLATION;
}

bool CrasAudioHandler::GetNoiseCancellationState() const {
  return audio_pref_handler_->GetNoiseCancellationState();
}

void CrasAudioHandler::RefreshNoiseCancellationState() {
  if (!noise_cancellation_supported()) {
    return;
  }

  const AudioDevice* internal_mic =
      GetDeviceByType(AudioDeviceType::kInternalMic);

  if (!internal_mic) {
    return;
  }

  // Refresh should only update the state in CRAS and leave the preference
  // as-is.
  CrasAudioClient::Get()->SetNoiseCancellationEnabled(
      GetNoiseCancellationState() &&
      (internal_mic->audio_effect & cras::EFFECT_TYPE_NOISE_CANCELLATION));
}

void CrasAudioHandler::SetNoiseCancellationState(
    bool noise_cancellation_on,
    AudioSettingsChangeSource source) {
  CrasAudioClient::Get()->SetNoiseCancellationEnabled(noise_cancellation_on);
  audio_pref_handler_->SetNoiseCancellationState(noise_cancellation_on);

  for (auto& observer : observers_) {
    observer.OnNoiseCancellationStateChanged();
  }
  base::UmaHistogramEnumeration(kNoiseCancellationEnabledSourceHistogramName,
                                source);
}

void CrasAudioHandler::RequestNoiseCancellationSupported(
    OnNoiseCancellationSupportedCallback callback) {
  CrasAudioClient::Get()->GetNoiseCancellationSupported(
      base::BindOnce(&CrasAudioHandler::HandleGetNoiseCancellationSupported,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrasAudioHandler::HandleGetNoiseCancellationSupported(
    OnNoiseCancellationSupportedCallback callback,
    std::optional<bool> noise_cancellation_supported) {
  if (!noise_cancellation_supported.has_value()) {
    LOG(ERROR)
        << "cras_audio_handler: Failed to retrieve noise cancellation support";
  } else {
    noise_cancellation_supported_ = noise_cancellation_supported.value();
  }

  std::move(callback).Run();
}

void CrasAudioHandler::SetNoiseCancellationSupportedForTesting(bool supported) {
  noise_cancellation_supported_ = supported;
}

bool CrasAudioHandler::IsStyleTransferSupportedForDevice(uint64_t device_id) {
  if (!style_transfer_supported()) {
    return false;
  }

  const AudioDevice* device = GetDeviceFromId(device_id);
  if (!device) {
    return false;
  }

  return device->audio_effect & cras::EFFECT_TYPE_STYLE_TRANSFER;
}

bool CrasAudioHandler::GetStyleTransferState() const {
  return audio_pref_handler_->GetStyleTransferState();
}

void CrasAudioHandler::RefreshStyleTransferState() {
  if (!style_transfer_supported()) {
    return;
  }

  const AudioDevice* internal_mic =
      GetDeviceByType(AudioDeviceType::kInternalMic);

  if (!internal_mic) {
    return;
  }

  // Refresh should only update the state in CRAS and leave the preference
  // as-is.
  CrasAudioClient::Get()->SetStyleTransferEnabled(
      GetStyleTransferState() &&
      (internal_mic->audio_effect & cras::EFFECT_TYPE_STYLE_TRANSFER));
}

void CrasAudioHandler::SetStyleTransferState(bool style_transfer_on) {
  CrasAudioClient::Get()->SetStyleTransferEnabled(style_transfer_on);
  audio_pref_handler_->SetStyleTransferState(style_transfer_on);

  for (auto& observer : observers_) {
    observer.OnStyleTransferStateChanged();
  }
}

void CrasAudioHandler::RequestStyleTransferSupported(
    OnStyleTransferSupportedCallback callback) {
  CrasAudioClient::Get()->GetStyleTransferSupported(
      base::BindOnce(&CrasAudioHandler::HandleGetStyleTransferSupported,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrasAudioHandler::HandleGetStyleTransferSupported(
    OnStyleTransferSupportedCallback callback,
    std::optional<bool> system_style_transfer_supported) {
  if (!system_style_transfer_supported.has_value()) {
    LOG(ERROR)
        << "cras_audio_handler: Failed to retrieve style transfer support";
  } else {
    style_transfer_supported_ = system_style_transfer_supported.value();
  }

  std::move(callback).Run();
}

void CrasAudioHandler::SetStyleTransferSupportedForTesting(bool supported) {
  style_transfer_supported_ = supported;
}

bool CrasAudioHandler::GetForceRespectUiGainsState() const {
  return audio_pref_handler_->GetForceRespectUiGainsState();
}

void CrasAudioHandler::RefreshForceRespectUiGainsState() {
  // Refresh should only update the state in CRAS and leave the preference
  // as-is.
  CrasAudioClient::Get()->SetForceRespectUiGains(GetForceRespectUiGainsState());
}

void CrasAudioHandler::SetForceRespectUiGainsState(bool state) {
  base::UmaHistogramBoolean(CrasAudioHandler::kForceRespectUiGainsHistogramName,
                            state);
  CrasAudioClient::Get()->SetForceRespectUiGains(state);
  audio_pref_handler_->SetForceRespectUiGainsState(state);

  for (auto& observer : observers_) {
    observer.OnForceRespectUiGainsStateChanged();
  }
}

bool CrasAudioHandler::IsHfpMicSrSupportedForDevice(uint64_t device_id) {
  if (!hfp_mic_sr_supported()) {
    return false;
  }

  const AudioDevice* device = GetDeviceFromId(device_id);
  if (!device || device->type != AudioDeviceType::kBluetoothNbMic) {
    return false;
  }

  return device->audio_effect & cras::EFFECT_TYPE_HFP_MIC_SR;
}

void CrasAudioHandler::RequestHfpMicSrSupported(
    OnHfpMicSrSupportedCallback callback) {
  CrasAudioClient::Get()->GetHfpMicSrSupported(
      base::BindOnce(&CrasAudioHandler::HandleGetHfpMicSrSupported,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrasAudioHandler::SetHfpMicSrSupportedForTesting(bool supported) {
  hfp_mic_sr_supported_ = supported;
}

void CrasAudioHandler::HandleGetHfpMicSrSupported(
    OnHfpMicSrSupportedCallback callback,
    std::optional<bool> hfp_mic_sr_supported) {
  if (!hfp_mic_sr_supported.has_value()) {
    LOG(ERROR) << "cras_audio_handler: Failed to retrieve hfp_mic_sr support";
  } else {
    hfp_mic_sr_supported_ = hfp_mic_sr_supported.value();
  }

  std::move(callback).Run();
}

bool CrasAudioHandler::GetHfpMicSrState() const {
  return audio_pref_handler_->GetHfpMicSrState();
}

void CrasAudioHandler::RefreshHfpMicSrState() {
  if (!hfp_mic_sr_supported()) {
    return;
  }

  const AudioDevice* device = GetDeviceByType(AudioDeviceType::kBluetoothNbMic);
  if (!device) {
    return;
  }

  // Refresh should only update the state in CRAS and leave the preference
  // as-is.
  CrasAudioClient::Get()->SetHfpMicSrEnabled(
      GetHfpMicSrState() &&
      (device->audio_effect & cras::EFFECT_TYPE_HFP_MIC_SR));
}

void CrasAudioHandler::SetHfpMicSrState(bool hfp_mic_sr_on,
                                        AudioSettingsChangeSource source) {
  CrasAudioClient::Get()->SetHfpMicSrEnabled(hfp_mic_sr_on);
  audio_pref_handler_->SetHfpMicSrState(hfp_mic_sr_on);

  for (auto& observer : observers_) {
    observer.OnHfpMicSrStateChanged();
  }
}

void CrasAudioHandler::SetKeyboardMicActive(bool active) {
  const AudioDevice* keyboard_mic = GetKeyboardMic();
  if (!keyboard_mic) {
    return;
  }
  // Keyboard mic is invisible to chromeos users. It is always added or removed
  // as additional active node.
  DCHECK(active_input_node_id_ && active_input_node_id_ != keyboard_mic->id);
  if (active) {
    AddActiveNode(keyboard_mic->id, true);
  } else {
    RemoveActiveNodeInternal(keyboard_mic->id, true);
  }
}

void CrasAudioHandler::SetSpeakOnMuteDetection(bool som_on) {
  CrasAudioClient::Get()->SetSpeakOnMuteDetection(som_on);
  speak_on_mute_detection_on_ = som_on;
}

void CrasAudioHandler::SetEwmaPowerReportEnabled(bool enabled) {
  CrasAudioClient::Get()->SetEwmaPowerReportEnabled(enabled);
  ewma_power_report_enabled_ = enabled;
}

double CrasAudioHandler::GetEwmaPower() {
  return ewma_power_;
}

void CrasAudioHandler::SetSidetoneEnabled(bool enabled) {
  CrasAudioClient::Get()->SetSidetoneEnabled(enabled);
  sidetone_enabled_ = enabled;
}

bool CrasAudioHandler::GetSidetoneEnabled() const {
  return sidetone_enabled_;
}

bool CrasAudioHandler::IsSidetoneSupported() const {
  return sidetone_supported_;
}

void CrasAudioHandler::UpdateSidetoneSupportedState() {
  CrasAudioClient::Get()->GetSidetoneSupported(
      base::BindOnce(&CrasAudioHandler::HandleGetSidetoneSupported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrasAudioHandler::HandleGetSidetoneSupported(
    std::optional<bool> supported) {
  if (!supported.has_value()) {
    LOG(ERROR) << "Failed to check whether sidetone is supported.";
    return;
  }

  sidetone_supported_ = *supported;
}

void CrasAudioHandler::AddActiveNode(uint64_t node_id, bool notify) {
  const AudioDevice* device = GetDeviceFromId(node_id);
  if (!device) {
    VLOG(1) << "AddActiveInputNode: Cannot find device id=" << "0x" << std::hex
            << node_id;
    return;
  }

  // If there is no primary active device, set |node_id| to primary active node.
  if ((device->is_input && !active_input_node_id_) ||
      (!device->is_input && !active_output_node_id_)) {
    SwitchToDevice(*device, notify, DeviceActivateType::kActivateByUser);
    return;
  }

  AddAdditionalActiveNode(node_id, notify);
}

void CrasAudioHandler::ChangeActiveNodes(const NodeIdList& new_active_ids) {
  AudioDeviceList input_devices;
  AudioDeviceList output_devices;

  for (uint64_t id : new_active_ids) {
    const AudioDevice* device = GetDeviceFromId(id);
    if (!device) {
      continue;
    }
    if (device->is_input) {
      input_devices.push_back(*device);
    } else {
      output_devices.push_back(*device);
    }
  }
  if (!input_devices.empty()) {
    SetActiveDevices(input_devices, true /* is_input */);
  }
  if (!output_devices.empty()) {
    SetActiveDevices(output_devices, false /* is_input */);
  }
}

bool CrasAudioHandler::SetActiveInputNodes(const NodeIdList& node_ids) {
  return SetActiveNodes(node_ids, true /* is_input */);
}

bool CrasAudioHandler::SetActiveOutputNodes(const NodeIdList& node_ids) {
  return SetActiveNodes(node_ids, false /* is_input */);
}

bool CrasAudioHandler::SetActiveNodes(const NodeIdList& node_ids,
                                      bool is_input) {
  AudioDeviceList devices;
  for (uint64_t id : node_ids) {
    const AudioDevice* device = GetDeviceFromId(id);
    if (!device || device->is_input != is_input) {
      return false;
    }

    devices.push_back(*device);
  }

  SetActiveDevices(devices, is_input);
  return true;
}

void CrasAudioHandler::SetActiveDevices(const AudioDeviceList& devices,
                                        bool is_input) {
  std::set<uint64_t> new_active_ids;
  for (const auto& active_device : devices) {
    CHECK_EQ(is_input, active_device.is_input);
    new_active_ids.insert(active_device.id);
  }

  bool active_devices_changed = false;

  // If  primary active node has to be switched, do that before adding or
  // removing any other active devices. Switching to a device can change
  // activity state of other devices - final device activity may end up being
  // unintended if it is set before active device switches.
  uint64_t primary_active =
      is_input ? active_input_node_id_ : active_output_node_id_;
  if (!new_active_ids.count(primary_active) && !devices.empty()) {
    active_devices_changed = true;
    SwitchToDevice(devices[0], false /* notify */,
                   DeviceActivateType::kActivateByUser);
  }

  AudioDeviceList existing_devices;
  GetAudioDevices(&existing_devices);
  for (const auto& existing_device : existing_devices) {
    if (existing_device.is_input != is_input) {
      continue;
    }

    bool should_be_active = new_active_ids.count(existing_device.id);
    if (existing_device.active == should_be_active) {
      continue;
    }
    active_devices_changed = true;

    if (should_be_active) {
      AddActiveNode(existing_device.id, false /* notify */);
    } else {
      RemoveActiveNodeInternal(existing_device.id, false /* notify */);
    }
  }

  if (active_devices_changed) {
    NotifyActiveNodeChanged(is_input);
  }
}

void CrasAudioHandler::SetHotwordModel(uint64_t node_id,
                                       const std::string& hotword_model,
                                       VoidCrasAudioHandlerCallback callback) {
  CrasAudioClient::Get()->SetHotwordModel(node_id, hotword_model,
                                          std::move(callback));
}

void CrasAudioHandler::SwapInternalSpeakerLeftRightChannel(bool swap) {
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (!device.is_input && device.type == AudioDeviceType::kInternalSpeaker) {
      CrasAudioClient::Get()->SwapLeftRight(device.id, swap);
      break;
    }
  }
}

void CrasAudioHandler::SetDisplayRotation(cras::DisplayRotation rotation) {
  display_rotation_ = rotation;
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (device.type == AudioDeviceType::kInternalSpeaker) {
      CrasAudioClient::Get()->SetDisplayRotation(device.id, display_rotation_);
      break;
    }
  }
}

void CrasAudioHandler::SetOutputMonoEnabled(bool enabled) {
  if (output_mono_enabled_ == enabled) {
    return;
  }
  output_mono_enabled_ = enabled;
  if (enabled) {
    CrasAudioClient::Get()->SetGlobalOutputChannelRemix(
        output_channels_,
        std::vector<double>(kStereoToMono, std::end(kStereoToMono)));
  } else {
    CrasAudioClient::Get()->SetGlobalOutputChannelRemix(
        output_channels_,
        std::vector<double>(kStereoToStereo, std::end(kStereoToStereo)));
  }

  for (auto& observer : observers_) {
    observer.OnOutputChannelRemixingChanged(enabled);
  }
}

bool CrasAudioHandler::has_alternative_input() const {
  return has_alternative_input_;
}

bool CrasAudioHandler::has_alternative_output() const {
  return has_alternative_output_;
}

void CrasAudioHandler::SetOutputVolumePercent(int volume_percent) {
  // Set all active devices to the same volume.
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (!device.is_input && device.active) {
      SetOutputNodeVolumePercent(device.id, volume_percent);
    }
  }
}

// TODO: Rename the 'Percent' to something more meaningful.
void CrasAudioHandler::SetInputGainPercent(int gain_percent) {
  // TODO(jennyz): Should we set all input devices' gain to the same level?
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (device.is_input && device.active) {
      SetInputNodeGainPercent(active_input_node_id_, gain_percent);
    }
  }
}

void CrasAudioHandler::AdjustOutputVolumeByPercent(int adjust_by_percent) {
  SetOutputVolumePercent(output_volume_ + adjust_by_percent);
}

void CrasAudioHandler::IncreaseOutputVolumeByOneStep(int one_step_percent) {
  // Set all active devices to the same volume.
  int new_output_volume = 0;
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (!device.is_input && device.active) {
      // only USB device should depend on number_of_volume_steps
      if (device.type == AudioDeviceType::kUsb) {
        int32_t number_of_volume_steps = device.number_of_volume_steps;
        if (number_of_volume_steps == 0) {
          LOG(ERROR)
              << device.ToString()
              << ": No valid number_of_volume_steps. Falling back to default.";
          number_of_volume_steps = NUMBER_OF_VOLUME_STEPS_DEFAULT;
        }
        int32_t volume_level = std::round(
            (double)output_volume_ * (double)number_of_volume_steps * 0.01);
        if (volume_level == 0 && output_volume_ > 0) {
          volume_level = 1;
        }
        // increase one level and convert to volume
        new_output_volume = std::min(
            100, static_cast<int>(std::floor(((double)(volume_level + 1)) /
                                             number_of_volume_steps * 100)));
      } else {
        new_output_volume = std::min(100, output_volume_ + one_step_percent);
      }
      SetOutputNodeVolumePercent(device.id, new_output_volume);
    }
  }
}

void CrasAudioHandler::DecreaseOutputVolumeByOneStep(int one_step_percent) {
  // Set all active devices to the same volume.
  int new_output_volume = 0;
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (!device.is_input && device.active) {
      if (device.type == AudioDeviceType::kUsb) {
        int32_t number_of_volume_steps = device.number_of_volume_steps;
        if (number_of_volume_steps == 0) {
          LOG(ERROR)
              << device.ToString()
              << ": No valid number_of_volume_steps. Falling back to default.";
          number_of_volume_steps = NUMBER_OF_VOLUME_STEPS_DEFAULT;
        }
        int32_t volume_level = std::round(
            (double)output_volume_ * (double)number_of_volume_steps * 0.01);

        // decrease one level and convert to volume
        new_output_volume = std::max(
            0, static_cast<int>(std::floor(((double)(volume_level - 1)) /
                                           number_of_volume_steps * 100)));
      } else {
        new_output_volume = std::max(0, output_volume_ - one_step_percent);
      }
      SetOutputNodeVolumePercent(device.id, new_output_volume);
    }
  }
}

void CrasAudioHandler::SetOutputMute(bool mute_on) {
  if (!SetOutputMuteInternal(mute_on)) {
    return;
  }

  // Save the mute state for all active output audio devices.
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (!device.is_input && device.active) {
      audio_pref_handler_->SetMuteValue(device, output_mute_on_);
    }
  }

  for (auto& observer : observers_) {
    observer.OnOutputMuteChanged(output_mute_on_);
  }
}

void CrasAudioHandler::SetOutputMute(
    bool mute_on,
    CrasAudioHandler::AudioSettingsChangeSource source) {
  SetOutputMute(mute_on);
  base::UmaHistogramEnumeration(
      CrasAudioHandler::kOutputVolumeMuteSourceHistogramName, source);
}

void CrasAudioHandler::SetOutputMuteLockedBySecurityCurtain(bool mute_on) {
  if (output_mute_forced_by_security_curtain_ == mute_on) {
    return;
  }

  output_mute_forced_by_security_curtain_ = mute_on;
  UpdateAudioOutputMute();
}

void CrasAudioHandler::AdjustOutputVolumeToAudibleLevel() {
  if (output_volume_ <= kMuteThresholdPercent) {
    for (const auto& item : audio_devices_) {
      int unmute_volume = kDefaultUnmuteVolumePercent;
      const AudioDevice& device = item.second;
      if (!device.is_input && device.active) {
        if (device.type == AudioDeviceType::kUsb) {
          int32_t number_of_volume_steps = device.number_of_volume_steps;
          DCHECK(number_of_volume_steps > 0);
          unmute_volume = 100 / number_of_volume_steps;
        }
        SetOutputNodeVolumePercent(device.id, unmute_volume);
      }
    }
  }
}

void CrasAudioHandler::SetInputMute(bool mute_on,
                                    InputMuteChangeMethod method) {
  const bool old_mute_on = input_mute_on_;
  SetInputMuteInternal(mute_on);

  if (old_mute_on != input_mute_on_) {
    for (auto& observer : observers_) {
      observer.OnInputMuteChanged(input_mute_on_, method);
    }
  }
}

void CrasAudioHandler::SetInputMute(
    bool mute_on,
    InputMuteChangeMethod method,
    CrasAudioHandler::AudioSettingsChangeSource source) {
  SetInputMute(mute_on, method);
  base::UmaHistogramEnumeration(
      CrasAudioHandler::kInputGainMuteSourceHistogramName, source);
}

void CrasAudioHandler::SetInputMuteLockedBySecurityCurtain(bool mute_on) {
  if (input_mute_forced_by_security_curtain_ == mute_on) {
    return;
  }

  input_mute_forced_by_security_curtain_ = mute_on;
  SetInputMute(mute_on, InputMuteChangeMethod::kOther);

  for (auto& observer : observers_) {
    observer.OnInputMutedBySecurityCurtainChanged(mute_on);
  }
}

void CrasAudioHandler::SetActiveDevice(const AudioDevice& active_device,
                                       bool notify,
                                       DeviceActivateType activate_by) {
  if (activate_by == DeviceActivateType::kActivateByUser) {
    audio_device_metrics_handler_.RecordUserSwitchAudioDevice(
        active_device.is_input);
  } else {
    audio_device_metrics_handler_.MaybeRecordSystemSwitchDecisionAndContext(
        active_device.is_input,
        active_device.is_input ? has_alternative_input_
                               : has_alternative_output_,
        /*is_switched=*/true, audio_devices_, previous_audio_devices_);
  }

  // Update *_selected_by_user_.
  // Including to unset it when selected by priority or by camera.
  if (active_device.is_input) {
    audio_device_metrics_handler_.set_input_device_selected_by_user(
        activate_by == DeviceActivateType::kActivateByUser);
  } else {
    audio_device_metrics_handler_.set_output_device_selected_by_user(
        activate_by == DeviceActivateType::kActivateByUser);
  }

  if (active_device.is_input) {
    CrasAudioClient::Get()->SetActiveInputNode(active_device.id);
  } else {
    CrasAudioClient::Get()->SetActiveOutputNode(active_device.id);
  }

  if (notify) {
    NotifyActiveNodeChanged(active_device.is_input);
  }

  // Active device has changed, update user preference and add the device to
  // most recently activated device list.
  if (features::IsAudioSelectionImprovementEnabled()) {
    SyncDevicePrefSetMap(active_device.is_input);
    audio_pref_handler_->UpdateMostRecentActivatedDeviceIdList(active_device);

    if (activate_by == DeviceActivateType::kActivateByUser) {
      // Remove notification if the hotplugged device that triggered the
      // notification has already been activated by user via settings or quick
      // settings.
      audio_selection_notification_handler_
          .RemoveNotificationIfHotpluggedDeviceActivated({active_device});
    }
  }

  // Save active state for the nodes.
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (device.is_input != active_device.is_input) {
      continue;
    }
    SaveDeviceState(device, device.active, activate_by);
  }
}

void CrasAudioHandler::SaveDeviceState(const AudioDevice& device,
                                       bool active,
                                       DeviceActivateType activate_by) {
  // Don't save the active state for non-simple usage device, which is invisible
  // to end users.
  if (!device.is_for_simple_usage()) {
    return;
  }

  if (!active) {
    audio_pref_handler_->SetDeviceActive(device, false, false);
  } else {
    switch (activate_by) {
      case DeviceActivateType::kActivateByUser:
        audio_pref_handler_->SetDeviceActive(device, true, true);
        break;
      case DeviceActivateType::kActivateByPriority:
        audio_pref_handler_->SetDeviceActive(device, true, false);
        break;
      default:
        // The device is made active due to its previous active state in prefs,
        // don't change its active state settings in prefs.
        break;
    }
  }
}

void CrasAudioHandler::SetVolumeGainPercentForDevice(uint64_t device_id,
                                                     int value) {
  const AudioDevice* device = GetDeviceFromId(device_id);
  if (!device) {
    return;
  }

  if (device->is_input) {
    SetInputNodeGainPercent(device_id, value);
  } else {
    SetOutputNodeVolumePercent(device_id, value);
  }
}

void CrasAudioHandler::SetMuteForDevice(uint64_t device_id, bool mute_on) {
  if (device_id == active_output_node_id_) {
    SetOutputMute(mute_on);
    return;
  }
  if (device_id == active_input_node_id_) {
    VLOG(1) << "SetMuteForDevice sets active input device id=" << "0x"
            << std::hex << device_id << " mute=" << mute_on;
    SetInputMute(mute_on, InputMuteChangeMethod::kOther);
    return;
  }

  const AudioDevice* device = GetDeviceFromId(device_id);
  // Input device's mute state is not recorded in the pref. crbug.com/365050.
  if (device && !device->is_input) {
    audio_pref_handler_->SetMuteValue(*device, mute_on);
  }
}

void CrasAudioHandler::SetMuteForDevice(
    uint64_t device_id,
    bool mute_on,
    CrasAudioHandler::AudioSettingsChangeSource source) {
  SetMuteForDevice(device_id, mute_on);
  if (device_id == active_output_node_id_) {
    base::UmaHistogramEnumeration(
        CrasAudioHandler::kOutputVolumeMuteSourceHistogramName, source);
  } else if (device_id == active_input_node_id_) {
    base::UmaHistogramEnumeration(
        CrasAudioHandler::kInputGainMuteSourceHistogramName, source);
  }
}

// If the HDMI device is the active output device, when the device enters/exits
// docking mode, or HDMI display changes resolution, or chromeos device
// suspends/resumes, cras will lose the HDMI output node for a short period of
// time, then rediscover it. This hotplug behavior will cause the audio output
// be leaked to the alternatvie active audio output during HDMI re-discovering
// period. See crbug.com/503667.
void CrasAudioHandler::SetActiveHDMIOutoutRediscoveringIfNecessary(
    bool force_rediscovering) {
  if (!GetDeviceFromId(active_output_node_id_)) {
    return;
  }

  // Marks the start of the HDMI re-discovering grace period, during which we
  // will mute the audio output to prevent it to be be leaked to the
  // alternative output device.
  if ((hdmi_rediscovering_ && force_rediscovering) ||
      (!hdmi_rediscovering_ && IsHDMIPrimaryOutputDevice())) {
    StartHDMIRediscoverGracePeriod();
  }
}

CrasAudioHandler::CrasAudioHandler(
    mojo::PendingRemote<media_session::mojom::MediaControllerManager>
        media_controller_manager,
    scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler,
    std::unique_ptr<Delegate> delegate)
    : media_controller_manager_(std::move(media_controller_manager)),
      audio_pref_handler_(audio_pref_handler),
      delegate_(std::move(delegate)) {
  SetupCrasAudioHandler(audio_pref_handler);
}

CrasAudioHandler::CrasAudioHandler(
    mojo::PendingRemote<media_session::mojom::MediaControllerManager>
        media_controller_manager,
    scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler)
    : media_controller_manager_(std::move(media_controller_manager)),
      audio_pref_handler_(audio_pref_handler) {
  SetupCrasAudioHandler(audio_pref_handler);
}

void CrasAudioHandler::SetupCrasAudioHandler(
    scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler) {
  DCHECK(audio_pref_handler);
  DCHECK(CrasAudioClient::Get());
  CrasAudioClient::Get()->AddObserver(this);
  audio_pref_handler_->AddAudioPrefObserver(this);
  ui::MicrophoneMuteSwitchMonitor::Get()->AddObserver(this);

  BindMediaControllerObserver();
  InitializeAudioState();
  // Unittest may not have the task runner for the current thread.
  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    main_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  }

  DCHECK(!g_cras_audio_handler);
  g_cras_audio_handler = this;
}

CrasAudioHandler::~CrasAudioHandler() {
  hdmi_rediscover_timer_.Stop();
  DCHECK(CrasAudioClient::Get());
  CrasAudioClient::Get()->RemoveObserver(this);
  audio_pref_handler_->RemoveAudioPrefObserver(this);
  ui::MicrophoneMuteSwitchMonitor::Get()->RemoveObserver(this);

  DCHECK(g_cras_audio_handler);
  g_cras_audio_handler = nullptr;
}

void CrasAudioHandler::BindMediaControllerObserver() {
  if (!media_controller_manager_) {
    return;
  }
  media_controller_manager_->CreateActiveMediaController(
      media_session_controller_remote_.BindNewPipeAndPassReceiver());
  media_session_controller_remote_->AddObserver(
      media_controller_observer_receiver_.BindNewPipeAndPassRemote());
}

void CrasAudioHandler::AudioClientRestarted() {
  InitializeAudioState();
}

void CrasAudioHandler::NodesChanged() {
  if (cras_service_available_) {
    GetNodes();
  }
}

void CrasAudioHandler::OutputNodeVolumeChanged(uint64_t node_id, int volume) {
  const AudioDevice* device = this->GetDeviceFromId(node_id);
  if (!device || device->is_input) {
    LOG(ERROR) << "Unexpected OutputNodeVolumeChanged received on node: 0x"
               << std::hex << node_id;
    return;
  }
  if (volume < 0 || volume > 100) {
    LOG(ERROR) << "Unexpected OutputNodeVolumeChanged received on volume: "
               << volume;
    return;
  }

  // Sync internal volume state and notify UI for the change. We trust cras
  // signal to report the volume state of the device, no matter which source
  // set the volume, i.e., volume could be set from non-chrome source, like
  // Bluetooth headset, etc. Assume all active output devices share a single
  // volume.
  if (device->active) {
    output_volume_ = volume;
  }
  audio_pref_handler_->SetVolumeGainValue(*device, volume);

  if (initializing_audio_state_) {
    // Do not notify the observers for volume changed event if CrasAudioHandler
    // is initializing its state, i.e., the volume change event is in responding
    // to SetOutputNodeVolume request from intializaing audio state, not
    // from user action, no need to notify UI to pop uo the volume slider bar.
    if (init_node_id_ == node_id && init_volume_ == volume) {
      --init_volume_count_;
      if (!init_volume_count_) {
        initializing_audio_state_ = false;
      }
      return;
    } else {
      // Reset the initializing_audio_state_ in case SetOutputNodeVolume request
      // is lost by cras due to cras is not ready when CrasAudioHandler is being
      // initialized.
      initializing_audio_state_ = false;
      init_volume_count_ = 0;
    }
  }

  for (auto& observer : observers_) {
    observer.OnOutputNodeVolumeChanged(node_id, volume);
  }
}

void CrasAudioHandler::InputNodeGainChanged(uint64_t node_id, int gain) {
  const AudioDevice* device = this->GetDeviceFromId(node_id);

  if (!device || !device->is_input) {
    LOG(ERROR) << "Unexpexted InputNodeGainChanged received on node: 0x"
               << std::hex << node_id;
    return;
  }

  if (device->active) {
    input_gain_ = gain;
  }

  audio_pref_handler_->SetVolumeGainValue(*device, gain);

  for (auto& observer : observers_) {
    observer.OnInputNodeGainChanged(node_id, gain);
  }
}

void CrasAudioHandler::ActiveOutputNodeChanged(uint64_t node_id) {
  if (active_output_node_id_ == node_id) {
    return;
  }

  // Active audio output device should always be changed by chrome.
  // During system boot, cras may change active input to unknown device 0x1,
  // we don't need to log it, since it is not an valid device.
  if (GetDeviceFromId(node_id)) {
    LOG(WARNING) << "Active output node changed unexpectedly by system node_id="
                 << "0x" << std::hex << node_id;
  }
}

void CrasAudioHandler::ActiveInputNodeChanged(uint64_t node_id) {
  if (active_input_node_id_ == node_id) {
    return;
  }

  // Active audio input device should always be changed by chrome.
  // During system boot, cras may change active input to unknown device 0x2,
  // we don't need to log it, since it is not an valid device.
  if (GetDeviceFromId(node_id)) {
    LOG(WARNING) << "Active input node changed unexpectedly by system node_id="
                 << "0x" << std::hex << node_id;
  }
}

void CrasAudioHandler::HotwordTriggered(uint64_t tv_sec, uint64_t tv_nsec) {
  for (auto& observer : observers_) {
    observer.OnHotwordTriggered(tv_sec, tv_nsec);
  }
}

void CrasAudioHandler::NumberOfActiveStreamsChanged() {
  GetNumberOfOutputStreams();
}

void CrasAudioHandler::BluetoothBatteryChanged(const std::string& address,
                                               uint32_t level) {
  for (auto& observer : observers_) {
    observer.OnBluetoothBatteryChanged(address, level);
  }
}

void CrasAudioHandler::NumberOfInputStreamsWithPermissionChanged(
    const base::flat_map<std::string, uint32_t>& num_input_streams) {
  HandleGetNumberOfInputStreamsWithPermission(num_input_streams);
  for (auto& observer : observers_) {
    observer.OnNumberOfInputStreamsWithPermissionChanged();
  }
}

// static
std::unique_ptr<CrasAudioHandler::AudioSurvey>
CrasAudioHandler::AbstractAudioSurvey(
    const base::flat_map<std::string, std::string>& survey_specific_data) {
  auto survey = std::make_unique<CrasAudioHandler::AudioSurvey>();
  for (const auto& it : survey_specific_data) {
    if (it.first == CrasAudioHandler::kSurveyNameKey) {
      if (it.second == CrasAudioHandler::kSurveyNameGeneral) {
        survey->set_type(SurveyType::kGeneral);
      } else if (it.second == CrasAudioHandler::kSurveyNameBluetooth) {
        survey->set_type(SurveyType::kBluetooth);
      } else if (it.second == CrasAudioHandler::kSurveyNameOutputProc) {
        survey->set_type(SurveyType::kOutputProc);
      }
    } else {
      survey->AddData(it.first, it.second);
    }
  }
  return survey;
}

void CrasAudioHandler::SurveyTriggered(
    const base::flat_map<std::string, std::string>& survey_specific_data) {
  auto survey = CrasAudioHandler::AbstractAudioSurvey(survey_specific_data);

  for (auto& observer : observers_) {
    observer.OnSurveyTriggered(*survey);
  }
}

void CrasAudioHandler::SpeakOnMuteDetected() {
  for (auto& observer : observers_) {
    observer.OnSpeakOnMuteDetected();
  }
}

void CrasAudioHandler::EwmaPowerReported(double power) {
  ewma_power_ = power;
}

void CrasAudioHandler::NumStreamIgnoreUiGains(int32_t num) {
  num_stream_ignore_ui_gains_ = num;
  for (auto& observer : observers_) {
    observer.OnNumStreamIgnoreUiGainsChanged(num);
  }
}

void CrasAudioHandler::SidetoneSupportedChanged(bool supported) {
  sidetone_supported_ = supported;
}

void CrasAudioHandler::ResendBluetoothBattery() {
  CrasAudioClient::Get()->ResendBluetoothBattery();
}

void CrasAudioHandler::SetPrefHandlerForTesting(
    scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler) {
  audio_pref_handler_->RemoveAudioPrefObserver(this);
  audio_pref_handler_ = audio_pref_handler;
  audio_pref_handler_->AddAudioPrefObserver(this);
}

void CrasAudioHandler::OnAudioPolicyPrefChanged() {
  ApplyAudioPolicy();
}

const AudioDevice* CrasAudioHandler::GetDeviceFromId(uint64_t device_id) const {
  AudioDeviceMap::const_iterator it = audio_devices_.find(device_id);
  if (it == audio_devices_.end()) {
    return nullptr;
  }
  return &it->second;
}

AudioDevice CrasAudioHandler::ConvertAudioNodeWithModifiedPriority(
    const AudioNode& node) {
  AudioDevice device(node);
  device.user_priority = audio_pref_handler_->GetUserPriority(device);
  return device;
}

const AudioDevice* CrasAudioHandler::GetDeviceFromStableDeviceId(
    bool is_input,
    uint64_t stable_device_id) const {
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (device.is_input == is_input &&
        device.stable_device_id == stable_device_id) {
      return &device;
    }
  }
  return nullptr;
}

const AudioDevice* CrasAudioHandler::GetKeyboardMic() const {
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (device.is_input && device.type == AudioDeviceType::kKeyboardMic) {
      return &device;
    }
  }
  return nullptr;
}

const AudioDevice* CrasAudioHandler::GetHotwordDevice() const {
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (device.is_input && device.type == AudioDeviceType::kHotword) {
      return &device;
    }
  }
  return nullptr;
}

void CrasAudioHandler::SetupAudioInputState() {
  // Set the initial audio state to the ones read from audio prefs.
  const AudioDevice* device = GetDeviceFromId(active_input_node_id_);
  if (!device) {
    LOG(ERROR) << "Can't set up audio state for unknown input device id ="
               << "0x" << std::hex << active_input_node_id_;
    return;
  }
  input_gain_ = audio_pref_handler_->GetInputGainValue(device);
  VLOG(1) << "SetupAudioInputState for active device id=" << "0x" << std::hex
          << device->id << " mute=" << input_mute_on_;
  SetInputMuteInternal(input_mute_on_);

  SetInputNodeGain(active_input_node_id_, input_gain_);
}

void CrasAudioHandler::SetupAudioOutputState() {
  const AudioDevice* device = GetDeviceFromId(active_output_node_id_);
  if (!device) {
    LOG(ERROR) << "Can't set up audio state for unknown output device id ="
               << "0x" << std::hex << active_output_node_id_;
    return;
  }
  DCHECK(!device->is_input);
  // Mute the output during HDMI re-discovering grace period.
  if (hdmi_rediscovering_ && !IsHDMIPrimaryOutputDevice()) {
    VLOG(1) << "Mute the output during HDMI re-discovering grace period";
    SetOutputMuteInternal(true);
  } else {
    SetOutputMuteInternal(audio_pref_handler_->GetMuteValue(*device));
  }
  output_volume_ = audio_pref_handler_->GetOutputVolumeValue(device);

  if (initializing_audio_state_) {
    // During power up, InitializeAudioState() could be called twice, first
    // by CrasAudioHandler constructor, then by cras server restarting signal,
    // both sending SetOutputNodeVolume requests, and could lead to two
    // OutputNodeVolumeChanged signals.
    ++init_volume_count_;
    init_node_id_ = active_output_node_id_;
    init_volume_ = output_volume_;
  }
  SetOutputNodeVolume(active_output_node_id_, output_volume_);
}

// This sets up the state of an additional active node.
void CrasAudioHandler::SetupAdditionalActiveAudioNodeState(uint64_t node_id) {
  const AudioDevice* device = GetDeviceFromId(node_id);
  if (!device) {
    VLOG(1) << "Can't set up audio state for unknown device id =" << "0x"
            << std::hex << node_id;
    return;
  }

  DCHECK(node_id != active_output_node_id_ && node_id != active_input_node_id_);

  // Note: The mute state is a system wide state, we don't set mute per device,
  // but just keep the mute state consistent for the active node in prefs.
  // The output volume should be set to the same value for all active output
  // devices. For input devices, we don't restore their gain value so far.
  // TODO(jennyz): crbug.com/417418, track the status for the decison if
  // we should persist input gain value in prefs.
  if (!device->is_input) {
    audio_pref_handler_->SetMuteValue(*device, IsOutputMuted());
    SetOutputNodeVolumePercent(node_id, GetOutputVolumePercent());
  }
}

void CrasAudioHandler::InitializeAudioState() {
  initializing_audio_state_ = true;
  ApplyAudioPolicy();

  // Defer querying cras for GetNodes until cras service becomes available.
  cras_service_available_ = false;
  CrasAudioClient::Get()->WaitForServiceToBeAvailable(base::BindOnce(
      &CrasAudioHandler::InitializeAudioAfterCrasServiceAvailable,
      weak_ptr_factory_.GetWeakPtr()));
}

void CrasAudioHandler::InitializeAudioAfterCrasServiceAvailable(
    bool service_is_available) {
  if (!service_is_available) {
    LOG(ERROR) << "Cras service is not available";
    cras_service_available_ = false;
    return;
  }

  cras_service_available_ = true;
  GetDefaultOutputBufferSizeInternal();
  GetSystemAecSupported();
  GetSystemAecGroupId();
  GetSystemNsSupported();
  GetSystemAgcSupported();
  RequestNoiseCancellationSupported(base::BindOnce(
      &CrasAudioHandler::GetNodes, weak_ptr_factory_.GetWeakPtr()));
  RequestStyleTransferSupported(base::BindOnce(&CrasAudioHandler::GetNodes,
                                               weak_ptr_factory_.GetWeakPtr()));
  RequestHfpMicSrSupported(base::BindOnce(&CrasAudioHandler::GetNodes,
                                          weak_ptr_factory_.GetWeakPtr()));
  GetNumberOfOutputStreams();
  GetNumberOfNonChromeOutputStreams();
  GetNumberOfInputStreamsWithPermissionInternal();
  GetNumStreamIgnoreUiGains();
  GetNumberOfArcStreams();
  CrasAudioClient::Get()->SetFixA2dpPacketSize(
      base::FeatureList::IsEnabled(features::kBluetoothFixA2dpPacketSize));

  // Sets Floss enabled based on feature flag.
  CrasAudioClient::Get()->SetFlossEnabled(floss::features::IsFlossEnabled());

  input_muted_by_microphone_mute_switch_ = IsMicrophoneMuteSwitchOn();
  if (input_muted_by_microphone_mute_switch_) {
    SetInputMute(true, InputMuteChangeMethod::kPhysicalShutter);
  }

  // Sets speak-on-mute detection enabled based on local variable, it re-applies
  // the previous state if CRAS restarts.
  CrasAudioClient::Get()->SetSpeakOnMuteDetection(speak_on_mute_detection_on_);

  // Sets force respect ui gains enabled based on audio pref, it re-applies the
  // previous state if CRAS restarts.
  CrasAudioClient::Get()->SetForceRespectUiGains(GetForceRespectUiGainsState());

  UpdateSidetoneSupportedState();
  CrasAudioClient::Get()->SetSidetoneEnabled(sidetone_enabled_);
  CrasAudioClient::Get()->SetEwmaPowerReportEnabled(ewma_power_report_enabled_);
}

void CrasAudioHandler::ApplyAudioPolicy() {
  bool mute_on = !audio_pref_handler_->GetAudioOutputAllowedValue();

  if (output_mute_forced_by_policy_ == mute_on) {
    return;
  }

  output_mute_forced_by_policy_ = mute_on;
  UpdateAudioOutputMute();
  // Policy for audio input is handled by kAudioCaptureAllowed in the Chrome
  // media system.
}

void CrasAudioHandler::UpdateAudioOutputMute() {
  if (output_mute_forced_by_policy_ ||
      output_mute_forced_by_security_curtain_) {
    // Mute the device, but do not update the preference.
    SetOutputMuteInternal(true);
  } else {
    // Restore the mute state.
    const AudioDevice* device = GetDeviceFromId(active_output_node_id_);
    if (device) {
      SetOutputMuteInternal(audio_pref_handler_->GetMuteValue(*device));
    }
  }
}

void CrasAudioHandler::SetOutputNodeVolume(uint64_t node_id, int volume) {
  CrasAudioClient::Get()->SetOutputNodeVolume(node_id, volume);
}

void CrasAudioHandler::SetOutputNodeVolumePercent(uint64_t node_id,
                                                  int volume_percent) {
  const AudioDevice* device = GetDeviceFromId(node_id);
  if (!device || device->is_input) {
    return;
  }

  volume_percent = min(max(volume_percent, 0), 100);
  if (volume_percent <= kMuteThresholdPercent) {
    volume_percent = 0;
  }

  // Save the volume setting in pref in case this is called on non-active
  // node for configuration.
  audio_pref_handler_->SetVolumeGainValue(*device, volume_percent);

  if (device->active) {
    SetOutputNodeVolume(node_id, volume_percent);
  }
}

bool CrasAudioHandler::SetOutputMuteInternal(bool mute_on) {
  bool is_output_mute_forced = (output_mute_forced_by_policy_ ||
                                output_mute_forced_by_security_curtain_);

  if (is_output_mute_forced && !mute_on) {
    // Do not allow unmuting if the policy forces the device to remain muted.
    return false;
  }

  output_mute_on_ = mute_on;
  CrasAudioClient::Get()->SetOutputUserMute(mute_on);
  return true;
}

void CrasAudioHandler::SetInputNodeGain(uint64_t node_id, int gain) {
  CrasAudioClient::Get()->SetInputNodeGain(node_id, gain);
}

void CrasAudioHandler::SetInputNodeGainPercent(uint64_t node_id,
                                               int gain_percent) {
  const AudioDevice* device = GetDeviceFromId(node_id);
  if (!device || !device->is_input) {
    return;
  }

  // NOTE: We do not sanitize input gain values since the range is completely
  // dependent on the device.

  audio_pref_handler_->SetVolumeGainValue(*device, gain_percent);

  if (device->active) {
    SetInputNodeGain(node_id, gain_percent);
  }
}

void CrasAudioHandler::SetInputMuteInternal(bool mute_on) {
  // Do not allow unmuting the device if hardware microphone mute switch is on.
  // The switch disables internal microphone, and cras audio handler is expected
  // to keep system wide cras mute on while the switch is toggled (which should
  // ensure non-internal audio input devices are kept muted).
  //
  // Also do not allow unmuting the device if the security curtain is showing,
  // to prevent a remote admin from spying on the user
  if (!mute_on && (input_muted_by_microphone_mute_switch_ ||
                   input_mute_forced_by_security_curtain_)) {
    return;
  }

  input_mute_on_ = mute_on;
  CrasAudioClient::Get()->SetInputMute(mute_on);
}

void CrasAudioHandler::GetNodes() {
  CrasAudioClient::Get()->GetNodes(base::BindOnce(
      &CrasAudioHandler::HandleGetNodes, weak_ptr_factory_.GetWeakPtr()));
}

void CrasAudioHandler::GetNumberOfNonChromeOutputStreams() {
  CrasAudioClient::Get()->GetNumberOfNonChromeOutputStreams(
      base::BindOnce(&CrasAudioHandler::HandleGetNumberOfNonChromeOutputStreams,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrasAudioHandler::GetNumberOfOutputStreams() {
  CrasAudioClient::Get()->GetNumberOfActiveOutputStreams(
      base::BindOnce(&CrasAudioHandler::HandleGetNumActiveOutputStreams,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool CrasAudioHandler::ChangeActiveDevice(
    const AudioDevice& new_active_device) {
  uint64_t& current_active_node_id = new_active_device.is_input
                                         ? active_input_node_id_
                                         : active_output_node_id_;
  // If the device we want to switch to is already the current active device,
  // do nothing.
  if (new_active_device.active &&
      new_active_device.id == current_active_node_id) {
    return false;
  }

  bool found_new_active_device = false;
  // Reset all other input or output devices' active status. The active audio
  // device from the previous user session can be remembered by cras, but not
  // in chrome. see crbug.com/273271.
  for (auto& item : audio_devices_) {
    AudioDevice& device = item.second;
    if (device.is_input == new_active_device.is_input &&
        device.id != new_active_device.id) {
      device.active = false;
    } else if (device.is_input == new_active_device.is_input &&
               device.id == new_active_device.id) {
      found_new_active_device = true;
    }
  }

  if (!found_new_active_device) {
    LOG(ERROR) << "Invalid new active device: " << new_active_device.ToString();
    return false;
  }

  // Update user priority whenever the audio device is activated.
  const AudioDevice* current_active_device =
      GetDeviceFromId(current_active_node_id);
  audio_pref_handler_->SetUserPriorityHigherThan(new_active_device,
                                                 current_active_device);

  // Set the current active input/output device to the new_active_device.
  current_active_node_id = new_active_device.id;
  audio_devices_[current_active_node_id].active = true;
  return true;
}

void CrasAudioHandler::SwitchToDevice(const AudioDevice& device,
                                      bool notify,
                                      DeviceActivateType activate_by) {
  if (!ChangeActiveDevice(device)) {
    // Record the decision of system not switching active device.
    if (activate_by != DeviceActivateType::kActivateByUser) {
      audio_device_metrics_handler_.MaybeRecordSystemSwitchDecisionAndContext(
          device.is_input,
          device.is_input ? has_alternative_input_ : has_alternative_output_,
          /*is_switched=*/false, audio_devices_, previous_audio_devices_);
    }
    return;
  }

  if (device.is_input) {
    SetupAudioInputState();
  } else {
    SetupAudioOutputState();
  }

  SetActiveDevice(device, notify, activate_by);

  // content::MediaStreamManager listens to
  // base::SystemMonitor::DevicesChangedObserver for audio devices,
  // and updates EnumerateDevices when OnDevicesChanged is called.
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  // In some unittest, |monitor| might be nullptr.
  if (!monitor) {
    return;
  }
  monitor->ProcessDevicesChanged(
      base::SystemMonitor::DeviceType::DEVTYPE_AUDIO);
}

bool CrasAudioHandler::HasDeviceChange(const AudioNodeList& new_nodes,
                                       bool is_input,
                                       AudioDeviceList* new_discovered,
                                       AudioDeviceList* removed_devices,
                                       bool* active_device_removed) {
  removed_devices->clear();
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (is_input != device.is_input) {
      continue;
    }
    if (!IsDeviceInList(device, new_nodes)) {
      removed_devices->push_back(device);
      if ((is_input && device.id == active_input_node_id_) ||
          (!is_input && device.id == active_output_node_id_)) {
        *active_device_removed = true;
      }
    }
  }

  bool new_or_changed_device = false;
  new_discovered->clear();

  for (const AudioNode& node : new_nodes) {
    if (is_input != node.is_input) {
      continue;
    }
    // Check if the new device is not in the old device list.
    AudioDevice device = ConvertAudioNodeWithModifiedPriority(node);
    DeviceStatus status = CheckDeviceStatus(device);
    if (status == NEW_DEVICE) {
      // When audio selection improvement flag is off, always put the new device
      // into the new discovered device list.
      // When audio selection improvement flag is on, only put simple usage
      // device into new discovered device list.
      if (!features::IsAudioSelectionImprovementEnabled() ||
          device.is_for_simple_usage()) {
        new_discovered->push_back(device);
      }
    }
    if (status == NEW_DEVICE || status == CHANGED_DEVICE) {
      new_or_changed_device = true;
    }
  }
  return new_or_changed_device || !removed_devices->empty();
}

CrasAudioHandler::DeviceStatus CrasAudioHandler::CheckDeviceStatus(
    const AudioDevice& device) {
  const AudioDevice* device_found =
      GetDeviceFromStableDeviceId(device.is_input, device.stable_device_id);
  if (!device_found) {
    return NEW_DEVICE;
  }

  if (!IsSameAudioDevice(device, *device_found)) {
    LOG(ERROR) << "Different Audio devices with same stable device id:"
               << " new device: " << device.ToString()
               << " old device: " << device_found->ToString();
    return CHANGED_DEVICE;
  }
  if (device.active != device_found->active) {
    return CHANGED_DEVICE;
  }
  return OLD_DEVICE;
}

void CrasAudioHandler::NotifyActiveNodeChanged(bool is_input) {
  if (is_input) {
    for (auto& observer : observers_) {
      observer.OnActiveInputNodeChanged();
    }
  } else {
    for (auto& observer : observers_) {
      observer.OnActiveOutputNodeChanged();
    }
  }
}

bool CrasAudioHandler::GetActiveDeviceFromUserPref(bool is_input,
                                                   AudioDevice* active_device) {
  bool found_active_device = false;
  bool last_active_device_activate_by_user = false;
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (device.is_input != is_input || !device.is_for_simple_usage()) {
      continue;
    }

    bool active = false;
    bool activate_by_user = false;
    // If the device entry is not found in prefs, it is likley a new audio
    // device plugged in after the cros is powered down. We should ignore the
    // previously saved active device, and select the active device by priority.
    // crbug.com/622045.
    if (!audio_pref_handler_->GetDeviceActive(device, &active,
                                              &activate_by_user)) {
      return false;
    }

    if (!active) {
      continue;
    }

    if (!found_active_device) {
      found_active_device = true;
      *active_device = device;
      last_active_device_activate_by_user = activate_by_user;
      continue;
    }

    // Choose the best one among multiple active devices from prefs.
    if (activate_by_user) {
      if (last_active_device_activate_by_user) {
        // If there are more than one active devices activated by user in the
        // prefs, most likely, after the device was shut down, and before it
        // is rebooted, user has plugged in some previously unplugged audio
        // devices. For such case, it does not make sense to honor the active
        // states in the prefs.
        VLOG(1) << "Found more than one user activated devices in the prefs.";
        return false;
      }
      // Device activated by user has higher priority than the one
      // is not activated by user.
      *active_device = device;
      last_active_device_activate_by_user = true;
    } else if (!last_active_device_activate_by_user) {
      // If there are more than one active devices activated by priority in the
      // prefs, most likely, cras is still enumerating the audio devices
      // progressively. For such case, it does not make sense to honor the
      // active states in the prefs.
      VLOG(1) << "Found more than one active devices by priority in the prefs.";
      return false;
    }
  }

  if (found_active_device && !active_device->is_for_simple_usage()) {
    // This is an odd case which is rare but possible to happen during cras
    // initialization depeneding the audio device enumation process. The only
    // audio node coming from cras is an internal audio device not visible
    // to user, such as AudioDeviceType::kPostMixLoopback.
    return false;
  }

  return found_active_device;
}

void CrasAudioHandler::PauseAllStreams() {
  if (media_controller_manager_) {
    media_controller_manager_->SuspendAllSessions();
  }
}

void CrasAudioHandler::HandleNonHotplugNodesChange(
    bool is_input,
    const AudioDeviceList& devices,
    const AudioDeviceList& hotplug_devices,
    bool has_device_change,
    bool has_device_removed,
    bool active_device_removed) {
  bool has_current_active_node =
      is_input ? active_input_node_id_ : active_output_node_id_;

  // No device change, extra NodesChanged signal received.
  if (!has_device_change && has_current_active_node) {
    return;
  }

  if (!hotplug_devices.empty()) {
    // Looks like a new chrome session starts.
    if (features::IsAudioSelectionImprovementEnabled()) {
      HandleSystemBoots(is_input, devices);
    } else {
      SwitchToPreviousActiveDeviceIfAvailable(is_input, devices);
    }
    return;
  }

  if (has_device_removed) {
    if (!active_device_removed && has_current_active_node) {
      // Removed a non-active device, keep the current active device.
      // Record the decision of system not switching active device.
      audio_device_metrics_handler_.MaybeRecordSystemSwitchDecisionAndContext(
          is_input, is_input ? has_alternative_input_ : has_alternative_output_,
          /*is_switched=*/false, audio_devices_, previous_audio_devices_);

      // When removing a non-active device, if the preferred device in the new
      // device set is not the currently active device, do not switch but keep
      // the current active device. Record metrics for Rule #2.
      if (features::IsAudioSelectionImprovementEnabled()) {
        const std::optional<AudioDevice> preferred_device =
            GetPreferredDeviceIfDeviceSetSeenBefore(
                is_input, GetSimpleUsageAudioDevices(audio_devices_, is_input));
        if (preferred_device.has_value()) {
          const AudioDevice* current_active_device = GetDeviceFromId(
              is_input ? active_input_node_id_ : active_output_node_id_);
          if (current_active_device &&
              current_active_device->stable_device_id !=
                  preferred_device->stable_device_id) {
            audio_device_metrics_handler_.RecordExceptionRulesMet(
                is_input
                    ? AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
                          kInputRule2UnplugNonActiveDevice
                    : AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
                          kOutputRule2UnplugNonActiveDevice);
          }
        }
      }

      return;
    }

    // Unplugged the current active device.
    if (active_device_removed) {
      // Pauses active streams when the active output device is
      // removed.
      if (!is_input) {
        PauseAllStreams();
      }

      if (!features::IsAudioSelectionImprovementEnabled()) {
        SwitchToTopPriorityDevice(devices);
        return;
      }

      CHECK(features::IsAudioSelectionImprovementEnabled());

      // If there is only one device left, activate it.
      if (devices.size() == 1) {
        SwitchToDevice(devices.front(), /*notify=*/true,
                       DeviceActivateType::kActivateByPriority);
        return;
      }

      // Check if the remaining device set was seen before, activate the
      // preferred one if so, otherwise, activate the most recently active
      // device.
      const std::optional<AudioDevice> preferred_device =
          GetPreferredDeviceIfDeviceSetSeenBefore(
              is_input, GetSimpleUsageAudioDevices(audio_devices_, is_input));
      if (preferred_device.has_value()) {
        SwitchToDevice(preferred_device.value(), /*notify=*/true,
                       DeviceActivateType::kActivateByPriority);
      } else {
        // Record metrics for Exception Rule #4 that unplugging an active device
        // and the remaining device set was not seen before.
        audio_device_metrics_handler_.RecordExceptionRulesMet(
            is_input ? AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
                           kInputRule4UnplugDeviceCausesUnseenSet
                     : AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
                           kOutputRule4UnplugDeviceCausesUnseenSet);

        if (!ActivateMostRecentActiveDevice(is_input)) {
          // Fall back to previous approach if no device in the most recently
          // active device list is currently connected.
          SwitchToTopPriorityDevice(devices);
        }
      }

      return;
    }
  }

  // Some unexpected error happens on cras side. See crbug.com/586026.
  // Either cras sent stale nodes to chrome again or cras triggered some
  // error. Restore the previously selected active.
  VLOG(1) << "Odd case from cras, the active node is lost unexpectedly.";
  SwitchToPreviousActiveDeviceIfAvailable(is_input, devices);
}

void CrasAudioHandler::HandleSystemBoots(bool is_input,
                                         const AudioDeviceList& devices) {
  // If there is only one device, activate it.
  if (devices.size() == 1) {
    SwitchToDevice(devices.front(), /*notify=*/true,
                   DeviceActivateType::kActivateByPriority);
    return;
  }

  // If the set of devices was seen before, activate the preferred one.
  const std::optional<AudioDevice> preferred_device =
      GetPreferredDeviceIfDeviceSetSeenBefore(
          is_input, GetSimpleUsageAudioDevices(audio_devices_, is_input));
  if (preferred_device.has_value()) {
    // Activate this device.
    SwitchToDevice(preferred_device.value(), /*notify=*/true,
                   DeviceActivateType::kActivateByPriority);
    return;
  }

  // Otherwise when the device set was not seen before, if a 3.5mm headphone is
  // connected, activate it. Do not show notification.
  for (const AudioDevice& device : devices) {
    if (device.type == AudioDeviceType::kHeadphone ||
        device.type == AudioDeviceType::kMic) {
      SwitchToDevice(device, /*notify=*/true,
                     DeviceActivateType::kActivateByPriority);
      return;
    }
  }

  // Otherwise, activate the most recent activated device and show
  // notification.
  if (ActivateMostRecentActiveDevice(is_input)) {
    should_show_notification_ = true;
    return;
  }

  // Otherwise when no most recent activated device, if there is an internal
  // device, activate it and show notification.
  for (const AudioDevice& device : devices) {
    if (device.type == AudioDeviceType::kInternalSpeaker ||
        device.type == AudioDeviceType::kInternalMic) {
      SwitchToDevice(device, /*notify=*/true,
                     DeviceActivateType::kActivateByPriority);
      should_show_notification_ = true;
      return;
    }
  }

  // Otherwise when no most recent activated device and no internal device (a
  // brand new chromebox), activate the highest priority and show
  // notification.
  // TODO(zhangwenyu): Add metrics for this case to understand how frequent it
  // happens.
  SwitchToTopPriorityDevice(devices);
  should_show_notification_ = true;
}

bool CrasAudioHandler::ActivateMostRecentActiveDevice(bool is_input) {
  const base::Value::List& ids =
      audio_pref_handler_->GetMostRecentActivatedDeviceIdList(is_input);
  for (int i = ids.size() - 1; i >= 0; i--) {
    std::optional<uint64_t> device_stable_id =
        ParseDeviceId(ids[i].GetString());
    if (!device_stable_id.has_value()) {
      continue;
    }
    const AudioDevice* device =
        GetDeviceFromStableDeviceId(is_input, *device_stable_id);
    if (!device) {
      continue;
    }
    SwitchToDevice(*device, /*notify=*/true,
                   DeviceActivateType::kActivateByPriority);
    return true;
  }
  return false;
}

bool CrasAudioHandler::ShouldSwitchToHotPlugDevice(
    const AudioDevice& hotplug_device) const {
  // Whenever 35mm headphone or mic is hot plugged, always pick it as the active
  // device.
  if (hotplug_device.type == AudioDeviceType::kHeadphone ||
      hotplug_device.type == AudioDeviceType::kMic) {
    return true;
  }

  const uint64_t active_node_id =
      hotplug_device.is_input ? active_input_node_id_ : active_output_node_id_;
  const AudioDevice* current_active_device = GetDeviceFromId(active_node_id);

  if (!current_active_device) {
    return true;
  }

  // Use built-in priority if hotplug_device or current_active_device has
  // kUserPriorityNone. Otherwise the newly plugged devices will never be
  // selected due to having user_priority = 0.
  // current_active_device can has kUserPriorityNone, if it is a new device
  // and it is activated when there is no active device (ex: the first active
  // device after boot).
  if ((hotplug_device.user_priority == kUserPriorityNone ||
       current_active_device->user_priority == kUserPriorityNone)) {
    return LessBuiltInPriority(*current_active_device, hotplug_device);
  }

  return LessUserPriority(*current_active_device, hotplug_device);
}

void CrasAudioHandler::HandleHotPlugDeviceByUserPriority(
    const AudioDevice& hotplug_device) {
  // This most likely may happen during the transition period of cras
  // initialization phase, in which a non-simple-usage node may appear like
  // a hotplug node.
  if (!hotplug_device.is_for_simple_usage()) {
    return;
  }

  if (features::IsAudioSelectionImprovementEnabled()) {
    HandleHotPlugDeviceWithNotification(hotplug_device);
    return;
  }

  if (ShouldSwitchToHotPlugDevice(hotplug_device)) {
    SwitchToDevice(hotplug_device, true,
                   DeviceActivateType::kActivateByPriority);
    return;
  } else {
    // Record the decision of system not switching active device.
    audio_device_metrics_handler_.MaybeRecordSystemSwitchDecisionAndContext(
        hotplug_device.is_input,
        hotplug_device.is_input ? has_alternative_input_
                                : has_alternative_output_,
        /*is_switched=*/false, audio_devices_, previous_audio_devices_);
  }

  // Do not active the hotplug device. The hotplug device is not the top
  // priority device.
  VLOG(1) << "Hotplug device remains inactive as its previous state:"
          << hotplug_device.ToString();
}

void CrasAudioHandler::SwitchToTopPriorityDevice(
    const AudioDeviceList& devices) {
  if (devices.empty()) {
    return;
  }

  // When the audio selection improvement flag is on, no user priority will be
  // maintained. Use built-in priority rather than user priority.
  AudioDevice top_device = features::IsAudioSelectionImprovementEnabled()
                               ? base::ranges::max(devices, LessBuiltInPriority)
                               : base::ranges::max(devices, LessUserPriority);
  if (!top_device.is_for_simple_usage()) {
    return;
  }

  // For the dual camera and dual microphone case, choose microphone
  // that is consistent to the active camera.
  if (IsFrontOrRearMic(top_device) && HasDualInternalMic() && IsCameraOn()) {
    ActivateInternalMicForActiveCamera();
    return;
  }

  SwitchToDevice(top_device, true, DeviceActivateType::kActivateByPriority);
}

void CrasAudioHandler::SwitchToPreviousActiveDeviceIfAvailable(
    bool is_input,
    const AudioDeviceList& devices) {
  // With new audio selection mechanism, activate the most recently active
  // device first.
  if (features::IsAudioSelectionImprovementEnabled() &&
      ActivateMostRecentActiveDevice(is_input)) {
    return;
  }

  AudioDevice previous_active_device;
  if (GetActiveDeviceFromUserPref(is_input, &previous_active_device)) {
    DCHECK(previous_active_device.is_for_simple_usage());
    // Switch to previous active device stored in user prefs.
    SwitchToDevice(previous_active_device, true,
                   DeviceActivateType::kActivateByRestorePreviousState);
  } else {
    // No previous active device, switch to the top priority device.
    SwitchToTopPriorityDevice(devices);
  }
}

void CrasAudioHandler::HandleRemoveNotification(bool is_input) {
  audio_selection_notification_handler_
      .RemoveNotificationIfHotpluggedDeviceDisconnected(
          is_input, GetSimpleUsageAudioDevices(audio_devices_, is_input));
}

void CrasAudioHandler::UpdateDevicesAndSwitchActive(
    const AudioNodeList& nodes) {
  AudioDeviceList output_devices;
  AudioDeviceList input_devices;
  AudioDeviceList hotplug_output_devices;
  AudioDeviceList hotplug_input_devices;
  AudioDeviceList removed_output_devices;
  AudioDeviceList removed_input_devices;
  bool active_output_removed = false;
  bool active_input_removed = false;
  bool output_devices_changed =
      HasDeviceChange(nodes, false, &hotplug_output_devices,
                      &removed_output_devices, &active_output_removed);
  bool input_devices_changed =
      HasDeviceChange(nodes, true, &hotplug_input_devices,
                      &removed_input_devices, &active_input_removed);

  if (!removed_input_devices.empty()) {
    // Add grace period in case the USB device is not firmly plugged, making
    // it disconnected and reconnected in a short period of time. In this case,
    // the notification should not be removed.
    remove_notification_timer_for_input_.Start(
        FROM_HERE, kRemoveNotificationDelay,
        base::BindOnce(&CrasAudioHandler::HandleRemoveNotification,
                       weak_ptr_factory_.GetWeakPtr(), /*is_input=*/true));
  }

  if (!removed_output_devices.empty()) {
    remove_notification_timer_for_output_.Start(
        FROM_HERE, kRemoveNotificationDelay,
        base::BindOnce(&CrasAudioHandler::HandleRemoveNotification,
                       weak_ptr_factory_.GetWeakPtr(), /*is_input=*/false));
  }

  // Record consecutive devices change metrics.
  if (input_devices_changed) {
    audio_device_metrics_handler_.RecordConsecutiveAudioDevicsChangeTimeElapsed(
        /*is_input=*/true, /*is_device_added=*/!hotplug_input_devices.empty());
  }
  if (output_devices_changed) {
    audio_device_metrics_handler_.RecordConsecutiveAudioDevicsChangeTimeElapsed(
        /*is_input=*/false,
        /*is_device_added=*/!hotplug_output_devices.empty());
  }

  std::vector<AudioDevice> devices;
  devices.reserve(nodes.size());
  for (AudioNode node : nodes) {
    devices.push_back(ConvertAudioNodeWithModifiedPriority(node));
  }

  // Updates the display_rotation to the internal speaker when it's added.
  for (AudioDevice device : devices) {
    DeviceStatus status = CheckDeviceStatus(device);
    if (status == NEW_DEVICE &&
        device.type == AudioDeviceType::kInternalSpeaker) {
      CrasAudioClient::Get()->SetDisplayRotation(device.id, display_rotation_);
    }
  }

  // Remove the least recently seen devices if there are too many devices.
  audio_pref_handler_->DropLeastRecentlySeenDevices(devices,
                                                    kMaxDeviceStoredInPref);

  // Before audio_devices_ is cleared, saves all audio device types to
  // previous_audio_devices_.
  previous_audio_devices_ = audio_devices_;

  audio_devices_.clear();
  has_alternative_input_ = false;
  has_alternative_output_ = false;

  // Local variables to help determine if there are more than one audio devices
  // currently connected. Note that kInternalMic, kFrontMic and kRearMic are
  // considered one audio device since they are all displayed as internal mic in
  // UI and user can not switch between them.
  bool has_internal_input_device = false;
  bool has_external_input_device = false;
  bool has_internal_output_device = false;
  bool has_external_output_device = false;

  for (AudioDevice device : devices) {
    // When audio selection improvement flag is on, ignore non simple usage
    // devices because users can't see/select them.
    if (features::IsAudioSelectionImprovementEnabled() &&
        !device.is_for_simple_usage()) {
      continue;
    }

    audio_devices_[device.id] = device;

    if (device.is_input) {
      input_devices.push_back(device);

      // If has_alternative_input_ has already been determined as true, no need
      // to calculate it again.
      if (has_alternative_input_) {
        continue;
      }

      bool has_two_external_input_devices =
          has_external_input_device && device.IsExternalDevice();
      bool has_one_internal_and_one_external_input_device =
          has_internal_input_device && device.IsExternalDevice();
      bool has_one_external_and_one_internal_input_device =
          has_external_input_device && device.IsInternalMic();
      if (has_two_external_input_devices ||
          has_one_internal_and_one_external_input_device ||
          has_one_external_and_one_internal_input_device) {
        has_alternative_input_ = true;
      }

      if (device.IsInternalMic()) {
        has_internal_input_device = true;
      } else if (device.IsExternalDevice()) {
        has_external_input_device = true;
      }

    } else {
      output_devices.push_back(device);

      // If has_alternative_output_ has already been determined as true, no need
      // to calculate it again.
      if (has_alternative_output_) {
        continue;
      }

      bool has_two_external_output_devices =
          has_external_output_device && device.IsExternalDevice();
      bool has_one_internal_and_one_external_output_device =
          has_internal_output_device && device.IsExternalDevice();
      bool has_one_external_and_one_internal_output_device =
          has_external_output_device && device.IsInternalSpeaker();
      if (has_two_external_output_devices ||
          has_one_internal_and_one_external_output_device ||
          has_one_external_and_one_internal_output_device) {
        has_alternative_output_ = true;
      }

      if (device.IsInternalSpeaker()) {
        has_internal_output_device = true;
      } else if (device.IsExternalDevice()) {
        has_external_output_device = true;
      }
    }
  }

  // Handle output device changes.
  HandleAudioDeviceChange(
      false, output_devices, hotplug_output_devices, output_devices_changed,
      !removed_output_devices.empty(), active_output_removed);

  // Handle input device changes.
  HandleAudioDeviceChange(true, input_devices, hotplug_input_devices,
                          input_devices_changed, !removed_input_devices.empty(),
                          active_input_removed);

  // At this moment, system has already made the switching or not switching
  // decision, set this flag to false.
  audio_device_metrics_handler_.set_is_chrome_restarts(false);

  if (features::IsAudioSelectionImprovementEnabled() &&
      should_show_notification_) {
    const AudioDevice* active_input_device =
        GetDeviceFromId(active_input_node_id_);
    const AudioDevice* active_output_device =
        GetDeviceFromId(active_output_node_id_);
    audio_selection_notification_handler_.ShowAudioSelectionNotification(
        hotplug_input_devices, hotplug_output_devices,
        active_input_device
            ? std::make_optional(active_input_device->display_name)
            : std::nullopt,
        active_output_device
            ? std::make_optional(active_output_device->display_name)
            : std::nullopt,
        base::BindRepeating(&CrasAudioHandler::SwitchToDevice,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindRepeating(&CrasAudioHandler::OpenSettingsAudioPage,
                            weak_ptr_factory_.GetWeakPtr()));

    // Reset show notification flag.
    should_show_notification_ = false;
  }

  // content::MediaStreamManager listens to
  // base::SystemMonitor::DevicesChangedObserver for audio devices,
  // and updates EnumerateDevices when OnDevicesChanged is called.
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  // In some unittest, |monitor| might be nullptr.
  if (!monitor) {
    return;
  }
  monitor->ProcessDevicesChanged(
      base::SystemMonitor::DeviceType::DEVTYPE_AUDIO);
}

void CrasAudioHandler::HandleAudioDeviceChange(
    bool is_input,
    const AudioDeviceList& devices,
    const AudioDeviceList& hotplug_devices,
    bool has_device_change,
    bool has_device_removed,
    bool active_device_removed) {
  uint64_t& active_node_id =
      is_input ? active_input_node_id_ : active_output_node_id_;

  if (has_device_change) {
    // Mark device selected by the system, including when the algorithm
    // does nothing ultimately. We still treat not switching the device
    // as a decision of the algorithm.
    if (is_input) {
      audio_device_metrics_handler_.set_input_device_selected_by_user(false);
    } else {
      audio_device_metrics_handler_.set_output_device_selected_by_user(false);
    }
  }

  // No audio devices found.
  if (devices.empty()) {
    VLOG(1) << "No " << (is_input ? "input" : "output") << " devices found";
    active_node_id = 0;
    NotifyActiveNodeChanged(is_input);
    return;
  }

  // If the previous active device is removed from the new node list,
  // or changed to inactive by cras, reset active_node_id.
  // See crbug.com/478968.
  const AudioDevice* active_device = GetDeviceFromId(active_node_id);
  if (!active_device || !active_device->active) {
    active_node_id = 0;
  }

  if (!active_node_id || hotplug_devices.empty() ||
      hotplug_devices.size() > 1) {
    HandleNonHotplugNodesChange(is_input, devices, hotplug_devices,
                                has_device_change, has_device_removed,
                                active_device_removed);
    if (features::IsAudioSelectionImprovementEnabled()) {
      SyncDevicePrefSetMap(is_input);
    }
  } else {
    // Typical user hotplug case.
    HandleHotPlugDeviceByUserPriority(hotplug_devices.front());
  }
}

void CrasAudioHandler::HandleGetNodes(std::optional<AudioNodeList> node_list) {
  if (!node_list.has_value()) {
    LOG(ERROR) << "Failed to retrieve audio nodes data";
    return;
  }

  if (node_list->empty()) {
    return;
  }

  UpdateDevicesAndSwitchActive(node_list.value());

  // Always set the input noise cancellation state on NodesChange event.
  RefreshNoiseCancellationState();

  // Always set the input style transfer state on NodesChange event.
  RefreshStyleTransferState();

  // Always set the hfp_mic_sr state on NodesChange event.
  RefreshHfpMicSrState();

  for (auto& observer : observers_) {
    observer.OnAudioNodesChanged();
  }
}

void CrasAudioHandler::HandleGetNumberOfNonChromeOutputStreams(
    std::optional<int32_t> new_output_streams_count) {
  if (!new_output_streams_count.has_value()) {
    LOG(ERROR) << "Failed to retrieve number of active output streams.";
    return;
  }
  DCHECK_GE(*new_output_streams_count, 0);

  if (*new_output_streams_count > 0 &&
      num_active_nonchrome_output_streams_ == 0) {
    for (auto& observer : observers_) {
      observer.OnNonChromeOutputStarted();
    }
  } else if (*new_output_streams_count == 0 &&
             num_active_nonchrome_output_streams_ > 0) {
    for (auto& observer : observers_) {
      observer.OnNonChromeOutputStopped();
    }
  }
  num_active_nonchrome_output_streams_ = *new_output_streams_count;
}

void CrasAudioHandler::HandleGetNumActiveOutputStreams(
    std::optional<int> new_output_streams_count) {
  if (!new_output_streams_count.has_value()) {
    LOG(ERROR) << "Failed to retrieve number of active output streams";
    return;
  }

  DCHECK_GE(*new_output_streams_count, 0);
  if (*new_output_streams_count > 0 && num_active_output_streams_ == 0) {
    for (auto& observer : observers_) {
      observer.OnOutputStarted();
    }
  } else if (*new_output_streams_count == 0 && num_active_output_streams_ > 0) {
    for (auto& observer : observers_) {
      observer.OnOutputStopped();
    }
  }
  num_active_output_streams_ = *new_output_streams_count;
}

void CrasAudioHandler::AddAdditionalActiveNode(uint64_t node_id, bool notify) {
  const AudioDevice* device = GetDeviceFromId(node_id);
  if (!device) {
    VLOG(1) << "AddActiveInputNode: Cannot find device id=" << "0x" << std::hex
            << node_id;
    return;
  }

  audio_devices_[node_id].active = true;
  SetupAdditionalActiveAudioNodeState(node_id);

  if (device->is_input) {
    DCHECK(node_id != active_input_node_id_);
    CrasAudioClient::Get()->AddActiveInputNode(node_id);
    if (notify) {
      NotifyActiveNodeChanged(true);
    }
  } else {
    DCHECK(node_id != active_output_node_id_);
    CrasAudioClient::Get()->AddActiveOutputNode(node_id);
    if (notify) {
      NotifyActiveNodeChanged(false);
    }
  }
}

void CrasAudioHandler::RemoveActiveNodeInternal(uint64_t node_id, bool notify) {
  const AudioDevice* device = GetDeviceFromId(node_id);
  if (!device) {
    VLOG(1) << "RemoveActiveInputNode: Cannot find device id=" << "0x"
            << std::hex << node_id;
    return;
  }

  audio_devices_[node_id].active = false;
  if (device->is_input) {
    if (node_id == active_input_node_id_) {
      active_input_node_id_ = 0;
    }
    CrasAudioClient::Get()->RemoveActiveInputNode(node_id);
    if (notify) {
      NotifyActiveNodeChanged(true);
    }
  } else {
    if (node_id == active_output_node_id_) {
      active_output_node_id_ = 0;
    }
    CrasAudioClient::Get()->RemoveActiveOutputNode(node_id);
    if (notify) {
      NotifyActiveNodeChanged(false);
    }
  }
}

void CrasAudioHandler::UpdateAudioAfterHDMIRediscoverGracePeriod() {
  VLOG(1) << "HDMI output re-discover grace period ends.";
  hdmi_rediscovering_ = false;
  if (!IsOutputMutedForDevice(active_output_node_id_)) {
    // Unmute the audio output after the HDMI transition period.
    VLOG(1) << "Unmute output after HDMI rediscovering grace period.";
    SetOutputMuteInternal(false);
  }
}

bool CrasAudioHandler::IsHDMIPrimaryOutputDevice() const {
  const AudioDevice* device = GetDeviceFromId(active_output_node_id_);
  return device && device->type == AudioDeviceType::kHdmi;
}

void CrasAudioHandler::StartHDMIRediscoverGracePeriod() {
  VLOG(1) << "Start HDMI rediscovering grace period.";
  hdmi_rediscovering_ = true;
  hdmi_rediscover_timer_.Stop();
  hdmi_rediscover_timer_.Start(
      FROM_HERE,
      base::Milliseconds(hdmi_rediscover_grace_period_duration_in_ms_), this,
      &CrasAudioHandler::UpdateAudioAfterHDMIRediscoverGracePeriod);
}

void CrasAudioHandler::SetHDMIRediscoverGracePeriodForTesting(
    int duration_in_ms) {
  hdmi_rediscover_grace_period_duration_in_ms_ = duration_in_ms;
}

void CrasAudioHandler::ActivateMicForCamera(
    media::VideoFacingMode camera_facing) {
  const AudioDevice* mic = GetMicForCamera(camera_facing);
  if (!mic || mic->active) {
    return;
  }

  SwitchToDevice(*mic, true, DeviceActivateType::kActivateByCamera);
}

void CrasAudioHandler::ActivateInternalMicForActiveCamera() {
  DCHECK(IsCameraOn());
  if (HasDualInternalMic()) {
    media::VideoFacingMode facing = front_camera_on_
                                        ? media::MEDIA_VIDEO_FACING_USER
                                        : media::MEDIA_VIDEO_FACING_ENVIRONMENT;
    ActivateMicForCamera(facing);
  }
}

// For the dual microphone case, from user point of view, they only see internal
// microphone in UI. Chrome will make the best decision on which one to pick.
// If the camera is off, the front microphone should be picked as the default
// active microphone. Otherwise, it will switch to the microphone that
// matches the active camera, i.e. front microphone for front camera and
// rear microphone for rear camera.
void CrasAudioHandler::SwitchToFrontOrRearMic() {
  DCHECK(HasDualInternalMic());
  if (IsCameraOn()) {
    ActivateInternalMicForActiveCamera();
  } else {
    SwitchToDevice(*GetDeviceByType(AudioDeviceType::kFrontMic), true,
                   DeviceActivateType::kActivateByUser);
  }
}

const AudioDevice* CrasAudioHandler::GetMicForCamera(
    media::VideoFacingMode camera_facing) {
  switch (camera_facing) {
    case media::MEDIA_VIDEO_FACING_USER:
      return GetDeviceByType(AudioDeviceType::kFrontMic);
    case media::MEDIA_VIDEO_FACING_ENVIRONMENT:
      return GetDeviceByType(AudioDeviceType::kRearMic);
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return nullptr;
}

bool CrasAudioHandler::HasDualInternalMic() const {
  bool has_front_mic = false;
  bool has_rear_mic = false;
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (device.type == AudioDeviceType::kFrontMic) {
      has_front_mic = true;
    } else if (device.type == AudioDeviceType::kRearMic) {
      has_rear_mic = true;
    }
    if (has_front_mic && has_rear_mic) {
      break;
    }
  }
  return has_front_mic && has_rear_mic;
}

bool CrasAudioHandler::HasActiveInputDeviceForSimpleUsage() const {
  const AudioDevice* active_input_device =
      GetDeviceFromId(active_input_node_id_);
  if (active_input_device) {
    return active_input_device->is_for_simple_usage();
  }
  return false;
}

bool CrasAudioHandler::IsFrontOrRearMic(const AudioDevice& device) const {
  return device.is_input && (device.type == AudioDeviceType::kFrontMic ||
                             device.type == AudioDeviceType::kRearMic);
}

bool CrasAudioHandler::IsCameraOn() const {
  return front_camera_on_ || rear_camera_on_;
}

bool CrasAudioHandler::HasExternalDevice(bool is_input) const {
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (is_input == device.is_input && device.IsExternalDevice()) {
      return true;
    }
  }
  return false;
}

void CrasAudioHandler::GetNumberOfInputStreamsWithPermissionInternal() {
  CrasAudioClient::Get()->GetNumberOfInputStreamsWithPermission(base::BindOnce(
      &CrasAudioHandler::HandleGetNumberOfInputStreamsWithPermission,
      weak_ptr_factory_.GetWeakPtr()));
}

// static
CrasAudioHandler::ClientType CrasAudioHandler::ConvertClientTypeStringToEnum(
    std::string client_type_str) {
  if (client_type_str == "CRAS_CLIENT_TYPE_PLUGIN") {
    return ClientType::VM_PLUGIN;
  } else if (client_type_str == "CRAS_CLIENT_TYPE_CROSVM") {
    return ClientType::VM_TERMINA;
  } else if (client_type_str == "CRAS_CLIENT_TYPE_CHROME") {
    return ClientType::CHROME;
  } else if (client_type_str == "CRAS_CLIENT_TYPE_ARC" ||
             client_type_str == "CRAS_CLIENT_TYPE_ARCVM") {
    return ClientType::ARC;
  } else if (client_type_str == "CRAS_CLIENT_TYPE_BOREALIS") {
    return ClientType::VM_BOREALIS;
  } else if (client_type_str == "CRAS_CLIENT_TYPE_LACROS") {
    return ClientType::LACROS;
  } else {
    return ClientType::UNKNOWN;
  }
}

void CrasAudioHandler::HandleGetNumberOfInputStreamsWithPermission(
    std::optional<base::flat_map<std::string, uint32_t>> num_input_streams) {
  if (!num_input_streams.has_value()) {
    LOG(ERROR) << "Failed to retrieve number of input streams with permission";
    return;
  }
  number_of_input_streams_with_permission_.clear();
  for (const auto& it : *num_input_streams) {
    CrasAudioHandler::ClientType type = ConvertClientTypeStringToEnum(it.first);
    if (type != ClientType::UNKNOWN) {
      number_of_input_streams_with_permission_[type] = it.second;
    }
  }
}

void CrasAudioHandler::GetDefaultOutputBufferSizeInternal() {
  CrasAudioClient::Get()->GetDefaultOutputBufferSize(
      base::BindOnce(&CrasAudioHandler::HandleGetDefaultOutputBufferSize,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrasAudioHandler::HandleGetDefaultOutputBufferSize(
    std::optional<int> buffer_size) {
  if (!buffer_size.has_value()) {
    LOG(ERROR) << "Failed to retrieve output buffer size";
    return;
  }

  default_output_buffer_size_ = buffer_size.value();
}

bool CrasAudioHandler::noise_cancellation_supported() const {
  return noise_cancellation_supported_;
}

bool CrasAudioHandler::style_transfer_supported() const {
  return style_transfer_supported_;
}

bool CrasAudioHandler::hfp_mic_sr_supported() const {
  return hfp_mic_sr_supported_;
}

bool CrasAudioHandler::system_aec_supported() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return system_aec_supported_;
}

// GetSystemAecSupported() is only called in the same thread
// as the CrasAudioHandler constructor. We are safe here without
// thread check, because unittest may not have the task runner
// for the current thread.
void CrasAudioHandler::GetSystemAecSupported() {
  CrasAudioClient::Get()->GetSystemAecSupported(
      base::BindOnce(&CrasAudioHandler::HandleGetSystemAecSupported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrasAudioHandler::HandleGetSystemAecSupported(
    std::optional<bool> system_aec_supported) {
  if (!system_aec_supported.has_value()) {
    LOG(ERROR) << "Failed to retrieve system aec supported";
    return;
  }
  system_aec_supported_ = system_aec_supported.value();
}

int32_t CrasAudioHandler::system_aec_group_id() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return system_aec_group_id_;
}

// GetSystemAecGroupId() is only called in the same thread
// as the CrasAudioHandler constructor. We are safe here without
// thread check, because unittest may not have the task runner
// for the current thread.
void CrasAudioHandler::GetSystemAecGroupId() {
  CrasAudioClient::Get()->GetSystemAecGroupId(
      base::BindOnce(&CrasAudioHandler::HandleGetSystemAecGroupId,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrasAudioHandler::HandleGetSystemAecGroupId(
    std::optional<int32_t> system_aec_group_id) {
  if (!system_aec_group_id.has_value()) {
    // If the group Id is not available, set the ID to reflect that.
    system_aec_group_id_ = kSystemAecGroupIdNotAvailable;
    return;
  }
  system_aec_group_id_ = system_aec_group_id.value();
}

bool CrasAudioHandler::system_ns_supported() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return system_ns_supported_;
}

// GetSystemNsSupported() is only called in the same thread
// as the CrasAudioHandler constructor. We are safe here without
// thread check, because unittest may not have the task runner
// for the current thread.
void CrasAudioHandler::GetSystemNsSupported() {
  CrasAudioClient::Get()->GetSystemNsSupported(
      base::BindOnce(&CrasAudioHandler::HandleGetSystemNsSupported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrasAudioHandler::HandleGetSystemNsSupported(
    std::optional<bool> system_ns_supported) {
  if (!system_ns_supported.has_value()) {
    LOG(ERROR) << "Failed to retrieve system ns supported";
    return;
  }
  system_ns_supported_ = system_ns_supported.value();
}

bool CrasAudioHandler::system_agc_supported() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return system_agc_supported_;
}

int32_t CrasAudioHandler::num_stream_ignore_ui_gains() const {
  return num_stream_ignore_ui_gains_;
}

// GetSystemAgcSupported() is only called in the same thread
// as the CrasAudioHandler constructor. We are safe here without
// thread check, because unittest may not have the task runner
// for the current thread.
void CrasAudioHandler::GetSystemAgcSupported() {
  CrasAudioClient::Get()->GetSystemAgcSupported(
      base::BindOnce(&CrasAudioHandler::HandleGetSystemAgcSupported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrasAudioHandler::HandleGetSystemAgcSupported(
    std::optional<bool> system_agc_supported) {
  if (!system_agc_supported.has_value()) {
    LOG(ERROR) << "Failed to retrieve system agc supported";
    return;
  }
  system_agc_supported_ = system_agc_supported.value();
}

void CrasAudioHandler::GetNumStreamIgnoreUiGains() {
  CrasAudioClient::Get()->GetNumStreamIgnoreUiGains(
      base::BindOnce(&CrasAudioHandler::HandleGetNumStreamIgnoreUiGains,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrasAudioHandler::HandleGetNumStreamIgnoreUiGains(
    std::optional<int32_t> new_stream_ignore_ui_gains_count) {
  if (!new_stream_ignore_ui_gains_count.has_value()) {
    LOG(ERROR) << "Failed to retrieve number of ignore ui gains streams.";
    return;
  }
  DCHECK_GE(*new_stream_ignore_ui_gains_count, 0);

  if (*new_stream_ignore_ui_gains_count != num_stream_ignore_ui_gains_) {
    for (auto& observer : observers_) {
      observer.OnNumStreamIgnoreUiGainsChanged(
          *new_stream_ignore_ui_gains_count);
    }
  }

  num_stream_ignore_ui_gains_ = *new_stream_ignore_ui_gains_count;
}

void CrasAudioHandler::GetNumberOfArcStreams() {
  CrasAudioClient::Get()->GetNumberOfArcStreams(
      base::BindOnce(&CrasAudioHandler::HandleGetNumberOfArcStreams,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrasAudioHandler::HandleGetNumberOfArcStreams(
    std::optional<int32_t> new_num_arc_streams) {
  if (!new_num_arc_streams.has_value()) {
    LOG(ERROR) << "Failed to retrieve number of active ARC streams.";
    return;
  }
  DCHECK_GE(*new_num_arc_streams, 0);

  if (*new_num_arc_streams != num_arc_streams_) {
    for (auto& observer : observers_) {
      observer.OnNumberOfArcStreamsChanged(*new_num_arc_streams);
    }
  }

  num_arc_streams_ = *new_num_arc_streams;
}

const std::optional<AudioDevice>
CrasAudioHandler::GetPreferredDeviceIfDeviceSetSeenBefore(
    bool is_input,
    const AudioDeviceList& devices) const {
  std::optional<uint64_t> id =
      audio_pref_handler_->GetPreferredDeviceFromPreferenceSet(is_input,
                                                               devices);

  if (!id.has_value()) {
    return std::nullopt;
  }

  const AudioDevice* device = GetDeviceFromStableDeviceId(is_input, id.value());
  if (!device) {
    return std::nullopt;
  }

  return *device;
}

void CrasAudioHandler::SyncDevicePrefSetMap(bool is_input) {
  const uint64_t& active_node_id =
      is_input ? active_input_node_id_ : active_output_node_id_;
  const AudioDevice* active_device = GetDeviceFromId(active_node_id);
  if (!active_device) {
    VLOG(1) << "SyncDevicePrefSetMap: No active device found.";
    // Early return since no active_device is available to sync.
    return;
  }

  audio_pref_handler_->UpdateDevicePreferenceSet(
      GetSimpleUsageAudioDevices(audio_devices_, is_input), *active_device);
}

void CrasAudioHandler::HandleHotPlugDeviceWithNotification(
    const AudioDevice& hotplug_device) {
  // Whenever a device with privilege is hot plugged, always pick it as the
  // active device. Make sure has_privilege is called only when the
  // kAudioSelectionImprovement flag is on.
  CHECK(features::IsAudioSelectionImprovementEnabled());
  if (hotplug_device.has_privilege()) {
    SwitchToDevice(hotplug_device, /*notify=*/true,
                   DeviceActivateType::kActivateByPriority);

    // Record audio selection exception rule #1.
    audio_device_metrics_handler_.RecordExceptionRulesMet(
        hotplug_device.is_input
            ? AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
                  kInputRule1HotPlugPrivilegedDevice
            : AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
                  kOutputRule1HotPlugPrivilegedDevice);
    return;
  }

  // Check if current set of audio devices was seen before.
  const std::optional<AudioDevice> preferred_device =
      GetPreferredDeviceIfDeviceSetSeenBefore(
          hotplug_device.is_input,
          GetSimpleUsageAudioDevices(audio_devices_,
                                     /*is_input=*/hotplug_device.is_input));
  // If the device set was seen and the preferred device is the hotplug device,
  // activate it. This is the only case that the hot plug device will be
  // activated.
  if (preferred_device.has_value() &&
      preferred_device->stable_device_id == hotplug_device.stable_device_id) {
    SwitchToDevice(hotplug_device, /*notify=*/true,
                   DeviceActivateType::kActivateByPriority);
    return;
  }

  // Otherwise sync the device preference set map.
  SyncDevicePrefSetMap(hotplug_device.is_input);

  if (!preferred_device.has_value()) {
    // The device set was not seen. Show notification to let user make decision.
    // Early return here since below metrics are not supposed to be recorded.
    should_show_notification_ = true;
    return;
  }

  // Only record system switch decision metrics when the set of devices is seen
  // but the system decides to not switch to it. Do not record if this set of
  // device is not seen and notification is shown. Because the system
  // technically does not make any decisions but let users make the decision.
  audio_device_metrics_handler_.MaybeRecordSystemSwitchDecisionAndContext(
      /*is_input=*/hotplug_device.is_input,
      /*has_alternative_device=*/hotplug_device.is_input
          ? has_alternative_input_
          : has_alternative_output_,
      /*is_switched=*/false, audio_devices_, previous_audio_devices_);

  // If the preferred device is not the hot plug device and also not the
  // currently activated device, do not switch but keep the currently active
  // device activated, according to audio selection exception rule #3, detailed
  // in go/audio-io.
  const AudioDevice* current_active_device = GetDeviceFromId(
      hotplug_device.is_input ? active_input_node_id_ : active_output_node_id_);
  if (current_active_device && current_active_device->stable_device_id !=
                                   preferred_device->stable_device_id) {
    audio_device_metrics_handler_.RecordExceptionRulesMet(
        hotplug_device.is_input
            ? AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
                  kInputRule3HotPlugUnpreferredDevice
            : AudioDeviceMetricsHandler::AudioSelectionExceptionRules::
                  kOutputRule3HotPlugUnpreferredDevice);
  }
}

void CrasAudioHandler::OpenSettingsAudioPage() {
  if (delegate_) {
    delegate_->OpenSettingsAudioPage();
  }
}

ScopedCrasAudioHandlerForTesting::ScopedCrasAudioHandlerForTesting() {
  CHECK(!CrasAudioClient::Get())
      << "ScopedCrasAudioHandlerForTesting expects that there is no "
         "CrasAudioClient running at its constructor.";

  fake_cras_audio_client_ = std::make_unique<FakeCrasAudioClient>();

  CrasAudioHandler::InitializeForTesting();
}

ScopedCrasAudioHandlerForTesting::~ScopedCrasAudioHandlerForTesting() {
  CrasAudioHandler::Shutdown();
}

CrasAudioHandler& ScopedCrasAudioHandlerForTesting::Get() {
  return *CrasAudioHandler::Get();
}

int32_t CrasAudioHandler::NumberOfNonChromeOutputStreams() const {
  return num_active_nonchrome_output_streams_;
}

int32_t CrasAudioHandler::NumberOfChromeOutputStreams() const {
  return num_active_output_streams_;
}

int32_t CrasAudioHandler::NumberOfArcStreams() const {
  return num_arc_streams_;
}

void CrasAudioHandler::SetNumberOfArcStreamsForTesting(int32_t num) {
  num_arc_streams_ = num;
}

}  // namespace ash
