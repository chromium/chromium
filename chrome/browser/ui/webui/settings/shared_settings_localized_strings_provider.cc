// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/shared_settings_localized_strings_provider.h"

#include <string>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/live_caption/caption_util.h"
#include "components/soda/constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "media/base/media_switches.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/webui/settings/public/constants/routes.mojom.h"
#endif

namespace settings {
#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace {

// Generates a Google Help URL which includes a "board type" parameter. Some
// help pages need to be adjusted depending on the type of CrOS device that is
// accessing the page.
std::u16string GetHelpUrlWithBoard(const std::string& original_url) {
  return base::ASCIIToUTF16(original_url +
                            "&b=" + base::SysInfo::GetLsbReleaseBoard());
}

}  // namespace
#endif

void AddCaptionSubpageStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"captionsTitle", IDS_SETTINGS_CAPTIONS},
      {"captionsPreferencesTitle", IDS_SETTINGS_CAPTIONS_PREFERENCES_TITLE},
      {"captionsPreferencesSubtitle",
       IDS_SETTINGS_CAPTIONS_PREFERENCES_SUBTITLE},
      {"captionsTextSize", IDS_SETTINGS_CAPTIONS_TEXT_SIZE},
      {"captionsTextFont", IDS_SETTINGS_CAPTIONS_TEXT_FONT},
      {"captionsTextColor", IDS_SETTINGS_CAPTIONS_TEXT_COLOR},
      {"captionsTextOpacity", IDS_SETTINGS_CAPTIONS_TEXT_OPACITY},
      {"captionsBackgroundOpacity", IDS_SETTINGS_CAPTIONS_BACKGROUND_OPACITY},
      {"captionsOpacityOpaque", IDS_SETTINGS_CAPTIONS_OPACITY_OPAQUE},
      {"captionsOpacitySemiTransparent",
       IDS_SETTINGS_CAPTIONS_OPACITY_SEMI_TRANSPARENT},
      {"captionsOpacityTransparent", IDS_SETTINGS_CAPTIONS_OPACITY_TRANSPARENT},
      {"captionsTextShadow", IDS_SETTINGS_CAPTIONS_TEXT_SHADOW},
      {"captionsTextShadowNone", IDS_SETTINGS_CAPTIONS_TEXT_SHADOW_NONE},
      {"captionsTextShadowRaised", IDS_SETTINGS_CAPTIONS_TEXT_SHADOW_RAISED},
      {"captionsTextShadowDepressed",
       IDS_SETTINGS_CAPTIONS_TEXT_SHADOW_DEPRESSED},
      {"captionsTextShadowUniform", IDS_SETTINGS_CAPTIONS_TEXT_SHADOW_UNIFORM},
      {"captionsTextShadowDropShadow",
       IDS_SETTINGS_CAPTIONS_TEXT_SHADOW_DROP_SHADOW},
      {"captionsBackgroundColor", IDS_SETTINGS_CAPTIONS_BACKGROUND_COLOR},
      {"captionsColorBlack", IDS_SETTINGS_CAPTIONS_COLOR_BLACK},
      {"captionsColorWhite", IDS_SETTINGS_CAPTIONS_COLOR_WHITE},
      {"captionsColorRed", IDS_SETTINGS_CAPTIONS_COLOR_RED},
      {"captionsColorGreen", IDS_SETTINGS_CAPTIONS_COLOR_GREEN},
      {"captionsColorBlue", IDS_SETTINGS_CAPTIONS_COLOR_BLUE},
      {"captionsColorYellow", IDS_SETTINGS_CAPTIONS_COLOR_YELLOW},
      {"captionsColorCyan", IDS_SETTINGS_CAPTIONS_COLOR_CYAN},
      {"captionsColorMagenta", IDS_SETTINGS_CAPTIONS_COLOR_MAGENTA},
      {"captionsDefaultSetting", IDS_SETTINGS_CAPTIONS_DEFAULT_SETTING},
      {"captionsLanguage", IDS_SETTINGS_CAPTIONS_LANGUAGE},
      {"captionsManageLanguagesTitle",
       IDS_SETTINGS_CAPTIONS_MANAGE_LANGUAGES_TITLE},
      {"captionsManageLanguagesSubtitle",
       IDS_SETTINGS_CAPTIONS_MANAGE_LANGUAGES_SUBTITLE},
      {"captionsLiveTranslateTargetLanguage",
       IDS_SETTINGS_CAPTIONS_LIVE_TRANSLATE_TARGET_LANGUAGE},
      {"removeLanguageAriaLabel",
       IDS_SETTINGS_CAPTIONS_REMOVE_LANGUAGE_ARIA_LABEL},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  AddLiveCaptionSectionStrings(html_source);
}

