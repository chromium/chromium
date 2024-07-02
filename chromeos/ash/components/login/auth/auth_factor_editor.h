// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_AUTH_FACTOR_EDITOR_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_AUTH_FACTOR_EDITOR_H_

#include <optional>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"

namespace ash {

// This class provides higher level API for cryptohomed operations related to
// establishing and updating AuthFactors.
// This implementation is only compatible with AuthSession-based API.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH) AuthFactorEditor {
 public:
  explicit AuthFactorEditor(UserDataAuthClient* client);

  AuthFactorEditor(const AuthFactorEditor&) = delete;
  AuthFactorEditor& operator=(const AuthFactorEditor&) = delete;

  ~AuthFactorEditor();

  // Invalidates any ongoing mount attempts by invalidating Weak pointers on
  // internal callbacks. Callbacks for ongoing operations will not be called
  // afterwards, but there is no guarantees about state of the factors.
  void InvalidateCurrentAttempts();

  base::WeakPtr<AuthFactorEditor> AsWeakPtr();

  // Retrieves information about all configured and possible AuthFactors,
  // and stores it in `context`.
  // Should only be used with AuthFactors feature enabled.
  // Session should be authenticated.
  void GetAuthFactorsConfiguration(std::unique_ptr<UserContext> context,
                                   AuthOperationCallback callback);

  // Attempts to add Kiosk-specific key to user identified by `context`. On
  // success, as this will modify the auth factor configurations of the user,
  // the context auth factor configurations will be cleared.
  // Session should be authenticated.
  void AddKioskKey(std::unique_ptr<UserContext> context,
                   AuthOperationCallback callback);

  // Attempts to add knowledge-based Key contained in `context` to corresponding
  // user. Until migration to AuthFactors this method supports both password and
  // PIN keys. On success, as this will modify the auth factor configurations of
  // the user, the context auth factor configurations will be cleared.
  // Session should be authenticated.
  void AddContextKnowledgeKey(std::unique_ptr<UserContext> context,
                              AuthOperationCallback callback);

  // Attempts to add Challenge-response Key contained in `context` to
  // corresponding user. On success, as this will modify the auth factor
  // configurations of the user, the context auth factor configurations will be
  // cleared.
  // Session should be authenticated.
  void AddContextChallengeResponseKey(std::unique_ptr<UserContext> context,
                                      AuthOperationCallback callback);

  // Attempts to replace factor labeled by Key contained in `context`
  // with key stored in ReplacementKey in the `context`. On success, as this
  // will modify the auth factor configurations of the user, the context auth
  // factor configurations will be cleared.
  // Session should be authenticated.
  void ReplaceContextKey(std::unique_ptr<UserContext> context,
                         AuthOperationCallback callback);

  // Adds a PIN factor of the user corresponding to `context`. The PIN factor
  // must not be configured prior to calling this. On success, as this will
  // modify the auth factor configurations of the user, the context auth factor
  // configurations will be cleared.
  // Session should be authenticated.
  void AddPinFactor(std::unique_ptr<UserContext> context,
                    cryptohome::PinSalt salt,
                    cryptohome::RawPin pin,
                    AuthOperationCallback callback);

  // Replaces an already configured PIN factor of the user corresponding to
  // `context`. The PIN factor must already be configured configured prior to
  // calling this. On success, as this will modify the auth factor
  // configurations of the user, the context auth factor configurations will be
  // cleared.
  // Session should be authenticated.
  void ReplacePinFactor(std::unique_ptr<UserContext> context,
                        cryptohome::PinSalt salt,
                        cryptohome::RawPin pin,
                        AuthOperationCallback callback);

  // Removes the PIN factor of the user corresponding to `context`. Yields an
  // error if no PIN factor was configured prior to calling this. On success, as
  // this will modify the auth factor configurations of the user, the context
  // auth factor configurations will be cleared.
  // Session should be authenticated.
  void RemovePinFactor(std::unique_ptr<UserContext> context,
                       AuthOperationCallback callback);

  // Adds a recovery key for the user by `context`. No key is added if there is
  // already a recovery key. On success, as this will modify the auth factor
  // configurations of the user, the context auth factor configurations will be
  // cleared.
  // Session must be authenticated.
  void AddRecoveryFactor(std::unique_ptr<UserContext> context,
                         AuthOperationCallback callback);

  // Rotates the recovery factor of the user by `context`. The recovery factor
  // must already be configured prior to calling this. On success, as this will
  // modify the auth factor configurations of the user, the context auth factor
  // configurations will be cleared.
  // Session must be authenticated.
  void RotateRecoveryFactor(std::unique_ptr<UserContext> context,
                            bool ensure_fresh_recovery_id,
                            AuthOperationCallback callback);

  // Remove all recovery keys for the user by `context`. On success, as this
  // will modify the auth factor configurations of the user, the context auth
  // factor configurations will be cleared. Session must be authenticated.
  void RemoveRecoveryFactor(std::unique_ptr<UserContext> context,
                            AuthOperationCallback callback);

