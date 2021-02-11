// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_PLATFORM_API_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_PLATFORM_API_H_

#include "libassistant/shared/public/platform_api.h"

#include <memory>

#include "chromeos/services/libassistant/public/mojom/audio_output_delegate.mojom.h"
#include "chromeos/services/libassistant/public/mojom/platform_delegate.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromeos {
namespace libassistant {

class AudioOutputProviderImpl;
class FakeAuthProvider;
class SystemProviderImpl;

// Implementation of the Libassistant PlatformApi.
// The components that haven't been migrated to this mojom service will still be
// implemented chromeos/service/assistant/platform (and simply be exposed here).
class PlatformApi : public assistant_client::PlatformApi {
 public:
  PlatformApi();
  PlatformApi(const PlatformApi&) = delete;
  PlatformApi& operator=(const PlatformApi&) = delete;
  ~PlatformApi() override;

  void Bind(
      mojo::PendingRemote<mojom::AudioOutputDelegate> audio_output_delegate,
      mojom::PlatformDelegate* platform_delegate);

  PlatformApi& SetAudioInputProvider(assistant_client::AudioInputProvider*);
  PlatformApi& SetFileProvider(assistant_client::FileProvider*);
  PlatformApi& SetNetworkProvider(assistant_client::NetworkProvider*);

  // assistant_client::PlatformApi implementation:
  assistant_client::AudioInputProvider& GetAudioInputProvider() override;
  assistant_client::AudioOutputProvider& GetAudioOutputProvider() override;
  assistant_client::AuthProvider& GetAuthProvider() override;
  assistant_client::FileProvider& GetFileProvider() override;
  assistant_client::NetworkProvider& GetNetworkProvider() override;
  assistant_client::SystemProvider& GetSystemProvider() override;

 private:
  // The below are all owned by the browser side |PlatformApiImpl|,
  // which outlives us.
  assistant_client::AudioInputProvider* audio_input_provider_ = nullptr;
  assistant_client::FileProvider* file_provider_ = nullptr;
  assistant_client::NetworkProvider* network_provider_ = nullptr;

  std::unique_ptr<AudioOutputProviderImpl> audio_output_provider_;
  std::unique_ptr<FakeAuthProvider> fake_auth_provider_;
  std::unique_ptr<SystemProviderImpl> system_provider_;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_PLATFORM_API_H_
