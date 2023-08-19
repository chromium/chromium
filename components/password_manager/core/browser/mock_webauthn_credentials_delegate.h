// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_WEBAUTHN_CREDENTIALS_DELEGATE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_WEBAUTHN_CREDENTIALS_DELEGATE_H_

#include <string>
#include <vector>

#include "build/build_config.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/webauthn_credentials_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace password_manager {

class MockWebAuthnCredentialsDelegate : public WebAuthnCredentialsDelegate {
 public:
  MockWebAuthnCredentialsDelegate();
  ~MockWebAuthnCredentialsDelegate() override;

  MockWebAuthnCredentialsDelegate(const MockWebAuthnCredentialsDelegate&) =
      delete;
  MockWebAuthnCredentialsDelegate& operator=(
      const MockWebAuthnCredentialsDelegate&) = delete;

  MOCK_METHOD(void, LaunchWebAuthnFlow, (), (override));
  MOCK_METHOD(void, SelectPasskey, (const std::string& backend_id), (override));
  MOCK_METHOD(const absl::optional<std::vector<PasskeyCredential>>&,
              GetPasskeys,
              (),
              (const override));
  MOCK_METHOD(bool, OfferPasskeysFromAnotherDeviceOption, (), (const override));
  MOCK_METHOD(void, RetrievePasskeys, (base::OnceClosure), (override));
#if BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(void, ShowAndroidHybridSignIn, (), (override));
  MOCK_METHOD(bool, IsAndroidHybridAvailable, (), (const override));
#endif
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_WEBAUTHN_CREDENTIALS_DELEGATE_H_
