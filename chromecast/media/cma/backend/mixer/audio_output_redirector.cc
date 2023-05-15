// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/audio_output_redirector.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/pattern.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/media/audio/audio_fader.h"
#include "chromecast/media/audio/audio_log.h"
#include "chromecast/media/audio/mixer_service/mixer_service_transport.pb.h"
#include "chromecast/media/audio/mixer_service/mixer_socket.h"
#include "chromecast/media/audio/net/conversions.h"
#include "chromecast/media/cma/backend/mixer/audio_output_redirector_input.h"
#include "chromecast/media/cma/backend/mixer/channel_layout.h"
#include "chromecast/media/cma/backend/mixer/mixer_input.h"
#include "chromecast/media/cma/backend/mixer/stream_mixer.h"
#include "chromecast/media/cma/base/decoder_config_adapter.h"
#include "media/base/audio_bus.h"
#include "media/base/channel_layout.h"
#include "media/base/channel_mixer.h"

namespace chromecast {
namespace media {

namespace {

using Patterns = std::vector<std::pair<AudioContentType, std::string>>;

constexpr int kDefaultBufferSize = 2048;
constexpr int kMaxChannels = 32;

constexpr int kAudioMessageHeaderSize =
    mixer_service::MixerSocket::kAudioMessageHeaderSize;

::media::ChannelLayout GetMediaChannelLayout(
    chromecast::media::ChannelLayout layout,
    int num_channels) {
  if (layout == media::ChannelLayout::UNSUPPORTED) {
    return mixer::GuessChannelLayout(num_channels);
  }
  return DecoderConfigAdapter::ToMediaChannelLayout(layout);
}

enum MessageTypes : int {
  kStreamConfig = 1,
};

}  // namespace

class AudioOutputRedirector::RedirectionConnection
    : public mixer_service::MixerSocket::Delegate {
 public:
  explicit RedirectionConnection(
      std::unique_ptr<mixer_service::MixerSocket> socket,
      scoped_refptr<base::TaskRunner> mixer_task_runner,
      base::WeakPtr<AudioOutputRedirector> redirector)
      : socket_(std::move(socket)),
        mixer_task_runner_(std::move(mixer_task_runner)),
        redirector_(std::move(redirector)) {
    DCHECK(socket_);
    DCHECK(mixer_task_runner_);

    socket_->SetDelegate(this);
  }

  RedirectionConnection(const RedirectionConnection&) = delete;
  RedirectionConnection& operator=(const RedirectionConnection&) = delete;

  ~RedirectionConnection() override = default;

  void SetStreamConfig(SampleFormat sample_format,
                       int sample_rate,
                       int num_channels,
                       int data_size) {
    mixer_service::Generic message;
    mixer_service::StreamConfig* config = message.mutable_stream_config();
    config->set_sample_format(
        audio_service::ConvertSampleFormat(sample_format));
    config->set_sample_rate(sample_rate);
    config->set_num_channels(num_channels);
    config->set_data_size(data_size);
    socket_->SendProto(kStreamConfig, message);

    sent_stream_config_ = true;
  }

  void SendAudio(scoped_refptr<net::IOBuffer> audio_buffer,
                 int data_size_bytes,
                 int64_t timestamp) {
    if (error_) {
      return;
    }
    DCHECK(sent_stream_config_);
    socket_->SendAudioBuffer(std::move(audio_buffer), data_size_bytes,
                             timestamp);
  }

  // mixer_service::MixerSocket::Delegate implementation:
  bool HandleMetadata(const mixer_service::Generic& message) override {
    if (!message.has_redirected_stream_patterns()) {
      return true;
    }

    Patterns new_patterns;
    for (const auto& p : message.redirected_stream_patterns().patterns()) {
      new_patterns.emplace_back(
          audio_service::ConvertContentType(p.content_type()),
          p.device_id_pattern());
    }
    mixer_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AudioOutputRedirector::UpdatePatterns,
                                  redirector_, std::move(new_patterns)));
    return true;
  }

 private:
  bool HandleAudioData(char* data, size_t size, int64_t timestamp) override {
    return true;
  }

