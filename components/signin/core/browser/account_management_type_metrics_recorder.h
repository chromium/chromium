// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_MANAGEMENT_TYPE_METRICS_RECORDER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_MANAGEMENT_TYPE_METRICS_RECORDER_H_

#include <map>
#include <optional>
#include <string>

#include "base/scoped_observation.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace signin {

// A helper class that records metrics about the "management type" for each
// account in the identity manager, i.e. whether the account is a managed
// (aka enterprise) account or a consumer account, plus some additional details.
class AccountManagementTypeMetricsRecorder : public IdentityManager::Observer {
 public:
  // `identity_manager` must outlive `this`.
  explicit AccountManagementTypeMetricsRecorder(
      IdentityManager& identity_manager);
  ~AccountManagementTypeMetricsRecorder() override;

  // IdentityManager::Observer:
  void OnRefreshTokensLoaded() override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

  // LINT.IfChange(AccountManagementTypesSummary)
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class AccountManagementTypesSummary {
    k0Consumer0Enterprise = 0,
    k0Consumer1Enterprise = 1,
    k0Consumer2plusEnterprise = 2,
    k1Consumer0Enterprise = 3,
    k1Consumer1Enterprise = 4,
    k1Consumer2plusEnterprise = 5,
    k2plusConsumer0Enterprise = 6,
    k2plusConsumer1Enterprise = 7,
    k2plusConsumer2plusEnterprise = 8,
    kMaxValue = k2plusConsumer2plusEnterprise
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:AccountManagementTypesSummary)

  static AccountManagementTypesSummary GetAccountTypesSummary(
      size_t num_consumer_accounts,
      size_t num_enterprise_accounts);

 private:
  void QueryAccountTypes(IdentityManager& identity_manager);

  void AccountTypeDetermined(const CoreAccountId& account_id);

  base::ScopedObservation<IdentityManager, IdentityManager::Observer>
      observation_{this};

  std::map<CoreAccountId, std::unique_ptr<AccountManagedStatusFinder>>
      status_finders_;

  // Counters for how many consumer and enterprise accounts there are in total.
  // Once all account types have been determined, a "summary" metric gets
  // logged.
  size_t num_pending_checks_ = 0;
  size_t num_consumer_accounts_ = 0;
  size_t num_enterprise_accounts_ = 0;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_MANAGEMENT_TYPE_METRICS_RECORDER_H_
