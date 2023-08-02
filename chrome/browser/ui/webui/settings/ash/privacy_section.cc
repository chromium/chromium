// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/privacy_section.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "build/branding_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/privacy_hub/privacy_hub_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/auth/legacy_fingerprint_engine.h"
#include "chrome/browser/ui/webui/settings/ash/metrics_consent_handler.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_features_util.h"
#include "chrome/browser/ui/webui/settings/ash/peripheral_data_access_handler.h"
#include "chrome/browser/ui/webui/settings/ash/privacy_hub_handler.h"
#include "chrome/browser/ui/webui/settings/ash/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/settings/settings_secure_dns_handler.h"
#include "chrome/browser/ui/webui/settings/shared_settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kFingerprintSubpagePathV2;
using ::chromeos::settings::mojom::kManageOtherPeopleSubpagePathV2;
using ::chromeos::settings::mojom::kPrivacyAndSecuritySectionPath;
using ::chromeos::settings::mojom::kPrivacyHubSubpagePath;
using ::chromeos::settings::mojom::kSecurityAndSignInSubpagePathV2;
using ::chromeos::settings::mojom::kSmartPrivacySubpagePath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

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
        {IDS_OS_SETTINGS_TAG_SECURITY_AND_PRIVACY,
         mojom::kPrivacyAndSecuritySectionPath,
         mojom::SearchResultIcon::kShield,
         mojom::SearchResultDefaultRank::kMedium,
         mojom::SearchResultType::kSection,
         {.section = mojom::Section::kPrivacyAndSecurity}},
    });

    if (!IsGuestModeActive()) {
      all_tags.insert(
          all_tags.end(),
          {{IDS_OS_SETTINGS_TAG_MANAGE_OTHER_PEOPLE_PAGE,
            mojom::kManageOtherPeopleSubpagePathV2,
            mojom::SearchResultIcon::kAvatar,
            mojom::SearchResultDefaultRank::kMedium,
            mojom::SearchResultType::kSubpage,
            {.subpage = mojom::Subpage::kManageOtherPeopleV2}},
           {IDS_OS_SETTINGS_TAG_GUEST_BROWSING,
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
            {.setting = mojom::Setting::kChangeAuthPinV2},
            {IDS_OS_SETTINGS_TAG_LOCK_SCREEN_PIN_OR_PASSWORD_ALT1,
             SearchConcept::kAltTagEnd}},
           {IDS_OS_SETTINGS_TAG_LOCK_SCREEN_WHEN_WAKING,
            mojom::kSecurityAndSignInSubpagePathV2,
            mojom::SearchResultIcon::kLock,
            mojom::SearchResultDefaultRank::kMedium,
            mojom::SearchResultType::kSetting,
            {.setting = mojom::Setting::kLockScreenV2},
            {IDS_OS_SETTINGS_TAG_LOCK_SCREEN_WHEN_WAKING_ALT1,
             SearchConcept::kAltTagEnd}},
           {IDS_OS_SETTINGS_TAG_LOCK_SCREEN_V2,
            mojom::kSecurityAndSignInSubpagePathV2,
            mojom::SearchResultIcon::kLock,
            mojom::SearchResultDefaultRank::kMedium,
            mojom::SearchResultType::kSubpage,
            {.subpage = mojom::Subpage::kSecurityAndSignInV2}}});
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

const std::vector<SearchConcept>& GetPciguardSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_PRIVACY_PERIPHERAL_DATA_ACCESS_PROTECTION,
       mojom::kPrivacyAndSecuritySectionPath,
       mojom::SearchResultIcon::kShield,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kPeripheralDataAccessProtection},
       {IDS_OS_SETTINGS_TAG_PRIVACY_PERIPHERAL_DATA_ACCESS_PROTECTION_ALT1,
        IDS_OS_SETTINGS_TAG_PRIVACY_PERIPHERAL_DATA_ACCESS_PROTECTION_ALT2,
        IDS_OS_SETTINGS_TAG_PRIVACY_PERIPHERAL_DATA_ACCESS_PROTECTION_ALT3,
        IDS_OS_SETTINGS_TAG_PRIVACY_PERIPHERAL_DATA_ACCESS_PROTECTION_ALT4,
        IDS_OS_SETTINGS_TAG_PRIVACY_PERIPHERAL_DATA_ACCESS_PROTECTION_ALT5}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetSmartPrivacySearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags([] {
    std::vector<SearchConcept> init_tags;

    if (ash::features::IsSnoopingProtectionEnabled() ||
        ash::features::IsQuickDimEnabled()) {
      init_tags.push_back({IDS_OS_SETTINGS_TAG_SMART_PRIVACY,
                           mojom::kSmartPrivacySubpagePath,
                           mojom::SearchResultIcon::kShield,
                           mojom::SearchResultDefaultRank::kMedium,
                           mojom::SearchResultType::kSubpage,
                           {.subpage = mojom::Subpage::kSmartPrivacy}});
    }

    if (ash::features::IsSnoopingProtectionEnabled()) {
      init_tags.push_back({IDS_OS_SETTINGS_TAG_SMART_PRIVACY_SNOOPING,
                           mojom::kSmartPrivacySubpagePath,
                           mojom::SearchResultIcon::kShield,
                           mojom::SearchResultDefaultRank::kMedium,
                           mojom::SearchResultType::kSetting,
                           {.setting = mojom::Setting::kSnoopingProtection},
                           {IDS_OS_SETTINGS_TAG_SMART_PRIVACY_SNOOPING_ALT1,
                            IDS_OS_SETTINGS_TAG_SMART_PRIVACY_SNOOPING_ALT2}});
    }

    // Quick dim: a.k.a leave detection, a.k.a lock on leave, a.k.a. smart
    // privacy screen lock.
    //
    // TODO(crbug.com/1241706): defrag these terms into one canonical name.
    if (ash::features::IsQuickDimEnabled()) {
      init_tags.push_back({IDS_OS_SETTINGS_TAG_SMART_PRIVACY_QUICK_DIM,
                           mojom::kSmartPrivacySubpagePath,
                           mojom::SearchResultIcon::kShield,
                           mojom::SearchResultDefaultRank::kMedium,
                           mojom::SearchResultType::kSetting,
                           {.setting = mojom::Setting::kQuickDim}});
    }

    return init_tags;
  }());

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

