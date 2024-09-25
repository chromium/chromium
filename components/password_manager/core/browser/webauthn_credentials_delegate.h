// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_WEBAUTHN_CREDENTIALS_DELEGATE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_WEBAUTHN_CREDENTIALS_DELEGATE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/passkey_credential.h"

namespace password_manager {

// Delegate facilitating communication between the password manager and
// WebAuthn. It is associated with a single frame.
class WebAuthnCredentialsDelegate {
 public:
  using OnPasskeySelectedCallback = base::OnceClosure;
  virtual ~WebAuthnCredentialsDelegate() = default;

  // Launches the WebAuthn flow that lets users use their phones (hybrid) or
  // security keys to sign-in. On Android this will trigger Google Play
  // Services.
  virtual void LaunchSecurityKeyOrHybridFlow() = 0;

  // Called when the user selects a passkey from the autofill suggestion list
  // The selected credential must be from the list returned by the last call to
  // GetPasskeys(). |callback| should be invoked when the selected passkey is
  // consumed.
  virtual void SelectPasskey(const std::string& backend_id,
                             OnPasskeySelectedCallback callback) = 0;

  // Returns the list of eligible passkeys to fulfill an ongoing WebAuthn
  // request if one has been received and is active. Returns std::nullopt
  // otherwise.
  virtual const std::optional<std::vector<PasskeyCredential>>& GetPasskeys()
      const = 0;

  // Returns whether an option to use a passkey on a security key or another
  // device (e.g. phone via hybrid) should be offered. This option can be used
  // to trigger `LaunchSecurityKeyOrHybridFlow`.
  virtual bool IsSecurityKeyOrHybridFlowAvailable() const = 0;

  // Initiates retrieval of passkeys from the platform authenticator.
  // |callback| is invoked when credentials have been received, which could be
  // immediately.
  virtual void RetrievePasskeys(base::OnceCallback<void()> callback) = 0;

  // Returns true iff a passkey was selected via `SelectPasskey` and
  // `OnPasskeySelectedCallback` has not been called yet.
  virtual bool HasPendingPasskeySelection() = 0;

  // Get a WeakPtr to the instance.
  virtual base::WeakPtr<WebAuthnCredentialsDelegate> AsWeakPtr() = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_WEBAUTHN_CREDENTIALS_DELEGATE_H_
