// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_localized_strings_provider.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/message_formatter.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/obsolete_system/obsolete_system.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/management/management_ui.h"
#include "chrome/browser/ui/webui/policy_indicator_localized_strings_provider.h"
#include "chrome/browser/ui/webui/settings/reset_settings_handler.h"
#include "chrome/browser/ui/webui/settings/shared_settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/version/version_ui.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/sync_utils.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/browsing_data/core/features.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"
#include "components/content_settings/core/common/features.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/google/core/common/google_util.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_utils.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/version_info/version_info.h"
#include "components/zoom/page_zoom_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "device/fido/features.h"
#include "media/base/media_switches.h"
#include "net/base/url_util.h"
#include "services/device/public/cpp/device_features.h"
#include "third_party/blink/public/common/features.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/strings/grit/ui_strings.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/account_manager_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_switches.h"
#include "chrome/browser/ash/account_manager/account_manager_util.h"
#include "chrome/browser/ash/assistant/assistant_util.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/chromeos/kerberos/kerberos_credentials_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "components/user_manager/user_manager.h"
#include "ui/chromeos/devicetype_utils.h"
#else  // !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/settings/system_handler.h"
#include "ui/accessibility/accessibility_features.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "device/fido/win/webauthn_api.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "base/metrics/field_trial_params.h"
#include "base/strings/strcat.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "ui/base/resource/resource_bundle.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // defined(OS_WIN)

#if defined(USE_NSS_CERTS)
#include "chrome/browser/ui/webui/certificate_manager_localized_strings_provider.h"
#endif
namespace settings {
namespace {

void AddCommonStrings(content::WebUIDataSource* html_source, Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"add", IDS_ADD},
    {"advancedPageTitle", IDS_SETTINGS_ADVANCED},
    {"back", IDS_ACCNAME_BACK},
    {"basicPageTitle", IDS_SETTINGS_BASIC},
    {"cancel", IDS_CANCEL},
    {"clear", IDS_SETTINGS_CLEAR},
    {"close", IDS_CLOSE},
    {"confirm", IDS_CONFIRM},
    {"continue", IDS_SETTINGS_CONTINUE},
    {"controlledByExtension", IDS_SETTINGS_CONTROLLED_BY_EXTENSION},
    {"custom", IDS_SETTINGS_CUSTOM},
    {"delete", IDS_SETTINGS_DELETE},
    {"disable", IDS_DISABLE},
    {"done", IDS_DONE},
    {"edit", IDS_SETTINGS_EDIT},
    {"extensionsLinkTooltip", IDS_SETTINGS_MENU_EXTENSIONS_LINK_TOOLTIP},
    {"fonts", IDS_SETTINGS_FONTS},
    {"learnMore", IDS_LEARN_MORE},
    {"menu", IDS_MENU},
    {"menuButtonLabel", IDS_SETTINGS_MENU_BUTTON_LABEL},
    {"moreActions", IDS_SETTINGS_MORE_ACTIONS},
    {"ok", IDS_OK},
    {"restart", IDS_SETTINGS_RESTART},
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    {"restartToApplyChanges", IDS_SETTINGS_RESTART_TO_APPLY_CHANGES},
#endif
    {"retry", IDS_SETTINGS_RETRY},
    {"save", IDS_SAVE},
    {"searchResultBubbleText", IDS_SEARCH_RESULT_BUBBLE_TEXT},
    {"searchResultsBubbleText", IDS_SEARCH_RESULTS_BUBBLE_TEXT},
    {"sentenceEnd", IDS_SENTENCE_END},
    {"settings", IDS_SETTINGS_SETTINGS},
    {"settingsAltPageTitle", IDS_SETTINGS_ALT_PAGE_TITLE},
    {"subpageArrowRoleDescription", IDS_SETTINGS_SUBPAGE_BUTTON},
    {"subpageBackButtonAriaLabel", IDS_SETTINGS_SUBPAGE_BACK_BUTTON_ARIA_LABEL},
    {"subpageBackButtonAriaRoleDescription",
     IDS_SETTINGS_SUBPAGE_BACK_BUTTON_ARIA_ROLE_DESCRIPTION},
    {"notValid", IDS_SETTINGS_NOT_VALID},
    {"notValidWebAddress", IDS_SETTINGS_NOT_VALID_WEB_ADDRESS},
    {"notValidWebAddressForContentType",
     IDS_SETTINGS_NOT_VALID_WEB_ADDRESS_FOR_CONTENT_TYPE},

    // Common font related strings shown in a11y and appearance sections.
    {"quickBrownFox", IDS_SETTINGS_QUICK_BROWN_FOX},
    {"verySmall", IDS_SETTINGS_VERY_SMALL_FONT},
    {"small", IDS_SETTINGS_SMALL_FONT},
    {"medium", IDS_SETTINGS_MEDIUM_FONT},
    {"large", IDS_SETTINGS_LARGE_FONT},
    {"veryLarge", IDS_SETTINGS_VERY_LARGE_FONT},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddBoolean(
      "isGuest",
#if BUILDFLAG(IS_CHROMEOS_ASH)
      user_manager::UserManager::Get()->IsLoggedInAsGuest() ||
          user_manager::UserManager::Get()->IsLoggedInAsPublicAccount());
#else
      profile->IsGuestSession() || profile->IsEphemeralGuestProfile());
#endif

  html_source->AddBoolean("isSupervised", profile->IsSupervised());
}

void AddA11yStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"moreFeaturesLink", IDS_SETTINGS_MORE_FEATURES_LINK},
    {"a11yPageTitle", IDS_SETTINGS_ACCESSIBILITY},
    {"a11yWebStore", IDS_SETTINGS_ACCESSIBILITY_WEB_STORE},
    {"moreFeaturesLinkDescription",
     IDS_SETTINGS_MORE_FEATURES_LINK_DESCRIPTION},
    {"accessibleImageLabelsTitle", IDS_SETTINGS_ACCESSIBLE_IMAGE_LABELS_TITLE},
    {"accessibleImageLabelsSubtitle",
     IDS_SETTINGS_ACCESSIBLE_IMAGE_LABELS_SUBTITLE},
    {"settingsSliderRoleDescription",
     IDS_SETTINGS_SLIDER_MIN_MAX_ARIA_ROLE_DESCRIPTION},
    {"caretBrowsingTitle", IDS_SETTINGS_ENABLE_CARET_BROWSING_TITLE},
    {"caretBrowsingSubtitle", IDS_SETTINGS_ENABLE_CARET_BROWSING_SUBTITLE},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"manageAccessibilityFeatures",
     IDS_SETTINGS_ACCESSIBILITY_MANAGE_ACCESSIBILITY_FEATURES},
#else  // !BUILDFLAG(IS_CHROMEOS_ASH)
    {"focusHighlightLabel",
     IDS_SETTINGS_ACCESSIBILITY_FOCUS_HIGHLIGHT_DESCRIPTION},
#endif
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

#if defined(OS_WIN)
  html_source->AddBoolean("isWindows10OrNewer",
                          base::win::GetVersion() >= base::win::Version::WIN10);
#endif
  html_source->AddBoolean(
      "showExperimentalA11yLabels",
      base::FeatureList::IsEnabled(features::kExperimentalAccessibilityLabels));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  html_source->AddBoolean(
      "showFocusHighlightOption",
      base::FeatureList::IsEnabled(features::kAccessibilityFocusHighlight));
#endif

  AddCaptionSubpageStrings(html_source);
}

void AddAboutStrings(content::WebUIDataSource* html_source, Profile* profile) {
  // Top level About Page strings.
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"aboutProductLogoAlt", IDS_SHORT_PRODUCT_LOGO_ALT_TEXT},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {"aboutReportAnIssue", IDS_SETTINGS_ABOUT_PAGE_REPORT_AN_ISSUE},
#endif
    {"aboutRelaunch", IDS_SETTINGS_ABOUT_PAGE_RELAUNCH},
    {"aboutUpgradeCheckStarted", IDS_SETTINGS_ABOUT_UPGRADE_CHECK_STARTED},
    {"aboutUpgradeRelaunch", IDS_SETTINGS_UPGRADE_SUCCESSFUL_RELAUNCH},
    {"aboutUpgradeUpdating", IDS_SETTINGS_UPGRADE_UPDATING},
    {"aboutUpgradeUpdatingPercent", IDS_SETTINGS_UPGRADE_UPDATING_PERCENT},
    {"aboutGetHelpUsingChrome", IDS_SETTINGS_GET_HELP_USING_CHROME},
    {"aboutPageTitle", IDS_SETTINGS_ABOUT_PROGRAM},
    {"aboutProductTitle", IDS_PRODUCT_NAME},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"aboutUpdateOsSettingsLink",
     IDS_SETTINGS_ABOUT_SEE_OS_SETTINGS_FOR_UPDATE_MESSAGE},
#endif
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString("managementPage",
                         ManagementUI::GetManagementPageSubtitle(profile));
  html_source->AddString(
      "aboutUpgradeUpToDate",
#if BUILDFLAG(IS_CHROMEOS_ASH)
      ui::SubstituteChromeOSDeviceType(IDS_SETTINGS_UPGRADE_UP_TO_DATE));
#else
      l10n_util::GetStringUTF16(IDS_SETTINGS_UPGRADE_UP_TO_DATE));
#endif

  html_source->AddString(
      "aboutBrowserVersion",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_ABOUT_PAGE_BROWSER_VERSION,
          base::UTF8ToUTF16(version_info::GetVersionNumber()),
          l10n_util::GetStringUTF16(version_info::IsOfficialBuild()
                                        ? IDS_VERSION_UI_OFFICIAL
                                        : IDS_VERSION_UI_UNOFFICIAL),
          base::UTF8ToUTF16(
              chrome::GetChannelName(chrome::WithExtendedStable(true))),
          l10n_util::GetStringUTF16(VersionUI::VersionProcessorVariation())));
  html_source->AddString(
      "aboutProductCopyright",
      base::i18n::MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_COPYRIGHT),
          base::Time::Now()));

  std::u16string license = l10n_util::GetStringFUTF16(
      IDS_VERSION_UI_LICENSE, base::ASCIIToUTF16(chrome::kChromiumProjectURL),
      base::ASCIIToUTF16(chrome::kChromeUICreditsURL));
  html_source->AddString("aboutProductLicense", license);

  html_source->AddBoolean("aboutObsoleteNowOrSoon",
                          ObsoleteSystem::IsObsoleteNowOrSoon());
  html_source->AddBoolean("aboutObsoleteEndOfTheLine",
                          ObsoleteSystem::IsObsoleteNowOrSoon() &&
                              ObsoleteSystem::IsEndOfTheLine());
  html_source->AddString("aboutObsoleteSystem",
                         ObsoleteSystem::LocalizedObsoleteString());
  html_source->AddString("aboutObsoleteSystemURL",
                         ObsoleteSystem::GetLinkURL());

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  html_source->AddString("aboutTermsURL", chrome::kChromeUITermsURL);
  html_source->AddLocalizedString("aboutProductTos",
                                  IDS_ABOUT_TERMS_OF_SERVICE);
#endif
}

void AddAppearanceStrings(content::WebUIDataSource* html_source,
                          Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"appearancePageTitle", IDS_SETTINGS_APPEARANCE},
    {"customWebAddress", IDS_SETTINGS_CUSTOM_WEB_ADDRESS},
    {"enterCustomWebAddress", IDS_SETTINGS_ENTER_CUSTOM_WEB_ADDRESS},
    {"homeButtonDisabled", IDS_SETTINGS_HOME_BUTTON_DISABLED},
    {"themes", IDS_SETTINGS_THEMES},
    {"chromeColors", IDS_SETTINGS_CHROME_COLORS},
    {"showHomeButton", IDS_SETTINGS_SHOW_HOME_BUTTON},
    {"showBookmarksBar", IDS_SETTINGS_SHOW_BOOKMARKS_BAR},
    {"homePageNtp", IDS_SETTINGS_HOME_PAGE_NTP},
    {"changeHomePage", IDS_SETTINGS_CHANGE_HOME_PAGE},
    {"themesGalleryUrl", IDS_THEMES_GALLERY_URL},
    {"chooseFromWebStore", IDS_SETTINGS_WEB_STORE},
    {"pageZoom", IDS_SETTINGS_PAGE_ZOOM_LABEL},
    {"fontSize", IDS_SETTINGS_FONT_SIZE_LABEL},
    {"customizeFonts", IDS_SETTINGS_CUSTOMIZE_FONTS},
    {"standardFont", IDS_SETTINGS_STANDARD_FONT_LABEL},
    {"serifFont", IDS_SETTINGS_SERIF_FONT_LABEL},
    {"sansSerifFont", IDS_SETTINGS_SANS_SERIF_FONT_LABEL},
    {"fixedWidthFont", IDS_SETTINGS_FIXED_WIDTH_FONT_LABEL},
    {"minimumFont", IDS_SETTINGS_MINIMUM_FONT_SIZE_LABEL},
    {"tiny", IDS_SETTINGS_TINY_FONT_SIZE},
    {"huge", IDS_SETTINGS_HUGE_FONT_SIZE},
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS)
    {"systemTheme", IDS_SETTINGS_SYSTEM_THEME},
    {"useSystemTheme", IDS_SETTINGS_USE_SYSTEM_THEME},
    {"classicTheme", IDS_SETTINGS_CLASSIC_THEME},
    {"useClassicTheme", IDS_SETTINGS_USE_CLASSIC_THEME},
#else
    {"resetToDefaultTheme", IDS_SETTINGS_RESET_TO_DEFAULT_THEME},
#endif
#if defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS)
    {"showWindowDecorations", IDS_SHOW_WINDOW_DECORATIONS},
#endif
#if defined(OS_MAC)
    {"tabsToLinks", IDS_SETTINGS_TABS_TO_LINKS_PREF},
    {"warnBeforeQuitting", IDS_SETTINGS_WARN_BEFORE_QUITTING_PREF},
#endif
    {"readerMode", IDS_SETTINGS_READER_MODE},
    {"readerModeDescription", IDS_SETTINGS_READER_MODE_DESCRIPTION},
    {"themeManagedDialogTitle", IDS_NTP_THEME_MANAGED_DIALOG_TITLE},
    {"themeManagedDialogBody", IDS_NTP_THEME_MANAGED_DIALOG_BODY},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString("presetZoomFactors",
                         zoom::GetPresetZoomFactorsAsJSON());
  html_source->AddBoolean("showReaderModeOption",
                          dom_distiller::OfferReaderModeInSettings());
}

void AddClearBrowsingDataStrings(content::WebUIDataSource* html_source,
                                 Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"clearTimeRange", IDS_SETTINGS_CLEAR_PERIOD_TITLE},
      {"clearBrowsingDataWithSync", IDS_SETTINGS_CLEAR_BROWSING_DATA_WITH_SYNC},
      {"clearBrowsingDataWithSyncError",
       IDS_SETTINGS_CLEAR_BROWSING_DATA_WITH_SYNC_ERROR},
      {"clearBrowsingDataWithSyncPassphraseError",
       IDS_SETTINGS_CLEAR_BROWSING_DATA_WITH_SYNC_PASSPHRASE_ERROR},
      {"clearBrowsingDataWithSyncPaused",
       IDS_SETTINGS_CLEAR_BROWSING_DATA_WITH_SYNC_PAUSED},
      {"clearBrowsingHistory", IDS_SETTINGS_CLEAR_BROWSING_HISTORY},
      {"clearBrowsingHistorySummary",
       IDS_SETTINGS_CLEAR_BROWSING_HISTORY_SUMMARY},
      {"clearDownloadHistory", IDS_SETTINGS_CLEAR_DOWNLOAD_HISTORY},
      {"clearCache", IDS_SETTINGS_CLEAR_CACHE},
      {"clearCookies", IDS_SETTINGS_CLEAR_COOKIES},
      {"clearCookiesSummary",
       IDS_SETTINGS_CLEAR_COOKIES_AND_SITE_DATA_SUMMARY_BASIC},
      {"clearCookiesSummarySignedIn",
       IDS_SETTINGS_CLEAR_COOKIES_AND_SITE_DATA_SUMMARY_BASIC_WITH_EXCEPTION},
      {"clearCookiesCounter", IDS_DEL_COOKIES_COUNTER},
      {"clearPasswords", IDS_SETTINGS_CLEAR_PASSWORDS},
      {"clearFormData", IDS_SETTINGS_CLEAR_FORM_DATA},
      {"clearHostedAppData", IDS_SETTINGS_CLEAR_HOSTED_APP_DATA},
      {"clearPeriodHour", IDS_SETTINGS_CLEAR_PERIOD_HOUR},
      {"clearPeriod24Hours", IDS_SETTINGS_CLEAR_PERIOD_24_HOURS},
      {"clearPeriod7Days", IDS_SETTINGS_CLEAR_PERIOD_7_DAYS},
      {"clearPeriod4Weeks", IDS_SETTINGS_CLEAR_PERIOD_FOUR_WEEKS},
      {"clearPeriodEverything", IDS_SETTINGS_CLEAR_PERIOD_EVERYTHING},
      {"historyDeletionDialogTitle",
       IDS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_TITLE},
      {"historyDeletionDialogOK", IDS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_OK},
      {"passwordsDeletionDialogTitle",
       IDS_CLEAR_BROWSING_DATA_PASSWORDS_NOTICE_TITLE},
      {"passwordsDeletionDialogOK",
       IDS_CLEAR_BROWSING_DATA_PASSWORDS_NOTICE_OK},
      {"installedAppsConfirm", IDS_SETTINGS_CLEAR_INSTALLED_APPS_DATA_CONFIRM},
      {"installedAppsTitle", IDS_SETTINGS_CLEAR_INSTALLED_APPS_DATA_TITLE},
      {"notificationWarning", IDS_SETTINGS_NOTIFICATION_WARNING},
  };

  html_source->AddString(
      "clearBrowsingHistorySummarySignedIn",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CLEAR_BROWSING_HISTORY_SUMMARY_SIGNED_IN,
          base::ASCIIToUTF16(chrome::kMyActivityUrlInClearBrowsingData)));
  html_source->AddString(
      "clearBrowsingHistorySummarySynced",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CLEAR_BROWSING_HISTORY_SUMMARY_SYNCED,
          base::ASCIIToUTF16(chrome::kMyActivityUrlInClearBrowsingData)));
  html_source->AddString(
      "historyDeletionDialogBody",
      l10n_util::GetStringFUTF16(
          IDS_CLEAR_BROWSING_DATA_HISTORY_NOTICE,
          l10n_util::GetStringUTF16(
              IDS_SETTINGS_CLEAR_DATA_MYACTIVITY_URL_IN_DIALOG)));
  html_source->AddString(
      "passwordsDeletionDialogBody",
      l10n_util::GetStringFUTF16(
          IDS_CLEAR_BROWSING_DATA_PASSWORDS_NOTICE,
          l10n_util::GetStringUTF16(IDS_PASSWORDS_WEB_LINK)));

  html_source->AddLocalizedStrings(kLocalizedStrings);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)
void AddDefaultBrowserStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"defaultBrowser", IDS_SETTINGS_DEFAULT_BROWSER},
      {"defaultBrowserDefault", IDS_SETTINGS_DEFAULT_BROWSER_DEFAULT},
      {"defaultBrowserMakeDefault", IDS_SETTINGS_DEFAULT_BROWSER_MAKE_DEFAULT},
      {"defaultBrowserMakeDefaultButton",
       IDS_SETTINGS_DEFAULT_BROWSER_MAKE_DEFAULT_BUTTON},
      {"defaultBrowserError", IDS_SETTINGS_DEFAULT_BROWSER_ERROR},
      {"defaultBrowserSecondary", IDS_SETTINGS_DEFAULT_BROWSER_SECONDARY},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}
#endif

void AddDownloadsStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"downloadsPageTitle", IDS_SETTINGS_DOWNLOADS},
      {"downloadLocation", IDS_SETTINGS_DOWNLOAD_LOCATION},
      {"changeDownloadLocation", IDS_SETTINGS_CHANGE_DOWNLOAD_LOCATION},
      {"promptForDownload", IDS_SETTINGS_PROMPT_FOR_DOWNLOAD},
      {"openFileTypesAutomatically",
       IDS_SETTINGS_OPEN_FILE_TYPES_AUTOMATICALLY},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
