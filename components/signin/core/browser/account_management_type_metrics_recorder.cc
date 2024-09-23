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

}  // namespace

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

void AccountManagementTypeMetricsRecorder::QueryAccountTypes(
    IdentityManager& identity_manager) {
  CHECK(identity_manager.AreRefreshTokensLoaded());
  std::vector<CoreAccountInfo> accounts =
      identity_manager.GetAccountsWithRefreshTokens();
  for (const CoreAccountInfo& account : accounts) {
    // base::Unretained() is safe because `this` owns the `status_finder`.
    auto status_finder = std::make_unique<AccountManagedStatusFinder>(
        &identity_manager, account,
        base::BindOnce(
            &AccountManagementTypeMetricsRecorder::AccountTypeDetermined,
            base::Unretained(this), account.account_id));
    if (status_finder->GetOutcome() !=
        AccountManagedStatusFinder::Outcome::kPending) {
      RecordAccountTypeMetrics(status_finder->GetOutcome());
    } else {
      status_finders_[account.account_id] = std::move(status_finder);
    }
  }
}

void AccountManagementTypeMetricsRecorder::AccountTypeDetermined(
    const CoreAccountId& account_id) {
  auto it = status_finders_.find(account_id);
  CHECK(it != status_finders_.end());
  RecordAccountTypeMetrics(it->second->GetOutcome());
  status_finders_.erase(it);
}

}  // namespace signin
