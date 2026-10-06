// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_PREFS_KEYS_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_PREFS_KEYS_H_

#include <array>
#include <string_view>

namespace signin::internal {

// Key names of the per-account preferences stored by `SigninPrefs`, i.e. the
// keys of the dictionary that `SigninPrefs` keeps per `GaiaId` (or within
// specific sub-dictionaries under it, such as
// `AvatarButtonPromoCountDictionary` or `CrossDevicePromoPrefs` -> `history`).
//
// Shared internally between `SigninPrefs`, `SigninPrefsAccessor`, and
// `SigninPrefsRegistry`. External consumers must use the typed `SigninPrefs`
// API.
//
// IMPORTANT: Every active key added here must also be registered in
// `kRegisteredSigninPrefs` (`signin_prefs_registry.cc`) with its enclosing
// parent path and `base::Value::Type`, and exercised in
// `SigninPrefsTest.AllWrittenPrefsAreRegisteredWithMatchingType` under every
// feature state that selects it.
//
// A key is a single dictionary key. A '.' inside a key is part of the name, it
// never denotes nesting.

// LINT.IfChange(SigninPrefsKeys)

// Pref used to track the last time the user signed out of Chrome.
inline constexpr std::string_view kChromeLastSignoutTime =
    "kChromeLastSignoutTime";

// Pref used to store the user choice for the Chrome Signin Intercept.
inline constexpr std::string_view kChromeSigninInterceptionUserChoice =
    "ChromeSigninInterceptionUserChoice";

// Pref used to track the last time the Chrome Signin bubble was declined. It is
// used to know when to allow future reprompts if the conditions are met. The
// pref will be cleared if the Chrome Signin setting equivalent to showing the
// bubble upon web signin is set to `ChromeSigninUserChoice::kDoNotSignin`, in
// order not to consider the bubble decline interaction anymore.
inline constexpr std::string_view
    kChromeSigninInterceptionLastBubbleDeclineTime =
        "ChromeSigninInterceptionLastBubbleDeclineTime";

// Pref used to track the number of times the Chrome Signin bubble was
// reprompted. It is used to know when to allow future reprompts.
// The pref will be cleared if the Chrome Signin setting equivalent to showing
// the bubble upon web signin is set to `ChromeSigninUserChoice::kDoNotSignin`,
// in order not to consider the bubble decline interaction anymore.
inline constexpr std::string_view kChromeSigninInterceptionRepromptCount =
    "ChromeSigninInterceptionRepromptCount";

// Pref used to store the number of dismisses of the Chrome Signin Bubble.
inline constexpr std::string_view kChromeSigninInterceptionDismissCount =
    "ChromeSigninInterceptionDismissCount";

// Pref to store the number of times the password bubble signin promo
// has been shown per account.
inline constexpr std::string_view kPasswordSignInPromoShownCount =
    "PasswordSignInPromoShownCount";

// Pref to store the number of times the address bubble signin promo
// has been shown per account.
inline constexpr std::string_view kAddressSignInPromoShownCount =
    "AddressSignInPromoShownCount";

// Pref to store the number of times the bookmark bubble signin promo
// has been shown per account.
inline constexpr std::string_view kBookmarkSignInPromoShownCount =
    "BookmarkSignInPromoShownCount";

// Pref to store the number of times any autofill bubble signin promo
// has been dismissed per account.
inline constexpr std::string_view kAutofillSignInPromoDismissCount =
    "AutofillSignInPromoDismissCount";

// Pref to store the number of times the bookmark manager signin promo has been
// shown per account.
inline constexpr std::string_view kBookmarkManagerSignInPromoShownCount =
    "BookmarkManagerSignInPromoShownCount";

// Pref to store the number of times the bookmark manager signin promo has been
// dismissed per account.
inline constexpr std::string_view kBookmarkManagerSignInPromoDismissCount =
    "BookmarkManagerSignInPromoDismissCount";

// Pref to store the number of times the Search AI Mode bubble signin promo
// has been shown per account.
inline constexpr std::string_view kSearchAIModeSignInPromoShownCount =
    "SearchAIModeSignInPromoShownCount";

// Pref to store the number of times the Search AI Mode bubble signin promo
// has been dismissed per account.
inline constexpr std::string_view kSearchAIModeSignInPromoDismissCount =
    "SearchAIModeSignInPromoDismissCount";

// Pref used to track the last time the Search AI Mode bubble signin promo was
// shown.
inline constexpr std::string_view kSearchAIModeSignInPromoLastImpressionTime =
    "SearchAIModeSignInPromoLastImpressionTime";

// Registers that the sign in occurred with an explicit user action from the
// bubble that appears after installing an extension. False by default.
inline constexpr std::string_view kExtensionsExplicitBrowserSigninEnabled =
    "ExtensionsExplicitBrowserSigninEnabled";

// Registers that the sign in occurred with an explicit user action from the
// bookmark sign-in promo. False by default.
inline constexpr std::string_view kBookmarksExplicitBrowserSigninEnabled =
    "BookmarksExplicitBrowserSigninEnabled";

// History sync promo on the history page.
//
// A timestamp of the last time the history sync promo was dismissed.
inline constexpr std::string_view
    kHistoryPageHistorySyncPromoLastDismissedTimestamp =
        "history_page.history_sync_promo_last_dismissed_timestamp";
// A boolean preference to store whether the history sync promo was shown one
// more time after the user dismissed it.
inline constexpr std::string_view
    kHistoryPageHistorySyncPromoShownAfterDismissal =
        "history_page.history_sync_promo_shown_after_dismissal";
// An integer preference to store the number of times the history sync promo
// has been shown on the history page.
inline constexpr std::string_view kHistoryPageHistorySyncPromoShownCount =
    "history.sync_promo_shown_count";

// Number of times the Bookmark Batch Upload promo was dismissed.
inline constexpr std::string_view kBookmarkBatchUploadPromoDismissCount =
    "BookmarkBatchUploadPromoDismissCount";
// The time at which the last Bookmark Batch Upload promo was dismissed.
inline constexpr std::string_view kBookmarkBatchUploadPromoLastDismissTime =
    "BookmarkBatchUploadPromoLastDismissTime";

// The remaining number of local data items that were not moved after the last
// batch upload.
inline constexpr std::string_view
    kBatchUploadLastUploadRemainingLocalDataCount =
        "BatchUploadLastUploadRemainingLocalDataCount";

inline constexpr std::string_view kPolicyDisclaimerLastRegistrationFailureTime =
    "PolicyDisclaimerLastRegistrationFailureTime";

// The stable account ID for metrics.
inline constexpr std::string_view kAccountMetricsId = "AccountMetricsId";
// Boolean indicating if the account is capped for metrics ID allocation.
// Being capped means no new IDs will be allocated because the limit of 100
// accounts has been reached.
inline constexpr std::string_view kAccountMetricsIdIsCapped =
    "AccountMetricsIdIsCapped";

// Pref sub-dictionary key inside the main account metadata dictionary
// to store cross-device signin promo details.
inline constexpr std::string_view kCrossDevicePromoPrefs =
    "CrossDevicePromoPrefs";
// Dictionary keys for data type specific promo data.
inline constexpr std::string_view kCrossDevicePromoHistoryDictKey = "history";
// Sub-dictionary serialization keys to be used per data type.
inline constexpr std::string_view kCrossDevicePromoShownCountKey =
    "shown_count";
inline constexpr std::string_view kCrossDevicePromoLastDismissedTimeKey =
    "last_dismissed_time";
inline constexpr std::string_view kCrossDevicePromoShownAfterDismissalKey =
    "shown_after_dismissal";

// Parent path for data-type sub-dictionaries under `kCrossDevicePromoPrefs`.
inline constexpr auto kCrossDevicePromoRootParents =
    std::to_array<std::string_view>({kCrossDevicePromoPrefs});

// Common parent path for cross-device history promo prefs within an account
// dictionary.
inline constexpr auto kCrossDeviceHistoryPromoParents =
    std::to_array<std::string_view>(
        {kCrossDevicePromoPrefs, kCrossDevicePromoHistoryDictKey});

// Dictionary pref that contains all the values related to the avatar button
// promo counts.
inline constexpr std::string_view kAvatarButtonPromoCountDictionary =
    "AvatarButtonPromoCountDictionary";

// Common parent path for avatar button promo prefs within an account
// dictionary.
inline constexpr auto kAvatarButtonPromoParents =
    std::to_array<std::string_view>({kAvatarButtonPromoCountDictionary});

// Shown and used counts for each avatar button promo variant, stored under
// `kAvatarButtonPromoCountDictionary`.
inline constexpr std::string_view kAvatarButtonHistorySyncPromoShownCount =
    "AvatarButtonHistorySyncPromoShownCount";
inline constexpr std::string_view kAvatarButtonHistorySyncPromoUsedCount =
    "AvatarButtonHistorySyncPromoUsedCount";
inline constexpr std::string_view kAvatarButtonBatchUploadPromoShownCount =
    "AvatarButtonBatchUploadPromoShownCount";
inline constexpr std::string_view kAvatarButtonBatchUploadPromoUsedCount =
    "AvatarButtonBatchUploadPromoUsedCount";
inline constexpr std::string_view
    kAvatarButtonBatchUploadBookmarkPromoShownCount =
        "AvatarButtonBatchUploadBookmarkPromoShownCount";
inline constexpr std::string_view
    kAvatarButtonBatchUploadBookmarkPromoUsedCount =
        "AvatarButtonBatchUploadBookmarkPromoUsedCount";
inline constexpr std::string_view
    kAvatarButtonBatchUploadWindows10DepreciationPromoShownCount =
        "AvatarButtonBatchUploadWindows10DepreciationPromoShownCount";
inline constexpr std::string_view
    kAvatarButtonBatchUploadWindows10DepreciationPromoUsedCount =
        "AvatarButtonBatchUploadWindows10DepreciationPromoUsedCount";
// Shown count, used count, and last-shown timestamp for the avatar button
// sign-in promo, stored per `GaiaId` under `kAvatarButtonPromoCountDictionary`
// when an account is present (or in a separate profile-scoped dictionary when
// signed out without an account).
inline constexpr std::string_view kAvatarButtonSigninPromoShownCount =
    "AvatarButtonSigninPromoShownCount";
inline constexpr std::string_view kAvatarButtonSigninPromoUsedCount =
    "AvatarButtonSigninPromoUsedCount";
inline constexpr std::string_view kAvatarButtonSigninPromoLastShownTime =
    "AvatarButtonSigninPromoLastShownTime";

// LINT.ThenChange(//components/signin/public/base/signin_prefs_registry.cc:SigninPrefsKeys)

// -----------------------------------------------------------------------------
// DEPRECATED prefs: Check `SigninPrefs::MigrateObsoleteSigninPrefs()`.
//
// Testing deprecating pref:
inline constexpr std::string_view kDeprecatedTestingPref =
    "DeprecatingTestingPref";

// DEPRECATED(10/2025):
// History Sync promo on the avatar button.
//
// Number of times the history sync promo was shown in the identity pill (avatar
// toolbar button).
inline constexpr std::string_view
    kDeprecatedHistorySyncPromoIdentityPillShownCount =
        "ChromeSigninSyncPromoIdentityPillShownCount";
// Number of times the history sync promo was used (clicked) in the identity
// pill (avatar toolbar button).
inline constexpr std::string_view
    kDeprecatedHistorySyncPromoIdentityPillUsedCount =
        "ChromeSigninSyncPromoIdentityPillUsedCount";

// DEPRECATED(09/2026):
// Sync promo on the avatar button.
//
// Number of times the sync promo was shown in the identity pill (avatar toolbar
// button).
inline constexpr std::string_view kDeprecatedSyncPromoIdentityPillShownCount =
    "SyncPromoIdentityPillShownCount";
// Number of times the sync promo was used (clicked) in the identity pill
// (avatar toolbar button).
inline constexpr std::string_view kDeprecatedSyncPromoIdentityPillUsedCount =
    "SyncPromoIdentityPillUsedCount";

// DEPRECATED(09/2026):
// SigninPromoLimitsExperiment per-account shown and dismiss counts.
inline constexpr std::string_view
    kDeprecatedPasswordSignInPromoShownCountForLimitsExperiment =
        "PasswordSignInPromoShownCountForLimitsExperiment";
inline constexpr std::string_view
    kDeprecatedAddressSignInPromoShownCountForLimitsExperiment =
        "AddressSignInPromoShownCountForLimitsExperiment";
inline constexpr std::string_view
    kDeprecatedBookmarkSignInPromoShownCountForLimitsExperiment =
        "BookmarkSignInPromoShownCountForLimitsExperiment";
inline constexpr std::string_view kDeprecatedAddressSignInPromoDismissCount =
    "AddressSignInPromoDismissCount";
inline constexpr std::string_view kDeprecatedBookmarkSignInPromoDismissCount =
    "BookmarkSignInPromoDismissCount";
inline constexpr std::string_view kDeprecatedPasswordSignInPromoDismissCount =
    "PasswordSignInPromoDismissCount";

// All deprecated account-dict pref keys removed in
// `SigninPrefs::MigrateObsoleteSigninPrefs()`.
inline constexpr auto kDeprecatedSigninPrefs = std::to_array<std::string_view>({
    kDeprecatedTestingPref,
    kDeprecatedHistorySyncPromoIdentityPillShownCount,
    kDeprecatedHistorySyncPromoIdentityPillUsedCount,
    kDeprecatedSyncPromoIdentityPillShownCount,
    kDeprecatedSyncPromoIdentityPillUsedCount,
    kDeprecatedPasswordSignInPromoShownCountForLimitsExperiment,
    kDeprecatedAddressSignInPromoShownCountForLimitsExperiment,
    kDeprecatedBookmarkSignInPromoShownCountForLimitsExperiment,
    kDeprecatedAddressSignInPromoDismissCount,
    kDeprecatedBookmarkSignInPromoDismissCount,
    kDeprecatedPasswordSignInPromoDismissCount,
});
//
// End of DEPRECATED prefs.
// -----------------------------------------------------------------------------

}  // namespace signin::internal

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_PREFS_KEYS_H_
