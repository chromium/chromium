// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/audio_output_provider_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/assistant/platform/audio_stream_handler.h"
#include "chromeos/services/assistant/public/mojom/assistant_audio_decoder.mojom.h"
#include "chromeos/services/libassistant/public/mojom/platform_delegate.mojom.h"
#include "libassistant/shared/public/platform_audio_buffer.h"
#include "media/audio/audio_device_description.h"

namespace chromeos {
namespace assistant {

namespace {

bool IsEncodedFormat(const assistant_client::OutputStreamFormat& format) {
  return format.encoding ==
             assistant_client::OutputStreamEncoding::STREAM_MP3 ||
         format.encoding ==
             assistant_client::OutputStreamEncoding::STREAM_OPUS_IN_OGG;
}

class AudioOutputImpl : public assistant_client::AudioOutput {
 public:
  AudioOutputImpl(
      mojo::PendingRemote<audio::mojom::StreamFactory> stream_factory,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      mojom::AssistantAudioDecoderFactory* audio_decoder_factory,
      chromeos::libassistant::mojom::AudioOutputDelegate* audio_output_delegate,
      assistant_client::OutputStreamType type,
      assistant_client::OutputStreamFormat format,
      const std::string& device_id)
      : stream_factory_(std::move(stream_factory)),
        main_task_runner_(task_runner),
        background_thread_task_runner_(background_task_runner),
        audio_decoder_factory_(audio_decoder_factory),
        audio_output_delegate_(audio_output_delegate),
        stream_type_(type),
        format_(format),
        audio_stream_handler_(
            std::make_unique<AudioStreamHandler>(task_runner)),
        device_owner_(std::make_unique<AudioDeviceOwner>(task_runner,
                                                         background_task_runner,
                                                         device_id)) {}

  ~AudioOutputImpl() override {
    // This ensures that it will be executed after StartOnMainThread.
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](std::unique_ptr<AudioDeviceOwner> device_owner,
               scoped_refptr<base::SequencedTaskRunner> background_runner) {
              // Ensures |device_owner| is destructed on the correct thread.
              background_runner->DeleteSoon(FROM_HERE, device_owner.release());
            },
            std::move(device_owner_), background_thread_task_runner_));
    main_task_runner_->DeleteSoon(FROM_HERE, audio_stream_handler_.release());
  }

  // assistant_client::AudioOutput overrides:
  assistant_client::OutputStreamType GetType() override { return stream_type_; }

  void Start(assistant_client::AudioOutput::Delegate* delegate) override {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AudioOutputImpl::StartOnMainThread,
                                  weak_ptr_factory_.GetWeakPtr(), delegate));
  }

  void Stop() override {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AudioOutputImpl::StopOnMainThread,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void StartOnMainThread(assistant_client::AudioOutput::Delegate* delegate) {
    // TODO(llin): Remove getting audio focus here after libassistant handles
    // acquiring audio focus for the internal media player.
    if (stream_type_ == assistant_client::OutputStreamType::STREAM_MEDIA) {
      audio_output_delegate_->RequestAudioFocus(
          chromeos::libassistant::mojom::AudioOutputStreamType::kMediaStream);
    }

    if (IsEncodedFormat(format_)) {
      audio_stream_handler_->StartAudioDecoder(
          audio_decoder_factory_, delegate,
          base::BindOnce(&AudioDeviceOwner::StartOnMainThread,
                         base::Unretained(device_owner_.get()),
                         audio_output_delegate_, audio_stream_handler_.get(),
                         std::move(stream_factory_)));
    } else {
      device_owner_->StartOnMainThread(audio_output_delegate_, delegate,
                                       std::move(stream_factory_), format_);
    }
  }

  void StopOnMainThread() {
    // TODO(llin): Remove abandoning audio focus here after libassistant handles
    // abandoning audio focus for the internal media player.
    if (stream_type_ == assistant_client::OutputStreamType::STREAM_MEDIA) {
      audio_output_delegate_->AbandonAudioFocusIfNeeded();
    }

    if (IsEncodedFormat(format_)) {
      device_owner_->SetDelegate(nullptr);
      audio_stream_handler_->OnStopped();
    } else {
      background_thread_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&AudioDeviceOwner::StopOnBackgroundThread,
                                    base::Unretained(device_owner_.get())));
    }
  }

  mojo::PendingRemote<audio::mojom::StreamFactory> stream_factory_;
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> background_thread_task_runner_;
  mojom::AssistantAudioDecoderFactory* audio_decoder_factory_;
  chromeos::libassistant::mojom::AudioOutputDelegate* const
      audio_output_delegate_;

  const assistant_client::OutputStreamType stream_type_;
  assistant_client::OutputStreamFormat format_;

  std::unique_ptr<AudioStreamHandler> audio_stream_handler_;
  std::unique_ptr<AudioDeviceOwner> device_owner_;

  base::WeakPtrFactory<AudioOutputImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AudioOutputImpl);
};

}  // namespace

