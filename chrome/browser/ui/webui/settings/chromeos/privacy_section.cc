// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/privacy_section.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/settings/shared_settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<SearchConcept>& GetPrivacySearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags([] {
    std::vector<SearchConcept> all_tags({
        {IDS_OS_SETTINGS_TAG_PRIVACY_VERIFIED_ACCESS,
         mojom::kPrivacyAndSecuritySectionPath,
         mojom::SearchResultIcon::kShield,
         mojom::SearchResultDefaultRank::kMedium,
         mojom::SearchResultType::kSetting,
         {.setting = mojom::Setting::kVerifiedAccess}},
        {IDS_OS_SETTINGS_TAG_PRIVACY,
         mojom::kPrivacyAndSecuritySectionPath,
         mojom::SearchResultIcon::kShield,
         mojom::SearchResultDefaultRank::kMedium,
         mojom::SearchResultType::kSection,
         {.section = mojom::Section::kPrivacyAndSecurity}},
    });

    if (chromeos::features::IsAccountManagementFlowsV2Enabled()) {
      all_tags.insert(
          all_tags.end(),
          {{IDS_OS_SETTINGS_TAG_GUEST_BROWSING,
            mojom::kManageOtherPeopleSubpagePathV2,
            mojom::SearchResultIcon::kAvatar,
            mojom::SearchResultDefaultRank::kMedium,
            mojom::SearchResultType::kSetting,
            {.setting = mojom::Setting::kGuestBrowsingV2}},
           {IDS_OS_SETTINGS_TAG_USERNAMES_AND_PHOTOS,
            mojom::kManageOtherPeopleSubpagePathV2,
            mojom::SearchResultIcon::kAvatar,
            mojom::SearchResultDefaultRank::kMedium,
            mojom::SearchResultType::kSetting,
            {.setting = mojom::Setting::kShowUsernamesAndPhotosAtSignInV2},
            {IDS_OS_SETTINGS_TAG_USERNAMES_AND_PHOTOS_ALT1,
             IDS_OS_SETTINGS_TAG_USERNAMES_AND_PHOTOS_ALT2,
             SearchConcept::kAltTagEnd}},
           {IDS_OS_SETTINGS_TAG_RESTRICT_SIGN_IN,
            mojom::kManageOtherPeopleSubpagePathV2,
            mojom::SearchResultIcon::kAvatar,
            mojom::SearchResultDefaultRank::kMedium,
            mojom::SearchResultType::kSetting,
            {.setting = mojom::Setting::kRestrictSignInV2},
            {IDS_OS_SETTINGS_TAG_RESTRICT_SIGN_IN_ALT1,
             SearchConcept::kAltTagEnd}},
           {IDS_OS_SETTINGS_TAG_RESTRICT_SIGN_IN_ADD,
            mojom::kManageOtherPeopleSubpagePathV2,
            mojom::SearchResultIcon::kAvatar,
            mojom::SearchResultDefaultRank::kMedium,
            mojom::SearchResultType::kSetting,
            {.setting = mojom::Setting::kAddToUserAllowlistV2}},
           {IDS_OS_SETTINGS_TAG_RESTRICT_SIGN_IN_REMOVE,
            mojom::kManageOtherPeopleSubpagePathV2,
            mojom::SearchResultIcon::kAvatar,
            mojom::SearchResultDefaultRank::kMedium,
            mojom::SearchResultType::kSetting,
            {.setting = mojom::Setting::kRemoveFromUserAllowlistV2}},
           {IDS_OS_SETTINGS_TAG_LOCK_SCREEN_PIN_OR_PASSWORD,
            mojom::kSecurityAndSignInSubpagePathV2,
            mojom::SearchResultIcon::kLock,
            mojom::SearchResultDefaultRank::kMedium,
            mojom::SearchResultType::kSetting,
            {.setting = mojom::Setting::kChangeAuthPin},
            {IDS_OS_SETTINGS_TAG_LOCK_SCREEN_PIN_OR_PASSWORD_ALT1,
             SearchConcept::kAltTagEnd}},
           {IDS_OS_SETTINGS_TAG_LOCK_SCREEN_WHEN_WAKING,
            mojom::kSecurityAndSignInSubpagePathV2,
            mojom::SearchResultIcon::kLock,
            mojom::SearchResultDefaultRank::kMedium,
            mojom::SearchResultType::kSetting,
            {.setting = mojom::Setting::kLockScreen},
            {IDS_OS_SETTINGS_TAG_LOCK_SCREEN_WHEN_WAKING_ALT1,
             SearchConcept::kAltTagEnd}},
           {IDS_OS_SETTINGS_TAG_LOCK_SCREEN,
            mojom::kSecurityAndSignInSubpagePathV2,
            mojom::SearchResultIcon::kLock,
            mojom::SearchResultDefaultRank::kMedium,
            mojom::SearchResultType::kSubpage,
            {.subpage = mojom::Subpage::kSecurityAndSignIn}}});
    }

    return all_tags;
  }());

  return *tags;
}

