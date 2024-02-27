// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/drivefs_auth.h"

#include "base/functional/bind.h"
#include "components/account_id/account_id.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace drivefs {

namespace {
constexpr char kIdentityConsumerId[] = "drivefs";
}  // namespace

DriveFsAuth::DriveFsAuth(const base::Clock* clock,
                         const base::FilePath& profile_path,
                         std::unique_ptr<base::OneShotTimer> timer,
                         Delegate* delegate)
    : clock_(clock),
      profile_path_(profile_path),
      timer_(std::move(timer)),
      delegate_(delegate) {}

DriveFsAuth::~DriveFsAuth() = default;

std::optional<std::string> DriveFsAuth::GetCachedAccessToken() {
  const auto& token = GetOrResetCachedToken(true);
  if (token.empty()) {
    return std::nullopt;
  }
  return token;
}

void DriveFsAuth::GetAccessToken(bool use_cached,
                                 AccessTokenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (get_access_token_callback_) {
    std::move(callback).Run(mojom::AccessTokenStatus::kTransientError,
                            mojom::AccessToken::New());
    return;
  }

  const std::string& token = GetOrResetCachedToken(use_cached);
  if (!token.empty()) {
    std::move(callback).Run(mojom::AccessTokenStatus::kSuccess,
                            mojom::AccessToken::New(token, last_token_expiry_));
    return;
  }

  signin::IdentityManager* identity_manager = delegate_->GetIdentityManager();
  if (!identity_manager) {
    std::move(callback).Run(mojom::AccessTokenStatus::kAuthError,
                            mojom::AccessToken::New());
    return;
  }
  get_access_token_callback_ = std::move(callback);
  // Timer is cancelled when it is destroyed, so use base::Unretained().
  timer_->Start(
      FROM_HERE, base::Seconds(30),
      base::BindOnce(&DriveFsAuth::AuthTimeout, base::Unretained(this)));
  std::set<std::string> scopes(
      {GaiaConstants::kDriveOAuth2Scope,
       GaiaConstants::kExperimentsAndConfigsOAuth2Scope});
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          kIdentityConsumerId, identity_manager, scopes,
          base::BindOnce(&DriveFsAuth::GotChromeAccessToken,
                         base::Unretained(this)),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
          signin::ConsentLevel::kSignin);
}

void DriveFsAuth::GotChromeAccessToken(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_->Stop();
  if (error.state() != GoogleServiceAuthError::NONE) {
    std::move(get_access_token_callback_)
        .Run(error.IsPersistentError()
                 ? mojom::AccessTokenStatus::kAuthError
                 : mojom::AccessTokenStatus::kTransientError,
             mojom::AccessToken::New());
    return;
  }
  UpdateCachedToken(access_token_info.token, access_token_info.expiration_time);
  std::move(get_access_token_callback_)
      .Run(mojom::AccessTokenStatus::kSuccess,
           mojom::AccessToken::New(access_token_info.token,
                                   access_token_info.expiration_time));
}

const std::string& DriveFsAuth::GetOrResetCachedToken(bool use_cached) {
  if (!use_cached || clock_->Now() >= last_token_expiry_) {
    last_token_.clear();
  }
  return last_token_;
}

void DriveFsAuth::UpdateCachedToken(const std::string& token,
                                    base::Time expiry) {
  last_token_ = token;
  last_token_expiry_ = expiry;
}

void DriveFsAuth::AuthTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  access_token_fetcher_.reset();
  std::move(get_access_token_callback_)
      .Run(mojom::AccessTokenStatus::kTransientError,
           mojom::AccessToken::New());
}

}  // namespace drivefs
