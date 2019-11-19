// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/audio/cras_audio_handler.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/system/sys_info.h"
#include "base/system/system_monitor.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/audio/audio_device.h"
#include "chromeos/audio/audio_devices_pref_handler_stub.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

using std::max;
using std::min;

namespace chromeos {

namespace {

// Default value for unmuting, as a percent in the range [0, 100].
// Used when sound is unmuted, but volume was less than kMuteThresholdPercent.
const int kDefaultUnmuteVolumePercent = 4;

// Volume value which should be considered as muted in range [0, 100].
const int kMuteThresholdPercent = 1;

// Mixer matrix, [0.5, 0.5; 0.5, 0.5]
const double kStereoToMono[] = {0.5, 0.5, 0.5, 0.5};
// Mixer matrix, [1, 0; 0, 1]
const double kStereoToStereo[] = {1, 0, 0, 1};

CrasAudioHandler* g_cras_audio_handler = nullptr;

bool IsSameAudioDevice(const AudioDevice& a, const AudioDevice& b) {
  return a.stable_device_id == b.stable_device_id && a.is_input == b.is_input &&
         a.type == b.type && a.device_name == b.device_name;
}

bool IsDeviceInList(const AudioDevice& device, const AudioNodeList& node_list) {
  for (const AudioNode& node : node_list) {
    if (device.stable_device_id == node.StableDeviceId())
      return true;
  }
  return false;
}

}  // namespace

CrasAudioHandler::AudioObserver::AudioObserver() = default;

CrasAudioHandler::AudioObserver::~AudioObserver() = default;

void CrasAudioHandler::AudioObserver::OnOutputNodeVolumeChanged(
    uint64_t /* node_id */,
    int /* volume */) {
}

void CrasAudioHandler::AudioObserver::OnInputNodeGainChanged(
    uint64_t /* node_id */,
    int /* gain */) {
}

void CrasAudioHandler::AudioObserver::OnOutputMuteChanged(bool /* mute_on */) {}

void CrasAudioHandler::AudioObserver::OnInputMuteChanged(bool /* mute_on */) {}

void CrasAudioHandler::AudioObserver::OnAudioNodesChanged() {}

void CrasAudioHandler::AudioObserver::OnActiveOutputNodeChanged() {}

void CrasAudioHandler::AudioObserver::OnActiveInputNodeChanged() {}

void CrasAudioHandler::AudioObserver::OnOutputChannelRemixingChanged(
    bool /* mono_on */) {}

void CrasAudioHandler::AudioObserver::OnHotwordTriggered(
    uint64_t /* tv_sec */,
    uint64_t /* tv_nsec */) {}

void CrasAudioHandler::AudioObserver::OnOutputStarted() {}

void CrasAudioHandler::AudioObserver::OnOutputStopped() {}

// static
void CrasAudioHandler::Initialize(
    service_manager::Connector* connector,
    scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler) {
  g_cras_audio_handler = new CrasAudioHandler(connector, audio_pref_handler);
}

// static
void CrasAudioHandler::InitializeForTesting() {
  // Make sure CrasAudioClient has been initialized.
  if (!CrasAudioClient::Get())
    CrasAudioClient::InitializeFake();
  CrasAudioHandler::Initialize(/*connector=*/nullptr,
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
  if (!HasDualInternalMic())
    return;

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
  if (camera_is_already_on)
    return;

  // If the current active input is an external device, keep it.
  const AudioDevice* active_input = GetDeviceFromId(active_input_node_id_);
  if (active_input && active_input->IsExternalDevice())
    return;

  // Activate the correct mic for the current active camera.
  ActivateMicForCamera(facing);
}

void CrasAudioHandler::OnVideoCaptureStoppedOnMainThread(
    media::VideoFacingMode facing) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  // Do nothing if the device doesn't have both front and rear microphones.
  if (!HasDualInternalMic())
    return;

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
  if (IsCameraOn())
    return;

  // If the current active input is an external device, keep it.
  const AudioDevice* active_input = GetDeviceFromId(active_input_node_id_);
  if (active_input && active_input->IsExternalDevice())
    return;

  // Switch to front mic properly.
  DeviceActivateType activated_by =
      HasExternalDevice(true) ? ACTIVATE_BY_USER : ACTIVATE_BY_PRIORITY;
  SwitchToDevice(*GetDeviceByType(AUDIO_TYPE_FRONT_MIC), true, activated_by);
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
  if (!device)
    return false;
  DCHECK(!device->is_input);
  return audio_pref_handler_->GetMuteValue(*device);
}

bool CrasAudioHandler::IsOutputVolumeBelowDefaultMuteLevel() {
  return output_volume_ <= kMuteThresholdPercent;
}

bool CrasAudioHandler::IsInputMuted() {
  return input_mute_on_;
}

bool CrasAudioHandler::IsInputMutedForDevice(uint64_t device_id) {
  const AudioDevice* device = GetDeviceFromId(device_id);
  if (!device)
    return false;
  DCHECK(device->is_input);
  // We don't record input mute state for each device in the prefs,
  // for any non-active input device, we assume mute is off.
  if (device->id == active_input_node_id_)
    return input_mute_on_;
  return false;
}

int CrasAudioHandler::GetOutputDefaultVolumeMuteThreshold() {
  return kMuteThresholdPercent;
}

int CrasAudioHandler::GetOutputVolumePercent() {
  return output_volume_;
}

int CrasAudioHandler::GetOutputVolumePercentForDevice(uint64_t device_id) {
  if (device_id == active_output_node_id_)
    return output_volume_;
  const AudioDevice* device = GetDeviceFromId(device_id);
  return static_cast<int>(audio_pref_handler_->GetOutputVolumeValue(device));
}

int CrasAudioHandler::GetInputGainPercent() {
  return input_gain_;
}

int CrasAudioHandler::GetInputGainPercentForDevice(uint64_t device_id) {
  if (device_id == active_input_node_id_)
    return input_gain_;
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

bool CrasAudioHandler::GetPrimaryActiveOutputDevice(AudioDevice* device) const {
  const AudioDevice* active_device = GetDeviceFromId(active_output_node_id_);
  if (!active_device || !device)
    return false;
  *device = *active_device;
  return true;
}

const AudioDevice* CrasAudioHandler::GetDeviceByType(AudioDeviceType type) {
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (device.type == type)
      return &device;
  }
  return nullptr;
}

void CrasAudioHandler::GetDefaultOutputBufferSize(int32_t* buffer_size) const {
  *buffer_size = default_output_buffer_size_;
}

void CrasAudioHandler::SetKeyboardMicActive(bool active) {
  const AudioDevice* keyboard_mic = GetKeyboardMic();
  if (!keyboard_mic)
    return;
  // Keyboard mic is invisible to chromeos users. It is always added or removed
  // as additional active node.
  DCHECK(active_input_node_id_ && active_input_node_id_ != keyboard_mic->id);
  if (active)
    AddActiveNode(keyboard_mic->id, true);
  else
    RemoveActiveNodeInternal(keyboard_mic->id, true);
}

void CrasAudioHandler::AddActiveNode(uint64_t node_id, bool notify) {
  const AudioDevice* device = GetDeviceFromId(node_id);
  if (!device) {
    VLOG(1) << "AddActiveInputNode: Cannot find device id="
            << "0x" << std::hex << node_id;
    return;
  }

  // If there is no primary active device, set |node_id| to primary active node.
  if ((device->is_input && !active_input_node_id_) ||
      (!device->is_input && !active_output_node_id_)) {
    SwitchToDevice(*device, notify, ACTIVATE_BY_USER);
    return;
  }

  AddAdditionalActiveNode(node_id, notify);
}

void CrasAudioHandler::ChangeActiveNodes(const NodeIdList& new_active_ids) {
  AudioDeviceList input_devices;
  AudioDeviceList output_devices;

  for (uint64_t id : new_active_ids) {
    const AudioDevice* device = GetDeviceFromId(id);
    if (!device)
      continue;
    if (device->is_input)
      input_devices.push_back(*device);
    else
      output_devices.push_back(*device);
  }
  if (!input_devices.empty())
    SetActiveDevices(input_devices, true /* is_input */);
  if (!output_devices.empty())
    SetActiveDevices(output_devices, false /* is_input */);
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
    if (!device || device->is_input != is_input)
      return false;

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
    SwitchToDevice(devices[0], false /* notify */, ACTIVATE_BY_USER);
  }

