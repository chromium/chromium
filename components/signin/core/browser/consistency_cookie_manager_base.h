// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_CONSISTENCY_COOKIE_MANAGER_BASE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_CONSISTENCY_COOKIE_MANAGER_BASE_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/base/signin_metrics.h"

class SigninClient;

namespace signin {

// The ConsistencyCookieManagerBase checks if:
// - the account reconcilor is running
// - the accounts on the device are updating
// - the user has started to interact with device account settings (from Chrome)
// If one of these conditions is true, then this object sets a cookie on Gaia
// with a "Updating" value.
//
// Otherwise the value of the cookie is "Consistent" if the accounts are
// consistent (web accounts match device accounts) or "Inconsistent".
//
// Subclasses have to call UpdateCookie() at the end of the constructor.
class ConsistencyCookieManagerBase : public AccountReconcilor::Observer {
 public:
  ~ConsistencyCookieManagerBase() override;

 protected:
  static const char kStateConsistent[];
  static const char kStateInconsistent[];
  static const char kStateUpdating[];

  ConsistencyCookieManagerBase(SigninClient* signin_client,
                               AccountReconcilor* reconcilor);

  // Calculates the cookie value solely based on the reconcilor state.
  virtual std::string CalculateCookieValue();

  // Gets the new value using CalculateCookieValue and sets the cookie.
  void UpdateCookie();

 private:
  // AccountReconcilor::Observer:
  void OnStateChanged(signin_metrics::AccountReconcilorState state) override;

  signin_metrics::AccountReconcilorState account_reconcilor_state_ =
      signin_metrics::ACCOUNT_RECONCILOR_OK;
  SigninClient* signin_client_ = nullptr;
  ScopedObserver<AccountReconcilor, AccountReconcilor::Observer>
      account_reconcilor_observer_;

  DISALLOW_COPY_AND_ASSIGN(ConsistencyCookieManagerBase);
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_CONSISTENCY_COOKIE_MANAGER_BASE_H_
