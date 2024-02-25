// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/audio/audio_output_provider_impl.h"

#include <algorithm>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/services/assistant/public/mojom/assistant_audio_decoder.mojom.h"
#include "chromeos/ash/services/libassistant/audio/audio_stream_handler.h"
#include "chromeos/ash/services/libassistant/public/mojom/platform_delegate.mojom.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "media/audio/audio_device_description.h"

namespace ash::libassistant {

namespace {

bool IsEncodedFormat(const assistant_client::OutputStreamFormat& format) {
  return format.encoding ==
             assistant_client::OutputStreamEncoding::STREAM_MP3 ||
         format.encoding ==
             assistant_client::OutputStreamEncoding::STREAM_OPUS_IN_OGG;
}

// Instances of this class will be owned by Libassistant, so any public method
// (including the constructor and destructor) can and will be called from other
// threads.
class AudioOutputImpl : public assistant_client::AudioOutput {
 public:
  AudioOutputImpl(
      AudioOutputProviderImpl* audio_output_provider_impl,
      mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory,
      scoped_refptr<base::SequencedTaskRunner> main_task_runner,
      mojom::AudioOutputDelegate* audio_output_delegate,
      assistant_client::OutputStreamType type,
      assistant_client::OutputStreamFormat format,
      const std::string& device_id)
      : audio_output_provider_impl_(audio_output_provider_impl),
        main_task_runner_(main_task_runner),
        stream_factory_(std::move(stream_factory)),
        audio_output_delegate_(audio_output_delegate),
        stream_type_(type),
        format_(format) {
    // The constructor runs on the Libassistant thread, so we need to detach the
    // main sequence checker.
    DETACH_FROM_SEQUENCE(main_sequence_checker_);
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AudioOutputImpl::InitializeOnMainThread,
                                  weak_ptr_factory_.GetWeakPtr(), device_id));
  }

  AudioOutputImpl(const AudioOutputImpl&) = delete;
  AudioOutputImpl& operator=(const AudioOutputImpl&) = delete;

  ~AudioOutputImpl() override {
    main_task_runner_->ReleaseSoon(FROM_HERE,
                                   std::move(audio_decoder_factory_manager_));
    main_task_runner_->DeleteSoon(FROM_HERE, device_owner_.release());
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
  void InitializeOnMainThread(const std::string& device_id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

    audio_stream_handler_ = std::make_unique<AudioStreamHandler>();
    device_owner_ = std::make_unique<AudioDeviceOwner>(device_id);
  }

  void StartOnMainThread(assistant_client::AudioOutput::Delegate* delegate) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

    // TODO(llin): Remove getting audio focus here after libassistant handles
    // acquiring audio focus for the internal media player.
    if (stream_type_ == assistant_client::OutputStreamType::STREAM_MEDIA) {
      audio_output_delegate_->RequestAudioFocus(
          mojom::AudioOutputStreamType::kMediaStream);
    }

    if (IsEncodedFormat(format_)) {
      if (!audio_decoder_factory_manager_) {
        audio_decoder_factory_manager_ =
            audio_output_provider_impl_
                ->GetOrCreateAudioDecoderFactoryManager();
      }

      audio_stream_handler_->StartAudioDecoder(
          audio_decoder_factory_manager_->GetAudioDecoderFactory(), delegate,
          base::BindOnce(&AudioDeviceOwner::Start,
                         base::Unretained(device_owner_.get()),
                         audio_output_delegate_, audio_stream_handler_.get(),
                         std::move(stream_factory_)));
    } else {
      device_owner_->Start(audio_output_delegate_, delegate,
                           std::move(stream_factory_), format_);
    }
  }

  void StopOnMainThread() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

    // TODO(llin): Remove abandoning audio focus here after libassistant handles
    // abandoning audio focus for the internal media player.
    if (stream_type_ == assistant_client::OutputStreamType::STREAM_MEDIA) {
      audio_output_delegate_->AbandonAudioFocusIfNeeded();
    }

    if (IsEncodedFormat(format_)) {
      device_owner_->SetDelegate(nullptr);
      audio_stream_handler_->OnStopped();
    } else {
      device_owner_->Stop();
    }
  }

  raw_ptr<AudioOutputProviderImpl> audio_output_provider_impl_
      GUARDED_BY_CONTEXT(main_sequence_checker_);
  scoped_refptr<AudioOutputProviderImpl::AudioDecoderFactoryManager>
      audio_decoder_factory_manager_ GUARDED_BY_CONTEXT(main_sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory_
      GUARDED_BY_CONTEXT(main_sequence_checker_);
  const raw_ptr<mojom::AudioOutputDelegate> audio_output_delegate_
      GUARDED_BY_CONTEXT(main_sequence_checker_);

  // Accessed from both Libassistant and main sequence, so should remain
  // |const|.
  const assistant_client::OutputStreamType stream_type_;

  assistant_client::OutputStreamFormat format_
      GUARDED_BY_CONTEXT(main_sequence_checker_);

  std::unique_ptr<AudioStreamHandler> audio_stream_handler_
      GUARDED_BY_CONTEXT(main_sequence_checker_);
  std::unique_ptr<AudioDeviceOwner> device_owner_
      GUARDED_BY_CONTEXT(main_sequence_checker_);

  // This class is used both from the Libassistant and main thread.
  SEQUENCE_CHECKER(main_sequence_checker_);

  base::WeakPtrFactory<AudioOutputImpl> weak_ptr_factory_{this};
};

