// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_management_type_metrics_recorder.h"

#include "base/metrics/histogram_functions.h"

namespace signin {

namespace {

void RecordAccountTypeMetrics(AccountManagedStatusFinder::Outcome outcome) {
  base::UmaHistogramEnumeration("Signin.AccountManagementType", outcome);
}

void RecordAccountTypeSummaryMetrics(size_t num_consumer_accounts,
                                     size_t num_enterprise_accounts) {
  base::UmaHistogramEnumeration(
      "Signin.AccountManagementTypesSummary",
      AccountManagementTypeMetricsRecorder::GetAccountTypesSummary(
          num_consumer_accounts, num_enterprise_accounts));
}

}  // namespace

AccountManagementTypeMetricsRecorder::AccountManagementTypesSummary
AccountManagementTypeMetricsRecorder::GetAccountTypesSummary(
    size_t num_consumer_accounts,
    size_t num_enterprise_accounts) {
  switch (num_consumer_accounts) {
    case 0:
      switch (num_enterprise_accounts) {
        case 0:
          return AccountManagementTypesSummary::k0Consumer0Enterprise;
        case 1:
          return AccountManagementTypesSummary::k0Consumer1Enterprise;
        default:  // 2 or more.
          return AccountManagementTypesSummary::k0Consumer2plusEnterprise;
      }
    case 1:
      switch (num_enterprise_accounts) {
        case 0:
          return AccountManagementTypesSummary::k1Consumer0Enterprise;
        case 1:
          return AccountManagementTypesSummary::k1Consumer1Enterprise;
        default:  // 2 or more.
          return AccountManagementTypesSummary::k1Consumer2plusEnterprise;
      }
    default:  // 2 or more.
      switch (num_enterprise_accounts) {
        case 0:
          return AccountManagementTypesSummary::k2plusConsumer0Enterprise;
        case 1:
          return AccountManagementTypesSummary::k2plusConsumer1Enterprise;
        default:  // 2 or more.
          return AccountManagementTypesSummary::k2plusConsumer2plusEnterprise;
      }
  }
}

AccountManagementTypeMetricsRecorder::AccountManagementTypeMetricsRecorder(
    IdentityManager& identity_manager) {
  if (identity_manager.AreRefreshTokensLoaded()) {
    QueryAccountTypes(identity_manager);
  } else {
    observation_.Observe(&identity_manager);
  }
}

AccountManagementTypeMetricsRecorder::~AccountManagementTypeMetricsRecorder() =
    default;

void AccountManagementTypeMetricsRecorder::OnRefreshTokensLoaded() {
  QueryAccountTypes(*observation_.GetSource());
  observation_.Reset();
}

void AccountManagementTypeMetricsRecorder::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  observation_.Reset();
}

void AccountManagementTypeMetricsRecorder::QueryAccountTypes(
    IdentityManager& identity_manager) {
  CHECK(identity_manager.AreRefreshTokensLoaded());
  std::vector<CoreAccountInfo> accounts =
      identity_manager.GetAccountsWithRefreshTokens();
  num_pending_checks_ = accounts.size();
  for (const CoreAccountInfo& account : accounts) {
    // base::Unretained() is safe because `this` owns the `status_finder`.
    status_finders_[account.account_id] =
        std::make_unique<AccountManagedStatusFinder>(
            &identity_manager, account,
            base::BindOnce(
                &AccountManagementTypeMetricsRecorder::AccountTypeDetermined,
                base::Unretained(this), account.account_id));
    // If the type of account is known synchronously, the finder won't call us
    // back, so do it manually.
    if (status_finders_[account.account_id]->GetOutcome() !=
        AccountManagedStatusFinder::Outcome::kPending) {
      AccountTypeDetermined(account.account_id);
    }
  }
}

void AccountManagementTypeMetricsRecorder::AccountTypeDetermined(
    const CoreAccountId& account_id) {
  auto it = status_finders_.find(account_id);
  CHECK(it != status_finders_.end());

  auto outcome = it->second->GetOutcome();
  status_finders_.erase(it);
  --num_pending_checks_;

  // Record per-account metrics.
  RecordAccountTypeMetrics(outcome);

  // Update account-type counters.
  switch (outcome) {
    case AccountManagedStatusFinder::Outcome::kPending:
    case AccountManagedStatusFinder::Outcome::kError:
    case AccountManagedStatusFinder::Outcome::kTimeout:
      // Error cases are ignored for the summary metrics.
      break;
    case AccountManagedStatusFinder::Outcome::kConsumerGmail:
    case AccountManagedStatusFinder::Outcome::kConsumerWellKnown:
    case AccountManagedStatusFinder::Outcome::kConsumerNotWellKnown:
      ++num_consumer_accounts_;
      break;
    case AccountManagedStatusFinder::Outcome::kEnterpriseGoogleDotCom:
    case AccountManagedStatusFinder::Outcome::kEnterprise:
      ++num_enterprise_accounts_;
      break;
  }

  // If all account type queries have been completed, record summary metrics.
  if (num_pending_checks_ == 0) {
    RecordAccountTypeSummaryMetrics(num_consumer_accounts_,
                                    num_enterprise_accounts_);
  }
}

}  // namespace signin
