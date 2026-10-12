// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_IDENTITY_DELEGATION_HELPER_H_
#define COMPONENTS_LENS_LENS_IDENTITY_DELEGATION_HELPER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"

namespace network::mojom {
class CookieManager;
}  // namespace network::mojom

namespace signin {
class IdentityManager;
}  // namespace signin

namespace lens {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(LensIdentityDelegationFetchStatus)
enum class LensIdentityDelegationFetchStatus {
  kSuccess = 0,
  kSignedOut = 1,
  kAccountError = 2,
  kNoSapisidCookie = 3,
  kHashFailed = 4,
  kNoCookieManager = 5,
  kMaxValue = kNoCookieManager,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/lens/enums.xml:LensIdentityDelegationFetchStatus)

// The reason a fetch resolved to `LensIdentityDelegationFetchStatus::
// kAccountError`. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
//
// Recorded only when an authuser index is supplied (always the case in
// production). Each reason describes the cookie jar account at that index.
// When several apply, the first in this order is recorded:
// kAuthUserIndexOutOfRange, kCookieAccountSignedOut, kInvalidSession,
// kPersistentAuthError.
//
// LINT.IfChange(LensIdentityDelegationAccountErrorReason)
enum class LensIdentityDelegationAccountErrorReason {
  // The requested authuser index is past the end of the cookie jar.
  kAuthUserIndexOutOfRange = 0,
  // The selected cookie jar account is signed out. Distinct from
  // `LensIdentityDelegationFetchStatus::kSignedOut`, which means there are no
  // valid signed-in cookie jar accounts at all.
  kCookieAccountSignedOut = 1,
  // The selected cookie jar account's session is invalid (requires reauth).
  kInvalidSession = 2,
  // Chrome holds a refresh token for the selected account and it is in a
  // persistent error state. This is the only reason that depends on Chrome's
  // token state rather than the cookie jar.
  kPersistentAuthError = 3,
  kMaxValue = kPersistentAuthError,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/lens/enums.xml:LensIdentityDelegationAccountErrorReason)

using GenerateSapisidHashCallback =
    base::RepeatingCallback<std::optional<std::string>(
        const std::string& email,
        const std::string& sapisid_cookie,
        const std::string& origin,
        base::Time timestamp)>;

// Fetches the headers needed for first-party identity delegation.
// Validates account health, queries cookies for SAPISID, and builds the
// Authorization and X-Goog-AuthUser headers.
//
// If `authuser_index` is provided, attempts to use the account at that index
// in the cookie jar. Otherwise, falls back to the primary signed-in account.
// If no valid signed-in account or cookie is found, returns only the Origin
// header (signed-out behavior).
void FetchIdentityDelegationHeaders(
    network::mojom::CookieManager* cookie_manager,
    signin::IdentityManager* identity_manager,
    const std::string& origin,
    GenerateSapisidHashCallback generate_sapisid_hash_callback,
    std::optional<size_t> authuser_index,
    base::OnceCallback<void(std::vector<std::string>)> callback);

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_IDENTITY_DELEGATION_HELPER_H_
