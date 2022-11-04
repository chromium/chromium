// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_AUTH_PERFORMER_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_AUTH_PERFORMER_H_

#include <memory>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"
#include "chromeos/ash/components/login/auth/public/auth_session_intent.h"
#include "chromeos/ash/components/login/auth/public/auth_session_status.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class UserContext;

// This class provides higher level API for cryptohomed operations related to
// AuthSession: It starts new auth sessions, can authenticate auth session
// using various factors, extend session lifetime, close session.
// This implementation is only compatible with AuthSession-based API.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH) AuthPerformer {
 public:
  explicit AuthPerformer(base::raw_ptr<UserDataAuthClient> client);

  AuthPerformer(const AuthPerformer&) = delete;
  AuthPerformer& operator=(const AuthPerformer&) = delete;

  virtual ~AuthPerformer();

  using StartSessionCallback =
      base::OnceCallback<void(bool /* user_exists */,
                              std::unique_ptr<UserContext>,
                              absl::optional<AuthenticationError>)>;

  using AuthSessionStatusCallback =
      base::OnceCallback<void(AuthSessionStatus status,
                              base::TimeDelta lifetime,
                              std::unique_ptr<UserContext>,
                              absl::optional<AuthenticationError>)>;

  // Invalidates any ongoing mount attempts by invalidating Weak pointers on
  // internal callbacks. Callbacks for ongoing operations will not be called
  // afterwards, but there is no guarantees about state of the session.
  void InvalidateCurrentAttempts();

  base::WeakPtr<AuthPerformer> AsWeakPtr();

  // Starts new AuthSession using identity passed in `context`,
  // fills information about supported (and configured if user exists) keys.
  // `Context` should not have associated auth session.
  // Does not authenticate new session.
  virtual void StartAuthSession(std::unique_ptr<UserContext> context,
                                bool ephemeral,
                                AuthSessionIntent intent,
                                StartSessionCallback callback);

  // Invalidates the session contained in `context`.
  virtual void InvalidateAuthSession(std::unique_ptr<UserContext> context,
                                     AuthOperationCallback callback);

  // Prepares async auth factors such as fingerprint, activates the relevant
  // sensors to start listening.
  void PrepareAuthFactor(std::unique_ptr<UserContext> context,
                         cryptohome::AuthFactorType type,
                         AuthOperationCallback callback);

  // Terminate async auth factors, disables relevant sensors.
  void TerminateAuthFactor(std::unique_ptr<UserContext> context,
                           cryptohome::AuthFactorType type,
                           AuthOperationCallback callback);

  // Attempts to authenticate session using Key in `context`.
  // If key is a plain text, it is assumed that it is a knowledge-based key,
  // so it will be hashed accordingly, and key label would be backfilled
  // if not specified explicitly. Note that before migration to AuthFactors
  // this flow includes both Password and PIN key types.
  // In all other cases it is assumed that all fields are filled correctly.
  // Session will become authenticated upon success.
  void AuthenticateUsingKnowledgeKey(std::unique_ptr<UserContext> context,
                                     AuthOperationCallback callback);

  // Attempts to authenticate session using Key in `context`.
  // It is expected that the `challenge_response_keys` field is correctly filled
  // in the `context`.
  // Session will become authenticated upon success.
  void AuthenticateUsingChallengeResponseKey(
      std::unique_ptr<UserContext> context,
      AuthOperationCallback callback);

  // Attempts to authenticate session using plain text password.
  // Does not fill any password-related fields in `context`.
  // Session will become authenticated upon success.
  virtual void AuthenticateWithPassword(const std::string& key_label,
                                        const std::string& password,
                                        std::unique_ptr<UserContext> context,
                                        AuthOperationCallback callback);

  // Attempts to authenticate session using PIN as a factor.
  // PINs use custom salt stored in LocalState, this salt should be provided
  // by the calling side.
  // Session will become authenticated upon success.
  void AuthenticateWithPin(const std::string& pin,
                           const std::string& pin_salt,
                           std::unique_ptr<UserContext> context,
                           AuthOperationCallback callback);

  // Attempts to authenticate Kiosk session using specific key based on
  // identity.
  // Session will become authenticated upon success.
  void AuthenticateAsKiosk(std::unique_ptr<UserContext> context,
                           AuthOperationCallback callback);

  void GetAuthSessionStatus(std::unique_ptr<UserContext> context,
                            AuthSessionStatusCallback callback);

 private:
  void OnServiceRunning(std::unique_ptr<UserContext> context,
                        bool ephemeral,
                        AuthSessionIntent intent,
                        StartSessionCallback callback,
                        bool service_is_running);
  void OnStartAuthSession(
      std::unique_ptr<UserContext> context,
      StartSessionCallback callback,
      absl::optional<user_data_auth::StartAuthSessionReply> reply);

  void OnInvalidateAuthSession(
      std::unique_ptr<UserContext> context,
      AuthOperationCallback callback,
      absl::optional<user_data_auth::InvalidateAuthSessionReply> reply);

  void OnPrepareAuthFactor(
      std::unique_ptr<UserContext> context,
      AuthOperationCallback callback,
      absl::optional<user_data_auth::PrepareAuthFactorReply> reply);

  void OnTerminateAuthFactor(
      std::unique_ptr<UserContext> context,
      AuthOperationCallback callback,
      absl::optional<user_data_auth::TerminateAuthFactorReply> reply);

  void HashKeyAndAuthenticate(std::unique_ptr<UserContext> context,
                              AuthOperationCallback callback,
                              const std::string& system_salt);

  void HashPasswordAndAuthenticate(const std::string& key_label,
                                   const std::string& password,
                                   std::unique_ptr<UserContext> context,
                                   AuthOperationCallback callback,
                                   const std::string& system_salt);

  void OnAuthenticateAuthSession(
      std::unique_ptr<UserContext> context,
      AuthOperationCallback callback,
      absl::optional<user_data_auth::AuthenticateAuthSessionReply> reply);

  void OnAuthenticateAuthFactor(
      std::unique_ptr<UserContext> context,
      AuthOperationCallback callback,
      absl::optional<user_data_auth::AuthenticateAuthFactorReply> reply);

  void OnGetAuthSessionStatus(
      std::unique_ptr<UserContext> context,
      AuthSessionStatusCallback callback,
      absl::optional<user_data_auth::GetAuthSessionStatusReply> reply);

  const base::raw_ptr<UserDataAuthClient> client_;
  base::WeakPtrFactory<AuthPerformer> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_AUTH_PERFORMER_H_
