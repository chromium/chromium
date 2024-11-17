// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_ACCOUNT_INFO_GETTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_ACCOUNT_INFO_GETTER_H_

#include "components/signin/public/identity_manager/account_info.h"

namespace autofill {

// Interface to get account information in Autofill.
class AccountInfoGetter {
 public:
  // Returns the account info that should be used when communicating with the
  // Payments server. The AccountInfo could be empty if there is no account to
  // be used by the Payments server.
  // TODO(crbug.com/40254942): Make it return std::optional.
  virtual CoreAccountInfo GetAccountInfoForPaymentsServer() const = 0;

  // Returns true - When user is both signed-in and enabled sync.
  // Returns false - When user is not signed-in or does not have sync the
  // feature enabled. This value should be exclusively used for metrics only
  // or in the communication with the payments server, if this communication
  // only influences metrics.
  // TODO(crbug.com/40066949): Simplify once ConsentLevel::kSync and
  // SyncService::IsSyncFeatureEnabled() are deleted from the codebase.
  virtual bool IsSyncFeatureEnabledForPaymentsServerMetrics() const = 0;

 protected:
  virtual ~AccountInfoGetter() = default;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_ACCOUNT_INFO_GETTER_H_
