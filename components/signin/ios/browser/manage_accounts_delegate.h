// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_IOS_BROWSER_MANAGE_ACCOUNTS_DELEGATE_H_
#define COMPONENTS_SIGNIN_IOS_BROWSER_MANAGE_ACCOUNTS_DELEGATE_H_

class GURL;

@protocol ManageAccountsDelegate<NSObject>

// Called when the user taps on a manage accounts button in a Google web
// property.
- (void)onManageAccounts;

// Called when the user taps on an add account button in a Google web property.
- (void)onAddAccount;

// Called when the user taps a sign-in or add account button in a Google web
// property with signin::kMobileIdentityConsistency enabled.
- (void)onShowConsistencyPromo;

// Called when the user taps on go incognito button in a Google web property.
// |url| is the continuation URL received from the server. If it is valid,
// then this delegate should open an incognito tab and navigate to |url|.
// If it is not valid, then this delegate should open a new incognito tab.
- (void)onGoIncognito:(const GURL&)url;

@end

#endif  // COMPONENTS_SIGNIN_IOS_BROWSER_MANAGE_ACCOUNTS_DELEGATE_H_
