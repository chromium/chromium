// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_ACCOUNT_PREVIEW_UTILS_H_
#define CHROME_BROWSER_UI_SIGNIN_ACCOUNT_PREVIEW_UTILS_H_

#include <optional>
#include <string>
#include <string_view>

#include "components/signin/core/browser/account_preview_data_service.h"

namespace signin {

// Returns a formatted subtitle string for a signin promo based on the account
// preview preference, or std::nullopt if preference contains no preferred
// data types.
std::optional<std::string> GetAccountPreviewPromoSubtitle(
    const AccountPreviewDataService::AccountPreviewPreference& preference);

// Returns a formatted subtitle string for the profile separation (multi-user)
// intercept bubble with account preview, or std::nullopt if preference contains
// no preferred data types.
std::optional<std::string> GetAccountPreviewProfileSeparationSubtitle(
    std::string_view existing_account_given_name,
    std::string_view new_account_email,
    const AccountPreviewDataService::AccountPreviewPreference& preference);

}  // namespace signin

#endif  // CHROME_BROWSER_UI_SIGNIN_ACCOUNT_PREVIEW_UTILS_H_
