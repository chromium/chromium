// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSync)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      if ([delegate_ respondsToSelector:@selector(onPrimaryAccountSet:)]) {
        [delegate_ onPrimaryAccountSet:event.GetCurrentState().primary_account];
      }
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      if ([delegate_ respondsToSelector:@selector(onPrimaryAccountCleared:)]) {
        [delegate_
            onPrimaryAccountCleared:event.GetPreviousState().primary_account];
      }
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      NOTREACHED() << "ConsentLevel::kNotRequired is not yet supported on iOS. "
                      "This code needs to be updated when it is supported.";
      break;
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

}  // namespace signin
