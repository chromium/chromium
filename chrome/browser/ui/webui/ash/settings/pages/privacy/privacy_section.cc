// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/privacy/privacy_section.h"

#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/web_app_id_constants.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/auth/legacy_fingerprint_engine.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/privacy_hub/privacy_hub_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/settings/os_settings_features_util.h"
#include "chrome/browser/ui/webui/ash/settings/pages/privacy/metrics_consent_handler.h"
#include "chrome/browser/ui/webui/ash/settings/pages/privacy/peripheral_data_access_handler.h"
#include "chrome/browser/ui/webui/ash/settings/pages/privacy/privacy_hub_handler.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/settings/settings_secure_dns_handler.h"
#include "chrome/browser/ui/webui/settings/shared_settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
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
using ::chromeos::settings::mojom::kPrivacyHubCameraSubpagePath;
using ::chromeos::settings::mojom::kPrivacyHubGeolocationAdvancedSubpagePath;
using ::chromeos::settings::mojom::kPrivacyHubGeolocationSubpagePath;
using ::chromeos::settings::mojom::kPrivacyHubMicrophoneSubpagePath;
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
        {ash::features::IsOsSettingsRevampWayfindingEnabled()
             ? IDS_OS_SETTINGS_TAG_PRIVACY_AND_SECURITY
             : IDS_OS_SETTINGS_TAG_SECURITY_AND_PRIVACY,
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
            {.subpage = mojom::Subpage::kSecurityAndSignInV2}},
           {IDS_OS_SETTINGS_TAG_LOCAL_DATA_RECOVERY,
            mojom::kSecurityAndSignInSubpagePathV2,
            mojom::SearchResultIcon::kLock,
            mojom::SearchResultDefaultRank::kMedium,
            mojom::SearchResultType::kSetting,
            {.setting = mojom::Setting::kDataRecovery}}});
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
       mojom::kPrivacyHubSubpagePath,
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

    if (IsGuestModeActive()) {
      return init_tags;
    }

      init_tags.push_back({IDS_OS_SETTINGS_TAG_PRIVACY_CONTROLS,
                           mojom::kPrivacyHubSubpagePath,
                           ash::features::IsOsSettingsRevampWayfindingEnabled()
                               ? mojom::SearchResultIcon::kPrivacyControls
                               : mojom::SearchResultIcon::kShield,
                           mojom::SearchResultDefaultRank::kMedium,
                           mojom::SearchResultType::kSubpage,
                           {.subpage = mojom::Subpage::kPrivacyHub}});
      if (ash::features::IsCrosPrivacyHubAppPermissionsEnabled()) {
        init_tags.push_back({IDS_OS_SETTINGS_TAG_CAMERA,
                             mojom::kPrivacyHubCameraSubpagePath,
                             mojom::SearchResultIcon::kCamera,
                             mojom::SearchResultDefaultRank::kMedium,
                             mojom::SearchResultType::kSubpage,
                             {.subpage = mojom::Subpage::kPrivacyHubCamera}});

        init_tags.push_back(
            {IDS_OS_SETTINGS_TAG_MICROPHONE,
             mojom::kPrivacyHubMicrophoneSubpagePath,
             mojom::SearchResultIcon::kMicrophone,
             mojom::SearchResultDefaultRank::kMedium,
             mojom::SearchResultType::kSubpage,
             {.subpage = mojom::Subpage::kPrivacyHubMicrophone}});
      } else {
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

    if (ash::features::IsCrosPrivacyHubLocationEnabled()) {
      init_tags.push_back(
          {IDS_OS_SETTINGS_TAG_GEOLOCATION,
           mojom::kPrivacyHubGeolocationSubpagePath,
           mojom::SearchResultIcon::kGeolocation,
           mojom::SearchResultDefaultRank::kMedium,
           mojom::SearchResultType::kSubpage,
           {.subpage = mojom::Subpage::kPrivacyHubGeolocation}});
      init_tags.push_back(
          {IDS_OS_SETTINGS_TAG_GEOLOCATION_ACCURACY,
           mojom::kPrivacyHubGeolocationAdvancedSubpagePath,
           mojom::SearchResultIcon::kGeolocation,
           mojom::SearchResultDefaultRank::kMedium,
           mojom::SearchResultType::kSubpage,
           {.subpage = mojom::Subpage::kPrivacyHubGeolocationAdvanced}});
    }
    return init_tags;
  }());

  return *tags;
}

}  // namespace

