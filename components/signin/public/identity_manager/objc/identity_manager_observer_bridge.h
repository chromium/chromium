// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_OBJC_IDENTITY_MANAGER_OBSERVER_BRIDGE_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_OBJC_IDENTITY_MANAGER_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"

// Implement this protocol and pass your implementation into a
// signin::IdentityManagerObserverBridge object to receive
// signin::IdentityManager::Observer callbacks in Objective-C.
@protocol IdentityManagerObserving <NSObject>

@optional

// These callbacks follow the semantics of the corresponding
// IdentityManager::Observer callbacks. See the comments on
// IdentityManager::Observer in identity_manager.h for the specification of
// these semantics.

- (void)primaryAccountDidChange:(const signin::PrimaryAccountChangeEvent&)event;
- (void)refreshTokenDidUpdateForAccount:(const CoreAccountInfo&)accountInfo;
- (void)refreshTokenWasRemovedForAccount:(const CoreAccountId&)accountId;
- (void)refreshTokensWasLoaded;
- (void)accountsInCookieWasUpdated:
            (const signin::AccountsInCookieJarInfo&)accountsInCookieJarInfo
                             error:(const GoogleServiceAuthError&)error;
- (void)batchOfRefreshTokenStateChangesDidEnd;
- (void)extendedAccountInfoDidUpdate:(const AccountInfo&)info;
- (void)accountsOnDeviceDidChange;
- (void)batchOfPrimaryAccountChangesDidEnd;
- (void)identityManagerDidShutdown:(signin::IdentityManager*)identityManager;

@end

namespace signin {

// Bridge class that listens for |IdentityManager| notifications and passes them
// to the Objective-C object.
class IdentityManagerObserverBridge : public IdentityManager::Observer {
 public:
  IdentityManagerObserverBridge(IdentityManager* identity_manager,
                                id<IdentityManagerObserving> target);

  IdentityManagerObserverBridge(const IdentityManagerObserverBridge&) = delete;
  IdentityManagerObserverBridge& operator=(
      const IdentityManagerObserverBridge&) = delete;

  ~IdentityManagerObserverBridge() override;

  // IdentityManager::Observer.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;
  void OnRefreshTokensLoaded() override;
  void OnAccountsInCookieUpdated(
      const AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;
  void OnEndBatchOfRefreshTokenStateChanges() override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnAccountsOnDeviceChanged() override;
  void OnEndBatchOfPrimaryAccountChanges() override;
  void OnIdentityManagerShutdown(IdentityManager* identity_manager) override;

 private:
  // Identity manager to observe.
  raw_ptr<IdentityManager> identity_manager_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  // Delegate to call.
  __weak id<IdentityManagerObserving> target_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_OBJC_IDENTITY_MANAGER_OBSERVER_BRIDGE_H_
