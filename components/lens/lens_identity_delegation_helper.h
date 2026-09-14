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