PrivacySection::PrivacySection(Profile* profile,
                               SearchTagRegistry* search_tag_registry,
                               PrefService* pref_service)
    : OsSettingsSection(profile, search_tag_registry),
      sync_subsection_(
          ash::features::IsOsSettingsRevampWayfindingEnabled()
              ? std::make_optional<SyncSection>(profile, search_tag_registry)
              : std::nullopt),
      pref_service_(pref_service),
      auth_performer_(UserDataAuthClient::Get()),
      fp_engine_(&auth_performer_) {
  if (ash::features::IsOsSettingsRevampWayfindingEnabled()) {
    CHECK(sync_subsection_);
  }

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

  web_ui->AddMessageHandler(std::make_unique<::settings::SecureDnsHandler>());

  // `sync_subsection_` is initialized only if the feature revamp wayfinding is
  // enabled.
  if (sync_subsection_) {
    sync_subsection_->AddHandlers(web_ui);
  }
}

void PrivacySection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  const bool kIsRevampEnabled =
      ash::features::IsOsSettingsRevampWayfindingEnabled();

  webui::LocalizedString kLocalizedStrings[] = {
      {"enableLogging", kIsRevampEnabled
                            ? IDS_OS_SETTINGS_REVAMP_ENABLE_LOGGING_TOGGLE_TITLE
                            : IDS_SETTINGS_ENABLE_LOGGING_TOGGLE_TITLE},
      {"enableLoggingDesc",
       kIsRevampEnabled
           ? IDS_OS_SETTINGS_REVAMP_ENABLE_LOGGING_TOGGLE_DESCRIPTION
           : IDS_SETTINGS_ENABLE_LOGGING_TOGGLE_DESC},
      {"enableContentProtectionAttestation",
       IDS_SETTINGS_ENABLE_CONTENT_PROTECTION_ATTESTATION},
      {"enableSuggestedContent",
       kIsRevampEnabled ? IDS_OS_SETTINGS_REVAMP_ENABLE_SUGGESTED_CONTENT_TITLE
                        : IDS_SETTINGS_ENABLE_SUGGESTED_CONTENT_TITLE},
      {"enableSuggestedContentDesc",
       kIsRevampEnabled
           ? IDS_OS_SETTINGS_REVAMP_ENABLE_SUGGESTED_CONTENT_DESCRIPTION
           : IDS_SETTINGS_ENABLE_SUGGESTED_CONTENT_DESC},
      {"peripheralDataAccessProtectionToggleTitle",
       kIsRevampEnabled
           ? IDS_OS_SETTINGS_REVAMP_DATA_ACCESS_PROTECTION_TOGGLE_TITLE
           : IDS_OS_SETTINGS_DATA_ACCESS_PROTECTION_TOGGLE_TITLE},
      {"peripheralDataAccessProtectionToggleDescription",
       kIsRevampEnabled
           ? IDS_OS_SETTINGS_REVAMP_DATA_ACCESS_PROTECTION_TOGGLE_DESCRIPTION
           : IDS_OS_SETTINGS_DATA_ACCESS_PROTECTION_TOGGLE_DESCRIPTION},
      {"peripheralDataAccessProtectionWarningTitle",
       kIsRevampEnabled
           ? IDS_OS_SETTINGS_REVAMP_DISABLE_DATA_ACCESS_PROTECTION_CONFIRM_DIALOG_TITLE
           : IDS_OS_SETTINGS_DISABLE_DATA_ACCESS_PROTECTION_CONFIRM_DIALOG_TITLE},
      {"peripheralDataAccessProtectionWarningDescription",
       kIsRevampEnabled
           ? IDS_OS_SETTINGS_REVAMP_DISABLE_DATA_ACCESS_PROTECTION_CONFIRM_DIALOG_DESCRIPTION
           : IDS_OS_SETTINGS_DISABLE_DATA_ACCESS_PROTECTION_CONFIRM_DIALOG_DESCRIPTION},
      {"peripheralDataAccessProtectionWarningSubDescription",
       kIsRevampEnabled
           ? IDS_OS_SETTINGS_REVAMP_DISABLE_DATA_ACCESS_PROTECTION_CONFIRM_DIALOG_SUB_DESCRIPTION
           : IDS_OS_SETTINGS_DISABLE_DATA_ACCESS_PROTECTION_CONFIRM_DIALOG_SUB_DESCRIPTION},
      {"peripheralDataAccessProtectionCancelButton",
       IDS_OS_SETTINGS_DATA_ACCESS_PROTECTION_CONFIRM_DIALOG_CANCEL_BUTTON_LABEL},
      {"peripheralDataAccessProtectionDisableButton",
       kIsRevampEnabled
           ? IDS_OS_SETTINGS_REVAMP_DATA_ACCESS_PROTECTION_CONFIRM_DIALOG_ALLOW_BUTTON_LABEL
           : IDS_OS_SETTINGS_DATA_ACCESS_PROTECTION_CONFIRM_DIALOG_DISABLE_BUTTON_LABEL},
      {"privacyPageTitle", kIsRevampEnabled
                               ? IDS_OS_SETTINGS_REVAMP_PRIVACY_TITLE
                               : IDS_OS_SETTINGS_PRIVACY_TITLE},
      {"privacyMenuItemDescription",
       IDS_OS_SETTINGS_PRIVACY_MENU_ITEM_DESCRIPTION},
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
      {"privacyHubPageCameraRowSubtext",
       IDS_OS_SETTINGS_PRIVACY_HUB_PAGE_CAMERA_ROW_SUBTEXT},
      {"privacyHubPageCameraRowFallbackSubtext",
       IDS_OS_SETTINGS_PRIVACY_HUB_PAGE_CAMERA_ROW_FALLBACK_SUBTEXT},
      {"privacyHubCameraSubpageCameraToggleSubtext",
       IDS_OS_SETTINGS_PRIVACY_HUB_CAMERA_SUBPAGE_CAMERA_TOGGLE_SUBTEXT},
      {"privacyHubCameraSubpageCameraToggleFallbackSubtext",
       IDS_OS_SETTINGS_PRIVACY_HUB_CAMERA_SUBPAGE_CAMERA_TOGGLE_FALLBACK_SUBTEXT},
      {"privacyHubCameraAccessBlockedText",
       IDS_OS_SETTINGS_PRIVACY_HUB_CAMERA_ACCESS_BLOCKED_TEXT},
      {"noCameraConnectedText",
       IDS_OS_SETTINGS_PRIVACY_HUB_NO_CAMERA_CONNECTED_TEXT},
      {"microphoneToggleTitle",
       IDS_OS_SETTINGS_PRIVACY_HUB_MICROPHONE_TOGGLE_TITLE},
      {"microphoneToggleSubtext",
       IDS_OS_SETTINGS_PRIVACY_HUB_MICROPHONE_TOGGLE_SUBTEXT},
      {"privacyHubPageMicrophoneRowSubtext",
       IDS_OS_SETTINGS_PRIVACY_HUB_PAGE_MICROPHONE_ROW_SUBTEXT},
      {"privacyHubMicrophoneSubpageMicrophoneToggleSubtext",
       IDS_OS_SETTINGS_PRIVACY_HUB_MICROPHONE_SUBPAGE_MICROPHONE_TOGGLE_SUBTEXT},
      {"privacyHubMicrophoneAccessBlockedText",
       IDS_OS_SETTINGS_PRIVACY_HUB_MICROPHONE_ACCESS_BLOCKED_TEXT},
      {"noMicrophoneConnectedText",
       IDS_OS_SETTINGS_PRIVACY_HUB_NO_MICROPHONE_CONNECTED_TEXT},
      {"speakOnMuteDetectionToggleTitle",
       IDS_OS_SETTINGS_PRIVACY_HUB_SPEAK_ON_MUTE_DETECTION_TOGGLE_TITLE},
      {"speakOnMuteDetectionToggleSubtext",
       IDS_OS_SETTINGS_PRIVACY_HUB_SPEAK_ON_MUTE_DETECTION_TOGGLE_SUBTEXT},
      {"geolocationAreaTitle",
       IDS_OS_SETTINGS_PRIVACY_HUB_GEOLOCATION_AREA_TITLE},
      {"geolocationAreaAllowedSubtext",
       IDS_OS_SETTINGS_PRIVACY_HUB_GEOLOCATION_AREA_ALLOWED_SUBTEXT},
      {"geolocationAreaOnlyAllowedForSystemSubtext",
       IDS_OS_SETTINGS_PRIVACY_HUB_GEOLOCATION_AREA_ONLY_ALLOWED_FOR_SYSTEM_SUBTEXT},
      {"geolocationAreaDisallowedSubtext",
       IDS_OS_SETTINGS_PRIVACY_HUB_GEOLOCATION_AREA_DISALLOWED_SUBTEXT},
      {"geolocationControlledByPrimaryUserText",
       IDS_OS_SETTINGS_PRIVACY_HUB_GEOLOCATION_PRIMARY_USER_CONTROLLED},
      {"geolocationAccessLevelAllowed",
       IDS_OS_SETTINGS_PRIVACY_HUB_GEOLOCATION_ACCESS_LEVEL_ALLOWED},
      {"geolocationAccessLevelOnlyAllowedForSystem",
       IDS_OS_SETTINGS_PRIVACY_HUB_GEOLOCATION_ACCESS_LEVEL_ONLY_ALLOWED_FOR_SYSTEM},
      {"geolocationAccessLevelDisallowed",
       IDS_OS_SETTINGS_PRIVACY_HUB_GEOLOCATION_ACCESS_LEVEL_DISALLOWED},
      {"geolocationChangeAccessButtonText",
       IDS_OS_SETTINGS_PRIVACY_HUB_GEOLOCATION_CHANGE_ACCESS_BUTTON_TEXT},
      {"geolocationAllowedModeDescription",
       IDS_OS_SETTINGS_PRIVACY_HUB_GEOLOCATION_ACCESS_LEVEL_DESCRIPTION_ALLOWED},
      {"geolocationOnlyAllowedForSystemModeDescription",
       IDS_OS_SETTINGS_PRIVACY_HUB_GEOLOCATION_ACCESS_LEVEL_DESCRIPTION_ONLY_ALLOWED_FOR_SYSTEM},
      {"geolocationBlockedModeDescription",
       IDS_OS_SETTINGS_PRIVACY_HUB_GEOLOCATION_ACCESS_LEVEL_DESCRIPTION_DISALLOWED},
      {"geolocationDialogAllowedModeDescription",
       IDS_OS_SETTINGS_PRIVACY_HUB_GEOLOCATION_ACCESS_LEVEL_DIALOG_DESCRIPTION_ALLOWED},
      {"geolocationDialogOnlyAllowedForSystemModeDescription",
       IDS_OS_SETTINGS_PRIVACY_HUB_GEOLOCATION_ACCESS_LEVEL_DIALOG_DESCRIPTION_ONLY_ALLOWED_FOR_SYSTEM},
      {"geolocationDialogBlockedModeDescription",
       IDS_OS_SETTINGS_PRIVACY_HUB_GEOLOCATION_ACCESS_LEVEL_DIALOG_DESCRIPTION_DISALLOWED},
      {"geolocationAccessLevelDialogConfirmButton", IDS_SETTINGS_DONE_BUTTON},
      {"geolocationAccessLevelDialogCancelButton", IDS_SETTINGS_CANCEL_BUTTON},
      {"geolocationAccuracyToggleText",
       IDS_OS_SETTINGS_PRIVACY_HUB_GEOLOCATION_ACCURACY_TOGGLE_TEXT},
      {"geolocationAccuracyToggleTitle",
       IDS_OS_SETTINGS_PRIVACY_HUB_GEOLOCATION_ACCURACY_TOGGLE_TITLE},
      {"geolocationAdvancedAreaTitle",
       IDS_OS_SETTINGS_PRIVACY_HUB_GEOLOCATION_ADVANCED_AREA_TITLE},
      {"systemGeolocationDialogTitle",
       IDS_SETTINGS_PRIVACY_HUB_GEOLOCATION_DIALOG_TITLE},
      {"systemGeolocationDialogBodyParagraph1",
       IDS_SETTINGS_PRIVACY_HUB_GEOLOCATION_DIALOG_BODY_PARAGRAPH1},
      {"systemGeolocationDialogBodyParagraph2",
       IDS_SETTINGS_PRIVACY_HUB_GEOLOCATION_DIALOG_BODY_PARAGRAPH2},
      {"systemGeolocationDialogConfirmButton",
       IDS_SETTINGS_PRIVACY_HUB_GEOLOCATION_DIALOG_CONFIRM_BUTTON},
      {"systemGeolocationDialogCancelButton",
       IDS_SETTINGS_PRIVACY_HUB_GEOLOCATION_DIALOG_CANCEL_BUTTON},
      {"microphoneHwToggleTooltip",
       IDS_OS_SETTINGS_PRIVACY_HUB_HW_MICROPHONE_TOGGLE_TOOLTIP},
      {"websitesSectionTitle",
       IDS_OS_SETTINGS_PRIVACY_HUB_WEBSITES_SECTION_TITLE},
      {"manageCameraPermissionsInChromeText",
       IDS_OS_SETTINGS_PRIVACY_HUB_MANAGE_CAMERA_PERMISSIONS_IN_CHROME_TEXT},
      {"manageMicPermissionsInChromeText",
       IDS_OS_SETTINGS_PRIVACY_HUB_MANAGE_MIC_PERMISSIONS_IN_CHROME_TEXT},
      {"manageLocationPermissionsInChromeText",
       IDS_OS_SETTINGS_PRIVACY_HUB_MANAGE_LOCATION_PERMISSIONS_IN_CHROME_TEXT},
      {"noWebsiteCanUseCameraText",
       IDS_OS_SETTINGS_PRIVACY_HUB_NO_WEBSITE_CAN_USE_CAMERA_TEXT},
      {"noWebsiteCanUseMicText",
       IDS_OS_SETTINGS_PRIVACY_HUB_NO_WEBSITE_CAN_USE_MIC_TEXT},
      {"noWebsiteCanUseLocationText",
       IDS_OS_SETTINGS_PRIVACY_HUB_NO_WEBSITE_CAN_USE_LOCATION_TEXT},
      {"privacyHubAppsSectionTitle",
       IDS_OS_SETTINGS_PRIVACY_HUB_APPS_SECTION_TITLE},
      {"privacyHubPermissionAllowedText",
       IDS_APP_MANAGEMENT_PERMISSION_ALLOWED},
      {"privacyHubPermissionAllowedTextWithDetails",
       IDS_APP_MANAGEMENT_PERMISSION_ALLOWED_WITH_DETAILS},
      {"privacyHubPermissionAskText", IDS_APP_MANAGEMENT_PERMISSION_ASK},
      {"privacyHubPermissionDeniedText", IDS_APP_MANAGEMENT_PERMISSION_DENIED},
      {"noAppCanUseMicText",
       IDS_OS_SETTINGS_PRIVACY_HUB_NO_APP_CAN_USE_MIC_TEXT},
      {"noAppCanUseCameraText",
       IDS_OS_SETTINGS_PRIVACY_HUB_NO_APP_CAN_USE_CAMERA_TEXT},
      {"noAppCanUseGeolocationText",
       IDS_OS_SETTINGS_PRIVACY_HUB_NO_APP_CAN_USE_LOCATION_TEXT},
      {"privacyHubSystemServicesSectionTitle",
       IDS_OS_SETTINGS_PRIVACY_HUB_SYSTEM_SERVICES_SECTION_TITLE},
      {"privacyHubSystemServicesGeolocationNotConfigured",
       IDS_OS_SETTINGS_PRIVACY_HUB_SYSTEM_SERVICES_GEOLOCATION_NOT_CONFIGURED},
      {"privacyHubSystemServicesAllowedText",
       IDS_OS_SETTINGS_PRIVACY_HUB_SYSTEM_SERVICES_ALLOWED_TEXT},
      {"privacyHubSystemServicesBlockedText",
       IDS_OS_SETTINGS_PRIVACY_HUB_SYSTEM_SERVICES_BLOCKED_TEXT},
      {"privacyHubSensorNameWithBlockedSuffix",
       IDS_OS_SETTINGS_PRIVACY_HUB_SENSOR_NAME_WITH_BLOCKED_SUFFIX},
      {"privacyHubCameraAppPermissionRowAriaLabel",
       IDS_OS_SETTINGS_PRIVACY_HUB_CAMERA_APP_PERMISSION_ROW_ARIA_LABEL},
      {"privacyHubLocationAppPermissionRowAriaLabel",
       IDS_OS_SETTINGS_PRIVACY_HUB_LOCATION_APP_PERMISSION_ROW_ARIA_LABEL},
      {"privacyHubMicrophoneAppPermissionRowAriaLabel",
       IDS_OS_SETTINGS_PRIVACY_HUB_MICROPHONE_APP_PERMISSION_ROW_ARIA_LABEL},
      {"privacyHubAppPermissionRowAriaDescription",
       IDS_OS_SETTINGS_PRIVACY_HUB_APP_PERMISSION_ROW_ARIA_DESCRIPTION},
      {"privacyHubAppPermissionRowAndroidSettingsLinkAriaDescription",
       IDS_OS_SETTINGS_PRIVACY_HUB_APP_PERMISSION_ROW_ANDROID_SETTINGS_LINK_ARIA_DESCRIPTION},
      {"privacyHubSystemServicesAutomaticTimeZoneBlockedText",
       IDS_OS_SETTINGS_PRIVACY_HUB_SYSTEM_SERVICES_AUTOMATIC_TIME_ZONE_BLOCKED_TEXT},
      {"privacyHubSystemServicesSunsetScheduleBlockedText",
       IDS_OS_SETTINGS_PRIVACY_HUB_SYSTEM_SERVICES_SUNSET_SCHEDULE_BLOCKED_TEXT},
      {"privacyHubSystemServicesAutomaticTimeZoneName",
       IDS_OS_SETTINGS_PRIVACY_HUB_SYSTEM_SERVICES_AUTOMATIC_TIME_ZONE_NAME},
      {"privacyHubSystemServicesSunsetScheduleName",
       IDS_OS_SETTINGS_PRIVACY_HUB_SYSTEM_SERVICES_SUNSET_SCHEDULE_NAME},
      {"privacyHubSystemServicesLocalWeatherName",
       IDS_OS_SETTINGS_PRIVACY_HUB_SYSTEM_SERVICES_LOCAL_WEATHER_NAME},
      {"privacyHubSystemServicesDarkThemeName",
       IDS_OS_SETTINGS_PRIVACY_HUB_SYSTEM_SERVICES_DARK_THEME_NAME},
      {"privacyHubNoCameraConnectedTooltipText",
       IDS_OS_SETTINGS_PRIVACY_HUB_CAMERA_TOGGLE_NO_CAMERA_CONNECTED_TOOLTIP_TEXT},
      {"privacyHubNoMicrophoneConnectedTooltipText",
       IDS_OS_SETTINGS_PRIVACY_HUB_MICROPHONE_TOGGLE_NO_MICROPHONE_CONNECTED_TOOLTIP_TEXT},
      {"privacyHubAllowCameraAccessDialogTitle",
       IDS_OS_SETTINGS_PRIVACY_HUB_ALLOW_CAMERA_ACCESS_DIALOG_TITLE},
      {"privacyHubAllowLocationAccessDialogTitle",
       IDS_OS_SETTINGS_PRIVACY_HUB_ALLOW_LOCATION_ACCESS_DIALOG_TITLE},
      {"privacyHubAllowMicrophoneAccessDialogTitle",
       IDS_OS_SETTINGS_PRIVACY_HUB_ALLOW_MICROPHONE_ACCESS_DIALOG_TITLE},
      {"privacyHubAllowCameraAccessDialogBodyText",
       IDS_OS_SETTINGS_PRIVACY_HUB_ALLOW_CAMERA_ACCESS_DIALOG_BODY_TEXT},
      {"privacyHubAllowLocationAccessDialogBodyText",
       IDS_OS_SETTINGS_PRIVACY_HUB_ALLOW_LOCATION_ACCESS_DIALOG_BODY_TEXT},
      {"privacyHubAllowMicrophoneAccessDialogBodyText",
       IDS_OS_SETTINGS_PRIVACY_HUB_ALLOW_MICROPHONE_ACCESS_DIALOG_BODY_TEXT},
      {"privacyHubDialogConfirmButtonLabel",
       IDS_OS_SETTINGS_PRIVACY_HUB_DIALOG_CONFIRM_BUTTON_LABEL},
      {"privacyHubDialogCancelButtonLabel",
       IDS_OS_SETTINGS_PRIVACY_HUB_DIALOG_CANCEL_BUTTON_LABEL},
  };

  html_source->AddLocalizedStrings(kLocalizedStrings);

  auto [sunrise_time, sunset_time] =
      ash::privacy_hub_util::SunriseSunsetSchedule();
  html_source->AddString("privacyHubSystemServicesInitSunRiseTime",
                         base::TimeFormatTimeOfDay(sunrise_time));
  html_source->AddString("privacyHubSystemServicesInitSunSetTime",
                         base::TimeFormatTimeOfDay(sunset_time));

  html_source->AddBoolean("isSnoopingProtectionEnabled",
                          ash::features::IsSnoopingProtectionEnabled());
  html_source->AddBoolean("isQuickDimEnabled",
                          ash::features::IsQuickDimEnabled());
  html_source->AddBoolean("isAuthPanelEnabled",
                          ash::features::IsUseAuthPanelInSessionEnabled());

  html_source->AddBoolean(
      "isPrivacyHubHatsEnabled",
      base::FeatureList::IsEnabled(
          ::features::kHappinessTrackingPrivacyHubPostLaunch));
  html_source->AddBoolean(
      "showAppPermissionsInsidePrivacyHub",
      ash::features::IsCrosPrivacyHubAppPermissionsEnabled());
  html_source->AddBoolean("showPrivacyHubLocationControl",
                          ash::features::IsCrosPrivacyHubLocationEnabled());
  html_source->AddBoolean("showSpeakOnMuteDetectionPage",
                          ash::features::IsVideoConferenceEnabled());
  html_source->AddBoolean("isArcReadOnlyPermissionsEnabled",
                          arc::IsReadOnlyPermissionsEnabled());

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

  html_source->AddString("geolocationAccuracyLearnMoreUrl",
                         chrome::kPrivacyHubGeolocationAccuracyLearnMoreURL);

  html_source->AddString("osSettingsAppId", web_app::kOsSettingsAppId);

  html_source->AddString(
      "authPrompt",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_IN_SESSION_AUTH_ORIGIN_NAME_PROMPT,
          l10n_util::GetStringUTF16(
              IDS_SETTINGS_IN_SESSION_AUTH_ORIGIN_NAME_PROMPT_LOCATION)));

  html_source->AddBoolean("showSecureDnsSetting", true);
  html_source->AddBoolean("showSecureDnsOsSettingLink", false);
  html_source->AddBoolean(
      "isDeprecateDnsDialogEnabled",
      ash::features::IsOsSettingsDeprecateDnsDialogEnabled());

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
  }

  // `sync_subsection_` is initialized only if the feature revamp wayfinding is
  // enabled.
  if (sync_subsection_) {
    sync_subsection_->AddLoadTimeData(html_source);
  }
}