const std::vector<SearchConcept>& GetPrivacyControlsSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags([] {
    std::vector<SearchConcept> init_tags;

    if (ash::features::IsCrosPrivacyHubEnabled()) {
      init_tags.push_back({IDS_OS_SETTINGS_TAG_PRIVACY_CONTROLS,
                           mojom::kPrivacyHubSubpagePath,
                           mojom::SearchResultIcon::kShield,
                           mojom::SearchResultDefaultRank::kMedium,
                           mojom::SearchResultType::kSubpage,
                           {.subpage = mojom::Subpage::kPrivacyHub}});
      init_tags.push_back({IDS_OS_SETTINGS_TAG_CAMERA,
                           mojom::kPrivacyHubSubpagePath,
                           mojom::SearchResultIcon::kCamera,
                           mojom::SearchResultDefaultRank::kMedium,
                           mojom::SearchResultType::kSetting,
                           {.setting = mojom::Setting::kCameraOnOff}});

      init_tags.push_back({IDS_OS_SETTINGS_TAG_MICROPHONE,
                           mojom::kPrivacyHubSubpagePath,
                           mojom::SearchResultIcon::kMicrophone,
                           mojom::SearchResultDefaultRank::kMedium,
                           mojom::SearchResultType::kSetting,
                           {.setting = mojom::Setting::kMicrophoneOnOff}});
    }

    if (ash::features::IsCrosPrivacyHubV1Enabled()) {
      init_tags.push_back({IDS_OS_SETTINGS_TAG_GEOLOCATION,
                           mojom::kPrivacyHubSubpagePath,
                           mojom::SearchResultIcon::kGeolocation,
                           mojom::SearchResultDefaultRank::kMedium,
                           mojom::SearchResultType::kSetting,
                           {.setting = mojom::Setting::kGeolocationOnOff}});
    }

    return init_tags;
  }());

  return *tags;
}

