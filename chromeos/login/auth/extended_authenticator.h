// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_EXTENDED_AUTHENTICATOR_H_
#define CHROMEOS_LOGIN_AUTH_EXTENDED_AUTHENTICATOR_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"

namespace chromeos {

class AuthStatusConsumer;
class UserContext;

// An interface to interact with cryptohomed: mount home dirs, create new home
// dirs, update passwords.
//
// Typical flow:
// AuthenticateToMount() calls cryptohomed to perform offline login,
// AuthenticateToCreate() calls cryptohomed to create new cryptohome.
class COMPONENT_EXPORT(CHROMEOS_LOGIN_AUTH) ExtendedAuthenticator
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

  // Updates consumer of the class.
  virtual void SetConsumer(AuthStatusConsumer* consumer) = 0;

  // TODO(crbug.com/866790): Following method was only used for supervised
  // users authentication. Check if there is a reason to keep it and remove it.

  // This call will attempt to mount the home dir for the user, key (and key
  // label) in |context|. If the key is of type KEY_TYPE_PASSWORD_PLAIN, it will
  // be hashed with the system salt before being passed to cryptohomed. This
  // call assumes that the home dir already exist for the user and will return
  // an error otherwise. On success, the user ID hash (used as the mount point)
  // will be passed to |success_callback|.
  virtual void AuthenticateToMount(const UserContext& context,
                                   ResultCallback success_callback) = 0;

  // This call will attempt to authenticate the user with the key (and key
  // label) in |context|. No further actions are taken after authentication.
  virtual void AuthenticateToCheck(const UserContext& context,
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
      base::OnceCallback<void(cryptohome::CryptohomeErrorCode)> callback) = 0;

  // Attempts to add a new |key| for the user identified/authorized by
  // |context|. If a key with the same label already exists, the behavior
  // depends on the |replace_existing| flag. If the flag is set, the old key is
  // replaced. If the flag is not set, an error occurs. It is not allowed to
  // replace the key used for authorization.
  virtual void AddKey(const UserContext& context,
                      const cryptohome::KeyDefinition& key,
                      bool replace_existing,
                      base::OnceClosure success_callback) = 0;

  // Attempts to remove the key labeled |key_to_remove| for the user identified/
  // authorized by |context|. It is possible to remove the key used for
  // authorization, although it should be done with extreme care.
  virtual void RemoveKey(const UserContext& context,
                         const std::string& key_to_remove,
                         base::OnceClosure success_callback) = 0;

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

  DISALLOW_COPY_AND_ASSIGN(ExtendedAuthenticator);
};

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_AUTH_EXTENDED_AUTHENTICATOR_H_