// Live Caption subtitle depends on whether multi-language is supported, and on
// Ash also depends on whether system-wide live caption is enabled.
int GetLiveCaptionSubtitle(const bool multi_language) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!ash::features::IsSystemLiveCaptionEnabled()) {
    return multi_language
               ? IDS_SETTINGS_CAPTIONS_ENABLE_LIVE_CAPTION_SUBTITLE_BROWSER_ONLY
               : IDS_SETTINGS_CAPTIONS_ENABLE_LIVE_CAPTION_SUBTITLE_BROWSER_ONLY_ENGLISH_ONLY;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return multi_language
             ? IDS_SETTINGS_CAPTIONS_ENABLE_LIVE_CAPTION_SUBTITLE
             : IDS_SETTINGS_CAPTIONS_ENABLE_LIVE_CAPTION_SUBTITLE_ENGLISH_ONLY;
}

void AddLiveCaptionSectionStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString(
      "captionsEnableLiveCaptionTitle",
      IDS_SETTINGS_CAPTIONS_ENABLE_LIVE_CAPTION_TITLE);
  html_source->AddLocalizedString(
      "captionsEnableLiveTranslateTitle",
      IDS_SETTINGS_CAPTIONS_ENABLE_LIVE_TRANSLATE_TITLE);
  html_source->AddLocalizedString(
      "captionsEnableLiveTranslateSubtitle",
      IDS_SETTINGS_CAPTIONS_ENABLE_LIVE_TRANSLATE_SUBTITLE);
  html_source->AddLocalizedString(
      "captionsMaskOffensiveWordsTitle",
      IDS_SETTINGS_CAPTIONS_MASK_OFFENSIVE_WORDS_TITLE);

  const bool liveCaptionMultiLanguageEnabled =
      base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage);

  const bool liveTranslateEnabled =
      base::FeatureList::IsEnabled(media::kLiveTranslate);

  const int live_caption_subtitle_message =
      GetLiveCaptionSubtitle(liveCaptionMultiLanguageEnabled);

  html_source->AddLocalizedString("captionsEnableLiveCaptionSubtitle",
                                  live_caption_subtitle_message);
  html_source->AddBoolean("enableLiveCaption",
                          captions::IsLiveCaptionFeatureSupported());
  html_source->AddBoolean("enableLiveCaptionMultiLanguage",
                          liveCaptionMultiLanguageEnabled);

  html_source->AddBoolean("enableLiveTranslate", liveTranslateEnabled);
}

#if BUILDFLAG(IS_CHROMEOS)
void AddPasswordPromptDialogStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"passwordPromptTitle", IDS_SETTINGS_PEOPLE_PASSWORD_PROMPT_TITLE},
      {"passwordPromptInvalidPassword",
       IDS_SETTINGS_PEOPLE_PASSWORD_PROMPT_INVALID_PASSWORD},
      {"passwordPromptPasswordLabel",
       IDS_SETTINGS_PEOPLE_PASSWORD_PROMPT_PASSWORD_LABEL},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void AddSharedSyncPageStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"syncDisabledByAdministrator", IDS_SIGNED_IN_WITH_SYNC_DISABLED_BY_POLICY},
    {"passphrasePlaceholder", IDS_SETTINGS_PASSPHRASE_PLACEHOLDER},
    {"existingPassphraseTitle", IDS_SETTINGS_EXISTING_PASSPHRASE_TITLE},
    {"submitPassphraseButton", IDS_SETTINGS_SUBMIT_PASSPHRASE},
    {"encryptWithGoogleCredentialsLabel",
     IDS_SETTINGS_ENCRYPT_WITH_GOOGLE_CREDENTIALS_LABEL},
    {"encryptionOptionsTitle", IDS_SETTINGS_ENCRYPTION_OPTIONS},
    {"mismatchedPassphraseError", IDS_SETTINGS_MISMATCHED_PASSPHRASE_ERROR},
    {"emptyPassphraseError", IDS_SETTINGS_EMPTY_PASSPHRASE_ERROR},
    {"incorrectPassphraseError", IDS_SETTINGS_INCORRECT_PASSPHRASE_ERROR},
    {"syncPageTitle", IDS_SETTINGS_SYNC_SYNC_AND_NON_PERSONALIZED_SERVICES},
    {"passphraseConfirmationPlaceholder",
     IDS_SETTINGS_PASSPHRASE_CONFIRMATION_PLACEHOLDER},
    {"syncLoading", IDS_SETTINGS_SYNC_LOADING},
    {"syncDataEncryptedText", IDS_SETTINGS_SYNC_DATA_ENCRYPTED_TEXT},
    {"sync", IDS_SETTINGS_SYNC},
    {"manageSyncedDataTitle",
     IDS_SETTINGS_NEW_MANAGE_SYNCED_DATA_TITLE_UNIFIED_CONSENT},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"manageSyncedDataSubtitle",
     IDS_SETTINGS_NEW_MANAGE_SYNCED_DATA_SUBTITLE_UNIFIED_CONSENT},
