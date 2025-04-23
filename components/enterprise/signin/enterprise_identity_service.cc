// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/signin/enterprise_identity_service.h"

#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace enterprise {

namespace {

constexpr int kAccountTypeFetchTimeoutInMs = 20000;

void HandleAccessTokenFetched(
    base::OnceCallback<void(base::expected<signin::AccessTokenInfo,
                                           GoogleServiceAuthError>)> callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo token_info) {
  if (error.state() == GoogleServiceAuthError::NONE) {
    std::move(callback).Run(std::move(token_info));
    return;
  }

  std::move(callback).Run(base::unexpected(std::move(error)));
}

}  // namespace

using GetManagedAccountsCallback =
    EnterpriseIdentityService::GetManagedAccountsCallback;

class EnterpriseIdentityServiceImpl : public EnterpriseIdentityService,
                                      public signin::IdentityManager::Observer {
 public:
  explicit EnterpriseIdentityServiceImpl(
      signin::IdentityManager* identity_manager);

  ~EnterpriseIdentityServiceImpl() override;

  // EnterpriseIdentityService:
  void GetManagedAccountsWithRefreshTokens(
      GetManagedAccountsCallback callback) override;
  void GetManagedAccountsAccessTokens(
      base::OnceCallback<void(std::vector<std::string>)> callback) override;
  void AddObserver(EnterpriseIdentityService::Observer* observer) override;
  void RemoveObserver(EnterpriseIdentityService::Observer* observer) override;

  // signin::IdentityManager::Observer:
  void OnRefreshTokensLoaded() override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

 private:
  // Will invoke `callback` with the subset of `accounts` which end up being
  // identified as managed.
  void GetManagedAccounts(const std::vector<CoreAccountInfo>& accounts,
                          GetManagedAccountsCallback callback);

  void OnAccountTypesIdentified(
      std::unique_ptr<std::vector<
          std::unique_ptr<signin::AccountManagedStatusFinder>>> status_finders,
      GetManagedAccountsCallback callback);

  void OnManagedAccountsIdentified(
      base::OnceCallback<void(std::vector<std::string>)> callback,
      std::vector<CoreAccountInfo> managed_accounts);

  void OnAccessTokensFetched(
      std::unique_ptr<std::vector<std::unique_ptr<signin::AccessTokenFetcher>>>
          token_fetchers,
      base::OnceCallback<void(std::vector<std::string>)> callback,
      std::vector<base::expected<signin::AccessTokenInfo,
                                 GoogleServiceAuthError>> access_token_infos);

  void OnRefreshTokenUpdatedForManagedAccounts(
      std::vector<CoreAccountInfo> accounts);

  bool refresh_tokens_loaded_{false};
  std::vector<base::OnceClosure> pending_requests_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  base::ObserverList<EnterpriseIdentityService::Observer> observers_;

  // Singular status finder used to verify if a single account is managed or
  // not, for the purpose of notifying observers.
  std::unique_ptr<signin::AccountManagedStatusFinder> status_finder_;

  base::WeakPtrFactory<EnterpriseIdentityServiceImpl> weak_factory_{this};
};

// static
std::unique_ptr<EnterpriseIdentityService> EnterpriseIdentityService::Create(
    signin::IdentityManager* identity_manager) {
  return std::make_unique<EnterpriseIdentityServiceImpl>(identity_manager);
}

