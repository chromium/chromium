// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/shared_settings_localized_strings_provider.h"

#include <string>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/soda/constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "media/base/media_switches.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/common/url_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  AddLiveCaptionSectionStrings(html_source);
}

void AddLiveCaptionSectionStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString(
      "captionsEnableLiveCaptionTitle",
      IDS_SETTINGS_CAPTIONS_ENABLE_LIVE_CAPTION_TITLE);

  const bool liveCaptionMultiLanguageEnabled =
      base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage) &&
      base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption);
  const int live_caption_subtitle_message =
      liveCaptionMultiLanguageEnabled
          ? IDS_SETTINGS_CAPTIONS_ENABLE_LIVE_CAPTION_SUBTITLE
          : IDS_SETTINGS_CAPTIONS_ENABLE_LIVE_CAPTION_SUBTITLE_ENGLISH_ONLY;
  html_source->AddLocalizedString("captionsEnableLiveCaptionSubtitle",
                                  live_caption_subtitle_message);

  base::Optional<speech::SodaLanguagePackComponentConfig> englishConfig =
      speech::GetLanguageComponentConfig(speech::LanguageCode::kEnUs);
  html_source->AddString("sodaLanguageCodeEnglish",
                         englishConfig->language_name);
  html_source->AddLocalizedString("sodaLanguageDisplayNameEnglish",
                                  englishConfig->display_name);
  base::Optional<speech::SodaLanguagePackComponentConfig> frenchConfig =
      speech::GetLanguageComponentConfig(speech::LanguageCode::kFrFr);
  html_source->AddString("sodaLanguageCodeFrench", frenchConfig->language_name);
  html_source->AddLocalizedString("sodaLanguageDisplayNameFrench",
                                  frenchConfig->display_name);
  base::Optional<speech::SodaLanguagePackComponentConfig> germanConfig =
      speech::GetLanguageComponentConfig(speech::LanguageCode::kDeDe);
  html_source->AddString("sodaLanguageCodeGerman", germanConfig->language_name);
  html_source->AddLocalizedString("sodaLanguageDisplayNameGerman",
                                  germanConfig->display_name);
  base::Optional<speech::SodaLanguagePackComponentConfig> italianConfig =
      speech::GetLanguageComponentConfig(speech::LanguageCode::kItIt);
  html_source->AddString("sodaLanguageCodeItalian",
                         italianConfig->language_name);
  html_source->AddLocalizedString("sodaLanguageDisplayNameItalian",
                                  italianConfig->display_name);
  base::Optional<speech::SodaLanguagePackComponentConfig> japaneseConfig =
      speech::GetLanguageComponentConfig(speech::LanguageCode::kJaJp);
  html_source->AddString("sodaLanguageCodeJapanese",
                         japaneseConfig->language_name);
  html_source->AddLocalizedString("sodaLanguageDisplayNameJapanese",
                                  japaneseConfig->display_name);
  base::Optional<speech::SodaLanguagePackComponentConfig> spanishConfig =
      speech::GetLanguageComponentConfig(speech::LanguageCode::kEsEs);
  html_source->AddString("sodaLanguageCodeSpanish",
                         spanishConfig->language_name);
  html_source->AddLocalizedString("sodaLanguageDisplayNameSpanish",
                                  spanishConfig->display_name);

  html_source->AddBoolean("enableLiveCaption",
                          base::FeatureList::IsEnabled(media::kLiveCaption));
  html_source->AddBoolean("enableLiveCaptionMultiLanguage",
                          liveCaptionMultiLanguageEnabled);
}

void AddPersonalizationOptionsStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"urlKeyedAnonymizedDataCollection",
     IDS_SETTINGS_ENABLE_URL_KEYED_ANONYMIZED_DATA_COLLECTION},
    {"urlKeyedAnonymizedDataCollectionDesc",
     IDS_SETTINGS_ENABLE_URL_KEYED_ANONYMIZED_DATA_COLLECTION_DESC},
    {"spellingPref", IDS_SETTINGS_SPELLING_PREF},
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    {"signinAllowedTitle", IDS_SETTINGS_SIGNIN_ALLOWED},
    {"signinAllowedDescription", IDS_SETTINGS_SIGNIN_ALLOWED_DESC},