const std::vector<SearchConcept>& GetFingerprintSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_FINGERPRINT_ADD,
       mojom::kFingerprintSubpagePathV2,
       mojom::SearchResultIcon::kFingerprint,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAddFingerprintV2}},
      {IDS_OS_SETTINGS_TAG_FINGERPRINT,
       mojom::kFingerprintSubpagePathV2,
       mojom::SearchResultIcon::kFingerprint,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kFingerprintV2}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetRemoveFingerprintSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_FINGERPRINT_REMOVE,
       mojom::kFingerprintSubpagePathV2,
       mojom::SearchResultIcon::kFingerprint,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kRemoveFingerprintV2}},
  });
  return *tags;
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const std::vector<SearchConcept>& GetPrivacyGoogleChromeSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_PRIVACY_CRASH_REPORTS,
       mojom::kPrivacyAndSecuritySectionPath,
       mojom::SearchResultIcon::kShield,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kUsageStatsAndCrashReports},
       {IDS_OS_SETTINGS_TAG_PRIVACY_CRASH_REPORTS_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace

PrivacySection::PrivacySection(Profile* profile,
                               SearchTagRegistry* search_tag_registry,
                               PrefService* pref_service)
    : OsSettingsSection(profile, search_tag_registry),
      pref_service_(pref_service) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.AddSearchTags(GetPrivacySearchConcepts());
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  updater.AddSearchTags(GetPrivacyGoogleChromeSearchConcepts());
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  // Fingerprint search tags are added if necessary. Remove fingerprint search
  // tags update dynamically during a user session.
  if (AreFingerprintSettingsAllowed() &&
      chromeos::features::IsAccountManagementFlowsV2Enabled()) {
    updater.AddSearchTags(GetFingerprintSearchConcepts());

    fingerprint_pref_change_registrar_.Init(pref_service_);
    fingerprint_pref_change_registrar_.Add(
        ::prefs::kQuickUnlockFingerprintRecord,
        base::BindRepeating(&PrivacySection::UpdateRemoveFingerprintSearchTags,
                            base::Unretained(this)));
    UpdateRemoveFingerprintSearchTags();
  }
}

PrivacySection::~PrivacySection() = default;

void PrivacySection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"privacyPageTitle", IDS_SETTINGS_PRIVACY},
      {"enableLogging", IDS_SETTINGS_ENABLE_LOGGING_TOGGLE_TITLE},
      {"enableLoggingDesc", IDS_SETTINGS_ENABLE_LOGGING_TOGGLE_DESC},
      {"enableContentProtectionAttestation",
       IDS_SETTINGS_ENABLE_CONTENT_PROTECTION_ATTESTATION},
      {"enableSuggestedContent", IDS_SETTINGS_ENABLE_SUGGESTED_CONTENT_TITLE},
      {"enableSuggestedContentDesc",
       IDS_SETTINGS_ENABLE_SUGGESTED_CONTENT_DESC},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddBoolean(
      "privacySettingsRedesignEnabled",
      base::FeatureList::IsEnabled(::features::kPrivacySettingsRedesign));

  html_source->AddBoolean("suggestedContentToggleEnabled",
                          base::FeatureList::IsEnabled(
                              ::chromeos::features::kSuggestedContentToggle));

  html_source->AddString("suggestedContentLearnMoreURL",
                         chrome::kSuggestedContentLearnMoreURL);

  html_source->AddString("syncAndGoogleServicesLearnMoreURL",
                         chrome::kSyncAndGoogleServicesLearnMoreURL);
  ::settings::AddPersonalizationOptionsStrings(html_source);
}

