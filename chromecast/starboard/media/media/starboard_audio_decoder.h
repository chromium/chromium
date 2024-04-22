// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_MEDIA_MEDIA_STARBOARD_AUDIO_DECODER_H_
#define CHROMECAST_STARBOARD_MEDIA_MEDIA_STARBOARD_AUDIO_DECODER_H_

#include <optional>

#include "base/sequence_checker.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "chromecast/starboard/media/media/starboard_decoder.h"
#include "chromecast/starboard/media/media/starboard_resampler.h"

namespace chromecast {
namespace media {

// An AudioDecoder that sends buffers to a Starboard SbPlayer.
//
// All functions and the constructor/destructor must be called on the same
// sequence (the media thread).
class StarboardAudioDecoder : public StarboardDecoder,
                              public MediaPipelineBackend::AudioDecoder {
 public:
  explicit StarboardAudioDecoder(StarboardApiWrapper* starboard);
  ~StarboardAudioDecoder() override;

  // Returns the audio config or nullopt if the config has not been set yet.
  const std::optional<StarboardAudioSampleInfo>& GetAudioSampleInfo();

  // MediaPipelineBackend::AudioDecoder implementation:
  bool SetConfig(const AudioConfig& config) override;
  bool SetVolume(float multiplier) override;
  RenderingDelay GetRenderingDelay() override;
  void GetStatistics(Statistics* statistics) override;
  MediaPipelineBackend::BufferStatus PushBuffer(
      CastDecoderBuffer* buffer) override;
  void SetDelegate(Delegate* delegate) override;
  AudioTrackTimestamp GetAudioTrackTimestamp() override;
  int GetStartThresholdInFrames() override;

 private:
  // StarboardDecoder impl:
  void InitializeInternal() override;

  SEQUENCE_CHECKER(sequence_checker_);
  // If SetVolume is called before player_ is created, we need to store the
  // value and update starboard once the player is ready. If this value is not
  // nullopt, it means that there is a pending volume change, and that the
  // volume must be set on the next call to Initialize().
  std::optional<float> volume_;
  std::optional<StarboardAudioSampleInfo> audio_sample_info_;
  AudioConfig config_;
  // The number of bytes sent to the SbPlayer. Used for statistics.
  uint64_t decoded_bytes_ = 0;
  // This format is what we will be changing all PCM data to during decoding.
  StarboardPcmSampleFormat format_to_decode_to_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_MEDIA_STARBOARD_AUDIO_DECODER_H_
