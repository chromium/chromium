// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_OBJC_IDENTITY_MANAGER_OBSERVER_BRIDGE_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_OBJC_IDENTITY_MANAGER_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "base/memory/raw_ptr.h"
#include "components/signin/public/identity_manager/account_info.h"
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

- (void)onPrimaryAccountChanged:(const signin::PrimaryAccountChangeEvent&)event;
- (void)onRefreshTokenUpdatedForAccount:(const CoreAccountInfo&)accountInfo;
- (void)onRefreshTokenRemovedForAccount:(const CoreAccountId&)accountId;
- (void)onRefreshTokensLoaded;
- (void)onAccountsInCookieUpdated:
            (const signin::AccountsInCookieJarInfo&)accountsInCookieJarInfo
                            error:(const GoogleServiceAuthError&)error;
- (void)onEndBatchOfRefreshTokenStateChanges;
- (void)onExtendedAccountInfoUpdated:(const AccountInfo&)info;
- (void)onIdentityManagerShutdown:(signin::IdentityManager*)identityManager;

@end

namespace signin {

// Bridge class that listens for |IdentityManager| notifications and
// passes them to its Objective-C delegate.
class IdentityManagerObserverBridge : public IdentityManager::Observer {
 public:
  IdentityManagerObserverBridge(
      IdentityManager* identity_manager,
      id<IdentityManagerObserverBridgeDelegate> delegate);

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
  void OnIdentityManagerShutdown(IdentityManager* identity_manager) override;

 private:
  // Identity manager to observe.
  raw_ptr<IdentityManager> identity_manager_;
  // Delegate to call.
  __weak id<IdentityManagerObserverBridgeDelegate> delegate_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_OBJC_IDENTITY_MANAGER_OBSERVER_BRIDGE_H_
