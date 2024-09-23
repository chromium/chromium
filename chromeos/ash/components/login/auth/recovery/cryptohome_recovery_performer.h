// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_RECOVERY_CRYPTOHOME_RECOVERY_PERFORMER_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_RECOVERY_CRYPTOHOME_RECOVERY_PERFORMER_H_

#include "base/timer/elapsed_timer.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_events_recorder.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"
#include "chromeos/ash/components/login/auth/recovery/cryptohome_recovery_service_client.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

class UserDataAuthClient;

// Helper class to authenticate using recovery. Coordinates calls to cryptohome
// and the requests over network to the recovery service.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH)
    CryptohomeRecoveryPerformer : public OAuth2AccessTokenConsumer {
 public:
  explicit CryptohomeRecoveryPerformer(
      UserDataAuthClient*,
      scoped_refptr<network::SharedURLLoaderFactory>);

  CryptohomeRecoveryPerformer(const CryptohomeRecoveryPerformer&) = delete;
  CryptohomeRecoveryPerformer& operator=(const CryptohomeRecoveryPerformer&) =
      delete;

  ~CryptohomeRecoveryPerformer() override;

  // Authenticates an auth session using recovery. `user_context` must contain
  // the following data:
  // - An activate auth session. On success, this auth session will be
  //   authenticated.
  // - A GaiaID. (As of this writing, only Gaia users can have recovery
  //   factors.)
  // - A reauth proof token and an access token that was obtained by
  //   authentication to gaia.
  // We should never trigger two concurrent requests as it updates the
  // `user_context_` the callback operations depend on.
  void AuthenticateWithRecovery(std::unique_ptr<UserContext> user_context,
                                AuthOperationCallback callback);

  // OAuth2AccessTokenConsumer:
  void OnGetTokenSuccess(const TokenResponse& token_response) override;
  void OnGetTokenFailure(const GoogleServiceAuthError& error) override;
  std::string GetConsumerName() const override;

 private:
  // Called with the reply when fetching the recovery epoch value via network.
  void OnNetworkFetchEpoch(std::optional<CryptohomeRecoveryEpochResponse> epoch,
                           CryptohomeRecoveryServerStatusCode);

  // Called with the reply to a call of GetRecoveryRequest.
  void OnGetRecoveryRequest(CryptohomeRecoveryEpochResponse epoch,
                            std::optional<RecoveryRequest> recovery_request,
                            std::unique_ptr<UserContext> context,
                            std::optional<AuthenticationError> error);

  // Called with the reply when fetching the recovery secret from the recovery
  // service via network.
  void OnFetchRecoveryServiceResponse(
      CryptohomeRecoveryEpochResponse epoch,
      std::optional<CryptohomeRecoveryResponse> response,
      CryptohomeRecoveryServerStatusCode);

  // Called with the response to the final call to AuthenticateWithRecovery.
  void OnAuthenticateWithRecovery(std::unique_ptr<UserContext> context,
                                  std::optional<AuthenticationError> error);

  // Record the result of the recovery and time taken.
  void RecordRecoveryResult(
      AuthEventsRecorder::CryptohomeRecoveryResult result);

  std::unique_ptr<UserContext> user_context_;
  AuthOperationCallback callback_;

  std::string access_token_;
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher_;

  std::unique_ptr<AuthPerformer> auth_performer_;
  CryptohomeRecoveryServiceClient service_client_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Used to record time taken for recovery.
  std::unique_ptr<base::ElapsedTimer> timer_;
  base::WeakPtrFactory<CryptohomeRecoveryPerformer> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_RECOVERY_CRYPTOHOME_RECOVERY_PERFORMER_H_
