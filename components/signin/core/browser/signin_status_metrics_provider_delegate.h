// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_STATUS_METRICS_PROVIDER_DELEGATE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_STATUS_METRICS_PROVIDER_DELEGATE_H_

#include <stddef.h>

#include <vector>

#include "base/macros.h"
#include "build/build_config.h"

class SigninStatusMetricsProvider;

namespace signin {
class IdentityManager;
}

// Provides information relating to the status of accounts in the embedder: how
// many there are, how many are open, and how many are signed in. Note that
// "open" is an embedder-defined concept; in some embedders, all accounts are
// open.
struct AccountsStatus {
  AccountsStatus();

  size_t num_accounts;
  size_t num_opened_accounts;
  size_t num_signed_in_accounts;
};

// Delegate for SigninStatusMetricsProvider to abstract dependencies on
// embedder.
class SigninStatusMetricsProviderDelegate {
 public:
  SigninStatusMetricsProviderDelegate();
  virtual ~SigninStatusMetricsProviderDelegate();

  // Set the |owner_| field to the owning SigninStatusMetricsProvider.
  void SetOwner(SigninStatusMetricsProvider* owner);

  // Initializes the instance. SetOwner() must have been called before this
  // method.
  virtual void Initialize() = 0;

  // Returns the status of all accounts.
  virtual AccountsStatus GetStatusOfAllAccounts() = 0;

  // Returns the IdentityManager instance (if any) associated with each account.
  virtual std::vector<signin::IdentityManager*>
  GetIdentityManagersForAllAccounts() = 0;

 protected:
  SigninStatusMetricsProvider* owner() { return owner_; }

 private:
  SigninStatusMetricsProvider* owner_;

  DISALLOW_COPY_AND_ASSIGN(SigninStatusMetricsProviderDelegate);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_STATUS_METRICS_PROVIDER_DELEGATE_H_