EnterpriseIdentityServiceImpl::EnterpriseIdentityServiceImpl(
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {
  CHECK(identity_manager_);

  refresh_tokens_loaded_ = identity_manager_->AreRefreshTokensLoaded();
  identity_manager_->AddObserver(this);
}

EnterpriseIdentityServiceImpl::~EnterpriseIdentityServiceImpl() {
  identity_manager_->RemoveObserver(this);
}

void EnterpriseIdentityServiceImpl::GetManagedAccountsWithRefreshTokens(
    GetManagedAccountsCallback callback) {
  if (!refresh_tokens_loaded_) {
    pending_requests_.push_back(base::BindOnce(
        &EnterpriseIdentityServiceImpl::GetManagedAccountsWithRefreshTokens,
        weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  GetManagedAccounts(identity_manager_->GetAccountsWithRefreshTokens(),
                     std::move(callback));
}

void EnterpriseIdentityServiceImpl::GetManagedAccountsAccessTokens(
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  GetManagedAccountsWithRefreshTokens(base::BindOnce(
      &EnterpriseIdentityServiceImpl::OnManagedAccountsIdentified,
      weak_factory_.GetWeakPtr(), std::move(callback)));
}

void EnterpriseIdentityServiceImpl::AddObserver(
    EnterpriseIdentityService::Observer* observer) {
  observers_.AddObserver(observer);
}

void EnterpriseIdentityServiceImpl::RemoveObserver(
    EnterpriseIdentityService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void EnterpriseIdentityServiceImpl::OnRefreshTokensLoaded() {
  if (refresh_tokens_loaded_) {
    // No-op.
    return;
  }

  refresh_tokens_loaded_ = true;

  // Resume all pending requests.
  for (auto& closure : pending_requests_) {
    std::move(closure).Run();
  }

  pending_requests_.clear();
}

void EnterpriseIdentityServiceImpl::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  if (!refresh_tokens_loaded_) {
    return;
  }

  // Verify if `account_info` represents a managed account. If so, notify
  // observers.
  GetManagedAccounts(std::vector<CoreAccountInfo>{account_info},
                     base::BindOnce(&EnterpriseIdentityServiceImpl::
                                        OnRefreshTokenUpdatedForManagedAccounts,
                                    weak_factory_.GetWeakPtr()));
}

void EnterpriseIdentityServiceImpl::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  // Needs to be shutdown before IdentityManager.
  NOTREACHED(base::NotFatalUntil::M142);
}

void EnterpriseIdentityServiceImpl::GetManagedAccounts(
    const std::vector<CoreAccountInfo>& accounts,
    GetManagedAccountsCallback callback) {
  if (accounts.empty()) {
    // No signed-in accounts.
    std::move(callback).Run(std::vector<CoreAccountInfo>());
    return;
  }

  // Using a unique pointer of a vector, as we'll need to fill-in the vector
  // beyond transferring its ownership to the barrier closure.
  auto status_finders = std::make_unique<
      std::vector<std::unique_ptr<signin::AccountManagedStatusFinder>>>();
  auto* status_finders_ptr = status_finders.get();
  auto barrier_closure = base::BarrierClosure(
      accounts.size(),
      base::BindOnce(&EnterpriseIdentityServiceImpl::OnAccountTypesIdentified,
                     weak_factory_.GetWeakPtr(), std::move(status_finders),
                     std::move(callback)));
  for (const auto& account : accounts) {
    status_finders_ptr->push_back(
        std::make_unique<signin::AccountManagedStatusFinder>(
            identity_manager_, account, barrier_closure,
            base::Milliseconds(kAccountTypeFetchTimeoutInMs)));

    // If the account was resolved synchronously, `barrier_closure` needs to be
    // invoked manually.
    if (status_finders_ptr->back()->GetOutcome() !=
        signin::AccountManagedStatusFinder::Outcome::kPending) {
      barrier_closure.Run();
    }
  }
}

void EnterpriseIdentityServiceImpl::OnAccountTypesIdentified(
    std::unique_ptr<std::vector<
        std::unique_ptr<signin::AccountManagedStatusFinder>>> status_finders,
    GetManagedAccountsCallback callback) {
  std::vector<CoreAccountInfo> managed_accounts;
  if (!status_finders) {
    std::move(callback).Run(managed_accounts);
    return;
  }

  for (const auto& status_finder : *status_finders) {
    if (!status_finder) {
      continue;
    }

    // Listing out all enum values to enforce all values are evaluated at
    // compile-time.
    switch (status_finder->GetOutcome()) {
      case signin::AccountManagedStatusFinder::Outcome::kEnterpriseGoogleDotCom:
      case signin::AccountManagedStatusFinder::Outcome::kEnterprise:
        managed_accounts.push_back(status_finder->GetAccountInfo());
        break;
      case signin::AccountManagedStatusFinder::Outcome::kConsumerGmail:
      case signin::AccountManagedStatusFinder::Outcome::kConsumerWellKnown:
      case signin::AccountManagedStatusFinder::Outcome::kConsumerNotWellKnown:
      case signin::AccountManagedStatusFinder::Outcome::kPending:
      case signin::AccountManagedStatusFinder::Outcome::kError:
      case signin::AccountManagedStatusFinder::Outcome::kTimeout:
        continue;
    }
  }

  std::move(callback).Run(managed_accounts);
}

void EnterpriseIdentityServiceImpl::OnManagedAccountsIdentified(
    base::OnceCallback<void(std::vector<std::string>)> callback,
    std::vector<CoreAccountInfo> managed_accounts) {
  if (managed_accounts.empty()) {
    std::move(callback).Run(std::vector<std::string>());
    return;
  }

  // Have to bind the token fetchers to the barrier callback to ensure they
  // remain alive while tokens are still being fetched.
  auto token_fetchers = std::make_unique<
      std::vector<std::unique_ptr<signin::AccessTokenFetcher>>>();
  auto* token_fetchers_ptr = token_fetchers.get();
  auto barrier_callback = base::BarrierCallback<
      base::expected<signin::AccessTokenInfo, GoogleServiceAuthError>>(
      managed_accounts.size(),
      base::BindOnce(&EnterpriseIdentityServiceImpl::OnAccessTokensFetched,
                     weak_factory_.GetWeakPtr(), std::move(token_fetchers),
                     std::move(callback)));

  for (const auto& account : managed_accounts) {
    token_fetchers_ptr->push_back(
        identity_manager_->CreateAccessTokenFetcherForAccount(
            account.account_id, "cloud_policy",
            signin::ScopeSet{GaiaConstants::kDeviceManagementServiceOAuth},
            base::BindOnce(HandleAccessTokenFetched, barrier_callback),
            signin::AccessTokenFetcher::Mode::kImmediate));
  }
}

void EnterpriseIdentityServiceImpl::OnAccessTokensFetched(
    std::unique_ptr<std::vector<std::unique_ptr<signin::AccessTokenFetcher>>>
        token_fetchers,
    base::OnceCallback<void(std::vector<std::string>)> callback,
    std::vector<base::expected<signin::AccessTokenInfo, GoogleServiceAuthError>>
        access_token_infos) {
  std::vector<std::string> access_tokens;
  for (const auto& access_token_info : access_token_infos) {
    if (access_token_info.has_value()) {
      access_tokens.push_back(access_token_info->token);
    }
  }
  std::move(callback).Run(access_tokens);
}

void EnterpriseIdentityServiceImpl::OnRefreshTokenUpdatedForManagedAccounts(
    std::vector<CoreAccountInfo> accounts) {
  if (accounts.empty()) {
    // Refresh tokens were updated for a consumer user, so no-op.
    return;
  }

  // Notify observers.
  observers_.Notify(
      &EnterpriseIdentityService::Observer::OnManagedAccountSessionChanged);
}

}  // namespace enterprise