  void OnConnectionError() override {
    if (error_) {
      return;
    }
    error_ = true;
    mixer_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AudioOutputRedirector::OnConnectionError, redirector_));
  }

  const std::unique_ptr<mixer_service::MixerSocket> socket_;
  const scoped_refptr<base::TaskRunner> mixer_task_runner_;
  const base::WeakPtr<AudioOutputRedirector> redirector_;

  bool error_ = false;
  bool sent_stream_config_ = false;
};

class AudioOutputRedirector::InputImpl : public AudioOutputRedirectorInput {
 public:
  using RenderingDelay = MediaPipelineBackend::AudioDecoder::RenderingDelay;

  InputImpl(AudioOutputRedirector* output_redirector, MixerInput* mixer_input);

  InputImpl(const InputImpl&) = delete;
  InputImpl& operator=(const InputImpl&) = delete;

  ~InputImpl() override;

  // AudioOutputRedirectorInput implementation:
  int Order() override { return output_redirector_->order(); }
  int64_t GetDelayMicroseconds() override {
    return output_redirector_->extra_delay_microseconds();
  }
  void Redirect(::media::AudioBus* const buffer,
                int num_frames,
                RenderingDelay rendering_delay,
                bool redirected) override;

 private:
  AudioOutputRedirector* const output_redirector_;
  MixerInput* const mixer_input_;
  const ::media::ChannelLayout output_channel_layout_;
  const int num_output_channels_;

  bool previous_ended_in_silence_;

  std::unique_ptr<::media::ChannelMixer> channel_mixer_;
  std::unique_ptr<::media::AudioBus> temp_buffer_;
};

AudioOutputRedirector::InputImpl::InputImpl(
    AudioOutputRedirector* output_redirector,
    MixerInput* mixer_input)
    : output_redirector_(output_redirector),
      mixer_input_(mixer_input),
      output_channel_layout_(output_redirector_->output_channel_layout()),
      num_output_channels_(output_redirector_->num_output_channels()),
      previous_ended_in_silence_(true) {
  DCHECK_LE(num_output_channels_, kMaxChannels);
  DCHECK(output_redirector_);
  DCHECK(mixer_input_);

  if (mixer_input_->num_channels() != num_output_channels_) {
    AUDIO_LOG(INFO) << "Remixing channels for " << mixer_input_->source()
                    << " from " << mixer_input_->num_channels() << " to "
                    << num_output_channels_;
    channel_mixer_ = std::make_unique<::media::ChannelMixer>(
        mixer::CreateAudioParametersForChannelMixer(
            mixer_input_->channel_layout(), mixer_input_->num_channels()),
        mixer::CreateAudioParametersForChannelMixer(output_channel_layout_,
                                                    num_output_channels_));
  }

  temp_buffer_ =
      ::media::AudioBus::Create(num_output_channels_, kDefaultBufferSize);
  temp_buffer_->Zero();

  mixer_input_->AddAudioOutputRedirector(this);
}

AudioOutputRedirector::InputImpl::~InputImpl() {
  mixer_input_->RemoveAudioOutputRedirector(this);
}

void AudioOutputRedirector::InputImpl::Redirect(::media::AudioBus* const buffer,
                                                int num_frames,
                                                RenderingDelay rendering_delay,
                                                bool redirected) {
  if (num_frames == 0) {
    return;
  }

  if (previous_ended_in_silence_ && redirected) {
    // Previous buffer ended in silence, and the current buffer was redirected
    // by a previous output splitter, so maintain silence.
    return;
  }
  if (!previous_ended_in_silence_ && !redirected && !channel_mixer_) {
    // No fading or channel mixing required, just mix directly.
    output_redirector_->MixInput(mixer_input_, buffer, num_frames,
                                 rendering_delay);
    return;
  }

  if (temp_buffer_->frames() < num_frames) {
    temp_buffer_ = ::media::AudioBus::Create(
        num_output_channels_, std::max(num_frames, kDefaultBufferSize));
  }

  if (channel_mixer_) {
    channel_mixer_->TransformPartial(buffer, num_frames, temp_buffer_.get());
  } else {
    buffer->CopyPartialFramesTo(0, num_frames, 0, temp_buffer_.get());
  }

  float* channels[kMaxChannels];
  for (int c = 0; c < num_output_channels_; ++c) {
    channels[c] = temp_buffer_->channel(c);
  }
  if (previous_ended_in_silence_) {
    if (!redirected) {
      // Smoothly fade in from previous silence.
      AudioFader::FadeInHelper(channels, num_output_channels_, num_frames,
                               num_frames, num_frames);
    }
  } else if (redirected) {
    // Smoothly fade out to silence, since output is now being redirected by a
    // previous output splitter.
    AudioFader::FadeOutHelper(channels, num_output_channels_, num_frames,
                              num_frames, num_frames);
  }
  previous_ended_in_silence_ = redirected;

  output_redirector_->MixInput(mixer_input_, temp_buffer_.get(), num_frames,
                               rendering_delay);
}

