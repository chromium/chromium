// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_PLATFORM_API_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_PLATFORM_API_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/libassistant/grpc/assistant_client_observer.h"
#include "chromeos/ash/services/libassistant/network_provider_impl.h"
#include "chromeos/ash/services/libassistant/public/mojom/audio_output_delegate.mojom.h"
#include "chromeos/ash/services/libassistant/public/mojom/platform_delegate.mojom.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash::libassistant {

class AudioOutputProviderImpl;
class FakeAuthProvider;
class FileProviderImpl;
class NetworkProviderImpl;
class SystemProviderImpl;

// Implementation of the Libassistant PlatformApi.
// The components that haven't been migrated to this mojom service will still be
// implemented chromeos/service/assistant/platform (and simply be exposed here).
class PlatformApi : public assistant_client::PlatformApi,
                    public AssistantClientObserver {
 public:
  PlatformApi();
  PlatformApi(const PlatformApi&) = delete;
  PlatformApi& operator=(const PlatformApi&) = delete;
  ~PlatformApi() override;

  void Bind(
      mojo::PendingRemote<mojom::AudioOutputDelegate> audio_output_delegate,
      mojom::PlatformDelegate* platform_delegate);

  PlatformApi& SetAudioInputProvider(assistant_client::AudioInputProvider*);

  // assistant_client::PlatformApi:
  assistant_client::AudioInputProvider& GetAudioInputProvider() override;
  assistant_client::AudioOutputProvider& GetAudioOutputProvider() override;
  assistant_client::AuthProvider& GetAuthProvider() override;
  assistant_client::FileProvider& GetFileProvider() override;
  assistant_client::NetworkProvider& GetNetworkProvider() override;
  assistant_client::SystemProvider& GetSystemProvider() override;

  // AssistantClientObserver:
  void OnAssistantClientCreated(AssistantClient* assistant_client) override;
  void OnAssistantClientDestroyed() override;

 private:
  // This is owned by |AudioInputController|.
  raw_ptr<assistant_client::AudioInputProvider> audio_input_provider_ = nullptr;

  std::unique_ptr<AudioOutputProviderImpl> audio_output_provider_;
  std::unique_ptr<FakeAuthProvider> fake_auth_provider_;
  std::unique_ptr<FileProviderImpl> file_provider_;
  std::unique_ptr<NetworkProviderImpl> network_provider_;
  std::unique_ptr<SystemProviderImpl> system_provider_;
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_PLATFORM_API_H_