class AudioDecoderFactoryManagerImpl
    : public AudioOutputProviderImpl::AudioDecoderFactoryManager {
 public:
  explicit AudioDecoderFactoryManagerImpl(
      mojom::PlatformDelegate* platform_delegate) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

    platform_delegate->BindAudioDecoderFactory(
        audio_decoder_factory_.BindNewPipeAndPassReceiver());
  }

  assistant::mojom::AssistantAudioDecoderFactory* GetAudioDecoderFactory()
      override {
    return audio_decoder_factory_.get();
  }

  base::WeakPtr<AudioDecoderFactoryManager> GetWeakPtr() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  ~AudioDecoderFactoryManagerImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

    audio_decoder_factory_.reset();
  }

 private:
  mojo::Remote<assistant::mojom::AssistantAudioDecoderFactory>
      audio_decoder_factory_;

  SEQUENCE_CHECKER(main_sequence_checker_);

  base::WeakPtrFactory<AudioDecoderFactoryManagerImpl> weak_ptr_factory_{this};
};

}  // namespace

AudioOutputProviderImpl::AudioOutputProviderImpl(const std::string& device_id)
    : loop_back_input_(media::AudioDeviceDescription::kLoopbackInputDeviceId),
      volume_control_impl_(),
      main_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      device_id_(device_id),
      start_audio_decoder_on_demand_(
          features::IsStartAssistantAudioDecoderOnDemandEnabled()) {}

void AudioOutputProviderImpl::Bind(
    mojo::PendingRemote<mojom::AudioOutputDelegate> audio_output_delegate,
    mojom::PlatformDelegate* platform_delegate) {
  platform_delegate_ = platform_delegate;

  audio_output_delegate_.Bind(std::move(audio_output_delegate));

  volume_control_impl_.Initialize(audio_output_delegate_.get(),
                                  platform_delegate);
  loop_back_input_.Initialize(platform_delegate);
}

AudioOutputProviderImpl::~AudioOutputProviderImpl() = default;

// Called from the Libassistant thread.
assistant_client::AudioOutput* AudioOutputProviderImpl::CreateAudioOutput(
    assistant_client::OutputStreamMetadata metadata) {
  return CreateAudioOutputInternal(metadata.type,
                                   metadata.buffer_stream_format);
}

// Called from the Libassistant thread.
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

// Called from the Libassistant thread.
assistant_client::AudioInput* AudioOutputProviderImpl::GetReferenceInput() {
  return &loop_back_input_;
}

// Called from the Libassistant thread.
bool AudioOutputProviderImpl::SupportsPlaybackTimestamp() const {
  // TODO(muyuanli): implement.
  return false;
}

// Called from the Libassistant thread.
assistant_client::VolumeControl& AudioOutputProviderImpl::GetVolumeControl() {
  return volume_control_impl_;
}

// Called from the Libassistant thread.
void AudioOutputProviderImpl::RegisterAudioEmittingStateCallback(
    AudioEmittingStateCallback callback) {
  // TODO(muyuanli): implement.
}

void AudioOutputProviderImpl::BindAudioDecoderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  if (start_audio_decoder_on_demand_)
    return;

  // Hold a ref counted object of |AudioDecoderFactoryManager| as it won't get
  // destructed.
  audio_decoder_factory_manager_ref_counted_ =
      GetOrCreateAudioDecoderFactoryManager();
}

void AudioOutputProviderImpl::UnBindAudioDecoderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  if (start_audio_decoder_on_demand_)
    return;

  audio_decoder_factory_manager_ref_counted_.reset();
}

scoped_refptr<AudioOutputProviderImpl::AudioDecoderFactoryManager>
AudioOutputProviderImpl::GetOrCreateAudioDecoderFactoryManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  if (audio_decoder_factory_manager_) {
    return base::WrapRefCounted<
        AudioOutputProviderImpl::AudioDecoderFactoryManager>(
        audio_decoder_factory_manager_.get());
  }

  auto impl =
      base::MakeRefCounted<AudioDecoderFactoryManagerImpl>(platform_delegate_);
  audio_decoder_factory_manager_ = impl->GetWeakPtr();
  return std::move(impl);
}

void AudioOutputProviderImpl::BindStreamFactory(
    mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  platform_delegate_->BindAudioStreamFactory(std::move(receiver));
}

// Called from the Libassistant thread.
assistant_client::AudioOutput*
AudioOutputProviderImpl::CreateAudioOutputInternal(
    assistant_client::OutputStreamType type,
    const assistant_client::OutputStreamFormat& stream_format) {
  mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory;
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioOutputProviderImpl::BindStreamFactory,
                     weak_ptr_factory_.GetWeakPtr(),
                     stream_factory.InitWithNewPipeAndPassReceiver()));

  // Owned by one arbitrary thread inside libassistant. It will be destroyed
  // once assistant_client::AudioOutput::Delegate::OnStopped() is called.
  return new AudioOutputImpl(this, std::move(stream_factory), main_task_runner_,
                             audio_output_delegate_.get(), type, stream_format,
                             device_id_);
}

}  // namespace ash::libassistant
