// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_identity_delegation_helper.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

void RecordFetchHeadersMetrics(lens::LensIdentityDelegationFetchStatus status,
                               base::TimeDelta duration,
                               std::optional<bool> has_primary_account) {
  base::UmaHistogramEnumeration("Lens.IdentityDelegation.FetchHeadersStatus",
                                status);
  // Split by whether the profile is signed into Chrome, to separately measure
  // web-only (Chrome signed-out) users. Not recorded when there is no
  // IdentityManager to query.
  if (has_primary_account.has_value()) {
    base::UmaHistogramEnumeration(
        base::StrCat({"Lens.IdentityDelegation.FetchHeadersStatus.",
                      has_primary_account.value() ? "ChromeSignedIn"
                                                  : "ChromeSignedOut"}),
        status);
  }
  base::UmaHistogramTimes("Lens.IdentityDelegation.TimeToFetchHeaders",
                          duration);
  std::string_view status_str;
  switch (status) {
    case lens::LensIdentityDelegationFetchStatus::kSuccess:
      status_str = "Success";
      break;
    case lens::LensIdentityDelegationFetchStatus::kSignedOut:
      status_str = "SignedOut";
      break;
    case lens::LensIdentityDelegationFetchStatus::kAccountError:
      status_str = "AccountError";
      break;
    case lens::LensIdentityDelegationFetchStatus::kNoSapisidCookie:
      status_str = "NoSapisidCookie";
      break;
    case lens::LensIdentityDelegationFetchStatus::kHashFailed:
      status_str = "HashFailed";
      break;
    case lens::LensIdentityDelegationFetchStatus::kNoCookieManager:
      status_str = "NoCookieManager";
      break;
  }
  base::UmaHistogramTimes(
      base::StrCat({"Lens.IdentityDelegation.TimeToFetchHeaders.", status_str}),
      duration);
}

void RecordAccountErrorReason(
    lens::LensIdentityDelegationAccountErrorReason reason,
    bool cookie_jar_accounts_fresh) {
  base::UmaHistogramEnumeration("Lens.IdentityDelegation.AccountErrorReason",
                                reason);
  // Split by cookie jar freshness, since reasons like
  // kAuthUserIndexOutOfRange may be caused by stale ListAccounts data.
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"Lens.IdentityDelegation.AccountErrorReason.",
           cookie_jar_accounts_fresh ? "CookieJarFresh" : "CookieJarStale"}),
      reason);
}

// Returns the reason `account` cannot be used for identity delegation, or
// std::nullopt if it is usable.
std::optional<lens::LensIdentityDelegationAccountErrorReason>
GetCookieJarAccountErrorReason(signin::IdentityManager* identity_manager,
                               const gaia::ListedAccount& account) {
  if (account.signed_out) {
    return lens::LensIdentityDelegationAccountErrorReason::
        kCookieAccountSignedOut;
  }
  if (!account.valid) {
    return lens::LensIdentityDelegationAccountErrorReason::kInvalidSession;
  }
  if (identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          account.id)) {
    return lens::LensIdentityDelegationAccountErrorReason::kPersistentAuthError;
  }
  return std::nullopt;
}

// Helper to find a cookie by name in the CookieAccessResultList.
std::optional<net::CanonicalCookie> GetCookie(
    const net::CookieAccessResultList& cookie_list,
    const std::string& cookie_name) {
  auto it = std::ranges::find_if(
      cookie_list,
      [&cookie_name](
          const net::CookieWithAccessResult& cookie_with_access_result) {
        return cookie_with_access_result.cookie.Name() == cookie_name;
      });

  if (it != cookie_list.end()) {
    return it->cookie;
  }
  return std::nullopt;
}

// Callback for CookieManager::GetCookieList.
void OnCookiesFetched(
    const std::string& email,
    const std::string& origin,
    size_t account_index,
    bool has_primary_account,
    base::TimeTicks fetch_start_time,
    base::TimeTicks cookie_fetch_start_time,
    lens::GenerateSapisidHashCallback generate_sapisid_hash_callback,
    base::OnceCallback<void(std::vector<std::string>)> callback,
    const net::CookieAccessResultList& cookie_list,
    const net::CookieAccessResultList& excluded_cookies) {
  base::UmaHistogramTimes("Lens.IdentityDelegation.TimeToFetchCookies",
                          base::TimeTicks::Now() - cookie_fetch_start_time);

  base::TimeDelta fetch_duration = base::TimeTicks::Now() - fetch_start_time;

  std::optional<net::CanonicalCookie> sapisid_cookie =
      GetCookie(cookie_list, "SAPISID");

  std::vector<std::string> headers;
  if (!origin.empty()) {
    headers.push_back("Origin");
    headers.push_back(origin);
  }

  if (!sapisid_cookie.has_value()) {
    // If no SAPISID cookie, return only the Origin header (signed-out
    // behavior).
    RecordFetchHeadersMetrics(
        lens::LensIdentityDelegationFetchStatus::kNoSapisidCookie,
        fetch_duration, has_primary_account);
    std::move(callback).Run(headers);
    return;
  }

  CHECK(generate_sapisid_hash_callback);
  base::Time now = base::Time::Now();
  std::optional<std::string> auth_header = generate_sapisid_hash_callback.Run(
      email, sapisid_cookie->Value(), origin, now);

  if (auth_header.has_value()) {
    headers.push_back("Authorization");
    headers.push_back(auth_header.value());
    headers.push_back("X-Goog-AuthUser");
    headers.push_back(base::NumberToString(account_index));
    RecordFetchHeadersMetrics(lens::LensIdentityDelegationFetchStatus::kSuccess,
                              fetch_duration, has_primary_account);
  } else {
    RecordFetchHeadersMetrics(
        lens::LensIdentityDelegationFetchStatus::kHashFailed, fetch_duration,
        has_primary_account);
  }

  std::move(callback).Run(headers);
}

}  // namespace