  AudioDeviceList existing_devices;
  GetAudioDevices(&existing_devices);
  for (const auto& existing_device : existing_devices) {
    if (existing_device.is_input != is_input)
      continue;

    bool should_be_active = new_active_ids.count(existing_device.id);
    if (existing_device.active == should_be_active)
      continue;
    active_devices_changed = true;

    if (should_be_active)
      AddActiveNode(existing_device.id, false /* notify */);
    else
      RemoveActiveNodeInternal(existing_device.id, false /* notify */);
  }

  if (active_devices_changed)
    NotifyActiveNodeChanged(is_input);
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
    if (!device.is_input && device.type == AUDIO_TYPE_INTERNAL_SPEAKER) {
      CrasAudioClient::Get()->SwapLeftRight(device.id, swap);
      break;
    }
  }
}

void CrasAudioHandler::SetOutputMonoEnabled(bool enabled) {
  if (output_mono_enabled_ == enabled)
    return;
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

  for (auto& observer : observers_)
    observer.OnOutputChannelRemixingChanged(enabled);
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
    if (!device.is_input && device.active)
      SetOutputNodeVolumePercent(device.id, volume_percent);
  }
}

// TODO: Rename the 'Percent' to something more meaningful.
void CrasAudioHandler::SetInputGainPercent(int gain_percent) {
  // TODO(jennyz): Should we set all input devices' gain to the same level?
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (device.is_input && device.active)
      SetInputNodeGainPercent(active_input_node_id_, gain_percent);
  }
}

void CrasAudioHandler::AdjustOutputVolumeByPercent(int adjust_by_percent) {
  SetOutputVolumePercent(output_volume_ + adjust_by_percent);
}

void CrasAudioHandler::SetOutputMute(bool mute_on) {
  if (!SetOutputMuteInternal(mute_on))
    return;

  // Save the mute state for all active output audio devices.
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (!device.is_input && device.active) {
      audio_pref_handler_->SetMuteValue(device, output_mute_on_);
    }
  }

  for (auto& observer : observers_)
    observer.OnOutputMuteChanged(output_mute_on_);
}

void CrasAudioHandler::AdjustOutputVolumeToAudibleLevel() {
  if (output_volume_ <= kMuteThresholdPercent) {
    // Avoid the situation when sound has been unmuted, but the volume
    // is set to a very low value, so user still can't hear any sound.
    SetOutputVolumePercent(kDefaultUnmuteVolumePercent);
  }
}

void CrasAudioHandler::SetInputMute(bool mute_on) {
  SetInputMuteInternal(mute_on);
  for (auto& observer : observers_)
    observer.OnInputMuteChanged(input_mute_on_);
}