#endif
    {"manageBrowserSyncedDataTitle",
     IDS_SETTINGS_NEW_MANAGE_BROWSER_SYNCED_DATA_TITLE},
    {"syncAdvancedDevicePageTitle",
     IDS_SETTINGS_NEW_SYNC_ADVANCED_DEVICE_PAGE_TITLE},
    {"syncAdvancedBrowserPageTitle",
     IDS_SETTINGS_NEW_SYNC_ADVANCED_BROWSER_PAGE_TITLE},
    {"enterPassphraseLabel", IDS_SYNC_ENTER_PASSPHRASE_BODY},
    {"enterPassphraseLabelWithDate", IDS_SYNC_ENTER_PASSPHRASE_BODY_WITH_DATE},
    {"existingPassphraseLabelWithDate",
     IDS_SYNC_FULL_ENCRYPTION_BODY_CUSTOM_WITH_DATE},
    {"existingPassphraseLabel", IDS_SYNC_FULL_ENCRYPTION_BODY_CUSTOM},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (base::FeatureList::IsEnabled(syncer::kSyncChromeOSAppsToggleSharing)) {
    html_source->AddLocalizedString(
        "manageSyncedDataSubtitle",
        IDS_SETTINGS_NEW_MANAGE_SYNCED_DATA_SUBTITLE_UNIFIED_CONSENT);
  }
#endif

  std::string sync_dashboard_url =
      google_util::AppendGoogleLocaleParam(
          GURL(chrome::kSyncGoogleDashboardURL),
          g_browser_process->GetApplicationLocale())
          .spec();

  html_source->AddString(
      "passphraseResetHintEncryption",
      l10n_util::GetStringFUTF8(IDS_SETTINGS_PASSPHRASE_RESET_HINT_ENCRYPTION,
                                base::ASCIIToUTF16(sync_dashboard_url)));
  html_source->AddString(
      "passphraseRecover",
      l10n_util::GetStringFUTF8(IDS_SETTINGS_PASSPHRASE_RECOVER,
                                base::ASCIIToUTF16(sync_dashboard_url)));
  html_source->AddString("syncDashboardUrl", sync_dashboard_url);
  html_source->AddString(
      "passphraseExplanationText",
      l10n_util::GetStringFUTF8(IDS_SETTINGS_PASSPHRASE_EXPLANATION_TEXT,
                                base::ASCIIToUTF16(sync_dashboard_url)));
  html_source->AddString(
      "encryptWithSyncPassphraseLabel",
      l10n_util::GetStringFUTF8(
          base::FeatureList::IsEnabled(syncer::kSyncEnableHistoryDataType)
              ? IDS_NEW_SETTINGS_ENCRYPT_WITH_SYNC_PASSPHRASE_LABEL
              : IDS_SETTINGS_ENCRYPT_WITH_SYNC_PASSPHRASE_LABEL,
#if BUILDFLAG(IS_CHROMEOS_ASH)
          GetHelpUrlWithBoard(chrome::kSyncEncryptionHelpURL)));
