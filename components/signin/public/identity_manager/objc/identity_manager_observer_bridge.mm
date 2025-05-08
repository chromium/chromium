// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"

#include "components/signin/public/base/account_consistency_method.h"

namespace signin {

IdentityManagerObserverBridge::IdentityManagerObserverBridge(
    IdentityManager* identity_manager,
    id<IdentityManagerObserverBridgeDelegate> delegate)
    : identity_manager_(identity_manager), delegate_(delegate) {
  identity_manager_observation_.Observe(identity_manager_);
}

IdentityManagerObserverBridge::~IdentityManagerObserverBridge() = default;

void IdentityManagerObserverBridge::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  if ([delegate_ respondsToSelector:@selector(onPrimaryAccountChanged:)]) {
    [delegate_ onPrimaryAccountChanged:event];
  }
}

void IdentityManagerObserverBridge::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  if ([delegate_
          respondsToSelector:@selector(onRefreshTokenUpdatedForAccount:)]) {
    [delegate_ onRefreshTokenUpdatedForAccount:account_info];
  }
}

void IdentityManagerObserverBridge::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  if ([delegate_
          respondsToSelector:@selector(onRefreshTokenRemovedForAccount:)]) {
    [delegate_ onRefreshTokenRemovedForAccount:account_id];
  }
}

void IdentityManagerObserverBridge::OnRefreshTokensLoaded() {
  if ([delegate_ respondsToSelector:@selector(onRefreshTokensLoaded)]) {
    [delegate_ onRefreshTokensLoaded];
  }
}

void IdentityManagerObserverBridge::OnAccountsInCookieUpdated(
    const AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  if ([delegate_ respondsToSelector:@selector(onAccountsInCookieUpdated:
                                                                  error:)]) {
    [delegate_ onAccountsInCookieUpdated:accounts_in_cookie_jar_info
                                   error:error];
  }
}

void IdentityManagerObserverBridge::OnEndBatchOfRefreshTokenStateChanges() {
  if ([delegate_
          respondsToSelector:@selector(onEndBatchOfRefreshTokenStateChanges)]) {
    [delegate_ onEndBatchOfRefreshTokenStateChanges];
  }
}

void IdentityManagerObserverBridge::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  if ([delegate_ respondsToSelector:@selector(onExtendedAccountInfoUpdated:)]) {
    [delegate_ onExtendedAccountInfoUpdated:info];
  }
}

void IdentityManagerObserverBridge::OnAccountsOnDeviceChanged() {
  if ([delegate_ respondsToSelector:@selector(onAccountsOnDeviceChanged)]) {
    [delegate_ onAccountsOnDeviceChanged];
  }
}

void IdentityManagerObserverBridge::OnEndBatchOfPrimaryAccountChanges() {
  if ([delegate_
          respondsToSelector:@selector(onEndBatchOfPrimaryAccountChanges)]) {
    [delegate_ onEndBatchOfPrimaryAccountChanges];
  }
}

void IdentityManagerObserverBridge::OnIdentityManagerShutdown(
    IdentityManager* identity_manager) {
  CHECK_EQ(identity_manager, identity_manager_, base::NotFatalUntil::M142);
  identity_manager_observation_.Reset();
  identity_manager_ = nullptr;
  if ([delegate_ respondsToSelector:@selector(onIdentityManagerShutdown:)]) {
    [delegate_ onIdentityManagerShutdown:identity_manager];
    // `this` should not be used after the previous line. Its onwer might have
    // deallocated it.
  }
}

}  // namespace signin
