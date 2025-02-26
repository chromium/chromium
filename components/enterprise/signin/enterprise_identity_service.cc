// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/signin/enterprise_identity_service.h"

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace enterprise {

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

  // signin::IdentityManager::Observer:
  void OnRefreshTokensLoaded() override;

 private:
  void OnAccountTypesIdentified(
      std::unique_ptr<std::vector<
          std::unique_ptr<signin::AccountManagedStatusFinder>>> status_finders,
      GetManagedAccountsCallback callback);

  bool refresh_tokens_loaded_{false};
  std::vector<base::OnceClosure> pending_requests_;
  const raw_ptr<signin::IdentityManager> identity_manager_;

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

  const auto& accounts = identity_manager_->GetAccountsWithRefreshTokens();

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
            identity_manager_, account, barrier_closure));

    // If the account was resolved synchronously, `barrier_closure` needs to be
    // invoked manually.
    if (status_finders_ptr->back()->GetOutcome() !=
        signin::AccountManagedStatusFinder::Outcome::kPending) {
      barrier_closure.Run();
    }
  }
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

}  // namespace enterprise
