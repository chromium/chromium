// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/account_utils.h"

#include "base/strings/string_number_conversions.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/base/url_util.h"
#include "third_party/re2/src/re2/re2.h"

namespace contextual_tasks {

std::optional<size_t> GetUserIndex(const GURL& url) {
  const re2::RE2 kUserPathRegex{"/u/(\\d+)/"};

  int account_id = 0;
  if (re2::RE2::PartialMatch(url.path(), kUserPathRegex, &account_id)) {
    return account_id;
  }

  std::string account_id_str;
  if (net::GetValueForKeyInQuery(url, "authuser", &account_id_str) &&
      base::StringToInt(account_id_str, &account_id)) {
    return account_id;
  }

  return std::nullopt;
}

CoreAccountInfo GetPrimaryAccountInfoFromProfile(
    signin::IdentityManager* identity_manager) {
  if (!identity_manager) {
    return CoreAccountInfo();
  }
  return identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
}

std::optional<gaia::ListedAccount> GetAccountFromCookieJar(
    signin::IdentityManager* identity_manager,
    const GURL& url) {
  if (!identity_manager) {
    return std::nullopt;
  }

  auto accounts_in_cookie_jar = identity_manager->GetAccountsInCookieJar();
  const std::vector<gaia::ListedAccount>& accounts =
      accounts_in_cookie_jar.GetAllAccounts();

  if (accounts.empty()) {
    return std::nullopt;
  }

  std::optional<size_t> user_index = GetUserIndex(url);
  if (!user_index.has_value()) {
    return accounts[0];
  }

  if (*user_index >= accounts.size()) {
    return std::nullopt;
  }

  return accounts[*user_index];
}

bool IsUrlForPrimaryAccount(signin::IdentityManager* identity_manager,
                            const GURL& url) {
  CoreAccountInfo primary_account =
      GetPrimaryAccountInfoFromProfile(identity_manager);
  if (primary_account.IsEmpty()) {
    return false;
  }

  std::optional<gaia::ListedAccount> account_from_url =
      GetAccountFromCookieJar(identity_manager, url);
  if (!account_from_url.has_value()) {
    return false;
  }

  return primary_account.gaia == account_from_url->gaia_id;
}

}  // namespace contextual_tasks