void AddChromeCleanupStrings(content::WebUIDataSource* html_source) {
  const char16_t kUnwantedSoftwareProtectionWhitePaperUrl[] =
      u"https://www.google.ca/chrome/browser/privacy/"
      u"whitepaper.html#unwantedsoftware";

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"chromeCleanupPageTitle",
       IDS_SETTINGS_RESET_CLEAN_UP_COMPUTER_PAGE_TITLE},
      {"chromeCleanupDetailsFilesAndPrograms",
       IDS_SETTINGS_RESET_CLEANUP_DETAILS_FILES_AND_PROGRAMS},
      {"chromeCleanupDetailsRegistryEntries",
       IDS_SETTINGS_RESET_CLEANUP_DETAILS_REGISTRY_ENTRIES},
      {"chromeCleanupExplanationCleanupError",
       IDS_SETTINGS_RESET_CLEANUP_EXPLANATION_CLEANUP_ERROR},
      {"chromeCleanupExplanationFindAndRemove",
       IDS_SETTINGS_RESET_CLEANUP_EXPLANATION_FIND_AND_REMOVE},
      {"chromeCleanupExplanationNoInternet",
       IDS_SETTINGS_RESET_CLEANUP_EXPLANATION_NO_INTERNET_CONNECTION},
      {"chromeCleanupExplanationPermissionsNeeded",
       IDS_SETTINGS_RESET_CLEANUP_EXPLANATION_PERMISSIONS_NEEDED},
      {"chromeCleanupExplanationRemove",
       // Note: removal explanation should be the same as used in the prompt
       // dialog. Reusing the string to ensure they will not diverge.
       IDS_CHROME_CLEANUP_PROMPT_EXPLANATION},
      {"chromeCleanupExplanationRemoving",
       IDS_SETTINGS_RESET_CLEANUP_EXPLANATION_CURRENTLY_REMOVING},
      {"chromeCleanupExplanationScanError",
       IDS_SETTINGS_RESET_CLEANUP_EXPLANATION_SCAN_ERROR},
      {"chromeCleanupFindButtonLabel",
       IDS_SETTINGS_RESET_CLEANUP_FIND_BUTTON_LABEL},
      {"chromeCleanupLinkShowItems",
       IDS_SETTINGS_RESET_CLEANUP_LINK_SHOW_FILES},
      {"chromeCleanupRemoveButtonLabel",
       IDS_SETTINGS_RESET_CLEANUP_REMOVE_BUTTON_LABEL},
      {"chromeCleanupRestartButtonLabel",
       IDS_SETTINGS_RESET_CLEANUP_RESTART_BUTTON_LABEL},
      {"chromeCleanupTitleErrorCantRemove",
       IDS_SETTINGS_RESET_CLEANUP_TITLE_ERROR_CANT_REMOVE},
      {"chromeCleanupTitleErrorPermissions",
       IDS_SETTINGS_RESET_CLEANUP_TITLE_ERROR_PERMISSIONS_NEEDED},
      {"chromeCleanupTitleFindAndRemove",
       IDS_SETTINGS_RESET_CLEANUP_TITLE_FIND_HARMFUL_SOFTWARE},
      {"chromeCleanupTitleNoInternet",
       IDS_SETTINGS_RESET_CLEANUP_TITLE_NO_INTERNET_CONNECTION},
      {"chromeCleanupTitleNothingFound",
       IDS_SETTINGS_RESET_CLEANUP_TITLE_NOTHING_FOUND},
      {"chromeCleanupTitleRemove", IDS_SETTINGS_RESET_CLEANUP_TITLE_REMOVE},
      {"chromeCleanupTitleRemoved", IDS_SETTINGS_RESET_CLEANUP_TITLE_DONE},
      {"chromeCleanupTitleRemoving", IDS_SETTINGS_RESET_CLEANUP_TITLE_REMOVING},
      {"chromeCleanupTitleRestart", IDS_SETTINGS_RESET_CLEANUP_TITLE_RESTART},
      {"chromeCleanupTitleScanning", IDS_SETTINGS_RESET_CLEANUP_TITLE_SCANNING},
      {"chromeCleanupTitleScanningFailed",
       IDS_SETTINGS_RESET_CLEANUP_TITLE_ERROR_SCANNING_FAILED},
      {"chromeCleanupTitleTryAgainButtonLabel",
       IDS_SETTINGS_RESET_CLEANUP_TRY_AGAIN_BUTTON_LABEL},
      {"chromeCleanupExplanationLogsPermissionPref",
       IDS_SETTINGS_RESET_CLEANUP_LOGS_PERMISSION_PREF},
      {"chromeCleanupExplanationNotificationPermission",
       IDS_SETTINGS_RESET_CLEANUP_EXPLANTION_NOTIFICATION_PERMISSION},
      {"chromeCleanupTitleCleanupUnavailable",
       IDS_SETTINGS_RESET_CLEANUP_TITLE_CLEANUP_UNAVAILABLE},
      {"chromeCleanupExplanationCleanupUnavailable",
       IDS_SETTINGS_RESET_CLEANUP_EXPLANATION_CLEANUP_UNAVAILABLE},
  };

  html_source->AddLocalizedStrings(kLocalizedStrings);
  const std::string cleanup_learn_more_url =
      google_util::AppendGoogleLocaleParam(
          GURL(chrome::kChromeCleanerLearnMoreURL),
          g_browser_process->GetApplicationLocale())
          .spec();
  html_source->AddString("chromeCleanupLearnMoreUrl", cleanup_learn_more_url);

  // The "powered by" footer contains an HTML fragment with the SVG logo of the
  // partner. The logo is added directly to the DOM, rather than as an <img>
  // src, to make sure that screen readers can find accessibility tags inside
  // the SVG.
  const std::string powered_by_element = base::StrCat(
      {"<span id='powered-by-logo'>",
       ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
           IDR_CHROME_CLEANUP_PARTNER),
       "</span>"});
  const std::u16string powered_by_html =
      l10n_util::GetStringFUTF16(IDS_SETTINGS_RESET_CLEANUP_FOOTER_POWERED_BY,
                                 base::UTF8ToUTF16(powered_by_element));
  html_source->AddString("chromeCleanupPoweredByHtml", powered_by_html);

  const std::u16string cleanup_details_explanation =
      l10n_util::GetStringFUTF16(IDS_SETTINGS_RESET_CLEANUP_DETAILS_EXPLANATION,
                                 kUnwantedSoftwareProtectionWhitePaperUrl);
  html_source->AddString("chromeCleanupDetailsExplanation",
                         cleanup_details_explanation);
}

void AddIncompatibleApplicationsStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"incompatibleApplicationsResetCardTitle",
       IDS_SETTINGS_INCOMPATIBLE_APPLICATIONS_RESET_CARD_TITLE},
      {"incompatibleApplicationsSubpageSubtitle",
       IDS_SETTINGS_INCOMPATIBLE_APPLICATIONS_SUBPAGE_SUBTITLE},
      {"incompatibleApplicationsSubpageSubtitleNoAdminRights",
       IDS_SETTINGS_INCOMPATIBLE_APPLICATIONS_SUBPAGE_SUBTITLE_NO_ADMIN_RIGHTS},
      {"incompatibleApplicationsListTitle",
       IDS_SETTINGS_INCOMPATIBLE_APPLICATIONS_LIST_TITLE},
      {"incompatibleApplicationsRemoveButton",
       IDS_SETTINGS_INCOMPATIBLE_APPLICATIONS_REMOVE_BUTTON},
      {"incompatibleApplicationsUpdateButton",
       IDS_SETTINGS_INCOMPATIBLE_APPLICATIONS_UPDATE_BUTTON},
      {"incompatibleApplicationsDone",
       IDS_SETTINGS_INCOMPATIBLE_APPLICATIONS_DONE},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  // The help URL is provided via Field Trial param. If none is provided, the
  // "Learn How" text is left empty so that no link is displayed.
  std::u16string learn_how_text;
  std::string help_url = GetFieldTrialParamValueByFeature(
      features::kIncompatibleApplicationsWarning, "HelpURL");
  if (!help_url.empty()) {
    learn_how_text = l10n_util::GetStringFUTF16(
        IDS_SETTINGS_INCOMPATIBLE_APPLICATIONS_SUBPAGE_LEARN_HOW,
        base::UTF8ToUTF16(help_url));
  }
  html_source->AddString("incompatibleApplicationsSubpageLearnHow",
                         learn_how_text);
}
#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

void AddResetStrings(content::WebUIDataSource* html_source, Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {"resetPageTitle", IDS_SETTINGS_RESET_AND_CLEANUP},
#else
    {"resetPageTitle", IDS_SETTINGS_RESET},
#endif
    {"resetTrigger", IDS_SETTINGS_RESET_SETTINGS_TRIGGER},
    {"resetPageExplanation", IDS_RESET_PROFILE_SETTINGS_EXPLANATION},
    {"resetPageExplanationBulletPoints",
     IDS_RESET_PROFILE_SETTINGS_EXPLANATION_IN_BULLET_POINTS},
    {"triggeredResetPageExplanation",
     IDS_TRIGGERED_RESET_PROFILE_SETTINGS_EXPLANATION},
    {"triggeredResetPageTitle", IDS_TRIGGERED_RESET_PROFILE_SETTINGS_TITLE},
    {"resetDialogTitle", IDS_SETTINGS_RESET_PROMPT_TITLE},
    {"resetDialogCommit", IDS_SETTINGS_RESET},
    {"resetPageFeedback", IDS_SETTINGS_RESET_PROFILE_FEEDBACK},

    // Automatic reset banner (now a dialog).
    {"resetAutomatedDialogTitle", IDS_SETTINGS_RESET_AUTOMATED_DIALOG_TITLE},
    {"resetProfileBannerButton", IDS_SETTINGS_RESET_BANNER_RESET_BUTTON_TEXT},
    {"resetProfileBannerDescription", IDS_SETTINGS_RESET_BANNER_TEXT},
#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {"resetCleanupComputerTrigger",
     IDS_SETTINGS_RESET_CLEAN_UP_COMPUTER_TRIGGER},
#endif
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddBoolean(
      "showResetProfileBanner",
      ResetSettingsHandler::ShouldShowResetProfileBanner(profile));
  bool is_reset_shortcuts_feature_enabled = false;
#if defined(OS_WIN)
  is_reset_shortcuts_feature_enabled =
      base::FeatureList::IsEnabled(safe_browsing::kResetShortcutsFeature);
#endif
  html_source->AddBoolean("showExplanationWithBulletPoints",
                          is_reset_shortcuts_feature_enabled);

  html_source->AddString("resetPageLearnMoreUrl",
                         chrome::kResetProfileSettingsLearnMoreURL);
  html_source->AddString("resetProfileBannerLearnMoreUrl",
                         chrome::kAutomaticSettingsResetLearnMoreURL);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void AddImportDataStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"importTitle", IDS_SETTINGS_IMPORT_SETTINGS_TITLE},
      {"importFromLabel", IDS_SETTINGS_IMPORT_FROM_LABEL},
      {"importDescription", IDS_SETTINGS_IMPORT_ITEMS_LABEL},
      {"importLoading", IDS_SETTINGS_IMPORT_LOADING_PROFILES},
      {"importHistory", IDS_SETTINGS_IMPORT_HISTORY_CHECKBOX},
      {"importFavorites", IDS_SETTINGS_IMPORT_FAVORITES_CHECKBOX},
      {"importPasswords", IDS_SETTINGS_IMPORT_PASSWORDS_CHECKBOX},
      {"importSearch", IDS_SETTINGS_IMPORT_SEARCH_ENGINES_CHECKBOX},
      {"importAutofillFormData",
       IDS_SETTINGS_IMPORT_AUTOFILL_FORM_DATA_CHECKBOX},
      {"importChooseFile", IDS_SETTINGS_IMPORT_CHOOSE_FILE},
      {"importCommit", IDS_SETTINGS_IMPORT_COMMIT},
      {"noProfileFound", IDS_SETTINGS_IMPORT_NO_PROFILE_FOUND},
      {"importSuccess", IDS_SETTINGS_IMPORT_SUCCESS},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}
#endif

void AddLanguagesStrings(content::WebUIDataSource* html_source,
                         Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"languagesListTitle", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_TITLE},
    {"searchLanguages", IDS_SETTINGS_LANGUAGE_SEARCH},
    {"languagesExpandA11yLabel",
     IDS_SETTINGS_LANGUAGES_EXPAND_ACCESSIBILITY_LABEL},
    {"orderBrowserLanguagesInstructions",
     IDS_SETTINGS_LANGUAGES_BROWSER_LANGUAGES_LIST_ORDERING_INSTRUCTIONS},
    {"moveToTop", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_TO_TOP},
    {"moveUp", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_UP},
    {"moveDown", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_DOWN},
    {"removeLanguage", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_REMOVE},
    {"addLanguages", IDS_SETTINGS_LANGUAGES_LANGUAGES_ADD},
    {"addLanguagesDialogTitle", IDS_SETTINGS_LANGUAGES_MANAGE_LANGUAGES_TITLE},
    {"allLanguages", IDS_SETTINGS_LANGUAGES_ALL_LANGUAGES},
    {"enabledLanguages", IDS_SETTINGS_LANGUAGES_ENABLED_LANGUAGES},
    {"isDisplayedInThisLanguage",
     IDS_SETTINGS_LANGUAGES_IS_DISPLAYED_IN_THIS_LANGUAGE},
    {"displayInThisLanguage", IDS_SETTINGS_LANGUAGES_DISPLAY_IN_THIS_LANGUAGE},
    {"offerToTranslateInThisLanguage",
     IDS_SETTINGS_LANGUAGES_OFFER_TO_TRANSLATE_IN_THIS_LANGUAGE},
    {"offerToEnableTranslate",
     IDS_SETTINGS_LANGUAGES_OFFER_TO_ENABLE_TRANSLATE},
    {"translateTargetLabel", IDS_SETTINGS_LANGUAGES_TRANSLATE_TARGET},
    {"spellCheckTitle", IDS_SETTINGS_LANGUAGES_SPELL_CHECK_TITLE},
    {"spellCheckBasicLabel", IDS_SETTINGS_LANGUAGES_SPELL_CHECK_BASIC_LABEL},
    {"spellCheckEnhancedLabel",
     IDS_SETTINGS_LANGUAGES_SPELL_CHECK_ENHANCED_LABEL},
    {"spellCheckEnhancedDescription",
     IDS_SETTINGS_LANGUAGES_SPELL_CHECK_ENHANCED_DESCRIPTION},
    // Managed dialog strings:
    {"languageManagedDialogTitle", IDS_SETTINGS_LANGUAGES_MANAGED_DIALOG_TITLE},
    {"languageManagedDialogBody", IDS_SETTINGS_LANGUAGES_MANAGED_DIALOG_BODY},
#if !defined(OS_MAC)
    {"spellCheckDisabledReason",
     IDS_SETTING_LANGUAGES_SPELL_CHECK_DISABLED_REASON},
    {"spellCheckLanguagesListTitle",
     IDS_SETTINGS_LANGUAGES_SPELL_CHECK_LANGUAGES_LIST_TITLE},
    {"manageSpellCheck", IDS_SETTINGS_LANGUAGES_SPELL_CHECK_MANAGE},
    {"editDictionaryPageTitle", IDS_SETTINGS_LANGUAGES_EDIT_DICTIONARY_TITLE},
    {"addDictionaryWordLabel", IDS_SETTINGS_LANGUAGES_ADD_DICTIONARY_WORD},
    {"addDictionaryWordButton",
     IDS_SETTINGS_LANGUAGES_ADD_DICTIONARY_WORD_BUTTON},
    {"addDictionaryWordDuplicateError",
     IDS_SETTINGS_LANGUAGES_ADD_DICTIONARY_WORD_DUPLICATE_ERROR},
    {"addDictionaryWordLengthError",
     IDS_SETTINGS_LANGUAGES_ADD_DICTIONARY_WORD_LENGTH_ERROR},
    {"deleteDictionaryWordButton",
     IDS_SETTINGS_LANGUAGES_DELETE_DICTIONARY_WORD_BUTTON},
    {"customDictionaryWords", IDS_SETTINGS_LANGUAGES_DICTIONARY_WORDS},
    {"noCustomDictionaryWordsFound",
     IDS_SETTINGS_LANGUAGES_DICTIONARY_WORDS_NONE},
    {"languagesDictionaryDownloadError",
     IDS_SETTINGS_LANGUAGES_DICTIONARY_DOWNLOAD_FAILED},
    {"languagesDictionaryDownloadErrorHelp",
     IDS_SETTINGS_LANGUAGES_DICTIONARY_DOWNLOAD_FAILED_HELP},
#endif
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Only the Chrome OS help article explains how language order affects website
  // language.
  html_source->AddString(
      "languagesLearnMoreURL",
      base::ASCIIToUTF16(chrome::kLanguageSettingsLearnMoreUrl));
  html_source->AddString(
      "languagesPageTitle",
      l10n_util::GetStringUTF16(IDS_SETTINGS_LANGUAGES_PAGE_TITLE));
#else
  html_source->AddString(
      "languagesPageTitle",
      l10n_util::GetStringUTF16(IDS_SETTINGS_LANGUAGES_PAGE_TITLE));
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  const user_manager::User* primary_user = user_manager->GetPrimaryUser();
  html_source->AddBoolean(
      "isSecondaryUser",
      user && user->GetAccountId() != primary_user->GetAccountId());

  html_source->AddString(
      "openChromeOSLanguagesSettingsLabel",
      l10n_util::GetStringUTF16(
          IDS_SETTINGS_LANGUAGES_OPEN_CHROME_OS_SETTINGS_LABEL));
  html_source->AddString(
      "chromeOSLanguagesSettingsPath",
      chromeos::settings::mojom::kLanguagesAndInputSectionPath);
  // TODO(crbug.com/1097328): Delete this.
  html_source->AddBoolean("isChromeOSLanguagesSettingsUpdate", true);

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
void AddChromeOSSettingsStrings(content::WebUIDataSource* html_source) {
  html_source->AddString(
      "osSettingsBannerText",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_OS_SETTINGS_BANNER,
          base::ASCIIToUTF16(chrome::kChromeUIOSSettingsURL)));
}
#endif

void AddOnStartupStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"onStartup", IDS_SETTINGS_ON_STARTUP},
      {"onStartupOpenNewTab", IDS_SETTINGS_ON_STARTUP_OPEN_NEW_TAB},
      {"onStartupContinue", IDS_SETTINGS_ON_STARTUP_CONTINUE},
      {"onStartupOpenSpecific", IDS_SETTINGS_ON_STARTUP_OPEN_SPECIFIC},
      {"onStartupUseCurrent", IDS_SETTINGS_ON_STARTUP_USE_CURRENT},
      {"onStartupAddNewPage", IDS_SETTINGS_ON_STARTUP_ADD_NEW_PAGE},
      {"onStartupEditPage", IDS_SETTINGS_ON_STARTUP_EDIT_PAGE},
      {"onStartupSiteUrl", IDS_SETTINGS_ON_STARTUP_SITE_URL},
      {"onStartupRemove", IDS_SETTINGS_ON_STARTUP_REMOVE},
      {"onStartupInvalidUrl", IDS_SETTINGS_INVALID_URL},
      {"onStartupUrlTooLong", IDS_SETTINGS_URL_TOOL_LONG},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

bool IsFidoAuthenticationAvailable(autofill::PersonalDataManager* personal_data,
                                   content::WebContents* web_contents) {
  // Don't show toggle switch if user is unable to downstream cards.
  if (personal_data->GetSyncSigninState() !=
          autofill::AutofillSyncSigninState::
              kSignedInAndWalletSyncTransportEnabled &&
      personal_data->GetSyncSigninState() !=
          autofill::AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled) {
    return false;
  }

  // If |autofill_manager| is not available, then don't show toggle switch.
  autofill::ContentAutofillDriverFactory* autofill_driver_factory =
      autofill::ContentAutofillDriverFactory::FromWebContents(web_contents);
  if (!autofill_driver_factory)
    return false;
  autofill::ContentAutofillDriver* autofill_driver =
      autofill_driver_factory->DriverForFrame(web_contents->GetMainFrame());
  if (!autofill_driver)
    return false;
  autofill::AutofillManager* autofill_manager =
      autofill_driver->autofill_manager();
  if (!autofill_manager)
    return false;

  // Show the toggle switch only if the flag is enabled. Once returned, this
  // decision may be overridden (from true to false) by the caller in the
  // payments section if no platform authenticator is found.
  return base::FeatureList::IsEnabled(
      autofill::features::kAutofillCreditCardAuthentication);
}

