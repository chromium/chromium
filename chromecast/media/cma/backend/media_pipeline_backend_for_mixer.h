// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_FOR_MIXER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_FOR_MIXER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/media/cma/backend/video_decoder_for_mixer.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "chromecast/public/volume_control.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace chromecast {
namespace media {

class AvSync;
class AudioDecoderForMixer;
class VideoDecoderForMixer;

// CMA Backend implementation for audio devices.
class MediaPipelineBackendForMixer : public MediaPipelineBackend,
                                     public VideoDecoderForMixer::Observer {
 public:
  explicit MediaPipelineBackendForMixer(
      const MediaPipelineDeviceParams& params);
  ~MediaPipelineBackendForMixer() override;

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

  // VideoDecoderForMixer::Observer implementation:
  void VideoReadyToPlay() override;

  bool Primary() const;
  std::string DeviceId() const;
  AudioContentType ContentType() const;
  AudioChannel AudioChannel() const;
  const scoped_refptr<base::SingleThreadTaskRunner>& GetTaskRunner() const;
  VideoDecoderForMixer* video_decoder() const { return video_decoder_.get(); }
  AudioDecoderForMixer* audio_decoder() const { return audio_decoder_.get(); }
  void OnAudioReadyForPlayback();
  void NewAudioPlaybackRateInEffect(float rate);

  // Gets current time on the same clock as the rendering delay timestamp.
  virtual int64_t MonotonicClockNow() const;

  int64_t GetPlaybackStartTimeForTesting() const {
    return start_playback_timestamp_us_;
  }
  int64_t GetPlaybackStartPtsForTesting() const {
    return start_playback_pts_us_;
  }

 protected:
  std::unique_ptr<VideoDecoderForMixer> video_decoder_;
  std::unique_ptr<AudioDecoderForMixer> audio_decoder_;

 private:
  // State variable for DCHECKing caller correctness.
  enum State {
    kStateUninitialized,
    kStateInitialized,
    kStatePlaying,
    kStatePaused,
  };
  State state_;

  void OnVideoReadyToPlay();
  bool IsIgnorePtsMode() const;
  void TryStartPlayback();

  const MediaPipelineDeviceParams params_;

  std::unique_ptr<AvSync> av_sync_;
  int64_t start_playback_timestamp_us_ = INT64_MIN;
  int64_t start_playback_pts_us_ = INT64_MIN;

  // The media pipeline doesn't start playback until these 3 conditions are
  // met.
  bool audio_ready_to_play_ = false;
  bool video_ready_to_play_ = false;
  bool playback_started_ = false;
  float starting_playback_rate_ = 1.0;

  base::WeakPtr<MediaPipelineBackendForMixer> weak_this_;
  base::WeakPtrFactory<MediaPipelineBackendForMixer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaPipelineBackendForMixer);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_FOR_MIXER_H_
