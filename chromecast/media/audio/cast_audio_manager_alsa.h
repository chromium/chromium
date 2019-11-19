// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_ALSA_H_
#define CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_ALSA_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "chromecast/media/audio/cast_audio_manager.h"

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
      base::RepeatingCallback<CmaBackendFactory*()> backend_factory_getter,
      GetSessionIdCallback get_session_id_callback,
      scoped_refptr<base::SingleThreadTaskRunner> browser_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      service_manager::Connector* connector,
      bool use_mixer);
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

  DISALLOW_COPY_AND_ASSIGN(CastAudioManagerAlsa);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_ALSA_H_