bool IsSecureDnsAvailable() {
  return ::features::kDnsOverHttpsShowUiParam.Get();
}

}  // namespace

PrivacySection::PrivacySection(Profile* profile,
                               SearchTagRegistry* search_tag_registry,
                               PrefService* pref_service)
    : OsSettingsSection(profile, search_tag_registry),
      pref_service_(pref_service),
      auth_performer_(UserDataAuthClient::Get()),
      fp_engine_(&auth_performer_) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.AddSearchTags(GetPrivacySearchConcepts());
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  updater.AddSearchTags(GetPrivacyGoogleChromeSearchConcepts());
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  // Fingerprint search tags are added if necessary. Remove fingerprint search
  // tags update dynamically during a user session.
  if (!IsGuestModeActive() && AreFingerprintSettingsAllowed()) {
    updater.AddSearchTags(GetFingerprintSearchConcepts());

    fingerprint_pref_change_registrar_.Init(pref_service_);
    fingerprint_pref_change_registrar_.Add(
        prefs::kQuickUnlockFingerprintRecord,
        base::BindRepeating(&PrivacySection::UpdateRemoveFingerprintSearchTags,
                            base::Unretained(this)));
    UpdateRemoveFingerprintSearchTags();
  }

  updater.AddSearchTags(GetPciguardSearchConcepts());

  // Conditionally adds search tags concepts based on the subset of smart
  // privacy functionality enabled.
  updater.AddSearchTags(GetSmartPrivacySearchConcepts());

  // Adds search concepts for the contents in the Privacy controls page
  // depending on the enabled flags.
  updater.AddSearchTags(GetPrivacyControlsSearchConcepts());
}

PrivacySection::~PrivacySection() = default;

void PrivacySection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(std::make_unique<PeripheralDataAccessHandler>());

  web_ui->AddMessageHandler(std::make_unique<MetricsConsentHandler>(
      profile(), g_browser_process->metrics_service(),
      user_manager::UserManager::Get()));

  web_ui->AddMessageHandler(std::make_unique<PrivacyHubHandler>());

  if (IsSecureDnsAvailable())
    web_ui->AddMessageHandler(std::make_unique<::settings::SecureDnsHandler>());
}

