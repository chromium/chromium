// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNTS_IN_COOKIE_JAR_INFO_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNTS_IN_COOKIE_JAR_INFO_H_

#include <vector>

#include "google_apis/gaia/gaia_auth_util.h"

namespace signin {

// Container for a response to get the accounts in the cookie jar.
class AccountsInCookieJarInfo {
 public:
  AccountsInCookieJarInfo();
  AccountsInCookieJarInfo(
      bool accounts_are_fresh,
      const std::vector<gaia::ListedAccount>& signed_in_accounts,
      const std::vector<gaia::ListedAccount>& signed_out_accounts);
  AccountsInCookieJarInfo(const AccountsInCookieJarInfo& other);
  AccountsInCookieJarInfo& operator=(const AccountsInCookieJarInfo& other);
  ~AccountsInCookieJarInfo();

  // True if the accounts info from cookie is fresh and does not need to be
  // updated.
  bool AreAccountsFresh() const;

  // The current list of signed in accounts from the cookie jar.
  const std::vector<gaia::ListedAccount>& GetSignedInAccounts() const;

  // The current list of signed out accounts from the cookie jar.
  const std::vector<gaia::ListedAccount>& GetSignedOutAccounts() const;

 private:
  bool accounts_are_fresh_ = true;
  std::vector<gaia::ListedAccount> signed_in_accounts_;
  std::vector<gaia::ListedAccount> signed_out_accounts_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNTS_IN_COOKIE_JAR_INFO_H_
