// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_WEBAUTHN_CREDENTIALS_DELEGATE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_WEBAUTHN_CREDENTIALS_DELEGATE_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "build/build_config.h"

namespace password_manager {

class PasskeyCredential;

// Delegate facilitating communication between the password manager and
// WebAuthn. It is associated with a single frame.
class WebAuthnCredentialsDelegate {
 public:
  using OnPasskeySelectedCallback = base::OnceClosure;

  // Reasons why `GetPasskeys` might not have a passkey list to return.
  enum class PasskeysUnavailableReason {
    kNotReceived,
    kRequestAborted,
  };

  virtual ~WebAuthnCredentialsDelegate() = default;

  // Launches the WebAuthn flow that lets users use their phones (hybrid) or
  // security keys to sign-in. On Android this will trigger Google Play
  // Services.
  virtual void LaunchSecurityKeyOrHybridFlow() = 0;

  // Called when the user selects a passkey from the autofill suggestion list
  // The selected credential must be from the list returned by the last call to
  // GetPasskeys(). `callback` should be invoked when the selected passkey is
  // consumed.
  virtual void SelectPasskey(const std::string& backend_id,
                             OnPasskeySelectedCallback callback) = 0;

  // Returns the list of eligible passkeys to fulfill an ongoing WebAuthn
  // request if one has been received and is active. Returns std::nullopt
  // otherwise.
  virtual base::expected<const std::vector<PasskeyCredential>*,
                         PasskeysUnavailableReason>
  GetPasskeys() const = 0;

  // Called when a passkey consumer is displaying a UI surface that will
  // include passkeys, if any are available. This is for metrics recording
  // purposes.
  virtual void NotifyForPasskeysDisplay() = 0;

  // Returns whether an option to use a passkey on a security key or another
  // device (e.g. phone via hybrid) should be offered. This option can be used
  // to trigger `LaunchSecurityKeyOrHybridFlow`.
  virtual bool IsSecurityKeyOrHybridFlowAvailable() const = 0;

  // Retrieval of passkeys is initiated by the navigator.credentials.get()
  // WebAuthn call. Callers of this method will be notified via `callback` when
  // the passkey list is available, or if the WebAuthn request is aborted
  // before passkeys become available.
  // `callback` can be invoked mmediately if the passkey list has already been
  // received.
  // This can be called multiple times and all callbacks will be invoked.
  virtual void RequestNotificationWhenPasskeysReady(
      base::OnceCallback<void()> callback) = 0;

  // Returns true iff a passkey was selected via `SelectPasskey` and
  // `OnPasskeySelectedCallback` has not been called yet.
  virtual bool HasPendingPasskeySelection() = 0;

  // Get a WeakPtr to the instance.
  virtual base::WeakPtr<WebAuthnCredentialsDelegate> AsWeakPtr() = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_WEBAUTHN_CREDENTIALS_DELEGATE_H_
