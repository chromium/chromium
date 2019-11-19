// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_OBJC_IDENTITY_MANAGER_OBSERVER_BRIDGE_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_OBJC_IDENTITY_MANAGER_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>
#include <vector>

#include "components/signin/public/identity_manager/identity_manager.h"

// Implement this protocol and pass your implementation into an
// IdentityManagerObserverBridge object to receive IdentityManager observer
// callbacks in Objective-C.
@protocol IdentityManagerObserverBridgeDelegate <NSObject>

@optional

// These callbacks follow the semantics of the corresponding
// IdentityManager::Observer callbacks. See the comments on
// IdentityManager::Observer in identity_manager.h for the specification of
// these semantics.

- (void)onPrimaryAccountSet:(const CoreAccountInfo&)primaryAccountInfo;
- (void)onPrimaryAccountCleared:
    (const CoreAccountInfo&)previousPrimaryAccountInfo;
- (void)onRefreshTokenUpdatedForAccount:(const CoreAccountInfo&)accountInfo;
- (void)onRefreshTokenRemovedForAccount:(const std::string&)accountId;
- (void)onRefreshTokensLoaded;
- (void)onAccountsInCookieUpdated:
            (const signin::AccountsInCookieJarInfo&)accountsInCookieJarInfo
                            error:(const GoogleServiceAuthError&)error;
- (void)onEndBatchOfRefreshTokenStateChanges;

@end

namespace signin {

// Bridge class that listens for |IdentityManager| notifications and
// passes them to its Objective-C delegate.
class IdentityManagerObserverBridge : public IdentityManager::Observer {
 public:
  IdentityManagerObserverBridge(
      IdentityManager* identity_manager,
      id<IdentityManagerObserverBridgeDelegate> delegate);
  ~IdentityManagerObserverBridge() override;

  // IdentityManager::Observer.
  void OnPrimaryAccountSet(
      const CoreAccountInfo& primary_account_info) override;
  void OnPrimaryAccountCleared(
      const CoreAccountInfo& previous_primary_account_info) override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;
  void OnRefreshTokensLoaded() override;
  void OnAccountsInCookieUpdated(
      const AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;
  void OnEndBatchOfRefreshTokenStateChanges() override;

 private:
  // Identity manager to observe.
  IdentityManager* identity_manager_;
  // Delegate to call.
  __weak id<IdentityManagerObserverBridgeDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(IdentityManagerObserverBridge);
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_OBJC_IDENTITY_MANAGER_OBSERVER_BRIDGE_H_
