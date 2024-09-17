// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/public/cpp/sounds/audio_stream_handler.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/cancelable_callback.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "media/audio/audio_handler.h"
#include "media/audio/flac_audio_handler.h"
#include "media/audio/wav_audio_handler.h"
#include "media/base/audio_codecs.h"
#include "media/base/channel_layout.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "services/audio/public/cpp/output_device.h"

namespace audio {

namespace {

// Volume percent.
const double kOutputVolumePercent = 0.8;

// Keep alive timeout for audio stream.
const int kKeepAliveMs = 1500;

AudioStreamHandler::TestObserver* g_observer_for_testing = nullptr;

}  // namespace

class AudioStreamHandler::AudioStreamContainer
    : public media::AudioRendererSink::RenderCallback {
 public:
  AudioStreamContainer(SoundsManager::StreamFactoryBinder stream_factory_binder,
                       std::unique_ptr<media::AudioHandler> audio_handler)
      : stream_factory_binder_(std::move(stream_factory_binder)),
        audio_handler_(std::move(audio_handler)) {
    DCHECK(audio_handler_);
    task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  }

  AudioStreamContainer(const AudioStreamContainer&) = delete;
  AudioStreamContainer& operator=(const AudioStreamContainer&) = delete;

  ~AudioStreamContainer() override {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
  }

  void Play() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    // Create OutputDevice if it is the first time playing.
    if (device_ == nullptr) {
      const media::AudioParameters params(
          media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
          media::ChannelLayoutConfig::Guess(audio_handler_->GetNumChannels()),
          audio_handler_->GetSampleRate(),
          media::AudioHandler::kDefaultFrameCount);
      if (g_observer_for_testing) {
        g_observer_for_testing->Initialize(this, params);
      } else {
        mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory;
        stream_factory_binder_.Run(
            stream_factory.InitWithNewPipeAndPassReceiver());
        device_ = std::make_unique<audio::OutputDevice>(
            std::move(stream_factory), params, this, std::string());
      }
    }

    {
      base::AutoLock al(state_lock_);

      delayed_stop_posted_ = false;
      stop_closure_.Reset(base::BindRepeating(&AudioStreamContainer::Stop,
                                              base::Unretained(this)));

      if (started_) {
        if (audio_handler_->AtEnd()) {
          audio_handler_->Reset();
        }
        return;
      } else {
        if (!g_observer_for_testing) {
          device_->SetVolume(kOutputVolumePercent);
        }
      }

      audio_handler_->Reset();
    }

    started_ = true;
    if (g_observer_for_testing) {
      g_observer_for_testing->OnPlay();
    } else {
      device_->Play();
    }
  }

  void Stop() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    if (started_) {
      // Do not hold the |state_lock_| while stopping the output stream.
      if (g_observer_for_testing) {
        g_observer_for_testing->OnStop();
      } else {
        device_->Pause();
      }
    }

    started_ = false;
    stop_closure_.Cancel();
    device_.reset();
  }

 private:
  // media::AudioRendererSink::RenderCallback overrides:
  // Following methods could be called from *ANY* thread.
  int Render(base::TimeDelta /* delay */,
             base::TimeTicks /* delay_timestamp */,
             const media::AudioGlitchInfo& /* glitch_info */,
             media::AudioBus* dest) override {
    base::AutoLock al(state_lock_);
    size_t frames_written = 0;
    if (audio_handler_->AtEnd() ||
        !audio_handler_->CopyTo(dest, &frames_written)) {
      if (delayed_stop_posted_) {
        return 0;
      }
      delayed_stop_posted_ = true;
      task_runner_->PostDelayedTask(FROM_HERE, stop_closure_.callback(),
                                    base::Milliseconds(kKeepAliveMs));
      return 0;
    }
    return dest->frames();
  }

  void OnRenderError() override {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&AudioStreamContainer::Stop,
                                          weak_factory_.GetWeakPtr()));
  }

  bool started_ = false;
  const SoundsManager::StreamFactoryBinder stream_factory_binder_;
  std::unique_ptr<audio::OutputDevice> device_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // All variables below must be accessed under |state_lock_| when |started_|.
  base::Lock state_lock_;
  bool delayed_stop_posted_ = false;
  std::unique_ptr<media::AudioHandler> audio_handler_;
  base::CancelableRepeatingClosure stop_closure_;

  base::WeakPtrFactory<AudioStreamHandler::AudioStreamContainer> weak_factory_{
      this};
};

AudioStreamHandler::AudioStreamHandler(
    SoundsManager::StreamFactoryBinder stream_factory_binder,
    std::string_view audio_data,
    media::AudioCodec codec) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<media::AudioHandler> audio_handler;
  switch (codec) {
    case media::AudioCodec::kPCM: {
      audio_handler = media::WavAudioHandler::Create(audio_data);
      if (!audio_handler || !audio_handler->Initialize()) {
        LOG(ERROR) << "wav_data is not valid";
        return;
      }
      break;
    }
    case media::AudioCodec::kFLAC: {
      auto tmp_handler = std::make_unique<media::FlacAudioHandler>(audio_data);
      if (!tmp_handler->Initialize()) {
        LOG(ERROR) << "flac_data is not valid";
        return;
      }
      audio_handler = std::move(tmp_handler);
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION() << "Unsupported audio codec encountered: "
                                << media::GetCodecName(codec);
      break;
  }

  // Check params.
  const media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Guess(audio_handler->GetNumChannels()),
      audio_handler->GetSampleRate(), media::AudioHandler::kDefaultFrameCount);
  if (!params.IsValid()) {
    LOG(ERROR) << "Audio params are invalid.";
    return;
  }

  duration_ = audio_handler->GetDuration();
  stream_ = base::SequenceBound<AudioStreamContainer>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::WithBaseSyncPrimitives(),
           base::TaskPriority::USER_VISIBLE}),
      std::move(stream_factory_binder), std::move(audio_handler));
}

AudioStreamHandler::~AudioStreamHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsInitialized()) {
    stream_.AsyncCall(&AudioStreamContainer::Stop);
  }
}

bool AudioStreamHandler::IsInitialized() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !!stream_;
}

bool AudioStreamHandler::Play() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsInitialized()) {
    return false;
  }

  stream_.AsyncCall(&AudioStreamContainer::Play);
  return true;
}

void AudioStreamHandler::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsInitialized()) {
    return;
  }

  stream_.AsyncCall(&AudioStreamContainer::Stop);
}

base::TimeDelta AudioStreamHandler::duration() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsInitialized());
  return duration_;
}

// static
void AudioStreamHandler::SetObserverForTesting(TestObserver* observer) {
  g_observer_for_testing = observer;
}

}  // namespace audio
