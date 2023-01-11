// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_mixer.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chromecast/media/audio/cast_audio_manager.h"
#include "chromecast/media/audio/cast_audio_output_stream.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/channel_layout.h"

namespace {
const int kFramesPerBuffer = 1024;
const int kSampleRate = 48000;
}  // namespace

namespace chromecast {
namespace media {

class CastAudioMixer::MixerProxyStream
    : public ::media::AudioOutputStream,
      public ::media::AudioConverter::InputCallback {
 public:
  MixerProxyStream(const ::media::AudioParameters& input_params,
                   const ::media::AudioParameters& output_params,
                   CastAudioMixer* audio_mixer)
      : audio_mixer_(audio_mixer),
        input_params_(input_params),
        output_params_(output_params),
        opened_(false),
        volume_(1.0),
        source_callback_(nullptr) {
    DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  }

  MixerProxyStream(const MixerProxyStream&) = delete;
  MixerProxyStream& operator=(const MixerProxyStream&) = delete;

  ~MixerProxyStream() override {
    DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  }

  void OnError(ErrorType type) {
    DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
    if (source_callback_)
      source_callback_->OnError(type);
  }

 private:
  // ResamplerProxy is an intermediate filter between MixerProxyStream and
  // CastAudioMixer::output_stream_ whose only responsibility is to resample
  // audio to the sample rate expected by CastAudioMixer::output_stream_.
  class ResamplerProxy : public ::media::AudioConverter::InputCallback {
   public:
    ResamplerProxy(::media::AudioConverter::InputCallback* input_callback,
                   const ::media::AudioParameters& input_params,
                   const ::media::AudioParameters& output_params) {
      resampler_.reset(
          new ::media::AudioConverter(input_params, output_params, false));
      resampler_->AddInput(input_callback);
      DETACH_FROM_THREAD(backend_thread_checker_);
    }

    ResamplerProxy(const ResamplerProxy&) = delete;
    ResamplerProxy& operator=(const ResamplerProxy&) = delete;

    ~ResamplerProxy() override {}

   private:
    // ::media::AudioConverter::InputCallback implementation
    double ProvideInput(::media::AudioBus* audio_bus,
                        uint32_t frames_delayed,
                        const ::media::AudioGlitchInfo& glitch_info) override {
      DCHECK_CALLED_ON_VALID_THREAD(backend_thread_checker_);
      resampler_->ConvertWithInfo(frames_delayed, glitch_info, audio_bus);
      // Volume multiplier has already been applied by |resampler_|.
      return 1.0;
    }

    std::unique_ptr<::media::AudioConverter> resampler_;

    THREAD_CHECKER(backend_thread_checker_);
  };

  // ::media::AudioOutputStream implementation
  bool Open() override {
    DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);

    ::media::AudioParameters::Format format = input_params_.format();
    DCHECK((format == ::media::AudioParameters::AUDIO_PCM_LINEAR) ||
           (format == ::media::AudioParameters::AUDIO_PCM_LOW_LATENCY));

    ::media::ChannelLayout channel_layout = input_params_.channel_layout();
    if ((channel_layout != ::media::CHANNEL_LAYOUT_MONO) &&
        (channel_layout != ::media::CHANNEL_LAYOUT_STEREO)) {
      LOG(WARNING) << "Unsupported channel layout: " << channel_layout;
      return false;
    }
    DCHECK_GE(input_params_.channels(), 1);
    DCHECK_LE(input_params_.channels(), 2);

    return opened_ = audio_mixer_->Register(this);
  }

  void Close() override {
    DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);

    if (proxy_)
      Stop();
    if (opened_)
      audio_mixer_->Unregister(this);

    // Signal to the manager that we're closed and can be removed.
    // This should be the last call in the function as it deletes "this".
    audio_mixer_->audio_manager_->ReleaseOutputStream(this);
  }

  void Start(AudioSourceCallback* source_callback) override {
    DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
    DCHECK(source_callback);

    if (!opened_ || proxy_)
      return;
    source_callback_ = source_callback;
    proxy_ =
        std::make_unique<ResamplerProxy>(this, input_params_, output_params_);
    audio_mixer_->AddInput(proxy_.get());
  }

  void Stop() override {
    DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);

