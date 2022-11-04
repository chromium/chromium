// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/recovery/cryptohome_recovery_performer.h"
#include "base/check.h"
#include "chromeos/ash/components/cryptohome/userdataauth_util.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/login/auth/recovery/cryptohome_recovery_service_client.h"
#include "chromeos/ash/components/login/auth/recovery/service_constants.h"
#include "components/device_event_log/device_event_log.h"

namespace ash {

CryptohomeRecoveryPerformer::CryptohomeRecoveryPerformer(
    UserDataAuthClient* user_data_auth_client,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : user_data_auth_client_(user_data_auth_client),
      service_client_(std::move(url_loader_factory)) {}

CryptohomeRecoveryPerformer::~CryptohomeRecoveryPerformer() = default;

void CryptohomeRecoveryPerformer::AuthenticateWithRecovery(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  DCHECK(!context->GetAuthSessionId().empty());

  LOGIN_LOG(EVENT) << "Authenticating with recovery";

  const std::string& access_token{context->GetAccessToken()};
  DCHECK(!access_token.empty()) << "Gaia access token must be set for recovery";
  service_client_.FetchEpoch(
      GaiaAccessToken(access_token),
      base::BindOnce(&CryptohomeRecoveryPerformer::OnNetworkFetchEpoch,
                     weak_factory_.GetWeakPtr(), std::move(context),
                     std::move(callback)));
}

void CryptohomeRecoveryPerformer::OnNetworkFetchEpoch(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<CryptohomeRecoveryEpochResponse> opt_epoch,
    CryptohomeRecoveryServerStatusCode status) {
  if (status != CryptohomeRecoveryServerStatusCode::kSuccess) {
    std::move(callback).Run(std::move(context),
                            AuthenticationError(AuthFailure(status)));
    return;
  }
  CryptohomeRecoveryEpochResponse epoch = std::move(*opt_epoch);

  user_data_auth::GetRecoveryRequestRequest request;

  request.set_auth_session_id(context->GetAuthSessionId());

  const std::string& gaia_id = context->GetGaiaID();
  DCHECK(!gaia_id.empty()) << "Recovery is only supported for gaia users";
  const std::string& access_token = context->GetAccessToken();
  DCHECK(!access_token.empty());
  const std::string& reauth_proof_token = context->GetReauthProofToken();
  CHECK(!reauth_proof_token.empty()) << "Reauth proof token must be set";

  request.set_requestor_user_id_type(
      user_data_auth::GetRecoveryRequestRequest::GAIA_ID);
  request.set_requestor_user_id(gaia_id);
  request.set_auth_factor_label(kCryptohomeRecoveryKeyLabel);
  request.set_gaia_access_token(access_token);
  request.set_gaia_reauth_proof_token(reauth_proof_token);
  request.set_epoch_response(epoch->data(), epoch->size());

  user_data_auth_client_->GetRecoveryRequest(
      std::move(request),
      base::BindOnce(&CryptohomeRecoveryPerformer::OnGetRecoveryRequest,
                     weak_factory_.GetWeakPtr(), std::move(context),
                     std::move(callback), std::move(epoch)));
}

void CryptohomeRecoveryPerformer::OnGetRecoveryRequest(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    CryptohomeRecoveryEpochResponse epoch,
    absl::optional<user_data_auth::GetRecoveryRequestReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(EVENT) << "Failed to obtain recovery request, error code "
                     << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }

  DCHECK(!reply->recovery_request().empty());
  const std::string& access_token = context->GetAccessToken();
  DCHECK(!access_token.empty());

  service_client_.FetchRecoveryResponse(
      reply->recovery_request(), GaiaAccessToken(access_token),
      base::BindOnce(
          &CryptohomeRecoveryPerformer::OnFetchRecoveryServiceResponse,
          weak_factory_.GetWeakPtr(), std::move(context), std::move(callback),
          std::move(epoch)));
}

void CryptohomeRecoveryPerformer::OnFetchRecoveryServiceResponse(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    CryptohomeRecoveryEpochResponse epoch,
    absl::optional<CryptohomeRecoveryResponse> opt_response,
    CryptohomeRecoveryServerStatusCode status) {
  if (status != CryptohomeRecoveryServerStatusCode::kSuccess) {
    std::move(callback).Run(std::move(context),
                            AuthenticationError(AuthFailure(status)));
    return;
  }
  const CryptohomeRecoveryResponse& response = *opt_response;

  user_data_auth::AuthenticateAuthFactorRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());
  request.set_auth_factor_label(kCryptohomeRecoveryKeyLabel);

  user_data_auth::CryptohomeRecoveryAuthInput* recovery_input =
      request.mutable_auth_input()->mutable_cryptohome_recovery_input();
  recovery_input->set_epoch_response(epoch->data(), epoch->size());
  recovery_input->set_recovery_response(response->data(), response->size());

  user_data_auth_client_->AuthenticateAuthFactor(
      std::move(request),
      base::BindOnce(&CryptohomeRecoveryPerformer::OnAuthenticateAuthFactor,
                     weak_factory_.GetWeakPtr(), std::move(context),
                     std::move(callback)));
}

void CryptohomeRecoveryPerformer::OnAuthenticateAuthFactor(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<user_data_auth::AuthenticateAuthFactorReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(EVENT)
        << "Failed to authenticate session via recovery factor, error code "
        << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  DCHECK(reply->authenticated());
  LOGIN_LOG(EVENT) << "Authenticated successfully";
  std::move(callback).Run(std::move(context), absl::nullopt);
}

}  // namespace ash
