// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_RECOVERY_CRYPTOHOME_RECOVERY_PERFORMER_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_RECOVERY_CRYPTOHOME_RECOVERY_PERFORMER_H_

#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"
#include "chromeos/ash/components/login/auth/recovery/cryptohome_recovery_service_client.h"

namespace ash {

// Helper class to authenticate using recovery. Coordinates calls to cryptohome
// and the requests over network to the recovery service.
class CryptohomeRecoveryPerformer {
 public:
  explicit CryptohomeRecoveryPerformer(
      UserDataAuthClient*,
      scoped_refptr<network::SharedURLLoaderFactory>);

  CryptohomeRecoveryPerformer(const CryptohomeRecoveryPerformer&) = delete;
  CryptohomeRecoveryPerformer& operator=(const CryptohomeRecoveryPerformer&) =
      delete;

  ~CryptohomeRecoveryPerformer();

  // Authenticates an auth session using recovery. `user_context` must contain
  // the following data:
  // - An activate auth session. On success, this auth session will be
  //   authenticated.
  // - A GaiaID. (As of this writing, only Gaia users can have recovery
  //   factors.)
  // - A reauth proof token and an access token that was obtained by
  //   authentication to gaia.
  void AuthenticateWithRecovery(std::unique_ptr<UserContext> user_context,
                                AuthOperationCallback callback);

 private:
  // Called with the reply when fetching the recovery epoch value via network.
  void OnNetworkFetchEpoch(
      std::unique_ptr<UserContext> context,
      AuthOperationCallback callback,
      absl::optional<CryptohomeRecoveryEpochResponse> epoch,
      CryptohomeRecoveryServerStatusCode);

  // Called with the reply to a call of GetRecoveryRequest.
  void OnGetRecoveryRequest(
      std::unique_ptr<UserContext> context,
      AuthOperationCallback callback,
      CryptohomeRecoveryEpochResponse epoch,
      absl::optional<user_data_auth::GetRecoveryRequestReply> reply);

  // Called with the reply when fetching the recovery secret from the recovery
  // service via network.
  void OnFetchRecoveryServiceResponse(
      std::unique_ptr<UserContext> context,
      AuthOperationCallback callback,
      CryptohomeRecoveryEpochResponse epoch,
      absl::optional<CryptohomeRecoveryResponse> response,
      CryptohomeRecoveryServerStatusCode);

  // Called with the response to the final call to AuthenticateAuthFactor.
  void OnAuthenticateAuthFactor(
      std::unique_ptr<UserContext> context,
      AuthOperationCallback callback,
      absl::optional<user_data_auth::AuthenticateAuthFactorReply> reply);

  const base::raw_ptr<UserDataAuthClient> user_data_auth_client_;
  CryptohomeRecoveryServiceClient service_client_;
  base::WeakPtrFactory<CryptohomeRecoveryPerformer> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_COMPONENTS_LOGIN_AUTH_RECOVERY_RECOVERY_PERFORMER_H_
