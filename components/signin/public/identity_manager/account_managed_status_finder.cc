// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/account_managed_status_finder.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/icu/source/i18n/unicode/regex.h"

namespace signin {

namespace {

// Regexes that match many of the larger public email providers as we know
// these users are not from hosted enterprise domains.
const wchar_t* const kNonManagedDomainPatterns[] = {
    L"aol\\.com",
    L"comcast\\.net",
    L"googlemail\\.com",
    L"gmail\\.com",
    L"gmx\\.de",
    L"hotmail(\\.co|\\.com|)\\.[^.]+",  // hotmail.com, hotmail.it,
                                        // hotmail.co.uk
    L"live\\.com",
    L"mail\\.ru",
    L"msn\\.com",
    L"naver\\.com",
    L"orange\\.fr",
    L"outlook\\.com",
    L"qq\\.com",
    L"yahoo(\\.co|\\.com|)\\.[^.]+",  // yahoo.com, yahoo.co.uk, yahoo.com.tw
    L"yandex\\.ru",
    L"web\\.de",
    L"wp\\.pl",
    L"consumer\\.example\\.com",
};

const char* g_non_managed_domain_for_testing = nullptr;

// Returns true if |domain| matches the regex |pattern|.
bool MatchDomain(const std::u16string& domain,
                 const std::u16string& pattern,
                 size_t index) {
  UErrorCode status = U_ZERO_ERROR;
  const icu::UnicodeString icu_pattern(pattern.data(), pattern.length());
  icu::RegexMatcher matcher(icu_pattern, UREGEX_CASE_INSENSITIVE, status);
  if (!U_SUCCESS(status)) {
    // http://crbug.com/365351 - if for some reason the matcher creation fails
    // just return that the pattern doesn't match the domain. This is safe
    // because the calling method (IsNonEnterpriseUser()) is just used to enable
    // an optimization for non-enterprise users - better to skip the
    // optimization than crash.
    DLOG(ERROR) << "Possible invalid domain pattern: " << pattern
                << " - Error: " << status;
    return false;
  }
  icu::UnicodeString icu_input(domain.data(), domain.length());
  matcher.reset(icu_input);
  status = U_ZERO_ERROR;
  UBool match = matcher.matches(status);
  DCHECK(U_SUCCESS(status));
  return !!match;  // !! == convert from UBool to bool.
}

}  // namespace

// static
bool AccountManagedStatusFinder::IsNonEnterpriseUser(const std::string& email) {
  if (email.empty() || email.find('@') == std::string::npos) {
    // An empty email means no logged-in user, or incognito user in case of
    // ChromiumOS. Also, some tests use nonsense email addresses (e.g. "test");
    // these should be treated as non-enterprise too.
    return true;
  }
  const std::u16string domain = base::UTF8ToUTF16(
      gaia::ExtractDomainName(gaia::CanonicalizeEmail(email)));
  for (size_t i = 0; i < std::size(kNonManagedDomainPatterns); i++) {
    std::u16string pattern = base::WideToUTF16(kNonManagedDomainPatterns[i]);
    if (MatchDomain(domain, pattern, i))
      return true;
  }
  if (g_non_managed_domain_for_testing &&
      domain == base::UTF8ToUTF16(g_non_managed_domain_for_testing)) {
    return true;
  }
  return false;
}

// static
void AccountManagedStatusFinder::SetNonEnterpriseDomainForTesting(
    const char* domain) {
  g_non_managed_domain_for_testing = domain;
}

}  // namespace signin
