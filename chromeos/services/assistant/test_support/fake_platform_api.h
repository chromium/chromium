// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_PLATFORM_API_H_
#define CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_PLATFORM_API_H_

#include "base/macros.h"
#include "chromeos/services/assistant/cros_platform_api.h"

namespace chromeos {
namespace assistant {

// Fake implementation of the |CrosPlatformApi| used during the unittests.
// As of now the |assistant_client::PlatformApi| methods are not implemented
// and will assert when called.
class FakePlatformApi : public CrosPlatformApi {
 public:
  FakePlatformApi() = default;
  ~FakePlatformApi() override = default;

  // CrosPlatformApi overrides
  assistant_client::AudioInputProvider& GetAudioInputProvider() override;
  assistant_client::AudioOutputProvider& GetAudioOutputProvider() override;
  assistant_client::AuthProvider& GetAuthProvider() override;
  assistant_client::FileProvider& GetFileProvider() override;
  assistant_client::NetworkProvider& GetNetworkProvider() override;
  assistant_client::SystemProvider& GetSystemProvider() override;
  void SetMicState(bool mic_open) override {}
  void OnHotwordEnabled(bool enable) override {}
  void OnConversationTurnStarted() override {}
  void OnConversationTurnFinished() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FakePlatformApi);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_PLATFORM_API_H_
