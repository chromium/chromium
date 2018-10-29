// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_ACCOUNT_TRACKER_H_
#define COMPONENTS_GCM_DRIVER_ACCOUNT_TRACKER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "services/identity/public/cpp/access_token_fetcher.h"
#include "services/identity/public/cpp/identity_manager.h"

class GoogleServiceAuthError;

namespace network {
class SharedURLLoaderFactory;
}

namespace gcm {

struct AccountIds {
  std::string account_key;  // The account ID used by OAuth2TokenService.
  std::string gaia;
  std::string email;
};

class AccountIdFetcher;

// The AccountTracker keeps track of what accounts exist on the
// profile and the state of their credentials. The tracker fetches the
// gaia ID of each account it knows about.
//
// The AccountTracker maintains these invariants:
// 1. Events are only fired after the gaia ID has been fetched.
// 2. Add/Remove and SignIn/SignOut pairs are always generated in order.
// 3. SignIn follows Add, and there will be a SignOut between SignIn & Remove.
// 4. If there is no primary account, there are no other accounts.
class AccountTracker : public identity::IdentityManager::Observer {
 public:
  AccountTracker(
      identity::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~AccountTracker() override;

  class Observer {
   public:
    virtual void OnAccountSignInChanged(const AccountIds& ids,
                                        bool is_signed_in) = 0;
  };

  void Shutdown();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the list of accounts that are signed in, and for which gaia IDs
  // have been fetched. The primary account for the profile will be first
  // in the vector. Additional accounts will be in order of their gaia IDs.
  std::vector<AccountIds> GetAccounts() const;

  // Indicates if all user information has been fetched. If the result is false,
  // there are still unfinished fetchers.
  virtual bool IsAllUserInfoFetched() const;

 private:
  friend class AccountIdFetcher;

  struct AccountState {
    AccountIds ids;
    bool is_signed_in;
  };

  // identity::IdentityManager::Observer implementation.
  void OnPrimaryAccountSet(const AccountInfo& primary_account_info) override;
  void OnPrimaryAccountCleared(
      const AccountInfo& previous_primary_account_info) override;
  void OnRefreshTokenUpdatedForAccount(const AccountInfo& account_info,
                                       bool is_valid) override;
  void OnRefreshTokenRemovedForAccount(const std::string& account_id) override;

  void OnUserInfoFetchSuccess(AccountIdFetcher* fetcher,
                              const std::string& gaia_id);
  void OnUserInfoFetchFailure(AccountIdFetcher* fetcher);

  void NotifySignInChanged(const AccountState& account);

  void UpdateSignInState(const std::string& account_key, bool is_signed_in);

  void StartTrackingAccount(const std::string& account_key);

  // Note: |account_key| is passed by value here, because the original
  // object may be stored in |accounts_| and if so, it will be destroyed
  // after erasing the key from the map.
  void StopTrackingAccount(const std::string account_key);

  void StopTrackingAllAccounts();
  void StartFetchingUserInfo(const std::string& account_key);
  void DeleteFetcher(AccountIdFetcher* fetcher);

  identity::IdentityManager* identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::map<std::string, std::unique_ptr<AccountIdFetcher>> user_info_requests_;
  std::map<std::string, AccountState> accounts_;
  base::ObserverList<Observer>::Unchecked observer_list_;
  bool shutdown_called_;
};

class AccountIdFetcher : public gaia::GaiaOAuthClient::Delegate {
 public:
  AccountIdFetcher(
      identity::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      AccountTracker* tracker,
      const std::string& account_key);
  ~AccountIdFetcher() override;

  const std::string& account_key() { return account_key_; }

  void Start();

  void AccessTokenFetched(GoogleServiceAuthError error,
                          identity::AccessTokenInfo access_token_info);

  // gaia::GaiaOAuthClient::Delegate implementation.
  void OnGetUserIdResponse(const std::string& gaia_id) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

 private:
  identity::IdentityManager* identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  AccountTracker* tracker_;
  const std::string account_key_;

  std::unique_ptr<identity::AccessTokenFetcher> access_token_fetcher_;
  std::unique_ptr<gaia::GaiaOAuthClient> gaia_oauth_client_;
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_ACCOUNT_TRACKER_H_
