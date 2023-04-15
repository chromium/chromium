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
AccountManagedStatusFinder::EmailEnterpriseStatus
AccountManagedStatusFinder::IsEnterpriseUserBasedOnEmail(
    const std::string& email) {
  size_t email_separator_pos = email.find('@');
  if (email.empty() || email_separator_pos == std::string::npos ||
      email_separator_pos == email.size() - 1) {
    // An empty email means no logged-in user, or incognito user in case of
    // ChromiumOS. Also, some tests use nonsense email addresses (e.g. "test");
    // these should be treated as non-enterprise too.
    return EmailEnterpriseStatus::kKnownNonEnterprise;
  }
  const std::u16string domain =
      base::UTF8ToUTF16(gaia::ExtractDomainName(email));
  for (size_t i = 0; i < std::size(kNonManagedDomainPatterns); i++) {
    std::u16string pattern = base::WideToUTF16(kNonManagedDomainPatterns[i]);
    if (MatchDomain(domain, pattern, i))
      return EmailEnterpriseStatus::kKnownNonEnterprise;
  }
  if (g_non_managed_domain_for_testing &&
      domain == base::UTF8ToUTF16(g_non_managed_domain_for_testing)) {
    return EmailEnterpriseStatus::kKnownNonEnterprise;
  }
  return EmailEnterpriseStatus::kUnknown;
}

// static
void AccountManagedStatusFinder::SetNonEnterpriseDomainForTesting(
    const char* domain) {
  g_non_managed_domain_for_testing = domain;
}

AccountManagedStatusFinder::AccountManagedStatusFinder(
    signin::IdentityManager* identity_manager,
    const CoreAccountInfo& account,
    base::OnceClosure async_callback)
    : identity_manager_(identity_manager), account_(account) {
  if (!identity_manager_->AreRefreshTokensLoaded()) {
    // We want to make sure that `account` exists in the IdentityManager but
    // we can only that after tokens are loaded. Wait for the
    // `OnRefreshTokensLoaded()` notification.
    identity_manager_observation_.Observe(identity_manager_.get());
    callback_ = std::move(async_callback);
    return;
  }

  outcome_ = DetermineOutcome();
  if (outcome_ == Outcome::kPending) {
    // Wait until the account information becomes available.
    identity_manager_observation_.Observe(identity_manager_.get());
    callback_ = std::move(async_callback);
    // TODO(crbug.com/1378553): Add a timeout mechanism.
  }

  // Result is known synchronously, ignore `async_callback`.
}

AccountManagedStatusFinder::~AccountManagedStatusFinder() = default;

void AccountManagedStatusFinder::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  DCHECK_EQ(outcome_, Outcome::kPending);

  // Don't care about other accounts.
  if (info.account_id != account_.account_id) {
    return;
  }

  // Keep waiting if `info` isn't complete yet.
  if (!info.IsValid()) {
    return;
  }

  // This is the relevant account! Determine its type.
  OutcomeDeterminedAsync(info.IsManaged() ? Outcome::kEnterprise
                                          : Outcome::kNonEnterprise);
}

void AccountManagedStatusFinder::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  DCHECK_EQ(outcome_, Outcome::kPending);

  // Don't care about other accounts.
  if (account_id != account_.account_id) {
    return;
  }

  // The interesting account was removed, we're done here.
  OutcomeDeterminedAsync(Outcome::kError);
}

void AccountManagedStatusFinder::OnRefreshTokensLoaded() {
  DCHECK_EQ(outcome_, Outcome::kPending);

  Outcome outcome = DetermineOutcome();
  if (outcome == Outcome::kPending) {
    // There is still not enough information to determine the account managed
    // status. Keep waiting for notifications from IdentityManager.
    return;
  }

  OutcomeDeterminedAsync(outcome);
}

void AccountManagedStatusFinder::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  DCHECK_EQ(outcome_, Outcome::kPending);

  OutcomeDeterminedAsync(Outcome::kError);
}

AccountManagedStatusFinder::Outcome
AccountManagedStatusFinder::DetermineOutcome() {
  // This must be called only after refresh tokens have been loaded.
  CHECK(identity_manager_->AreRefreshTokensLoaded());

  // First make sure that the account actually exists in the IdentityManager,
  // then check the easy cases: For most accounts, it's possible to statically
  // tell the account type from the email.
  if (!identity_manager_->HasAccountWithRefreshToken(account_.account_id)) {
    return Outcome::kError;
  }

  if (IsEnterpriseUserBasedOnEmail(account_.email) ==
      EmailEnterpriseStatus::kKnownNonEnterprise) {
    return Outcome::kNonEnterprise;
  }

  if (gaia::IsGoogleInternalAccountEmail(
          gaia::CanonicalizeEmail(account_.email))) {
    // Special case: @google.com accounts are a particular sub-type of
    // enterprise accounts.
    return Outcome::kEnterpriseGoogleDotCom;
  }

  // The easy cases didn't apply, so actually get the canonical info from
  // IdentityManager. This may or may not be available immediately.
  AccountInfo info = identity_manager_->FindExtendedAccountInfo(account_);
  if (info.IsValid()) {
    return info.IsManaged() ? Outcome::kEnterprise : Outcome::kNonEnterprise;
  }

  // Extended account info isn't (fully) available yet. Observe the
  // IdentityManager to get notified once it is.
  return Outcome::kPending;
}

void AccountManagedStatusFinder::OutcomeDeterminedAsync(Outcome type) {
  DCHECK_EQ(outcome_, Outcome::kPending);
  DCHECK_NE(type, Outcome::kPending);

  outcome_ = type;

  // The type of an account can't change, so no need to observe any longer.
  identity_manager_observation_.Reset();
  identity_manager_ = nullptr;

  // Let the client know the type was determined.
  std::move(callback_).Run();
}

}  // namespace signin
