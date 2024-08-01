// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_effects/media_device_info.h"

#include "content/public/browser/audio_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/video_capture_service.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"

namespace {
media_effects::MediaDeviceInfo* g_instance_override = nullptr;
}

namespace media_effects {

std::optional<std::string> GetRealDefaultDeviceId(
    const std::vector<media::AudioDeviceDescription>& infos) {
  for (const auto& info : infos) {
    if (info.is_system_default &&
        !media::AudioDeviceDescription::IsDefaultDevice(info.unique_id)) {
      return info.unique_id;
    }
  }
  return std::nullopt;
}

std::optional<std::string> GetRealCommunicationsDeviceId(
    const std::vector<media::AudioDeviceDescription>& infos) {
  for (const auto& info : infos) {
    if (info.is_communications_device &&
        !media::AudioDeviceDescription::IsCommunicationsDevice(
            info.unique_id)) {
      return info.unique_id;
    }
  }
  return std::nullopt;
}

std::vector<std::string> GetRealAudioDeviceNames(
    const std::vector<media::AudioDeviceDescription>& infos) {
  std::vector<std::string> real_names;
  for (const auto& info : infos) {
    if (!media::AudioDeviceDescription::IsDefaultDevice(info.unique_id) &&
        !media::AudioDeviceDescription::IsCommunicationsDevice(
            info.unique_id)) {
      real_names.push_back(info.device_name);
    }
  }
  return real_names;
}

std::vector<std::string> GetRealVideoDeviceNames(
    const std::vector<media::VideoCaptureDeviceInfo>& infos) {
  std::vector<std::string> names;
  for (const auto& info : infos) {
    names.push_back(info.descriptor.GetNameAndModel());
  }
  return names;
}

MediaDeviceInfo::MediaDeviceInfo() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (auto* monitor = base::SystemMonitor::Get(); monitor) {
    monitor->AddDevicesChangedObserver(this);
  }

  content::GetVideoCaptureService().ConnectToVideoSourceProvider(
      video_source_provider_.BindNewPipeAndPassReceiver());
  content::GetAudioService().BindSystemInfo(
      audio_system_info_.BindNewPipeAndPassReceiver());
  audio_system_info_.reset_on_disconnect();
  video_source_provider_.reset_on_disconnect();

  // Initialize the device lists. `base::SystemMonitor` only calls observers for
  // changes that happen after they start observing.
  OnDevicesChanged(base::SystemMonitor::DeviceType::DEVTYPE_AUDIO);
  OnDevicesChanged(base::SystemMonitor::DeviceType::DEVTYPE_VIDEO_CAPTURE);
}

MediaDeviceInfo::~MediaDeviceInfo() {
  if (auto* monitor = base::SystemMonitor::Get(); monitor) {
    monitor->RemoveDevicesChangedObserver(this);
  }
}

MediaDeviceInfo* MediaDeviceInfo::GetInstance() {
  if (g_instance_override) {
    return g_instance_override;
  }

  static base::NoDestructor<MediaDeviceInfo> instance;
  return instance.get();
}

std::pair<std::unique_ptr<MediaDeviceInfo>, base::AutoReset<MediaDeviceInfo*>>
MediaDeviceInfo::OverrideInstanceForTesting() {
  auto instance = base::WrapUnique(new MediaDeviceInfo());
  base::AutoReset auto_reset{
      &g_instance_override,
      // Can't use std::make_optional because the constructor is private.
      instance.get()};
  return std::make_pair(std::move(instance), std::move(auto_reset));
}

std::optional<media::AudioDeviceDescription>
MediaDeviceInfo::GetAudioDeviceInfoForId(const std::string& device_id) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!audio_device_infos_) {
    return std::nullopt;
  }

  auto device_iter =
      base::ranges::find(audio_device_infos_.value(), device_id,
                         &media::AudioDeviceDescription::unique_id);
  if (device_iter != audio_device_infos_->end()) {
    return *device_iter;
  }
  return std::nullopt;
}

std::optional<media::VideoCaptureDeviceInfo>
MediaDeviceInfo::GetVideoDeviceInfoForId(const std::string& device_id) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!video_device_infos_) {
    return std::nullopt;
  }

  auto device_iter =
      base::ranges::find(video_device_infos_.value(), device_id,
                         [](const media::VideoCaptureDeviceInfo& device) {
                           return device.descriptor.device_id;
                         });
  if (device_iter != video_device_infos_->end()) {
    return *device_iter;
  }
  return std::nullopt;
}

const std::optional<std::vector<media::AudioDeviceDescription>>&
MediaDeviceInfo::GetAudioDeviceInfos() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return audio_device_infos_;
}
const std::optional<std::vector<media::VideoCaptureDeviceInfo>>&
MediaDeviceInfo::GetVideoDeviceInfos() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return video_device_infos_;
}

void MediaDeviceInfo::GetAudioInputStreamParameters(
    const std::string& device_id,
    audio::mojom::SystemInfo::GetInputStreamParametersCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (audio_system_info_) {
    audio_system_info_->GetInputStreamParameters(device_id,
                                                 std::move(callback));
  }
}

void MediaDeviceInfo::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!observers_.HasObserver(observer)) {
    observers_.AddObserver(observer);
  }
}

void MediaDeviceInfo::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observers_.RemoveObserver(observer);
}

void MediaDeviceInfo::OnDevicesChanged(
    base::SystemMonitor::DeviceType device_type) {
  if (device_type == base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE &&
      video_source_provider_) {
    // Unretained is safe here because the `video_source_provider_` remote will
    // be destroyed before `this`.
    video_source_provider_->GetSourceInfos(base::BindOnce(
        [](MediaDeviceInfo* mdi,
           video_capture::mojom::VideoSourceProvider::GetSourceInfosResult,
           const std::vector<media::VideoCaptureDeviceInfo>& device_infos) {
          mdi->OnVideoDeviceInfosReceived(device_infos);
        },
        base::Unretained(this)));
  } else if (device_type == base::SystemMonitor::DEVTYPE_AUDIO &&
             audio_system_info_) {
    // Unretained is safe here because the `audio_system_info_` remote will
    // be destroyed before `this`.
    audio_system_info_->GetInputDeviceDescriptions(
        base::BindOnce(&MediaDeviceInfo::OnAudioDeviceDescriptionsReceived,
                       base::Unretained(this)));
  }
}

void MediaDeviceInfo::OnAudioDeviceDescriptionsReceived(
    const std::vector<media::AudioDeviceDescription> device_infos) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  audio_device_infos_ = device_infos;
  NotifyAudioDevicesChanged();
}

void MediaDeviceInfo::OnVideoDeviceInfosReceived(
    const std::vector<media::VideoCaptureDeviceInfo>& device_infos) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  video_device_infos_ = device_infos;
  NotifyVideoDevicesChanged();
}

void MediaDeviceInfo::NotifyAudioDevicesChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (auto& observer : observers_) {
    observer.OnAudioDevicesChanged(audio_device_infos_);
  }
}

void MediaDeviceInfo::NotifyVideoDevicesChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (auto& observer : observers_) {
    observer.OnVideoDevicesChanged(video_device_infos_);
  }
}

}  // namespace media_effects