void PrivacySection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"enableLogging", IDS_SETTINGS_ENABLE_LOGGING_TOGGLE_TITLE},
      {"enableLoggingDesc", IDS_SETTINGS_ENABLE_LOGGING_TOGGLE_DESC},
      {"enableContentProtectionAttestation",
       IDS_SETTINGS_ENABLE_CONTENT_PROTECTION_ATTESTATION},
      {"enableSuggestedContent", IDS_SETTINGS_ENABLE_SUGGESTED_CONTENT_TITLE},
      {"enableSuggestedContentDesc",
       IDS_SETTINGS_ENABLE_SUGGESTED_CONTENT_DESC},
      {"peripheralDataAccessProtectionToggleTitle",
       IDS_OS_SETTINGS_DATA_ACCESS_PROTECTION_TOGGLE_TITLE},
      {"peripheralDataAccessProtectionToggleDescription",
       IDS_OS_SETTINGS_DATA_ACCESS_PROTECTION_TOGGLE_DESCRIPTION},
      {"peripheralDataAccessProtectionWarningTitle",
       IDS_OS_SETTINGS_DISABLE_DATA_ACCESS_PROTECTION_CONFIRM_DIALOG_TITLE},
      {"peripheralDataAccessProtectionWarningDescription",
       IDS_OS_SETTINGS_DISABLE_DATA_ACCESS_PROTECTION_CONFIRM_DIALOG_DESCRIPTION},
      {"peripheralDataAccessProtectionWarningSubDescription",
       IDS_OS_SETTINGS_DISABLE_DATA_ACCESS_PROTECTION_CONFIRM_DIALOG_SUB_DESCRIPTION},
      {"peripheralDataAccessProtectionCancelButton",
       IDS_OS_SETTINGS_DATA_ACCESS_PROTECTION_CONFIRM_DIALOG_CANCEL_BUTTON_LABEL},
      {"peripheralDataAccessProtectionDisableButton",
       IDS_OS_SETTINGS_DATA_ACCESS_PROTECTION_CONFIRM_DIALOG_DISABLE_BUTTON_LABEL},
      {"privacyPageTitle", IDS_SETTINGS_PRIVACY_V2},
      {"smartPrivacyTitle", IDS_OS_SETTINGS_SMART_PRIVACY_TITLE},
      {"smartPrivacyQuickDimTitle",
       IDS_OS_SETTINGS_SMART_PRIVACY_QUICK_DIM_TITLE},
      {"smartPrivacyQuickDimSubtext",
       IDS_OS_SETTINGS_SMART_PRIVACY_QUICK_DIM_SUBTEXT},
      {"smartPrivacyQuickLockLong",
       IDS_OS_SETTINGS_SMART_PRIVACY_QUICK_LOCK_LONG},
      {"smartPrivacyQuickLockShort",
       IDS_OS_SETTINGS_SMART_PRIVACY_QUICK_LOCK_SHORT},
      {"smartPrivacyQuickLockTitle",
       IDS_OS_SETTINGS_SMART_PRIVACY_QUICK_LOCK_TITLE},
      {"smartPrivacySnoopingTitle",
       IDS_OS_SETTINGS_SMART_PRIVACY_SNOOPING_TITLE},
      {"smartPrivacySnoopingSubtext",
       IDS_OS_SETTINGS_SMART_PRIVACY_SNOOPING_SUBTEXT},
      {"smartPrivacySnoopingNotifications",
       IDS_OS_SETTINGS_SMART_PRIVACY_SNOOPING_NOTIFICATIONS},
      {"privacyHubTitle", IDS_OS_SETTINGS_PRIVACY_HUB_TITLE},
      {"privacyHubSubtext", IDS_OS_SETTINGS_PRIVACY_HUB_SUBTEXT},
      {"cameraToggleTitle", IDS_OS_SETTINGS_PRIVACY_HUB_CAMERA_TOGGLE_TITLE},
      {"cameraToggleSubtext",
       IDS_OS_SETTINGS_PRIVACY_HUB_CAMERA_TOGGLE_SUBTEXT},
      {"cameraToggleFallbackSubtext",
       IDS_OS_SETTINGS_PRIVACY_HUB_FALLBACK_CAMERA_TOGGLE_SUBTEXT},
      {"noCameraConnectedText",
       IDS_OS_SETTINGS_PRIVACY_HUB_NO_CAMERA_CONNECTED_TEXT},
      {"microphoneToggleTitle",
       IDS_OS_SETTINGS_PRIVACY_HUB_MICROPHONE_TOGGLE_TITLE},
      {"microphoneToggleSubtext",
       IDS_OS_SETTINGS_PRIVACY_HUB_MICROPHONE_TOGGLE_SUBTEXT},
      {"noMicrophoneConnectedText",
       IDS_OS_SETTINGS_PRIVACY_HUB_NO_MICROPHONE_CONNECTED_TEXT},
      {"speakOnMuteDetectionToggleTitle",
       IDS_OS_SETTINGS_PRIVACY_HUB_SPEAK_ON_MUTE_DETECTION_TOGGLE_TITLE},
      {"speakOnMuteDetectionToggleSubtext",
       IDS_OS_SETTINGS_PRIVACY_HUB_SPEAK_ON_MUTE_DETECTION_TOGGLE_SUBTEXT},
      {"geolocationToggleTitle",
       IDS_OS_SETTINGS_PRIVACY_HUB_GEOLOCATION_TOGGLE_TITLE},
      {"geolocationToggleDesc",
       IDS_OS_SETTINGS_PRIVACY_HUB_GEOLOCATION_TOGGLE_DESC},
      {"microphoneHwToggleTooltip",
       IDS_OS_SETTINGS_PRIVACY_HUB_HW_MICROPHONE_TOGGLE_TOOLTIP},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddBoolean("isSnoopingProtectionEnabled",
                          ash::features::IsSnoopingProtectionEnabled());
  html_source->AddBoolean("isQuickDimEnabled",
                          ash::features::IsQuickDimEnabled());

  html_source->AddBoolean(
      "isPrivacyHubHatsEnabled",
      base::FeatureList::IsEnabled(
          ::features::kHappinessTrackingPrivacyHubPostLaunch));
  html_source->AddBoolean("showPrivacyHubPage",
                          ash::features::IsCrosPrivacyHubEnabled());
  html_source->AddBoolean("showPrivacyHubMVPPage",
                          ash::features::IsCrosPrivacyHubV1Enabled());
  html_source->AddBoolean("showPrivacyHubFuturePage",
                          ash::features::IsCrosPrivacyHubV2Enabled());
  html_source->AddBoolean("showSpeakOnMuteDetectionPage",
                          ash::features::IsVideoConferenceEnabled());

  html_source->AddString(
      "smartPrivacyDesc",
      ui::SubstituteChromeOSDeviceType(IDS_OS_SETTINGS_SMART_PRIVACY_DESC));

  html_source->AddString("smartPrivacyLearnMoreURL",
                         chrome::kSmartPrivacySettingsLearnMoreURL);

  html_source->AddString("suggestedContentLearnMoreURL",
                         chrome::kSuggestedContentLearnMoreURL);

  html_source->AddString("syncAndGoogleServicesLearnMoreURL",
                         chrome::kSyncAndGoogleServicesLearnMoreURL);

  html_source->AddString("peripheralDataAccessLearnMoreURL",
                         chrome::kPeripheralDataAccessHelpURL);

  html_source->AddString("speakOnMuteDetectionLearnMoreURL",
                         chrome::kSpeakOnMuteDetectionLearnMoreURL);

  html_source->AddString("geolocationToggleLearnMoreURL",
                         chrome::kGeolocationToggleLearnMoreURL);

  html_source->AddBoolean("showSecureDnsSetting", IsSecureDnsAvailable());
  html_source->AddBoolean("showSecureDnsOsSettingLink", false);

  ::settings::AddSecureDnsStrings(html_source);

  html_source->AddBoolean("isRevenBranding", switches::IsRevenBranding());
  if (switches::IsRevenBranding()) {
    html_source->AddString(
        "enableHWDataUsage",
        l10n_util::GetStringFUTF8(
            IDS_OS_SETTINGS_HW_DATA_USAGE_TOGGLE_TITLE,
            l10n_util::GetStringUTF16(IDS_INSTALLED_PRODUCT_OS_NAME)));
    html_source->AddString(
        "enableHWDataUsageDesc",
        l10n_util::GetStringFUTF8(
            IDS_OS_SETTINGS_HW_DATA_USAGE_TOGGLE_DESC,
            l10n_util::GetStringUTF16(IDS_INSTALLED_PRODUCT_OS_NAME)));
    // TODO(dkuzmin): add learn more link here once available b/190964241
  }
}

