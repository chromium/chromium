// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/shared_settings_localized_strings_provider.h"

#include <string>

#include "base/feature_list.h"
#include "base/strings/strcat.h"
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
#include "chrome/grit/branded_strings.h"
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
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
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
std::u16string GetHelpUrlWithBoard(const std::u16string& original_url) {
  return base::StrCat(
      {original_url, u"&b=",
       base::ASCIIToUTF16(base::SysInfo::GetLsbReleaseBoard())});
}

}  // namespace
#endif

void AddAxAnnotationsSectionStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"mainNodeAnnotationsDownloadErrorLabel",
       IDS_SETTINGS_MAIN_NODE_ANNOTATIONS_DOWNLOAD_ERROR},
      {"mainNodeAnnotationsDownloadProgressLabel",
       IDS_SETTINGS_MAIN_NODE_ANNOTATIONS_DOWNLOAD_PROGRESS},
      {"mainNodeAnnotationsDownloadingLabel",
       IDS_SETTINGS_MAIN_NODE_ANNOTATIONS_DOWNLOADING},
      {"mainNodeAnnotationsTitle", IDS_SETTINGS_MAIN_NODE_ANNOTATIONS_TITLE},
      {"mainNodeAnnotationsSubtitle",
       IDS_SETTINGS_MAIN_NODE_ANNOTATIONS_SUBTITLE},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
  html_source->AddBoolean(
      "mainNodeAnnotationsEnabled",
      base::FeatureList::IsEnabled(features::kMainNodeAnnotations));
}

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
      {"captionsLiveTranslateTargetLanguageSubtitle",
       IDS_SETTINGS_CAPTIONS_LIVE_TRANSLATE_TARGET_LANGUAGE_SUBTITLE},
      {"removeLanguageLabel", IDS_SETTINGS_CAPTIONS_REMOVE_LANGUAGE_LABEL},
      {"makeDefaultLanguageLabel",
       IDS_SETTINGS_CAPTIONS_MAKE_DEFAULT_LANGUAGE_LABEL},
      {"defaultLanguageLabel", IDS_SETTINGS_CAPTIONS_DEFAULT_LANGUAGE_LABEL},
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

  const bool liveTranslateEnabled = media::IsLiveTranslateEnabled();

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
          IDS_SETTINGS_ENCRYPT_WITH_SYNC_PASSPHRASE_LABEL,
#if BUILDFLAG(IS_CHROMEOS_ASH)
          GetHelpUrlWithBoard(chrome::kSyncEncryptionHelpURL)));
#else
                              chrome::kSyncEncryptionHelpURL));
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

void AddSecureDnsStrings(content::WebUIDataSource* html_source) {
  webui::LocalizedString kLocalizedStrings[] = {
      {"secureDns", IDS_SETTINGS_SECURE_DNS},
      {"secureDnsDescription", IDS_SETTINGS_SECURE_DNS_DESCRIPTION},
      {"secureDnsDisabledForManagedEnvironment",
       IDS_SETTINGS_SECURE_DNS_DISABLED_FOR_MANAGED_ENVIRONMENT},
      {"secureDnsDisabledForParentalControl",
       IDS_SETTINGS_SECURE_DNS_DISABLED_FOR_PARENTAL_CONTROL},
      {"secureDnsAutomaticModeDescription",
       IDS_SETTINGS_AUTOMATIC_MODE_DESCRIPTION},
      {"secureDnsCustomProviderDescription",
       IDS_SETTINGS_SECURE_DNS_CUSTOM_DESCRIPTION},
      {"secureDnsDropdownA11yLabel",
       IDS_SETTINGS_SECURE_DNS_DROPDOWN_ACCESSIBILITY_LABEL},
      {"secureDnsSecureDropdownModeDescription",
       IDS_SETTINGS_SECURE_DROPDOWN_MODE_DESCRIPTION},
      {"secureDnsSecureDropdownModePrivacyPolicy",
       IDS_SETTINGS_SECURE_DROPDOWN_MODE_PRIVACY_POLICY},
      {"secureDnsCustomPlaceholder",
       IDS_SETTINGS_SECURE_DNS_CUSTOM_PLACEHOLDER},
      {"secureDnsCustomFormatError",
       IDS_SETTINGS_SECURE_DNS_CUSTOM_FORMAT_ERROR},
      {"secureDnsCustomConnectionError",
       IDS_SETTINGS_SECURE_DNS_CUSTOM_CONNECTION_ERROR},
#if BUILDFLAG(IS_CHROMEOS_ASH)
      {"secureDnsOsSettingsTitle", IDS_OS_SETTINGS_SECURE_DNS_TITLE},
      {"secureDnsWithIdentifiersDescription",
       IDS_SETTINGS_SECURE_DNS_WITH_IDENTIFIERS_DESCRIPTION},
      {"secureDnsWithIdentifiersAndDomainConfigDescription",
       IDS_OS_SETTINGS_SECURE_DNS_WITH_IDENTIFIERS_AND_DOMAIN_CONFIG_DESCRIPTION},
      {"secureDnsDialogTitle", IDS_OS_SETTINGS_REVAMP_SECURE_DNS_DIALOG_TITLE},
      {"secureDnsDialogBody", IDS_OS_SETTINGS_REVAMP_SECURE_DNS_DIALOG_BODY},
      {"secureDnsDialogCancel",
       IDS_OS_SETTINGS_REVAMP_SECURE_DNS_DIALOG_CANCEL},
      {"secureDnsDialogTurnOff",
       IDS_OS_SETTINGS_REVAMP_SECURE_DNS_DIALOG_TURN_OFF},
      {"secureDnsAutomaticModeDescription",
       IDS_OS_SETTINGS_SECURE_DNS_AUTOMATIC_MODE_DESCRIPTION},
      {"secureDnsSecureDropdownModeNetworkDefaultDescription",
       IDS_OS_SETTINGS_SECURE_DNS_NETWORK_DEFAULT_MODE_DESCRIPTION},
#endif
  };
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const std::u16string product_os_name =
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_OS_NAME);
  html_source->AddString(
      "secureDnsOsSettingsDescription",
      l10n_util::GetStringFUTF16(IDS_OS_SETTINGS_SECURE_DNS_DESCRIPTION,
                                 product_os_name));
  html_source->AddString(
      "secureDnsWithDomainConfigDescription",
      l10n_util::GetStringFUTF16(
          IDS_OS_SETTINGS_SECURE_DNS_WITH_DOMAIN_CONFIG_DESCRIPTION,
          product_os_name));
#endif
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

}  // namespace settings
