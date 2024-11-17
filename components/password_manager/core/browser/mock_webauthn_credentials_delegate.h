// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_WEBAUTHN_CREDENTIALS_DELEGATE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_WEBAUTHN_CREDENTIALS_DELEGATE_H_

#include <optional>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/webauthn_credentials_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockWebAuthnCredentialsDelegate : public WebAuthnCredentialsDelegate {
 public:
  MockWebAuthnCredentialsDelegate();
  ~MockWebAuthnCredentialsDelegate() override;

  MockWebAuthnCredentialsDelegate(const MockWebAuthnCredentialsDelegate&) =
      delete;
  MockWebAuthnCredentialsDelegate& operator=(
      const MockWebAuthnCredentialsDelegate&) = delete;

  MOCK_METHOD(void, LaunchSecurityKeyOrHybridFlow, (), (override));
  MOCK_METHOD(void,
              SelectPasskey,
              (const std::string&,
               WebAuthnCredentialsDelegate::OnPasskeySelectedCallback),
              (override));
  MOCK_METHOD(const std::optional<std::vector<PasskeyCredential>>&,
              GetPasskeys,
              (),
              (const override));
  MOCK_METHOD(bool, IsSecurityKeyOrHybridFlowAvailable, (), (const override));
  MOCK_METHOD(void, RetrievePasskeys, (base::OnceClosure), (override));
  MOCK_METHOD(bool, HasPendingPasskeySelection, (), (override));
  base::WeakPtr<WebAuthnCredentialsDelegate> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<MockWebAuthnCredentialsDelegate> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_WEBAUTHN_CREDENTIALS_DELEGATE_H_