void AddAutofillStrings(content::WebUIDataSource* html_source,
                        Profile* profile,
                        content::WebContents* web_contents) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"autofillPageTitle", IDS_SETTINGS_AUTOFILL},
      {"passwords", IDS_SETTINGS_PASSWORDS},
      {"passwordsDevice", IDS_SETTINGS_DEVICE_PASSWORDS},
      {"checkPasswords", IDS_SETTINGS_CHECK_PASSWORDS},
      {"checkPasswordsCanceled", IDS_SETTINGS_CHECK_PASSWORDS_CANCELED},
      {"checkedPasswords", IDS_SETTINGS_CHECKED_PASSWORDS},
      {"checkPasswordsDescription", IDS_SETTINGS_CHECK_PASSWORDS_DESCRIPTION},
      {"checkPasswordsErrorOffline",
       IDS_SETTINGS_CHECK_PASSWORDS_ERROR_OFFLINE},
      {"checkPasswordsErrorSignedOut",
       IDS_SETTINGS_CHECK_PASSWORDS_ERROR_SIGNED_OUT},
      {"checkPasswordsErrorNoPasswords",
       IDS_SETTINGS_CHECK_PASSWORDS_ERROR_NO_PASSWORDS},
      {"checkPasswordsErrorQuota",
       IDS_SETTINGS_CHECK_PASSWORDS_ERROR_QUOTA_LIMIT},
      {"checkPasswordsErrorGeneric",
       IDS_SETTINGS_CHECK_PASSWORDS_ERROR_GENERIC},
      {"noCompromisedCredentials",
       IDS_SETTINGS_NO_COMPROMISED_CREDENTIALS_LABEL},
      {"checkPasswordsAgain", IDS_SETTINGS_CHECK_PASSWORDS_AGAIN},
      {"checkPasswordsAgainAfterError",
       IDS_SETTINGS_CHECK_PASSWORDS_AGAIN_AFTER_ERROR},
      {"checkPasswordsProgress", IDS_SETTINGS_CHECK_PASSWORDS_PROGRESS},
      {"checkPasswordsStop", IDS_SETTINGS_CHECK_PASSWORDS_STOP},
      {"compromisedPasswords", IDS_SETTINGS_COMPROMISED_PASSWORDS},
      {"compromisedPasswordsDescription",
       IDS_SETTINGS_COMPROMISED_PASSWORDS_ADVICE},
      {"weakPasswords", IDS_SETTINGS_WEAK_PASSWORDS},
      {"changePasswordButton", IDS_SETTINGS_CHANGE_PASSWORD_BUTTON},
      {"changePasswordInApp", IDS_SETTINGS_CHANGE_PASSWORD_IN_APP_LABEL},
      {"leakedPassword", IDS_SETTINGS_COMPROMISED_PASSWORD_REASON_LEAKED},
      {"phishedPassword", IDS_SETTINGS_COMPROMISED_PASSWORD_REASON_PHISHED},
      {"phishedAndLeakedPassword",
       IDS_SETTINGS_COMPROMISED_PASSWORD_REASON_PHISHED_AND_LEAKED},
      {"showCompromisedPassword", IDS_SETTINGS_COMPROMISED_PASSWORD_SHOW},
      {"hideCompromisedPassword", IDS_SETTINGS_COMPROMISED_PASSWORD_HIDE},
      {"removeCompromisedPassword", IDS_SETTINGS_COMPROMISED_PASSWORD_REMOVE},
      {"removeCompromisedPasswordConfirmationTitle",
       IDS_SETTINGS_REMOVE_COMPROMISED_PASSWORD_CONFIRMATION_TITLE},
      {"removeCompromisedPasswordConfirmationDescription",
       IDS_SETTINGS_REMOVE_COMPROMISED_PASSWORD_CONFIRMATION_DESCRIPTION},
      {"editCompromisedPasswordSite",
       IDS_SETTINGS_COMPROMISED_EDIT_PASSWORD_SITE},
      {"editCompromisedPasswordApp",
       IDS_SETTINGS_COMPROMISED_EDIT_PASSWORD_APP},
      {"alreadyChangedPasswordLink",
       IDS_SETTINGS_COMPROMISED_ALREADY_CHANGED_PASSWORD},
      {"editDisclaimerTitle", IDS_SETTINGS_COMPROMISED_EDIT_DISCLAIMER_TITLE},
      {"editDisclaimerDescription",
       IDS_SETTINGS_COMPROMISED_EDIT_DISCLAIMER_DESCRIPTION},
      {"creditCards", IDS_AUTOFILL_PAYMENT_METHODS},
      {"noPaymentMethodsFound", IDS_SETTINGS_PAYMENT_METHODS_NONE},
      {"googlePayments", IDS_SETTINGS_GOOGLE_PAYMENTS},
      {"googlePaymentsCached", IDS_SETTINGS_GOOGLE_PAYMENTS_CACHED},
      {"enableProfilesLabel", IDS_AUTOFILL_ENABLE_PROFILES_TOGGLE_LABEL},
      {"enableProfilesSublabel", IDS_AUTOFILL_ENABLE_PROFILES_TOGGLE_SUBLABEL},
      {"enableCreditCardsLabel", IDS_AUTOFILL_ENABLE_CREDIT_CARDS_TOGGLE_LABEL},
      {"enableCreditCardsSublabel",
       IDS_AUTOFILL_ENABLE_CREDIT_CARDS_TOGGLE_SUBLABEL},
      {"enableCreditCardFIDOAuthLabel", IDS_ENABLE_CREDIT_CARD_FIDO_AUTH_LABEL},
      {"enableCreditCardFIDOAuthSublabel",
       IDS_ENABLE_CREDIT_CARD_FIDO_AUTH_SUBLABEL},
      {"addresses", IDS_AUTOFILL_ADDRESSES},
      {"addressesTitle", IDS_AUTOFILL_ADDRESSES_SETTINGS_TITLE},
      {"addAddressTitle", IDS_SETTINGS_AUTOFILL_ADDRESSES_ADD_TITLE},
      {"editAddressTitle", IDS_SETTINGS_AUTOFILL_ADDRESSES_EDIT_TITLE},
      {"addressCountry", IDS_SETTINGS_AUTOFILL_ADDRESSES_COUNTRY},
      {"addressPhone", IDS_SETTINGS_AUTOFILL_ADDRESSES_PHONE},
      {"addressEmail", IDS_SETTINGS_AUTOFILL_ADDRESSES_EMAIL},
      {"honorificLabel", IDS_SETTINGS_AUTOFILL_ADDRESS_HONORIFIC_LABEL},
      {"removeAddress", IDS_SETTINGS_ADDRESS_REMOVE},
      {"removeAddressConfirmationTitle",
       IDS_SETTINGS_ADDRESS_REMOVE_CONFIRMATION_TITLE},
      {"removeAddressConfirmationDescription",
       IDS_SETTINGS_ADDRESS_REMOVE_CONFIRMATION_DESCRIPTION},
      {"removeCreditCard", IDS_SETTINGS_CREDIT_CARD_REMOVE},
      {"clearCreditCard", IDS_SETTINGS_CREDIT_CARD_CLEAR},
      {"creditCardType", IDS_SETTINGS_AUTOFILL_CREDIT_CARD_TYPE_COLUMN_LABEL},
      {"creditCardExpiration", IDS_SETTINGS_CREDIT_CARD_EXPIRATION_DATE},
      {"creditCardName", IDS_SETTINGS_NAME_ON_CREDIT_CARD},
      {"creditCardNickname", IDS_SETTINGS_CREDIT_CARD_NICKNAME},
      {"creditCardNicknameInvalid", IDS_SETTINGS_CREDIT_CARD_NICKNAME_INVALID},
      {"creditCardNumber", IDS_SETTINGS_CREDIT_CARD_NUMBER},
      {"creditCardExpirationMonth", IDS_SETTINGS_CREDIT_CARD_EXPIRATION_MONTH},
      {"creditCardExpirationYear", IDS_SETTINGS_CREDIT_CARD_EXPIRATION_YEAR},
      {"creditCardExpired", IDS_SETTINGS_CREDIT_CARD_EXPIRED},
      {"editCreditCardTitle", IDS_SETTINGS_EDIT_CREDIT_CARD_TITLE},
      {"addCreditCardTitle", IDS_SETTINGS_ADD_CREDIT_CARD_TITLE},
      {"migrateCreditCardsLabel", IDS_SETTINGS_MIGRATABLE_CARDS_LABEL},
      {"migratableCardsInfoSingle", IDS_SETTINGS_SINGLE_MIGRATABLE_CARD_INFO},
      {"migratableCardsInfoMultiple",
       IDS_SETTINGS_MULTIPLE_MIGRATABLE_CARDS_INFO},
      {"remoteCreditCardLinkLabel", IDS_SETTINGS_REMOTE_CREDIT_CARD_LINK_LABEL},
      {"upiIdLabel", IDS_SETTINGS_UPI_ID_LABEL},
      {"upiIdExpirationNever", IDS_SETTINGS_UPI_ID_EXPIRATION_NEVER},
      {"canMakePaymentToggleLabel", IDS_SETTINGS_CAN_MAKE_PAYMENT_TOGGLE_LABEL},
      {"autofillDetail", IDS_SETTINGS_AUTOFILL_DETAIL},
      {"passwordsSavePasswordsLabel",
       IDS_SETTINGS_PASSWORDS_SAVE_PASSWORDS_TOGGLE_LABEL},
      {"passwordsAutosigninLabel",
       IDS_SETTINGS_PASSWORDS_AUTOSIGNIN_CHECKBOX_LABEL},
      {"passwordsAutosigninDescription",
       IDS_SETTINGS_PASSWORDS_AUTOSIGNIN_CHECKBOX_DESC},
      {"passwordsLeakDetectionLabel",
       IDS_SETTINGS_PASSWORDS_LEAK_DETECTION_LABEL},
      {"passwordsLeakDetectionGeneralDescription",
       IDS_PASSWORD_MANAGER_LEAK_HELP_MESSAGE},
      {"passwordsLeakDetectionSignedOutEnabledDescription",
       IDS_SETTINGS_PASSWORDS_LEAK_DETECTION_SIGNED_OUT_ENABLED_DESC},
      {"savedPasswordsHeading", IDS_SETTINGS_PASSWORDS_SAVED_HEADING},
      {"passwordExceptionsHeading", IDS_SETTINGS_PASSWORDS_EXCEPTIONS_HEADING},
      {"deviceOnlyPasswordsHeading",
       IDS_SETTINGS_DEVICE_PASSWORDS_ON_DEVICE_ONLY_HEADING},
      {"deviceAndAccountPasswordsHeading",
       IDS_SETTINGS_DEVICE_PASSWORDS_ON_DEVICE_AND_ACCOUNT_HEADING},
      {"deletePasswordException", IDS_SETTINGS_PASSWORDS_DELETE_EXCEPTION},
      {"removePassword", IDS_SETTINGS_PASSWORD_REMOVE},
      {"searchPasswords", IDS_SETTINGS_PASSWORD_SEARCH},
      {"showPassword", IDS_SETTINGS_PASSWORD_SHOW},
      {"hidePassword", IDS_SETTINGS_PASSWORD_HIDE},
      {"passwordDetailsTitle", IDS_SETTINGS_PASSWORDS_VIEW_DETAILS_TITLE},
      {"passwordViewDetails", IDS_SETTINGS_PASSWORD_DETAILS},
      {"editPasswordTitle", IDS_SETTINGS_PASSWORD_EDIT_TITLE},
      {"editPassword", IDS_SETTINGS_PASSWORD_EDIT},
      {"editPasswordFootnote", IDS_SETTINGS_PASSWORD_EDIT_FOOTNOTE},
      {"usernameAlreadyUsed", IDS_SETTINGS_PASSWORD_USERNAME_ALREADY_USED},
      {"copyPassword", IDS_SETTINGS_PASSWORD_COPY},
      {"passwordStoredOnDevice", IDS_SETTINGS_PASSWORD_STORED_ON_DEVICE},
      {"passwordStoredInAccount", IDS_SETTINGS_PASSWORD_STORED_IN_ACCOUNT},
      {"passwordStoredInAccountAndOnDevice",
       IDS_SETTINGS_PASSWORD_STORED_IN_ACCOUNT_AND_ON_DEVICE},
      {"editPasswordWebsiteLabel", IDS_SETTINGS_PASSWORDS_WEBSITE},
      {"editPasswordUsernameLabel", IDS_SETTINGS_PASSWORDS_USERNAME},
      {"editPasswordPasswordLabel", IDS_SETTINGS_PASSWORDS_PASSWORD},
      {"noAddressesFound", IDS_SETTINGS_ADDRESS_NONE},
      {"noPasswordsFound", IDS_SETTINGS_PASSWORDS_NONE},
      {"noExceptionsFound", IDS_SETTINGS_PASSWORDS_EXCEPTIONS_NONE},
      {"import", IDS_PASSWORD_MANAGER_IMPORT_BUTTON},
      {"exportMenuItem", IDS_SETTINGS_PASSWORDS_EXPORT_MENU_ITEM},
      {"optInAccountStorageLabel",
       IDS_SETTINGS_PASSWORDS_OPT_IN_ACCOUNT_STORAGE_LABEL},
      {"optOutAccountStorageLabel",
       IDS_SETTINGS_PASSWORDS_OPT_OUT_ACCOUNT_STORAGE_LABEL},
      {"undoRemovePassword", IDS_SETTINGS_PASSWORD_UNDO},
      {"movePasswordToAccount", IDS_SETTINGS_PASSWORD_MOVE_TO_ACCOUNT},
      {"passwordDeleted", IDS_SETTINGS_PASSWORD_DELETED_PASSWORD},
      {"passwordDeletedFromDevice",
       IDS_SETTINGS_PASSWORD_DELETED_PASSWORD_FROM_DEVICE},
      {"passwordDeletedFromAccount",
       IDS_SETTINGS_PASSWORD_DELETED_PASSWORD_FROM_ACCOUNT},
      {"passwordDeletedFromAccountAndDevice",
       IDS_SETTINGS_PASSWORD_DELETED_PASSWORD_FROM_ACCOUNT_AND_DEVICE},
      {"passwordMovePasswordsToAccount",
       IDS_SETTINGS_PASSWORD_MOVE_PASSWORDS_TO_ACCOUNT},
      {"passwordMovePasswordsToAccountDialogBodyText",
       IDS_SETTINGS_PASSWORD_MOVE_PASSWORDS_TO_ACCOUNT_DIALOG_BODY_TEXT},
      {"passwordMovePasswordsToAccountDialogTitle",
       IDS_SETTINGS_PASSWORD_MOVE_PASSWORDS_TO_ACCOUNT_DIALOG_TITLE},
      {"passwordMoveToAccountDialogTitle",
       IDS_SETTINGS_PASSWORD_MOVE_TO_ACCOUNT_DIALOG_TITLE},
      {"passwordMoveToAccountDialogBody",
       IDS_SETTINGS_PASSWORD_MOVE_TO_ACCOUNT_DIALOG_BODY},
      {"passwordMoveMultiplePasswordsToAccountDialogMoveButtonText",
       IDS_SETTINGS_PASSWORD_MOVE_MULTIPLE_PASSWORDS_TO_ACCOUNT_DIALOG_MOVE_BUTTON_TEXT},
      {"passwordMoveMultiplePasswordsToAccountDialogCancelButtonText",
       IDS_SETTINGS_PASSWORD_MOVE_MULTIPLE_PASSWORDS_TO_ACCOUNT_DIALOG_CANCEL_BUTTON_TEXT},
      {"passwordMoveToAccountDialogMoveButtonText",
       IDS_SETTINGS_PASSWORD_MOVE_TO_ACCOUNT_DIALOG_MOVE_BUTTON_TEXT},
      {"passwordMoveToAccountDialogCancelButtonText",
       IDS_SETTINGS_PASSWORD_MOVE_TO_ACCOUNT_DIALOG_CANCEL_BUTTON_TEXT},
      {"passwordOpenMoveMultiplePasswordsToAccountDialogButtonText",
       IDS_SETTINGS_PASSWORD_OPEN_MOVE_MULTIPLE_PASSWORDS_TO_ACCOUNT_DIALOG_BUTTON_TEXT},
      {"passwordRemoveDialogTitle", IDS_SETTINGS_PASSWORD_REMOVE_DIALOG_TITLE},
      {"passwordRemoveDialogBody", IDS_SETTINGS_PASSWORD_REMOVE_DIALOG_BODY},
      {"passwordRemoveDialogRemoveButtonText",
       IDS_SETTINGS_PASSWORD_REMOVE_DIALOG_REMOVE_BUTTON_TEXT},
      {"passwordRemoveDialogCancelButtonText",
       IDS_SETTINGS_PASSWORD_REMOVE_DIALOG_CANCEL_BUTTON_TEXT},
      {"passwordRemoveDialogFromAccountCheckboxLabel",
       IDS_SETTINGS_PASSWORD_REMOVE_DIALOG_FROM_ACCOUNT_CHECKBOX_LABEL},
      {"passwordRemoveDialogFromDeviceCheckboxLabel",
       IDS_SETTINGS_PASSWORD_REMOVE_DIALOG_FROM_DEVICE_CHECKBOX_LABEL},
      {"devicePasswordsLinkLabel", IDS_SETTINGS_DEVICE_PASSWORDS_LINK_LABEL},
      {"devicePasswordsMoved",
       IDS_SETTINGS_PASSWORD_MOVE_PASSWORDS_TO_ACCOUNT_SNACKBAR},
      {"passwordRowMoreActionsButton", IDS_SETTINGS_PASSWORD_ROW_MORE_ACTIONS},
      {"passwordRowFederatedMoreActionsButton",
       IDS_SETTINGS_PASSWORD_ROW_FEDERATED_MORE_ACTIONS},
      {"exportPasswordsTitle", IDS_SETTINGS_PASSWORDS_EXPORT_TITLE},
      {"exportPasswordsDescription", IDS_SETTINGS_PASSWORDS_EXPORT_DESCRIPTION},
      {"exportPasswords", IDS_SETTINGS_PASSWORDS_EXPORT},
      {"exportingPasswordsTitle", IDS_SETTINGS_PASSWORDS_EXPORTING_TITLE},
      {"exportPasswordsTryAgain", IDS_SETTINGS_PASSWORDS_EXPORT_TRY_AGAIN},
      {"exportPasswordsFailTitle",
       IDS_SETTINGS_PASSWORDS_EXPORTING_FAILURE_TITLE},
      {"exportPasswordsFailTips",
       IDS_SETTINGS_PASSWORDS_EXPORTING_FAILURE_TIPS},
      {"exportPasswordsFailTipsEnoughSpace",
       IDS_SETTINGS_PASSWORDS_EXPORTING_FAILURE_TIP_ENOUGH_SPACE},
      {"exportPasswordsFailTipsAnotherFolder",
       IDS_SETTINGS_PASSWORDS_EXPORTING_FAILURE_TIP_ANOTHER_FOLDER},
      {"managePasswordsPlaintext",
       IDS_SETTINGS_PASSWORDS_MANAGE_PASSWORDS_PLAINTEXT},
      {"savedToThisDeviceOnly",
       IDS_SETTINGS_PAYMENTS_SAVED_TO_THIS_DEVICE_ONLY}};

  GURL google_password_manager_url = GetGooglePasswordManagerURL(
      password_manager::ManagePasswordsReferrer::kChromeSettings);

  html_source->AddString(
      "managePasswordsLabel",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_PASSWORDS_MANAGE_PASSWORDS,
          base::UTF8ToUTF16(google_password_manager_url.spec())));
  html_source->AddString(
      "optInAccountStorageBody",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_PASSWORDS_OPT_IN_ACCOUNT_STORAGE_BODY,
          base::UTF8ToUTF16(google_password_manager_url.spec())));
  html_source->AddString(
      "optOutAccountStorageBody",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_PASSWORDS_OPT_OUT_ACCOUNT_STORAGE_BODY,
          base::UTF8ToUTF16(google_password_manager_url.spec())));
  html_source->AddString(
      "checkPasswordsErrorQuotaGoogleAccount",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CHECK_PASSWORDS_ERROR_QUOTA_LIMIT_GOOGLE_ACCOUNT,
          base::UTF8ToUTF16(
              password_manager::GetPasswordCheckupURL(
                  password_manager::PasswordCheckupReferrer::kPasswordCheck)
                  .spec())));
  html_source->AddString("googlePasswordManagerUrl",
                         google_password_manager_url.spec());
  html_source->AddString("passwordCheckLearnMoreURL",
                         chrome::kPasswordCheckLearnMoreURL);
  html_source->AddString("passwordManagerLearnMoreURL",
                         chrome::kPasswordManagerLearnMoreURL);
  html_source->AddString("manageAddressesUrl",
                         autofill::payments::GetManageAddressesUrl().spec());
  html_source->AddString("manageCreditCardsLabel",
                         l10n_util::GetStringFUTF16(
                             IDS_SETTINGS_PAYMENTS_MANAGE_CREDIT_CARDS,
                             base::UTF8ToUTF16(chrome::kPaymentMethodsURL)));
  html_source->AddString("manageCreditCardsUrl",
                         autofill::payments::GetManageInstrumentsUrl().spec());
  html_source->AddString("paymentMethodsLearnMoreURL",
                         chrome::kPaymentMethodsLearnMoreURL);
  html_source->AddString(
      "weakPasswordsDescription",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_WEAK_PASSWORDS_DESCRIPTION,
          base::ASCIIToUTF16(chrome::kSeeMoreSecurityTipsURL)));
  html_source->AddString(
      "weakPasswordsDescriptionGeneration",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_WEAK_PASSWORDS_DESCRIPTION_GENERATION,
          base::ASCIIToUTF16(chrome::kPasswordGenerationLearnMoreURL)));
  html_source->AddString("signedOutUserLabel",
                         l10n_util::GetStringFUTF16(
                             IDS_SETTINGS_SIGNED_OUT_USER_LABEL,
                             base::ASCIIToUTF16(chrome::kSyncLearnMoreURL)));
  html_source->AddString(
      "signedOutUserHasCompromisedCredentialsLabel",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_SIGNED_OUT_USER_HAS_COMPROMISED_CREDENTIALS_LABEL,
          base::ASCIIToUTF16(chrome::kSyncLearnMoreURL)));

  bool is_guest_mode = false;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  is_guest_mode = user_manager::UserManager::Get()->IsLoggedInAsGuest() ||
                  user_manager::UserManager::Get()->IsLoggedInAsPublicAccount();
