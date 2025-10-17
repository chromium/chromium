// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_ACCOUNT_UTILS_H_
#define COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_ACCOUNT_UTILS_H_

#include <optional>

#include "components/signin/public/identity_manager/account_info.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "url/gurl.h"

namespace signin {
class IdentityManager;
}

namespace contextual_tasks {

// Extracts the user index from a URL. Looks for either '/u/<index>' or
// 'authuser=<index>' param in the URL.
std::optional<size_t> GetUserIndex(const GURL& url);

// Returns the primary account info from the profile. The account can be empty
// if the profile isn't signed in.
CoreAccountInfo GetPrimaryAccountInfoFromProfile(
    signin::IdentityManager* identity_manager);

// Returns the account from the cookie jar that corresponds to the user index
// in the URL ('authuser=' or '/u/'). If no index is specified, it defaults
// to the first account in the cookie jar.
std::optional<gaia::ListedAccount> GetAccountFromCookieJar(
    signin::IdentityManager* identity_manager,
    const GURL& url);

// Returns true if the user mentioned in the URL is same as the primary
// signed-in account, or if there is no user info in the URL.
bool IsUrlForPrimaryAccount(signin::IdentityManager* identity_manager,
                            const GURL& url);

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_INTERNAL_ACCOUNT_UTILS_H_
