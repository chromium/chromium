// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_MEDIA_PIPELINE_BACKEND_ANDROID_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_MEDIA_PIPELINE_BACKEND_ANDROID_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "chromecast/public/volume_control.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace chromecast {
namespace media {
class AudioDecoderAndroid;
class VideoDecoderNull;

// TODO(ckuiper): This class is very similar to MediaPipelineBackendForMixer
// (alsa/media_pipeline_backend_alsa.h) and should be consolidated into one
// shared class/file.
class MediaPipelineBackendAndroid : public MediaPipelineBackend {
 public:
  using RenderingDelay = AudioDecoder::RenderingDelay;
  using AudioTrackTimestamp = AudioDecoder::AudioTrackTimestamp;

  explicit MediaPipelineBackendAndroid(const MediaPipelineDeviceParams& params);

  MediaPipelineBackendAndroid(const MediaPipelineBackendAndroid&) = delete;
  MediaPipelineBackendAndroid& operator=(const MediaPipelineBackendAndroid&) =
      delete;

  ~MediaPipelineBackendAndroid() override;

  // MediaPipelineBackend implementation:
  AudioDecoder* CreateAudioDecoder() override;
  VideoDecoder* CreateVideoDecoder() override;
  bool Initialize() override;
  bool Start(int64_t start_pts) override;
  void Stop() override;
  bool Pause() override;
  bool Resume() override;
  bool SetPlaybackRate(float rate) override;
  int64_t GetCurrentPts() override;

  bool Primary() const;
  std::string DeviceId() const;
  AudioContentType ContentType() const;
  AudioChannel AudioChannel() const;
  const scoped_refptr<base::SingleThreadTaskRunner>& GetTaskRunner() const;

 private:
  // State variable for DCHECKing caller correctness.
  enum State {
    kStateUninitialized,
    kStateInitialized,
    kStatePlaying,
    kStatePaused,
  };
  State state_;

  const MediaPipelineDeviceParams params_;
  std::unique_ptr<VideoDecoderNull> video_decoder_;
  std::unique_ptr<AudioDecoderAndroid> audio_decoder_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_MEDIA_PIPELINE_BACKEND_ANDROID_H_
