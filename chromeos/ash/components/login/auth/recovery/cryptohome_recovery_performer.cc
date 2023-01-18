// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/recovery/cryptohome_recovery_performer.h"

#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "chromeos/ash/components/cryptohome/userdataauth_util.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/auth_factor.pb.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/login/auth/recovery/cryptohome_recovery_service_client.h"
#include "chromeos/ash/components/login/auth/recovery/service_constants.h"
#include "components/device_event_log/device_event_log.h"
#include "google_apis/gaia/gaia_access_token_fetcher.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace ash {

CryptohomeRecoveryPerformer::CryptohomeRecoveryPerformer(
    UserDataAuthClient* user_data_auth_client,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : user_data_auth_client_(user_data_auth_client),
      service_client_(url_loader_factory),
      url_loader_factory_(url_loader_factory) {}

CryptohomeRecoveryPerformer::~CryptohomeRecoveryPerformer() = default;

void CryptohomeRecoveryPerformer::AuthenticateWithRecovery(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  DCHECK(!context->GetAuthSessionId().empty());
  DCHECK(!context->GetRefreshToken().empty())
      << "Gaia refresh token must be set for recovery";

  LOGIN_LOG(EVENT) << "Authenticating with recovery";

  user_context_ = std::move(context);
  callback_ = std::move(callback);

  const std::string& refresh_token{user_context_->GetRefreshToken()};
  access_token_fetcher_ =
      GaiaAccessTokenFetcher::CreateExchangeRefreshTokenForAccessTokenInstance(
          this, url_loader_factory_, refresh_token);
  access_token_fetcher_->Start(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(),
      GetRecoveryOAuth2Scope());
}

void CryptohomeRecoveryPerformer::OnGetTokenSuccess(
    const TokenResponse& token_response) {
  access_token_ = token_response.access_token;
  service_client_.FetchEpoch(
      GaiaAccessToken(access_token_),
      base::BindOnce(&CryptohomeRecoveryPerformer::OnNetworkFetchEpoch,
                     weak_factory_.GetWeakPtr()));
}

void CryptohomeRecoveryPerformer::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {
  LOGIN_LOG(EVENT) << "Failed to fetch access token for recovery, error: "
                   << error.ToString();
  std::move(callback_).Run(
      std::move(user_context_),
      AuthenticationError{AuthFailure::CRYPTOHOME_RECOVERY_OAUTH_TOKEN_ERROR});
}

std::string CryptohomeRecoveryPerformer::GetConsumerName() const {
  return "cryptohome_recovery_performer";
}

void CryptohomeRecoveryPerformer::OnNetworkFetchEpoch(
    absl::optional<CryptohomeRecoveryEpochResponse> opt_epoch,
    CryptohomeRecoveryServerStatusCode status) {
  if (status != CryptohomeRecoveryServerStatusCode::kSuccess) {
    std::move(callback_).Run(std::move(user_context_),
                             AuthenticationError(AuthFailure(status)));
    return;
  }
  CryptohomeRecoveryEpochResponse epoch = std::move(*opt_epoch);

  user_data_auth::GetRecoveryRequestRequest request;

  request.set_auth_session_id(user_context_->GetAuthSessionId());

  const std::string& gaia_id = user_context_->GetGaiaID();
  DCHECK(!gaia_id.empty()) << "Recovery is only supported for gaia users";
  DCHECK(!access_token_.empty());
  const std::string& reauth_proof_token = user_context_->GetReauthProofToken();
  CHECK(!reauth_proof_token.empty()) << "Reauth proof token must be set";

  request.set_requestor_user_id_type(
      user_data_auth::GetRecoveryRequestRequest::GAIA_ID);
  request.set_requestor_user_id(gaia_id);
  request.set_auth_factor_label(kCryptohomeRecoveryKeyLabel);
  request.set_gaia_access_token(access_token_);
  request.set_gaia_reauth_proof_token(reauth_proof_token);
  request.set_epoch_response(epoch->data(), epoch->size());

  user_data_auth_client_->GetRecoveryRequest(
      std::move(request),
      base::BindOnce(&CryptohomeRecoveryPerformer::OnGetRecoveryRequest,
                     weak_factory_.GetWeakPtr(), std::move(epoch)));
}

void CryptohomeRecoveryPerformer::OnGetRecoveryRequest(
    CryptohomeRecoveryEpochResponse epoch,
    absl::optional<user_data_auth::GetRecoveryRequestReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(EVENT) << "Failed to obtain recovery request, error code "
                     << error;
    std::move(callback_).Run(std::move(user_context_),
                             AuthenticationError{error});
    return;
  }

  DCHECK(!reply->recovery_request().empty());
  DCHECK(!access_token_.empty());

  service_client_.FetchRecoveryResponse(
      reply->recovery_request(), GaiaAccessToken(access_token_),
      base::BindOnce(
          &CryptohomeRecoveryPerformer::OnFetchRecoveryServiceResponse,
          weak_factory_.GetWeakPtr(), std::move(epoch)));
}

void CryptohomeRecoveryPerformer::OnFetchRecoveryServiceResponse(
    CryptohomeRecoveryEpochResponse epoch,
    absl::optional<CryptohomeRecoveryResponse> opt_response,
    CryptohomeRecoveryServerStatusCode status) {
  if (status != CryptohomeRecoveryServerStatusCode::kSuccess) {
    std::move(callback_).Run(std::move(user_context_),
                             AuthenticationError(AuthFailure(status)));
    return;
  }
  const CryptohomeRecoveryResponse& response = *opt_response;

  user_data_auth::AuthenticateAuthFactorRequest request;
  request.set_auth_session_id(user_context_->GetAuthSessionId());
  request.set_auth_factor_label(kCryptohomeRecoveryKeyLabel);

  user_data_auth::CryptohomeRecoveryAuthInput* recovery_input =
      request.mutable_auth_input()->mutable_cryptohome_recovery_input();
  recovery_input->set_epoch_response(epoch->data(), epoch->size());
  recovery_input->set_recovery_response(response->data(), response->size());

  user_data_auth_client_->AuthenticateAuthFactor(
      std::move(request),
      base::BindOnce(&CryptohomeRecoveryPerformer::OnAuthenticateAuthFactor,
                     weak_factory_.GetWeakPtr()));
}

void CryptohomeRecoveryPerformer::OnAuthenticateAuthFactor(
    absl::optional<user_data_auth::AuthenticateAuthFactorReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(EVENT)
        << "Failed to authenticate session via recovery factor, error code "
        << error;
    std::move(callback_).Run(std::move(user_context_),
                             AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  if (!base::Contains(reply->authorized_for(),
                      user_data_auth::AUTH_INTENT_DECRYPT)) {
    NOTREACHED() << "Authentication via recovery factor failed to authorize "
                    "for decryption";
    std::move(callback_).Run(
        std::move(user_context_),
        AuthenticationError(AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME));
    return;
  }
  LOGIN_LOG(EVENT) << "Authenticated successfully";
  std::move(callback_).Run(std::move(user_context_), absl::nullopt);
}

}  // namespace ash
