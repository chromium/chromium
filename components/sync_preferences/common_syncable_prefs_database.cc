// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/common_syncable_prefs_database.h"

#include "base/containers/fixed_flat_map.h"
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

namespace {
// Not an enum class to ease cast to int.
namespace syncable_prefs_ids {
enum {
  kSyncablePrefForTesting = 0,
  kAutofillCreditCardEnabled = 1,
  kAutofillEnabledDeprecated = 2,
  kAutofillHasSeenIban = 3,
  kAutofillIBANEnabled = 4,
  kAutofillLastVersionDeduped = 5,
  kAutofillLastVersionDisusedAddressesDeleted = 6,
  kAutofillProfileEnabled = 7,
  kShowAppsShortcutInBookmarkBar = 8,
  kShowBookmarkBar = 9,
  kShowManagedBookmarksInBookmarkBar = 10,
  kClearBrowsingDataHistoryNoticeShownTimes = 11,
  kDeleteBrowsingHistory = 12,
  kDeleteBrowsingHistoryBasic = 13,
  kDeleteCache = 14,
  kDeleteCacheBasic = 15,
  kDeleteCookies = 16,
  kDeleteCookiesBasic = 17,
  kDeleteDownloadHistory = 18,
  kDeleteFormData = 19,
  kDeleteHostedAppsData = 20,
  kDeletePasswords = 21,
  kDeleteSiteSettings = 22,
  kDeleteTimePeriod = 23,
  kDeleteTimePeriodBasic = 24,
  kLastClearBrowsingDataTime = 25,
  kPreferencesMigratedToBasic = 26,
  kPriceEmailNotificationsEnabled = 27,
  kFont = 28,
  kOfferReaderMode = 29,
  kReaderForAccessibility = 30,
  kTheme = 31,
  kAcceptLanguages = 32,
  kApplicationLocale = 33,
  kSelectedLanguages = 34,
  kSyncDemographicsPrefName = 35,
  kCustomLinksInitialized = 36,
  kCustomLinksList = 37,
  kKeywordSpaceTriggeringEnabled = 38,
  kCredentialsEnableAutosignin = 39,
  kCredentialsEnableService = 40,
  kPasswordDismissCompromisedAlertEnabled = 41,
  kPasswordLeakDetectionEnabled = 42,
  kSyncedLastTimePasswordCheckCompleted = 43,
  kWasAutoSignInFirstRunExperienceShown = 44,
  kCanMakePaymentEnabled = 45,
  kAccountTailoredSecurityUpdateTimestamp = 46,
  kCookieControlsMode = 47,
  kSafeBrowsingEnabled = 48,
  kSyncedDefaultSearchProviderGUID = 49,
  kPrefForceTriggerTranslateCount = 50,
  kPrefNeverPromptSitesDeprecated = 51,
  kPrefTranslateAcceptedCount = 52,
  kPrefTranslateAutoAlwaysCount = 53,
  kPrefTranslateAutoNeverCount = 54,
  kPrefTranslateDeniedCount = 55,
  kPrefTranslateIgnoredCount = 56,
  kBlockedLanguages = 57,
  kOfferTranslateEnabled = 58,
  kPrefAlwaysTranslateList = 59,
  kPrefNeverPromptSitesWithTime = 60,
  kPrefTranslateRecentTarget = 61
};
}  // namespace syncable_prefs_ids

const auto& SyncablePreferences() {
  // List of syncable preferences common across platforms.
  static const auto kCommonSyncablePrefsAllowlist =
      base::MakeFixedFlatMap<base::StringPiece, SyncablePrefMetadata>({
        {autofill::prefs::kAutofillCreditCardEnabled,
         {syncable_prefs_ids::kAutofillCreditCardEnabled}},
            {autofill::prefs::kAutofillEnabledDeprecated,
             {syncable_prefs_ids::kAutofillEnabledDeprecated}},
            {autofill::prefs::kAutofillHasSeenIban,
             {syncable_prefs_ids::kAutofillHasSeenIban}},
            {autofill::prefs::kAutofillIBANEnabled,
             {syncable_prefs_ids::kAutofillIBANEnabled}},
            {autofill::prefs::kAutofillLastVersionDeduped,
             {syncable_prefs_ids::kAutofillLastVersionDeduped}},
            {autofill::prefs::kAutofillLastVersionDisusedAddressesDeleted,
             {syncable_prefs_ids::kAutofillLastVersionDisusedAddressesDeleted}},
            {autofill::prefs::kAutofillProfileEnabled,
             {syncable_prefs_ids::kAutofillProfileEnabled}},
            {bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
             {syncable_prefs_ids::kShowAppsShortcutInBookmarkBar}},
            {bookmarks::prefs::kShowBookmarkBar,
             {syncable_prefs_ids::kShowBookmarkBar}},
            {bookmarks::prefs::kShowManagedBookmarksInBookmarkBar,
             {syncable_prefs_ids::kShowManagedBookmarksInBookmarkBar}},
            {browsing_data::prefs::kClearBrowsingDataHistoryNoticeShownTimes,
             {syncable_prefs_ids::kClearBrowsingDataHistoryNoticeShownTimes}},
            {browsing_data::prefs::kDeleteBrowsingHistory,
             {syncable_prefs_ids::kDeleteBrowsingHistory}},
            {browsing_data::prefs::kDeleteBrowsingHistoryBasic,
             {syncable_prefs_ids::kDeleteBrowsingHistoryBasic}},
            {browsing_data::prefs::kDeleteCache,
             {syncable_prefs_ids::kDeleteCache}},
            {browsing_data::prefs::kDeleteCacheBasic,
             {syncable_prefs_ids::kDeleteCacheBasic}},
            {browsing_data::prefs::kDeleteCookies,
             {syncable_prefs_ids::kDeleteCookies}},
            {browsing_data::prefs::kDeleteCookiesBasic,
             {syncable_prefs_ids::kDeleteCookiesBasic}},
            {browsing_data::prefs::kDeleteDownloadHistory,
             {syncable_prefs_ids::kDeleteDownloadHistory}},
            {browsing_data::prefs::kDeleteFormData,
             {syncable_prefs_ids::kDeleteFormData}},
            {browsing_data::prefs::kDeleteHostedAppsData,
             {syncable_prefs_ids::kDeleteHostedAppsData}},
            {browsing_data::prefs::kDeletePasswords,
             {syncable_prefs_ids::kDeletePasswords}},
            {browsing_data::prefs::kDeleteSiteSettings,
             {syncable_prefs_ids::kDeleteSiteSettings}},
            {browsing_data::prefs::kDeleteTimePeriod,
             {syncable_prefs_ids::kDeleteTimePeriod}},
            {browsing_data::prefs::kDeleteTimePeriodBasic,
             {syncable_prefs_ids::kDeleteTimePeriodBasic}},
            {browsing_data::prefs::kLastClearBrowsingDataTime,
             {syncable_prefs_ids::kLastClearBrowsingDataTime}},
            {browsing_data::prefs::kPreferencesMigratedToBasic,
             {syncable_prefs_ids::kPreferencesMigratedToBasic}},
            {commerce::kPriceEmailNotificationsEnabled,
             {syncable_prefs_ids::kPriceEmailNotificationsEnabled}},
            {dom_distiller::prefs::kFont, {syncable_prefs_ids::kFont}},
            {dom_distiller::prefs::kOfferReaderMode,
             {syncable_prefs_ids::kOfferReaderMode}},
            {dom_distiller::prefs::kReaderForAccessibility,
             {syncable_prefs_ids::kReaderForAccessibility}},
            {dom_distiller::prefs::kTheme, {syncable_prefs_ids::kTheme}},
            {language::prefs::kAcceptLanguages,
             {syncable_prefs_ids::kAcceptLanguages}},
            {language::prefs::kApplicationLocale,
             {syncable_prefs_ids::kApplicationLocale}},
            {language::prefs::kSelectedLanguages,
             {syncable_prefs_ids::kSelectedLanguages}},
            {metrics::kSyncDemographicsPrefName,
             {syncable_prefs_ids::kSyncDemographicsPrefName}},
            {ntp_tiles::prefs::kCustomLinksInitialized,
             {syncable_prefs_ids::kCustomLinksInitialized}},
            {ntp_tiles::prefs::kCustomLinksList,
             {syncable_prefs_ids::kCustomLinksList}},
            {omnibox::kKeywordSpaceTriggeringEnabled,
             {syncable_prefs_ids::kKeywordSpaceTriggeringEnabled}},
            {password_manager::prefs::kCredentialsEnableAutosignin,
             {syncable_prefs_ids::kCredentialsEnableAutosignin}},
            {password_manager::prefs::kCredentialsEnableService,
             {syncable_prefs_ids::kCredentialsEnableService}},
            {password_manager::prefs::kPasswordDismissCompromisedAlertEnabled,
             {syncable_prefs_ids::kPasswordDismissCompromisedAlertEnabled}},
            {password_manager::prefs::kPasswordLeakDetectionEnabled,
             {syncable_prefs_ids::kPasswordLeakDetectionEnabled}},
            {password_manager::prefs::kSyncedLastTimePasswordCheckCompleted,
             {syncable_prefs_ids::kSyncedLastTimePasswordCheckCompleted}},
            {password_manager::prefs::kWasAutoSignInFirstRunExperienceShown,
             {syncable_prefs_ids::kWasAutoSignInFirstRunExperienceShown}},
            {payments::kCanMakePaymentEnabled,
             {syncable_prefs_ids::kCanMakePaymentEnabled}},
            {prefs::kAccountTailoredSecurityUpdateTimestamp,
             {syncable_prefs_ids::kAccountTailoredSecurityUpdateTimestamp}},
            {prefs::kCookieControlsMode,
             {syncable_prefs_ids::kCookieControlsMode}},
            {prefs::kSafeBrowsingEnabled,
             {syncable_prefs_ids::kSafeBrowsingEnabled}},
            {prefs::kSyncedDefaultSearchProviderGUID,
             {syncable_prefs_ids::kSyncedDefaultSearchProviderGUID}},
            {translate::TranslatePrefs::kPrefForceTriggerTranslateCount,
             {syncable_prefs_ids::kPrefForceTriggerTranslateCount}},
            {translate::TranslatePrefs::kPrefNeverPromptSitesDeprecated,
             {syncable_prefs_ids::kPrefNeverPromptSitesDeprecated}},
            {translate::TranslatePrefs::kPrefTranslateAcceptedCount,
             {syncable_prefs_ids::kPrefTranslateAcceptedCount}},
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
            {translate::TranslatePrefs::kPrefTranslateAutoAlwaysCount,
             {syncable_prefs_ids::kPrefTranslateAutoAlwaysCount}},
            {translate::TranslatePrefs::kPrefTranslateAutoNeverCount,
             {syncable_prefs_ids::kPrefTranslateAutoNeverCount}},
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
            {translate::TranslatePrefs::kPrefTranslateDeniedCount,
             {syncable_prefs_ids::kPrefTranslateDeniedCount}},
            {translate::TranslatePrefs::kPrefTranslateIgnoredCount,
             {syncable_prefs_ids::kPrefTranslateIgnoredCount}},
            {translate::prefs::kBlockedLanguages,
             {syncable_prefs_ids::kBlockedLanguages}},
            {translate::prefs::kOfferTranslateEnabled,
             {syncable_prefs_ids::kOfferTranslateEnabled}},
            {translate::prefs::kPrefAlwaysTranslateList,
             {syncable_prefs_ids::kPrefAlwaysTranslateList}},
            {translate::prefs::kPrefNeverPromptSitesWithTime,
             {syncable_prefs_ids::kPrefNeverPromptSitesWithTime}},
            {translate::prefs::kPrefTranslateRecentTarget,
             {syncable_prefs_ids::kPrefTranslateRecentTarget}},
        {
          kSyncablePrefForTesting, {
            syncable_prefs_ids::kSyncablePrefForTesting
          }
        }
      });
  return kCommonSyncablePrefsAllowlist;
}
}  // namespace

absl::optional<SyncablePrefMetadata>
CommonSyncablePrefsDatabase::GetSyncablePrefMetadata(
    const std::string& pref_name) const {
  const auto* it = SyncablePreferences().find(pref_name);
  if (it == SyncablePreferences().end()) {
    return absl::nullopt;
  }
  return it->second;
}

}  // namespace sync_preferences