#else   // !BUILDFLAG(IS_CHROMEOS_ASH)
  is_guest_mode = profile->IsOffTheRecord();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(profile);
  html_source->AddBoolean(
      "migrationEnabled",
      !is_guest_mode && autofill::IsCreditCardMigrationEnabled(
                            personal_data, profile->GetPrefs(),
                            ProfileSyncServiceFactory::GetForProfile(profile),
                            /*is_test_mode=*/false,
                            /*log_manager=*/nullptr));
  html_source->AddBoolean(
      "fidoAuthenticationAvailableForAutofill",
      IsFidoAuthenticationAvailable(personal_data, web_contents));

  ui::Accelerator undo_accelerator(ui::VKEY_Z, ui::EF_PLATFORM_ACCELERATOR);
  html_source->AddString(
      "undoDescription",
      l10n_util::GetStringFUTF16(IDS_UNDO_DESCRIPTION,
                                 undo_accelerator.GetShortcutText()));

  html_source->AddBoolean("showUpiIdSettings",
                          base::FeatureList::IsEnabled(
                              autofill::features::kAutofillSaveAndFillVPA));

  html_source->AddBoolean(
      "showHonorific",
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableSupportForHonorificPrefixes));

  html_source->AddLocalizedStrings(kLocalizedStrings);
}

void AddSignOutDialogStrings(content::WebUIDataSource* html_source,
                             Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool is_dice_enabled = false;
  bool use_browser_sync_consent =
      chromeos::features::ShouldUseBrowserSyncConsent();
#else
  bool is_dice_enabled =
      AccountConsistencyModeManager::IsDiceEnabledForProfile(profile);
  bool use_browser_sync_consent = false;
#endif

  if (use_browser_sync_consent || is_dice_enabled) {
    static constexpr webui::LocalizedString kTurnOffStrings[] = {
        {"syncDisconnect", IDS_SETTINGS_PEOPLE_SYNC_TURN_OFF},
        {"syncDisconnectTitle",
         IDS_SETTINGS_TURN_OFF_SYNC_AND_SIGN_OUT_DIALOG_TITLE},
    };
    html_source->AddLocalizedStrings(kTurnOffStrings);
  } else {
    static constexpr webui::LocalizedString kSignOutStrings[] = {
        {"syncDisconnect", IDS_SETTINGS_PEOPLE_SIGN_OUT},
        {"syncDisconnectTitle", IDS_SETTINGS_SYNC_DISCONNECT_TITLE},
    };
    html_source->AddLocalizedStrings(kSignOutStrings);
  }

  std::string sync_dashboard_url =
      google_util::AppendGoogleLocaleParam(
          GURL(chrome::kSyncGoogleDashboardURL),
          g_browser_process->GetApplicationLocale())
          .spec();

  if (is_dice_enabled) {
    static constexpr webui::LocalizedString kSyncDisconnectStrings[] = {
        {"syncDisconnectDeleteProfile",
         IDS_SETTINGS_TURN_OFF_SYNC_DIALOG_CHECKBOX},
        {"syncDisconnectConfirm",
         IDS_SETTINGS_TURN_OFF_SYNC_DIALOG_MANAGED_CONFIRM},
        {"syncDisconnectExplanation",
         IDS_SETTINGS_SYNC_DISCONNECT_AND_SIGN_OUT_EXPLANATION},
    };
    html_source->AddLocalizedStrings(kSyncDisconnectStrings);
  } else {
    static constexpr webui::LocalizedString kSyncDisconnectStrings[] = {
        {"syncDisconnectDeleteProfile",
         IDS_SETTINGS_SYNC_DISCONNECT_DELETE_PROFILE},
        {"syncDisconnectConfirm", IDS_SETTINGS_SYNC_DISCONNECT_CONFIRM},
    };
    html_source->AddLocalizedStrings(kSyncDisconnectStrings);

    html_source->AddString(
        "syncDisconnectExplanation",
        l10n_util::GetStringFUTF8(IDS_SETTINGS_SYNC_DISCONNECT_EXPLANATION,
                                  base::ASCIIToUTF16(sync_dashboard_url)));
  }

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  html_source->AddString(
      "syncDisconnectManagedProfileExplanation",
      l10n_util::GetStringFUTF8(
          IDS_SETTINGS_SYNC_DISCONNECT_MANAGED_PROFILE_EXPLANATION, u"$1",
          base::ASCIIToUTF16(sync_dashboard_url)));
#endif
}

void AddPeopleStrings(content::WebUIDataSource* html_source, Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    // Top level people strings:
    {"peopleSignInPromptSecondaryWithAccount",
     IDS_SETTINGS_PEOPLE_SIGN_IN_PROMPT_SECONDARY_WITH_ACCOUNT},
    {"peopleSignInPromptSecondaryWithNoAccount",
     IDS_SETTINGS_PEOPLE_SIGN_IN_PROMPT_SECONDARY_WITH_ACCOUNT},
    {"peoplePageTitle", IDS_SETTINGS_PEOPLE},
    {"syncSettingsSavedToast", IDS_SETTINGS_SYNC_SETTINGS_SAVED_TOAST_LABEL},
    {"peopleSignInPrompt", IDS_SETTINGS_PEOPLE_SIGN_IN_PROMPT},
    {"manageGoogleAccount", IDS_SETTINGS_MANAGE_GOOGLE_ACCOUNT},
    {"syncAndNonPersonalizedServices",
     IDS_SETTINGS_SYNC_SYNC_AND_NON_PERSONALIZED_SERVICES},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"accountManagerSubMenuLabel", IDS_SETTINGS_ACCOUNT_MANAGER_SUBMENU_LABEL},
#else
    {"editPerson", IDS_SETTINGS_CUSTOMIZE_PROFILE},
    {"profileNameAndPicture", IDS_SETTINGS_CUSTOMIZE_YOUR_CHROME_PROFILE},
#endif

  // Manage profile strings:
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    {"showShortcutLabel", IDS_SETTINGS_PROFILE_SHORTCUT_TOGGLE_LABEL},
    {"nameInputLabel", IDS_SETTINGS_PROFILE_NAME_INPUT_LABEL},
    {"nameYourProfile", IDS_SETTING_NAME_YOUR_PROFILE},
    {"pickThemeColor", IDS_SETTINGS_PICK_A_THEME_COLOR},
    {"pickAvatar", IDS_SETTINGS_PICK_AN_AVATAR},
    {"createShortcutTitle", IDS_SETTINGS_CREATE_SHORTCUT},
    {"createShortcutSubtitle", IDS_SETTINGS_CREATE_SHORTCUT_SUBTITLE},

    // Color picker strings:
    {"colorPickerLabel", IDS_NTP_CUSTOMIZE_COLOR_PICKER_LABEL},
    {"defaultThemeLabel", IDS_NTP_CUSTOMIZE_DEFAULT_LABEL},
    {"thirdPartyThemeDescription", IDS_NTP_CUSTOMIZE_3PT_THEME_DESC},
    {"uninstallThirdPartyThemeButton", IDS_NTP_CUSTOMIZE_3PT_THEME_UNINSTALL},

    // Managed theme dialog strings:
    {"themeManagedDialogTitle", IDS_NTP_THEME_MANAGED_DIALOG_TITLE},
    {"themeManagedDialogBody", IDS_NTP_THEME_MANAGED_DIALOG_BODY},
#endif
    {"deleteProfileWarningExpandA11yLabel",
     IDS_SETTINGS_SYNC_DISCONNECT_EXPAND_ACCESSIBILITY_LABEL},
    {"deleteProfileWarningWithCountsSingular",
     IDS_SETTINGS_SYNC_DISCONNECT_DELETE_PROFILE_WARNING_WITH_COUNTS_SINGULAR},
    {"deleteProfileWarningWithCountsPlural",
     IDS_SETTINGS_SYNC_DISCONNECT_DELETE_PROFILE_WARNING_WITH_COUNTS_PLURAL},
    {"deleteProfileWarningWithoutCounts",
     IDS_SETTINGS_SYNC_DISCONNECT_DELETE_PROFILE_WARNING_WITHOUT_COUNTS},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  // Add Google Account URL and include UTM parameter to signal the source of
  // the navigation.
  html_source->AddString(
      "googleAccountUrl",
      net::AppendQueryParameter(GURL(chrome::kGoogleAccountURL), "utm_source",
                                "chrome-settings")
          .spec());
  html_source->AddBoolean("profileShortcutsEnabled",
                          ProfileShortcutManager::IsFeatureEnabled());

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  html_source->AddBoolean("signinAvailable",
                          AccountConsistencyModeManager::IsDiceSignInAllowed());
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Toggles the Chrome OS Account Manager submenu in the People section.
  html_source->AddBoolean("isAccountManagerEnabled",
                          ash::IsAccountManagerAvailable(profile));
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  html_source->AddBoolean("isAccountManagerEnabled",
                          IsAccountManagerAvailable(profile));
#endif

  AddSignOutDialogStrings(html_source, profile);
  AddSyncControlsStrings(html_source);
  AddSyncAccountControlStrings(html_source);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  AddPasswordPromptDialogStrings(html_source);
#endif
  AddSyncPageStrings(html_source);
}

void AddPrivacyStrings(content::WebUIDataSource* html_source,
                       Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"privacyPageTitle", IDS_SETTINGS_PRIVACY},
      {"privacyPageMore", IDS_SETTINGS_PRIVACY_MORE},
      {"doNotTrack", IDS_SETTINGS_ENABLE_DO_NOT_TRACK},
      {"doNotTrackDialogTitle", IDS_SETTINGS_ENABLE_DO_NOT_TRACK_DIALOG_TITLE},
      // TODO(crbug.com/1062607): This string is no longer used. Remove.
      {"permissionsPageTitle", IDS_SETTINGS_PERMISSIONS},
      {"permissionsPageDescription", IDS_SETTINGS_PERMISSIONS_DESCRIPTION},
      {"securityPageTitle", IDS_SETTINGS_SECURITY},
      {"securityPageDescription", IDS_SETTINGS_SECURITY_DESCRIPTION},
      {"advancedProtectionProgramTitle",
       IDS_SETTINGS_ADVANCED_PROTECTION_PROGRAM},
      {"advancedProtectionProgramDesc",
       IDS_SETTINGS_ADVANCED_PROTECTION_PROGRAM_DESC},
      {"manageCertificates", IDS_SETTINGS_MANAGE_CERTIFICATES},
      {"manageCertificatesDescription",
       IDS_SETTINGS_MANAGE_CERTIFICATES_DESCRIPTION},
      {"secureDns", IDS_SETTINGS_SECURE_DNS},
      {"secureDnsDescription", IDS_SETTINGS_SECURE_DNS_DESCRIPTION},
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
      {"secureDnsCustomPlaceholder",
       IDS_SETTINGS_SECURE_DNS_CUSTOM_PLACEHOLDER},
      {"secureDnsCustomFormatError",
       IDS_SETTINGS_SECURE_DNS_CUSTOM_FORMAT_ERROR},
      {"secureDnsCustomConnectionError",
       IDS_SETTINGS_SECURE_DNS_CUSTOM_CONNECTION_ERROR},
      {"contentSettings", IDS_SETTINGS_CONTENT_SETTINGS},
      {"siteSettings", IDS_SETTINGS_SITE_SETTINGS},
      {"siteSettingsDescription", IDS_SETTINGS_SITE_SETTINGS_DESCRIPTION},
      {"clearData", IDS_SETTINGS_CLEAR_DATA},
      {"clearingData", IDS_SETTINGS_CLEARING_DATA},
      {"clearedData", IDS_SETTINGS_CLEARED_DATA},
      {"clearBrowsingData", IDS_SETTINGS_CLEAR_BROWSING_DATA},
      {"clearBrowsingDataDescription", IDS_SETTINGS_CLEAR_DATA_DESCRIPTION},
      {"titleAndCount", IDS_SETTINGS_TITLE_AND_COUNT},
      {"safeBrowsingEnableExtendedReporting",
       IDS_SETTINGS_SAFEBROWSING_ENABLE_REPORTING},
      {"safeBrowsingEnableExtendedReportingDesc",
       IDS_SETTINGS_SAFEBROWSING_ENABLE_REPORTING_DESC},
      {"safeBrowsingEnhanced", IDS_SETTINGS_SAFEBROWSING_ENHANCED},
      {"safeBrowsingEnhancedDesc", IDS_SETTINGS_SAFEBROWSING_ENHANCED_DESC},
      {"safeBrowsingEnhancedExpandA11yLabel",
       IDS_SETTINGS_SAFEBROWSING_ENHANCED_EXPAND_ACCESSIBILITY_LABEL},
      {"safeBrowsingEnhancedBulOne",
       IDS_SETTINGS_SAFEBROWSING_ENHANCED_BULLET_ONE},
      {"safeBrowsingEnhancedBulTwo",
       IDS_SETTINGS_SAFEBROWSING_ENHANCED_BULLET_TWO},
      {"safeBrowsingEnhancedBulThree",
       IDS_SETTINGS_SAFEBROWSING_ENHANCED_BULLET_THREE},
      {"safeBrowsingEnhancedBulFour",
       IDS_SETTINGS_SAFEBROWSING_ENHANCED_BULLET_FOUR},
      {"safeBrowsingEnhancedBulFive",
       IDS_SETTINGS_SAFEBROWSING_ENHANCED_BULLET_FIVE},
      {"safeBrowsingStandard", IDS_SETTINGS_SAFEBROWSING_STANDARD},
      {"safeBrowsingStandardDesc", IDS_SETTINGS_SAFEBROWSING_STANDARD_DESC},
      {"safeBrowsingStandardExpandA11yLabel",
       IDS_SETTINGS_SAFEBROWSING_STANDARD_EXPAND_ACCESSIBILITY_LABEL},
      {"safeBrowsingStandardBulOne",
       IDS_SETTINGS_SAFEBROWSING_STANDARD_BULLET_ONE},
      {"safeBrowsingStandardBulTwo",
       IDS_SETTINGS_SAFEBROWSING_STANDARD_BULLET_TWO},
      {"safeBrowsingStandardReportingLabel",
       IDS_SETTINGS_SAFEBROWSING_STANDARD_HELP_IMPROVE},
      {"safeBrowsingNone", IDS_SETTINGS_SAFEBROWSING_NONE},
      {"safeBrowsingNoneDesc", IDS_SETTINGS_SAFEBROWSING_NONE_DESC},
      {"safeBrowsingDisableDialog",
       IDS_SETTINGS_SAFEBROWSING_DISABLE_DIALOG_TITLE},
      {"safeBrowsingDisableDialogDesc",
       IDS_SETTINGS_SAFEBROWSING_DISABLE_DIALOG_DESC},
      {"safeBrowsingDisableDialogConfirm",
       IDS_SETTINGS_SAFEBROWSING_DISABLE_DIALOG_CONFIRM},
      {"safeBrowsingEnableProtection",
       IDS_SETTINGS_SAFEBROWSING_ENABLEPROTECTION},
      {"safeBrowsingEnableProtectionDesc",
       IDS_SETTINGS_SAFEBROWSING_ENABLEPROTECTION_DESC},
      {"safeBrowsingSectionLabel", IDS_SETTINGS_SAFEBROWSING_SECTION_LABEL},
      {"syncAndGoogleServicesPrivacyDescription",
       IDS_SETTINGS_SYNC_AND_GOOGLE_SERVICES_PRIVACY_DESC_UNIFIED_CONSENT},
      {"urlKeyedAnonymizedDataCollection",
       IDS_SETTINGS_ENABLE_URL_KEYED_ANONYMIZED_DATA_COLLECTION},
      {"urlKeyedAnonymizedDataCollectionDesc",
       IDS_SETTINGS_ENABLE_URL_KEYED_ANONYMIZED_DATA_COLLECTION_DESC},
      {"noRecentPermissions", IDS_SETTINGS_RECENT_PERMISSIONS_NO_CHANGES},
      {"recentPermissionAllowedOneItem",
       IDS_SETTINGS_RECENT_PERMISSIONS_ALLOWED_ONE_ITEM},
      {"recentPermissionAllowedTwoItems",
       IDS_SETTINGS_RECENT_PERMISSIONS_ALLOWED_TWO_ITEMS},
      {"recentPermissionAllowedMoreThanTwoItems",
       IDS_SETTINGS_RECENT_PERMISSIONS_ALLOWED_MORE_THAN_TWO_ITEMS},
      {"recentPermissionAutoBlockedOneItem",
       IDS_SETTINGS_RECENT_PERMISSIONS_AUTOMATICALLY_BLOCKED_ONE_ITEM},
      {"recentPermissionAutoBlockedTwoItems",
       IDS_SETTINGS_RECENT_PERMISSIONS_AUTOMATICALLY_BLOCKED_TWO_ITEMS},
      {"recentPermissionAutoBlockedMoreThanTwoItems",
       IDS_SETTINGS_RECENT_PERMISSIONS_AUTOMATICALLY_BLOCKED_MORE_THAN_TWO_ITEMS},
      {"recentPermissionBlockedOneItem",
       IDS_SETTINGS_RECENT_PERMISSIONS_BLOCKED_ONE_ITEM},
      {"recentPermissionBlockedTwoItems",
       IDS_SETTINGS_RECENT_PERMISSIONS_BLOCKED_TWO_ITEMS},
      {"recentPermissionBlockedMoreThanTwoItems",
       IDS_SETTINGS_RECENT_PERMISSIONS_BLOCKED_MORE_THAN_TWO_ITEMS},
      {"networkPredictionEnabled",
       IDS_SETTINGS_NETWORK_PREDICTION_ENABLED_LABEL},
      {"networkPredictionEnabledDesc",
       IDS_SETTINGS_NETWORK_PREDICTION_ENABLED_DESC},
      {"networkPredictionEnabledDescCookiesPage",
       IDS_SETTINGS_NETWORK_PREDICTION_ENABLED_DESC_COOKIES_PAGE},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString("cookiesSettingsHelpCenterURL",
                         chrome::kCookiesSettingsHelpCenterURL);

  html_source->AddString("safeBrowsingHelpCenterURL",
                         chrome::kSafeBrowsingHelpCenterURL);

  html_source->AddString("syncAndGoogleServicesLearnMoreURL",
                         chrome::kSyncAndGoogleServicesLearnMoreURL);

  html_source->AddString(
      "doNotTrackDialogMessage",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_ENABLE_DO_NOT_TRACK_DIALOG_TEXT,
          base::ASCIIToUTF16(chrome::kDoNotTrackLearnMoreURL)));
  html_source->AddString(
      "exceptionsLearnMoreURL",
      base::ASCIIToUTF16(chrome::kContentSettingsExceptionsLearnMoreURL));
  html_source->AddBoolean(
      "installedAppsInCbd",
      base::FeatureList::IsEnabled(features::kInstalledAppsInCbd));
  html_source->AddBoolean(
      "driveSuggestAvailable",
      base::FeatureList::IsEnabled(omnibox::kDocumentProvider));
  html_source->AddBoolean("showSecureDnsSetting",
                          features::kDnsOverHttpsShowUiParam.Get());

  // The link to the Advanced Protection Program landing page, with a referrer
  // from Chrome settings.
  GURL advanced_protection_url(
      "https://landing.google.com/advancedprotection/");
  advanced_protection_url = net::AppendQueryParameter(advanced_protection_url,
                                                      "utm_source", "Chrome");
  advanced_protection_url = net::AppendQueryParameter(
      advanced_protection_url, "utm_medium", "ChromeSecuritySettings");
  advanced_protection_url = net::AppendQueryParameter(
      advanced_protection_url, "utm_campaign", "ChromeSettings");
  html_source->AddString("advancedProtectionURL",
                         advanced_protection_url.spec());

  AddPersonalizationOptionsStrings(html_source);
}

