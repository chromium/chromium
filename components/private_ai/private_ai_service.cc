// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/private_ai_service.h"

#include "base/sequence_checker.h"
#include "components/private_ai/client.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/features.h"
#include "components/private_ai/phosphor/blind_sign_auth_factory.h"
#include "components/private_ai/phosphor/token_fetcher_impl.h"
#include "components/private_ai/phosphor/token_manager_impl.h"
#include "components/private_ai/private_ai_network_driver.h"
#include "components/private_ai/private_ai_oak_session_driver.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/version_info/channel.h"
#include "google_apis/google_api_keys.h"

namespace private_ai {

// static
std::string PrivateAiService::GetApiKey(version_info::Channel channel) {
  std::string api_key = kPrivateAiApiKey.Get();
  if (api_key.empty() && google_apis::IsGoogleChromeAPIKeyUsed()) {
    return google_apis::GetAPIKey(channel);
  }
  return api_key;
}

// static
bool PrivateAiService::CanPrivateAiBeEnabled(version_info::Channel channel) {
  return base::FeatureList::IsEnabled(kPrivateAi) &&
         !GetApiKey(channel).empty();
}

PrivateAiService::PrivateAiService(
    signin::IdentityManager* identity_manager,
    phosphor::BlindSignAuthFactory* bsa_factory,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<PrivateAiNetworkDriver> network_driver,
    std::unique_ptr<PrivateAiOakSessionDriver> oak_session_driver,
    network::mojom::NetworkContext* network_context,
    const std::string& url,
    const std::string& api_key,
    const std::string& proxy_url,
    bool use_token_attestation,
    version_info::Channel channel)
    : identity_manager_(identity_manager),
      network_driver_(std::move(network_driver)),
      oak_session_driver_(std::move(oak_session_driver)) {
  CHECK(identity_manager_);
  CHECK(bsa_factory);
  CHECK(url_loader_factory);
  CHECK(network_driver_);
  CHECK(oak_session_driver_);
  identity_manager_->AddObserver(this);

  auto bsa = bsa_factory->CreateBlindSignAuth(url_loader_factory->Clone());
  auto token_fetcher = std::make_unique<phosphor::TokenFetcherImpl>(
      this, std::move(bsa), &logger_);
  token_fetcher_ = token_fetcher.get();
  token_manager_ = std::make_unique<phosphor::TokenManagerImpl>(
      std::move(token_fetcher), &logger_);

  client_ =
      Client::Create(url, api_key, proxy_url, use_token_attestation,
                     network_context, GetTokenManager(), GetLogger(),
                     oak_session_driver_.get(), network_driver_.get(), channel);
}

PrivateAiService::~PrivateAiService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PrivateAiService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  is_shutting_down_ = true;
  identity_manager_->RemoveObserver(this);
}

phosphor::TokenManager* PrivateAiService::GetTokenManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return token_manager_.get();
}

Client* PrivateAiService::GetClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(client_);
  return client_.get();
}

PrivateAiLogger* PrivateAiService::GetLogger() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return &logger_;
}

void PrivateAiService::SetClientForTesting(std::unique_ptr<Client> client) {
  client_ = std::move(client);
}

bool PrivateAiService::IsTokenFetchEnabled() {
  CHECK(identity_manager_);
  if (is_shutting_down_ ||
      !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return false;
  }
  return true;
}

void PrivateAiService::RequestOAuthToken(RequestOAuthTokenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsTokenFetchEnabled()) {
    std::move(callback).Run(phosphor::GetAuthnTokensResult::kFailedNoAccount,
                            std::nullopt);
    return;
  }

  auto oauth_token_fetcher =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          signin::OAuthConsumerId::kPrivateAiService, identity_manager_,
          signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
          signin::ConsentLevel::kSignin);

  auto* fetcher_ptr = oauth_token_fetcher.get();
  fetcher_ptr->Start(
      base::BindOnce(&PrivateAiService::OnRequestOAuthTokenCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(oauth_token_fetcher), std::move(callback)));
}

void PrivateAiService::OnRequestOAuthTokenCompleted(
    std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> fetcher,
    RequestOAuthTokenCallback callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    LOG(ERROR) << "OAuth token fetch failed with error: " << error.ToString();
    phosphor::GetAuthnTokensResult result =
        error.IsTransientError()
            ? phosphor::GetAuthnTokensResult::kFailedOAuthTokenTransient
            : phosphor::GetAuthnTokensResult::kFailedOAuthTokenPersistent;
    std::move(callback).Run(result, std::nullopt);
    return;
  }
  std::move(callback).Run(phosphor::GetAuthnTokensResult::kSuccess,
                          access_token_info.token);
}

void PrivateAiService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (token_fetcher_) {
    bool account_available =
        event.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
        signin::PrimaryAccountChangeEvent::Type::kCleared;
    token_fetcher_->OnAccountStatusChanged(account_available);
    token_manager_->OnAccountStatusChanged(account_available);
  }
}

void PrivateAiService::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (token_fetcher_ &&
      account_info.account_id == identity_manager_->GetPrimaryAccountId(
                                     signin::ConsentLevel::kSignin)) {
    bool account_available = (error.state() == GoogleServiceAuthError::NONE);
    token_fetcher_->OnAccountStatusChanged(account_available);
    token_manager_->OnAccountStatusChanged(account_available);
  }
}

}  // namespace private_ai
