// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_MOCK_GPM_PIN_SERVICE_H_
#define CHROME_BROWSER_WEBAUTHN_MOCK_GPM_PIN_SERVICE_H_

#include "base/functional/callback.h"
#include "chrome/browser/webauthn/gpm_pin_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace webauthn {

class MockGpmPinService : public GpmPinService {
 public:
  MockGpmPinService();
  MockGpmPinService(const MockGpmPinService&) = delete;
  MockGpmPinService& operator=(const MockGpmPinService&) = delete;
  ~MockGpmPinService() override;

  MOCK_METHOD(void,
              SyncPinState,
              (const CoreAccountInfo&, PinStateCallback),
              (override));
  MOCK_METHOD(void,
              VerifyPin,
              (const CoreAccountInfo&, GpmPin, VerifyPinCallback),
              (override));
  MOCK_METHOD(void,
              SetPin,
              (const CoreAccountInfo&, GpmPin, SetPinCallback),
              (override));
};

}  // namespace webauthn

#endif  // CHROME_BROWSER_WEBAUTHN_MOCK_GPM_PIN_SERVICE_H_