namespace lens {

void FetchIdentityDelegationHeaders(
    network::mojom::CookieManager* cookie_manager,
    signin::IdentityManager* identity_manager,
    const std::string& origin,
    GenerateSapisidHashCallback generate_sapisid_hash_callback,
    std::optional<size_t> authuser_index,
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  base::TimeTicks fetch_start_time = base::TimeTicks::Now();
  CHECK(generate_sapisid_hash_callback);
  std::string canonical_origin =
      origin.empty() ? "" : url::Origin::Create(GURL(origin)).Serialize();

  // Whether the profile is signed into Chrome. Unknown if there is no
  // IdentityManager.
  std::optional<bool> has_primary_account;
  if (identity_manager) {
    has_primary_account =
        identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
  }

  auto return_signed_out_headers =
      [&canonical_origin, &callback, fetch_start_time,
       has_primary_account](LensIdentityDelegationFetchStatus status) {
        RecordFetchHeadersMetrics(status,
                                  base::TimeTicks::Now() - fetch_start_time,
                                  has_primary_account);
        std::vector<std::string> headers;
        if (!canonical_origin.empty()) {
          headers.push_back("Origin");
          headers.push_back(canonical_origin);
        }
        std::move(callback).Run(std::move(headers));
      };

  if (!cookie_manager) {
    return_signed_out_headers(
        LensIdentityDelegationFetchStatus::kNoCookieManager);
    return;
  }

  if (!identity_manager) {
    return_signed_out_headers(LensIdentityDelegationFetchStatus::kSignedOut);
    return;
  }

  signin::AccountsInCookieJarInfo cookie_jar_info =
      identity_manager->GetAccountsInCookieJar();

  const std::vector<gaia::ListedAccount>& all_accounts =
      cookie_jar_info.GetAllAccounts();
  const bool cookie_jar_accounts_fresh = cookie_jar_info.AreAccountsFresh();
  base::UmaHistogramCounts100("Lens.IdentityDelegation.CookieJarAccountCount",
                              all_accounts.size());
  base::UmaHistogramBoolean("Lens.IdentityDelegation.CookieJarAccountsFresh",
                            cookie_jar_accounts_fresh);

  // Find a valid signed-in account.
  const std::vector<gaia::ListedAccount>& accounts =
      cookie_jar_info.GetValidSignedInAccounts();

  if (accounts.empty()) {
    return_signed_out_headers(LensIdentityDelegationFetchStatus::kSignedOut);
    return;
  }

  size_t true_authuser_index = 0;
  bool found_account = false;
  if (authuser_index.has_value()) {
    // Account error reasons are only recorded on this path, which is the one
    // used by all production callers.
    std::optional<LensIdentityDelegationAccountErrorReason>
        account_error_reason;
    if (authuser_index.value() < all_accounts.size()) {
      account_error_reason = GetCookieJarAccountErrorReason(
          identity_manager, all_accounts[authuser_index.value()]);
    } else {
      account_error_reason =
          LensIdentityDelegationAccountErrorReason::kAuthUserIndexOutOfRange;
    }
    if (account_error_reason.has_value()) {
      RecordAccountErrorReason(account_error_reason.value(),
                               cookie_jar_accounts_fresh);
    } else {
      true_authuser_index = authuser_index.value();
      found_account = true;
    }
  } else {
    CoreAccountInfo primary_account =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

    if (!primary_account.IsEmpty() &&
        !identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
            primary_account.account_id)) {
      for (size_t i = 0; i < all_accounts.size(); ++i) {
        if (all_accounts[i].id == primary_account.account_id &&
            all_accounts[i].valid && !all_accounts[i].signed_out) {
          true_authuser_index = i;
          found_account = true;
          break;
        }
      }
    }
  }

  if (!found_account) {
    return_signed_out_headers(LensIdentityDelegationFetchStatus::kAccountError);
    return;
  }

  const gaia::ListedAccount& selected_account =
      all_accounts[true_authuser_index];

  // Fetch cookies for google.com to get SAPISID.
  // Use google.com as the GURL for cookie retrieval.
  GURL google_url("https://google.com");
  base::TimeTicks cookie_fetch_start_time = base::TimeTicks::Now();
  // We MUST use the selected account's email to generate the SAPISIDHASH.
  // The X-Goog-AuthUser header tells the backend which identity's email should
  // be used to verify the signature against the SAPISID cookie.
  cookie_manager->GetCookieList(
      google_url, net::CookieOptions::MakeAllInclusive(),
      net::CookiePartitionKeyCollection(),
      base::BindOnce(
          &OnCookiesFetched, selected_account.raw_email, canonical_origin,
          true_authuser_index, has_primary_account.value(), fetch_start_time,
          cookie_fetch_start_time, std::move(generate_sapisid_hash_callback),
          std::move(callback)));
}

}  // namespace lens
