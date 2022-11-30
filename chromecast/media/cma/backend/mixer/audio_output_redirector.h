// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_AUDIO_OUTPUT_REDIRECTOR_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_AUDIO_OUTPUT_REDIRECTOR_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/media/audio/mixer_service/redirected_audio_connection.h"
#include "chromecast/net/io_buffer_pool.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/volume_control.h"
#include "media/base/channel_layout.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace media {
class AudioBus;
}  // namespace media

namespace chromecast {
namespace media {
class MixerInput;
class StreamMixer;

namespace mixer_service {
class Generic;
class MixerSocket;
}  // namespace mixer_service

// The AudioOutputRedirector class determines which MixerInputs match the config
// conditions, and adds an AudioOutputRedirectorInput to each one.
// When the mixer is writing output, it tells each AudioOutputRedirector to
// create a new buffer (by calling PrepareNextBuffer()). As audio is pulled from
// each MixerInput, the resulting audio is passed through any added
// AudioOutputRedirectorInputs and is mixed into the redirected buffer by the
// AudioOutputRedirector (with appropriate fading to avoid pops/clicks, and
// volume applied if desired). Once all MixerInputs have been processed, the
// mixer will call FinishBuffer() on each AudioOutputRedirector; the
// redirected buffer is then sent to the RedirectedAudioOutput associated
// with each redirector.
// Created on the IO thread, but otherwise runs on the mixer thread.
class AudioOutputRedirector {
 public:
  using RenderingDelay = MediaPipelineBackend::AudioDecoder::RenderingDelay;

  AudioOutputRedirector(StreamMixer* mixer,
                        std::unique_ptr<mixer_service::MixerSocket> socket,
                        const mixer_service::Generic& message);

  AudioOutputRedirector(const AudioOutputRedirector&) = delete;
  AudioOutputRedirector& operator=(const AudioOutputRedirector&) = delete;

  ~AudioOutputRedirector();

  int order() const { return config_.order; }
  int num_output_channels() const { return config_.num_output_channels; }
  ::media::ChannelLayout output_channel_layout() const {
    return output_channel_layout_;
  }

  int64_t extra_delay_microseconds() const {
    return config_.extra_delay_microseconds;
  }

  // Adds/removes mixer inputs. The mixer adds/removes all mixer inputs from
  // the AudioOutputRedirector; this class does the work to determine which
  // inputs match the desired conditions.
  void AddInput(MixerInput* mixer_input);
  void RemoveInput(MixerInput* mixer_input);

  // Sets the sample rate for subsequent audio from inputs.
  void SetSampleRate(int output_samples_per_second);

  // Called by the mixer when it is preparing to write another buffer of
  // |num_frames| frames.
  void PrepareNextBuffer(int num_frames);

  // Mixes audio into the redirected output buffer. Called by the
  // AudioOutputRedirectorInput implementation to mix in audio from each
  // matching MixerInput.
  void MixInput(MixerInput* mixer_input,
                ::media::AudioBus* data,
                int num_frames,
                RenderingDelay rendering_delay);

  // Called by the mixer once all MixerInputs have been processed; passes the
  // redirected audio buffer to the output plugin.
  void FinishBuffer();

 private:
  class InputImpl;
  class RedirectionConnection;

  using Config = mixer_service::RedirectedAudioConnection::Config;

  static Config ParseConfig(const mixer_service::Generic& message);

  // Updates the set of patterns used to determine which inputs should be
  // redirected by this AudioOutputRedirector. Any inputs which no longer match
  // will stop being redirected.
  void UpdatePatterns(
      std::vector<std::pair<AudioContentType, std::string>> patterns);
  void OnConnectionError();

  bool ApplyToInput(MixerInput* mixer_input);

  StreamMixer* const mixer_;
  const Config config_;
  const ::media::ChannelLayout output_channel_layout_;
  std::unique_ptr<RedirectionConnection> output_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  int sample_rate_ = 0;

  std::vector<std::pair<AudioContentType, std::string>> patterns_;

  int next_num_frames_ = 0;
  int64_t next_output_timestamp_ = INT64_MIN;
  int input_count_ = 0;

  scoped_refptr<IOBufferPool> buffer_pool_;
  scoped_refptr<::net::IOBuffer> current_mix_buffer_;
  float* current_mix_data_ = nullptr;

  base::flat_map<MixerInput*, std::unique_ptr<InputImpl>> inputs_;
  base::flat_set<MixerInput*> non_redirected_inputs_;

  base::WeakPtrFactory<AudioOutputRedirector> weak_factory_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_AUDIO_OUTPUT_REDIRECTOR_H_
