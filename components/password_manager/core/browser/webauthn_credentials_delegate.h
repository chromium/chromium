// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_WEBAUTHN_CREDENTIALS_DELEGATE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_WEBAUTHN_CREDENTIALS_DELEGATE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "components/autofill/core/browser/ui/suggestion.h"

namespace password_manager {

// Delegate facilitating communication between the password manager and
// WebAuthn.
class WebAuthnCredentialsDelegate {
 public:
  virtual ~WebAuthnCredentialsDelegate() = default;

  // Returns true if integration between WebAuthn and Autofill is enabled.
  virtual bool IsWebAuthnAutofillEnabled() const = 0;

  // Launches the normal WebAuthn flow that lets users use their phones or
  // security keys to sign-in.
  virtual void LaunchWebAuthnFlow() = 0;

  // Called when the user selects a WebAuthn credential from the autofill
  // suggestion list.
  virtual void SelectWebAuthnCredential(std::string backend_id) = 0;

  // Returns the list of eligible WebAuthn credentials to fulfill an ongoing
  // WebAuthn request.
  virtual const std::vector<autofill::Suggestion>& GetWebAuthnSuggestions()
      const = 0;

  // Initiates retrieval of discoverable WebAuthn credentials from the platform
  // authenticator. |callback| is invoked upon completion.
  virtual void RetrieveWebAuthnSuggestions(
      base::OnceCallback<void()> callback) = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_WEBAUTHN_CREDENTIALS_DELEGATE_H_
