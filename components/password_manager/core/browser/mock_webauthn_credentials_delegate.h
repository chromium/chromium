// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_WEBAUTHN_CREDENTIALS_DELEGATE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_WEBAUTHN_CREDENTIALS_DELEGATE_H_

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

  MOCK_METHOD(bool, IsWebAuthnAutofillEnabled, (), (const, override));
  MOCK_METHOD(void, LaunchWebAuthnFlow, (), (override));
  MOCK_METHOD(void,
              SelectWebAuthnCredential,
              (std::string backend_id),
              (override));
  MOCK_METHOD(const std::vector<autofill::Suggestion>&,
              GetWebAuthnSuggestions,
              (),
              (const override));
  MOCK_METHOD(void,
              RetrieveWebAuthnSuggestions,
              (base::OnceClosure),
              (override));
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_WEBAUTHN_CREDENTIALS_DELEGATE_H_
