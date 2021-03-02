// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_AUDIO_AUDIO_OUTPUT_PROVIDER_IMPL_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_AUDIO_AUDIO_OUTPUT_PROVIDER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/services/assistant/public/mojom/assistant_audio_decoder.mojom.h"
#include "chromeos/services/libassistant/audio/audio_device_owner.h"
#include "chromeos/services/libassistant/audio/audio_input_impl.h"
#include "chromeos/services/libassistant/audio/volume_control_impl.h"
#include "chromeos/services/libassistant/public/mojom/audio_output_delegate.mojom.h"
#include "chromeos/services/libassistant/public/mojom/platform_delegate.mojom-forward.h"
#include "libassistant/shared/public/platform_audio_output.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"

namespace chromeos {

namespace libassistant {

class AudioOutputProviderImpl : public assistant_client::AudioOutputProvider {
 public:
  explicit AudioOutputProviderImpl(const std::string& device_id);
  ~AudioOutputProviderImpl() override;

  void Bind(
      mojo::PendingRemote<mojom::AudioOutputDelegate> audio_output_delegate,
      mojom::PlatformDelegate* platform_delegate);

  // assistant_client::AudioOutputProvider overrides:
  assistant_client::AudioOutput* CreateAudioOutput(
      assistant_client::OutputStreamType type,
      const assistant_client::OutputStreamFormat& stream_format) override;

  std::vector<assistant_client::OutputStreamEncoding>
  GetSupportedStreamEncodings() override;

  assistant_client::AudioInput* GetReferenceInput() override;

  bool SupportsPlaybackTimestamp() const override;

  assistant_client::VolumeControl& GetVolumeControl() override;

  void RegisterAudioEmittingStateCallback(
      AudioEmittingStateCallback callback) override;

 private:
  void BindStreamFactory(
      mojo::PendingReceiver<audio::mojom::StreamFactory> receiver);

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

  DISALLOW_COPY_AND_ASSIGN(AudioOutputProviderImpl);
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_AUDIO_AUDIO_OUTPUT_PROVIDER_IMPL_H_
