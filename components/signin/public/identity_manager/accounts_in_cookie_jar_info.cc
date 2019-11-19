// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"

namespace signin {

AccountsInCookieJarInfo::AccountsInCookieJarInfo() = default;

AccountsInCookieJarInfo::AccountsInCookieJarInfo(
    bool accounts_are_fresh_param,
    const std::vector<gaia::ListedAccount>& signed_in_accounts_param,
    const std::vector<gaia::ListedAccount>& signed_out_accounts_param)
    : accounts_are_fresh(accounts_are_fresh_param),
      signed_in_accounts(signed_in_accounts_param),
      signed_out_accounts(signed_out_accounts_param) {}

AccountsInCookieJarInfo::AccountsInCookieJarInfo(
    const AccountsInCookieJarInfo& other) {
  if (this == &other)
    return;
  accounts_are_fresh = other.accounts_are_fresh;
  signed_in_accounts = other.signed_in_accounts;
  signed_out_accounts = other.signed_out_accounts;
}

AccountsInCookieJarInfo::~AccountsInCookieJarInfo() = default;

}  // namespace signin
