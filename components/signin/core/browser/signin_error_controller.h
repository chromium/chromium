// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_ERROR_CONTROLLER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_ERROR_CONTROLLER_H_

#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"

// Keep track of auth errors and expose them to observers in the UI. Services
// that wish to expose auth errors to the user should register an
// AuthStatusProvider to report their current authentication state, and should
// invoke AuthStatusChanged() when their authentication state may have changed.
class SigninErrorController : public KeyedService,
                              public signin::IdentityManager::Observer {
 public:
  enum class AccountMode {
    // Signin error controller monitors all the accounts. When multiple accounts
    // are in error state, only one of the errors is reported.
    ANY_ACCOUNT,

    // Only errors on the primary account are reported. Other accounts are
    // ignored.
    PRIMARY_ACCOUNT
  };

  // The observer class for SigninErrorController lets the controller notify
  // observers when an error arises or changes.
  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnErrorChanged() = 0;
  };

  SigninErrorController(AccountMode mode,
                        signin::IdentityManager* identity_manager);
  ~SigninErrorController() override;

  // KeyedService implementation:
  void Shutdown() override;

  // True if there exists an error worth elevating to the user. Note that
  // |SigninErrorController| can be running in |AccountMode::ANY_ACCOUNT| mode,
  // in which case |HasError| can return an error for any account, not just the
  // Primary Account. See |error_account_id()|.
  bool HasError() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  const CoreAccountId& error_account_id() const { return error_account_id_; }
  const GoogleServiceAuthError& auth_error() const { return auth_error_; }

 private:
  // Invoked when the auth status has changed.
  void Update();

  // Checks for Secondary Account errors and updates |auth_error_| and
  // |error_account_id_| accordingly. Does not do anything if no Secondary
  // Account has any error. Returns true if an error was found in a Secondary
  // Account, false otherwise.
  // Note: This function must not be called if |account_mode_| is
  // |AccountMode::PRIMARY_ACCOUNT|.
  bool UpdateSecondaryAccountErrors(
      const CoreAccountId& primary_account_id,
      const CoreAccountId& prev_account_id,
      const GoogleServiceAuthError::State& prev_error_state);

  // signin::IdentityManager::Observer:
  void OnEndBatchOfRefreshTokenStateChanges() override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error) override;
  void OnPrimaryAccountSet(
      const CoreAccountInfo& primary_account_info) override;
  void OnPrimaryAccountCleared(
      const CoreAccountInfo& previous_primary_account_info) override;

  const AccountMode account_mode_;
  signin::IdentityManager* identity_manager_;

  ScopedObserver<signin::IdentityManager, signin::IdentityManager::Observer>
      scoped_identity_manager_observer_{this};

  // The account that generated the last auth error.
  CoreAccountId error_account_id_;

  // The auth error detected the last time AuthStatusChanged() was invoked (or
  // NONE if AuthStatusChanged() has never been invoked).
  GoogleServiceAuthError auth_error_;

  base::ObserverList<Observer, false>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(SigninErrorController);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_ERROR_CONTROLLER_H_