AudioOutputProviderImpl::AudioOutputProviderImpl(
    mojo::PendingRemote<chromeos::libassistant::mojom::AudioOutputDelegate>
        audio_output_delegate,
    chromeos::libassistant::mojom::PlatformDelegate* platform_delegate,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const std::string& device_id)
    : platform_delegate_(platform_delegate),
      audio_output_delegate_(std::move(audio_output_delegate)),
      loop_back_input_(platform_delegate_,
                       media::AudioDeviceDescription::kLoopbackInputDeviceId),
      volume_control_impl_(audio_output_delegate_.get(), platform_delegate),
      main_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      background_task_runner_(background_task_runner),
      device_id_(device_id) {
  platform_delegate_->BindAudioDecoderFactory(
      audio_decoder_factory_.BindNewPipeAndPassReceiver());
}

AudioOutputProviderImpl::~AudioOutputProviderImpl() = default;

assistant_client::AudioOutput* AudioOutputProviderImpl::CreateAudioOutput(
    assistant_client::OutputStreamType type,
    const assistant_client::OutputStreamFormat& stream_format) {
  mojo::PendingRemote<audio::mojom::StreamFactory> stream_factory;
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioOutputProviderImpl::BindStreamFactory,
                     weak_ptr_factory_.GetWeakPtr(),
                     stream_factory.InitWithNewPipeAndPassReceiver()));
  // Owned by one arbitrary thread inside libassistant. It will be destroyed
  // once assistant_client::AudioOutput::Delegate::OnStopped() is called.
  return new AudioOutputImpl(
      std::move(stream_factory), main_task_runner_, background_task_runner_,
      audio_decoder_factory_.get(), audio_output_delegate_.get(), type,
      stream_format, device_id_);
}

std::vector<assistant_client::OutputStreamEncoding>
AudioOutputProviderImpl::GetSupportedStreamEncodings() {
  return std::vector<assistant_client::OutputStreamEncoding>{
      assistant_client::OutputStreamEncoding::STREAM_PCM_S16,
      assistant_client::OutputStreamEncoding::STREAM_PCM_S32,
      assistant_client::OutputStreamEncoding::STREAM_PCM_F32,
      assistant_client::OutputStreamEncoding::STREAM_MP3,
      assistant_client::OutputStreamEncoding::STREAM_OPUS_IN_OGG,
  };
}

assistant_client::AudioInput* AudioOutputProviderImpl::GetReferenceInput() {
  return &loop_back_input_;
}

bool AudioOutputProviderImpl::SupportsPlaybackTimestamp() const {
  // TODO(muyuanli): implement.
  return false;
}

assistant_client::VolumeControl& AudioOutputProviderImpl::GetVolumeControl() {
  return volume_control_impl_;
}

void AudioOutputProviderImpl::RegisterAudioEmittingStateCallback(
    AudioEmittingStateCallback callback) {
  // TODO(muyuanli): implement.
}

void AudioOutputProviderImpl::BindStreamFactory(
    mojo::PendingReceiver<audio::mojom::StreamFactory> receiver) {
  platform_delegate_->BindAudioStreamFactory(std::move(receiver));
}

}  // namespace assistant
}  // namespace chromeos
