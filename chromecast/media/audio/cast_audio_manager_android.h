// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_ANDROID_H_
#define CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_ANDROID_H_

#include <memory>
#include <string>

#include "base/task/single_thread_task_runner.h"
#include "chromecast/media/audio/cast_audio_manager_helper.h"
#include "media/audio/android/audio_manager_android.h"

namespace chromecast::media {

class CastAudioManagerAndroid : public ::media::AudioManagerAndroid {
 public:
  CastAudioManagerAndroid(
      std::unique_ptr<::media::AudioThread> audio_thread,
      ::media::AudioLogFactory* audio_log_factory,
      CastAudioManagerHelper::Delegate* delegate,
      base::RepeatingCallback<CmaBackendFactory*()> backend_factory_getter,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner);
  ~CastAudioManagerAndroid() override;

  CastAudioManagerAndroid(const CastAudioManagerAndroid&) = delete;
  CastAudioManagerAndroid& operator=(const CastAudioManagerAndroid&) = delete;

  // AudioManager implementation.
  void GetAudioOutputDeviceNames(
      ::media::AudioDeviceNames* device_names) override;
  ::media::AudioOutputStream* MakeAudioOutputStreamProxy(
      const ::media::AudioParameters& params,
      const std::string& device_id) override;
  ::media::AudioOutputStream* MakeLinearOutputStream(
      const ::media::AudioParameters& params,
      const ::media::AudioManager::LogCallback& log_callback) override;
  ::media::AudioOutputStream* MakeLowLatencyOutputStream(
      const ::media::AudioParameters& params,
      const std::string& device_id_or_group_id,
      const ::media::AudioManager::LogCallback& log_callback) override;
  ::media::AudioOutputStream* MakeBitstreamOutputStream(
      const ::media::AudioParameters& params,
      const std::string& device_id,
      const ::media::AudioManager::LogCallback& log_callback) override;

  bool HasAudioInputDevices() override;
  void GetAudioInputDeviceNames(
      ::media::AudioDeviceNames* device_names) override;
  ::media::AudioParameters GetInputStreamParameters(
      const std::string& device_id) override;

  // Make this public for testing.
  using ::media::AudioManagerBase::GetOutputStreamParameters;

 private:
  friend class CastAudioManagerTest;

  // CastAudioManager implementation.
  ::media::AudioInputStream* MakeLinearInputStream(
      const ::media::AudioParameters& params,
      const std::string& device_id,
      const ::media::AudioManager::LogCallback& log_callback) override;
  ::media::AudioInputStream* MakeLowLatencyInputStream(
      const ::media::AudioParameters& params,
      const std::string& device_id,
      const ::media::AudioManager::LogCallback& log_callback) override;

  CastAudioManagerHelper helper_;
};

}  // namespace chromecast::media

#endif  // CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_ANDROID_H_
