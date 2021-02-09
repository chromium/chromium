// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/platform_api.h"
#include "base/check.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/libassistant/power_manager_provider_impl.h"
#include "chromeos/services/libassistant/system_provider_impl.h"

namespace chromeos {
namespace libassistant {

PlatformApi::PlatformApi() {
  // Only enable native power features if they are supported by the UI.
  std::unique_ptr<PowerManagerProviderImpl> provider;
  if (assistant::features::IsPowerManagerEnabled()) {
    provider = std::make_unique<PowerManagerProviderImpl>();
  }
  system_provider_ = std::make_unique<SystemProviderImpl>(std::move(provider));
}

PlatformApi::~PlatformApi() = default;

void PlatformApi::Initialize(
    chromeos::libassistant::mojom::PlatformDelegate* delegate) {
  system_provider_->Initialize(delegate);
}

PlatformApi& PlatformApi::SetAudioInputProvider(
    assistant_client::AudioInputProvider* provider) {
  audio_input_provider_ = provider;
  return *this;
}

PlatformApi& PlatformApi::SetAudioOutputProvider(
    assistant_client::AudioOutputProvider* provider) {
  audio_output_provider_ = provider;
  return *this;
}

PlatformApi& PlatformApi::SetAuthProvider(
    assistant_client::AuthProvider* provider) {
  auth_provider_ = provider;
  return *this;
}

PlatformApi& PlatformApi::SetFileProvider(
    assistant_client::FileProvider* provider) {
  file_provider_ = provider;
  return *this;
}

PlatformApi& PlatformApi::SetNetworkProvider(
    assistant_client::NetworkProvider* provider) {
  network_provider_ = provider;
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
  DCHECK(auth_provider_);
  return *auth_provider_;
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

}  // namespace libassistant
}  // namespace chromeos