int PrivacySection::GetSectionNameMessageId() const {
  return IDS_SETTINGS_PRIVACY_V2;
}

mojom::Section PrivacySection::GetSection() const {
  return mojom::Section::kPrivacyAndSecurity;
}

mojom::SearchResultIcon PrivacySection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kShield;
}

const char* PrivacySection::GetSectionPath() const {
  return mojom::kPrivacyAndSecuritySectionPath;
}

bool PrivacySection::LogMetric(mojom::Setting setting,
                               base::Value& value) const {
  switch (setting) {
    case mojom::Setting::kPeripheralDataAccessProtection:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Privacy.PeripheralDataAccessProtection",
          value.GetBool());
      return true;
    case mojom::Setting::kVerifiedAccess:
      base::UmaHistogramBoolean("ChromeOS.Settings.Privacy.VerifiedAccessOnOff",
                                value.GetBool());
      return true;
    default:
      return false;
  }
}

void PrivacySection::RegisterHierarchy(HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSetting(mojom::Setting::kVerifiedAccess);
  generator->RegisterTopLevelSetting(
      mojom::Setting::kUsageStatsAndCrashReports);

  // Security and sign-in.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_PEOPLE_LOCK_SCREEN_TITLE_LOGIN_LOCK_V2,
      mojom::Subpage::kSecurityAndSignInV2, mojom::SearchResultIcon::kLock,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kSecurityAndSignInSubpagePathV2);
  static constexpr mojom::Setting kSecurityAndSignInSettings[] = {
      mojom::Setting::kLockScreenV2,
      mojom::Setting::kChangeAuthPinV2,
      mojom::Setting::kPeripheralDataAccessProtection,
      mojom::Setting::kLockScreenNotification,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kSecurityAndSignInV2,
                            kSecurityAndSignInSettings, generator);

  // Fingerprint.
  generator->RegisterNestedSubpage(
      IDS_SETTINGS_PEOPLE_LOCK_SCREEN_FINGERPRINT_SUBPAGE_TITLE,
      mojom::Subpage::kFingerprintV2, mojom::Subpage::kSecurityAndSignInV2,
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

  // Smart privacy.
  generator->RegisterTopLevelSubpage(
      IDS_OS_SETTINGS_SMART_PRIVACY_TITLE, mojom::Subpage::kSmartPrivacy,
      mojom::SearchResultIcon::kShield, mojom::SearchResultDefaultRank::kMedium,
      mojom::kSmartPrivacySubpagePath);
  RegisterNestedSettingBulk(
      mojom::Subpage::kSmartPrivacy,
      {{mojom::Setting::kSnoopingProtection, mojom::Setting::kQuickDim}},
      generator);

  // Privacy hub.
  generator->RegisterTopLevelSubpage(
      IDS_OS_SETTINGS_PRIVACY_HUB_TITLE, mojom::Subpage::kPrivacyHub,
      mojom::SearchResultIcon::kShield, mojom::SearchResultDefaultRank::kMedium,
      mojom::kPrivacyHubSubpagePath);
  RegisterNestedSettingBulk(
      mojom::Subpage::kPrivacyHub,
      {{mojom::Setting::kCameraOnOff, mojom::Setting::kMicrophoneOnOff,
        mojom::Setting::kGeolocationOnOff,
        mojom::Setting::kSpeakOnMuteDetectionOnOff}},
      generator);
}

bool PrivacySection::AreFingerprintSettingsAllowed() {
  return fp_engine_.IsFingerprintEnabled(
      *profile()->GetPrefs(), LegacyFingerprintEngine::Purpose::kAny);
}

void PrivacySection::UpdateRemoveFingerprintSearchTags() {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.RemoveSearchTags(GetRemoveFingerprintSearchConcepts());

  // "Remove fingerprint" search tag should exist only when 1 or more
  // fingerprints are registered.
  int registered_fingerprint_count =
      pref_service_->GetInteger(prefs::kQuickUnlockFingerprintRecord);
  if (registered_fingerprint_count > 0) {
    updater.AddSearchTags(GetRemoveFingerprintSearchConcepts());
  }
}

}  // namespace ash::settings
