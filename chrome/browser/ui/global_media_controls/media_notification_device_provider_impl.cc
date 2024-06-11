// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_notification_device_provider_impl.h"

#include <algorithm>

#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_monitor.h"
#include "content/public/browser/audio_service.h"
#include "media/audio/audio_device_description.h"

namespace {
// Remove the default fallback device from the given list of audio device
// descriptions if it is possible to determine which of the other devices is
// fallen back to. If such a device is found, it's unique id will be
// overwritten with |media::AudioDeviceDescription::kDefaultDeviceId|.
void MaybeRemoveDefaultDevice(media::AudioDeviceDescriptions& descriptions) {
  // Determine which of the audio devices is the fallback "default" device.
  auto default_device_it = base::ranges::find(
      descriptions, media::AudioDeviceDescription::kDefaultDeviceId,
      &media::AudioDeviceDescription::unique_id);

  // If there is no default device, there is nothing to remove.
  if (default_device_it == descriptions.end())
    return;

  // If name of the device associated with the default id is known, the default
  // device description will contain that name prefixed by a localized string.
  // See media::AudioDeviceDescription::LocalizeDeviceDescriptions for more
  // context.
  // Note that due to complexities with how the default device is reported by
  // PulseAudio, the default device name will not contain the name of the real
  // device on Linux. In this case, the default device name does not contain
  // the following prefix.
  const std::string default_device_name_prefix =
      media::AudioDeviceDescription::GetDefaultDeviceName() + " - ";

  if (base::StartsWith(default_device_it->device_name,
                       default_device_name_prefix)) {
    std::string real_default_device_name =
        default_device_it->device_name.substr(
            default_device_name_prefix.size());

    // Find all the devices that have the name of the real default device.
    std::vector<media::AudioDeviceDescriptions::iterator>
        devices_with_real_default_name;
    for (auto it = descriptions.begin(); it != descriptions.end(); ++it) {
      if (it->device_name == real_default_device_name) {
        devices_with_real_default_name.push_back(it);
      }
    }

    if (devices_with_real_default_name.size() == 1) {
      // If there is only one device that has name of the real default device,
      // there is no ambiguity as to if this device is the real default device.
      // In this case, we should remove the "default" fallback device from the
      // list and mark the real device as "default".
      devices_with_real_default_name.front()->unique_id =
          media::AudioDeviceDescription::kDefaultDeviceId;
      descriptions.erase(default_device_it);
    }
  }
}

}  // anonymous namespace

MediaNotificationDeviceProviderImpl::MediaNotificationDeviceProviderImpl(
    std::unique_ptr<media::AudioSystem> audio_system)
    : audio_system_(std::move(audio_system)) {
  output_device_callback_list_.set_removal_callback(base::BindRepeating(
      &MediaNotificationDeviceProviderImpl::OnSubscriberRemoved,
      weak_ptr_factory_.GetWeakPtr()));
}

MediaNotificationDeviceProviderImpl::~MediaNotificationDeviceProviderImpl() {
  if (monitor_)
    monitor_->RemoveDevicesChangedObserver(this);
}

base::CallbackListSubscription
MediaNotificationDeviceProviderImpl::RegisterOutputDeviceDescriptionsCallback(
    GetOutputDevicesCallback cb) {
  if (!monitor_) {
    monitor_ = MediaNotificationDeviceMonitor::Create(this);
    monitor_->AddDevicesChangedObserver(this);
  }
  monitor_->StartMonitoring();
  if (has_device_list_)
    cb.Run(audio_device_descriptions_);
  auto subscription = output_device_callback_list_.Add(std::move(cb));
  if (!has_device_list_)
    GetDevices();
  return subscription;
}

void MediaNotificationDeviceProviderImpl::GetOutputDeviceDescriptions(
    media::AudioSystem::OnDeviceDescriptionsCallback cb) {
  if (!audio_system_)
    audio_system_ = content::CreateAudioSystemForAudioService();
  audio_system_->GetDeviceDescriptions(
      /*for_input=*/false,
      base::BindOnce(
          [](media::AudioSystem::OnDeviceDescriptionsCallback cb,
             media::AudioDeviceDescriptions descriptions) {
            MaybeRemoveDefaultDevice(descriptions);
            std::move(cb).Run(descriptions);
          },
          std::move(cb)));
}

void MediaNotificationDeviceProviderImpl::OnDevicesChanged() {
  GetDevices();
}

void MediaNotificationDeviceProviderImpl::GetDevices() {
  if (is_querying_for_output_devices_)
    return;
  is_querying_for_output_devices_ = true;
  GetOutputDeviceDescriptions(
      base::BindOnce(&MediaNotificationDeviceProviderImpl::NotifySubscribers,
                     weak_ptr_factory_.GetWeakPtr()));
}

void MediaNotificationDeviceProviderImpl::NotifySubscribers(
    media::AudioDeviceDescriptions descriptions) {
  is_querying_for_output_devices_ = false;
  audio_device_descriptions_ = std::move(descriptions);
  has_device_list_ = true;
  output_device_callback_list_.Notify(audio_device_descriptions_);
}

void MediaNotificationDeviceProviderImpl::OnSubscriberRemoved() {
  if (output_device_callback_list_.empty())
    monitor_->StopMonitoring();
}