void CrasAudioHandler::SetActiveDevice(const AudioDevice& active_device,
                                       bool notify,
                                       DeviceActivateType activate_by) {
  if (active_device.is_input)
    CrasAudioClient::Get()->SetActiveInputNode(active_device.id);
  else
    CrasAudioClient::Get()->SetActiveOutputNode(active_device.id);

  if (notify)
    NotifyActiveNodeChanged(active_device.is_input);

  // Save active state for the nodes.
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (device.is_input != active_device.is_input)
      continue;
    SaveDeviceState(device, device.active, activate_by);
  }
}

void CrasAudioHandler::SaveDeviceState(const AudioDevice& device,
                                       bool active,
                                       DeviceActivateType activate_by) {
  // Don't save the active state for non-simple usage device, which is invisible
  // to end users.
  if (!device.is_for_simple_usage())
    return;

  if (!active) {
    audio_pref_handler_->SetDeviceActive(device, false, false);
  } else {
    switch (activate_by) {
      case ACTIVATE_BY_USER:
        audio_pref_handler_->SetDeviceActive(device, true, true);
        break;
      case ACTIVATE_BY_PRIORITY:
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
  if (!device)
    return;

  if (device->is_input)
    SetInputNodeGainPercent(device_id, value);
  else
    SetOutputNodeVolumePercent(device_id, value);
}

void CrasAudioHandler::SetMuteForDevice(uint64_t device_id, bool mute_on) {
  if (device_id == active_output_node_id_) {
    SetOutputMute(mute_on);
    return;
  }
  if (device_id == active_input_node_id_) {
    VLOG(1) << "SetMuteForDevice sets active input device id="
            << "0x" << std::hex << device_id << " mute=" << mute_on;
    SetInputMute(mute_on);
    return;
  }

  const AudioDevice* device = GetDeviceFromId(device_id);
  // Input device's mute state is not recorded in the pref. crbug.com/365050.
  if (device && !device->is_input)
    audio_pref_handler_->SetMuteValue(*device, mute_on);
}

// If the HDMI device is the active output device, when the device enters/exits
// docking mode, or HDMI display changes resolution, or chromeos device
// suspends/resumes, cras will lose the HDMI output node for a short period of
// time, then rediscover it. This hotplug behavior will cause the audio output
// be leaked to the alternatvie active audio output during HDMI re-discovering
// period. See crbug.com/503667.
void CrasAudioHandler::SetActiveHDMIOutoutRediscoveringIfNecessary(
    bool force_rediscovering) {
  if (!GetDeviceFromId(active_output_node_id_))
    return;

  // Marks the start of the HDMI re-discovering grace period, during which we
  // will mute the audio output to prevent it to be be leaked to the
  // alternative output device.
  if ((hdmi_rediscovering_ && force_rediscovering) ||
      (!hdmi_rediscovering_ && IsHDMIPrimaryOutputDevice())) {
    StartHDMIRediscoverGracePeriod();
  }
}

CrasAudioHandler::CrasAudioHandler(
    service_manager::Connector* connector,
    scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler)
    : connector_(connector), audio_pref_handler_(audio_pref_handler) {
  DCHECK(audio_pref_handler);
  DCHECK(CrasAudioClient::Get());
  CrasAudioClient::Get()->AddObserver(this);
  audio_pref_handler_->AddAudioPrefObserver(this);
  InitializeAudioState();
  // Unittest may not have the task runner for the current thread.
  if (base::ThreadTaskRunnerHandle::IsSet())
    main_task_runner_ = base::ThreadTaskRunnerHandle::Get();

  DCHECK(!g_cras_audio_handler);
  g_cras_audio_handler = this;
}

CrasAudioHandler::~CrasAudioHandler() {
  hdmi_rediscover_timer_.Stop();
  DCHECK(CrasAudioClient::Get());
  CrasAudioClient::Get()->RemoveObserver(this);
  audio_pref_handler_->RemoveAudioPrefObserver(this);

  DCHECK(g_cras_audio_handler);
  g_cras_audio_handler = nullptr;
}

void CrasAudioHandler::AudioClientRestarted() {
  InitializeAudioState();
}

void CrasAudioHandler::NodesChanged() {
  if (cras_service_available_)
    GetNodes();
}

void CrasAudioHandler::OutputNodeVolumeChanged(uint64_t node_id, int volume) {
  const AudioDevice* device = this->GetDeviceFromId(node_id);

  // If this is not an active output node, ignore this event. Because when this
  // node set to active, it will be applied with the volume value stored in
  // preference.
  if (!device || !device->active || device->is_input) {
    LOG(ERROR) << "Unexpexted OutputNodeVolumeChanged received on node: 0x"
               << std::hex << node_id;
    return;
  }

  // Sync internal volume state and notify UI for the change. We trust cras
  // signal to report the volume state of the device, no matter which source
  // set the volume, i.e., volume could be set from non-chrome source, like
  // Bluetooth headset, etc. Assume all active output devices share a single
  // volume.
  output_volume_ = volume;
  audio_pref_handler_->SetVolumeGainValue(*device, volume);

  if (initializing_audio_state_) {
    // Do not notify the observers for volume changed event if CrasAudioHandler
    // is initializing its state, i.e., the volume change event is in responding
    // to SetOutputNodeVolume request from intializaing audio state, not
    // from user action, no need to notify UI to pop uo the volume slider bar.
    if (init_node_id_ == node_id && init_volume_ == volume) {
      --init_volume_count_;
      if (!init_volume_count_)
        initializing_audio_state_ = false;
      return;
    } else {
      // Reset the initializing_audio_state_ in case SetOutputNodeVolume request
      // is lost by cras due to cras is not ready when CrasAudioHandler is being
      // initialized.
      initializing_audio_state_ = false;
      init_volume_count_ = 0;
    }
  }

  for (auto& observer : observers_)
    observer.OnOutputNodeVolumeChanged(node_id, volume);
}

void CrasAudioHandler::ActiveOutputNodeChanged(uint64_t node_id) {
  if (active_output_node_id_ == node_id)
    return;

  // Active audio output device should always be changed by chrome.
  // During system boot, cras may change active input to unknown device 0x1,
  // we don't need to log it, since it is not an valid device.
  if (GetDeviceFromId(node_id)) {
    LOG(WARNING) << "Active output node changed unexpectedly by system node_id="
                 << "0x" << std::hex << node_id;
  }
}

void CrasAudioHandler::ActiveInputNodeChanged(uint64_t node_id) {
  if (active_input_node_id_ == node_id)
    return;

  // Active audio input device should always be changed by chrome.
  // During system boot, cras may change active input to unknown device 0x2,
  // we don't need to log it, since it is not an valid device.
  if (GetDeviceFromId(node_id)) {
    LOG(WARNING) << "Active input node changed unexpectedly by system node_id="
                 << "0x" << std::hex << node_id;
  }
}

void CrasAudioHandler::HotwordTriggered(uint64_t tv_sec, uint64_t tv_nsec) {
  for (auto& observer : observers_)
    observer.OnHotwordTriggered(tv_sec, tv_nsec);
}

void CrasAudioHandler::NumberOfActiveStreamsChanged() {
  GetNumberOfOutputStreams();
}

void CrasAudioHandler::OnAudioPolicyPrefChanged() {
  ApplyAudioPolicy();
}

const AudioDevice* CrasAudioHandler::GetDeviceFromId(uint64_t device_id) const {
  AudioDeviceMap::const_iterator it = audio_devices_.find(device_id);
  if (it == audio_devices_.end())
    return nullptr;
  return &it->second;
}

const AudioDevice* CrasAudioHandler::GetDeviceFromStableDeviceId(
    uint64_t stable_device_id) const {
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (device.stable_device_id == stable_device_id)
      return &device;
  }
  return nullptr;
}

const AudioDevice* CrasAudioHandler::GetKeyboardMic() const {
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (device.is_input && device.type == AUDIO_TYPE_KEYBOARD_MIC)
      return &device;
  }
  return nullptr;
}

