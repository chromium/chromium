// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_CROS_PLATFORM_API_H_
#define CHROMEOS_SERVICES_ASSISTANT_CROS_PLATFORM_API_H_

#include "base/macros.h"
#include "libassistant/shared/public/platform_api.h"

namespace chromeos {
namespace assistant {

// Platform API required by the voice assistant, extended with some methods used
// when ChromeOS needs to make changes to the platform state.
class CrosPlatformApi : public assistant_client::PlatformApi {
 public:
  CrosPlatformApi() = default;
  ~CrosPlatformApi() override = default;

  // Called when the mic state associated with the interaction is changed.
  virtual void SetMicState(bool mic_open) = 0;

  virtual void OnConversationTurnStarted() = 0;
  virtual void OnConversationTurnFinished() = 0;

  // Called when hotword enabled status changed.
  virtual void OnHotwordEnabled(bool enable) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrosPlatformApi);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_CROS_PLATFORM_API_H_
