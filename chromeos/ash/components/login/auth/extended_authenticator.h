// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_EXTENDED_AUTHENTICATOR_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_EXTENDED_AUTHENTICATOR_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"

namespace ash {

class AuthStatusConsumer;
class UserContext;

// An interface to interact with cryptohomed: mount home dirs, create new home
// dirs, update passwords.
//
// Typical flow:
// AuthenticateToMount() calls cryptohomed to perform offline login,
// AuthenticateToCreate() calls cryptohomed to create new cryptohome.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH) ExtendedAuthenticator
    : public base::RefCountedThreadSafe<ExtendedAuthenticator> {
 public:
  enum AuthState {
    SUCCESS,       // Login succeeded.
    NO_MOUNT,      // No cryptohome exist for user.
    FAILED_MOUNT,  // Failed to mount existing cryptohome - login failed.
    FAILED_TPM,    // Failed to mount/create cryptohome because of TPM error.
  };

  using ResultCallback = base::OnceCallback<void(const std::string& result)>;
  using ContextCallback = base::OnceCallback<void(const UserContext& context)>;

  static scoped_refptr<ExtendedAuthenticator> Create(
      AuthStatusConsumer* consumer);

  ExtendedAuthenticator(const ExtendedAuthenticator&) = delete;
  ExtendedAuthenticator& operator=(const ExtendedAuthenticator&) = delete;

  // Updates consumer of the class.
  virtual void SetConsumer(AuthStatusConsumer* consumer) = 0;

  // This call will attempt to authenticate the user with the key (and key
  // label) in |context|. No further actions are taken after authentication.
  virtual void AuthenticateToCheck(const UserContext& context,
                                   base::OnceClosure success_callback) = 0;

  // This call will attempt to authenticate the user with the key (and key
  // label) in |context|, and unlock the WebAuthn secret using key if
  // authentication succeeds.
  virtual void AuthenticateToUnlockWebAuthnSecret(
      const UserContext& context,
      base::OnceClosure success_callback) = 0;

  // Attempts to start fingerprint auth session (prepare biometrics daemon for
  // upcoming fingerprint scan) for the user with |account_id|. |callback| will
  // be invoked with whether the fingerprint auth session is successfully
  // started.
  virtual void StartFingerprintAuthSession(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> callback) = 0;

  // Attempts to end the current fingerprint auth session. Logs an error if no
  // response.
  virtual void EndFingerprintAuthSession() = 0;

  // Waits for a fingerprint scan from the user in |context|, and calls
  // |callback| with a fingerprint-specific CryptohomeErrorCode. No further
  // actions are taken after authentication.
  virtual void AuthenticateWithFingerprint(
      const UserContext& context,
      base::OnceCallback<void(user_data_auth::CryptohomeErrorCode)>
          callback) = 0;

  // Hashes the key in |user_context| with the system salt it its type is
  // KEY_TYPE_PASSWORD_PLAIN and passes the resulting UserContext to the
  // |callback|.
  virtual void TransformKeyIfNeeded(const UserContext& user_context,
                                    ContextCallback callback) = 0;

 protected:
  ExtendedAuthenticator();
  virtual ~ExtendedAuthenticator();

 private:
  friend class base::RefCountedThreadSafe<ExtendedAuthenticator>;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_EXTENDED_AUTHENTICATOR_H_
