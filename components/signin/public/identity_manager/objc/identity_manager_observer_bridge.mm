// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"

#import "components/signin/public/base/account_consistency_method.h"

namespace signin {

IdentityManagerObserverBridge::IdentityManagerObserverBridge(
    IdentityManager* identity_manager,
    id<IdentityManagerObserving> target)
    : identity_manager_(identity_manager), target_(target) {
  identity_manager_observation_.Observe(identity_manager_);
}

IdentityManagerObserverBridge::~IdentityManagerObserverBridge() = default;

void IdentityManagerObserverBridge::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  if ([target_ respondsToSelector:@selector(primaryAccountDidChange:)]) {
    [target_ primaryAccountDidChange:event];
  }
}

void IdentityManagerObserverBridge::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  if ([target_
          respondsToSelector:@selector(refreshTokenDidUpdateForAccount:)]) {
    [target_ refreshTokenDidUpdateForAccount:account_info];
  }
}

void IdentityManagerObserverBridge::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  if ([target_
          respondsToSelector:@selector(refreshTokenWasRemovedForAccount:)]) {
    [target_ refreshTokenWasRemovedForAccount:account_id];
  }
}

void IdentityManagerObserverBridge::OnRefreshTokensLoaded() {
  if ([target_ respondsToSelector:@selector(refreshTokensWasLoaded)]) {
    [target_ refreshTokensWasLoaded];
  }
}

void IdentityManagerObserverBridge::OnAccountsInCookieUpdated(
    const AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  if ([target_
          respondsToSelector:@selector(accountsInCookieWasUpdated:error:)]) {
    [target_ accountsInCookieWasUpdated:accounts_in_cookie_jar_info
                                  error:error];
  }
}

void IdentityManagerObserverBridge::OnEndBatchOfRefreshTokenStateChanges() {
  if ([target_ respondsToSelector:@selector(
                                      batchOfRefreshTokenStateChangesDidEnd)]) {
    [target_ batchOfRefreshTokenStateChangesDidEnd];
  }
}

void IdentityManagerObserverBridge::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  if ([target_ respondsToSelector:@selector(extendedAccountInfoDidUpdate:)]) {
    [target_ extendedAccountInfoDidUpdate:info];
  }
}

void IdentityManagerObserverBridge::OnAccountsOnDeviceChanged() {
  if ([target_ respondsToSelector:@selector(accountsOnDeviceDidChange)]) {
    [target_ accountsOnDeviceDidChange];
  }
}

void IdentityManagerObserverBridge::OnEndBatchOfPrimaryAccountChanges() {
  if ([target_
          respondsToSelector:@selector(batchOfPrimaryAccountChangesDidEnd)]) {
    [target_ batchOfPrimaryAccountChangesDidEnd];
  }
}

void IdentityManagerObserverBridge::OnIdentityManagerShutdown(
    IdentityManager* identity_manager) {
  CHECK_EQ(identity_manager, identity_manager_, base::NotFatalUntil::M142);
  identity_manager_observation_.Reset();
  identity_manager_ = nullptr;
  if ([target_ respondsToSelector:@selector(identityManagerDidShutdown:)]) {
    [target_ identityManagerDidShutdown:identity_manager];
    // `this` should not be used after the previous line. Its onwer might have
    // deallocated it.
  }
}

}  // namespace signin
