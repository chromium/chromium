// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/common_syncable_prefs_database.h"

#include <string_view>

#include "base/containers/fixed_flat_map.h"
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
#include "components/saved_tab_groups/public/pref_names.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/sharing_message/pref_names.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/variations/service/google_groups_manager_prefs.h"

namespace sync_preferences {

namespace {
// Not an enum class to ease cast to int.
namespace syncable_prefs_ids {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. When adding a new entry, append the
// enumerator to the end and add it to the `SyncablePref` enum in
// tools/metrics/histograms/metadata/sync/enums.xml. When removing an unused
// enumerator, comment it out here, making it clear the value was previously
// used, and add "(obsolete)" to the corresponding entry in enums.xml.
// LINT.IfChange(CommonSyncablePref)
enum {
  kSyncablePrefForTesting = 0,  // For tests.
  kAutofillCreditCardEnabled = 1,
  // kAutofillEnabledDeprecated = 2, (deprecated)
  kAutofillHasSeenIban = 3,
  // kAutofillIbanEnabled = 4,  (obsolete).
  kAutofillLastVersionDeduped = 5,
  // kAutofillLastVersionDisusedAddressesDeleted = 6, (deprecated)
  kAutofillProfileEnabled = 7,
  kShowAppsShortcutInBookmarkBar = 8,
  kShowBookmarkBar = 9,
  kShowManagedBookmarksInBookmarkBar = 10,
  // kClearBrowsingDataHistoryNoticeShownTimes = 11, (was never actually synced)
  // kDeleteBrowsingHistory = 12, (no longer synced)
  // kDeleteBrowsingHistoryBasic = 13, (no longer synced)
  // kDeleteCache = 14, (no longer synced)
  // kDeleteCacheBasic = 15, (no longer synced)
  // kDeleteCookies = 16, (no longer synced)
  // kDeleteCookiesBasic = 17, (no longer synced)
  // kDeleteDownloadHistory = 18, (no longer synced)
  // kDeleteFormData = 19, (no longer synced)
  // kDeleteHostedAppsData = 20, (no longer synced)
  // kDeletePasswords = 21, (no longer synced)
  // kDeleteSiteSettings = 22, (no longer synced)
  // kDeleteTimePeriod = 23, (no longer synced)
  // kDeleteTimePeriodBasic = 24, (no longer synced)
  kLastClearBrowsingDataTime = 25,
  // kPreferencesMigratedToBasic = 26, (deprecated)
  kPriceEmailNotificationsEnabled = 27,
  kFont = 28,
  // kOfferReaderMode = 29, (deprecated)
  kReaderForAccessibility = 30,
  kTheme = 31,
  kAcceptLanguages = 32,
  // kApplicationLocale = 33,  (moved to chrome_syncable_prefs_database.cc)
  kSelectedLanguages = 34,
  kSyncDemographicsPrefName = 35,
  kCustomLinksInitialized = 36,
  kCustomLinksList = 37,
  kKeywordSpaceTriggeringEnabled = 38,
  kCredentialsEnableAutosignin = 39,
  kCredentialsEnableService = 40,
  kPasswordDismissCompromisedAlertEnabled = 41,
  kPasswordLeakDetectionEnabled = 42,
  // kSyncedLastTimePasswordCheckCompleted = 43, (deprecated)
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
  // kDeleteTimePeriodV2 = 65, (no longer synced)
  // kDeleteTimePeriodV2Basic = 66, (no longer synced)
  kSyncableMergeableListPrefForTesting = 67,         // For tests.
  kSyncableHistorySensitiveListPrefForTesting = 68,  // For tests.
  kAutofillPaymentCardBenefits = 69,
  // kCloseTabs = 70, (no longer synced)
  kShowTabGroupsInBookmarkBar = 71,
  kFacilitatedPaymentsPix = 72,
  kSyncableTabGroups = 73,
  kAutoPinNewTabGroups = 74,
  kShowGoogleLensShortcut = 75,
  kSharingVapidKey = 76,
  // See components/sync_preferences/README.md about adding new entries here.
  // vvvvv IMPORTANT! vvvvv
  // Note to the reviewer: IT IS YOUR RESPONSIBILITY to ensure that new syncable
  // prefs follow privacy guidelines! See the readme file linked above for
  // guidance and escalation path in case anything is unclear.
  // ^^^^^ IMPORTANT! ^^^^^
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:CommonSyncablePref)
}  // namespace syncable_prefs_ids

// List of syncable preferences common across platforms.
constexpr auto kCommonSyncablePrefsAllowlist =
    base::MakeFixedFlatMap<std::string_view, SyncablePrefMetadata>({
        {autofill::prefs::kAutofillCreditCardEnabled,
         {syncable_prefs_ids::kAutofillCreditCardEnabled, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {autofill::prefs::kAutofillHasSeenIban,
         {syncable_prefs_ids::kAutofillHasSeenIban, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {autofill::prefs::kAutofillLastVersionDeduped,
         {syncable_prefs_ids::kAutofillLastVersionDeduped, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {autofill::prefs::kAutofillProfileEnabled,
         {syncable_prefs_ids::kAutofillProfileEnabled, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
         {syncable_prefs_ids::kShowAppsShortcutInBookmarkBar,
          syncer::PREFERENCES, PrefSensitivity::kNone, MergeBehavior::kNone}},
        {bookmarks::prefs::kShowTabGroupsInBookmarkBar,
         {syncable_prefs_ids::kShowTabGroupsInBookmarkBar, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {bookmarks::prefs::kShowBookmarkBar,
         {syncable_prefs_ids::kShowBookmarkBar, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {bookmarks::prefs::kShowManagedBookmarksInBookmarkBar,
         {syncable_prefs_ids::kShowManagedBookmarksInBookmarkBar,
          syncer::PREFERENCES, PrefSensitivity::kNone, MergeBehavior::kNone}},
        {browsing_data::prefs::kLastClearBrowsingDataTime,
         {syncable_prefs_ids::kLastClearBrowsingDataTime, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {commerce::kPriceEmailNotificationsEnabled,
         {syncable_prefs_ids::kPriceEmailNotificationsEnabled,
          syncer::PREFERENCES, PrefSensitivity::kNone, MergeBehavior::kNone}},
        {dom_distiller::prefs::kFont,
         {syncable_prefs_ids::kFont, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {dom_distiller::prefs::kReaderForAccessibility,
         {syncable_prefs_ids::kReaderForAccessibility, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {dom_distiller::prefs::kTheme,
         {syncable_prefs_ids::kTheme, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {language::prefs::kAcceptLanguages,
         {syncable_prefs_ids::kAcceptLanguages, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {language::prefs::kSelectedLanguages,
         {syncable_prefs_ids::kSelectedLanguages, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {metrics::kSyncDemographicsPrefName,
         {syncable_prefs_ids::kSyncDemographicsPrefName,
          syncer::PRIORITY_PREFERENCES, PrefSensitivity::kNone,
          MergeBehavior::kNone}},
        {ntp_tiles::prefs::kCustomLinksInitialized,
         {syncable_prefs_ids::kCustomLinksInitialized, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {ntp_tiles::prefs::kCustomLinksList,
         {syncable_prefs_ids::kCustomLinksList, syncer::PREFERENCES,
          PrefSensitivity::kSensitiveRequiresHistory, MergeBehavior::kNone}},
        {omnibox::kKeywordSpaceTriggeringEnabled,
         {syncable_prefs_ids::kKeywordSpaceTriggeringEnabled,
          syncer::PREFERENCES, PrefSensitivity::kNone, MergeBehavior::kNone}},
        {omnibox::kShowGoogleLensShortcut,
         {syncable_prefs_ids::kShowGoogleLensShortcut, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {password_manager::prefs::kCredentialsEnableAutosignin,
         {syncable_prefs_ids::kCredentialsEnableAutosignin,
          syncer::PRIORITY_PREFERENCES, PrefSensitivity::kNone,
          MergeBehavior::kNone}},
        {password_manager::prefs::kCredentialsEnableService,
         {syncable_prefs_ids::kCredentialsEnableService,
          syncer::PRIORITY_PREFERENCES, PrefSensitivity::kNone,
          MergeBehavior::kNone}},
        {password_manager::prefs::kPasswordDismissCompromisedAlertEnabled,
         {syncable_prefs_ids::kPasswordDismissCompromisedAlertEnabled,
          syncer::PREFERENCES, PrefSensitivity::kNone, MergeBehavior::kNone}},
        {password_manager::prefs::kPasswordLeakDetectionEnabled,
         {syncable_prefs_ids::kPasswordLeakDetectionEnabled,
          syncer::PREFERENCES, PrefSensitivity::kNone, MergeBehavior::kNone}},
        {password_manager::prefs::kWasAutoSignInFirstRunExperienceShown,
         {syncable_prefs_ids::kWasAutoSignInFirstRunExperienceShown,
          syncer::PRIORITY_PREFERENCES, PrefSensitivity::kNone,
          MergeBehavior::kNone}},
        {payments::kCanMakePaymentEnabled,
         {syncable_prefs_ids::kCanMakePaymentEnabled, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {prefs::kAccountTailoredSecurityUpdateTimestamp,
         {syncable_prefs_ids::kAccountTailoredSecurityUpdateTimestamp,
          syncer::PRIORITY_PREFERENCES, PrefSensitivity::kNone,
          MergeBehavior::kNone}},
        {prefs::kCookieControlsMode,
         {syncable_prefs_ids::kCookieControlsMode, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {prefs::kSafeBrowsingEnabled,
         {syncable_prefs_ids::kSafeBrowsingEnabled, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
// TODO(crbug.com/40904479): Maybe move to chrome_syncable_prefs_database.cc,
// see bug.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
        {prefs::kSyncedDefaultSearchProviderGUID,
         {syncable_prefs_ids::kSyncedDefaultSearchProviderGUID,
          syncer::PREFERENCES, PrefSensitivity::kNone, MergeBehavior::kNone}},
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
        {tab_groups::prefs::kAutoPinNewTabGroups,
         {syncable_prefs_ids::kAutoPinNewTabGroups, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {translate::TranslatePrefs::kPrefForceTriggerTranslateCount,
         {syncable_prefs_ids::kPrefForceTriggerTranslateCount,
          syncer::PREFERENCES, PrefSensitivity::kNone, MergeBehavior::kNone}},
        {translate::TranslatePrefs::kPrefTranslateAcceptedCount,
         {syncable_prefs_ids::kPrefTranslateAcceptedCount, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
        {translate::TranslatePrefs::kPrefTranslateAutoAlwaysCount,
         {syncable_prefs_ids::kPrefTranslateAutoAlwaysCount,
          syncer::PREFERENCES, PrefSensitivity::kNone, MergeBehavior::kNone}},
        {translate::TranslatePrefs::kPrefTranslateAutoNeverCount,
         {syncable_prefs_ids::kPrefTranslateAutoNeverCount, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
        {translate::TranslatePrefs::kPrefTranslateDeniedCount,
         {syncable_prefs_ids::kPrefTranslateDeniedCount, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {translate::prefs::kBlockedLanguages,
         {syncable_prefs_ids::kBlockedLanguages, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {translate::prefs::kOfferTranslateEnabled,
         {syncable_prefs_ids::kOfferTranslateEnabled, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {translate::prefs::kPrefAlwaysTranslateList,
         {syncable_prefs_ids::kPrefAlwaysTranslateList, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {translate::prefs::kPrefNeverPromptSitesWithTime,
         {syncable_prefs_ids::kPrefNeverPromptSitesWithTime,
          syncer::PREFERENCES, PrefSensitivity::kSensitiveRequiresHistory,
          MergeBehavior::kNone}},
        {translate::prefs::kPrefTranslateRecentTarget,
         {syncable_prefs_ids::kPrefTranslateRecentTarget, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
// For Ash, the OS_PRIORITY_PREFERENCES equivalent is defined in
// chrome/browser/sync/prefs/chrome_syncable_prefs_database.cc instead.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
        {variations::kDogfoodGroupsSyncPrefName,
         {syncable_prefs_ids::kDogfoodGroupsSyncPrefName,
          syncer::PRIORITY_PREFERENCES, PrefSensitivity::kNone,
          MergeBehavior::kNone}},
#endif
        {kSyncablePrefForTesting,
         {syncable_prefs_ids::kSyncablePrefForTesting, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {kSyncableMergeableDictPrefForTesting,
         {syncable_prefs_ids::kSyncableMergeableDictPrefForTesting,
          syncer::PREFERENCES, PrefSensitivity::kNone,
          MergeBehavior::kMergeableDict}},
        {autofill::prefs::kAutofillPaymentCvcStorage,
         {syncable_prefs_ids::kAutofillPaymentCvcStorage, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {kSyncableMergeableListPrefForTesting,
         {syncable_prefs_ids::kSyncableMergeableListPrefForTesting,
          syncer::PREFERENCES, PrefSensitivity::kNone,
          MergeBehavior::kMergeableListWithRewriteOnUpdate}},
        {kSyncableHistorySensitiveListPrefForTesting,
         {syncable_prefs_ids::kSyncableHistorySensitiveListPrefForTesting,
          syncer::PREFERENCES, PrefSensitivity::kSensitiveRequiresHistory,
          MergeBehavior::kNone}},
        {tab_groups::prefs::kSyncableTabGroups,
         {syncable_prefs_ids::kSyncableTabGroups, syncer::PREFERENCES,
          sync_preferences::PrefSensitivity::kNone,
          sync_preferences::MergeBehavior::kNone}},
        {autofill::prefs::kAutofillPaymentCardBenefits,
         {syncable_prefs_ids::kAutofillPaymentCardBenefits, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {prefs::kSharingVapidKey,
         {syncable_prefs_ids::kSharingVapidKey, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
#if BUILDFLAG(IS_ANDROID)
        {autofill::prefs::kFacilitatedPaymentsPix,
         {syncable_prefs_ids::kFacilitatedPaymentsPix, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
#endif  // BUILDFLAG(IS_ANDROID)
    });

}  // namespace

std::optional<SyncablePrefMetadata>
CommonSyncablePrefsDatabase::GetSyncablePrefMetadata(
    std::string_view pref_name) const {
  const auto it = kCommonSyncablePrefsAllowlist.find(pref_name);
  if (it == kCommonSyncablePrefsAllowlist.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::map<std::string_view, SyncablePrefMetadata>
CommonSyncablePrefsDatabase::GetAllSyncablePrefsForTest() const {
  return {kCommonSyncablePrefsAllowlist.begin(),
          kCommonSyncablePrefsAllowlist.end()};
}

}  // namespace sync_preferences
