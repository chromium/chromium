// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_MIGRATION_CROS_PLATFORM_API_H_
#define CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_MIGRATION_CROS_PLATFORM_API_H_

#include "base/macros.h"

namespace assistant_client {
class AudioInputProvider;
class AudioOutputProvider;
class AuthProvider;
class FileProvider;
class NetworkProvider;
class SystemProvider;
}  // namespace assistant_client

namespace chromeos {
namespace assistant {

// Platform API required by the voice assistant, extended with some methods used
// when ChromeOS needs to make changes to the platform state.
// Note that this no longer inherits from |assistant_client::PlatformApi|,
// because we are in the process of migrating its functionality from here to the
// Libassistant mojom service.
class CrosPlatformApi {
 public:
  CrosPlatformApi() = default;
  virtual ~CrosPlatformApi() = default;

  // Called when the mic state associated with the interaction is changed.
  virtual void SetMicState(bool mic_open) = 0;

  virtual void OnConversationTurnStarted() = 0;
  virtual void OnConversationTurnFinished() = 0;

  // Called when hotword enabled status changed.
  virtual void OnHotwordEnabled(bool enable) = 0;

  // Returns the platform's audio input provider.
  virtual assistant_client::AudioInputProvider& GetAudioInputProvider() = 0;

  // Returns the platform's audio output provider.
  virtual assistant_client::AudioOutputProvider& GetAudioOutputProvider() = 0;

  // Returns the platform's authentication provider.
  virtual assistant_client::AuthProvider& GetAuthProvider() = 0;

  // Returns the file provider to be used by libassistant.
  virtual assistant_client::FileProvider& GetFileProvider() = 0;

  // Returns the network provider to be used by libassistant.
  virtual assistant_client::NetworkProvider& GetNetworkProvider() = 0;

  // Returns the system provider to be used by libassistant.
  virtual assistant_client::SystemProvider& GetSystemProvider() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrosPlatformApi);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_MIGRATION_CROS_PLATFORM_API_H_