const AudioDevice* CrasAudioHandler::GetHotwordDevice() const {
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (device.is_input && device.type == AUDIO_TYPE_HOTWORD)
      return &device;
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
  VLOG(1) << "SetupAudioInputState for active device id="
          << "0x" << std::hex << device->id << " mute=" << input_mute_on_;
  SetInputMuteInternal(input_mute_on_);
  // TODO(rkc,jennyz): Set input gain once we decide on how to store
  // the gain values since the range and step are both device specific.
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
    output_mute_on_ = true;
  } else {
    output_mute_on_ = audio_pref_handler_->GetMuteValue(*device);
  }
  output_volume_ = audio_pref_handler_->GetOutputVolumeValue(device);

  SetOutputMuteInternal(output_mute_on_);

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
    VLOG(1) << "Can't set up audio state for unknown device id ="
            << "0x" << std::hex << node_id;
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
  GetNodes();
  GetNumberOfOutputStreams();
}

void CrasAudioHandler::ApplyAudioPolicy() {
  output_mute_locked_ = false;
  if (!audio_pref_handler_->GetAudioOutputAllowedValue()) {
    // Mute the device, but do not update the preference.
    SetOutputMuteInternal(true);
    output_mute_locked_ = true;
  } else {
    // Restore the mute state.
    const AudioDevice* device = GetDeviceFromId(active_output_node_id_);
    if (device)
      SetOutputMuteInternal(audio_pref_handler_->GetMuteValue(*device));
  }

  // Policy for audio input is handled by kAudioCaptureAllowed in the Chrome
  // media system.
}

void CrasAudioHandler::SetOutputNodeVolume(uint64_t node_id, int volume) {
  CrasAudioClient::Get()->SetOutputNodeVolume(node_id, volume);
}

void CrasAudioHandler::SetOutputNodeVolumePercent(uint64_t node_id,
                                                  int volume_percent) {
  const AudioDevice* device = GetDeviceFromId(node_id);
  if (!device || device->is_input)
    return;

  volume_percent = min(max(volume_percent, 0), 100);
  if (volume_percent <= kMuteThresholdPercent)
    volume_percent = 0;

  // Save the volume setting in pref in case this is called on non-active
  // node for configuration.
  audio_pref_handler_->SetVolumeGainValue(*device, volume_percent);

  if (device->active)
    SetOutputNodeVolume(node_id, volume_percent);
}

bool  CrasAudioHandler::SetOutputMuteInternal(bool mute_on) {
  if (output_mute_locked_)
    return false;

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
  if (!device || !device->is_input)
    return;

  // NOTE: We do not sanitize input gain values since the range is completely
  // dependent on the device.
  if (active_input_node_id_ == node_id)
    input_gain_ = gain_percent;

  audio_pref_handler_->SetVolumeGainValue(*device, gain_percent);

  if (device->active) {
    SetInputNodeGain(node_id, gain_percent);
    for (auto& observer : observers_)
      observer.OnInputNodeGainChanged(node_id, gain_percent);
  }
}

void CrasAudioHandler::SetInputMuteInternal(bool mute_on) {
  input_mute_on_ = mute_on;
  CrasAudioClient::Get()->SetInputMute(mute_on);
}

