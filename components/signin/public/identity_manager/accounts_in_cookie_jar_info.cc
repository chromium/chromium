// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"

#include <algorithm>
#include <iterator>

namespace signin {

AccountsInCookieJarInfo::AccountsInCookieJarInfo() = default;

AccountsInCookieJarInfo::AccountsInCookieJarInfo(
    bool accounts_are_fresh,
    const std::vector<gaia::ListedAccount>& accounts)
    : accounts_are_fresh_(accounts_are_fresh), all_accounts_(accounts) {
  // `potentially_invalid_signed_in_accounts_` contains accounts that are not
  // considered signed out.
  std::ranges::copy_if(
      all_accounts_,
      std::back_inserter(potentially_invalid_signed_in_accounts_),
      [](const gaia::ListedAccount& a) { return !a.signed_out; });

  // `valid_signed_in_accounts_` contains accounts that are valid and not
  // considered signed out.
  std::ranges::copy_if(
      all_accounts_, std::back_inserter(valid_signed_in_accounts_),
      [](const gaia::ListedAccount& a) { return a.valid && !a.signed_out; });

  // `signed_out_accounts_` contains accounts that are signed out.
  std::ranges::copy_if(
      all_accounts_, std::back_inserter(signed_out_accounts_),
      [](const gaia::ListedAccount& a) { return a.signed_out; });
}

AccountsInCookieJarInfo::AccountsInCookieJarInfo(
    const AccountsInCookieJarInfo& other) = default;
AccountsInCookieJarInfo& AccountsInCookieJarInfo::operator=(
    const AccountsInCookieJarInfo& other) = default;

AccountsInCookieJarInfo::~AccountsInCookieJarInfo() = default;

bool AccountsInCookieJarInfo::AreAccountsFresh() const {
  return accounts_are_fresh_;
}

const std::vector<gaia::ListedAccount>&
AccountsInCookieJarInfo::GetAllAccounts() const {
  return all_accounts_;
}

const std::vector<gaia::ListedAccount>&
AccountsInCookieJarInfo::GetValidSignedInAccounts() const {
  return valid_signed_in_accounts_;
}

const std::vector<gaia::ListedAccount>&
AccountsInCookieJarInfo::GetPotentiallyInvalidSignedInAccounts() const {
  return potentially_invalid_signed_in_accounts_;
}

const std::vector<gaia::ListedAccount>&
AccountsInCookieJarInfo::GetSignedInAccounts() const {
  return GetPotentiallyInvalidSignedInAccounts();
}

const std::vector<gaia::ListedAccount>&
AccountsInCookieJarInfo::GetSignedOutAccounts() const {
  return signed_out_accounts_;
}

}  // namespace signin
