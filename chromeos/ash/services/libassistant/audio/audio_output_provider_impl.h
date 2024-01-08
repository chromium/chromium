// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_AUDIO_AUDIO_OUTPUT_PROVIDER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_AUDIO_AUDIO_OUTPUT_PROVIDER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/ash/services/assistant/public/mojom/assistant_audio_decoder.mojom.h"
#include "chromeos/ash/services/libassistant/audio/audio_device_owner.h"
#include "chromeos/ash/services/libassistant/audio/audio_input_impl.h"
#include "chromeos/ash/services/libassistant/audio/volume_control_impl.h"
#include "chromeos/ash/services/libassistant/public/mojom/audio_output_delegate.mojom.h"
#include "chromeos/ash/services/libassistant/public/mojom/platform_delegate.mojom-forward.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::libassistant {

class AudioOutputProviderImpl : public assistant_client::AudioOutputProvider {
 public:
  // |AudioDecoderFactoryManager| is responsible for managing life time of
  // AssistantAudioDecoder service. |AudioOutputImpl| will hold a ref counted
  // object of |AudioDecoderFactoryManager|. |AudioDecoderFactoryManager| will
  // be destructed when all of ref counted objects of
  // |AudioDecoderFactoryManager| are destructed, i.e. All of |AudioOutputImpl|
  // are destructed.
  //
  // Define |AudioDecoderFactoryManager| here as we want to reference it from
  // |AudioOutputProviderImpl|.
  class AudioDecoderFactoryManager
      : public base::RefCounted<AudioDecoderFactoryManager> {
   public:
    virtual assistant::mojom::AssistantAudioDecoderFactory*
    GetAudioDecoderFactory() = 0;

    AudioDecoderFactoryManager() = default;
    AudioDecoderFactoryManager(const AudioDecoderFactoryManager&) = delete;
    AudioDecoderFactoryManager& operator=(const AudioDecoderFactoryManager&) =
        delete;

   protected:
    friend class base::RefCounted<AudioDecoderFactoryManager>;
    virtual ~AudioDecoderFactoryManager() = default;
  };

  explicit AudioOutputProviderImpl(const std::string& device_id);

  AudioOutputProviderImpl(const AudioOutputProviderImpl&) = delete;
  AudioOutputProviderImpl& operator=(const AudioOutputProviderImpl&) = delete;

  ~AudioOutputProviderImpl() override;

  void Bind(
      mojo::PendingRemote<mojom::AudioOutputDelegate> audio_output_delegate,
      mojom::PlatformDelegate* platform_delegate);

  // assistant_client::AudioOutputProvider overrides:
  assistant_client::AudioOutput* CreateAudioOutput(
      assistant_client::OutputStreamMetadata metadata) override;
  std::vector<assistant_client::OutputStreamEncoding>
  GetSupportedStreamEncodings() override;
  assistant_client::AudioInput* GetReferenceInput() override;
  bool SupportsPlaybackTimestamp() const override;
  assistant_client::VolumeControl& GetVolumeControl() override;
  void RegisterAudioEmittingStateCallback(
      AudioEmittingStateCallback callback) override;

  void BindAudioDecoderFactory();
  void UnBindAudioDecoderFactory();
  scoped_refptr<AudioDecoderFactoryManager>
  GetOrCreateAudioDecoderFactoryManager();

 private:
  void BindStreamFactory(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver);

  assistant_client::AudioOutput* CreateAudioOutputInternal(
      assistant_client::OutputStreamType type,
      const assistant_client::OutputStreamFormat& stream_format);

  // Owned by |AssistantManagerServiceImpl|.
  raw_ptr<mojom::PlatformDelegate> platform_delegate_ = nullptr;

  mojo::Remote<mojom::AudioOutputDelegate> audio_output_delegate_;

  AudioInputImpl loop_back_input_;
  VolumeControlImpl volume_control_impl_;
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  std::string device_id_;

  // Life time of |AudioDecoderFactoryManager| is managed by |AudioOutput|
  // instances. |AudioOutputImpl| will hold a ref counted object of
  // |AudioDecoderFactoryManager|.
  base::WeakPtr<AudioDecoderFactoryManager> audio_decoder_factory_manager_
      GUARDED_BY_CONTEXT(main_sequence_checker_);

  const bool start_audio_decoder_on_demand_;
  // This is used to implement |start_audio_decoder_on_demand_| flag off
  // behavior. |AudioOutputProviderImpl| will hold a ref counted object of
  // |AudioDecoderFactoryManager| as it won't destructed.
  scoped_refptr<AudioDecoderFactoryManager>
      audio_decoder_factory_manager_ref_counted_
          GUARDED_BY_CONTEXT(main_sequence_checker_);

  SEQUENCE_CHECKER(main_sequence_checker_);

  base::WeakPtrFactory<AudioOutputProviderImpl> weak_ptr_factory_{this};
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_AUDIO_AUDIO_OUTPUT_PROVIDER_IMPL_H_