    if (!proxy_)
      return;
    audio_mixer_->RemoveInput(proxy_.get());
    // Once the above function returns it is guaranteed that proxy_ or
    // source_callback_ would not be used on the backend thread, so it is safe
    // to reset them.
    proxy_.reset();
    source_callback_ = nullptr;
  }

  // There is nothing to flush since the proxy stream is removed during Stop().
  void Flush() override {}

  void SetVolume(double volume) override {
    DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);

    base::AutoLock auto_lock(volume_lock_);
    volume_ = volume;
  }

  void GetVolume(double* volume) override {
    DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);

    *volume = volume_;
  }

  // ::media::AudioConverter::InputCallback implementation
  double ProvideInput(::media::AudioBus* audio_bus,
                      uint32_t frames_delayed,
                      const ::media::AudioGlitchInfo& glitch_info) override {
    // Called on backend thread. Member variables accessed from both backend
    // and audio thread must be thread-safe.
    DCHECK(source_callback_);

    const base::TimeDelta delay = ::media::AudioTimestampHelper::FramesToTime(
        frames_delayed, input_params_.sample_rate());
    source_callback_->OnMoreData(delay, base::TimeTicks::Now(), glitch_info,
                                 audio_bus);

    base::AutoLock auto_lock(volume_lock_);
    return volume_;
  }

  CastAudioMixer* const audio_mixer_;
  const ::media::AudioParameters input_params_;
  const ::media::AudioParameters output_params_;

  bool opened_;
  double volume_;
  base::Lock volume_lock_;
  AudioSourceCallback* source_callback_;
  std::unique_ptr<ResamplerProxy> proxy_;

  THREAD_CHECKER(audio_thread_checker_);
};

CastAudioMixer::CastAudioMixer(CastAudioManager* audio_manager)
    : audio_manager_(audio_manager), error_(false), output_stream_(nullptr) {
  output_params_ = ::media::AudioParameters(
      ::media::AudioParameters::Format::AUDIO_PCM_LOW_LATENCY,
      ::media::ChannelLayoutConfig::Stereo(), kSampleRate, kFramesPerBuffer);
  mixer_.reset(
      new ::media::AudioConverter(output_params_, output_params_, false));
  DETACH_FROM_THREAD(audio_thread_checker_);
}

CastAudioMixer::~CastAudioMixer() {}

::media::AudioOutputStream* CastAudioMixer::MakeStream(
    const ::media::AudioParameters& params) {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  return new MixerProxyStream(params, output_params_, this);
}

bool CastAudioMixer::Register(MixerProxyStream* proxy_stream) {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  DCHECK(!base::Contains(proxy_streams_, proxy_stream));

  // Do not allow opening new streams while in error state.
  if (error_)
    return false;

  // Initialize a backend instance if there are no output streams.
  // The stream will fail to register if the CastAudioOutputStream
  // is not opened properly.
  if (proxy_streams_.empty()) {
    DCHECK(!output_stream_);
    output_stream_ = audio_manager_->MakeMixerOutputStream(output_params_);
    if (!output_stream_->Open()) {
      output_stream_->Close();
      output_stream_ = nullptr;
      return false;
    }
  }

  proxy_streams_.insert(proxy_stream);
  return true;
}

void CastAudioMixer::Unregister(MixerProxyStream* proxy_stream) {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  DCHECK(base::Contains(proxy_streams_, proxy_stream));

  proxy_streams_.erase(proxy_stream);

  // Reset the state once all streams have been unregistered.
  if (proxy_streams_.empty()) {
    DCHECK(mixer_->empty());
    if (output_stream_)
      output_stream_->Close();
    output_stream_ = nullptr;
    error_ = false;
  }
}

void CastAudioMixer::AddInput(
    ::media::AudioConverter::InputCallback* input_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);

  // Start the backend if there are no current inputs.
  if (mixer_->empty() && output_stream_)
    output_stream_->Start(this);

  base::AutoLock auto_lock(mixer_lock_);
  mixer_->AddInput(input_callback);
}

void CastAudioMixer::RemoveInput(
    ::media::AudioConverter::InputCallback* input_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);

  {
    base::AutoLock auto_lock(mixer_lock_);
    mixer_->RemoveInput(input_callback);
  }

  // Stop |output_stream_| if there are no inputs and the stream is running.
  if (mixer_->empty() && output_stream_)
    output_stream_->Stop();
}

int CastAudioMixer::OnMoreData(base::TimeDelta delay,
                               base::TimeTicks /* delay_timestamp */,
                               const ::media::AudioGlitchInfo& glitch_info,
                               ::media::AudioBus* dest) {
  // Called on backend thread.
  uint32_t frames_delayed = ::media::AudioTimestampHelper::TimeToFrames(
      delay, output_params_.sample_rate());

  base::AutoLock auto_lock(mixer_lock_);
  mixer_->ConvertWithInfo(frames_delayed, glitch_info, dest);
  return dest->frames();
}

void CastAudioMixer::OnError(ErrorType type) {
  // Called on backend thread.
  audio_manager_->GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&CastAudioMixer::HandleError,
                                base::Unretained(this), type));
}

void CastAudioMixer::HandleError(ErrorType type) {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);

  error_ = true;
  for (auto it = proxy_streams_.begin(); it != proxy_streams_.end(); ++it)
    (*it)->OnError(type);
}

}  // namespace media
}  // namespace chromecast
