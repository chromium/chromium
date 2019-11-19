// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PLATFORM_API_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_PLATFORM_API_IMPL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/services/assistant/cros_platform_api.h"
#include "chromeos/services/assistant/platform/audio_input_provider_impl.h"
#include "chromeos/services/assistant/platform/audio_output_provider_impl.h"
#include "chromeos/services/assistant/platform/file_provider_impl.h"
#include "chromeos/services/assistant/platform/network_provider_impl.h"
#include "chromeos/services/assistant/platform/system_provider_impl.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "libassistant/shared/public/platform_auth.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"

namespace chromeos {
class PowerManagerClient;

namespace assistant {

class AssistantMediaSession;

// Platform API required by the voice assistant.
class PlatformApiImpl : public CrosPlatformApi,
                        CrasAudioHandler::AudioObserver {
 public:
  PlatformApiImpl(
      mojom::Client* client,
      AssistantMediaSession* media_session,
      PowerManagerClient* power_manager_client,
      CrasAudioHandler* cras_audio_handler,
      mojo::PendingRemote<device::mojom::BatteryMonitor> battery_monitor,
      scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> background_task_runner,
      std::string pref_locale);
  ~PlatformApiImpl() override;

  // assistant_client::PlatformApi overrides
  AudioInputProviderImpl& GetAudioInputProvider() override;
  assistant_client::AudioOutputProvider& GetAudioOutputProvider() override;
  assistant_client::AuthProvider& GetAuthProvider() override;
  assistant_client::FileProvider& GetFileProvider() override;
  assistant_client::NetworkProvider& GetNetworkProvider() override;
  assistant_client::SystemProvider& GetSystemProvider() override;

  // chromeos::CrasAudioHandler::AudioObserver overrides
  void OnAudioNodesChanged() override;

  // Called when the mic state associated with the interaction is changed.
  void SetMicState(bool mic_open) override;

  void OnConversationTurnStarted() override;
  void OnConversationTurnFinished() override;

  // Called when hotword enabled status changed.
  void OnHotwordEnabled(bool enable) override;

 private:
  // ChromeOS does not use auth manager, so we don't yet need to implement a
  // real auth provider.
  class DummyAuthProvider : public assistant_client::AuthProvider {
   public:
    DummyAuthProvider() = default;
    ~DummyAuthProvider() override = default;

    // assistant_client::AuthProvider overrides
    std::string GetAuthClientId() override;
    std::vector<std::string> GetClientCertificateChain() override;

    void CreateCredentialAttestationJwt(
        const std::string& authorization_code,
        const std::vector<std::pair<std::string, std::string>>& claims,
        CredentialCallback attestation_callback) override;

    void CreateRefreshAssertionJwt(
        const std::string& key_identifier,
        const std::vector<std::pair<std::string, std::string>>& claims,
        AssertionCallback assertion_callback) override;

    void CreateDeviceAttestationJwt(
        const std::vector<std::pair<std::string, std::string>>& claims,
        AssertionCallback attestation_callback) override;

    std::string GetAttestationCertFingerprint() override;

    void RemoveCredentialKey(const std::string& key_identifier) override;

    void Reset() override;
  };

  AudioInputProviderImpl audio_input_provider_;
  AudioOutputProviderImpl audio_output_provider_;
  DummyAuthProvider auth_provider_;
  FileProviderImpl file_provider_;
  NetworkProviderImpl network_provider_;
  std::unique_ptr<SystemProviderImpl> system_provider_;
  std::string pref_locale_;

  CrasAudioHandler* const cras_audio_handler_;

  DISALLOW_COPY_AND_ASSIGN(PlatformApiImpl);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PLATFORM_API_IMPL_H_