#else
          base::ASCIIToUTF16(chrome::kSyncEncryptionHelpURL)));
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  html_source->AddBoolean(
      "showSyncSettingsRevamp",
      base::FeatureList::IsEnabled(syncer::kSyncChromeOSAppsToggleSharing) &&
          crosapi::browser_util::IsLacrosEnabled());
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  html_source->AddBoolean(
      "showSyncSettingsRevamp",
      base::FeatureList::IsEnabled(syncer::kSyncChromeOSAppsToggleSharing));
#endif

  html_source->AddString("syncErrorsHelpUrl", chrome::kSyncErrorsHelpURL);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void AddNearbyShareData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"nearbyShareTitle", IDS_SETTINGS_NEARBY_SHARE_TITLE},
      {"nearbyShareSetUpButtonTitle",
       IDS_SETTINGS_NEARBY_SHARE_SET_UP_BUTTON_TITLE},
      {"nearbyShareDeviceNameRowTitle",
       IDS_SETTINGS_NEARBY_SHARE_DEVICE_NAME_ROW_TITLE},
      {"nearbyShareDeviceNameDialogTitle",
       IDS_SETTINGS_NEARBY_SHARE_DEVICE_NAME_DIALOG_TITLE},
      {"nearbyShareDeviceNameFieldLabel",
       IDS_SETTINGS_NEARBY_SHARE_DEVICE_NAME_FIELD_LABEL},
      {"nearbyShareEditDeviceName", IDS_SETTINGS_NEARBY_SHARE_EDIT_DEVICE_NAME},
      {"fastInitiationNotificationToggleTitle",
       IDS_SETTINGS_NEARBY_SHARE_FAST_INITIATION_NOTIFICATION_TOGGLE_TITLE},
      {"fastInitiationNotificationToggleDescription",
       IDS_SETTINGS_NEARBY_SHARE_FAST_INITIATION_NOTIFICATION_TOGGLE_DESCRIPTION},
      {"fastInitiationNotificationToggleAriaLabel",
       IDS_SETTINGS_NEARBY_SHARE_FAST_INITIATION_NOTIFICATION_TOGGLE_ARIA_LABEL},
      {"nearbyShareDeviceNameAriaDescription",
       IDS_SETTINGS_NEARBY_SHARE_DEVICE_NAME_ARIA_DESCRIPTION},
      {"nearbyShareConfirmDeviceName",
       IDS_SETTINGS_NEARBY_SHARE_CONFIRM_DEVICE_NAME},
      {"nearbyShareManageContactsLabel",
       IDS_SETTINGS_NEARBY_SHARE_MANAGE_CONTACTS_LABEL},
      {"nearbyShareManageContactsRowTitle",
       IDS_SETTINGS_NEARBY_SHARE_MANAGE_CONTACTS_ROW_TITLE},
      {"nearbyShareEditDataUsage", IDS_SETTINGS_NEARBY_SHARE_EDIT_DATA_USAGE},
      {"nearbyShareUpdateDataUsage",
       IDS_SETTINGS_NEARBY_SHARE_UPDATE_DATA_USAGE},
      {"nearbyShareDataUsageDialogTitle",
       IDS_SETTINGS_NEARBY_SHARE_DATA_USAGE_DIALOG_TITLE},
      {"nearbyShareDataUsageWifiOnlyLabel",
       IDS_SETTINGS_NEARBY_SHARE_DATA_USAGE_WIFI_ONLY_LABEL},
      {"nearbyShareDataUsageWifiOnlyDescription",
       IDS_SETTINGS_NEARBY_SHARE_DATA_USAGE_WIFI_ONLY_DESCRIPTION},
      {"nearbyShareDataUsageDataLabel",
       IDS_SETTINGS_NEARBY_SHARE_DATA_USAGE_MOBILE_DATA_LABEL},
      {"nearbyShareDataUsageDataDescription",
       IDS_SETTINGS_NEARBY_SHARE_DATA_USAGE_MOBILE_DATA_DESCRIPTION},
      {"nearbyShareDataUsageDataTooltip",
       IDS_SETTINGS_NEARBY_SHARE_DATA_USAGE_MOBILE_DATA_TOOLTIP},
      {"nearbyShareDataUsageOfflineLabel",
       IDS_SETTINGS_NEARBY_SHARE_DATA_USAGE_OFFLINE_LABEL},
      {"nearbyShareDataUsageOfflineDescription",
       IDS_SETTINGS_NEARBY_SHARE_DATA_USAGE_OFFLINE_DESCRIPTION},
      {"nearbyShareDataUsageDataEditButtonDescription",
       IDS_SETTINGS_NEARBY_SHARE_DATA_USAGE_EDIT_BUTTON_DATA_DESCRIPTION},
      {"nearbyShareDataUsageWifiOnlyEditButtonDescription",
       IDS_SETTINGS_NEARBY_SHARE_DATA_USAGE_EDIT_BUTTON_WIFI_ONLY_DESCRIPTION},
      {"nearbyShareDataUsageOfflineEditButtonDescription",
       IDS_SETTINGS_NEARBY_SHARE_DATA_USAGE_EDIT_BUTTON_OFFLINE_DESCRIPTION},
      {"nearbyShareContactVisibilityRowTitle",
       IDS_SETTINGS_NEARBY_SHARE_CONTACT_VISIBILITY_ROW_TITLE},
      {"nearbyShareEditVisibility", IDS_SETTINGS_NEARBY_SHARE_EDIT_VISIBILITY},
      {"nearbyShareVisibilityDialogTitle",
       IDS_SETTINGS_NEARBY_SHARE_VISIBILITY_DIALOG_TITLE},
      {"nearbyShareDescription", IDS_SETTINGS_NEARBY_SHARE_DESCRIPTION},
      {"nearbyShareHighVisibilityTitle",
       IDS_SETTINGS_NEARBY_SHARE_HIGH_VISIBILITY_TITLE},
      {"nearbyShareHighVisibilityOn",
       IDS_SETTINGS_NEARBY_SHARE_HIGH_VISIBILITY_ON},
      {"nearbyShareHighVisibilityOff",
       IDS_SETTINGS_NEARBY_SHARE_HIGH_VISIBILITY_OFF},
      {"nearbyShareVisibilityDialogSave",
       IDS_SETTINGS_NEARBY_SHARE_VISIBILITY_DIALOG_SAVE}};

  html_source->AddLocalizedStrings(kLocalizedStrings);

  // To use lottie, the worker-src CSP needs to be updated for the web ui that
  // is using it. Since as of now there are only a couple of webuis using
  // lottie animations, this update has to be performed manually. As the usage
  // increases, set this as the default so manual override is no longer
  // required.
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc,
      "worker-src blob: chrome://resources 'self';");
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void AddSecureDnsStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"secureDns", IDS_SETTINGS_SECURE_DNS},
    {"secureDnsDescription", IDS_SETTINGS_SECURE_DNS_DESCRIPTION},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"secureDnsWithIdentifiersDescription",
     IDS_SETTINGS_SECURE_DNS_WITH_IDENTIFIERS_DESCRIPTION},