int PrivacySection::GetSectionNameMessageId() const {
  return IDS_SETTINGS_PRIVACY;
}

mojom::Section PrivacySection::GetSection() const {
  return mojom::Section::kPrivacyAndSecurity;
}

mojom::SearchResultIcon PrivacySection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kShield;
}

std::string PrivacySection::GetSectionPath() const {
  return mojom::kPrivacyAndSecuritySectionPath;
}

bool PrivacySection::LogMetric(mojom::Setting setting,
                               base::Value& value) const {
  // Unimplemented.
  return false;
}

void PrivacySection::RegisterHierarchy(HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSetting(mojom::Setting::kVerifiedAccess);
  generator->RegisterTopLevelSetting(
      mojom::Setting::kUsageStatsAndCrashReports);

  // Security and sign-in.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_PEOPLE_LOCK_SCREEN_TITLE_LOGIN_LOCK,
      mojom::Subpage::kSecurityAndSignInV2, mojom::SearchResultIcon::kLock,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kSecurityAndSignInSubpagePathV2);
  static constexpr mojom::Setting kSecurityAndSignInSettings[] = {
      mojom::Setting::kLockScreenV2,
      mojom::Setting::kChangeAuthPinV2,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kSecurityAndSignInV2,
                            kSecurityAndSignInSettings, generator);

  // Fingerprint.
  generator->RegisterNestedSubpage(
      IDS_SETTINGS_PEOPLE_LOCK_SCREEN_FINGERPRINT_SUBPAGE_TITLE,
      mojom::Subpage::kFingerprintV2, mojom::Subpage::kSecurityAndSignIn,
      mojom::SearchResultIcon::kFingerprint,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kFingerprintSubpagePathV2);
  static constexpr mojom::Setting kFingerprintSettings[] = {
      mojom::Setting::kAddFingerprintV2,
      mojom::Setting::kRemoveFingerprintV2,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kFingerprintV2,
                            kFingerprintSettings, generator);

  // Manage other people.
  generator->RegisterTopLevelSubpage(IDS_SETTINGS_PEOPLE_MANAGE_OTHER_PEOPLE,
                                     mojom::Subpage::kManageOtherPeopleV2,
                                     mojom::SearchResultIcon::kAvatar,
                                     mojom::SearchResultDefaultRank::kMedium,
                                     mojom::kManageOtherPeopleSubpagePathV2);
  static constexpr mojom::Setting kManageOtherPeopleSettings[] = {
      mojom::Setting::kGuestBrowsingV2,
      mojom::Setting::kShowUsernamesAndPhotosAtSignInV2,
      mojom::Setting::kRestrictSignInV2,
      mojom::Setting::kAddToUserAllowlistV2,
      mojom::Setting::kRemoveFromUserAllowlistV2,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kManageOtherPeopleV2,
                            kManageOtherPeopleSettings, generator);
}

bool PrivacySection::AreFingerprintSettingsAllowed() {
  return chromeos::quick_unlock::IsFingerprintEnabled(profile());
}

void PrivacySection::UpdateRemoveFingerprintSearchTags() {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.RemoveSearchTags(GetRemoveFingerprintSearchConcepts());

  // "Remove fingerprint" search tag should exist only when 1 or more
  // fingerprints are registered.
  int registered_fingerprint_count =
      pref_service_->GetInteger(::prefs::kQuickUnlockFingerprintRecord);
  if (registered_fingerprint_count > 0) {
    updater.AddSearchTags(GetRemoveFingerprintSearchConcepts());
  }
}

}  // namespace settings
}  // namespace chromeos
