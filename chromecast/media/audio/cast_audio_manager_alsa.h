// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_ALSA_H_
#define CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_ALSA_H_

#include <memory>
#include <string>

#include "base/task/single_thread_task_runner.h"
#include "chromecast/media/audio/cast_audio_manager.h"
#include "chromecast/media/audio/cast_audio_manager_helper.h"

namespace media {
class AlsaWrapper;
}

namespace chromecast {

namespace media {

class CastAudioManagerAlsa : public CastAudioManager {
 public:
  enum StreamType {
    kStreamPlayback = 0,
    kStreamCapture,
  };

  CastAudioManagerAlsa(
      std::unique_ptr<::media::AudioThread> audio_thread,
      ::media::AudioLogFactory* audio_log_factory,
      CastAudioManagerHelper::Delegate* delegate,
      base::RepeatingCallback<CmaBackendFactory*()> backend_factory_getter,
      scoped_refptr<base::SingleThreadTaskRunner> browser_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      bool use_mixer);

  CastAudioManagerAlsa(const CastAudioManagerAlsa&) = delete;
  CastAudioManagerAlsa& operator=(const CastAudioManagerAlsa&) = delete;

  ~CastAudioManagerAlsa() override;

  // CastAudioManager implementation.
  bool HasAudioInputDevices() override;
  void GetAudioInputDeviceNames(
      ::media::AudioDeviceNames* device_names) override;
  ::media::AudioParameters GetInputStreamParameters(
      const std::string& device_id) override;

 private:
  // CastAudioManager implementation.
  ::media::AudioInputStream* MakeLinearInputStream(
      const ::media::AudioParameters& params,
      const std::string& device_id,
      const ::media::AudioManager::LogCallback& log_callback) override;
  ::media::AudioInputStream* MakeLowLatencyInputStream(
      const ::media::AudioParameters& params,
      const std::string& device_id,
      const ::media::AudioManager::LogCallback& log_callback) override;

  ::media::AudioInputStream* MakeInputStream(
      const ::media::AudioParameters& params,
      const std::string& device_id);

  // Gets a list of available ALSA devices.
  void GetAlsaAudioDevices(StreamType type,
                           ::media::AudioDeviceNames* device_names);

  // Gets the ALSA devices' names and ids that support streams of the
  // given type.
  void GetAlsaDevicesInfo(StreamType type,
                          void** hint,
                          ::media::AudioDeviceNames* device_names);

  std::unique_ptr<::media::AlsaWrapper> wrapper_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_ALSA_H_
