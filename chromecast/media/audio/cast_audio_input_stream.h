// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_INPUT_STREAM_H_
#define CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_INPUT_STREAM_H_

#include <memory>
#include <string>

#include "base/threading/thread_checker.h"
#include "chromecast/media/audio/capture_service/capture_service_receiver.h"
#include "chromecast/media/audio/capture_service/constants.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"

namespace media {
class AudioManagerBase;
}  // namespace media

namespace chromecast {
namespace media {

class CaptureServiceReceiver;
class CastAudioManager;

class CastAudioInputStream : public ::media::AudioInputStream,
                             public CaptureServiceReceiver::Delegate {
 public:
  CastAudioInputStream(::media::AudioManagerBase* audio_manager,
                       const ::media::AudioParameters& audio_params,
                       const std::string& device_id);
  CastAudioInputStream(const CastAudioInputStream&) = delete;
  CastAudioInputStream& operator=(const CastAudioInputStream&) = delete;
  ~CastAudioInputStream() override;

  // ::media::AudioInputStream implementation:
  ::media::AudioInputStream::OpenOutcome Open() override;
  void Start(AudioInputCallback* source_callback) override;
  void Stop() override;
  void Close() override;
  double GetMaxVolume() override;
  void SetVolume(double volume) override;
  double GetVolume() override;
  bool SetAutomaticGainControl(bool enabled) override;
  bool GetAutomaticGainControl() override;
  bool IsMuted() override;
  void SetOutputDeviceForAec(const std::string& output_device_id) override;

  // CaptureServiceReceiver::Delegate implementation:
  bool OnInitialStreamInfo(
      const capture_service::StreamInfo& stream_info) override;
  bool OnCaptureData(const char* data, size_t size) override;
  void OnCaptureError() override;
  void OnCaptureMetadata(const char* data, size_t size) override;

 private:
  // Hold a raw pointer to audio manager to inform releasing |this|. The pointer
  // may be null, if |this| is not created by audio manager, e.g., in unit test.
  ::media::AudioManagerBase* const audio_manager_;
  const ::media::AudioParameters audio_params_;
  capture_service::StreamInfo stream_info_;
  std::unique_ptr<CaptureServiceReceiver> capture_service_receiver_;
  AudioInputCallback* input_callback_ = nullptr;
  std::unique_ptr<::media::AudioBus> audio_bus_;

  THREAD_CHECKER(audio_thread_checker_);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_INPUT_STREAM_H_
