// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_EFFECTS_MEDIA_DEVICE_INFO_H_
#define COMPONENTS_MEDIA_EFFECTS_MEDIA_DEVICE_INFO_H_

#include "base/system/system_monitor.h"
#include "media/audio/audio_device_description.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/mojom/system_info.mojom.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"

namespace media_effects {

// Get the id of the real default device if present in the passed `infos`,
// otherwise return nullopt.
std::optional<std::string> GetRealDefaultDeviceId(
    const std::vector<media::AudioDeviceDescription>& infos);

// Get the id of the real communications device if present in the passed
// `infos`, otherwise return nullopt. Only relevant on Windows.
std::optional<std::string> GetRealCommunicationsDeviceId(
    const std::vector<media::AudioDeviceDescription>& infos);

// Returns a list of the real mics names by excluding virtual devices such as
// default.
std::vector<std::string> GetRealAudioDeviceNames(
    const std::vector<media::AudioDeviceDescription>& infos);

// Returns a list of the cameras names.
std::vector<std::string> GetRealVideoDeviceNames(
    const std::vector<media::VideoCaptureDeviceInfo>& infos);

// This class manages a cache of device infos for currently connected audio and
// video capture devices. It is similar to `MediaCaptureDevicesImpl` from
// content, but it holds the media::types instead of blink::MediaStreamDevice.
// It allows usages within chrome to access the full device information.
class MediaDeviceInfo : public base::SystemMonitor::DevicesChangedObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // If the cache hasn't yet received the device list from the service,
    // `device_infos` will be nullopt.
    virtual void OnAudioDevicesChanged(
        const std::optional<std::vector<media::AudioDeviceDescription>>&
            device_infos) {}
    // If the cache hasn't yet received the device list from the service,
    // `device_infos` will be nullopt.
    virtual void OnVideoDevicesChanged(
        const std::optional<std::vector<media::VideoCaptureDeviceInfo>>&
            device_infos) {}
  };

  ~MediaDeviceInfo() override;
  static MediaDeviceInfo* GetInstance();
  // Create a new instance and use it to override the singleton for testing.
  // This allows tests to be hermetic by creating a new cache in SetUp.
  static std::pair<std::unique_ptr<MediaDeviceInfo>,
                   base::AutoReset<MediaDeviceInfo*>>
  OverrideInstanceForTesting();

  // Get the cached device infos synchronously. If the cache hasn't yet received
  // the device list from the service, this will return nullopt.
  std::optional<media::AudioDeviceDescription> GetAudioDeviceInfoForId(
      const std::string& device_id) const;
  std::optional<media::VideoCaptureDeviceInfo> GetVideoDeviceInfoForId(
      const std::string& device_id) const;

  const std::optional<std::vector<media::AudioDeviceDescription>>&
  GetAudioDeviceInfos() const;
  const std::optional<std::vector<media::VideoCaptureDeviceInfo>>&
  GetVideoDeviceInfos() const;

  // Used to get mic format info (e.g. sample rate).
  void GetAudioInputStreamParameters(
      const std::string& device_id,
      audio::mojom::SystemInfo::GetInputStreamParametersCallback callback);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Allow `NoDestructor` to use the private constructor.
  friend class base::NoDestructor<MediaDeviceInfo>;
  MediaDeviceInfo();
  // base::SystemMonitor::DevicesChangedObserver implementation
  void OnDevicesChanged(base::SystemMonitor::DeviceType device_type) override;

  void OnAudioDeviceDescriptionsReceived(
      const std::vector<media::AudioDeviceDescription> device_infos);

  void OnVideoDeviceInfosReceived(
      const std::vector<media::VideoCaptureDeviceInfo>& device_infos);

  void NotifyAudioDevicesChanged();
  void NotifyVideoDevicesChanged();

  // Mojo remotes for getting device infos.
  mojo::Remote<video_capture::mojom::VideoSourceProvider>
      video_source_provider_;
  mojo::Remote<audio::mojom::SystemInfo> audio_system_info_;

  // Cached vector of device infos. This is updated on every device change.
  std::optional<std::vector<media::VideoCaptureDeviceInfo>> video_device_infos_;
  // Cached vector of device infos. This is updated on every device change.
  std::optional<std::vector<media::AudioDeviceDescription>> audio_device_infos_;

  base::ObserverList<Observer> observers_;
};

}  // namespace media_effects
#endif  // COMPONENTS_MEDIA_EFFECTS_MEDIA_DEVICE_INFO_H_
