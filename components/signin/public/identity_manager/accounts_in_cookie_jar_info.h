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
  AccountsInCookieJarInfo(bool accounts_are_fresh,
                          const std::vector<gaia::ListedAccount>& accounts);
  AccountsInCookieJarInfo(const AccountsInCookieJarInfo& other);
  AccountsInCookieJarInfo& operator=(const AccountsInCookieJarInfo& other);
  ~AccountsInCookieJarInfo();

  // True if the accounts info from cookie is fresh and does not need to be
  // updated.
  bool AreAccountsFresh() const;

  // The current list of all accounts in the cookie jar, regardless of their
  // validity or signed in/signed out status.
  const std::vector<gaia::ListedAccount>& GetAllAccounts() const;

  // The current list of signed in accounts from the cookie jar that are also
  // valid. When the account refresh token gets revoked remotely, the account
  // becomes invalid, so prefer this method over
  // GetPotentiallyInvalidSignedInAccounts unless your feature actually needs
  // invalid accounts as well.
  const std::vector<gaia::ListedAccount>& GetValidSignedInAccounts() const;

  // The current list of signed in accounts from the cookie jar. Please note
  // that accounts returned by this method can have be in an invalid state (for
  // example, if the token for that account was revoked remotely). Unless your
  // feature needs invalid accounts, consider using `GetValidSignedInAccounts`
  // instead.
  //
  // TODO(crbug.com/370769493): Audit callers and migrate them to
  //     `GetValidSignedInAccounts`.
  const std::vector<gaia::ListedAccount>&
  GetPotentiallyInvalidSignedInAccounts() const;

  // DEPRECATED: This method is being removed, do not use.
  // TODO(crbug.com/324462717): Remove after migrating internal usage.
  const std::vector<gaia::ListedAccount>& GetSignedInAccounts() const;

  // The current list of signed out accounts from the cookie jar.
  const std::vector<gaia::ListedAccount>& GetSignedOutAccounts() const;

  friend bool operator==(const AccountsInCookieJarInfo& lhs,
                         const AccountsInCookieJarInfo& rhs) = default;

 private:
  bool accounts_are_fresh_ = true;
  std::vector<gaia::ListedAccount> all_accounts_;
  std::vector<gaia::ListedAccount> valid_signed_in_accounts_;
  std::vector<gaia::ListedAccount> potentially_invalid_signed_in_accounts_;
  std::vector<gaia::ListedAccount> signed_out_accounts_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNTS_IN_COOKIE_JAR_INFO_H_
