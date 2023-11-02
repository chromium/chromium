// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/platform_api.h"

#include "base/check.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/ash/services/libassistant/audio/audio_output_provider_impl.h"
#include "chromeos/ash/services/libassistant/fake_auth_provider.h"
#include "chromeos/ash/services/libassistant/file_provider_impl.h"
#include "chromeos/ash/services/libassistant/power_manager_provider_impl.h"
#include "chromeos/ash/services/libassistant/system_provider_impl.h"
#include "media/audio/audio_device_description.h"

namespace ash::libassistant {

PlatformApi::PlatformApi()
    : audio_output_provider_(std::make_unique<AudioOutputProviderImpl>(
          media::AudioDeviceDescription::kDefaultDeviceId)),
      fake_auth_provider_(std::make_unique<FakeAuthProvider>()),
      file_provider_(std::make_unique<FileProviderImpl>()),
      network_provider_(std::make_unique<NetworkProviderImpl>()) {
  // Only enable native power features if they are supported by the UI.
  std::unique_ptr<PowerManagerProviderImpl> provider;
  if (assistant::features::IsPowerManagerEnabled()) {
    provider = std::make_unique<PowerManagerProviderImpl>();
  }
  system_provider_ = std::make_unique<SystemProviderImpl>(std::move(provider));
}

PlatformApi::~PlatformApi() = default;

void PlatformApi::Bind(
    mojo::PendingRemote<mojom::AudioOutputDelegate> audio_output_delegate,
    mojom::PlatformDelegate* platform_delegate) {
  audio_output_provider_->Bind(std::move(audio_output_delegate),
                               platform_delegate);
  network_provider_->Initialize(platform_delegate);
  system_provider_->Initialize(platform_delegate);
}

PlatformApi& PlatformApi::SetAudioInputProvider(
    assistant_client::AudioInputProvider* provider) {
  audio_input_provider_ = provider;
  return *this;
}

assistant_client::AudioInputProvider& PlatformApi::GetAudioInputProvider() {
  DCHECK(audio_input_provider_);
  return *audio_input_provider_;
}

assistant_client::AudioOutputProvider& PlatformApi::GetAudioOutputProvider() {
  DCHECK(audio_output_provider_);
  return *audio_output_provider_;
}

assistant_client::AuthProvider& PlatformApi::GetAuthProvider() {
  DCHECK(fake_auth_provider_);
  return *fake_auth_provider_;
}

assistant_client::FileProvider& PlatformApi::GetFileProvider() {
  DCHECK(file_provider_);
  return *file_provider_;
}

assistant_client::NetworkProvider& PlatformApi::GetNetworkProvider() {
  DCHECK(network_provider_);
  return *network_provider_;
}

assistant_client::SystemProvider& PlatformApi::GetSystemProvider() {
  DCHECK(system_provider_);
  return *system_provider_;
}

void PlatformApi::OnAssistantClientCreated(AssistantClient* assistant_client) {
  audio_output_provider_->BindAudioDecoderFactory();
}

void PlatformApi::OnAssistantClientDestroyed() {
  audio_output_provider_->UnBindAudioDecoderFactory();
}

}  // namespace ash::libassistant
