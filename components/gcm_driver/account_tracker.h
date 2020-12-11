// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_ACCOUNT_TRACKER_H_
#define COMPONENTS_GCM_DRIVER_ACCOUNT_TRACKER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/core_account_id.h"

namespace gcm {

// The AccountTracker keeps track of what accounts exist on the
// profile and the state of their credentials.
class AccountTracker : public signin::IdentityManager::Observer {
 public:
  explicit AccountTracker(signin::IdentityManager* identity_manager);
  ~AccountTracker() override;

  class Observer {
   public:
    virtual void OnAccountSignInChanged(const CoreAccountInfo& account,
                                        bool is_signed_in) = 0;
  };

  void Shutdown();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the list of accounts that are signed in, and for which gaia account
  // have been fetched. The primary account for the profile will be first
  // in the vector. Additional accounts will be in order of their gaia account.
  std::vector<CoreAccountInfo> GetAccounts() const;

 private:
  struct AccountState {
    CoreAccountInfo account;
    bool is_signed_in;
  };

  // signin::IdentityManager::Observer implementation.
  void OnPrimaryAccountSet(
      const CoreAccountInfo& primary_account_info) override;
  void OnPrimaryAccountCleared(
      const CoreAccountInfo& previous_primary_account_info) override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;

  // Add |account_info| to the lists of accounts tracked by this AccountTracker.
  void StartTrackingAccount(const CoreAccountInfo& account_info);

  // Stops tracking |account_id|. Notifies all observers if the account was
  // previously signed in.
  void StopTrackingAccount(const CoreAccountId account_id);

  // Stops tracking all accounts.
  void StopTrackingAllAccounts();

  // Updates the is_signed_in corresponding to the given account. Notifies all
  // observers of the signed in state changes.
  void UpdateSignInState(const CoreAccountId& account_id, bool is_signed_in);

  signin::IdentityManager* identity_manager_;
  std::map<CoreAccountId, AccountState> accounts_;
  base::ObserverList<Observer>::Unchecked observer_list_;
  bool shutdown_called_;
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_ACCOUNT_TRACKER_H_