void AddPrivacySandboxStrings(content::WebUIDataSource* html_source,
                              Profile* profile) {
  // Strings used outside the privacy sandbox page. The i18n preprocessor might
  // replace those before the corresponding flag value is checked, which is why
  // they are included independently of the flag value.
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"privacySandboxTitle", IDS_SETTINGS_PRIVACY_SANDBOX_TITLE},
      {"privacySandboxTrialsEnabled",
       IDS_SETTINGS_PRIVACY_SANDBOX_TRIALS_ENABLED},
      {"privacySandboxTrialsDisabled",
       IDS_SETTINGS_PRIVACY_SANDBOX_TRIALS_DISABLED},
      {"privacySandboxCookiesDialog",
       IDS_SETTINGS_PRIVACY_SANDBOX_COOKIES_DIALOG},
      {"privacySandboxCookiesDialogMore",
       IDS_SETTINGS_PRIVACY_SANDBOX_COOKIES_DIALOG_MORE},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  // Strings that only need to be available when the flag is enabled.
  if (PrivacySandboxSettingsFactory::GetForProfile(profile)
          ->PrivacySandboxSettingsFunctional()) {
    static constexpr webui::LocalizedString kLocalizedStringsBehindFlag[] = {
        {"privacySandboxPageHeading",
         IDS_SETTINGS_PRIVACY_SANDBOX_PAGE_HEADING},
        {"privacySandboxPageExplanation1",
         IDS_SETTINGS_PRIVACY_SANDBOX_PAGE_EXPLANATION1},
        {"privacySandboxPageExplanation2",
         IDS_SETTINGS_PRIVACY_SANDBOX_PAGE_EXPLANATION2},
        {"privacySandboxPageExplanation3",
         IDS_SETTINGS_PRIVACY_SANDBOX_PAGE_EXPLANATION3},
        {"privacySandboxPageSettingTitle",
         IDS_SETTINGS_PRIVACY_SANDBOX_PAGE_SETTING_TITLE},
        {"privacySandboxPageSettingExplanation1",
         IDS_SETTINGS_PRIVACY_SANDBOX_PAGE_SETTING_EXPLANATION1},
        {"privacySandboxPageSettingExplanation2",
         IDS_SETTINGS_PRIVACY_SANDBOX_PAGE_SETTING_EXPLANATION2},
        {"privacySandboxPageSettingExplanation3",
         IDS_SETTINGS_PRIVACY_SANDBOX_PAGE_SETTING_EXPLANATION3},
        {"privacySandboxPageDetails",
         IDS_SETTINGS_PRIVACY_SANDBOX_PAGE_DETAILS},
    };
    html_source->AddLocalizedStrings(kLocalizedStringsBehindFlag);

    // TODO(crbug/1152336): Solidify the final URL in code once the website is
    // launched.
    html_source->AddString("privacySandboxURL",
                           features::kPrivacySandboxSettingsURL.Get());
    html_source->AddString(
        "privacySandboxPageExplanation4",
        l10n_util::GetStringFUTF16(
            IDS_SETTINGS_PRIVACY_SANDBOX_PAGE_EXPLANATION4,
            base::ASCIIToUTF16(features::kPrivacySandboxSettingsURL.Get())));
  }
}

void AddSafetyCheckStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"safetyCheckSectionTitle", IDS_SETTINGS_SAFETY_CHECK_SECTION_TITLE},
    {"safetyCheckParentPrimaryLabelBefore",
     IDS_SETTINGS_SAFETY_CHECK_PARENT_PRIMARY_LABEL_BEFORE},
    {"safetyCheckRunning", IDS_SETTINGS_SAFETY_CHECK_RUNNING},
    {"safetyCheckParentPrimaryLabelAfter",
     IDS_SETTINGS_SAFETY_CHECK_PARENT_PRIMARY_LABEL_AFTER},
    {"safetyCheckAriaLiveRunning", IDS_SETTINGS_SAFETY_CHECK_ARIA_LIVE_RUNNING},
    {"safetyCheckAriaLiveAfter", IDS_SETTINGS_SAFETY_CHECK_ARIA_LIVE_AFTER},
    {"safetyCheckParentButton", IDS_SETTINGS_SAFETY_CHECK_PARENT_BUTTON},
    {"safetyCheckParentButtonAriaLabel",
     IDS_SETTINGS_SAFETY_CHECK_PARENT_BUTTON_ARIA_LABEL},
    {"safetyCheckParentRunAgainButtonAriaLabel",
     IDS_SETTINGS_SAFETY_CHECK_PARENT_RUN_AGAIN_BUTTON_ARIA_LABEL},
    {"safetyCheckIconRunningAriaLabel",
     IDS_SETTINGS_SAFETY_CHECK_ICON_RUNNING_ARIA_LABEL},
    {"safetyCheckIconSafeAriaLabel",
     IDS_SETTINGS_SAFETY_CHECK_ICON_SAFE_ARIA_LABEL},
    {"safetyCheckIconInfoAriaLabel",
     IDS_SETTINGS_SAFETY_CHECK_ICON_INFO_ARIA_LABEL},
    {"safetyCheckIconWarningAriaLabel",
     IDS_SETTINGS_SAFETY_CHECK_ICON_WARNING_ARIA_LABEL},
    {"safetyCheckReview", IDS_SETTINGS_SAFETY_CHECK_REVIEW},
    {"safetyCheckUpdatesPrimaryLabel",
     IDS_SETTINGS_SAFETY_CHECK_UPDATES_PRIMARY_LABEL},
    {"safetyCheckUpdatesButtonAriaLabel", IDS_UPDATE_RECOMMENDED_DIALOG_TITLE},
    {"safetyCheckPasswordsButtonAriaLabel",
     IDS_SETTINGS_SAFETY_CHECK_PASSWORDS_BUTTON_ARIA_LABEL},
    {"safetyCheckSafeBrowsingButton",
     IDS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_BUTTON},
    {"safetyCheckSafeBrowsingButtonAriaLabel",
     IDS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_BUTTON_ARIA_LABEL},
    {"safetyCheckExtensionsPrimaryLabel",
     IDS_SETTINGS_SAFETY_CHECK_EXTENSIONS_PRIMARY_LABEL},
    {"safetyCheckExtensionsButtonAriaLabel",
     IDS_SETTINGS_SAFETY_CHECK_EXTENSIONS_BUTTON_ARIA_LABEL},
#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {"safetyCheckChromeCleanerPrimaryLabel",
     IDS_SETTINGS_SAFETY_CHECK_CHROME_CLEANER_PRIMARY_LABEL},
    {"safetyCheckChromeCleanerButtonAriaLabel",
     IDS_SETTINGS_SAFETY_CHECK_CHROME_CLEANER_BUTTON_ARIA_LABEL},
#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

void AddSearchInSettingsStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"searchPrompt", IDS_SETTINGS_SEARCH_PROMPT},
      {"searchNoResults", IDS_SEARCH_NO_RESULTS},
      {"searchResults", IDS_SEARCH_RESULTS},
      {"clearSearch", IDS_CLEAR_SEARCH},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  std::u16string help_text = l10n_util::GetStringFUTF16(
      IDS_SETTINGS_SEARCH_NO_RESULTS_HELP,
      base::ASCIIToUTF16(chrome::kSettingsSearchHelpURL));
  html_source->AddString("searchNoResultsHelp", help_text);
}

void AddSearchStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"searchEnginesManage", IDS_SETTINGS_SEARCH_MANAGE_SEARCH_ENGINES},
      {"searchPageTitle", IDS_SETTINGS_SEARCH},
      {"searchExplanation", IDS_SETTINGS_SEARCH_EXPLANATION},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString("searchExplanationLearnMoreURL",
                         base::ASCIIToUTF16(chrome::kOmniboxLearnMoreURL));
}

void AddSearchEnginesStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"searchEnginesPageTitle", IDS_SETTINGS_SEARCH_ENGINES},
      {"searchEnginesAddSearchEngine",
       IDS_SETTINGS_SEARCH_ENGINES_ADD_SEARCH_ENGINE},
      {"searchEnginesEditSearchEngine",
       IDS_SETTINGS_SEARCH_ENGINES_EDIT_SEARCH_ENGINE},
      {"searchEngines", IDS_SETTINGS_SEARCH_ENGINES},
      {"searchEnginesDefault", IDS_SETTINGS_SEARCH_ENGINES_DEFAULT_ENGINES},
      {"searchEnginesOther", IDS_SETTINGS_SEARCH_ENGINES_OTHER_ENGINES},
      {"searchEnginesNoOtherEngines",
       IDS_SETTINGS_SEARCH_ENGINES_NO_OTHER_ENGINES},
      {"searchEnginesExtension", IDS_SETTINGS_SEARCH_ENGINES_EXTENSION_ENGINES},
      {"searchEnginesSearch", IDS_SETTINGS_SEARCH_ENGINES_SEARCH},
      {"searchEnginesSearchEngine", IDS_SETTINGS_SEARCH_ENGINES_SEARCH_ENGINE},
      {"searchEnginesKeyword", IDS_SETTINGS_SEARCH_ENGINES_KEYWORD},
      {"searchEnginesQueryURL", IDS_SETTINGS_SEARCH_ENGINES_QUERY_URL},
      {"searchEnginesQueryURLExplanation",
       IDS_SETTINGS_SEARCH_ENGINES_QUERY_URL_EXPLANATION},
      {"searchEnginesMakeDefault", IDS_SETTINGS_SEARCH_ENGINES_MAKE_DEFAULT},
      {"searchEnginesRemoveFromList",
       IDS_SETTINGS_SEARCH_ENGINES_REMOVE_FROM_LIST},
      {"searchEnginesManageExtension",
       IDS_SETTINGS_SEARCH_ENGINES_MANAGE_EXTENSION},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

void AddSiteSettingsStrings(content::WebUIDataSource* html_source,
                            Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"addSite", IDS_SETTINGS_ADD_SITE},
    {"addSiteTitle", IDS_SETTINGS_ADD_SITE_TITLE},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"androidSmsNote", IDS_SETTINGS_ANDROID_SMS_NOTE},
