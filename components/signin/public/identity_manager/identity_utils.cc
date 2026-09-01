// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/identity_utils.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/containers/to_vector.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_id.h"
#include "third_party/icu/source/i18n/unicode/regex.h"

namespace signin {

namespace {

bool IsAccountAllowed(const PrefService* prefs, std::string_view email) {
  return !prefs || IsUsernameAllowedByPatternFromPrefs(prefs, email);
}

}  // namespace

bool IsUsernameAllowedByPattern(std::string_view username,
                                std::string_view pattern) {
  if (pattern.empty()) {
    return true;
  }

  // Patterns like "*@foo.com" are not accepted by our regex engine (since they
  // are not valid regular expressions - they should instead be ".*@foo.com").
  // For convenience, detect these patterns and insert a "." character at the
  // front.
  std::u16string utf16_pattern = base::UTF8ToUTF16(pattern);
  if (utf16_pattern[0] == L'*') {
    utf16_pattern.insert(utf16_pattern.begin(), L'.');
  }

  // See if the username matches the policy-provided pattern.
  UErrorCode status = U_ZERO_ERROR;
  const icu::UnicodeString icu_pattern(false, utf16_pattern.data(),
                                       utf16_pattern.length());
  icu::RegexMatcher matcher(icu_pattern, UREGEX_CASE_INSENSITIVE, status);
  if (!U_SUCCESS(status)) {
    LOG(ERROR) << "Invalid login regex: " << utf16_pattern
               << ", status: " << status;
    // If an invalid pattern is provided, then prohibit *all* logins (better to
    // break signin than to quietly allow users to sign in).
    return false;
  }
  // The default encoding is UTF-8 in Chromium's ICU.
  icu::UnicodeString icu_input(username.data());
  matcher.reset(icu_input);
  status = U_ZERO_ERROR;
  UBool match = matcher.matches(status);
  DCHECK(U_SUCCESS(status));
  return !!match;  // !! == convert from UBool to bool.
}

bool IsUsernameAllowedByPatternFromPrefs(const PrefService* prefs,
                                         std::string_view username) {
  if (!prefs) {
    return true;
  }

  return IsUsernameAllowedByPattern(
      username, prefs->GetString(prefs::kGoogleServicesUsernamePattern));
}

base::flat_set<GaiaId> GetAllGaiaIdsForKeyedPreferences(
    const IdentityManager* identity_manager,
    const AccountsInCookieJarInfo& accounts_in_cookie_jar_info) {
  CHECK(accounts_in_cookie_jar_info.AreAccountsFresh());
  // Get all accounts in Chrome; both signed in and signed out accounts in
  // cookies.

  // `base::flat_set` has an optimized constructor from a vector.
  base::flat_set<GaiaId> gaia_ids(
      base::ToVector(accounts_in_cookie_jar_info.GetAllAccounts(),
                     &gaia::ListedAccount::gaia_id));

  // If there is a Primary account, also keep it even if it was removed (not in
  // the cookie jar at all).
  GaiaId primary_account_gaia_id =
      identity_manager
          ? identity_manager
                ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                .gaia
          : GaiaId();
  if (!primary_account_gaia_id.empty()) {
    gaia_ids.insert(primary_account_gaia_id);
  }

  return gaia_ids;
}

std::vector<AccountInfo> GetOrderedAccountsForDisplay(
    const IdentityManager* identity_manager,
    const PrefService* local_state) {
  CHECK(identity_manager);

  std::vector<AccountInfo> accounts;
  CoreAccountId primary_account_id =
      identity_manager->GetPrimaryAccountId(ConsentLevel::kSignin);

  // 1. Primary account (if any) is always first, even if it is not in the
  // cookie jar.
  if (!primary_account_id.empty()) {
    AccountInfo primary_account = identity_manager->FindExtendedAccountInfo(
        identity_manager->GetPrimaryAccountInfo(ConsentLevel::kSignin));
    if (!primary_account.GetEmail().empty() &&
        IsAccountAllowed(local_state, primary_account.GetEmail())) {
      accounts.push_back(std::move(primary_account));
    }
  }

#if BUILDFLAG(IS_IOS)
  // 2. On iOS: Device accounts are returned in system keychain order.
  for (const AccountInfo& account : identity_manager->GetAccountsOnDevice()) {
    if (account.GetAccountId() == primary_account_id) {
      continue;
    }
    AccountInfo extended_info =
        identity_manager->FindExtendedAccountInfo(account);
    // Some device accounts may not be in Chrome.
    const AccountInfo& account_to_use =
        extended_info.IsEmpty() ? account : extended_info;
    if (IsAccountAllowed(local_state, account_to_use.GetEmail())) {
      accounts.push_back(account_to_use);
    }
  }
#elif BUILDFLAG(IS_ANDROID)
  // 3. On Android: ProfileOAuth2TokenServiceDelegateAndroid stores accounts in
  // a std::vector<CoreAccountId> directly populated from
  // AccountManagerFacade.getAccounts(). Because insertion order is preserved
  // (unlike Desktop/iOS token service delegates which use a map or set),
  // GetExtendedAccountInfoForAccountsWithRefreshToken() safely and
  // deterministically returns device accounts in the OS AccountManager order
  // (with the device's primary/default Google account at index 0).
  for (const AccountInfo& account :
       identity_manager->GetExtendedAccountInfoForAccountsWithRefreshToken()) {
    if (account.GetAccountId() == primary_account_id) {
      continue;
    }
    if (IsAccountAllowed(local_state, account.GetEmail())) {
      accounts.push_back(account);
    }
  }
#else
  // 4. On Desktop: Token service stores accounts in an unordered std::map, so
  // the default account ordering is determined by the Gaia cookie jar.
  std::vector<AccountInfo> accounts_with_tokens =
      identity_manager->GetExtendedAccountInfoForAccountsWithRefreshToken();
  AccountsInCookieJarInfo accounts_in_jar =
      identity_manager->GetAccountsInCookieJar();

  for (const gaia::ListedAccount& listed_account :
       accounts_in_jar.GetPotentiallyInvalidSignedInAccounts()) {
    if (listed_account.id == primary_account_id) {
      continue;
    }
    if (!IsAccountAllowed(local_state, listed_account.email)) {
      continue;
    }
    auto it = std::ranges::find(accounts_with_tokens, listed_account.id,
                                &AccountInfo::GetAccountId);
    if (it != accounts_with_tokens.end()) {
      accounts.push_back(*it);
    }
  }
#endif

  return accounts;
}

}  // namespace signin
