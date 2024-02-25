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
  identity_manager_->AddObserver(this);
}

IdentityManagerObserverBridge::~IdentityManagerObserverBridge() {
  identity_manager_->RemoveObserver(this);
}

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

void IdentityManagerObserverBridge::OnIdentityManagerShutdown(
    IdentityManager* identity_manager) {
  if ([delegate_ respondsToSelector:@selector(onIdentityManagerShutdown:)]) {
    [delegate_ onIdentityManagerShutdown:identity_manager];
  }
}

}  // namespace signin
