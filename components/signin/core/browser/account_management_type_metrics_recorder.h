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

 private:
  void QueryAccountTypes(IdentityManager& identity_manager);

  void AccountTypeDetermined(const CoreAccountId& account_id);

  base::ScopedObservation<IdentityManager, IdentityManager::Observer>
      observation_{this};

  std::map<CoreAccountId, std::unique_ptr<AccountManagedStatusFinder>>
      status_finders_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_MANAGEMENT_TYPE_METRICS_RECORDER_H_