void CrasAudioHandler::GetNodes() {
  CrasAudioClient::Get()->GetNodes(base::BindOnce(
      &CrasAudioHandler::HandleGetNodes, weak_ptr_factory_.GetWeakPtr()));
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

  // Set the current active input/output device to the new_active_device.
  current_active_node_id = new_active_device.id;
  audio_devices_[current_active_node_id].active = true;
  return true;
}

void CrasAudioHandler::SwitchToDevice(const AudioDevice& device,
                                      bool notify,
                                      DeviceActivateType activate_by) {
  if (!ChangeActiveDevice(device))
    return;

  if (device.is_input)
    SetupAudioInputState();
  else
    SetupAudioOutputState();

  SetActiveDevice(device, notify, activate_by);

  // content::MediaStreamManager listens to
  // base::SystemMonitor::DevicesChangedObserver for audio devices,
  // and updates EnumerateDevices when OnDevicesChanged is called.
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  // In some unittest, |monitor| might be nullptr.
  if (!monitor)
    return;
  monitor->ProcessDevicesChanged(
      base::SystemMonitor::DeviceType::DEVTYPE_AUDIO);
}

bool CrasAudioHandler::HasDeviceChange(const AudioNodeList& new_nodes,
                                       bool is_input,
                                       AudioDevicePriorityQueue* new_discovered,
                                       bool* device_removed,
                                       bool* active_device_removed) {
  *device_removed = false;
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (is_input != device.is_input)
      continue;
    if (!IsDeviceInList(device, new_nodes)) {
      *device_removed = true;
      if ((is_input && device.id == active_input_node_id_) ||
          (!is_input && device.id == active_output_node_id_)) {
        *active_device_removed = true;
      }
    }
  }

  bool new_or_changed_device = false;
  while (!new_discovered->empty())
    new_discovered->pop();

  for (const AudioNode& node : new_nodes) {
    if (is_input != node.is_input)
      continue;
    // Check if the new device is not in the old device list.
    AudioDevice device(node);
    DeviceStatus status = CheckDeviceStatus(device);
    if (status == NEW_DEVICE)
      new_discovered->push(device);
    if (status == NEW_DEVICE || status == CHANGED_DEVICE) {
      new_or_changed_device = true;
    }
  }
  return new_or_changed_device || *device_removed;
}

CrasAudioHandler::DeviceStatus CrasAudioHandler::CheckDeviceStatus(
    const AudioDevice& device) {
  const AudioDevice* device_found =
      GetDeviceFromStableDeviceId(device.stable_device_id);
  if (!device_found)
    return NEW_DEVICE;

  if (!IsSameAudioDevice(device, *device_found)) {
    LOG(ERROR) << "Different Audio devices with same stable device id:"
               << " new device: " << device.ToString()
               << " old device: " << device_found->ToString();
    return CHANGED_DEVICE;
  }
  if (device.active != device_found->active)
    return CHANGED_DEVICE;
  return OLD_DEVICE;
}

void CrasAudioHandler::NotifyActiveNodeChanged(bool is_input) {
  if (is_input) {
    for (auto& observer : observers_)
      observer.OnActiveInputNodeChanged();
  } else {
    for (auto& observer : observers_)
      observer.OnActiveOutputNodeChanged();
  }
}

bool CrasAudioHandler::GetActiveDeviceFromUserPref(bool is_input,
                                                   AudioDevice* active_device) {
  bool found_active_device = false;
  bool last_active_device_activate_by_user = false;
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (device.is_input != is_input || !device.is_for_simple_usage())
      continue;

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

    if (!active)
      continue;

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
    // to user, such as AUDIO_TYPE_POST_MIX_LOOPBACK.
    return false;
  }

  return found_active_device;
}

void CrasAudioHandler::PauseAllStreams() {
  if (!connector_) {
    LOG(ERROR) << "Failed to get connector";
    return;
  }
  mojo::Remote<media_session::mojom::MediaControllerManager> controller_manager;
  connector_->Connect(media_session::mojom::kServiceName,
                      controller_manager.BindNewPipeAndPassReceiver());
  controller_manager->SuspendAllSessions();
}

void CrasAudioHandler::HandleNonHotplugNodesChange(
    bool is_input,
    const AudioDevicePriorityQueue& hotplug_nodes,
    bool has_device_change,
    bool has_device_removed,
    bool active_device_removed) {
  bool has_current_active_node =
      is_input ? active_input_node_id_ : active_output_node_id_;

  // No device change, extra NodesChanged signal received.
  if (!has_device_change && has_current_active_node)
    return;

  if (hotplug_nodes.empty()) {
    if (has_device_removed) {
      if (!active_device_removed && has_current_active_node) {
        // Removed a non-active device, keep the current active device.
        return;
      }

      if (active_device_removed) {
        // Pauses active streams when the active output device is
        // removed.
        if (!is_input)
          PauseAllStreams();

        // Unplugged the current active device.
        SwitchToTopPriorityDevice(is_input);

        return;
      }
    }

    // Some unexpected error happens on cras side. See crbug.com/586026.
    // Either cras sent stale nodes to chrome again or cras triggered some
    // error. Restore the previously selected active.
    VLOG(1) << "Odd case from cras, the active node is lost unexpectedly.";
    SwitchToPreviousActiveDeviceIfAvailable(is_input);
  } else {
    // Looks like a new chrome session starts.
    SwitchToPreviousActiveDeviceIfAvailable(is_input);
  }
}