#endif
    {"appCacheOrigin", IDS_SETTINGS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL},
    {"cookieAppCache", IDS_SETTINGS_COOKIES_APPLICATION_CACHE},
    {"cookieCacheStorage", IDS_SETTINGS_COOKIES_CACHE_STORAGE},
    {"cookieDatabaseStorage", IDS_SETTINGS_COOKIES_DATABASE_STORAGE},
    {"cookieFileSystem", IDS_SETTINGS_COOKIES_FILE_SYSTEM},
    {"cookieFlashLso", IDS_SETTINGS_COOKIES_FLASH_LSO},
    {"cookieLocalStorage", IDS_SETTINGS_COOKIES_LOCAL_STORAGE},
    {"cookieMediaLicense", IDS_SETTINGS_COOKIES_MEDIA_LICENSE},
    {"cookieServiceWorker", IDS_SETTINGS_COOKIES_SERVICE_WORKER},
    {"cookieSharedWorker", IDS_SETTINGS_COOKIES_SHARED_WORKER},
    {"embeddedOnAnyHost", IDS_SETTINGS_EXCEPTIONS_EMBEDDED_ON_ANY_HOST},
    {"embeddedOnHost", IDS_SETTINGS_EXCEPTIONS_EMBEDDED_ON_HOST},
    {"editSiteTitle", IDS_SETTINGS_EDIT_SITE_TITLE},
    {"appCacheManifest", IDS_SETTINGS_COOKIES_APPLICATION_CACHE_MANIFEST_LABEL},
    {"cacheStorageLastModified",
     IDS_SETTINGS_COOKIES_LOCAL_STORAGE_LAST_MODIFIED_LABEL},
    {"cacheStorageOrigin", IDS_SETTINGS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL},
    {"cacheStorageSize", IDS_SETTINGS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL},
    {"cookieAccessibleToScript",
     IDS_SETTINGS_COOKIES_COOKIE_ACCESSIBLE_TO_SCRIPT_LABEL},
    {"cookieContent", IDS_SETTINGS_COOKIES_COOKIE_CONTENT_LABEL},
    {"cookieCreated", IDS_SETTINGS_COOKIES_COOKIE_CREATED_LABEL},
    {"cookieDomain", IDS_SETTINGS_COOKIES_COOKIE_DOMAIN_LABEL},
    {"cookieExpires", IDS_SETTINGS_COOKIES_COOKIE_EXPIRES_LABEL},
    {"cookieName", IDS_SETTINGS_COOKIES_COOKIE_NAME_LABEL},
    {"cookiePath", IDS_SETTINGS_COOKIES_COOKIE_PATH_LABEL},
    {"cookieSendFor", IDS_SETTINGS_COOKIES_COOKIE_SENDFOR_LABEL},
    {"databaseOrigin", IDS_SETTINGS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL},
    {"fileSystemOrigin", IDS_SETTINGS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL},
    {"fileSystemPersistentUsage",
     IDS_SETTINGS_COOKIES_FILE_SYSTEM_PERSISTENT_USAGE_LABEL},
    {"fileSystemTemporaryUsage",
     IDS_SETTINGS_COOKIES_FILE_SYSTEM_TEMPORARY_USAGE_LABEL},
    {"indexedDbSize", IDS_SETTINGS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL},
    {"indexedDbLastModified",
     IDS_SETTINGS_COOKIES_LOCAL_STORAGE_LAST_MODIFIED_LABEL},
    {"indexedDbOrigin", IDS_SETTINGS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL},
    {"localStorageLastModified",
     IDS_SETTINGS_COOKIES_LOCAL_STORAGE_LAST_MODIFIED_LABEL},
    {"localStorageOrigin", IDS_SETTINGS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL},
    {"localStorageSize", IDS_SETTINGS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL},
    {"mediaLicenseOrigin", IDS_SETTINGS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL},
    {"mediaLicenseSize", IDS_SETTINGS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL},
    {"mediaLicenseLastModified",
     IDS_SETTINGS_COOKIES_LOCAL_STORAGE_LAST_MODIFIED_LABEL},
    {"noBluetoothDevicesFound", IDS_SETTINGS_NO_BLUETOOTH_DEVICES_FOUND},
    {"noHidDevicesFound", IDS_SETTINGS_NO_HID_DEVICES_FOUND},
    {"noSerialPortsFound", IDS_SETTINGS_NO_SERIAL_PORTS_FOUND},
    {"noUsbDevicesFound", IDS_SETTINGS_NO_USB_DEVICES_FOUND},
    {"serviceWorkerOrigin", IDS_SETTINGS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL},
    {"serviceWorkerSize",
     IDS_SETTINGS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL},
    {"sharedWorkerWorker", IDS_SETTINGS_COOKIES_SHARED_WORKER_WORKER_LABEL},
    {"sharedWorkerName", IDS_SETTINGS_COOKIES_COOKIE_NAME_LABEL},
    {"siteSettingsCategoryPageTitle", IDS_SETTINGS_SITE_SETTINGS_CATEGORY},
    {"siteSettingsRecentPermissionsSectionLabel",
     IDS_SETTINGS_SITE_SETTINGS_RECENT_ACTIVITY},
    {"siteSettingsCategoryCamera", IDS_SETTINGS_SITE_SETTINGS_CAMERA},
    {"siteSettingsCameraLabel", IDS_SETTINGS_SITE_SETTINGS_CAMERA_LABEL},
    {"cookiePageTitle", IDS_SETTINGS_COOKIES_PAGE},
    {"cookiePageGeneralControls", IDS_SETTINGS_COOKIES_CONTROLS},
    {"cookiePageAllowAll", IDS_SETTINGS_COOKIES_ALLOW_ALL},
    {"cookiePageAllowAllBulOne", IDS_SETTINGS_COOKIES_ALLOW_ALL_BULLET_ONE},
    {"cookiePageAllowAllBulTwo", IDS_SETTINGS_COOKIES_ALLOW_ALL_BULLET_TWO},
    {"cookiePageBlockThirdIncognito",
     IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_INCOGNITO},
    {"cookiePageBlockThirdIncognitoBulOne",
     IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_INCOGNITO_BULLET_ONE},
    {"cookiePageBlockThirdIncognitoBulTwo",
     IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_INCOGNITO_BULLET_TWO},
    {"cookiePageBlockThird", IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY},
    {"cookiePageBlockThirdBulOne",
     IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_BULLET_ONE},
    {"cookiePageBlockThirdBulTwo",
     IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_BULLET_TWO},
    {"cookiePageBlockAll", IDS_SETTINGS_COOKIES_BLOCK_ALL},
    {"cookiePageBlockAllBulOne", IDS_SETTINGS_COOKIES_BLOCK_ALL_BULLET_ONE},
    {"cookiePageBlockAllBulTwo", IDS_SETTINGS_COOKIES_BLOCK_ALL_BULLET_TWO},
    {"cookiePageBlockAllBulThree", IDS_SETTINGS_COOKIES_BLOCK_ALL_BULLET_THREE},
    {"cookiePageClearOnExit", IDS_SETTINGS_COOKIES_CLEAR_ON_EXIT},
    {"cookiePageAllowExceptions", IDS_SETTINGS_COOKIES_ALLOW_EXCEPTIONS},
    {"cookiePageBlockExceptions", IDS_SETTINGS_COOKIES_BLOCK_EXCEPTIONS},
    {"cookiePageSessionOnlyExceptions",
     IDS_SETTINGS_COOKIES_SESSION_ONLY_EXCEPTIONS},
    {"cookiesManageSiteSpecificExceptions",
     IDS_SETTINGS_COOKIES_SITE_SPECIFIC_EXCEPTIONS},
    {"siteSettingsCategoryCookies", IDS_SETTINGS_SITE_SETTINGS_COOKIES},
    {"siteSettingsCategoryHandlers", IDS_SETTINGS_SITE_SETTINGS_HANDLERS},
    {"siteSettingsCategoryImages", IDS_SETTINGS_SITE_SETTINGS_IMAGES},
    {"siteSettingsCategoryInsecureContent",
     IDS_SETTINGS_SITE_SETTINGS_INSECURE_CONTENT},
    {"siteSettingsCategoryLocation", IDS_SETTINGS_SITE_SETTINGS_LOCATION},
    {"siteSettingsCategoryJavascript", IDS_SETTINGS_SITE_SETTINGS_JAVASCRIPT},
    {"siteSettingsCategoryMicrophone", IDS_SETTINGS_SITE_SETTINGS_MIC},
    {"siteSettingsMicrophoneLabel", IDS_SETTINGS_SITE_SETTINGS_MIC_LABEL},
    {"siteSettingsCategoryNotifications",
     IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS},
    {"siteSettingsNotificationsAsk",
     IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS_ASK},
    {"siteSettingsNotificationsBlock",
     IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS_BLOCK},
    {"siteSettingsEnableQuietNotificationPrompts",
     IDS_SETTINGS_SITE_SETTINGS_ENABLE_QUIET_NOTIFICATION_PROMPTS},
    {"siteSettingsCategoryPopups", IDS_SETTINGS_SITE_SETTINGS_POPUPS},
    {"siteSettingsCategoryZoomLevels", IDS_SETTINGS_SITE_SETTINGS_ZOOM_LEVELS},
    {"siteSettingsAllSites", IDS_SETTINGS_SITE_SETTINGS_ALL_SITES},
    {"siteSettingsAllSitesDescription",
     IDS_SETTINGS_SITE_SETTINGS_ALL_SITES_DESCRIPTION},
    {"siteSettingsAllSitesSearch", IDS_SETTINGS_SITE_SETTINGS_ALL_SITES_SEARCH},
    {"siteSettingsAllSitesSort", IDS_SETTINGS_SITE_SETTINGS_ALL_SITES_SORT},
    {"siteSettingsAllSitesSortMethodMostVisited",
     IDS_SETTINGS_SITE_SETTINGS_ALL_SITES_SORT_METHOD_MOST_VISITED},
    {"siteSettingsAllSitesSortMethodStorage",
     IDS_SETTINGS_SITE_SETTINGS_ALL_SITES_SORT_METHOD_STORAGE},
    {"siteSettingsAllSitesSortMethodName",
     IDS_SETTINGS_SITE_SETTINGS_ALL_SITES_SORT_METHOD_NAME},
    {"siteSettingsSiteRepresentationSeparator",
     IDS_SETTINGS_SITE_SETTINGS_SITE_REPRESENTATION_SEPARATOR},
    {"siteSettingsAutomaticDownloads",
     IDS_SETTINGS_SITE_SETTINGS_AUTOMATIC_DOWNLOADS},
    {"siteSettingsAutomaticDownloadsMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_AUTOMATIC_DOWNLOADS_MID_SENTENCE},
    {"siteSettingsBackgroundSync", IDS_SETTINGS_SITE_SETTINGS_BACKGROUND_SYNC},
    {"siteSettingsBackgroundSyncMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_BACKGROUND_SYNC_MID_SENTENCE},
    {"siteSettingsCamera", IDS_SETTINGS_SITE_SETTINGS_CAMERA},
    {"siteSettingsCameraMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_CAMERA_MID_SENTENCE},
    {"siteSettingsClipboard", IDS_SETTINGS_SITE_SETTINGS_CLIPBOARD},
    {"siteSettingsClipboardMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_CLIPBOARD_MID_SENTENCE},
    {"siteSettingsClipboardAsk", IDS_SETTINGS_SITE_SETTINGS_CLIPBOARD_ASK},
    {"siteSettingsClipboardAskRecommended",
     IDS_SETTINGS_SITE_SETTINGS_CLIPBOARD_ASK_RECOMMENDED},
    {"siteSettingsClipboardBlock", IDS_SETTINGS_SITE_SETTINGS_CLIPBOARD_BLOCK},
    {"siteSettingsCookies", IDS_SETTINGS_SITE_SETTINGS_COOKIES},
    {"siteSettingsCookiesMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_COOKIES_MID_SENTENCE},
    {"siteSettingsHandlers", IDS_SETTINGS_SITE_SETTINGS_HANDLERS},
    {"siteSettingsHandlersMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_HANDLERS_MID_SENTENCE},
    {"siteSettingsLocation", IDS_SETTINGS_SITE_SETTINGS_LOCATION},
    {"siteSettingsLocationMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_LOCATION_MID_SENTENCE},
    {"siteSettingsMic", IDS_SETTINGS_SITE_SETTINGS_MIC},
    {"siteSettingsMicMidSentence", IDS_SETTINGS_SITE_SETTINGS_MIC_MID_SENTENCE},
    {"siteSettingsNotifications", IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS},
    {"siteSettingsNotificationsMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS_MID_SENTENCE},
    {"siteSettingsImages", IDS_SETTINGS_SITE_SETTINGS_IMAGES},
    {"siteSettingsImagesMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_IMAGES_MID_SENTENCE},
    {"siteSettingsInsecureContent",
     IDS_SETTINGS_SITE_SETTINGS_INSECURE_CONTENT},
    {"siteSettingsInsecureContentMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_INSECURE_CONTENT_MID_SENTENCE},
    {"siteSettingsInsecureContentBlock",
     IDS_SETTINGS_SITE_SETTINGS_INSECURE_CONTENT_BLOCK},
    {"siteSettingsJavascript", IDS_SETTINGS_SITE_SETTINGS_JAVASCRIPT},
    {"siteSettingsJavascriptMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_JAVASCRIPT_MID_SENTENCE},
    {"siteSettingsSound", IDS_SETTINGS_SITE_SETTINGS_SOUND},
    {"siteSettingsSoundMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_SOUND_MID_SENTENCE},
    {"siteSettingsSoundAllow", IDS_SETTINGS_SITE_SETTINGS_SOUND_ALLOW},
    {"siteSettingsSoundAllowRecommended",
     IDS_SETTINGS_SITE_SETTINGS_SOUND_ALLOW_RECOMMENDED},
    {"siteSettingsSoundBlock", IDS_SETTINGS_SITE_SETTINGS_SOUND_BLOCK},
    {"siteSettingsPdfDocuments", IDS_SETTINGS_SITE_SETTINGS_PDF_DOCUMENTS},
    {"siteSettingsPdfDownloadPdfs",
     IDS_SETTINGS_SITE_SETTINGS_PDF_DOWNLOAD_PDFS},
    {"siteSettingsProtectedContent",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT},
    {"siteSettingsProtectedContentMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT_MID_SENTENCE},
    {"siteSettingsProtectedContentIdentifiers",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT_IDENTIFIERS},
    {"siteSettingsProtectedContentEnable",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT_ENABLE},
#if BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_WIN)
    {"siteSettingsProtectedContentIdentifiersExplanation",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT_IDENTIFIERS_EXPLANATION},
    {"siteSettingsProtectedContentEnableIdentifiers",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT_ENABLE_IDENTIFIERS},
#endif
    {"siteSettingsPopups", IDS_SETTINGS_SITE_SETTINGS_POPUPS},
    {"siteSettingsPopupsMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_POPUPS_MID_SENTENCE},
    {"siteSettingsHidDevices", IDS_SETTINGS_SITE_SETTINGS_HID_DEVICES},
    {"siteSettingsHidDevicesMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_HID_DEVICES_MID_SENTENCE},
    {"siteSettingsHidDevicesAsk", IDS_SETTINGS_SITE_SETTINGS_HID_DEVICES_ASK},
    {"siteSettingsHidDevicesAskRecommended",
     IDS_SETTINGS_SITE_SETTINGS_HID_DEVICES_ASK_RECOMMENDED},
    {"siteSettingsHidDevicesBlock",
     IDS_SETTINGS_SITE_SETTINGS_HID_DEVICES_BLOCK},
    {"siteSettingsMidiDevices", IDS_SETTINGS_SITE_SETTINGS_MIDI_DEVICES},
    {"siteSettingsMidiDevicesMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_MIDI_DEVICES_MID_SENTENCE},
    {"siteSettingsMidiDevicesAsk", IDS_SETTINGS_SITE_SETTINGS_MIDI_DEVICES_ASK},
    {"siteSettingsMidiDevicesAskRecommended",
     IDS_SETTINGS_SITE_SETTINGS_MIDI_DEVICES_ASK_RECOMMENDED},
    {"siteSettingsMidiDevicesBlock",
     IDS_SETTINGS_SITE_SETTINGS_MIDI_DEVICES_BLOCK},
    {"siteSettingsSerialPorts", IDS_SETTINGS_SITE_SETTINGS_SERIAL_PORTS},
    {"siteSettingsSerialPortsMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_SERIAL_PORTS_MID_SENTENCE},
    {"siteSettingsSerialPortsAsk", IDS_SETTINGS_SITE_SETTINGS_SERIAL_PORTS_ASK},
    {"siteSettingsSerialPortsAskRecommended",
     IDS_SETTINGS_SITE_SETTINGS_SERIAL_PORTS_ASK_RECOMMENDED},
    {"siteSettingsSerialPortsBlock",
     IDS_SETTINGS_SITE_SETTINGS_SERIAL_PORTS_BLOCK},
    {"siteSettingsUsbDevices", IDS_SETTINGS_SITE_SETTINGS_USB_DEVICES},
    {"siteSettingsUsbDevicesMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_USB_DEVICES_MID_SENTENCE},
    {"siteSettingsUsbDevicesAsk", IDS_SETTINGS_SITE_SETTINGS_USB_DEVICES_ASK},
    {"siteSettingsUsbDevicesAskRecommended",
     IDS_SETTINGS_SITE_SETTINGS_USB_DEVICES_ASK_RECOMMENDED},
    {"siteSettingsUsbDevicesBlock",
     IDS_SETTINGS_SITE_SETTINGS_USB_DEVICES_BLOCK},
    {"siteSettingsBluetoothDevices",
     IDS_SETTINGS_SITE_SETTINGS_BLUETOOTH_DEVICES},
    {"siteSettingsBluetoothDevicesMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_BLUETOOTH_DEVICES_MID_SENTENCE},
    {"siteSettingsBluetoothDevicesAsk",
     IDS_SETTINGS_SITE_SETTINGS_BLUETOOTH_DEVICES_ASK},
    {"siteSettingsBluetoothDevicesAskRecommended",
     IDS_SETTINGS_SITE_SETTINGS_BLUETOOTH_DEVICES_ASK_RECOMMENDED},
    {"siteSettingsBluetoothDevicesBlock",
     IDS_SETTINGS_SITE_SETTINGS_BLUETOOTH_DEVICES_BLOCK},
    {"siteSettingsFileSystemWrite",
     IDS_SETTINGS_SITE_SETTINGS_FILE_SYSTEM_ACCESS_WRITE},
    {"siteSettingsFileSystemWriteMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_FILE_SYSTEM_ACCESS_WRITE_MID_SENTENCE},
    {"siteSettingsFileSystemWriteAsk",
     IDS_SETTINGS_SITE_SETTINGS_FILE_SYSTEM_ACCESS_WRITE_ASK},
    {"siteSettingsFileSystemWriteAskRecommended",
     IDS_SETTINGS_SITE_SETTINGS_FILE_SYSTEM_ACCESS_WRITE_ASK_RECOMMENDED},
    {"siteSettingsFileSystemWriteBlock",
     IDS_SETTINGS_SITE_SETTINGS_FILE_SYSTEM_ACCESS_WRITE_BLOCK},
    {"siteSettingsRemoveZoomLevel",
     IDS_SETTINGS_SITE_SETTINGS_REMOVE_ZOOM_LEVEL},
    {"siteSettingsZoomLevels", IDS_SETTINGS_SITE_SETTINGS_ZOOM_LEVELS},
    {"siteSettingsZoomLevelsMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_ZOOM_LEVELS_MID_SENTENCE},
    {"siteSettingsNoZoomedSites", IDS_SETTINGS_SITE_SETTINGS_NO_ZOOMED_SITES},
    {"siteSettingsMaySaveCookies", IDS_SETTINGS_SITE_SETTINGS_MAY_SAVE_COOKIES},
    {"siteSettingsAskFirst", IDS_SETTINGS_SITE_SETTINGS_ASK_FIRST},
    {"siteSettingsAskFirstRecommended",
     IDS_SETTINGS_SITE_SETTINGS_ASK_FIRST_RECOMMENDED},
    {"siteSettingsAskBeforeAccessing",
     IDS_SETTINGS_SITE_SETTINGS_ASK_BEFORE_ACCESSING},
    {"siteSettingsAskBeforeAccessingRecommended",
     IDS_SETTINGS_SITE_SETTINGS_ASK_BEFORE_ACCESSING_RECOMMENDED},
    {"siteSettingsAskBeforeSending",
     IDS_SETTINGS_SITE_SETTINGS_ASK_BEFORE_SENDING},
    {"siteSettingsAskBeforeSendingRecommended",
     IDS_SETTINGS_SITE_SETTINGS_ASK_BEFORE_SENDING_RECOMMENDED},
    {"siteSettingsAllowRecentlyClosedSites",
     IDS_SETTINGS_SITE_SETTINGS_ALLOW_RECENTLY_CLOSED_SITES},
    {"siteSettingsAllowRecentlyClosedSitesRecommended",
     IDS_SETTINGS_SITE_SETTINGS_ALLOW_RECENTLY_CLOSED_SITES_RECOMMENDED},
    {"siteSettingsBackgroundSyncBlock",
     IDS_SETTINGS_SITE_SETTINGS_BACKGROUND_SYNC_BLOCK},
    {"siteSettingsHandlersAsk", IDS_SETTINGS_SITE_SETTINGS_HANDLERS_ASK},
    {"siteSettingsHandlersAskRecommended",
     IDS_SETTINGS_SITE_SETTINGS_HANDLERS_ASK_RECOMMENDED},
    {"siteSettingsHandlersBlocked",
     IDS_SETTINGS_SITE_SETTINGS_HANDLERS_BLOCKED},
    {"siteSettingsAutoDownloadAsk",
     IDS_SETTINGS_SITE_SETTINGS_AUTOMATIC_DOWNLOAD_ASK},
    {"siteSettingsAutoDownloadAskRecommended",
     IDS_SETTINGS_SITE_SETTINGS_AUTOMATIC_DOWNLOAD_ASK_RECOMMENDED},
    {"siteSettingsAutoDownloadBlock",
     IDS_SETTINGS_SITE_SETTINGS_AUTOMATIC_DOWNLOAD_BLOCK},
    {"siteSettingsDontShowImages", IDS_SETTINGS_SITE_SETTINGS_DONT_SHOW_IMAGES},
    {"siteSettingsShowAll", IDS_SETTINGS_SITE_SETTINGS_SHOW_ALL},
    {"siteSettingsShowAllRecommended",
     IDS_SETTINGS_SITE_SETTINGS_SHOW_ALL_RECOMMENDED},
    {"siteSettingsCookiesAllowed",
     IDS_SETTINGS_SITE_SETTINGS_COOKIES_ALLOW_SITES},
    {"siteSettingsCookiesAllowedRecommended",
     IDS_SETTINGS_SITE_SETTINGS_COOKIES_ALLOW_SITES_RECOMMENDED},
    {"siteSettingsAllow", IDS_SETTINGS_SITE_SETTINGS_ALLOW},
    {"siteSettingsBlock", IDS_SETTINGS_SITE_SETTINGS_BLOCK},
    {"siteSettingsBlockSound", IDS_SETTINGS_SITE_SETTINGS_BLOCK_SOUND},
    {"siteSettingsSessionOnly", IDS_SETTINGS_SITE_SETTINGS_SESSION_ONLY},
    {"siteSettingsAllowed", IDS_SETTINGS_SITE_SETTINGS_ALLOWED},
    {"siteSettingsAllowedRecommended",
     IDS_SETTINGS_SITE_SETTINGS_ALLOWED_RECOMMENDED},
    {"siteSettingsBlocked", IDS_SETTINGS_SITE_SETTINGS_BLOCKED},
    {"siteSettingsBlockedRecommended",
     IDS_SETTINGS_SITE_SETTINGS_BLOCKED_RECOMMENDED},
    {"siteSettingsSiteUrl", IDS_SETTINGS_SITE_SETTINGS_SITE_URL},
    {"siteSettingsActionAskDefault",
     IDS_SETTINGS_SITE_SETTINGS_ASK_DEFAULT_MENU},
    {"siteSettingsActionAllowDefault",
     IDS_SETTINGS_SITE_SETTINGS_ALLOW_DEFAULT_MENU},
    {"siteSettingsActionAutomaticDefault",
     IDS_SETTINGS_SITE_SETTINGS_AUTOMATIC_DEFAULT_MENU},
    {"siteSettingsActionBlockDefault",
     IDS_SETTINGS_SITE_SETTINGS_BLOCK_DEFAULT_MENU},
    {"siteSettingsActionMuteDefault",
     IDS_SETTINGS_SITE_SETTINGS_MUTE_DEFAULT_MENU},
    {"siteSettingsActionAllow", IDS_SETTINGS_SITE_SETTINGS_ALLOW_MENU},
    {"siteSettingsActionBlock", IDS_SETTINGS_SITE_SETTINGS_BLOCK_MENU},
    {"siteSettingsActionAsk", IDS_SETTINGS_SITE_SETTINGS_ASK_MENU},
    {"siteSettingsActionMute", IDS_SETTINGS_SITE_SETTINGS_MUTE_MENU},
    {"siteSettingsActionReset", IDS_SETTINGS_SITE_SETTINGS_RESET_MENU},
    {"siteSettingsActionSessionOnly",
     IDS_SETTINGS_SITE_SETTINGS_SESSION_ONLY_MENU},
    {"siteSettingsUsage", IDS_SETTINGS_SITE_SETTINGS_USAGE},
    {"siteSettingsUsageNone", IDS_SETTINGS_SITE_SETTINGS_USAGE_NONE},
    {"siteSettingsPermissions", IDS_SETTINGS_SITE_SETTINGS_PERMISSIONS},
    {"siteSettingsPermissionsMore",
     IDS_SETTINGS_SITE_SETTINGS_PERMISSIONS_MORE},
    {"siteSettingsContent", IDS_SETTINGS_SITE_SETTINGS_CONTENT},
    {"siteSettingsContentMore", IDS_SETTINGS_SITE_SETTINGS_CONTENT_MORE},
    {"siteSettingsSourceExtensionAllow",
     IDS_PAGE_INFO_PERMISSION_ALLOWED_BY_EXTENSION},
    {"siteSettingsSourceExtensionBlock",
     IDS_PAGE_INFO_PERMISSION_BLOCKED_BY_EXTENSION},
    {"siteSettingsSourceExtensionAsk",
     IDS_PAGE_INFO_PERMISSION_ASK_BY_EXTENSION},
    {"siteSettingsSourcePolicyAllow",
     IDS_PAGE_INFO_PERMISSION_ALLOWED_BY_POLICY},
    {"siteSettingsSourcePolicyBlock",
     IDS_PAGE_INFO_PERMISSION_BLOCKED_BY_POLICY},
    {"siteSettingsSourcePolicyAsk", IDS_PAGE_INFO_PERMISSION_ASK_BY_POLICY},
    {"siteSettingsAdsBlockNotBlocklistedSingular",
     IDS_SETTINGS_SITE_SETTINGS_ADS_BLOCK_NOT_BLOCKLISTED_SINGULAR},
    {"siteSettingsAllowlisted", IDS_SETTINGS_SITE_SETTINGS_ALLOWLISTED},
    {"siteSettingsAdsBlockBlocklistedSingular",
     IDS_SETTINGS_SITE_SETTINGS_ADS_BLOCK_BLOCKLISTED_SINGULAR},
    {"siteSettingsSourceDrmDisabled",
     IDS_SETTINGS_SITE_SETTINGS_SOURCE_DRM_DISABLED},
    {"siteSettingsSourceEmbargo",
     IDS_PAGE_INFO_PERMISSION_AUTOMATICALLY_BLOCKED},
    {"siteSettingsSourceInsecureOrigin",
     IDS_SETTINGS_SITE_SETTINGS_SOURCE_INSECURE_ORIGIN},
    {"siteSettingsSourceKillSwitch",
     IDS_SETTINGS_SITE_SETTINGS_SOURCE_KILL_SWITCH},
    {"siteSettingsReset", IDS_SETTINGS_SITE_SETTINGS_RESET_BUTTON},
    {"siteSettingsCookieHeader", IDS_SETTINGS_SITE_SETTINGS_COOKIE_HEADER},
    {"siteSettingsCookieLink", IDS_SETTINGS_SITE_SETTINGS_COOKIE_LINK},
    {"siteSettingsCookieRemove", IDS_SETTINGS_SITE_SETTINGS_COOKIE_REMOVE},
    {"siteSettingsCookieRemoveAll",
     IDS_SETTINGS_SITE_SETTINGS_COOKIE_REMOVE_ALL},
    {"siteSettingsCookieRemoveAllShown",
     IDS_SETTINGS_SITE_SETTINGS_COOKIE_REMOVE_ALL_SHOWN},
    {"siteSettingsCookieRemoveAllThirdParty",
     IDS_SETTINGS_SITE_SETTINGS_COOKIE_REMOVE_ALL_THIRD_PARTY},
    {"siteSettingsCookieRemoveThirdPartyDialogTitle",
     IDS_SETTINGS_SITE_SETTINGS_THIRD_PARTY_COOKIE_REMOVE_DIALOG_TITLE},
    {"siteSettingsCookieRemoveThirdPartyConfirmation",
     IDS_SETTINGS_SITE_SETTINGS_THIRD_PARTY_COOKIE_REMOVE_CONFIRMATION},
    {"siteSettingsCookiesClearThirdParty",
     IDS_SETTINGS_SITE_SETTINGS_CLEAR_THIRD_PARTY_COOKIES},
    {"siteSettingsCookiesThirdPartyExceptionLabel",
     IDS_SETTINGS_SITE_SETTINGS_THIRD_PARTY_COOKIES_EXCEPTION_LABEL},
    {"siteSettingsCookieRemoveDialogTitle",
     IDS_SETTINGS_SITE_SETTINGS_COOKIE_REMOVE_DIALOG_TITLE},
    {"siteSettingsCookieRemoveMultipleConfirmation",
     IDS_SETTINGS_SITE_SETTINGS_COOKIE_REMOVE_MULTIPLE},
    {"siteSettingsCookieRemoveSite",
     IDS_SETTINGS_SITE_SETTINGS_COOKIE_REMOVE_SITE},
    {"siteSettingsCookiesClearAll",
     IDS_SETTINGS_SITE_SETTINGS_COOKIES_CLEAR_ALL},
    {"siteSettingsCookieSearch", IDS_SETTINGS_SITE_SETTINGS_COOKIE_SEARCH},
    {"siteSettingsCookieSubpage", IDS_SETTINGS_SITE_SETTINGS_COOKIE_SUBPAGE},
    {"siteSettingsDelete", IDS_SETTINGS_SITE_SETTINGS_DELETE},
    {"siteSettingsClearAllStorageDialogTitle",
     IDS_SETTINGS_SITE_SETTINGS_CLEAR_ALL_STORAGE_DIALOG_TITLE},
    {"siteSettingsClearAllStorageDescription",
     IDS_SETTINGS_SITE_SETTINGS_CLEAR_ALL_STORAGE_DESCRIPTION},
    {"siteSettingsClearAllStorageLabel",
     IDS_SETTINGS_SITE_SETTINGS_CLEAR_ALL_STORAGE_LABEL},
    {"siteSettingsClearAllStorageConfirmation",
     IDS_SETTINGS_SITE_SETTINGS_CLEAR_ALL_STORAGE_CONFIRMATION},
    {"siteSettingsClearAllStorageConfirmationInstalled",
     IDS_SETTINGS_SITE_SETTINGS_CLEAR_ALL_STORAGE_CONFIRMATION_INSTALLED},
    {"siteSettingsClearAllStorageSignOut",
     IDS_SETTINGS_SITE_SETTINGS_CLEAR_ALL_STORAGE_SIGN_OUT},
    {"siteSettingsOriginDeleteConfirmation",
     IDS_SETTINGS_SITE_SETTINGS_ORIGIN_DELETE_CONFIRMATION},
    {"siteSettingsOriginDeleteConfirmationInstalled",
     IDS_SETTINGS_SITE_SETTINGS_ORIGIN_DELETE_CONFIRMATION_INSTALLED},
    {"siteSettingsSiteGroupDeleteConfirmationInstalledPlural",
     IDS_SETTINGS_SITE_SETTINGS_SITE_GROUP_DELETE_CONFIRMATION_INSTALLED_PLURAL},
    {"siteSettingsSiteClearStorage",
     IDS_SETTINGS_SITE_SETTINGS_SITE_CLEAR_STORAGE},
    {"siteSettingsSiteClearStorageConfirmation",
     IDS_SETTINGS_SITE_SETTINGS_SITE_CLEAR_STORAGE_CONFIRMATION},
    {"siteSettingsSiteClearStorageConfirmationNew",
     IDS_SETTINGS_SITE_SETTINGS_SITE_CLEAR_STORAGE_CONFIRMATION_NEW},
    {"siteSettingsSiteClearStorageDialogTitle",
     IDS_SETTINGS_SITE_SETTINGS_SITE_CLEAR_STORAGE_DIALOG_TITLE},
    {"siteSettingsSiteClearStorageSignOut",
     IDS_SETTINGS_SITE_SETTINGS_SITE_CLEAR_STORAGE_SIGN_OUT},
    {"siteSettingsSiteClearStorageOfflineData",
     IDS_SETTINGS_SITE_SETTINGS_SITE_CLEAR_STORAGE_OFFLINE_DATA},
    {"siteSettingsSiteClearStorageApps",
     IDS_SETTINGS_SITE_SETTINGS_SITE_CLEAR_STORAGE_APPS},
    {"siteSettingsSiteGroupDelete", IDS_SETTINGS_SITE_SETTINGS_GROUP_DELETE},
    {"siteSettingsSiteGroupDeleteDialogTitle",
     IDS_SETTINGS_SITE_SETTINGS_SITE_GROUP_DELETE_DIALOG_TITLE},
    {"siteSettingsSiteGroupDeleteConfirmation",
     IDS_SETTINGS_SITE_SETTINGS_SITE_GROUP_DELETE_CONFIRMATION},
    {"siteSettingsSiteGroupDeleteConfirmationNew",
     IDS_SETTINGS_SITE_SETTINGS_SITE_GROUP_DELETE_CONFIRMATION_NEW},
    {"siteSettingsSiteGroupDeleteConfirmationInstalled",
     IDS_SETTINGS_SITE_SETTINGS_SITE_GROUP_DELETE_CONFIRMATION_INSTALLED},
    {"siteSettingsSiteGroupDeleteSignOut",
     IDS_SETTINGS_SITE_SETTINGS_SITE_GROUP_DELETE_SIGN_OUT},
    {"siteSettingsSiteGroupDeleteOfflineData",
     IDS_SETTINGS_SITE_SETTINGS_SITE_GROUP_DELETE_OFFLINE_DATA},
    {"siteSettingsSiteGroupDeleteApps",
     IDS_SETTINGS_SITE_SETTINGS_SITE_GROUP_DELETE_APPS},
    {"siteSettingsSiteGroupReset", IDS_SETTINGS_SITE_SETTINGS_GROUP_RESET},
    {"siteSettingsSiteGroupResetDialogTitle",
     IDS_SETTINGS_SITE_SETTINGS_SITE_GROUP_RESET_DIALOG_TITLE},
    {"siteSettingsSiteGroupResetConfirmation",
     IDS_SETTINGS_SITE_SETTINGS_SITE_GROUP_RESET_CONFIRMATION},
    {"siteSettingsSiteResetAll", IDS_SETTINGS_SITE_SETTINGS_SITE_RESET_ALL},
    {"siteSettingsSiteResetConfirmation",
     IDS_SETTINGS_SITE_SETTINGS_SITE_RESET_CONFIRMATION},
    {"thirdPartyCookie", IDS_NEW_TAB_OTR_THIRD_PARTY_COOKIE},
    {"thirdPartyCookieSublabel", IDS_NEW_TAB_OTR_THIRD_PARTY_COOKIE_SUBLABEL},
    {"deleteDataPostSession",
     IDS_SETTINGS_SITE_SETTINGS_DELETE_DATA_POST_SESSION},
    {"handlerIsDefault", IDS_SETTINGS_SITE_SETTINGS_HANDLER_IS_DEFAULT},
    {"handlerSetDefault", IDS_SETTINGS_SITE_SETTINGS_HANDLER_SET_DEFAULT},
    {"handlerRemove", IDS_SETTINGS_SITE_SETTINGS_REMOVE},
    {"incognitoSiteOnly", IDS_SETTINGS_SITE_SETTINGS_INCOGNITO_ONLY},
    {"incognitoSiteExceptionDesc",
     IDS_SETTINGS_SITE_SETTINGS_INCOGNITO_SITE_EXCEPTION_DESC},
    {"noSitesAdded", IDS_SETTINGS_SITE_NO_SITES_ADDED},
    {"siteSettingsDefaultBehavior",
     IDS_SETTINGS_SITE_SETTINGS_DEFAULT_BEHAVIOR},
    {"siteSettingsDefaultBehaviorDescription",
     IDS_SETTINGS_SITE_SETTINGS_DEFAULT_BEHAVIOR_DESCRIPTION},
    {"siteSettingsCustomizedBehaviors",
     IDS_SETTINGS_SITE_SETTINGS_CUSTOMIZED_BEHAVIORS},
    {"siteSettingsCustomizedBehaviorsDescription",
     IDS_SETTINGS_SITE_SETTINGS_CUSTOMIZED_BEHAVIORS_DESCRIPTION},
    {"siteSettingsAdsDescription", IDS_SETTINGS_SITE_SETTINGS_ADS_DESCRIPTION},
    {"siteSettingsAdsAllowed", IDS_SETTINGS_SITE_SETTINGS_ADS_ALLOWED},
    {"siteSettingsAdsBlocked", IDS_SETTINGS_SITE_SETTINGS_ADS_BLOCKED},
    {"siteSettingsAdsAllowedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_ADS_ALLOWED_EXCEPTIONS},
    {"siteSettingsAdsBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_ADS_BLOCKED_EXCEPTIONS},
    {"siteSettingsArDescription", IDS_SETTINGS_SITE_SETTINGS_AR_DESCRIPTION},
    {"siteSettingsArAllowed", IDS_SETTINGS_SITE_SETTINGS_AR_ALLOWED},
    {"siteSettingsArBlocked", IDS_SETTINGS_SITE_SETTINGS_AR_BLOCKED},
    {"siteSettingsArAllowedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_AR_ALLOWED_EXCEPTIONS},
    {"siteSettingsArBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_AR_BLOCKED_EXCEPTIONS},
    {"siteSettingsAutomaticDownloadsDescription",
     IDS_SETTINGS_SITE_SETTINGS_AUTOMATIC_DOWNLOADS_DESCRIPTION},
    {"siteSettingsAutomaticDownloadsAllowed",
     IDS_SETTINGS_SITE_SETTINGS_AUTOMATIC_DOWNLOADS_ALLOWED},
    {"siteSettingsAutomaticDownloadsBlocked",
     IDS_SETTINGS_SITE_SETTINGS_AUTOMATIC_DOWNLOADS_BLOCKED},
    {"siteSettingsAutomaticDownloadsAllowedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_AUTOMATIC_DOWNLOADS_ALLOWED_EXCEPTIONS},
    {"siteSettingsAutomaticDownloadsBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_AUTOMATIC_DOWNLOADS_BLOCKED_EXCEPTIONS},
    {"siteSettingsBackgroundSyncDescription",
     IDS_SETTINGS_SITE_SETTINGS_BACKGROUND_SYNC_DESCRIPTION},
    {"siteSettingsBackgroundSyncAllowed",
     IDS_SETTINGS_SITE_SETTINGS_BACKGROUND_SYNC_ALLOWED},
    {"siteSettingsBackgroundSyncBlocked",
     IDS_SETTINGS_SITE_SETTINGS_BACKGROUND_SYNC_BLOCKED},
    {"siteSettingsBackgroundSyncAllowedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_BACKGROUND_SYNC_ALLOWED_EXCEPTIONS},
    {"siteSettingsBackgroundSyncBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_BACKGROUND_SYNC_BLOCKED_EXCEPTIONS},
    {"siteSettingsBluetoothDevicesDescription",
     IDS_SETTINGS_SITE_SETTINGS_BLUETOOTH_DEVICES_DESCRIPTION},
    {"siteSettingsBluetoothDevicesAllowed",
     IDS_SETTINGS_SITE_SETTINGS_BLUETOOTH_DEVICES_ALLOWED},
    {"siteSettingsBluetoothDevicesBlocked",
     IDS_SETTINGS_SITE_SETTINGS_BLUETOOTH_DEVICES_BLOCKED},
    {"siteSettingsCameraDescription",
     IDS_SETTINGS_SITE_SETTINGS_CAMERA_DESCRIPTION},
    {"siteSettingsCameraAllowed", IDS_SETTINGS_SITE_SETTINGS_CAMERA_ALLOWED},
    {"siteSettingsCameraBlocked", IDS_SETTINGS_SITE_SETTINGS_CAMERA_BLOCKED},
    {"siteSettingsCameraBlockedSubLabel",
     IDS_SETTINGS_SITE_SETTINGS_CAMERA_BLOCKED_SUB_LABEL},
    {"siteSettingsCameraAllowedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_CAMERA_ALLOWED_EXCEPTIONS},
    {"siteSettingsCameraBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_CAMERA_BLOCKED_EXCEPTIONS},
    {"siteSettingsClipboardDescription",
     IDS_SETTINGS_SITE_SETTINGS_CLIPBOARD_DESCRIPTION},
    {"siteSettingsClipboardAllowed",
     IDS_SETTINGS_SITE_SETTINGS_CLIPBOARD_ALLOWED},
    {"siteSettingsClipboardBlocked",
     IDS_SETTINGS_SITE_SETTINGS_CLIPBOARD_BLOCKED},
    {"siteSettingsClipboardAllowedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_CLIPBOARD_ALLOWED_EXCEPTIONS},
    {"siteSettingsClipboardBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_CLIPBOARD_BLOCKED_EXCEPTIONS},
    {"siteSettingsDeviceUseDescription",
     IDS_SETTINGS_SITE_SETTINGS_DEVICE_USE_DESCRIPTION},
    {"siteSettingsDeviceUseAllowed",
     IDS_SETTINGS_SITE_SETTINGS_DEVICE_USE_ALLOWED},
    {"siteSettingsDeviceUseBlocked",
     IDS_SETTINGS_SITE_SETTINGS_DEVICE_USE_BLOCKED},
    {"siteSettingsDeviceUseAllowedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_DEVICE_USE_ALLOWED_EXCEPTIONS},
    {"siteSettingsDeviceUseBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_DEVICE_USE_BLOCKED_EXCEPTIONS},
    {"siteSettingsFileHandlingDescription",
     IDS_SETTINGS_SITE_SETTINGS_FILE_HANDLING_DESCRIPTION},
    {"siteSettingsFileHandlingAllowed",
     IDS_SETTINGS_SITE_SETTINGS_FILE_HANDLING_ALLOWED},
    {"siteSettingsFileHandlingBlocked",
     IDS_SETTINGS_SITE_SETTINGS_FILE_HANDLING_BLOCKED},
    {"siteSettingsFileHandlingAllowedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_FILE_HANDLING_ALLOWED_EXCEPTIONS},
    {"siteSettingsFileHandlingBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_FILE_HANDLING_BLOCKED_EXCEPTIONS},
    {"siteSettingsFileSystemWriteDescription",
     IDS_SETTINGS_SITE_SETTINGS_FILE_SYSTEM_WRITE_DESCRIPTION},
    {"siteSettingsFileSystemWriteAllowed",
     IDS_SETTINGS_SITE_SETTINGS_FILE_SYSTEM_WRITE_ALLOWED},
    {"siteSettingsFileSystemWriteBlocked",
     IDS_SETTINGS_SITE_SETTINGS_FILE_SYSTEM_WRITE_BLOCKED},
    {"siteSettingsFileSystemWriteBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_FILE_SYSTEM_WRITE_BLOCKED_EXCEPTIONS},
    {"siteSettingsFontsDescription",
     IDS_SETTINGS_SITE_SETTINGS_FONTS_DESCRIPTION},
    {"siteSettingsFontsAllowed", IDS_SETTINGS_SITE_SETTINGS_FONTS_ALLOWED},
    {"siteSettingsFontsBlocked", IDS_SETTINGS_SITE_SETTINGS_FONTS_BLOCKED},
    {"siteSettingsFontsAllowedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_FONTS_ALLOWED_EXCEPTIONS},
    {"siteSettingsFontsBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_FONTS_BLOCKED_EXCEPTIONS},
    {"siteSettingsHidDevicesDescription",
     IDS_SETTINGS_SITE_SETTINGS_HID_DEVICES_DESCRIPTION},
    {"siteSettingsHidDevicesAllowed",
     IDS_SETTINGS_SITE_SETTINGS_HID_DEVICES_ALLOWED},
    {"siteSettingsHidDevicesBlocked",
     IDS_SETTINGS_SITE_SETTINGS_HID_DEVICES_BLOCKED},
    {"siteSettingsImagesDescription",
     IDS_SETTINGS_SITE_SETTINGS_IMAGES_DESCRIPTION},
    {"siteSettingsImagesAllowed", IDS_SETTINGS_SITE_SETTINGS_IMAGES_ALLOWED},
    {"siteSettingsImagesBlocked", IDS_SETTINGS_SITE_SETTINGS_IMAGES_BLOCKED},
    {"siteSettingsImagesAllowedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_IMAGES_ALLOWED_EXCEPTIONS},
    {"siteSettingsImagedBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_IMAGES_BLOCKED_EXCEPTIONS},
    {"siteSettingsInsecureContentDescription",
     IDS_SETTINGS_SITE_SETTINGS_INSECURE_CONTENT_DESCRIPTION},
    {"siteSettingsInsecureContentAllowedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_INSECURE_CONTENT_ALLOWED_EXCEPTIONS},
    {"siteSettingsInsecureContentBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_INSECURE_CONTENT_BLOCKED_EXCEPTIONS},
    {"siteSettingsJavascriptDescription",
     IDS_SETTINGS_SITE_SETTINGS_JAVASCRIPT_DESCRIPTION},
    {"siteSettingsJavascriptAllowed",
     IDS_SETTINGS_SITE_SETTINGS_JAVASCRIPT_ALLOWED},
    {"siteSettingsJavascriptBlocked",
     IDS_SETTINGS_SITE_SETTINGS_JAVASCRIPT_BLOCKED},
    {"siteSettingsJavascriptAllowedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_JAVASCRIPT_ALLOWED_EXCEPTIONS},
    {"siteSettingsJavascriptBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_JAVASCRIPT_BLOCKED_EXCEPTIONS},
    {"siteSettingsLocationDescription",
     IDS_SETTINGS_SITE_SETTINGS_LOCATION_DESCRIPTION},
    {"siteSettingsLocationAllowed",
     IDS_SETTINGS_SITE_SETTINGS_LOCATION_ALLOWED},
    {"siteSettingsLocationBlocked",
     IDS_SETTINGS_SITE_SETTINGS_LOCATION_BLOCKED},
    {"siteSettingsLocationBlockedSubLabel",
     IDS_SETTINGS_SITE_SETTINGS_LOCATION_BLOCKED_SUB_LABEL},
    {"siteSettingsLocationAllowedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_LOCATION_ALLOWED_EXCEPTIONS},
    {"siteSettingsLocationBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_LOCATION_BLOCKED_EXCEPTIONS},
    {"siteSettingsMicDescription", IDS_SETTINGS_SITE_SETTINGS_MIC_DESCRIPTION},
    {"siteSettingsMicAllowed", IDS_SETTINGS_SITE_SETTINGS_MIC_ALLOWED},
    {"siteSettingsMicBlocked", IDS_SETTINGS_SITE_SETTINGS_MIC_BLOCKED},
    {"siteSettingsMicBlockedSubLabel",
     IDS_SETTINGS_SITE_SETTINGS_MIC_BLOCKED_SUB_LABEL},
    {"siteSettingsMicAllowedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_MIC_ALLOWED_EXCEPTIONS},
    {"siteSettingsMicBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_MIC_BLOCKED_EXCEPTIONS},
    {"siteSettingsMidiDescription",
     IDS_SETTINGS_SITE_SETTINGS_MIDI_DESCRIPTION},
    {"siteSettingsMidiAllowed", IDS_SETTINGS_SITE_SETTINGS_MIDI_ALLOWED},
    {"siteSettingsMidiBlocked", IDS_SETTINGS_SITE_SETTINGS_MIDI_BLOCKED},
    {"siteSettingsMidiAllowedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_MIDI_ALLOWED_EXCEPTIONS},
    {"siteSettingsMidiBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_MIDI_BLOCKED_EXCEPTIONS},
    {"siteSettingsMotionSensorsDescription",
     IDS_SETTINGS_SITE_SETTINGS_MOTION_SENSORS_DESCRIPTION},
    {"siteSettingsMotionSensorsAllowed",
     IDS_SETTINGS_SITE_SETTINGS_MOTION_SENSORS_ALLOWED},
    {"siteSettingsMotionSensorsBlocked",
     IDS_SETTINGS_SITE_SETTINGS_MOTION_SENSORS_BLOCKED},
    {"siteSettingsMotionSensorsBlockedSubLabel",
     IDS_SETTINGS_SITE_SETTINGS_MOTION_SENSORS_BLOCKED_SUB_LABEL},
    {"siteSettingsMotionSensorsAllowedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_MOTION_SENSORS_ALLOWED_EXCEPTIONS},
    {"siteSettingsMotionSensorsBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_MOTION_SENSORS_BLOCKED_EXCEPTIONS},
    {"siteSettingsNotificationsDescription",
     IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS_DESCRIPTION},
    {"siteSettingsNotificationsAllowed",
     IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS_ALLOWED},
    {"siteSettingsNotificationsPartial",
     IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS_PARTIAL},
    {"siteSettingsNotificationsPartialSubLabel",
     IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS_PARTIAL_SUB_LABEL},
    {"siteSettingsNotificationsBlocked",
     IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS_BLOCKED},
    {"siteSettingsNotificationsBlockedSubLabel",
     IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS_BLOCKED_SUB_LABEL},
    {"siteSettingsNotificationsAllowedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS_ALLOWED_EXCEPTIONS},
    {"siteSettingsNotificationsBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS_BLOCKED_EXCEPTIONS},
    {"siteSettingsPaymentHandlersDescription",
     IDS_SETTINGS_SITE_SETTINGS_PAYMENT_HANDLERS_DESCRIPTION},
    {"siteSettingsPaymentHandlersAllowed",
     IDS_SETTINGS_SITE_SETTINGS_PAYMENT_HANDLERS_ALLOWED},
    {"siteSettingsPaymentHandlersBlocked",
     IDS_SETTINGS_SITE_SETTINGS_PAYMENT_HANDLERS_BLOCKED},
    {"siteSettingsPaymentHandlersAllowedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_PAYMENT_HANDLERS_ALLOWED_EXCEPTIONS},
    {"siteSettingsPaymentHandlersBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_PAYMENT_HANDLERS_BLOCKED_EXCEPTIONS},
    {"siteSettingsPdfsDescription",
     IDS_SETTINGS_SITE_SETTINGS_PDFS_DESCRIPTION},
    {"siteSettingsPdfsAllowed", IDS_SETTINGS_SITE_SETTINGS_PDFS_ALLOWED},
    {"siteSettingsPdfsBlocked", IDS_SETTINGS_SITE_SETTINGS_PDFS_BLOCKED},
    {"siteSettingsPopupsDescription",
     IDS_SETTINGS_SITE_SETTINGS_POPUPS_DESCRIPTION},
    {"siteSettingsPopupsAllowed", IDS_SETTINGS_SITE_SETTINGS_POPUPS_ALLOWED},
    {"siteSettingsPopupsBlocked", IDS_SETTINGS_SITE_SETTINGS_POPUPS_BLOCKED},
    {"siteSettingsPopupsAllowedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_POPUPS_ALLOWED_EXCEPTIONS},
    {"siteSettingsPopupsBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_POPUPS_BLOCKED_EXCEPTIONS},
    {"siteSettingsProtectedContentDescription",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT_DESCRIPTION},
    {"siteSettingsProtectedContentAllowed",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT_ALLOWED},
    {"siteSettingsProtectedContentBlocked",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT_BLOCKED},
    {"siteSettingsProtectedContentAllowedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT_ALLOWED_EXCEPTIONS},
    {"siteSettingsProtectedContentBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT_BLOCKED_EXCEPTIONS},
    {"siteSettingsProtocolHandlersDescription",
     IDS_SETTINGS_SITE_SETTINGS_PROTOCOL_HANDLERS_DESCRIPTION},
    {"siteSettingsProtocolHandlersAllowed",
     IDS_SETTINGS_SITE_SETTINGS_PROTOCOL_HANDLERS_ALLOWED},
    {"siteSettingsProtocolHandlersBlocked",
     IDS_SETTINGS_SITE_SETTINGS_PROTOCOL_HANDLERS_BLOCKED},
    {"siteSettingsProtocolHandlersBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_PROTOCOL_HANDLERS_BLOCKED_EXCEPTIONS},
    {"siteSettingsSerialPortsDescription",
     IDS_SETTINGS_SITE_SETTINGS_SERIAL_PORTS_DESCRIPTION},
    {"siteSettingsSerialPortsAllowed",
     IDS_SETTINGS_SITE_SETTINGS_SERIAL_PORTS_ALLOWED},
    {"siteSettingsSerialPortsBlocked",
     IDS_SETTINGS_SITE_SETTINGS_SERIAL_PORTS_BLOCKED},
    {"siteSettingsSoundDescription",
     IDS_SETTINGS_SITE_SETTINGS_SOUND_DESCRIPTION},
    {"siteSettingsSoundAllowed", IDS_SETTINGS_SITE_SETTINGS_SOUND_ALLOWED},
    {"siteSettingsSoundBlocked", IDS_SETTINGS_SITE_SETTINGS_SOUND_BLOCKED},
    {"siteSettingsSoundAllowedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_SOUND_ALLOWED_EXCEPTIONS},
    {"siteSettingsSoundBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_SOUND_BLOCKED_EXCEPTIONS},
    {"siteSettingsUsbDescription", IDS_SETTINGS_SITE_SETTINGS_USB_DESCRIPTION},
    {"siteSettingsUsbAllowed", IDS_SETTINGS_SITE_SETTINGS_USB_ALLOWED},
    {"siteSettingsUsbBlocked", IDS_SETTINGS_SITE_SETTINGS_USB_BLOCKED},
    {"siteSettingsVrDescription", IDS_SETTINGS_SITE_SETTINGS_VR_DESCRIPTION},
    {"siteSettingsVrAllowed", IDS_SETTINGS_SITE_SETTINGS_VR_ALLOWED},
    {"siteSettingsVrBlocked", IDS_SETTINGS_SITE_SETTINGS_VR_BLOCKED},
    {"siteSettingsVrAllowedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_VR_ALLOWED_EXCEPTIONS},
    {"siteSettingsVrBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_VR_BLOCKED_EXCEPTIONS},
    {"siteSettingsZoomLevelsDescription",
     IDS_SETTINGS_SITE_SETTINGS_ZOOM_LEVELS_DESCRIPTION},
    {"siteSettingsAds", IDS_SETTINGS_SITE_SETTINGS_ADS},
    {"siteSettingsAdsMidSentence", IDS_SETTINGS_SITE_SETTINGS_ADS_MID_SENTENCE},
    {"siteSettingsAdsBlock", IDS_SETTINGS_SITE_SETTINGS_ADS_BLOCK},
    {"siteSettingsAdsBlockRecommended",
     IDS_SETTINGS_SITE_SETTINGS_ADS_BLOCK_RECOMMENDED},
    {"siteSettingsPaymentHandler", IDS_SETTINGS_SITE_SETTINGS_PAYMENT_HANDLER},
    {"siteSettingsPaymentHandlerMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_PAYMENT_HANDLER_MID_SENTENCE},
    {"siteSettingsPaymentHandlerAllow",
     IDS_SETTINGS_SITE_SETTINGS_PAYMENT_HANDLER_ALLOW},
    {"siteSettingsPaymentHandlerAllowRecommended",
     IDS_SETTINGS_SITE_SETTINGS_PAYMENT_HANDLER_ALLOW_RECOMMENDED},
    {"siteSettingsPaymentHandlerBlock",
     IDS_SETTINGS_SITE_SETTINGS_PAYMENT_HANDLER_BLOCK},
    {"siteSettingsBlockAutoplaySetting",
     IDS_SETTINGS_SITE_SETTINGS_BLOCK_AUTOPLAY},
    {"emptyAllSitesPage", IDS_SETTINGS_SITE_SETTINGS_EMPTY_ALL_SITES_PAGE},
    {"noSitesFound", IDS_SETTINGS_SITE_SETTINGS_NO_SITES_FOUND},
    {"siteSettingsBluetoothScanning",
     IDS_SETTINGS_SITE_SETTINGS_BLUETOOTH_SCANNING},
    {"siteSettingsBluetoothScanningMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_BLUETOOTH_SCANNING_MID_SENTENCE},
    {"siteSettingsBluetoothScanningAsk",
     IDS_SETTINGS_SITE_SETTINGS_BLUETOOTH_SCANNING_ASK},
    {"siteSettingsBluetoothScanningAskRecommended",
     IDS_SETTINGS_SITE_SETTINGS_BLUETOOTH_SCANNING_ASK_RECOMMENDED},
    {"siteSettingsBluetoothScanningBlock",
     IDS_SETTINGS_SITE_SETTINGS_BLUETOOTH_SCANNING_BLOCK},
    {"siteSettingsAr", IDS_SETTINGS_SITE_SETTINGS_AR},
    {"siteSettingsArMidSentence", IDS_SETTINGS_SITE_SETTINGS_AR_MID_SENTENCE},
    {"siteSettingsArAsk", IDS_SETTINGS_SITE_SETTINGS_AR_ASK},
    {"siteSettingsArAskRecommended",
     IDS_SETTINGS_SITE_SETTINGS_AR_ASK_RECOMMENDED},
    {"siteSettingsArBlock", IDS_SETTINGS_SITE_SETTINGS_AR_BLOCK},
    {"siteSettingsVr", IDS_SETTINGS_SITE_SETTINGS_VR},
    {"siteSettingsVrMidSentence", IDS_SETTINGS_SITE_SETTINGS_VR_MID_SENTENCE},
    {"siteSettingsVrAsk", IDS_SETTINGS_SITE_SETTINGS_VR_ASK},
    {"siteSettingsVrAskRecommended",
     IDS_SETTINGS_SITE_SETTINGS_VR_ASK_RECOMMENDED},
    {"siteSettingsVrBlock", IDS_SETTINGS_SITE_SETTINGS_VR_BLOCK},
    {"siteSettingsWindowPlacement",
     IDS_SETTINGS_SITE_SETTINGS_WINDOW_PLACEMENT},
    {"siteSettingsWindowPlacementMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_WINDOW_PLACEMENT_MID_SENTENCE},
    {"siteSettingsWindowPlacementAsk",
     IDS_SETTINGS_SITE_SETTINGS_WINDOW_PLACEMENT_ASK},
    {"siteSettingsWindowPlacementAskRecommended",
     IDS_SETTINGS_SITE_SETTINGS_WINDOW_PLACEMENT_ASK_RECOMMENDED},
    {"siteSettingsWindowPlacementBlock",
     IDS_SETTINGS_SITE_SETTINGS_WINDOW_PLACEMENT_BLOCK},
    {"siteSettingsFontAccessMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_FONT_ACCESS_MID_SENTENCE},
    {"siteSettingsFontAccessAsk", IDS_SETTINGS_SITE_SETTINGS_FONT_ACCESS_ASK},
    {"siteSettingsFontAccessBlock",
     IDS_SETTINGS_SITE_SETTINGS_FONT_ACCESS_BLOCK},
    {"siteSettingsIdleDetection", IDS_SETTINGS_SITE_SETTINGS_IDLE_DETECTION},
    {"siteSettingsIdleDetectionMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_IDLE_DETECTION_MID_SENTENCE},
    {"siteSettingsIdleDetectionAsk",
     IDS_SETTINGS_SITE_SETTINGS_IDLE_DETECTION_ASK},
    {"siteSettingsIdleDetectionBlock",
     IDS_SETTINGS_SITE_SETTINGS_IDLE_DETECTION_BLOCK},
    {"siteSettingsFileHandling", IDS_SETTINGS_SITE_SETTINGS_FILE_HANDLING},
    {"siteSettingsFileHandlingMidSentence",
     IDS_SETTINGS_SITE_SETTINGS_FILE_HANDLING_MID_SENTENCE},
    {"siteSettingsFileHandlingAsk",
     IDS_SETTINGS_SITE_SETTINGS_FILE_HANDLING_ASK},
    {"siteSettingsFileHandlingBlock",
     IDS_SETTINGS_SITE_SETTINGS_FILE_HANDLING_BLOCK},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  // These ones cannot be constexpr because we need to check base::FeatureList.
  static webui::LocalizedString kSensorsLocalizedStrings[] = {
      {"siteSettingsSensors",
       base::FeatureList::IsEnabled(features::kGenericSensorExtraClasses)
           ? IDS_SETTINGS_SITE_SETTINGS_SENSORS
           : IDS_SETTINGS_SITE_SETTINGS_MOTION_SENSORS},
      {"siteSettingsSensorsMidSentence",
       base::FeatureList::IsEnabled(features::kGenericSensorExtraClasses)
           ? IDS_SETTINGS_SITE_SETTINGS_SENSORS_MID_SENTENCE
           : IDS_SETTINGS_SITE_SETTINGS_MOTION_SENSORS_MID_SENTENCE},
      {"siteSettingsSensorsAllow",
       base::FeatureList::IsEnabled(features::kGenericSensorExtraClasses)
           ? IDS_SETTINGS_SITE_SETTINGS_SENSORS_ALLOW
           : IDS_SETTINGS_SITE_SETTINGS_MOTION_SENSORS_ALLOW},
      {"siteSettingsSensorsBlock",
       base::FeatureList::IsEnabled(features::kGenericSensorExtraClasses)
           ? IDS_SETTINGS_SITE_SETTINGS_SENSORS_BLOCK
           : IDS_SETTINGS_SITE_SETTINGS_MOTION_SENSORS_BLOCK},
  };
  html_source->AddLocalizedStrings(kSensorsLocalizedStrings);

  html_source->AddBoolean(
      "enableSafeBrowsingSubresourceFilter",
      base::FeatureList::IsEnabled(
          subresource_filter::kSafeBrowsingSubresourceFilter));

  html_source->AddBoolean(
      "enableBlockAutoplayContentSetting",
      base::FeatureList::IsEnabled(media::kAutoplayDisableSettings));

  html_source->AddBoolean(
      "enablePaymentHandlerContentSetting",
      base::FeatureList::IsEnabled(features::kServiceWorkerPaymentApps));

  base::CommandLine& cmd = *base::CommandLine::ForCurrentProcess();
  html_source->AddBoolean(
      "enableExperimentalWebPlatformFeatures",
      cmd.HasSwitch(::switches::kEnableExperimentalWebPlatformFeatures));

  html_source->AddBoolean(
      "enableRemovingAllThirdPartyCookies",
      base::FeatureList::IsEnabled(
          browsing_data::features::kEnableRemovingAllThirdPartyCookies));

  html_source->AddBoolean(
      "enableQuietNotificationPromptsSetting",
      base::FeatureList::IsEnabled(features::kQuietNotificationPrompts));

  html_source->AddBoolean("enableWebBluetoothNewPermissionsBackend",
                          base::FeatureList::IsEnabled(
                              features::kWebBluetoothNewPermissionsBackend));

  html_source->AddBoolean(
      "enableFontAccessContentSetting",
      base::FeatureList::IsEnabled(::blink::features::kFontAccess));

  html_source->AddBoolean(
      "enableFileHandlingContentSetting",
      base::FeatureList::IsEnabled(::blink::features::kFileHandlingAPI));

  // The exception placeholder should not be translated. See crbug.com/1095878.
  html_source->AddString("addSiteExceptionPlaceholder", "[*.]example.com");
}

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)
void AddSystemStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"systemPageTitle", IDS_SETTINGS_SYSTEM},
#if !defined(OS_MAC)
    {"backgroundAppsLabel", IDS_SETTINGS_SYSTEM_BACKGROUND_APPS_LABEL},
#endif
    {"hardwareAccelerationLabel",
     IDS_SETTINGS_SYSTEM_HARDWARE_ACCELERATION_LABEL},
    {"proxySettingsLabel", IDS_SETTINGS_SYSTEM_PROXY_SETTINGS_LABEL},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString(
      "proxySettingsExtensionLabel",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_SYSTEM_PROXY_SETTINGS_EXTENSION_LABEL,
          l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME)));
  html_source->AddString(
      "proxySettingsPolicyLabel",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_SYSTEM_PROXY_SETTINGS_POLICY_LABEL,
          l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME)));

  // TODO(dbeam): we should probably rename anything involving "localized
  // strings" to "load time data" as all primitive types are used now.
  SystemHandler::AddLoadTimeData(html_source);
}
#endif

void AddExtensionsStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("extensionsPageTitle",
                                  IDS_SETTINGS_EXTENSIONS_CHECKBOX_LABEL);
}

void AddSecurityKeysStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kSecurityKeysStrings[] = {
      {"securityKeysBioEnrollmentAddTitle",
       IDS_SETTINGS_SECURITY_KEYS_BIO_ENROLLMENT_ADD_TITLE},
      {"securityKeysBioEnrollmentDelete",
       IDS_SETTINGS_SECURITY_KEYS_BIO_ENROLLMENT_DELETE},
      {"securityKeysBioEnrollmentDialogTitle",
       IDS_SETTINGS_SECURITY_KEYS_BIO_ENROLLMENT_DIALOG_TITLE},
      {"securityKeysBioEnrollmentEnrollingCompleteLabel",
       IDS_SETTINGS_SECURITY_KEYS_BIO_ENROLLMENT_ENROLLING_COMPLETE_LABEL},
      {"securityKeysBioEnrollmentEnrollingLabel",
       IDS_SETTINGS_SECURITY_KEYS_BIO_ENROLLMENT_ENROLLING_LABEL},
      {"securityKeysBioEnrollmentEnrollingFailedLabel",
       IDS_SETTINGS_SECURITY_KEYS_BIO_ENROLLMENT_FAILED_LABEL},
      {"securityKeysBioEnrollmentTryAgainLabel",
       IDS_SETTINGS_SECURITY_KEYS_BIO_ENROLLMENT_TRY_AGAIN_LABEL},
      {"securityKeysBioEnrollmentEnrollmentsLabel",
       IDS_SETTINGS_SECURITY_KEYS_BIO_ENROLLMENT_ENROLLMENTS_LABEL},
      {"securityKeysBioEnrollmentNoEnrollmentsLabel",
       IDS_SETTINGS_SECURITY_KEYS_BIO_ENROLLMENT_NO_ENROLLMENTS_LABEL},
      {"securityKeysBioEnrollmentSubpageDescription",
       IDS_SETTINGS_SECURITY_KEYS_BIO_ENROLLMENT_SUBPAGE_DESCRIPTION},
      {"securityKeysBioEnrollmentSubpageLabel",
       IDS_SETTINGS_SECURITY_KEYS_BIO_ENROLLMENT_SUBPAGE_LABEL},
      {"securityKeysBioEnrollmentChooseName",
       IDS_SETTINGS_SECURITY_KEYS_BIO_CHOOSE_NAME},
      {"securityKeysBioEnrollmentNameLabel",
       IDS_SETTINGS_SECURITY_KEYS_BIO_NAME_LABEL},
      {"securityKeysConfirmPIN", IDS_SETTINGS_SECURITY_KEYS_CONFIRM_PIN},
      {"securityKeysCredentialWebsite",
       IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_WEBSITE},
      {"securityKeysNoCredentialManagement",
       IDS_SETTINGS_SECURITY_KEYS_NO_CREDENTIAL_MANAGEMENT},
      {"securityKeysCredentialManagementRemoved",
       IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_REMOVED},
      {"securityKeysCredentialManagementDesc",
       IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_DESC},
      {"securityKeysCredentialManagementDialogTitle",
       IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_DIALOG_TITLE},
      {"securityKeysCredentialManagementLabel",
       IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_LABEL},
      {"securityKeysCredentialManagementNoCredentials",
       IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_NO_CREDENTIALS},
      {"securityKeysCredentialUsername",
       IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_USERNAME},
      {"securityKeysCurrentPIN", IDS_SETTINGS_SECURITY_KEYS_CURRENT_PIN},
      {"securityKeysCurrentPINIntro",
       IDS_SETTINGS_SECURITY_KEYS_CURRENT_PIN_INTRO},
      {"securityKeysDesc", IDS_SETTINGS_SECURITY_KEYS_DESC},
      {"securityKeysHidePINs", IDS_SETTINGS_SECURITY_KEYS_HIDE_PINS},
      {"securityKeysNewPIN", IDS_SETTINGS_SECURITY_KEYS_NEW_PIN},
      {"securityKeysNoPIN", IDS_SETTINGS_SECURITY_KEYS_NO_PIN},
      {"securityKeysNoReset", IDS_SETTINGS_SECURITY_KEYS_NO_RESET},
      {"securityKeysPIN", IDS_SETTINGS_SECURITY_KEYS_PIN},
      {"securityKeysPINError", IDS_SETTINGS_SECURITY_KEYS_PIN_ERROR},
      {"securityKeysPINHardLock", IDS_SETTINGS_SECURITY_KEYS_PIN_HARD_LOCK},
      {"securityKeysPINIncorrect", IDS_SETTINGS_SECURITY_KEYS_PIN_INCORRECT},
      {"securityKeysPINIncorrectRetriesPl",
       IDS_SETTINGS_SECURITY_KEYS_PIN_INCORRECT_RETRIES_PL},
      {"securityKeysPINIncorrectRetriesSin",
       IDS_SETTINGS_SECURITY_KEYS_PIN_INCORRECT_RETRIES_SIN},
      {"securityKeysPINMismatch",
       IDS_SETTINGS_SECURITY_KEYS_PIN_ERROR_MISMATCH},
      {"securityKeysPINPrompt", IDS_SETTINGS_SECURITY_KEYS_PIN_PROMPT},
      {"securityKeysPINSoftLock", IDS_SETTINGS_SECURITY_KEYS_PIN_SOFT_LOCK},
      {"securityKeysPINSuccess", IDS_SETTINGS_SECURITY_KEYS_PIN_SUCCESS},
      {"securityKeysPINTooLong", IDS_SETTINGS_SECURITY_KEYS_PIN_ERROR_TOO_LONG},
      {"securityKeysPINTooShort",
       IDS_SETTINGS_SECURITY_KEYS_PIN_ERROR_TOO_SHORT_SMALL},
      {"securityKeysReset", IDS_SETTINGS_SECURITY_KEYS_RESET},
      {"securityKeysResetConfirmTitle",
       IDS_SETTINGS_SECURITY_KEYS_RESET_CONFIRM_TITLE},
      {"securityKeysResetDesc", IDS_SETTINGS_SECURITY_KEYS_RESET_DESC},
      {"securityKeysResetError", IDS_SETTINGS_SECURITY_KEYS_RESET_ERROR},
      {"securityKeysResetNotAllowed",
       IDS_SETTINGS_SECURITY_KEYS_RESET_NOTALLOWED},
      {"securityKeysResetStep1", IDS_SETTINGS_SECURITY_KEYS_RESET_STEP1},
      {"securityKeysResetStep2", IDS_SETTINGS_SECURITY_KEYS_RESET_STEP2},
      {"securityKeysResetSuccess", IDS_SETTINGS_SECURITY_KEYS_RESET_SUCCESS},
      {"securityKeysResetTitle", IDS_SETTINGS_SECURITY_KEYS_RESET_TITLE},
      {"securityKeysSetPIN", IDS_SETTINGS_SECURITY_KEYS_SET_PIN},
      {"securityKeysSetPINChangeTitle",
       IDS_SETTINGS_SECURITY_KEYS_SET_PIN_CHANGE_TITLE},
      {"securityKeysSetPINConfirm", IDS_SETTINGS_SECURITY_KEYS_SET_PIN_CONFIRM},
      {"securityKeysSetPINCreateTitle",
       IDS_SETTINGS_SECURITY_KEYS_SET_PIN_CREATE_TITLE},
      {"securityKeysSetPINDesc", IDS_SETTINGS_SECURITY_KEYS_SET_PIN_DESC},
      {"securityKeysSetPINInitialTitle",
       IDS_SETTINGS_SECURITY_KEYS_SET_PIN_INITIAL_TITLE},
      {"securityKeysShowPINs", IDS_SETTINGS_SECURITY_KEYS_SHOW_PINS},
      {"securityKeysTitle", IDS_SETTINGS_SECURITY_KEYS_TITLE},
      {"securityKeysTouchToContinue",
       IDS_SETTINGS_SECURITY_KEYS_TOUCH_TO_CONTINUE},
      {"securityKeysSetPinButton", IDS_SETTINGS_SECURITY_KEYS_SET_PIN_BUTTON},
      {"securityKeysSamePINAsCurrent",
       IDS_SETTINGS_SECURITY_KEYS_SAME_PIN_AS_CURRENT},
  };
  html_source->AddLocalizedStrings(kSecurityKeysStrings);
  bool win_native_api_available = false;
#if defined(OS_WIN)
  win_native_api_available =
      base::FeatureList::IsEnabled(device::kWebAuthUseNativeWinApi) &&
      device::WinWebAuthnApi::GetDefault()->IsAvailable();
#endif
  html_source->AddBoolean("enableSecurityKeysSubpage",
                          !win_native_api_available);
  html_source->AddBoolean("enableSecurityKeysBioEnrollment",
                          !win_native_api_available);
}

}  // namespace

void AddLocalizedStrings(content::WebUIDataSource* html_source,
                         Profile* profile,
                         content::WebContents* web_contents) {
  AddA11yStrings(html_source);
  AddAboutStrings(html_source, profile);
  AddAutofillStrings(html_source, profile, web_contents);
  AddAppearanceStrings(html_source, profile);

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  AddChromeCleanupStrings(html_source);
  AddIncompatibleApplicationsStrings(html_source);
#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

  AddClearBrowsingDataStrings(html_source, profile);
  AddCommonStrings(html_source, profile);
  AddDownloadsStrings(html_source);
  AddLanguagesStrings(html_source, profile);
  AddOnStartupStrings(html_source);
  AddPeopleStrings(html_source, profile);
  AddPrivacySandboxStrings(html_source, profile);
  AddPrivacyStrings(html_source, profile);
  AddSafetyCheckStrings(html_source);
  AddResetStrings(html_source, profile);
  AddSearchEnginesStrings(html_source);
  AddSearchInSettingsStrings(html_source);
  AddSearchStrings(html_source);
  AddSiteSettingsStrings(html_source, profile);

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  AddChromeOSSettingsStrings(html_source);
#else
  AddDefaultBrowserStrings(html_source);
  AddSystemStrings(html_source);
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  AddImportDataStrings(html_source);
#endif
  AddExtensionsStrings(html_source);

#if defined(USE_NSS_CERTS)
  certificate_manager::AddLocalizedStrings(html_source);
#endif

  policy_indicator::AddLocalizedStrings(html_source);
  AddSecurityKeysStrings(html_source);

  html_source->UseStringsJs();
}

}  // namespace settings