  // Sets the user's password factor if none already exists. On success, as this
  // will modify the auth factor configurations of the user, the context auth
  // factor configurations will be cleared.
  // Session should be authenticated.
  void SetPasswordFactor(std::unique_ptr<UserContext> context,
                         cryptohome::RawPassword new_password,
                         const cryptohome::KeyLabel& label,
                         AuthOperationCallback callback);

  // Updates the user's password with a new value. A password must
  // already be configured prior to calling this. On success, as this will
  // modify the auth factor configurations of the user, the context auth factor
  // configurations will be cleared.
  // Session should be authenticated.
  void UpdatePasswordFactor(std::unique_ptr<UserContext> context,
                            cryptohome::RawPassword new_password,
                            const cryptohome::KeyLabel& label,
                            AuthOperationCallback callback);

  // Updates the user's password factor's metadata. The password must already
  // be configured prior to calling this. On success, as this will modify the
  // auth factor configurations of the user, the context auth factor
  // configurations will be cleared.
  // Session should be authenticated.
  void UpdatePasswordFactorMetadata(std::unique_ptr<UserContext> context,
                                    const cryptohome::KeyLabel& label,
                                    const cryptohome::SystemSalt& system_salt,
                                    AuthOperationCallback callback);

  // Replaces the user's password factor with a new password factor (E.g.
  // Changing to local password from Gaia password). A password must already be
  // configured prior to calling this. The new password factor label must be
  // different from the old password factor label. On success, as this will
  // modify the auth factor configurations of the user, the context auth factor
  // configurations will be cleared. Session should be authenticated.
  void ReplacePasswordFactor(std::unique_ptr<UserContext> context,
                             const cryptohome::KeyLabel& old_label,
                             cryptohome::RawPassword new_password,
                             const cryptohome::KeyLabel& new_label,
                             AuthOperationCallback callback);

  // Updates the user's PIN factor's metadata. The PIN must already
  // be configured prior to calling this. On success, as this will modify the
  // auth factor configurations of the user, the context auth factor
  // configurations will be cleared.
  // Session should be authenticated.
  void UpdatePinFactorMetadata(std::unique_ptr<UserContext> context,
                               cryptohome::PinSalt salt,
                               AuthOperationCallback callback);

 private:
  void OnListAuthFactors(
      std::unique_ptr<UserContext> context,
      AuthOperationCallback callback,
      std::optional<user_data_auth::ListAuthFactorsReply> reply);

  void HashContextKeyAndAdd(std::unique_ptr<UserContext> context,
                            AuthOperationCallback callback,
                            const std::string& system_salt);

  void OnAddAuthFactor(std::unique_ptr<UserContext> context,
                       AuthOperationCallback callback,
                       std::optional<user_data_auth::AddAuthFactorReply> reply);

  void HashContextKeyAndReplace(std::unique_ptr<UserContext> context,
                                AuthOperationCallback callback,
                                const std::string& system_salt);

  void OnUpdateAuthFactor(
      std::unique_ptr<UserContext> context,
      AuthOperationCallback callback,
      std::optional<user_data_auth::UpdateAuthFactorReply> reply);

  void OnReplaceAuthFactor(
      std::unique_ptr<UserContext> context,
      AuthOperationCallback callback,
      std::optional<user_data_auth::ReplaceAuthFactorReply> reply);

  void OnRemoveAuthFactor(
      std::unique_ptr<UserContext> context,
      AuthOperationCallback callback,
      std::optional<user_data_auth::RemoveAuthFactorReply> reply);

  void OnUpdateAuthFactorMetadata(
      std::unique_ptr<UserContext> context,
      AuthOperationCallback callback,
      std::optional<user_data_auth::UpdateAuthFactorMetadataReply> reply);

  void SetPasswordFactorImpl(std::unique_ptr<UserContext> context,
                             cryptohome::RawPassword new_password,
                             const cryptohome::KeyLabel& label,
                             AuthOperationCallback calllback,
                             const std::string& system_salt);

  void UpdatePasswordFactorImpl(std::unique_ptr<UserContext> context,
                                cryptohome::RawPassword new_password,
                                const cryptohome::KeyLabel& label,
                                AuthOperationCallback callback,
                                const std::string& system_salt);

  void ReplacePasswordFactorImpl(std::unique_ptr<UserContext> context,
                                 const cryptohome::KeyLabel& old_label,
                                 cryptohome::RawPassword new_password,
                                 const cryptohome::KeyLabel& new_label,
                                 AuthOperationCallback callback,
                                 const std::string& system_salt);

  const raw_ptr<UserDataAuthClient, DanglingUntriaged> client_;
  base::WeakPtrFactory<AuthFactorEditor> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_AUTH_FACTOR_EDITOR_H_
