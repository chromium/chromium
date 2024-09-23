// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/recovery/cryptohome_recovery_performer.h"

#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "chromeos/ash/components/cryptohome/userdataauth_util.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
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
namespace {
AuthEventsRecorder::CryptohomeRecoveryResult
GetRecoveryResultFromCryptohomeError(
    user_data_auth::CryptohomeErrorCode error) {
  switch (error) {
    case user_data_auth::CRYPTOHOME_ERROR_RECOVERY_TRANSIENT:
      return AuthEventsRecorder::CryptohomeRecoveryResult::
          kRecoveryTransientError;
    case user_data_auth::CRYPTOHOME_ERROR_RECOVERY_FATAL:
      return AuthEventsRecorder::CryptohomeRecoveryResult::kRecoveryFatalError;
    default:
      return AuthEventsRecorder::CryptohomeRecoveryResult::
          kAuthenticateRecoveryFactorError;
  }
}
}  // namespace

CryptohomeRecoveryPerformer::CryptohomeRecoveryPerformer(
    UserDataAuthClient* user_data_auth_client,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : auth_performer_(std::make_unique<AuthPerformer>(user_data_auth_client)),
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

  timer_ = std::make_unique<base::ElapsedTimer>();
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
  RecordRecoveryResult(
      AuthEventsRecorder::CryptohomeRecoveryResult::kOAuthTokenFetchError);
  std::move(callback_).Run(
      std::move(user_context_),
      AuthenticationError{AuthFailure::CRYPTOHOME_RECOVERY_OAUTH_TOKEN_ERROR});
}

std::string CryptohomeRecoveryPerformer::GetConsumerName() const {
  return "cryptohome_recovery_performer";
}

void CryptohomeRecoveryPerformer::OnNetworkFetchEpoch(
    std::optional<CryptohomeRecoveryEpochResponse> opt_epoch,
    CryptohomeRecoveryServerStatusCode status) {
  if (status != CryptohomeRecoveryServerStatusCode::kSuccess) {
    RecordRecoveryResult(
        AuthEventsRecorder::CryptohomeRecoveryResult::kEpochFetchError);
    std::move(callback_).Run(std::move(user_context_),
                             AuthenticationError(AuthFailure(status)));
    return;
  }
  CryptohomeRecoveryEpochResponse epoch = std::move(*opt_epoch);

  auth_performer_->GetRecoveryRequest(
      access_token_, epoch, std::move(user_context_),
      base::BindOnce(&CryptohomeRecoveryPerformer::OnGetRecoveryRequest,
                     weak_factory_.GetWeakPtr(), epoch));
}

void CryptohomeRecoveryPerformer::OnGetRecoveryRequest(
    CryptohomeRecoveryEpochResponse epoch,
    std::optional<RecoveryRequest> recovery_request,
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    LOGIN_LOG(EVENT) << "Failed to obtain recovery request, error code "
                     << error->get_cryptohome_code();
    RecordRecoveryResult(
        AuthEventsRecorder::CryptohomeRecoveryResult::kGetRecoveryRequestError);
    std::move(callback_).Run(std::move(context), std::move(error));
    return;
  }
  user_context_ = std::move(context);
  CHECK(!access_token_.empty());
  CHECK(recovery_request.has_value());
  CHECK(!recovery_request.value()->empty());

  service_client_.FetchRecoveryResponse(
      recovery_request.value().value(), GaiaAccessToken(access_token_),
      base::BindOnce(
          &CryptohomeRecoveryPerformer::OnFetchRecoveryServiceResponse,
          weak_factory_.GetWeakPtr(), std::move(epoch)));
}

void CryptohomeRecoveryPerformer::OnFetchRecoveryServiceResponse(
    CryptohomeRecoveryEpochResponse epoch,
    std::optional<CryptohomeRecoveryResponse> opt_response,
    CryptohomeRecoveryServerStatusCode status) {
  if (status != CryptohomeRecoveryServerStatusCode::kSuccess) {
    RecordRecoveryResult(AuthEventsRecorder::CryptohomeRecoveryResult::
                             kRecoveryResponseFetchError);
    std::move(callback_).Run(std::move(user_context_),
                             AuthenticationError(AuthFailure(status)));
    return;
  }
  const CryptohomeRecoveryResponse& response = *opt_response;

  auth_performer_->AuthenticateWithRecovery(
      epoch, response, RecoveryLedgerName(GetRecoveryLedgerName()),
      RecoveryLedgerPubKey(GetRecoveryLedgerPublicKey()),
      GetRecoveryLedgerPublicKeyHash(), std::move(user_context_),
      base::BindOnce(&CryptohomeRecoveryPerformer::OnAuthenticateWithRecovery,
                     weak_factory_.GetWeakPtr()));
}

void CryptohomeRecoveryPerformer::OnAuthenticateWithRecovery(
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    RecordRecoveryResult(
        GetRecoveryResultFromCryptohomeError(error->get_cryptohome_code()));
    std::move(callback_).Run(std::move(context), std::move(error));
    return;
  }
  if (!context->GetAuthorizedIntents().Has(AuthSessionIntent::kDecrypt)) {
    NOTREACHED_IN_MIGRATION()
        << "Authentication via recovery factor failed to authorize "
           "for decryption";
    RecordRecoveryResult(
        AuthEventsRecorder::CryptohomeRecoveryResult::kMountCryptohomeError);
    std::move(callback_).Run(
        std::move(context),
        AuthenticationError(AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME));
    return;
  }
  LOGIN_LOG(EVENT) << "Authenticated successfully";
  RecordRecoveryResult(
      AuthEventsRecorder::CryptohomeRecoveryResult::kSucceeded);
  std::move(callback_).Run(std::move(context), std::nullopt);
}

void CryptohomeRecoveryPerformer::RecordRecoveryResult(
    AuthEventsRecorder::CryptohomeRecoveryResult result) {
  AuthEventsRecorder::Get()->OnRecoveryDone(result, timer_->Elapsed());
  timer_.reset();
}

}  // namespace ash
