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
#include "components/variations/service/google_groups_updater_service.h"

namespace sync_preferences {

const char kSyncablePrefForTesting[] = "syncable-test-preference";
const char kSyncableMergeableDictPrefForTesting[] =
    "syncable-mergeable-dict-test-preference";

namespace {
// Not an enum class to ease cast to int.
namespace syncable_prefs_ids {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. When adding a new entry, append the
// enumerator to the end and add it to the `SyncablePref` enum in
// tools/metrics/histograms/enums.xml. When removing an unused enumerator,
// comment it out here, making it clear the value was previously used, and
// add "(obsolete)" to the corresponding entry in enums.xml.
enum {
  kSyncablePrefForTesting = 0,  // For tests.
  kAutofillCreditCardEnabled = 1,
  // kAutofillEnabledDeprecated = 2, (deprecated)
  kAutofillHasSeenIban = 3,
  // kAutofillIbanEnabled = 4,  (obsolete).
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
  // kPrefNeverPromptSitesDeprecated = 51, (deprecated)
  kPrefTranslateAcceptedCount = 52,
  kPrefTranslateAutoAlwaysCount = 53,
  kPrefTranslateAutoNeverCount = 54,
  kPrefTranslateDeniedCount = 55,
  // kPrefTranslateIgnoredCount = 56, (no longer synced)
  kBlockedLanguages = 57,
  kOfferTranslateEnabled = 58,
  kPrefAlwaysTranslateList = 59,
  kPrefNeverPromptSitesWithTime = 60,
  kPrefTranslateRecentTarget = 61,
  kDogfoodGroupsSyncPrefName = 62,
  kSyncableMergeableDictPrefForTesting = 63,  // For tests.
  kAutofillPaymentCvcStorage = 64,
  // See components/sync_preferences/README.md about adding new entries here.
  // vvvvv IMPORTANT! vvvvv
  // Note to the reviewer: IT IS YOUR RESPONSIBILITY to ensure that new syncable
  // prefs follow privacy guidelines! See the readme file linked above for
  // guidance and escalation path in case anything is unclear.
  // ^^^^^ IMPORTANT! ^^^^^
};
}  // namespace syncable_prefs_ids

const auto& SyncablePreferences() {
  // List of syncable preferences common across platforms.
  static const auto kCommonSyncablePrefsAllowlist =
      base::MakeFixedFlatMap<base::StringPiece, SyncablePrefMetadata>({
        {autofill::prefs::kAutofillCreditCardEnabled,
         {syncable_prefs_ids::kAutofillCreditCardEnabled, syncer::PREFERENCES,
          false, MergeBehavior::kNone}},
            {autofill::prefs::kAutofillHasSeenIban,
             {syncable_prefs_ids::kAutofillHasSeenIban, syncer::PREFERENCES,
              false, MergeBehavior::kNone}},
            {autofill::prefs::kAutofillLastVersionDeduped,
             {syncable_prefs_ids::kAutofillLastVersionDeduped,
              syncer::PREFERENCES, false, MergeBehavior::kNone}},
            {autofill::prefs::kAutofillLastVersionDisusedAddressesDeleted,
             {syncable_prefs_ids::kAutofillLastVersionDisusedAddressesDeleted,
              syncer::PREFERENCES, false, MergeBehavior::kNone}},
            {autofill::prefs::kAutofillProfileEnabled,
             {syncable_prefs_ids::kAutofillProfileEnabled, syncer::PREFERENCES,
              false, MergeBehavior::kNone}},
            {bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
             {syncable_prefs_ids::kShowAppsShortcutInBookmarkBar,
              syncer::PREFERENCES, false, MergeBehavior::kNone}},
            {bookmarks::prefs::kShowBookmarkBar,
             {syncable_prefs_ids::kShowBookmarkBar, syncer::PREFERENCES, false,
              MergeBehavior::kNone}},
            {bookmarks::prefs::kShowManagedBookmarksInBookmarkBar,
             {syncable_prefs_ids::kShowManagedBookmarksInBookmarkBar,
              syncer::PREFERENCES, false, MergeBehavior::kNone}},
            {browsing_data::prefs::kClearBrowsingDataHistoryNoticeShownTimes,
             {syncable_prefs_ids::kClearBrowsingDataHistoryNoticeShownTimes,
              syncer::PREFERENCES, false, MergeBehavior::kNone}},
            {browsing_data::prefs::kDeleteBrowsingHistory,
             {syncable_prefs_ids::kDeleteBrowsingHistory, syncer::PREFERENCES,
              false, MergeBehavior::kNone}},
            {browsing_data::prefs::kDeleteBrowsingHistoryBasic,
             {syncable_prefs_ids::kDeleteBrowsingHistoryBasic,
              syncer::PREFERENCES, false, MergeBehavior::kNone}},
            {browsing_data::prefs::kDeleteCache,
             {syncable_prefs_ids::kDeleteCache, syncer::PREFERENCES, false,
              MergeBehavior::kNone}},
            {browsing_data::prefs::kDeleteCacheBasic,
             {syncable_prefs_ids::kDeleteCacheBasic, syncer::PREFERENCES, false,
              MergeBehavior::kNone}},
            {browsing_data::prefs::kDeleteCookies,
             {syncable_prefs_ids::kDeleteCookies, syncer::PREFERENCES, false,
              MergeBehavior::kNone}},
            {browsing_data::prefs::kDeleteCookiesBasic,
             {syncable_prefs_ids::kDeleteCookiesBasic, syncer::PREFERENCES,
              false, MergeBehavior::kNone}},
            {browsing_data::prefs::kDeleteDownloadHistory,
             {syncable_prefs_ids::kDeleteDownloadHistory, syncer::PREFERENCES,
              false, MergeBehavior::kNone}},
            {browsing_data::prefs::kDeleteFormData,
             {syncable_prefs_ids::kDeleteFormData, syncer::PREFERENCES, false,
              MergeBehavior::kNone}},
            {browsing_data::prefs::kDeleteHostedAppsData,
             {syncable_prefs_ids::kDeleteHostedAppsData, syncer::PREFERENCES,
              false, MergeBehavior::kNone}},
            {browsing_data::prefs::kDeletePasswords,
             {syncable_prefs_ids::kDeletePasswords, syncer::PREFERENCES, false,
              MergeBehavior::kNone}},
            {browsing_data::prefs::kDeleteSiteSettings,
             {syncable_prefs_ids::kDeleteSiteSettings, syncer::PREFERENCES,
              false, MergeBehavior::kNone}},
            {browsing_data::prefs::kDeleteTimePeriod,
             {syncable_prefs_ids::kDeleteTimePeriod, syncer::PREFERENCES, false,
              MergeBehavior::kNone}},
            {browsing_data::prefs::kDeleteTimePeriodBasic,
             {syncable_prefs_ids::kDeleteTimePeriodBasic, syncer::PREFERENCES,
              false, MergeBehavior::kNone}},
            {browsing_data::prefs::kLastClearBrowsingDataTime,
             {syncable_prefs_ids::kLastClearBrowsingDataTime,
              syncer::PREFERENCES, false, MergeBehavior::kNone}},
            {browsing_data::prefs::kPreferencesMigratedToBasic,
             {syncable_prefs_ids::kPreferencesMigratedToBasic,
              syncer::PREFERENCES, false, MergeBehavior::kNone}},
            {commerce::kPriceEmailNotificationsEnabled,
             {syncable_prefs_ids::kPriceEmailNotificationsEnabled,
              syncer::PREFERENCES, false, MergeBehavior::kNone}},
            {dom_distiller::prefs::kFont,
             {syncable_prefs_ids::kFont, syncer::PREFERENCES, false,
              MergeBehavior::kNone}},
            {dom_distiller::prefs::kOfferReaderMode,
             {syncable_prefs_ids::kOfferReaderMode, syncer::PREFERENCES, false,
              MergeBehavior::kNone}},
            {dom_distiller::prefs::kReaderForAccessibility,
             {syncable_prefs_ids::kReaderForAccessibility, syncer::PREFERENCES,
              false, MergeBehavior::kNone}},
            {dom_distiller::prefs::kTheme,
             {syncable_prefs_ids::kTheme, syncer::PREFERENCES, false,
              MergeBehavior::kNone}},
            {language::prefs::kAcceptLanguages,
             {syncable_prefs_ids::kAcceptLanguages, syncer::PREFERENCES, false,
              MergeBehavior::kNone}},
            // TODO(crbug.com/1424774): Move this to
            // chrome_syncable_prefs_database.
            {language::prefs::kApplicationLocale,
             {syncable_prefs_ids::kApplicationLocale,
              syncer::OS_PRIORITY_PREFERENCES, false, MergeBehavior::kNone}},
            {language::prefs::kSelectedLanguages,
             {syncable_prefs_ids::kSelectedLanguages, syncer::PREFERENCES,
              false, MergeBehavior::kNone}},
            {metrics::kSyncDemographicsPrefName,
             {syncable_prefs_ids::kSyncDemographicsPrefName,
              syncer::PRIORITY_PREFERENCES, false, MergeBehavior::kNone}},
            {ntp_tiles::prefs::kCustomLinksInitialized,
             {syncable_prefs_ids::kCustomLinksInitialized, syncer::PREFERENCES,
              false, MergeBehavior::kNone}},
            {ntp_tiles::prefs::kCustomLinksList,
             {syncable_prefs_ids::kCustomLinksList, syncer::PREFERENCES, true,
              MergeBehavior::kNone}},
            {omnibox::kKeywordSpaceTriggeringEnabled,
             {syncable_prefs_ids::kKeywordSpaceTriggeringEnabled,
              syncer::PREFERENCES, false, MergeBehavior::kNone}},
            {password_manager::prefs::kCredentialsEnableAutosignin,
             {syncable_prefs_ids::kCredentialsEnableAutosignin,
              syncer::PRIORITY_PREFERENCES, false, MergeBehavior::kNone}},
            {password_manager::prefs::kCredentialsEnableService,
             {syncable_prefs_ids::kCredentialsEnableService,
              syncer::PRIORITY_PREFERENCES, false, MergeBehavior::kNone}},
            {password_manager::prefs::kPasswordDismissCompromisedAlertEnabled,
             {syncable_prefs_ids::kPasswordDismissCompromisedAlertEnabled,
              syncer::PREFERENCES, false, MergeBehavior::kNone}},
            {password_manager::prefs::kPasswordLeakDetectionEnabled,
             {syncable_prefs_ids::kPasswordLeakDetectionEnabled,
              syncer::PREFERENCES, false, MergeBehavior::kNone}},
            {password_manager::prefs::kSyncedLastTimePasswordCheckCompleted,
             {syncable_prefs_ids::kSyncedLastTimePasswordCheckCompleted,
              syncer::PRIORITY_PREFERENCES, false, MergeBehavior::kNone}},
            {password_manager::prefs::kWasAutoSignInFirstRunExperienceShown,
             {syncable_prefs_ids::kWasAutoSignInFirstRunExperienceShown,
              syncer::PRIORITY_PREFERENCES, false, MergeBehavior::kNone}},
            {payments::kCanMakePaymentEnabled,
             {syncable_prefs_ids::kCanMakePaymentEnabled, syncer::PREFERENCES,
              false, MergeBehavior::kNone}},
            {prefs::kAccountTailoredSecurityUpdateTimestamp,
             {syncable_prefs_ids::kAccountTailoredSecurityUpdateTimestamp,
              syncer::PRIORITY_PREFERENCES, false, MergeBehavior::kNone}},
            {prefs::kCookieControlsMode,
             {syncable_prefs_ids::kCookieControlsMode, syncer::PREFERENCES,
              false, MergeBehavior::kNone}},
            {prefs::kSafeBrowsingEnabled,
             {syncable_prefs_ids::kSafeBrowsingEnabled, syncer::PREFERENCES,
              false, MergeBehavior::kNone}},
// TODO(crbug.com/1434910): Maybe move to chrome_syncable_prefs_database.cc,
// see bug.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
            {prefs::kSyncedDefaultSearchProviderGUID,
             {syncable_prefs_ids::kSyncedDefaultSearchProviderGUID,
              syncer::PREFERENCES, false, MergeBehavior::kNone}},
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
            {translate::TranslatePrefs::kPrefForceTriggerTranslateCount,
             {syncable_prefs_ids::kPrefForceTriggerTranslateCount,
              syncer::PREFERENCES, false, MergeBehavior::kNone}},
            {translate::TranslatePrefs::kPrefTranslateAcceptedCount,
             {syncable_prefs_ids::kPrefTranslateAcceptedCount,
              syncer::PREFERENCES, false, MergeBehavior::kNone}},
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
            {translate::TranslatePrefs::kPrefTranslateAutoAlwaysCount,
             {syncable_prefs_ids::kPrefTranslateAutoAlwaysCount,
              syncer::PREFERENCES, false, MergeBehavior::kNone}},
            {translate::TranslatePrefs::kPrefTranslateAutoNeverCount,
             {syncable_prefs_ids::kPrefTranslateAutoNeverCount,
              syncer::PREFERENCES, false, MergeBehavior::kNone}},
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
            {translate::TranslatePrefs::kPrefTranslateDeniedCount,
             {syncable_prefs_ids::kPrefTranslateDeniedCount,
              syncer::PREFERENCES, false, MergeBehavior::kNone}},
            {translate::prefs::kBlockedLanguages,
             {syncable_prefs_ids::kBlockedLanguages, syncer::PREFERENCES, false,
              MergeBehavior::kNone}},
            {translate::prefs::kOfferTranslateEnabled,
             {syncable_prefs_ids::kOfferTranslateEnabled, syncer::PREFERENCES,
              false, MergeBehavior::kNone}},
            {translate::prefs::kPrefAlwaysTranslateList,
             {syncable_prefs_ids::kPrefAlwaysTranslateList, syncer::PREFERENCES,
              false, MergeBehavior::kNone}},
            {translate::prefs::kPrefNeverPromptSitesWithTime,
             {syncable_prefs_ids::kPrefNeverPromptSitesWithTime,
              syncer::PREFERENCES, true, MergeBehavior::kNone}},
            {translate::prefs::kPrefTranslateRecentTarget,
             {syncable_prefs_ids::kPrefTranslateRecentTarget,
              syncer::PREFERENCES, false, MergeBehavior::kNone}},
// For Ash, the OS_PRIORITY_PREFERENCES equivalent is defined in
// chrome/browser/sync/prefs/chrome_syncable_prefs_database.cc instead.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
            {variations::kDogfoodGroupsSyncPrefName,
             {syncable_prefs_ids::kDogfoodGroupsSyncPrefName,
              syncer::PRIORITY_PREFERENCES, false, MergeBehavior::kNone}},
#endif
            {kSyncablePrefForTesting,
             {syncable_prefs_ids::kSyncablePrefForTesting, syncer::PREFERENCES,
              false, MergeBehavior::kNone}},
            {kSyncableMergeableDictPrefForTesting,
             {syncable_prefs_ids::kSyncableMergeableDictPrefForTesting,
              syncer::PREFERENCES, false, MergeBehavior::kMergeableDict}},
            {autofill::prefs::kAutofillPaymentCvcStorage,
             {syncable_prefs_ids::kAutofillPaymentCvcStorage,
              syncer::PREFERENCES, false, MergeBehavior::kNone}},
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