#endif
    {"secureDnsDisabledForManagedEnvironment",
     IDS_SETTINGS_SECURE_DNS_DISABLED_FOR_MANAGED_ENVIRONMENT},
    {"secureDnsDisabledForParentalControl",
     IDS_SETTINGS_SECURE_DNS_DISABLED_FOR_PARENTAL_CONTROL},
    {"secureDnsAutomaticModeDescription",
     IDS_SETTINGS_AUTOMATIC_MODE_DESCRIPTION},
    {"secureDnsAutomaticModeDescriptionSecondary",
     IDS_SETTINGS_AUTOMATIC_MODE_DESCRIPTION_SECONDARY},
    {"secureDnsSecureModeA11yLabel",
     IDS_SETTINGS_SECURE_MODE_DESCRIPTION_ACCESSIBILITY_LABEL},
    {"secureDnsDropdownA11yLabel",
     IDS_SETTINGS_SECURE_DNS_DROPDOWN_ACCESSIBILITY_LABEL},
    {"secureDnsSecureDropdownModeDescription",
     IDS_SETTINGS_SECURE_DROPDOWN_MODE_DESCRIPTION},
    {"secureDnsSecureDropdownModePrivacyPolicy",
     IDS_SETTINGS_SECURE_DROPDOWN_MODE_PRIVACY_POLICY},
    {"secureDnsCustomPlaceholder", IDS_SETTINGS_SECURE_DNS_CUSTOM_PLACEHOLDER},
    {"secureDnsCustomFormatError", IDS_SETTINGS_SECURE_DNS_CUSTOM_FORMAT_ERROR},
    {"secureDnsCustomConnectionError",
     IDS_SETTINGS_SECURE_DNS_CUSTOM_CONNECTION_ERROR},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

}  // namespace settings