int PrivacySection::GetSectionNameMessageId() const {
  return ash::features::IsOsSettingsRevampWayfindingEnabled()
             ? IDS_OS_SETTINGS_REVAMP_PRIVACY_TITLE
             : IDS_OS_SETTINGS_PRIVACY_TITLE;
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
    case mojom::Setting::kRevenEnableHwDataUsage:
      base::UmaHistogramBoolean("ChromeOS.Settings.RevenEnableHwDataUsage",
                                value.GetBool());
      return true;
    default:
      return false;
  }
}

void PrivacySection::RegisterHierarchy(HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSetting(mojom::Setting::kVerifiedAccess);
  generator->RegisterTopLevelSetting(mojom::Setting::kRevenEnableHwDataUsage);

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
      mojom::Setting::kDataRecovery,
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
        mojom::Setting::kSpeakOnMuteDetectionOnOff,
        mojom::Setting::kUsageStatsAndCrashReports}},
      generator);

  // Privacy hub microphone.
  generator->RegisterNestedSubpage(
      IDS_OS_SETTINGS_PRIVACY_HUB_MICROPHONE_TOGGLE_TITLE,
      mojom::Subpage::kPrivacyHubMicrophone, mojom::Subpage::kPrivacyHub,
      mojom::SearchResultIcon::kMicrophone,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kPrivacyHubMicrophoneSubpagePath);

  // Privacy hub geolocation.
  generator->RegisterNestedSubpage(
      IDS_OS_SETTINGS_PRIVACY_HUB_GEOLOCATION_AREA_TITLE,
      mojom::Subpage::kPrivacyHubGeolocation, mojom::Subpage::kPrivacyHub,
      mojom::SearchResultIcon::kGeolocation,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kPrivacyHubGeolocationSubpagePath);

  // Privacy hub geolocation advanced.
  generator->RegisterNestedSubpage(
      IDS_OS_SETTINGS_PRIVACY_HUB_GEOLOCATION_ACCURACY_TOGGLE_TITLE,
      mojom::Subpage::kPrivacyHubGeolocationAdvanced,
      mojom::Subpage::kPrivacyHubGeolocation,
      mojom::SearchResultIcon::kGeolocation,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kPrivacyHubGeolocationAdvancedSubpagePath);
  RegisterNestedSettingBulk(mojom::Subpage::kPrivacyHubGeolocation,
                            {{mojom::Setting::kGeolocationAdvanced}},
                            generator);

  // Privacy hub camera.
  generator->RegisterNestedSubpage(
      IDS_OS_SETTINGS_PRIVACY_HUB_CAMERA_TOGGLE_TITLE,
      mojom::Subpage::kPrivacyHubCamera, mojom::Subpage::kPrivacyHub,
      mojom::SearchResultIcon::kCamera, mojom::SearchResultDefaultRank::kMedium,
      mojom::kPrivacyHubCameraSubpagePath);

  // `sync_subsection_` is initialized only if the feature revamp wayfinding is
  // enabled.
  if (sync_subsection_) {
    sync_subsection_->RegisterHierarchy(generator);
  }
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
