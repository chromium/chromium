// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ALSA_MIXER_OUTPUT_STREAM_ALSA_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ALSA_MIXER_OUTPUT_STREAM_ALSA_H_

#include <alsa/asoundlib.h>

#include <cstdint>
#include <vector>

#include "chromecast/public/media/mixer_output_stream.h"

namespace chromecast {
namespace media {

class AlsaWrapper;

// MixerOutputStream implementation for ALSA
class MixerOutputStreamAlsa : public MixerOutputStream {
 public:
  MixerOutputStreamAlsa();

  MixerOutputStreamAlsa(const MixerOutputStreamAlsa&) = delete;
  MixerOutputStreamAlsa& operator=(const MixerOutputStreamAlsa&) = delete;

  ~MixerOutputStreamAlsa() override;

  void SetAlsaWrapperForTest(std::unique_ptr<AlsaWrapper> alsa);

  // MixerOutputStream implementation:
  bool Start(int requested_sample_rate, int channels) override;
  int GetNumChannels() override;
  int GetSampleRate() override;
  MediaPipelineBackend::AudioDecoder::RenderingDelay GetRenderingDelay()
      override;
  int OptimalWriteFramesCount() override;
  bool Write(const float* data,
             int data_size,
             bool* out_playback_interrupted) override;
  void Stop() override;

 private:
  // Reads the buffer size, period size, start threshold, and avail min value
  // from the provided command line flags or uses default values if no flags are
  // provided.
  void DefineAlsaParameters();

  // Takes the provided ALSA config and sets all ALSA output hardware/software
  // playback parameters.  It will try to select sane fallback parameters based
  // on what the output hardware supports and will log warnings if it does so.
  // If any ALSA function returns an unexpected error code, the error code will
  // be returned by this function. Otherwise, it will return 0.
  int SetAlsaPlaybackParams(int requested_rate);

  // Determines output sample rate based on the requested rate and the sample
  // rate the device supports.
  int DetermineOutputRate(int requested_rate);

  void UpdateRenderingDelay();

  // Checks ALSA output for current state and if it's suspended, tries to
  // recover.
  // Returns true if ALSA device is recovered successfully.
  bool MaybeRecoverDeviceFromSuspendedState();

  std::unique_ptr<AlsaWrapper> alsa_;

  snd_pcm_t* pcm_ = nullptr;
  snd_pcm_hw_params_t* pcm_hw_params_ = nullptr;
  snd_pcm_status_t* pcm_status_ = nullptr;
  snd_pcm_format_t pcm_format_ = SND_PCM_FORMAT_UNKNOWN;

  int num_output_channels_ = 0;
  int sample_rate_ = kInvalidSampleRate;

  // User-configurable ALSA parameters. This caches the results, so the code
  // only has to interact with the command line parameters once.
  snd_pcm_uframes_t alsa_buffer_size_ = 0;
  snd_pcm_uframes_t alsa_period_size_ = 0;
  snd_pcm_uframes_t alsa_start_threshold_ = 0;
  snd_pcm_uframes_t alsa_avail_min_ = 0;

  bool first_write_ = false;
  MediaPipelineBackend::AudioDecoder::RenderingDelay rendering_delay_;

  std::vector<uint8_t> output_buffer_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ALSA_MIXER_OUTPUT_STREAM_ALSA_H_