#endif
    {"searchSuggestPref", IDS_SETTINGS_SUGGEST_PREF},
    {"enablePersonalizationLogging", IDS_SETTINGS_ENABLE_LOGGING_PREF},
    {"enablePersonalizationLoggingDesc", IDS_SETTINGS_ENABLE_LOGGING_PREF_DESC},
    {"spellingDescription", IDS_SETTINGS_SPELLING_PREF_DESC},
    {"searchSuggestPrefDesc", IDS_SETTINGS_SUGGEST_PREF_DESC},
    {"linkDoctorPref", IDS_SETTINGS_LINKDOCTOR_PREF},
    {"linkDoctorPrefDesc", IDS_SETTINGS_LINKDOCTOR_PREF_DESC},
    {"driveSuggestPref", IDS_DRIVE_SUGGEST_PREF},
    {"driveSuggestPrefDesc", IDS_DRIVE_SUGGEST_PREF_DESC},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

void AddSyncControlsStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"autofillCheckboxLabel", IDS_SETTINGS_AUTOFILL_CHECKBOX_LABEL},
      {"historyCheckboxLabel", IDS_SETTINGS_HISTORY_CHECKBOX_LABEL},
      {"extensionsCheckboxLabel", IDS_SETTINGS_EXTENSIONS_CHECKBOX_LABEL},
      {"openTabsCheckboxLabel", IDS_SETTINGS_OPEN_TABS_CHECKBOX_LABEL},
      {"wifiConfigurationsCheckboxLabel",
       IDS_SETTINGS_WIFI_CONFIGURATIONS_CHECKBOX_LABEL},
      {"syncEverythingCheckboxLabel",
       IDS_SETTINGS_SYNC_EVERYTHING_CHECKBOX_LABEL},
      {"appCheckboxLabel", IDS_SETTINGS_APPS_CHECKBOX_LABEL},
      {"enablePaymentsIntegrationCheckboxLabel",
       IDS_AUTOFILL_ENABLE_PAYMENTS_INTEGRATION_CHECKBOX_LABEL},
      {"nonPersonalizedServicesSectionLabel",
       IDS_SETTINGS_NON_PERSONALIZED_SERVICES_SECTION_LABEL},
      {"customizeSyncLabel", IDS_SETTINGS_CUSTOMIZE_SYNC},
      {"syncData", IDS_SETTINGS_SYNC_DATA},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

void AddSyncAccountControlStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"syncingTo", IDS_SETTINGS_PEOPLE_SYNCING_TO_ACCOUNT},
      {"peopleSignIn", IDS_PROFILES_DICE_SIGNIN_BUTTON},
      {"syncPaused", IDS_SETTINGS_PEOPLE_SYNC_PAUSED},
      {"turnOffSync", IDS_SETTINGS_PEOPLE_SYNC_TURN_OFF},
      {"settingsCheckboxLabel", IDS_SETTINGS_SETTINGS_CHECKBOX_LABEL},
      {"syncNotWorking", IDS_SETTINGS_PEOPLE_SYNC_NOT_WORKING},
      {"syncDisabled", IDS_PROFILES_DICE_SYNC_DISABLED_TITLE},
      {"syncPasswordsNotWorking",
       IDS_SETTINGS_PEOPLE_SYNC_PASSWORDS_NOT_WORKING},
      {"peopleSignOut", IDS_SETTINGS_PEOPLE_SIGN_OUT},
      {"useAnotherAccount", IDS_SETTINGS_PEOPLE_SYNC_ANOTHER_ACCOUNT},
      {"syncAdvancedPageTitle", IDS_SETTINGS_NEW_SYNC_ADVANCED_PAGE_TITLE},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif

void AddSyncPageStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"syncDisabledByAdministrator",
       IDS_SIGNED_IN_WITH_SYNC_DISABLED_BY_POLICY},
      {"passwordsCheckboxLabel", IDS_SETTINGS_PASSWORDS_CHECKBOX_LABEL},
      {"passphrasePlaceholder", IDS_SETTINGS_PASSPHRASE_PLACEHOLDER},
      {"peopleSignInSyncPagePromptSecondaryWithAccount",
       IDS_SETTINGS_PEOPLE_SIGN_IN_PROMPT_SECONDARY_WITH_ACCOUNT},
      {"peopleSignInSyncPagePromptSecondaryWithNoAccount",
       IDS_SETTINGS_PEOPLE_SIGN_IN_PROMPT_SECONDARY_WITH_ACCOUNT},
      {"existingPassphraseTitle", IDS_SETTINGS_EXISTING_PASSPHRASE_TITLE},
      {"submitPassphraseButton", IDS_SETTINGS_SUBMIT_PASSPHRASE},
      {"encryptWithGoogleCredentialsLabel",
       IDS_SETTINGS_ENCRYPT_WITH_GOOGLE_CREDENTIALS_LABEL},
      {"bookmarksCheckboxLabel", IDS_SETTINGS_BOOKMARKS_CHECKBOX_LABEL},
      {"encryptionOptionsTitle", IDS_SETTINGS_ENCRYPTION_OPTIONS},
      {"mismatchedPassphraseError", IDS_SETTINGS_MISMATCHED_PASSPHRASE_ERROR},
      {"emptyPassphraseError", IDS_SETTINGS_EMPTY_PASSPHRASE_ERROR},
      {"incorrectPassphraseError", IDS_SETTINGS_INCORRECT_PASSPHRASE_ERROR},
      {"syncPageTitle", IDS_SETTINGS_SYNC_SYNC_AND_NON_PERSONALIZED_SERVICES},
      {"passphraseConfirmationPlaceholder",
       IDS_SETTINGS_PASSPHRASE_CONFIRMATION_PLACEHOLDER},
      {"readingListCheckboxLabel", IDS_SETTINGS_READING_LIST_CHECKBOX_LABEL},
      {"syncLoading", IDS_SETTINGS_SYNC_LOADING},
      {"themesAndWallpapersCheckboxLabel",
       IDS_SETTINGS_THEMES_AND_WALLPAPERS_CHECKBOX_LABEL},
      {"syncDataEncryptedText", IDS_SETTINGS_SYNC_DATA_ENCRYPTED_TEXT},
      {"sync", IDS_SETTINGS_SYNC},
      {"cancelSync", IDS_SETTINGS_SYNC_SETTINGS_CANCEL_SYNC},
      {"syncSetupCancelDialogTitle",
       IDS_SETTINGS_SYNC_SETUP_CANCEL_DIALOG_TITLE},
      {"syncSetupCancelDialogBody", IDS_SETTINGS_SYNC_SETUP_CANCEL_DIALOG_BODY},
      {"personalizeGoogleServicesTitle",
       IDS_SETTINGS_PERSONALIZE_GOOGLE_SERVICES_TITLE},
      {"manageSyncedDataTitle",
       IDS_SETTINGS_NEW_MANAGE_SYNCED_DATA_TITLE_UNIFIED_CONSENT},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

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
  html_source->AddString("activityControlsUrl",
                         chrome::kGoogleAccountActivityControlsURL);
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
          base::ASCIIToUTF16(chrome::kSyncEncryptionHelpURL)));
#endif
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
      {"nearbyShareDeviceNameAriaDescription",
       IDS_SETTINGS_NEARBY_SHARE_DEVICE_NAME_ARIA_DESCRIPTION},
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
       IDS_SETTINGS_NEARBY_SHARE_HIGH_VISIBILITY_OFF}};

  html_source->AddLocalizedStrings(kLocalizedStrings);

  // To use lottie, the worker-src CSP needs to be updated for the web ui that
  // is using it. Since as of now there are only a couple of webuis using
  // lottie animations, this update has to be performed manually. As the usage
  // increases, set this as the default so manual override is no longer
  // required.
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc, "worker-src blob: 'self';");
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace settings
