// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_AUDIO_DECODER_ANDROID_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_AUDIO_DECODER_ANDROID_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "chromecast/media/api/cast_audio_decoder.h"
#include "chromecast/media/cma/backend/android/audio_sink_android.h"
#include "chromecast/media/cma/backend/android/audio_sink_manager.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "media/base/audio_buffer.h"
#include "media/base/media_util.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {
class AudioBus;
class AudioRendererAlgorithm;
}  // namespace media

namespace chromecast {
namespace media {
class DecoderBufferBase;
class MediaPipelineBackendAndroid;

// TODO(ckuiper): This class is very similar to AudioDecoderAlsa
// (alsa/audio_decoder_alsa.h) and should be consolidated into one shared
// class/file.
class AudioDecoderAndroid : public MediaPipelineBackend::AudioDecoder,
                            public AudioSinkAndroid::Delegate {
 public:
  using BufferStatus = MediaPipelineBackend::BufferStatus;

  AudioDecoderAndroid(MediaPipelineBackendAndroid* backend, bool is_apk_audio);

  AudioDecoderAndroid(const AudioDecoderAndroid&) = delete;
  AudioDecoderAndroid& operator=(const AudioDecoderAndroid&) = delete;

  ~AudioDecoderAndroid() override;

  void Initialize();
  bool Start(int64_t start_pts);
  void Stop();
  bool Pause();
  bool Resume();
  bool SetPlaybackRate(float rate);

  int64_t current_pts() const { return current_pts_; }

  // MediaPipelineBackend::AudioDecoder implementation:
  void SetDelegate(MediaPipelineBackend::Decoder::Delegate* delegate) override;
  BufferStatus PushBuffer(CastDecoderBuffer* buffer) override;
  void GetStatistics(Statistics* statistics) override;
  bool SetConfig(const AudioConfig& config) override;
  bool SetVolume(float multiplier) override;
  RenderingDelay GetRenderingDelay() override;
  AudioTrackTimestamp GetAudioTrackTimestamp() override;
  int GetStartThresholdInFrames() override;

 private:
  struct RateShifterInfo {
    explicit RateShifterInfo(float playback_rate);

    double rate;
    double input_frames;
    int64_t output_frames;
  };

  // AudioSinkAndroid::Delegate implementation:
  void OnWritePcmCompletion(BufferStatus status) override;
  void OnSinkError(SinkError error) override;

  void CleanUpPcm();
  bool ResetSinkForNewConfig(const AudioConfig& config) ABSL_MUST_USE_RESULT;
  void CreateDecoder();
  void CreateRateShifter(const AudioConfig& config);
  void OnBufferDecoded(uint64_t input_bytes,
                       CastAudioDecoder::Status status,
                       const AudioConfig& config,
                       scoped_refptr<DecoderBufferBase> decoded);
  void CheckBufferComplete();
  void PushRateShifted();
  void PushMorePcm();
  void RunEos();
  bool BypassDecoder() const;
  bool ShouldStartClock() const;
  void UpdateStatistics(Statistics delta);

  MediaPipelineBackendAndroid* const backend_;
  const bool is_apk_audio_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  MediaPipelineBackend::Decoder::Delegate* delegate_;

  Statistics stats_;

  bool pending_buffer_complete_;
  bool got_eos_;
  bool pushed_eos_;
  bool sink_error_;

  AudioConfig config_;
  std::unique_ptr<CastAudioDecoder> decoder_;

  std::unique_ptr<::media::AudioRendererAlgorithm> rate_shifter_;
  ::media::NullMediaLog media_log_;
  base::circular_deque<RateShifterInfo> rate_shifter_info_;
  std::unique_ptr<::media::AudioBus> rate_shifter_output_;

  int64_t current_pts_;

  ManagedAudioSink sink_;
  int64_t pending_output_frames_;
  float volume_multiplier_;

  scoped_refptr<::media::AudioBufferMemoryPool> pool_;

  base::WeakPtrFactory<AudioDecoderAndroid> weak_factory_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_AUDIO_DECODER_ANDROID_H_
