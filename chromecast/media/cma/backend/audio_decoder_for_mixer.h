// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_DECODER_FOR_MIXER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_DECODER_FOR_MIXER_H_

#include <cstdint>
#include <memory>

#include "base/bind.h"
#include "base/containers/circular_deque.h"
#include "base/location.h"
#include "chromecast/media/audio/mixer_service/output_stream_connection.h"
#include "chromecast/media/cma/decoder/cast_audio_decoder.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "media/base/audio_buffer.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace chromecast {
class AudioResampler;
class IOBufferPool;

namespace media {
class DecoderBufferBase;
class MediaPipelineBackendForMixer;

// AudioDecoder implementation that streams decoded stream to the StreamMixer.
class AudioDecoderForMixer
    : public MediaPipelineBackend::AudioDecoder,
      public mixer_service::OutputStreamConnection::Delegate {
 public:
  using BufferStatus = MediaPipelineBackend::BufferStatus;

  explicit AudioDecoderForMixer(MediaPipelineBackendForMixer* backend);
  ~AudioDecoderForMixer() override;

  virtual void Initialize();
  virtual bool Start(int64_t pts, bool start_playback_asap);
  void StartPlaybackAt(int64_t timestamp);
  virtual void Stop();
  virtual bool Pause();
  virtual bool Resume();
  virtual float SetPlaybackRate(float rate);
  virtual bool GetTimestampedPts(int64_t* timestamp, int64_t* pts) const;
  virtual int64_t GetCurrentPts() const;

  // MediaPipelineBackend::AudioDecoder implementation:
  void SetDelegate(MediaPipelineBackend::Decoder::Delegate* delegate) override;
  BufferStatus PushBuffer(CastDecoderBuffer* buffer) override;
  void GetStatistics(Statistics* statistics) override;
  bool SetConfig(const AudioConfig& config) override;
  bool SetVolume(float multiplier) override;
  RenderingDelay GetRenderingDelay() override;

  // This allows for very small changes in the rate of audio playback that are
  // (supposedly) imperceptible.
  float SetAvSyncPlaybackRate(float rate);
  void RestartPlaybackAt(int64_t pts, int64_t timestamp);

  RenderingDelay GetMixerRenderingDelay();

 private:
  friend class MockAudioDecoderForMixer;
  friend class AvSyncTest;

  // mixer_service::OutputStreamConnection::Delegate implementation:
  void FillNextBuffer(void* buffer,
                      int frames,
                      int64_t playout_timestamp) override;
  void OnAudioReadyForPlayback(int64_t mixer_delay) override;
  void OnEosPlayed() override;
  void OnMixerError() override;

  void CreateBufferPool(const AudioConfig& config, int frame_count);
  void CreateMixerInput(const AudioConfig& config, bool start_playback_asap);
  void CleanUpPcm();
  void ResetMixerInputForNewConfig(const AudioConfig& config);
  void CreateDecoder();

  void OnDecoderInitialized(bool success);
  void OnBufferDecoded(uint64_t input_bytes,
                       bool has_config,
                       CastAudioDecoder::Status status,
                       const AudioConfig& config,
                       scoped_refptr<DecoderBufferBase> decoded);
  void CheckBufferComplete();
  void WritePcm(scoped_refptr<DecoderBufferBase> buffer);
  bool BypassDecoder() const;
  void UpdateStatistics(Statistics delta);

  MediaPipelineBackendForMixer* const backend_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  MediaPipelineBackend::Decoder::Delegate* delegate_ = nullptr;

  Statistics stats_;

  int buffer_pool_frames_ = 0;
  bool pending_buffer_complete_ = false;
  bool mixer_error_ = false;
  bool paused_ = false;
  bool reported_ready_for_playback_ = false;
  RenderingDelay mixer_delay_;

  AudioConfig config_;
  std::unique_ptr<CastAudioDecoder> decoder_;

  std::unique_ptr<AudioResampler> audio_resampler_;
  float av_sync_clock_rate_ = 1.0f;

  std::unique_ptr<mixer_service::OutputStreamConnection> mixer_input_;

  RenderingDelay next_buffer_delay_;
  int64_t pending_output_frames_ = -1;
  float volume_multiplier_ = 1.0f;

  int64_t last_push_pts_ = INT64_MIN;
  int64_t last_push_playout_timestamp_ = INT64_MIN;

  scoped_refptr<::media::AudioBufferMemoryPool> pool_;
  scoped_refptr<IOBufferPool> buffer_pool_;

  int64_t playback_start_pts_ = 0;
  bool start_playback_asap_ = false;

  base::WeakPtrFactory<AudioDecoderForMixer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioDecoderForMixer);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_DECODER_FOR_MIXER_H_
