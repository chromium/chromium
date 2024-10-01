// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"

namespace signin {

AccountsInCookieJarInfo::AccountsInCookieJarInfo() = default;

AccountsInCookieJarInfo::AccountsInCookieJarInfo(
    bool accounts_are_fresh,
    const std::vector<gaia::ListedAccount>& signed_in_accounts,
    const std::vector<gaia::ListedAccount>& signed_out_accounts)
    : accounts_are_fresh_(accounts_are_fresh),
      signed_in_accounts_(signed_in_accounts),
      signed_out_accounts_(signed_out_accounts) {}

AccountsInCookieJarInfo::AccountsInCookieJarInfo(
    const AccountsInCookieJarInfo& other) = default;
AccountsInCookieJarInfo& AccountsInCookieJarInfo::operator=(
    const AccountsInCookieJarInfo& other) = default;

AccountsInCookieJarInfo::~AccountsInCookieJarInfo() = default;

bool AccountsInCookieJarInfo::AreAccountsFresh() const {
  return accounts_are_fresh_;
}

const std::vector<gaia::ListedAccount>&
AccountsInCookieJarInfo::GetSignedInAccounts() const {
  return signed_in_accounts_;
}

const std::vector<gaia::ListedAccount>&
AccountsInCookieJarInfo::GetSignedOutAccounts() const {
  return signed_out_accounts_;
}

}  // namespace signin
