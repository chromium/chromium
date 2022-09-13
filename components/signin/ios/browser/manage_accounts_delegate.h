// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_IOS_BROWSER_MANAGE_ACCOUNTS_DELEGATE_H_
#define COMPONENTS_SIGNIN_IOS_BROWSER_MANAGE_ACCOUNTS_DELEGATE_H_

class GURL;
namespace web {
class WebState;
}

class ManageAccountsDelegate {
 public:
  ManageAccountsDelegate(const ManageAccountsDelegate&) = delete;
  ManageAccountsDelegate& operator=(const ManageAccountsDelegate&) = delete;

  virtual ~ManageAccountsDelegate() = default;

  // Called when Gaia cookies have been regenerated for a specific user sign-in.
  // This occurs when a SAPISID cookie has been deleted by the operating system.
  virtual void OnRestoreGaiaCookies() = 0;

  // Called when the user taps on a manage accounts button in a Google web
  // property.
  virtual void OnManageAccounts() = 0;

  // Called when the user taps on an add account button in a Google web
  // property.
  virtual void OnAddAccount() = 0;

  // Called when the user taps a sign-in or add account button in a Google web
  // property.
  // |url| is the continuation URL received from the server. If it is valid,
  // then this delegate should navigate to |url|.
  virtual void OnShowConsistencyPromo(const GURL& url,
                                      web::WebState* webState) = 0;

  // Called when the user taps on go incognito button in a Google web property.
  // |url| is the continuation URL received from the server. If it is valid,
  // then this delegate should open an incognito tab and navigate to |url|.
  // If it is not valid, then this delegate should open a new incognito tab.
  virtual void OnGoIncognito(const GURL& url) = 0;

 protected:
  ManageAccountsDelegate() = default;
};

#endif  // COMPONENTS_SIGNIN_IOS_BROWSER_MANAGE_ACCOUNTS_DELEGATE_H_