void CrasAudioHandler::HandleHotPlugDevice(
    const AudioDevice& hotplug_device,
    const AudioDevicePriorityQueue& device_priority_queue) {
  // This most likely may happen during the transition period of cras
  // initialization phase, in which a non-simple-usage node may appear like
  // a hotplug node.
  if (!hotplug_device.is_for_simple_usage())
    return;

  // Whenever 35mm headphone or mic is hot plugged, always pick it as the active
  // device.
  if (hotplug_device.type == AUDIO_TYPE_HEADPHONE ||
      hotplug_device.type == AUDIO_TYPE_MIC) {
    SwitchToDevice(hotplug_device, true, ACTIVATE_BY_PRIORITY);
    return;
  }

  bool last_state_active = false;
  bool last_activate_by_user = false;
  if (!audio_pref_handler_->GetDeviceActive(hotplug_device, &last_state_active,
                                            &last_activate_by_user)) {
    // |hotplug_device| is plugged in for the first time, activate it if it
    // is of the highest priority.
    if (device_priority_queue.top().id == hotplug_device.id) {
      VLOG(1) << "Hotplug a device for the first time: "
              << hotplug_device.ToString();
      SwitchToDevice(hotplug_device, true, ACTIVATE_BY_PRIORITY);
    }
  } else if (last_state_active) {
    if (!last_activate_by_user &&
        device_priority_queue.top().id != hotplug_device.id) {
      // This handles crbug.com/698809. Before the device is powered off, unplug
      // the external output device, leave the internal audio device(such as
      // internal speaker) active. Then turn off the power, plug in the exteranl
      // device, turn on power. On some device, cras sends NodesChanged first
      // signal with only external device; then later it sends another
      // NodesChanged signal with internal audio device also discovered.
      // For such case, do not switch to the lower priority device which was
      // made active not by user's choice.
      return;
    }

    SwitchToDevice(hotplug_device, true, ACTIVATE_BY_RESTORE_PREVIOUS_STATE);
  } else {
    // The hot plugged device was not active last time it was plugged in.
    // Let's check how the current active device is activated, if it is not
    // activated by user choice, then select the hot plugged device it is of
    // higher priority.
    uint64_t& active_node_id = hotplug_device.is_input ? active_input_node_id_
                                                       : active_output_node_id_;
    const AudioDevice* active_device = GetDeviceFromId(active_node_id);
    if (!active_device) {
      // Can't find any current active device.
      // This is an odd case, but if it happens, switch to hotplug device.
      LOG(ERROR) << "Can not find current active device when the device is"
                 << " hot plugged: " << hotplug_device.ToString();
      SwitchToDevice(hotplug_device, true, ACTIVATE_BY_PRIORITY);
      return;
    }

    bool activate_by_user = false;
    bool state_active = false;
    bool found_active_state = audio_pref_handler_->GetDeviceActive(
        *active_device, &state_active, &activate_by_user);
    DCHECK(found_active_state && state_active);
    if (!found_active_state || !state_active) {
      LOG(ERROR) << "Cannot retrieve current active device's state in prefs: "
                 << active_device->ToString();
      return;
    }
    if (!activate_by_user &&
        device_priority_queue.top().id == hotplug_device.id) {
      SwitchToDevice(hotplug_device, true, ACTIVATE_BY_PRIORITY);
    } else {
      // Do not active the hotplug device. Either the current device is
      // expliciltly activated by user, or the hotplug device is of lower
      // priority.
      VLOG(1) << "Hotplug device remains inactive as its previous state:"
              << hotplug_device.ToString();
    }
  }
}

void CrasAudioHandler::SwitchToTopPriorityDevice(bool is_input) {
  AudioDevice top_device =
      is_input ? input_devices_pq_.top() : output_devices_pq_.top();
  if (!top_device.is_for_simple_usage())
    return;

  // For the dual camera and dual microphone case, choose microphone
  // that is consistent to the active camera.
  if (IsFrontOrRearMic(top_device) && HasDualInternalMic() && IsCameraOn()) {
    ActivateInternalMicForActiveCamera();
    return;
  }

  SwitchToDevice(top_device, true, ACTIVATE_BY_PRIORITY);
}

void CrasAudioHandler::SwitchToPreviousActiveDeviceIfAvailable(bool is_input) {
  AudioDevice previous_active_device;
  if (GetActiveDeviceFromUserPref(is_input, &previous_active_device)) {
    DCHECK(previous_active_device.is_for_simple_usage());
    // Switch to previous active device stored in user prefs.
    SwitchToDevice(previous_active_device, true,
                   ACTIVATE_BY_RESTORE_PREVIOUS_STATE);
  } else {
    // No previous active device, switch to the top priority device.
    SwitchToTopPriorityDevice(is_input);
  }
}

