// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_AUDIO_AUDIO_OUTPUT_PROVIDER_IMPL_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_AUDIO_AUDIO_OUTPUT_PROVIDER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/services/assistant/public/mojom/assistant_audio_decoder.mojom.h"
#include "chromeos/services/libassistant/audio/audio_device_owner.h"
#include "chromeos/services/libassistant/audio/audio_input_impl.h"
#include "chromeos/services/libassistant/audio/volume_control_impl.h"
#include "chromeos/services/libassistant/public/mojom/audio_output_delegate.mojom.h"
#include "chromeos/services/libassistant/public/mojom/platform_delegate.mojom-forward.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

namespace libassistant {

class AudioOutputProviderImpl : public assistant_client::AudioOutputProvider {
 public:
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

 private:
  void BindStreamFactory(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver);

  assistant_client::AudioOutput* CreateAudioOutputInternal(
      assistant_client::OutputStreamType type,
      const assistant_client::OutputStreamFormat& stream_format);

  // Owned by |AssistantManagerServiceImpl|.
  mojom::PlatformDelegate* platform_delegate_ = nullptr;

  mojo::Remote<mojom::AudioOutputDelegate> audio_output_delegate_;

  AudioInputImpl loop_back_input_;
  VolumeControlImpl volume_control_impl_;
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  mojo::Remote<chromeos::assistant::mojom::AssistantAudioDecoderFactory>
      audio_decoder_factory_;
  std::string device_id_;
  base::WeakPtrFactory<AudioOutputProviderImpl> weak_ptr_factory_{this};
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_AUDIO_AUDIO_OUTPUT_PROVIDER_IMPL_H_