// static
AudioOutputRedirector::Config AudioOutputRedirector::ParseConfig(
    const mixer_service::Generic& message) {
  Config config;
  DCHECK(message.has_redirection_request());
  const mixer_service::RedirectionRequest& request =
      message.redirection_request();
  if (request.has_num_channels()) {
    config.num_output_channels = request.num_channels();
  }
  if (request.has_channel_layout() &&
      request.channel_layout() != audio_service::CHANNEL_LAYOUT_BITSTREAM) {
    config.output_channel_layout =
        audio_service::ConvertChannelLayout(request.channel_layout());
  } else {
    config.output_channel_layout = media::ChannelLayout::UNSUPPORTED;
  }
  if (request.has_order()) {
    config.order = request.order();
  }
  if (request.has_apply_volume()) {
    config.apply_volume = request.apply_volume();
  }
  if (request.has_extra_delay_microseconds()) {
    config.extra_delay_microseconds = request.extra_delay_microseconds();
  }
  return config;
}

AudioOutputRedirector::AudioOutputRedirector(
    StreamMixer* mixer,
    std::unique_ptr<mixer_service::MixerSocket> socket,
    const mixer_service::Generic& message)
    : mixer_(mixer),
      config_(ParseConfig(message)),
      output_channel_layout_(
          GetMediaChannelLayout(config_.output_channel_layout,
                                config_.num_output_channels)),
      io_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      buffer_pool_(
          base::MakeRefCounted<IOBufferPool>(kDefaultBufferSize,
                                             std::numeric_limits<size_t>::max(),
                                             true /* threadsafe */)),
      weak_factory_(this) {
  DCHECK_GT(config_.num_output_channels, 0);

  output_ = std::make_unique<RedirectionConnection>(
      std::move(socket), mixer_->task_runner(), weak_factory_.GetWeakPtr());
  output_->HandleMetadata(message);
}

AudioOutputRedirector::~AudioOutputRedirector() {
  io_task_runner_->DeleteSoon(FROM_HERE, std::move(output_));
}

void AudioOutputRedirector::AddInput(MixerInput* mixer_input) {
  if (ApplyToInput(mixer_input)) {
    DCHECK_EQ(mixer_input->output_samples_per_second(), sample_rate_);
    inputs_[mixer_input] = std::make_unique<InputImpl>(this, mixer_input);
  } else {
    non_redirected_inputs_.insert(mixer_input);
  }
}

void AudioOutputRedirector::RemoveInput(MixerInput* mixer_input) {
  inputs_.erase(mixer_input);
  non_redirected_inputs_.erase(mixer_input);
}

bool AudioOutputRedirector::ApplyToInput(MixerInput* mixer_input) {
  if (!mixer_input->primary()) {
    return false;
  }

  for (const auto& pattern : patterns_) {
    if (mixer_input->content_type() == pattern.first &&
        base::MatchPattern(mixer_input->device_id(), pattern.second)) {
      return true;
    }
  }

  return false;
}

