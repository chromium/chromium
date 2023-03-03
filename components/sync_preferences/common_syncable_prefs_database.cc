// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/common_syncable_prefs_database.h"

#include "base/containers/fixed_flat_set.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/commerce/core/pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/dom_distiller/core/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/metrics/demographics/user_demographics.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/payments/core/payment_prefs.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"

namespace sync_preferences {

const char kSyncablePrefForTesting[] = "syncable-test-preference";

bool CommonSyncablePrefsDatabase::IsPreferenceSyncable(
    const std::string& pref_name) const {
  // List of syncable preferences common across platforms.
  static const auto kCommonSyncablePrefsAllowlist =
      base::MakeFixedFlatSet<base::StringPiece>({
        // clang-format off
        autofill::prefs::kAutofillCreditCardEnabled,
        autofill::prefs::kAutofillEnabledDeprecated,
        autofill::prefs::kAutofillHasSeenIban,
        autofill::prefs::kAutofillIBANEnabled,
        autofill::prefs::kAutofillLastVersionDeduped,
        autofill::prefs::kAutofillLastVersionDisusedAddressesDeleted,
        autofill::prefs::kAutofillProfileEnabled,
        bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
        bookmarks::prefs::kShowBookmarkBar,
        bookmarks::prefs::kShowManagedBookmarksInBookmarkBar,
        browsing_data::prefs::kClearBrowsingDataHistoryNoticeShownTimes,
        browsing_data::prefs::kDeleteBrowsingHistory,
        browsing_data::prefs::kDeleteBrowsingHistoryBasic,
        browsing_data::prefs::kDeleteCache,
        browsing_data::prefs::kDeleteCacheBasic,
        browsing_data::prefs::kDeleteCookies,
        browsing_data::prefs::kDeleteCookiesBasic,
        browsing_data::prefs::kDeleteDownloadHistory,
        browsing_data::prefs::kDeleteFormData,
        browsing_data::prefs::kDeleteHostedAppsData,
        browsing_data::prefs::kDeletePasswords,
        browsing_data::prefs::kDeleteSiteSettings,
        browsing_data::prefs::kDeleteTimePeriod,
        browsing_data::prefs::kDeleteTimePeriodBasic,
        browsing_data::prefs::kLastClearBrowsingDataTime,
        browsing_data::prefs::kPreferencesMigratedToBasic,
        commerce::kPriceEmailNotificationsEnabled,
        dom_distiller::prefs::kFont,
        dom_distiller::prefs::kOfferReaderMode,
        dom_distiller::prefs::kReaderForAccessibility,
        dom_distiller::prefs::kTheme,
        language::prefs::kAcceptLanguages,
        language::prefs::kApplicationLocale,
        language::prefs::kSelectedLanguages,
        metrics::kSyncDemographicsPrefName,
        ntp_tiles::prefs::kCustomLinksInitialized,
        ntp_tiles::prefs::kCustomLinksList,
        omnibox::kKeywordSpaceTriggeringEnabled,
        password_manager::prefs::kCredentialsEnableAutosignin,
        password_manager::prefs::kCredentialsEnableService,
        password_manager::prefs::kPasswordDismissCompromisedAlertEnabled,
        password_manager::prefs::kPasswordLeakDetectionEnabled,
        password_manager::prefs::kSyncedLastTimePasswordCheckCompleted,
        password_manager::prefs::kWasAutoSignInFirstRunExperienceShown,
        payments::kCanMakePaymentEnabled,
        prefs::kAccountTailoredSecurityUpdateTimestamp,
        prefs::kCookieControlsMode,
        prefs::kSafeBrowsingEnabled,
        prefs::kSyncedDefaultSearchProviderGUID,
        translate::TranslatePrefs::kPrefForceTriggerTranslateCount,
        translate::TranslatePrefs::kPrefNeverPromptSitesDeprecated,
        translate::TranslatePrefs::kPrefTranslateAcceptedCount,
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
        translate::TranslatePrefs::kPrefTranslateAutoAlwaysCount,
        translate::TranslatePrefs::kPrefTranslateAutoNeverCount,
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
        translate::TranslatePrefs::kPrefTranslateDeniedCount,
        translate::TranslatePrefs::kPrefTranslateIgnoredCount,
        translate::prefs::kBlockedLanguages,
        translate::prefs::kOfferTranslateEnabled,
        translate::prefs::kPrefAlwaysTranslateList,
        translate::prefs::kPrefNeverPromptSitesWithTime,
        translate::prefs::kPrefTranslateRecentTarget,
        kSyncablePrefForTesting
        // clang-format on
      });
  return kCommonSyncablePrefsAllowlist.count(pref_name);
}

}  // namespace sync_preferences