void CrasAudioHandler::UpdateDevicesAndSwitchActive(
    const AudioNodeList& nodes) {
  size_t old_output_device_size = 0;
  size_t old_input_device_size = 0;
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (device.is_input)
      ++old_input_device_size;
    else
      ++old_output_device_size;
  }

  AudioDevicePriorityQueue hotplug_output_nodes;
  AudioDevicePriorityQueue hotplug_input_nodes;
  bool has_output_removed = false;
  bool has_input_removed = false;
  bool active_output_removed = false;
  bool active_input_removed = false;
  bool output_devices_changed =
      HasDeviceChange(nodes, false, &hotplug_output_nodes, &has_output_removed,
                      &active_output_removed);
  bool input_devices_changed =
      HasDeviceChange(nodes, true, &hotplug_input_nodes, &has_input_removed,
                      &active_input_removed);
  audio_devices_.clear();
  has_alternative_input_ = false;
  has_alternative_output_ = false;

  while (!input_devices_pq_.empty())
    input_devices_pq_.pop();
  while (!output_devices_pq_.empty())
    output_devices_pq_.pop();

  size_t new_output_device_size = 0;
  size_t new_input_device_size = 0;
  for (size_t i = 0; i < nodes.size(); ++i) {
    AudioDevice device(nodes[i]);
    audio_devices_[device.id] = device;
    if (!has_alternative_input_ && device.is_input &&
        device.IsExternalDevice()) {
      has_alternative_input_ = true;
    } else if (!has_alternative_output_ && !device.is_input &&
               device.IsExternalDevice()) {
      has_alternative_output_ = true;
    }

    if (device.is_input) {
      input_devices_pq_.push(device);
      ++new_input_device_size;
    } else {
      output_devices_pq_.push(device);
      ++new_output_device_size;
    }
  }

  // Handle output device changes.
  HandleAudioDeviceChange(false, output_devices_pq_, hotplug_output_nodes,
                          output_devices_changed, has_output_removed,
                          active_output_removed);

  // Handle input device changes.
  HandleAudioDeviceChange(true, input_devices_pq_, hotplug_input_nodes,
                          input_devices_changed, has_input_removed,
                          active_input_removed);

  // content::MediaStreamManager listens to
  // base::SystemMonitor::DevicesChangedObserver for audio devices,
  // and updates EnumerateDevices when OnDevicesChanged is called.
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  // In some unittest, |monitor| might be nullptr.
  if (!monitor)
    return;
  monitor->ProcessDevicesChanged(
      base::SystemMonitor::DeviceType::DEVTYPE_AUDIO);
}

void CrasAudioHandler::HandleAudioDeviceChange(
    bool is_input,
    const AudioDevicePriorityQueue& devices_pq,
    const AudioDevicePriorityQueue& hotplug_nodes,
    bool has_device_change,
    bool has_device_removed,
    bool active_device_removed) {
  uint64_t& active_node_id =
      is_input ? active_input_node_id_ : active_output_node_id_;

  // No audio devices found.
  if (devices_pq.empty()) {
    VLOG(1) << "No " << (is_input ? "input" : "output") << " devices found";
    active_node_id = 0;
    NotifyActiveNodeChanged(is_input);
    return;
  }

  // If the previous active device is removed from the new node list,
  // or changed to inactive by cras, reset active_node_id.
  // See crbug.com/478968.
  const AudioDevice* active_device = GetDeviceFromId(active_node_id);
  if (!active_device || !active_device->active)
    active_node_id = 0;

  if (!active_node_id || hotplug_nodes.empty() || hotplug_nodes.size() > 1) {
    HandleNonHotplugNodesChange(is_input, hotplug_nodes, has_device_change,
                                has_device_removed, active_device_removed);
  } else {
    // Typical user hotplug case.
    HandleHotPlugDevice(hotplug_nodes.top(), devices_pq);
  }
}

void CrasAudioHandler::HandleGetNodes(
    base::Optional<chromeos::AudioNodeList> node_list) {
  if (!node_list.has_value()) {
    LOG(ERROR) << "Failed to retrieve audio nodes data";
    return;
  }

  if (node_list->empty())
    return;

  UpdateDevicesAndSwitchActive(node_list.value());
  for (auto& observer : observers_)
    observer.OnAudioNodesChanged();
}

void CrasAudioHandler::HandleGetNumActiveOutputStreams(
    base::Optional<int> new_output_streams_count) {
  if (!new_output_streams_count.has_value()) {
    LOG(ERROR) << "Failed to retrieve number of active output streams";
    return;
  }

  DCHECK(*new_output_streams_count >= 0);
  if (*new_output_streams_count > 0 && num_active_output_streams_ == 0) {
    for (auto& observer : observers_)
      observer.OnOutputStarted();
  } else if (*new_output_streams_count == 0 && num_active_output_streams_ > 0) {
    for (auto& observer : observers_)
      observer.OnOutputStopped();
  }
  num_active_output_streams_ = *new_output_streams_count;
}

void CrasAudioHandler::AddAdditionalActiveNode(uint64_t node_id, bool notify) {
  const AudioDevice* device = GetDeviceFromId(node_id);
  if (!device) {
    VLOG(1) << "AddActiveInputNode: Cannot find device id="
            << "0x" << std::hex << node_id;
    return;
  }

  audio_devices_[node_id].active = true;
  SetupAdditionalActiveAudioNodeState(node_id);

  if (device->is_input) {
    DCHECK(node_id != active_input_node_id_);
    CrasAudioClient::Get()->AddActiveInputNode(node_id);
    if (notify)
      NotifyActiveNodeChanged(true);
  } else {
    DCHECK(node_id != active_output_node_id_);
    CrasAudioClient::Get()->AddActiveOutputNode(node_id);
    if (notify)
      NotifyActiveNodeChanged(false);
  }
}

void CrasAudioHandler::RemoveActiveNodeInternal(uint64_t node_id, bool notify) {
  const AudioDevice* device = GetDeviceFromId(node_id);
  if (!device) {
    VLOG(1) << "RemoveActiveInputNode: Cannot find device id="
            << "0x" << std::hex << node_id;
    return;
  }

  audio_devices_[node_id].active = false;
  if (device->is_input) {
    if (node_id == active_input_node_id_)
      active_input_node_id_ = 0;
    CrasAudioClient::Get()->RemoveActiveInputNode(node_id);
    if (notify)
      NotifyActiveNodeChanged(true);
  } else {
    if (node_id == active_output_node_id_)
      active_output_node_id_ = 0;
    CrasAudioClient::Get()->RemoveActiveOutputNode(node_id);
    if (notify)
      NotifyActiveNodeChanged(false);
  }
}

