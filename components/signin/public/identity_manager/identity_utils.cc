// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/identity_utils.h"

#include <string>
#include <string_view>

#include "base/check.h"
#include "base/containers/to_vector.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/icu/source/i18n/unicode/regex.h"

namespace signin {

namespace {

bool IsUsernameAllowedByPattern(std::string_view username,
                                std::string_view pattern) {
  if (pattern.empty())
    return true;

  // Patterns like "*@foo.com" are not accepted by our regex engine (since they
  // are not valid regular expressions - they should instead be ".*@foo.com").
  // For convenience, detect these patterns and insert a "." character at the
  // front.
  std::u16string utf16_pattern = base::UTF8ToUTF16(pattern);
  if (utf16_pattern[0] == L'*')
    utf16_pattern.insert(utf16_pattern.begin(), L'.');

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

}  // namespace

bool IsUsernameAllowedByPatternFromPrefs(const PrefService* prefs,
                                         const std::string& username) {
  return IsUsernameAllowedByPattern(
      username, prefs->GetString(prefs::kGoogleServicesUsernamePattern));
}

bool IsImplicitBrowserSigninOrExplicitDisabled(
    const IdentityManager* identity_manager,
    const PrefService* prefs) {
  if (!switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
    return true;
  }

  // The feature is enabled, check if the user is implicitly signed in.
  // Signed out users or signed in explicitly should return false.
  return identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin) &&
         !prefs->GetBoolean(prefs::kExplicitBrowserSignin);
}

bool AreGoogleCookiesRebuiltAfterClearingWhenSignedIn(
    signin::IdentityManager& manager,
    PrefService& prefs) {
  return !signin::IsImplicitBrowserSigninOrExplicitDisabled(&manager, &prefs) &&
         !manager.HasPrimaryAccount(signin::ConsentLevel::kSync);
}

base::flat_set<std::string> GetAllGaiaIdsForKeyedPreferences(
    const IdentityManager* identity_manager,
    const AccountsInCookieJarInfo& accounts_in_cookie_jar_info) {
  CHECK(accounts_in_cookie_jar_info.AreAccountsFresh());
  // Get all accounts in Chrome; both signed in and signed out accounts in
  // cookies.

  // `base::flat_set` has an optimized constructor from a vector.
  base::flat_set<std::string> gaia_ids(base::ToVector(
      accounts_in_cookie_jar_info.GetPotentiallyInvalidSignedInAccounts(),
      &gaia::ListedAccount::gaia_id));

  for (const gaia::ListedAccount& account :
       accounts_in_cookie_jar_info.GetSignedOutAccounts()) {
    gaia_ids.insert(account.gaia_id);
  }

  // If there is a Primary account, also keep it even if it was removed (not in
  // the cookie jar at all).
  std::string primary_account_gaia_id =
      identity_manager
          ? identity_manager
                ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                .gaia
          : std::string();
  if (!primary_account_gaia_id.empty()) {
    gaia_ids.insert(primary_account_gaia_id);
  }

  return gaia_ids;
}

}  // namespace signin
