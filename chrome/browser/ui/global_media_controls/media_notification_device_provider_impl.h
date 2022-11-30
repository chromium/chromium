// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_PROVIDER_IMPL_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_PROVIDER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_monitor.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_provider.h"
#include "media/audio/audio_device_description.h"

class MediaNotificationDeviceProviderImpl
    : public MediaNotificationDeviceProvider,
      public MediaNotificationDeviceMonitor::DevicesChangedObserver {
 public:
  explicit MediaNotificationDeviceProviderImpl(
      std::unique_ptr<media::AudioSystem> audio_system);
  MediaNotificationDeviceProviderImpl(
      const MediaNotificationDeviceProviderImpl&) = delete;
  MediaNotificationDeviceProviderImpl& operator=(
      const MediaNotificationDeviceProviderImpl&) = delete;
  ~MediaNotificationDeviceProviderImpl() override;

  // MediaNotificationDeviceProvider
  base::CallbackListSubscription RegisterOutputDeviceDescriptionsCallback(
      GetOutputDevicesCallback cb) override;

  void GetOutputDeviceDescriptions(
      media::AudioSystem::OnDeviceDescriptionsCallback on_descriptions_cb)
      override;

  // MediaNotificationDeviceMonitor::DevicesChangedObserver
  void OnDevicesChanged() override;

 private:
  void GetDevices();

  void NotifySubscribers(media::AudioDeviceDescriptions descriptions);

  void OnSubscriberRemoved();

  bool is_querying_for_output_devices_ = false;
  bool has_device_list_ = false;
  MediaNotificationDeviceProvider::GetOutputDevicesCallbackList
      output_device_callback_list_;
  std::unique_ptr<media::AudioSystem> audio_system_;
  std::unique_ptr<MediaNotificationDeviceMonitor> monitor_;
  media::AudioDeviceDescriptions audio_device_descriptions_;

  base::WeakPtrFactory<MediaNotificationDeviceProviderImpl> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_PROVIDER_IMPL_H_