void CrasAudioHandler::UpdateAudioAfterHDMIRediscoverGracePeriod() {
  VLOG(1) << "HDMI output re-discover grace period ends.";
  hdmi_rediscovering_ = false;
  if (!IsOutputMutedForDevice(active_output_node_id_)) {
    // Unmute the audio output after the HDMI transition period.
    VLOG(1) << "Unmute output after HDMI rediscovering grace period.";
    SetOutputMuteInternal(false);

    // Notify UI about the mute state change.
    for (auto& observer : observers_)
      observer.OnOutputMuteChanged(output_mute_on_);
  }
}

bool CrasAudioHandler::IsHDMIPrimaryOutputDevice() const {
  const AudioDevice* device = GetDeviceFromId(active_output_node_id_);
  return device && device->type == AUDIO_TYPE_HDMI;
}

void CrasAudioHandler::StartHDMIRediscoverGracePeriod() {
  VLOG(1) << "Start HDMI rediscovering grace period.";
  hdmi_rediscovering_ = true;
  hdmi_rediscover_timer_.Stop();
  hdmi_rediscover_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(
                     hdmi_rediscover_grace_period_duration_in_ms_),
      this, &CrasAudioHandler::UpdateAudioAfterHDMIRediscoverGracePeriod);
}

void CrasAudioHandler::SetHDMIRediscoverGracePeriodForTesting(
    int duration_in_ms) {
  hdmi_rediscover_grace_period_duration_in_ms_ = duration_in_ms;
}

void CrasAudioHandler::ActivateMicForCamera(
    media::VideoFacingMode camera_facing) {
  const AudioDevice* mic = GetMicForCamera(camera_facing);
  if (!mic || mic->active)
    return;

  SwitchToDevice(*mic, true, ACTIVATE_BY_CAMERA);
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
    SwitchToDevice(*GetDeviceByType(AUDIO_TYPE_FRONT_MIC), true,
                   ACTIVATE_BY_USER);
  }
}

const AudioDevice* CrasAudioHandler::GetMicForCamera(
    media::VideoFacingMode camera_facing) {
  switch (camera_facing) {
    case media::MEDIA_VIDEO_FACING_USER:
      return GetDeviceByType(AUDIO_TYPE_FRONT_MIC);
    case media::MEDIA_VIDEO_FACING_ENVIRONMENT:
      return GetDeviceByType(AUDIO_TYPE_REAR_MIC);
    default:
      NOTREACHED();
  }
  return nullptr;
}

bool CrasAudioHandler::HasDualInternalMic() const {
  bool has_front_mic = false;
  bool has_rear_mic = false;
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (device.type == AUDIO_TYPE_FRONT_MIC)
      has_front_mic = true;
    else if (device.type == AUDIO_TYPE_REAR_MIC)
      has_rear_mic = true;
    if (has_front_mic && has_rear_mic)
      break;
  }
  return has_front_mic && has_rear_mic;
}

bool CrasAudioHandler::IsFrontOrRearMic(const AudioDevice& device) const {
  return device.is_input && (device.type == AUDIO_TYPE_FRONT_MIC ||
                             device.type == AUDIO_TYPE_REAR_MIC);
}

bool CrasAudioHandler::IsCameraOn() const {
  return front_camera_on_ || rear_camera_on_;
}

bool CrasAudioHandler::HasExternalDevice(bool is_input) const {
  for (const auto& item : audio_devices_) {
    const AudioDevice& device = item.second;
    if (is_input == device.is_input && device.IsExternalDevice())
      return true;
  }
  return false;
}

void CrasAudioHandler::GetDefaultOutputBufferSizeInternal() {
  CrasAudioClient::Get()->GetDefaultOutputBufferSize(
      base::BindOnce(&CrasAudioHandler::HandleGetDefaultOutputBufferSize,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrasAudioHandler::HandleGetDefaultOutputBufferSize(
    base::Optional<int> buffer_size) {
  if (!buffer_size.has_value()) {
    LOG(ERROR) << "Failed to retrieve output buffer size";
    return;
  }

  default_output_buffer_size_ = buffer_size.value();
}

bool CrasAudioHandler::system_aec_supported() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return system_aec_supported_;
}

// GetSystemAecSupported() is only called in the same thread
// as the CrasAudioHanler constructor. We are safe here without
// thread check, because unittest may not have the task runner
// for the current thread.
void CrasAudioHandler::GetSystemAecSupported() {
  CrasAudioClient::Get()->GetSystemAecSupported(
      base::BindOnce(&CrasAudioHandler::HandleGetSystemAecSupported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrasAudioHandler::HandleGetSystemAecSupported(
    base::Optional<bool> system_aec_supported) {
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
// as the CrasAudioHanler constructor. We are safe here without
// thread check, because unittest may not have the task runner
// for the current thread.
void CrasAudioHandler::GetSystemAecGroupId() {
  CrasAudioClient::Get()->GetSystemAecGroupId(
      base::BindOnce(&CrasAudioHandler::HandleGetSystemAecGroupId,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrasAudioHandler::HandleGetSystemAecGroupId(
    base::Optional<int32_t> system_aec_group_id) {
  if (!system_aec_group_id.has_value()) {
    // If the group Id is not available, set the ID to reflect that.
    system_aec_group_id_ = kSystemAecGroupIdNotAvailable;
    return;
  }
  system_aec_group_id_ = system_aec_group_id.value();
}

}  // namespace chromeos
