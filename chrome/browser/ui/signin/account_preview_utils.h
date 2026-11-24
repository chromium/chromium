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

// Returns a formatted subtitle string for the profile menu signin promo based
// on the account preview preference and user email, or std::nullopt if
// preference contains no preferred data types.
std::optional<std::string> GetAccountPreviewProfileMenuSubtitle(
    std::string_view email,
    const AccountPreviewDataService::AccountPreviewPreference& preference);

// Returns a formatted subtitle string for the Settings signin promo based on
// the account preview preference, or std::nullopt if preference contains no
// preferred data types.
std::optional<std::string> GetAccountPreviewSettingsPromoSubtitle(
    const AccountPreviewDataService::AccountPreviewPreference& preference);

// Returns a formatted subtitle string for the history page signin promo when
// the user is signed in without history sync, based on the account preview
// preference.
std::string GetAccountPreviewHistorySignedInPromoSubtitle(
    const AccountPreviewDataService::AccountPreviewPreference& preference);

// Returns a formatted subtitle string for the history page signin promo based
// on the account preview preference and user email, or std::nullopt if
// preference contains no preferred data types.
std::optional<std::string> GetAccountPreviewHistoryPromoSubtitle(
    std::string_view email,
    const AccountPreviewDataService::AccountPreviewPreference& preference);

// Returns a formatted subtitle string for the recent tabs signin promo when
// the user is signed in without history sync, based on the account preview
// preference and user email.
std::string GetAccountPreviewRecentTabsSignedInPromoSubtitle(
    std::string_view email,
    const AccountPreviewDataService::AccountPreviewPreference& preference);

// Returns a formatted subtitle string for the recent tabs signin promo based
// on the account preview preference, or std::nullopt if preference contains no
// preferred data types.
std::optional<std::string> GetAccountPreviewRecentTabsPromoSubtitle(
    const AccountPreviewDataService::AccountPreviewPreference& preference);

// Returns a formatted subtitle string for the profile separation (multi-user)
// intercept bubble with account preview, or std::nullopt if preference contains
// no preferred data types.
std::optional<std::string> GetAccountPreviewProfileSeparationSubtitle(
    std::string_view existing_account_given_name,
    std::string_view new_account_email,
    const AccountPreviewDataService::AccountPreviewPreference& preference);

// Returns a formatted title string for the bookmark manager signin promo based
// on the account preview preference.
std::u16string GetAccountPreviewBookmarkManagerPromoTitle(
    const AccountPreviewDataService::AccountPreviewPreference& preference);

// Returns a formatted title string for the password manager signin promo card
// based on the account preview preference.
std::u16string GetAccountPreviewPasswordManagerPromoTitle(
    const AccountPreviewDataService::AccountPreviewPreference& preference);

// Returns a formatted title string for the Smart Account Switching startup
// prompt bubble, personalized with the other device form factor in
// `preference` if available, or falling back to a generic title otherwise.
// Selects either the in-profile switching or new-profile creation copy
// according to the active `switches::kEnableAccountPreviewSwitchingAccount`
// configuration. Requires `switches::kEnableAccountPreviewSwitchingAccount` to
// be enabled.
std::string GetAccountPreviewAccountSwitchingTitle(
    const AccountPreviewDataService::AccountPreviewPreference& preference);

// Returns a formatted subtitle string for the Smart Account Switching startup
// prompt bubble based on the account preview preference and target account
// email, falling back to a generic subtitle if `preference` contains no
// supported preferred data types. Selects either the in-profile switching or
// new-profile creation copy according to the active
// `switches::kEnableAccountPreviewSwitchingAccount` configuration. Requires
// `switches::kEnableAccountPreviewSwitchingAccount` to be enabled.
std::string GetAccountPreviewAccountSwitchingSubtitle(
    std::string_view new_account_email,
    const AccountPreviewDataService::AccountPreviewPreference& preference);

// Returns a formatted label for the Profile Menu "Switch to [Account]" row.
std::u16string GetAccountSwitchingProfileMenuButtonLabel(
    std::string_view email);

}  // namespace signin

#endif  // CHROME_BROWSER_UI_SIGNIN_ACCOUNT_PREVIEW_UTILS_H_