void AudioOutputRedirector::UpdatePatterns(
    std::vector<std::pair<AudioContentType, std::string>> patterns) {
  patterns_ = std::move(patterns);
  // Remove streams that no longer match.
  for (auto it = inputs_.begin(); it != inputs_.end();) {
    MixerInput* mixer_input = it->first;
    if (!ApplyToInput(mixer_input)) {
      non_redirected_inputs_.insert(mixer_input);
      it = inputs_.erase(it);
    } else {
      ++it;
    }
  }

  // Add streams that previously didn't match.
  for (auto it = non_redirected_inputs_.begin();
       it != non_redirected_inputs_.end();) {
    MixerInput* mixer_input = *it;
    if (ApplyToInput(mixer_input)) {
      inputs_[mixer_input] = std::make_unique<InputImpl>(this, mixer_input);
      it = non_redirected_inputs_.erase(it);
    } else {
      ++it;
    }
  }
}

void AudioOutputRedirector::OnConnectionError() {
  mixer_->RemoveAudioOutputRedirector(this);
}

void AudioOutputRedirector::SetSampleRate(int output_samples_per_second) {
  sample_rate_ = output_samples_per_second;

  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RedirectionConnection::SetStreamConfig,
                     base::Unretained(output_.get()), kSampleFormatPlanarF32,
                     sample_rate_, config_.num_output_channels,
                     buffer_pool_->buffer_size() - kAudioMessageHeaderSize));
}

void AudioOutputRedirector::PrepareNextBuffer(int num_frames) {
  size_t required_size_bytes =
      kAudioMessageHeaderSize +
      num_frames * config_.num_output_channels * sizeof(float);
  if (buffer_pool_->buffer_size() < required_size_bytes) {
    buffer_pool_ = base::MakeRefCounted<IOBufferPool>(
        required_size_bytes, std::numeric_limits<size_t>::max(),
        true /* threadsafe */);
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &RedirectionConnection::SetStreamConfig,
            base::Unretained(output_.get()), kSampleFormatPlanarF32,
            sample_rate_, config_.num_output_channels,
            num_frames * config_.num_output_channels * sizeof(float)));
  }

  current_mix_buffer_ = buffer_pool_->GetBuffer();
  current_mix_data_ = reinterpret_cast<float*>(current_mix_buffer_->data() +
                                               kAudioMessageHeaderSize);
  std::fill_n(current_mix_data_, num_frames * config_.num_output_channels,
              0.0f);
  next_output_timestamp_ = INT64_MIN;
  next_num_frames_ = num_frames;
  input_count_ = 0;
}

void AudioOutputRedirector::MixInput(MixerInput* mixer_input,
                                     ::media::AudioBus* data,
                                     int num_frames,
                                     RenderingDelay rendering_delay) {
  DCHECK(current_mix_data_);
  DCHECK_GE(next_num_frames_, num_frames);
  DCHECK_EQ(config_.num_output_channels, data->channels());

  if (rendering_delay.timestamp_microseconds != INT64_MIN) {
    int64_t output_timestamp = rendering_delay.timestamp_microseconds +
                               rendering_delay.delay_microseconds +
                               extra_delay_microseconds();
    if (next_output_timestamp_ == INT64_MIN ||
        output_timestamp < next_output_timestamp_) {
      next_output_timestamp_ = output_timestamp;
    }
  }

  if (num_frames <= 0) {
    return;
  }

  ++input_count_;
  for (int c = 0; c < config_.num_output_channels; ++c) {
    float* dest_channel = current_mix_data_ + c * next_num_frames_;
    if (config_.apply_volume) {
      mixer_input->VolumeScaleAccumulate(data->channel(c), num_frames,
                                         dest_channel, c);
    } else {
      const float* temp_channel = data->channel(c);
      for (int i = 0; i < num_frames; ++i) {
        dest_channel[i] += temp_channel[i];
      }
    }
  }
}

void AudioOutputRedirector::FinishBuffer() {
  if (input_count_ == 0) {
    return;
  }

  // Hard limit to [1.0, -1.0].
  for (int s = 0; s < config_.num_output_channels * next_num_frames_; ++s) {
    current_mix_data_[s] = std::clamp(current_mix_data_[s], -1.0f, 1.0f);
  }

  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &RedirectionConnection::SendAudio, base::Unretained(output_.get()),
          std::move(current_mix_buffer_),
          config_.num_output_channels * next_num_frames_ * sizeof(float),
          next_output_timestamp_));
}

}  // namespace media
}  // namespace chromecast
