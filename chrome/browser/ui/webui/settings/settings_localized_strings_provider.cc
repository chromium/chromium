// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_localized_strings_provider.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/message_formatter.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_for_testing/buildflags.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/obsolete_system/obsolete_system.h"
#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/preloading/preloading_features.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/account_consistency_mode_manager_factory.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
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
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/browsing_data/core/features.h"
#include "components/content_settings/core/common/features.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/google/core/common/google_util.h"
#include "components/history/core/common/pref_names.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/performance_manager/public/features.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/services/screen_ai/buildflags/buildflags.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/supervised_user/core/common/features.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_utils.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/zoom/page_zoom_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "crypto/crypto_buildflags.h"
#include "device/fido/features.h"
#include "media/base/media_switches.h"
#include "net/base/url_util.h"
#include "net/net_buildflags.h"
#include "services/device/public/cpp/device_features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/strings/grit/ui_strings.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/account_manager/account_manager_util.h"
#include "chrome/browser/ash/assistant/assistant_util.h"
#include "chrome/browser/ash/kerberos/kerberos_credentials_manager.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/common/webui_url_constants.h"
#include "components/user_manager/user_manager.h"
#include "ui/chromeos/devicetype_utils.h"
#else  // !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/settings/system_handler.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#endif

#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS)
#include "ui/display/screen.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "device/fido/win/webauthn_api.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "base/metrics/field_trial_params.h"
#include "base/strings/strcat.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "ui/base/resource/resource_bundle.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(USE_NSS_CERTS)
#include "chrome/browser/ui/webui/certificate_manager_localized_strings_provider.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/linux_ui_factory.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "net/base/features.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
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
    {"manage", IDS_SETTINGS_MANAGE},
    {"menu", IDS_MENU},
    {"menuButtonLabel", IDS_SETTINGS_MENU_BUTTON_LABEL},
    {"moreActions", IDS_SETTINGS_MORE_ACTIONS},
    {"noThanks", IDS_NO_THANKS},
    {"ok", IDS_OK},
    {"opensInNewTab", IDS_SETTINGS_OPENS_IN_NEW_TAB},
    {"sendFeedbackButton", IDS_SETTINGS_SEND_FEEDBACK_ROLE_DESCRIPTION},
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    {"relaunchConfirmationDialogTitle", IDS_RELAUNCH_CONFIRMATION_DIALOG_TITLE},
#endif
    {"remove", IDS_REMOVE},
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
    {"subpageLearnMoreAriaLabel", IDS_SETTINGS_SUBPAGE_LEARN_MORE_ARIA_LABEL},
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
          user_manager::UserManager::Get()->IsLoggedInAsManagedGuestSession());
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
      chromeos::BrowserParamsProxy::Get()->SessionType() ==
              crosapi::mojom::SessionType::kPublicSession ||
          profile->IsGuestSession());
#else
                          profile->IsGuestSession());
#endif

  html_source->AddBoolean("isChildAccount", profile->IsChild());

  html_source->AddBoolean(
      "clearingCookiesKeepsSupervisedUsersSignedIn",
      base::FeatureList::IsEnabled(
          supervised_user::kClearingCookiesKeepsSupervisedUsersSignedIn));

#if BUILDFLAG(IS_LINUX)
  bool allow_qt_theme = base::FeatureList::IsEnabled(ui::kAllowQt);
#else
  bool allow_qt_theme = false;
#endif
  html_source->AddBoolean("allowQtTheme", allow_qt_theme);
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
    {"pdfOcrTitle", IDS_SETTINGS_PDF_OCR_TITLE},
    {"pdfOcrSubtitle", IDS_SETTINGS_PDF_OCR_SUBTITLE},
    {"settingsSliderRoleDescription",
     IDS_SETTINGS_SLIDER_MIN_MAX_ARIA_ROLE_DESCRIPTION},
    {"caretBrowsingTitle", IDS_SETTINGS_ENABLE_CARET_BROWSING_TITLE},
    {"caretBrowsingSubtitle", IDS_SETTINGS_ENABLE_CARET_BROWSING_SUBTITLE},
#if BUILDFLAG(IS_CHROMEOS)
    {"manageAccessibilityFeatures",
     IDS_SETTINGS_ACCESSIBILITY_MANAGE_ACCESSIBILITY_FEATURES},
#else  // !BUILDFLAG(IS_CHROMEOS)
    {"focusHighlightLabel",
     IDS_SETTINGS_ACCESSIBILITY_FOCUS_HIGHLIGHT_DESCRIPTION},
#endif
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

#if BUILDFLAG(IS_WIN)
  html_source->AddBoolean("isWindows10OrNewer", true);
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  html_source->AddBoolean(
      "showFocusHighlightOption",
      base::FeatureList::IsEnabled(features::kAccessibilityFocusHighlight));
#endif
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  html_source->AddBoolean("pdfOcrEnabled",
                          base::FeatureList::IsEnabled(features::kPdfOcr));
#endif

  AddCaptionSubpageStrings(html_source);
}

void AddAboutStrings(content::WebUIDataSource* html_source, Profile* profile) {
  // Top level About Page strings.
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"aboutProductLogoAlt", IDS_SHORT_PRODUCT_LOGO_ALT_TEXT},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {"aboutReportAnIssue", IDS_SETTINGS_ABOUT_PAGE_REPORT_AN_ISSUE},
    {"aboutPrivacyPolicy", IDS_SETTINGS_ABOUT_PAGE_PRIVACY_POLICY},
#endif
    {"aboutRelaunch", IDS_SETTINGS_ABOUT_PAGE_RELAUNCH},
    {"aboutUpgradeCheckStarted", IDS_SETTINGS_ABOUT_UPGRADE_CHECK_STARTED},
    {"aboutUpgradeRelaunch", IDS_SETTINGS_UPGRADE_SUCCESSFUL_RELAUNCH},
    {"aboutUpgradeUpdating", IDS_SETTINGS_UPGRADE_UPDATING},
    {"aboutUpgradeUpdatingPercent", IDS_SETTINGS_UPGRADE_UPDATING_PERCENT},
    {"aboutGetHelpUsingChrome", IDS_SETTINGS_GET_HELP_USING_CHROME},
    {"aboutPageTitle", IDS_SETTINGS_ABOUT_PROGRAM},
    {"aboutProductTitle", IDS_PRODUCT_NAME},
    {"aboutLearnMoreUpdatingErrors",
     IDS_SETTINGS_ABOUT_PAGE_LEARN_MORE_UPDATE_ERRORS},
    {"aboutLearnMoreSystemRequirements",
     IDS_SETTINGS_ABOUT_PAGE_LEARN_MORE_SYSTEM_REQUIREMENTS},
#if BUILDFLAG(IS_MAC)
    {"aboutLearnMoreUpdating", IDS_SETTINGS_ABOUT_PAGE_LEARN_MORE_UPDATING},
#endif
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString(
      "managementPage",
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
      base::FeatureList::IsEnabled(supervised_user::kEnableManagedByParentUi)
          ? chrome::GetDeviceManagedUiHelpLabel(profile)
          : ManagementUI::GetManagementPageSubtitle(profile)
#else
      ManagementUI::GetManagementPageSubtitle(profile)
#endif
  );
  html_source->AddString(
      "aboutUpgradeUpToDate",
#if BUILDFLAG(IS_CHROMEOS_ASH)
      ui::SubstituteChromeOSDeviceType(IDS_SETTINGS_UPGRADE_UP_TO_DATE));
#else
      l10n_util::GetStringUTF16(IDS_SETTINGS_UPGRADE_UP_TO_DATE));
#endif

  std::u16string browser_version = VersionUI::GetAnnotatedVersionStringForUi();

  html_source->AddString("aboutBrowserVersion", browser_version);
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

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) || \
    BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
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
    {"colorSchemeMode", IDS_SETTINGS_COLOR_SCHEME_MODE},
    {"lightMode", IDS_NTP_CUSTOMIZE_CHROME_COLOR_SCHEME_MODE_LIGHT_LABEL},
    {"darkMode", IDS_NTP_CUSTOMIZE_CHROME_COLOR_SCHEME_MODE_DARK_LABEL},
    {"systemMode", IDS_NTP_CUSTOMIZE_CHROME_COLOR_SCHEME_MODE_SYSTEM_LABEL},
    {"showHomeButton", IDS_SETTINGS_SHOW_HOME_BUTTON},
    {"showBookmarksBar", IDS_SETTINGS_SHOW_BOOKMARKS_BAR},
    {"showHoverCardImages", IDS_SETTINGS_SHOW_HOVER_CARD_IMAGES},
    {"sidePanel", IDS_SETTINGS_SIDE_PANEL},
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
    {"mathFont", IDS_SETTINGS_MATH_FONT_LABEL},
    {"minimumFont", IDS_SETTINGS_MINIMUM_FONT_SIZE_LABEL},
    {"tiny", IDS_SETTINGS_TINY_FONT_SIZE},
    {"huge", IDS_SETTINGS_HUGE_FONT_SIZE},
    {"sidePanelAlignLeft", IDS_SETTINGS_SIDE_PANEL_ALIGN_LEFT},
    {"sidePanelAlignRight", IDS_SETTINGS_SIDE_PANEL_ALIGN_RIGHT},
#if BUILDFLAG(IS_LINUX)
    {"gtkTheme", IDS_SETTINGS_GTK_THEME},
    {"useGtkTheme", IDS_SETTINGS_USE_GTK_THEME},
    {"qtTheme", IDS_SETTINGS_QT_THEME},
    {"useQtTheme", IDS_SETTINGS_USE_QT_THEME},
    {"classicTheme", IDS_SETTINGS_CLASSIC_THEME},
    {"useClassicTheme", IDS_SETTINGS_USE_CLASSIC_THEME},
#else
    {"resetToDefaultTheme", IDS_SETTINGS_RESET_TO_DEFAULT_THEME},
#endif
#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS)
    {"showWindowDecorations", IDS_SHOW_WINDOW_DECORATIONS},
#endif
#if BUILDFLAG(IS_MAC)
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
  html_source->AddBoolean("showSidePanelOptions", true);
  html_source->AddBoolean(
      "showHoverCardImagesOption",
      base::FeatureList::IsEnabled(features::kTabHoverCardImageSettings));

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS)
  bool show_custom_chrome_frame = ui::OzonePlatform::GetInstance()
                                      ->GetPlatformRuntimeProperties()
                                      .supports_server_side_window_decorations;
  html_source->AddBoolean("showCustomChromeFrame", show_custom_chrome_frame);
#endif
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
    {"clearBrowsingHistorySummarySignedInNoLink",
     IDS_SETTINGS_CLEAR_BROWSING_HISTORY_SUMMARY_SIGNED_IN_NO_LINK},
    {"clearDownloadHistory", IDS_SETTINGS_CLEAR_DOWNLOAD_HISTORY},
    {"clearCache", IDS_SETTINGS_CLEAR_CACHE},
    {"clearCookies", IDS_SETTINGS_CLEAR_COOKIES},
    {"clearCookiesSummary",
     IDS_SETTINGS_CLEAR_COOKIES_AND_SITE_DATA_SUMMARY_BASIC},
    {"clearCookiesSummarySignedIn",
     IDS_SETTINGS_CLEAR_COOKIES_AND_SITE_DATA_SUMMARY_BASIC_WITH_EXCEPTION},
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    {"clearCookiesSummarySignedInMainProfile",
     IDS_SETTINGS_CLEAR_COOKIES_AND_SITE_DATA_SUMMARY_BASIC_MAIN_PROFILE},
#endif
    {"clearCookiesSummarySignedInSupervisedProfile",
     IDS_SETTINGS_CLEAR_COOKIES_AND_SITE_DATA_SUMMARY_BASIC_SUPERVISED_PROFILE},
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
    {"passwordsDeletionDialogOK", IDS_CLEAR_BROWSING_DATA_PASSWORDS_NOTICE_OK},
    {"notificationWarning", IDS_SETTINGS_NOTIFICATION_WARNING},
  };

  html_source->AddString(
      "clearGoogleSearchHistoryGoogleDse",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CLEAR_GOOGLE_SEARCH_HISTORY_GOOGLE_DSE,
          base::ASCIIToUTF16(chrome::kSearchHistoryUrlInClearBrowsingData),
          base::ASCIIToUTF16(chrome::kMyActivityUrlInClearBrowsingData)));
  html_source->AddString(
      "clearGoogleSearchHistoryNonGoogleDse",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CLEAR_GOOGLE_SEARCH_HISTORY_NON_GOOGLE_DSE,
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

#if !BUILDFLAG(IS_CHROMEOS)
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
      {"showDownloadsWhenFinished", IDS_SETTINGS_DOWNLOADS_SHOW_WHEN_FINISHED},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
void AddGetTheMostOutOfChromeStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"getTheMostOutOfChrome", IDS_SETTINGS_GET_THE_MOST_OUT_OF_CHROME},
      {"getTheMostOutOfChromeDescription",
       IDS_SETTINGS_GET_THE_MOST_OUT_OF_CHROME_DESCRIPTION},
      {"getTheMostOutOfChromeIntro",
       IDS_SETTINGS_GET_THE_MOST_OUT_OF_CHROME_INTRO},
      {"getTheMostOutOfChromeMoreThanABrowser",
       IDS_SETTINGS_GET_THE_MOST_OUT_OF_CHROME_MORE_THAN_A_BROWSER},
      {"getTheMostOutOfChromeYourDataInChrome",
       IDS_SETTINGS_GET_THE_MOST_OUT_OF_CHROME_YOUR_DATA_IN_CHROME},
      {"getTheMostOutOfChromeBeyondCookies",
       IDS_SETTINGS_GET_THE_MOST_OUT_OF_CHROME_BEYOND_COOKIES},
  };

  html_source->AddLocalizedStrings(kLocalizedStrings);
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
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
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

void AddResetStrings(content::WebUIDataSource* html_source, Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"resetPageTitle", IDS_SETTINGS_RESET},
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
      {"resetLearnMoreAccessibilityText",
       IDS_SETTINGS_RESET_LEARN_MORE_ACCESSIBILITY_TEXT},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddBoolean(
      "showResetProfileBanner",
      ResetSettingsHandler::ShouldShowResetProfileBanner(profile));
  bool is_reset_shortcuts_feature_enabled = false;
#if BUILDFLAG(IS_WIN)
  // TODO(crbug/1234599): Remove this flag from the JS.
  is_reset_shortcuts_feature_enabled = true;
#endif
  html_source->AddBoolean("showExplanationWithBulletPoints",
                          is_reset_shortcuts_feature_enabled);

  html_source->AddString("resetPageLearnMoreUrl",
                         chrome::kResetProfileSettingsLearnMoreURL);
  html_source->AddString("resetProfileBannerLearnMoreUrl",
                         chrome::kAutomaticSettingsResetLearnMoreURL);
}

#if !BUILDFLAG(IS_CHROMEOS)
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

void AddPerformanceStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"performancePageTitle", IDS_SETTINGS_PERFORMANCE_PAGE_TITLE},
      {"highEfficiencyModeLabel",
       IDS_SETTINGS_PERFORMANCE_HIGH_EFFICIENCY_MODE_SETTING},
      {"highEfficiencyModeDescription",
       IDS_SETTINGS_PERFORMANCE_HIGH_EFFICIENCY_MODE_SETTING_DESCRIPTION},
      {"highEfficiencyModeHeuristicsLabel",
       IDS_SETTINGS_PERFORMANCE_HIGH_EFFICIENCY_MODE_HEURISTICS_LABEL},
      {"highEfficiencyModeRecommendedBadge",
       IDS_SETTINGS_PERFORMANCE_HIGH_EFFICIENCY_MODE_RECOMMENDED_BADGE},
      {"highEfficiencyModeOnTimerLabel",
       IDS_SETTINGS_PERFORMANCE_HIGH_EFFICIENCY_MODE_ON_TIMER_LABEL},
      {"highEfficiencyModeRadioGroupAriaLabel",
       IDS_SETTINGS_PERFORMANCE_HIGH_EFFICIENCY_MODE_RADIO_GROUP_ARIA_LABEL},
      {"highEfficiencyChooseDiscardTimeAriaLabel",
       IDS_SETTINGS_PERFORMANCE_HIGH_EFFICIENCY_MODE_CHOOSE_DISCARD_TIME_ARIA_LABEL},
      {"batteryPageTitle", IDS_SETTINGS_BATTERY_PAGE_TITLE},
      {"batterySaverModeLabel",
       IDS_SETTINGS_PERFORMANCE_BATTERY_SAVER_MODE_SETTING},
      {"batterySaverModeDescription",
       IDS_SETTINGS_PERFORMANCE_BATTERY_SAVER_MODE_SETTING_DESCRIPTION},
      {"batterySaverModeEnabledOnBatteryLabel",
       IDS_SETTINGS_PERFORMANCE_BATTERY_SAVER_MODE_ON_BATTERY_LABEL},
      {"batterySaverModeRadioGroupAriaLabel",
       IDS_SETTINGS_PERFORMANCE_BATTERY_SAVER_MODE_RADIO_GROUP_ARIA_LABEL},
      {"tabDiscardingExceptionsAddButtonAriaLabel",
       IDS_SETTINGS_PERFORMANCE_TAB_DISCARDING_EXCEPTIONS_ADD_BUTTON_ARIA_LABEL},
      {"tabDiscardingExceptionsSaveButtonAriaLabel",
       IDS_SETTINGS_PERFORMANCE_TAB_DISCARDING_EXCEPTIONS_SAVE_BUTTON_ARIA_LABEL},
      {"tabDiscardingExceptionsHeader",
       IDS_SETTINGS_PERFORMANCE_TAB_DISCARDING_EXCEPTIONS_HEADER},
      {"tabDiscardingExceptionsAdditionalSites",
       IDS_SETTINGS_PERFORMANCE_TAB_DISCARDING_EXCEPTIONS_ADDITIONAL_SITES},
      {"tabDiscardingExceptionsAddDialogCurrentTabs",
       IDS_SETTINGS_PERFORMANCE_TAB_DISCARDING_EXCEPTIONS_ADD_DIALOG_CURRENT_TABS},
      {"tabDiscardingExceptionsAddDialogManual",
       IDS_SETTINGS_PERFORMANCE_TAB_DISCARDING_EXCEPTIONS_ADD_DIALOG_MANUAL},
      {"tabDiscardingExceptionsActiveSiteAriaDescription",
       IDS_SETTINGS_PERFORMANCE_TAB_DISCARDING_EXCEPTIONS_ACTIVE_SITE_ARIA_DESCRIPTION},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddBoolean(
      "highEfficiencyShowRecommendedBadge",
      performance_manager::features::kHighEfficiencyShowRecommendedBadge.Get());

  html_source->AddString(
      "tabDiscardTimerFiveMinutes",
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                             ui::TimeFormat::LENGTH_LONG, base::Minutes(5)));
  html_source->AddString(
      "tabDiscardTimerFifteenMinutes",
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                             ui::TimeFormat::LENGTH_LONG, base::Minutes(15)));
  html_source->AddString(
      "tabDiscardTimerThirtyMinutes",
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                             ui::TimeFormat::LENGTH_LONG, base::Minutes(30)));
  html_source->AddString(
      "tabDiscardTimerOneHour",
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                             ui::TimeFormat::LENGTH_LONG, base::Hours(1)));
  html_source->AddString(
      "tabDiscardTimerTwoHours",
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                             ui::TimeFormat::LENGTH_LONG, base::Hours(2)));
  html_source->AddString(
      "tabDiscardTimerFourHours",
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                             ui::TimeFormat::LENGTH_LONG, base::Hours(4)));
  html_source->AddString(
      "tabDiscardTimerEightHours",
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                             ui::TimeFormat::LENGTH_LONG, base::Hours(8)));
  html_source->AddString(
      "tabDiscardTimerSixteenHours",
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                             ui::TimeFormat::LENGTH_LONG, base::Hours(16)));
  html_source->AddString(
      "tabDiscardTimerTwentyFourHours",
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                             ui::TimeFormat::LENGTH_LONG, base::Hours(24)));

  html_source->AddString(
      "batterySaverModeEnabledBelowThresholdLabel",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_PERFORMANCE_BATTERY_SAVER_MODE_BELOW_THRESHOLD_LABEL,
          base::NumberToString16(
              performance_manager::user_tuning::BatterySaverModeManager::
                  kLowBatteryThresholdPercent)));
  html_source->AddString(
      "tabDiscardingExceptionsAddDialogHelp",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_PERFORMANCE_TAB_DISCARDING_EXCEPTIONS_ADD_DIALOG_HELP,
          base::ASCIIToUTF16(chrome::kHighEfficiencyModeTabDiscardingHelpUrl)));

  html_source->AddString("highEfficiencyLearnMoreUrl",
                         chrome::kHighEfficiencyModeLearnMoreUrl);
  html_source->AddString("batterySaverLearnMoreUrl",
                         chrome::kBatterySaverModeLearnMoreUrl);
}

void AddLanguagesStrings(content::WebUIDataSource* html_source,
                         Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"languagesPageTitle", IDS_SETTINGS_LANGUAGES_PAGE_TITLE},
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    {"languagesCardTitle", IDS_SETTINGS_LANGUAGES_CARD_TITLE},
    {"searchLanguages", IDS_SETTINGS_LANGUAGE_SEARCH},
    {"languagesExpandA11yLabel",
     IDS_SETTINGS_LANGUAGES_EXPAND_ACCESSIBILITY_LABEL},
    {"preferredLanguagesDesc", IDS_SETTINGS_LANGUAGES_PREFERRED_LANGUAGES_DESC},
    {"moveToTop", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_TO_TOP},
    {"moveUp", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_UP},
    {"moveDown", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_DOWN},
    {"removeLanguage", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_REMOVE},
    {"addLanguages", IDS_SETTINGS_LANGUAGES_LANGUAGES_ADD},
    {"addLanguagesDialogTitle", IDS_SETTINGS_LANGUAGES_MANAGE_LANGUAGES_TITLE},
#if BUILDFLAG(IS_WIN)
    {"isDisplayedInThisLanguage",
     IDS_SETTINGS_LANGUAGES_IS_DISPLAYED_IN_THIS_LANGUAGE},
    {"displayInThisLanguage", IDS_SETTINGS_LANGUAGES_DISPLAY_IN_THIS_LANGUAGE},
#endif
    {"offerToEnableTranslate",
     IDS_SETTINGS_LANGUAGES_OFFER_TO_ENABLE_TRANSLATE},
    {"offerToEnableTranslateSublabel",
     IDS_SETTINGS_LANGUAGES_OFFER_TO_ENABLE_TRANSLATE_SUBLABEL},
    {"noLanguagesAdded", IDS_SETTINGS_LANGUAGES_NO_LANGUAGES_ADDED},
    {"addLanguageAriaLabel", IDS_SETTINGS_LANGUAGES_ADD_ARIA_LABEL},
    {"removeAutomaticLanguageAriaLabel",
     IDS_SETTINGS_LANGUAGES_REMOVE_AUTOMATIC_ARIA_LABEL},
    {"removeNeverLanguageAriaLabel",
     IDS_SETTINGS_LANGUAGES_REMOVE_NEVER_ARIA_LABEL},
    {"translatePageTitle", IDS_SETTINGS_TRANSLATE_PAGE_TITLE},
    {"targetLanguageLabel", IDS_SETTINGS_TARGET_TRANSLATE_LABEL},
    {"automaticallyTranslateLanguages",
     IDS_SETTINGS_LANGUAGES_AUTOMATIC_TRANSLATE},
    {"addAutomaticallyTranslateLanguagesAriaLabel",
     IDS_SETTINGS_LANGUAGES_AUTOMATIC_TRANSLATE_ADD_ARIA_LABEL},
    {"neverTranslateLanguages", IDS_SETTINGS_LANGUAGES_NEVER_LANGUAGES},
    {"addNeverTranslateLanguagesAriaLabel",
     IDS_SETTINGS_LANGUAGES_NEVER_TRANSLATE_ADD_ARIA_LABEL},
    {"translateTargetLabel", IDS_SETTINGS_LANGUAGES_TRANSLATE_TARGET},
    {"spellCheckTitle", IDS_SETTINGS_LANGUAGES_SPELL_CHECK_TITLE},
    {"spellCheckBasicLabel", IDS_SETTINGS_LANGUAGES_SPELL_CHECK_BASIC_LABEL},
    {"spellCheckEnhancedLabel",
     IDS_SETTINGS_LANGUAGES_SPELL_CHECK_ENHANCED_LABEL},
    {"spellCheckEnhancedDescription",
     IDS_SETTINGS_LANGUAGES_SPELL_CHECK_ENHANCED_DESCRIPTION},
    {"offerToEnableSpellCheck",
     IDS_SETTINGS_LANGUAGES_OFFER_TO_ENABLE_SPELL_CHECK},
    // Managed dialog strings:
    {"languageManagedDialogTitle", IDS_SETTINGS_LANGUAGES_MANAGED_DIALOG_TITLE},
    {"languageManagedDialogBody", IDS_SETTINGS_LANGUAGES_MANAGED_DIALOG_BODY},
#if !BUILDFLAG(IS_MAC)
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
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"openChromeOSLanguagesSettingsLabel",
     IDS_SETTINGS_LANGUAGES_OPEN_CHROME_OS_SETTINGS_LABEL},
#endif
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  html_source->AddString(
      "chromeOSLanguagesSettingsPath",
      chromeos::settings::mojom::kLanguagesAndInputSectionPath);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void AddOnStartupStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"onStartup", IDS_SETTINGS_ON_STARTUP},
      {"onStartupOpenNewTab", IDS_SETTINGS_ON_STARTUP_OPEN_NEW_TAB},
      {"onStartupContinue", IDS_SETTINGS_ON_STARTUP_CONTINUE},
      {"onStartupOpenSpecific", IDS_SETTINGS_ON_STARTUP_OPEN_SPECIFIC},
      {"onStartupContinueAndOpenSpecific",
       IDS_SETTINGS_ON_STARTUP_CONTINUE_AND_OPEN_SPECIFIC},
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
  if (!personal_data->IsPaymentsDownloadActive()) {
    return false;
  }

  // If |autofill_manager| is not available, then don't show toggle switch.
  autofill::ContentAutofillDriverFactory* autofill_driver_factory =
      autofill::ContentAutofillDriverFactory::FromWebContents(web_contents);
  if (!autofill_driver_factory)
    return false;
  autofill::ContentAutofillDriver* autofill_driver =
      autofill_driver_factory->DriverForFrame(
          web_contents->GetPrimaryMainFrame());
  if (!autofill_driver)
    return false;
  if (!autofill_driver->autofill_manager())
    return false;

  // Show the toggle switch only if FIDO authentication is available. Once
  // returned, this decision may be overridden (from true to false) by the
  // caller in the payments section if no platform authenticator is found.
  return ::autofill::IsCreditCardFidoAuthenticationEnabled();
}

bool CheckDeviceAuthAvailability(content::WebContents* web_contents) {
  // If `client` is not available, then don't show toggle switch.
  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(web_contents);
  if (!client) {
    return false;
  }

  return autofill::IsDeviceAuthAvailable(client->GetDeviceAuthenticator());
}

bool CheckCvcStorageAvailability() {
  return base::FeatureList::IsEnabled(
      autofill::features::kAutofillEnableCvcStorageAndFilling);
}

void AddAutofillStrings(content::WebUIDataSource* html_source,
                        Profile* profile,
                        content::WebContents* web_contents) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"autofillPageTitle", IDS_SETTINGS_AUTOFILL_AND_PASSWORDS},
    {"passwordsDescription", IDS_SETTINGS_PASSWORD_MANAGER_DESCRIPTION},
    {"passwordsDevice", IDS_SETTINGS_DEVICE_PASSWORDS},
    {"checkPasswords", IDS_SETTINGS_CHECK_PASSWORDS},
    {"checkPasswordsCanceled", IDS_SETTINGS_CHECK_PASSWORDS_CANCELED},
    {"checkedPasswords", IDS_SETTINGS_CHECKED_PASSWORDS},
    {"checkPasswordsDescription", IDS_SETTINGS_CHECK_PASSWORDS_DESCRIPTION},
    {"checkPasswordsErrorOffline", IDS_SETTINGS_CHECK_PASSWORDS_ERROR_OFFLINE},
    {"checkPasswordsErrorSignedOut",
     IDS_SETTINGS_CHECK_PASSWORDS_ERROR_SIGNED_OUT},
    {"checkPasswordsErrorNoPasswords",
     IDS_SETTINGS_CHECK_PASSWORDS_ERROR_NO_PASSWORDS},
    {"checkPasswordsErrorQuota",
     IDS_SETTINGS_CHECK_PASSWORDS_ERROR_QUOTA_LIMIT},
    {"checkPasswordsErrorGeneric", IDS_SETTINGS_CHECK_PASSWORDS_ERROR_GENERIC},
    {"noCompromisedCredentials", IDS_SETTINGS_NO_COMPROMISED_CREDENTIALS_LABEL},
    {"checkPasswordsAgain", IDS_SETTINGS_CHECK_PASSWORDS_AGAIN},
    {"checkPasswordsAgainAfterError",
     IDS_SETTINGS_CHECK_PASSWORDS_AGAIN_AFTER_ERROR},
    {"checkPasswordsProgress", IDS_SETTINGS_CHECK_PASSWORDS_PROGRESS},
    {"checkPasswordsStop", IDS_SETTINGS_CHECK_PASSWORDS_STOP},
    {"compromisedPasswords", IDS_SETTINGS_COMPROMISED_PASSWORDS},
    {"compromisedPasswordsDescription",
     IDS_SETTINGS_COMPROMISED_PASSWORDS_ADVICE},
    {"mutedPasswords", IDS_SETTINGS_MUTED_PASSWORDS},
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
    {"muteCompromisedPassword", IDS_SETTINGS_COMPROMISED_PASSWORD_MUTE},
    {"unmuteMutedCompromisedPassword",
     IDS_SETTINGS_COMPROMISED_PASSWORD_UNMUTE},
    {"removeCompromisedPasswordConfirmationTitle",
     IDS_SETTINGS_REMOVE_COMPROMISED_PASSWORD_CONFIRMATION_TITLE},
    {"removeCompromisedPasswordConfirmationDescription",
     IDS_SETTINGS_REMOVE_COMPROMISED_PASSWORD_CONFIRMATION_DESCRIPTION},
    {"alreadyChangedPasswordLink",
     IDS_SETTINGS_COMPROMISED_ALREADY_CHANGED_PASSWORD},
    {"editDisclaimerTitle", IDS_SETTINGS_COMPROMISED_EDIT_DISCLAIMER_TITLE},
    {"editDisclaimerDescription",
     IDS_SETTINGS_COMPROMISED_EDIT_DISCLAIMER_DESCRIPTION},
    {"genericCreditCard", IDS_AUTOFILL_CC_GENERIC},
    {"creditCards", IDS_AUTOFILL_PAYMENT_METHODS},
    {"paymentsMethodsTableAriaLabel",
     IDS_AUTOFILL_PAYMENT_METHODS_TABLE_ARIA_LABEL},
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
    {"enableCvcStorageLabel",
     IDS_AUTOFILL_SETTINGS_PAGE_ENABLE_CVC_STORAGE_LABEL},
    {"enableCvcStorageSublabel",
     IDS_AUTOFILL_SETTINGS_PAGE_ENABLE_CVC_STORAGE_SUBLABEL},
    {"enableCvcStorageDeleteDataSublabel",
     IDS_AUTOFILL_SETTINGS_PAGE_ENABLE_CVC_STORAGE_WITH_DELETE_LINK_SUBLABEL},
    {"enableMandatoryAuthToggleLabel",
     IDS_AUTOFILL_SETTINGS_PAGE_ENABLE_PAYMENT_METHOD_MANDATORY_REAUTH_LABEL},
    {"enableMandatoryAuthToggleSublabel",
     IDS_AUTOFILL_SETTINGS_PAGE_ENABLE_PAYMENT_METHOD_MANDATORY_REAUTH_SUBLABEL},
    {"bulkRemoveCvcConfirmationTitle",
     IDS_AUTOFILL_SETTINGS_PAGE_BULK_REMOVE_CVC_TITLE},
    {"bulkRemoveCvcConfirmationDescription",
     IDS_AUTOFILL_SETTINGS_PAGE_BULK_REMOVE_CVC_DESCRIPTION},
    {"addresses", IDS_AUTOFILL_ADDRESSES},
    {"addressesTableAriaLabel", IDS_AUTOFILL_ADDRESSES_TABLE_ARIA_LABEL},
    {"addressesTitle", IDS_AUTOFILL_ADDRESSES_SETTINGS_TITLE},
    {"addAddressTitle", IDS_SETTINGS_AUTOFILL_ADDRESSES_ADD_TITLE},
    {"editAddressTitle", IDS_SETTINGS_AUTOFILL_ADDRESSES_EDIT_TITLE},
    {"localAddressIconA11yLabel", IDS_AUTOFILL_LOCAL_ADDRESS_ICON_A11Y_LABEL},
    {"newAccountAddressSourceNotice",
     IDS_AUTOFILL_ADDRESS_WILL_BE_SAVED_IN_ACCOUNT_SOURCE_NOTICE},
    {"editAccountAddressSourceNotice",
     IDS_AUTOFILL_ADDRESS_ALREADY_SAVED_IN_ACCOUNT_SOURCE_NOTICE},
    {"deleteAccountAddressSourceNotice",
     IDS_AUTOFILL_DELETE_ACCOUNT_ADDRESS_SOURCE_NOTICE},
    {"addressCountry", IDS_SETTINGS_AUTOFILL_ADDRESSES_COUNTRY},
    {"addressPhone", IDS_SETTINGS_AUTOFILL_ADDRESSES_PHONE},
    {"addressEmail", IDS_SETTINGS_AUTOFILL_ADDRESSES_EMAIL},
    {"honorificLabel", IDS_SETTINGS_AUTOFILL_ADDRESS_HONORIFIC_LABEL},
    {"creditCardDescription", IDS_SETTINGS_AUTOFILL_CARD_DESCRIPTION},
    {"creditCardA11yLabeled", IDS_SETTINGS_AUTOFILL_CARD_A11Y_LABELED},
    {"creditCardExpDateA11yLabeled",
     IDS_SETTINGS_AUTOFILL_CARD_EXP_DATE_A11Y_LABELED},
    {"moreActionsForAddress", IDS_SETTINGS_AUTOFILL_MORE_ACTIONS_FOR_ADDRESS},
    {"moreActionsForCreditCard",
     IDS_SETTINGS_AUTOFILL_MORE_ACTIONS_FOR_CREDIT_CARD},
    {"removeAddress", IDS_SETTINGS_ADDRESS_REMOVE},
    {"removeAddressConfirmationTitle",
     IDS_SETTINGS_ADDRESS_REMOVE_CONFIRMATION_TITLE},
    {"removeSyncAddressConfirmationDescription",
     IDS_AUTOFILL_DELETE_SYNC_ADDRESS_SOURCE_NOTICE},
    {"removeLocalAddressConfirmationDescription",
     IDS_AUTOFILL_DELETE_LOCAL_ADDRESS_SOURCE_NOTICE},
    {"removeLocalCreditCardConfirmationTitle",
     IDS_SETTINGS_LOCAL_CARD_REMOVE_CONFIRMATION_TITLE},
    {"removeLocalPaymentMethodConfirmationDescription",
     IDS_SETTINGS_LOCAL_PAYMENT_METHOD_REMOVE_CONFIRMATION_DESCRIPTION},
    {"addressRemovedMessage", IDS_SETTINGS_ADDRESS_REMOVED_MESSAGE},
    {"editAddressRequiredFieldError",
     IDS_AUTOFILL_EDIT_ADDRESS_REQUIRED_FIELD_FORM_ERROR},
    {"editAddressRequiredFieldsError",
     IDS_AUTOFILL_EDIT_ADDRESS_REQUIRED_FIELDS_FORM_ERROR},
    {"clearCreditCard", IDS_SETTINGS_CREDIT_CARD_CLEAR},
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
    {"addPaymentMethods", IDS_SETTINGS_ADD_PAYMENT_METHODS},
    {"addPaymentMethodCreditOrDebitCard",
     IDS_SETTINGS_ADD_PAYMENT_METHOD_CREDIT_OR_DEBIT_CARD},
    {"addPaymentMethodIban", IDS_SETTINGS_ADD_PAYMENT_METHOD_IBAN},
    {"ibanSavedToThisDeviceOnly", IDS_SETTINGS_IBAN_SAVED_TO_THIS_DEVICE_ONLY},
    {"addIbanTitle", IDS_SETTINGS_ADD_IBAN_TITLE},
    {"editIbanTitle", IDS_SETTINGS_EDIT_IBAN_TITLE},
    {"ibanNickname", IDS_IBAN_NICKNAME},
    {"moreActionsForIban", IDS_SETTINGS_AUTOFILL_MORE_ACTIONS_FOR_IBAN},
    {"moreActionsForIbanDescription",
     IDS_SETTINGS_AUTOFILL_MORE_ACTIONS_FOR_IBAN_DESCRIPTION},
    {"editIban", IDS_SETTINGS_IBAN_EDIT},
    {"removeLocalIbanConfirmationTitle",
     IDS_SETTINGS_LOCAL_IBAN_REMOVE_CONFIRMATION_TITLE},
    {"migrateCreditCardsLabel", IDS_SETTINGS_MIGRATABLE_CARDS_LABEL},
    {"migratableCardsInfoSingle", IDS_SETTINGS_SINGLE_MIGRATABLE_CARD_INFO},
    {"migratableCardsInfoMultiple",
     IDS_SETTINGS_MULTIPLE_MIGRATABLE_CARDS_INFO},
    {"remoteCreditCardLinkLabel", IDS_SETTINGS_REMOTE_CREDIT_CARD_LINK_LABEL},
    {"upiIdLabel", IDS_SETTINGS_UPI_ID_LABEL},
    {"upiIdExpirationNever", IDS_SETTINGS_UPI_ID_EXPIRATION_NEVER},
    {"canMakePaymentToggleLabel", IDS_SETTINGS_CAN_MAKE_PAYMENT_TOGGLE_LABEL},
    {"autofillDetail", IDS_SETTINGS_AUTOFILL_DETAIL},
    {"passwords", IDS_SETTINGS_PASSWORD_MANAGER},
    {"passwordsSavePasswordsLabel",
     IDS_SETTINGS_PASSWORDS_SAVE_PASSWORDS_TOGGLE_LABEL},
    {"passwordsAutosigninLabel",
     IDS_SETTINGS_PASSWORDS_AUTOSIGNIN_CHECKBOX_LABEL},
    {"passwordsAutosigninDescription",
     IDS_SETTINGS_PASSWORDS_AUTOSIGNIN_CHECKBOX_DESC},
    {"passwordsLeakDetectionLabel",
     IDS_SETTINGS_PASSWORDS_LEAK_DETECTION_LABEL},
    {"passwordsLeakDetectionLabelUpdated",
     IDS_SETTINGS_PASSWORDS_LEAK_DETECTION_LABEL_UPDATED},
    {"passwordsLeakDetectionGeneralDescription",
     IDS_PASSWORD_MANAGER_LEAK_HELP_MESSAGE},
    {"passwordsLeakDetectionGeneralDescriptionUpdated",
     IDS_PASSWORD_MANAGER_LEAK_HELP_MESSAGE_UPDATED},
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
    {"addPasswordTitle", IDS_SETTINGS_PASSWORD_ADD_TITLE},
    {"addPasswordFootnote", IDS_SETTINGS_PASSWORD_ADD_FOOTNOTE},
    {"addPasswordStoreOptionAccount",
     IDS_SETTINGS_PASSWORD_STORE_PICKER_OPTION_ACCOUNT},
    {"addPasswordStoreOptionDevice",
     IDS_PASSWORD_MANAGER_DESTINATION_DROPDOWN_SAVE_TO_DEVICE},
    {"addPasswordStorePickerA11yDescription",
     IDS_PASSWORD_MANAGER_DESTINATION_DROPDOWN_ACCESSIBLE_NAME},
    {"usernameAlreadyUsed", IDS_SETTINGS_PASSWORD_USERNAME_ALREADY_USED},
    {"missingTLD", IDS_SETTINGS_PASSWORD_MISSING_TLD},
    {"viewExistingPassword", IDS_SETTINGS_PASSWORD_VIEW_EXISTING_PASSWORD},
    {"viewExistingPasswordAriaDescription",
     IDS_SETTINGS_PASSWORD_VIEW_EXISTING_PASSWORD_ARIA_DESCRIPTION},
    {"copyPassword", IDS_SETTINGS_PASSWORD_COPY},
    {"sendPassword", IDS_SETTINGS_PASSWORD_SEND},
    {"copyUsername", IDS_SETTINGS_USERNAME_COPY},
    {"passwordStoredOnDevice", IDS_SETTINGS_PASSWORD_STORED_ON_DEVICE},
    {"passwordStoredInAccount", IDS_SETTINGS_PASSWORD_STORED_IN_ACCOUNT},
    {"passwordStoredInAccountAndOnDevice",
     IDS_SETTINGS_PASSWORD_STORED_IN_ACCOUNT_AND_ON_DEVICE},
    {"editPasswordWebsiteLabel", IDS_SETTINGS_PASSWORDS_WEBSITE},
    {"editPasswordAppLabel", IDS_SETTINGS_COMPROMISED_EDIT_PASSWORD_APP},
    {"editPasswordUsernameLabel", IDS_SETTINGS_PASSWORDS_USERNAME},
    {"editPasswordPasswordLabel", IDS_SETTINGS_PASSWORDS_PASSWORD},
    {"passwordNoteLabel", IDS_SETTINGS_PASSWORDS_NOTE},
    {"passwordNoNoteAdded", IDS_SETTINGS_PASSWORDS_NO_NOTE_ADDED},
    {"passwordNoteCharacterCount", IDS_SETTINGS_PASSWORDS_NOTE_CHARACTER_COUNT},
    {"passwordNoteCharacterCountWarning",
     IDS_SETTINGS_PASSWORDS_NOTE_CHARACTER_COUNT_WARNING},
    {"passwordsTimedOut", IDS_SETTINGS_PASSWORDS_TIMED_OUT},
    {"passwordsGotIt", IDS_SETTINGS_GOT_IT},
#if BUILDFLAG(IS_MAC)
    {"passkeyLengthError", IDS_SETTINGS_PASSKEYS_LENGTH_ERROR},
    {"editPasskeySiteLabel", IDS_SETTINGS_PASSKEYS_SITE_LABEL},
    {"editPasskeyDialogTitle", IDS_SETTINGS_PASSKEYS_DIALOG_TITLE},
    {"passkeyEditDialogFootnote", IDS_SETTINGS_PASSKEYS_EDIT_DIALOG_FOOTNOTE},
#endif
    {"noAddressesFound", IDS_SETTINGS_ADDRESS_NONE},
    {"noPasswordsFound", IDS_SETTINGS_PASSWORDS_NONE},
    {"noPasswordsFoundImport", IDS_SETTINGS_PASSWORDS_NONE_WITH_IMPORT},
    {"noExceptionsFound", IDS_SETTINGS_PASSWORDS_EXCEPTIONS_NONE},
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
    {"passwordCopiedToClipboard", IDS_SETTINGS_PASSWORD_COPIED_TO_CLIPBOARD},
    {"passwordUsernameCopiedToClipboard",
     IDS_SETTINGS_PASSWORD_USERNAME_COPIED_TO_CLIPBOARD},
    {"passwordMovePasswordsToAccount",
     IDS_SETTINGS_PASSWORD_MOVE_PASSWORDS_TO_ACCOUNT},
    {"passwordMovePasswordsToAccountDialogBodyText",
     IDS_SETTINGS_PASSWORD_MOVE_PASSWORDS_TO_ACCOUNT_DIALOG_BODY_TEXT},
    {"passwordMovePasswordsToAccountDialogTitle",
     IDS_SETTINGS_PASSWORD_MOVE_PASSWORDS_TO_ACCOUNT_DIALOG_TITLE},
    {"passwordMoveMultiplePasswordsToAccountDialogMoveButtonText",
     IDS_SETTINGS_PASSWORD_MOVE_MULTIPLE_PASSWORDS_TO_ACCOUNT_DIALOG_MOVE_BUTTON_TEXT},
    {"passwordMoveMultiplePasswordsToAccountDialogCancelButtonText",
     IDS_SETTINGS_PASSWORD_MOVE_MULTIPLE_PASSWORDS_TO_ACCOUNT_DIALOG_CANCEL_BUTTON_TEXT},
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
    {"passwordTableAriaLabel", IDS_SETTINGS_PASSWORD_TABLE_ARIA_LABEL},
    {"passwordRowPasswordDetailPageButton",
     IDS_SETTINGS_PASSWORD_ROW_PASSWORD_DETAIL_PAGE},
    {"localPasswordManager",
     IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SAVING_ON_DEVICE},
    {"importMenuItem", IDS_SETTINGS_PASSWORDS_IMPORT_MENU_ITEM},
    {"importPasswordsTitle", IDS_SETTINGS_PASSWORDS_IMPORT_TITLE},
    {"importPasswordsErrorTitle", IDS_SETTINGS_PASSWORDS_IMPORT_ERROR_TITLE},
    {"importPasswordsCompleteTitle",
     IDS_SETTINGS_PASSWORDS_IMPORT_COMPLETE_TITLE},
    {"importPasswordsSuccessTitle",
     IDS_SETTINGS_PASSWORDS_IMPORT_SUCCESS_TITLE},
    {"importPasswordsChooseFile", IDS_SETTINGS_PASSWORDS_IMPORT_CHOOSE_FILE},
    {"importPasswordsSuccessTip", IDS_SETTINGS_PASSWORDS_IMPORT_SUCCESS_TIP},
    {"importPasswordsDeleteFileOption",
     IDS_SETTINGS_PASSWORDS_IMPORT_DELETE_FILE_OPTION},
    {"importPasswordsMissingPassword",
     IDS_SETTINGS_PASSWORDS_IMPORT_MISSING_PASSWORD},
    {"importPasswordsMissingURL", IDS_SETTINGS_PASSWORDS_IMPORT_MISSING_URL},
    {"importPasswordsInvalidURL", IDS_SETTINGS_PASSWORDS_IMPORT_INVALID_URL},
    {"importPasswordsLongURL", IDS_SETTINGS_PASSWORDS_IMPORT_LONG_URL},
    {"importPasswordsLongPassword",
     IDS_SETTINGS_PASSWORDS_IMPORT_LONG_PASSWORD},
    {"importPasswordsLongUsername",
     IDS_SETTINGS_PASSWORDS_IMPORT_LONG_USERNAME},
    {"importPasswordsLongNote", IDS_SETTINGS_PASSWORDS_IMPORT_LONG_NOTE},
    {"importPasswordsConflictDevice",
     IDS_SETTINGS_PASSWORDS_IMPORT_CONFLICT_DEVICE},
    {"importPasswordsConflictAccount",
     IDS_SETTINGS_PASSWORDS_IMPORT_CONFLICT_ACCOUNT},
    {"importPasswordsConflictsDescription",
     IDS_SETTINGS_PASSWORDS_IMPORT_CONFLICTS_DESCRIPTION},
    {"importPasswordsCancel", IDS_SETTINGS_PASSWORDS_IMPORT_CANCEL},
    {"importPasswordsSkip", IDS_SETTINGS_PASSWORDS_IMPORT_SKIP},
    {"importPasswordsReplace", IDS_SETTINGS_PASSWORDS_IMPORT_REPLACE},
    {"importPasswordsUnknownError",
     IDS_SETTINGS_PASSWORDS_IMPORT_ERROR_UNKNOWN},
    {"importPasswordsBadFormatError",
     IDS_SETTINGS_PASSWORDS_IMPORT_ERROR_BAD_FORMAT},
    {"importPasswordsGenericDescription",
     IDS_SETTINGS_PASSWORDS_IMPORT_DESCRIPTION_ACCOUNT_STORE_USERS},
    {"importPasswordsDescriptionAccount",
     IDS_SETTINGS_PASSWORDS_IMPORT_DESCRIPTION_SYNCING_USERS},
    {"importPasswordsDescriptionDevice",
     IDS_SETTINGS_PASSWORDS_IMPORT_DESCRIPTION_SIGNEDOUT_USERS},
    {"importPasswordsStorePickerA11yDescription",
     IDS_SETTINGS_PASSWORDS_IMPORT_STORE_PICKER_ACCESSIBLE_NAME},
    {"importPasswordsAlreadyActive",
     IDS_SETTINGS_PASSWORDS_IMPORT_ALREADY_ACTIVE},
    {"importPasswordsLimitExceeded",
     IDS_SETTINGS_PASSWORDS_IMPORT_ERROR_LIMIT_EXCEEDED},
    {"importPasswordsFileSizeExceeded",
     IDS_SETTINGS_PASSWORDS_IMPORT_FILE_SIZE_EXCEEDED},
    {"exportMenuItem", IDS_SETTINGS_PASSWORDS_EXPORT_MENU_ITEM},
    {"exportPasswordsTitle", IDS_SETTINGS_PASSWORDS_EXPORT_TITLE},
    {"exportPasswordsDescription", IDS_SETTINGS_PASSWORDS_EXPORT_DESCRIPTION},
    {"exportPasswords", IDS_SETTINGS_PASSWORDS_EXPORT},
    {"exportingPasswordsTitle", IDS_SETTINGS_PASSWORDS_EXPORTING_TITLE},
    {"exportPasswordsTryAgain", IDS_SETTINGS_PASSWORDS_EXPORT_TRY_AGAIN},
    {"exportPasswordsFailTitle",
     IDS_SETTINGS_PASSWORDS_EXPORTING_FAILURE_TITLE},
    {"exportPasswordsFailTips", IDS_SETTINGS_PASSWORDS_EXPORTING_FAILURE_TIPS},
    {"exportPasswordsFailTipsEnoughSpace",
     IDS_SETTINGS_PASSWORDS_EXPORTING_FAILURE_TIP_ENOUGH_SPACE},
    {"exportPasswordsFailTipsAnotherFolder",
     IDS_SETTINGS_PASSWORDS_EXPORTING_FAILURE_TIP_ANOTHER_FOLDER},
    {"managePasswordsPlaintext",
     IDS_SETTINGS_PASSWORDS_MANAGE_PASSWORDS_PLAINTEXT},
    {"savedToThisDeviceOnly", IDS_SETTINGS_PAYMENTS_SAVED_TO_THIS_DEVICE_ONLY},
    {"trustedVaultBannerLabel", IDS_SETTINGS_TRUSTED_VAULT_BANNER_LABEL},
    {"trustedVaultBannerSubLabelOfferOptIn",
     IDS_SETTINGS_TRUSTED_VAULT_BANNER_SUB_LABEL_OFFER_OPT_IN},
    {"trustedVaultBannerSubLabelOptedIn",
     IDS_SETTINGS_TRUSTED_VAULT_BANNER_SUB_LABEL_OPTED_IN},
    {"noSearchResults", IDS_SEARCH_NO_RESULTS},
    {"searchResultsPlural", IDS_SEARCH_RESULTS_PLURAL},
    {"searchResultsSingular", IDS_SEARCH_RESULTS_SINGULAR},
    {"showPasswordLabel", IDS_SETTINGS_PASSWORD_SHOW_PASSWORD_A11Y},
    {"hidePasswordLabel", IDS_SETTINGS_PASSWORD_HIDE_PASSWORD_A11Y},
    {"addVirtualCard", IDS_AUTOFILL_ADD_VIRTUAL_CARD},
    {"removeVirtualCard", IDS_AUTOFILL_REMOVE_VIRTUAL_CARD},
    {"editServerCard", IDS_AUTOFILL_EDIT_SERVER_CREDIT_CARD},
    {"virtualCardTurnedOn", IDS_AUTOFILL_VIRTUAL_CARD_TURNED_ON_LABEL},
    {"unenrollVirtualCardDialogTitle",
     IDS_AUTOFILL_VIRTUAL_CARD_UNENROLL_DIALOG_TITLE},
    {"unenrollVirtualCardDialogConfirm",
     IDS_AUTOFILL_VIRTUAL_CARD_UNENROLL_DIALOG_CONFIRM_BUTTON_LABEL},
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
    {"managePasskeysLabel", IDS_AUTOFILL_MANAGE_PASSKEYS_LABEL},
    {"managePasskeysTitle", IDS_AUTOFILL_MANAGE_PASSKEYS_TITLE},
    {"managePasskeysSearch", IDS_AUTOFILL_MANAGE_PASSKEYS_SEARCH},
    {"managePasskeysNoSupport", IDS_AUTOFILL_MANAGE_PASSKEYS_NO_SUPPORT},
    {"managePasskeysCannotDeleteTitle",
     IDS_AUTOFILL_MANAGE_PASSKEYS_CANNOT_DELETE_TITLE},
    {"managePasskeysCannotDeleteBody",
     IDS_AUTOFILL_MANAGE_PASSKEYS_CANNOT_DELETE_BODY},
    {"managePasskeysDeleteConfirmationTitle",
     IDS_AUTOFILL_MANAGE_PASSKEYS_DELETE_CONFIRMATION_TITLE},
    {"managePasskeysDeleteConfirmationDescription",
     IDS_AUTOFILL_MANAGE_PASSKEYS_DELETE_CONFIRMATION_DESCRIPTION},
    {"managePasskeysMoreActionsLabel",
     IDS_AUTOFILL_MANAGE_PASSKEYS_MORE_ACTIONS_LABEL},
#endif
#if BUILDFLAG(IS_MAC)
    {"biometricAuthenticaionForFillingLabel",
     IDS_SETTINGS_PASSWORDS_BIOMETRIC_AUTHENTICATION_FOR_FILLING_TOGGLE_LABEL_MAC},
    {"managePasskeysSubTitle", IDS_AUTOFILL_MANAGE_PASSKEYS_SUB_TITLE_MAC},
#elif BUILDFLAG(IS_WIN)
    {"biometricAuthenticaionForFillingLabel",
     IDS_SETTINGS_PASSWORDS_BIOMETRIC_AUTHENTICATION_FOR_FILLING_TOGGLE_LABEL_WIN},
    {"managePasskeysSubTitle", IDS_AUTOFILL_MANAGE_PASSKEYS_SUB_TITLE_WIN},
#endif
  };

  GURL google_password_manager_url = GetGooglePasswordManagerURL(
      password_manager::ManagePasswordsReferrer::kChromeSettings);

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
  html_source->AddString("addressesAndPaymentMethodsLearnMoreURL",
                         chrome::kAddressesAndPaymentMethodsLearnMoreURL);
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
  html_source->AddString("trustedVaultOptInUrl",
                         chrome::kSyncTrustedVaultOptInURL);
  html_source->AddString("trustedVaultLearnMoreUrl",
                         chrome::kSyncTrustedVaultLearnMoreURL);

  bool is_guest_mode = false;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  is_guest_mode =
      user_manager::UserManager::Get()->IsLoggedInAsGuest() ||
      user_manager::UserManager::Get()->IsLoggedInAsManagedGuestSession();
#else   // !BUILDFLAG(IS_CHROMEOS_ASH)
  is_guest_mode = profile->IsOffTheRecord();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(profile);
  html_source->AddBoolean(
      "migrationEnabled",
      !is_guest_mode &&
          autofill::IsCreditCardMigrationEnabled(
              personal_data, SyncServiceFactory::GetForProfile(profile),
              /*is_test_mode=*/false,
              /*log_manager=*/nullptr));

  html_source->AddBoolean("showIbansSettings",
                          autofill::ShouldShowIbanOnSettingsPage(
                              personal_data->GetCountryCodeForExperimentGroup(),
                              profile->GetPrefs()));

  html_source->AddBoolean("deviceAuthAvailable",
                          CheckDeviceAuthAvailability(web_contents));

  html_source->AddBoolean("cvcStorageAvailable", CheckCvcStorageAvailability());

  html_source->AddBoolean(
      "autofillEnablePaymentsMandatoryReauth",
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnablePaymentsMandatoryReauth));

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

  html_source->AddBoolean(
      "virtualCardEnrollmentEnabled",
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableUpdateVirtualCardEnrollment) &&
          base::FeatureList::IsEnabled(
              autofill::features::
                  kAutofillEnableVirtualCardManagementInDesktopSettingsPage));
  html_source->AddString(
      "unenrollVirtualCardDialogLabel",
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_UNENROLL_DIALOG_LABEL,
          base::UTF8ToUTF16(
              autofill::payments::GetVirtualCardEnrollmentSupportUrl()
                  .spec())));

  html_source->AddLocalizedStrings(kLocalizedStrings);
  // PASSWORD_VIEW page timeouts in 5 minutes:
  html_source->AddString(
      "passwordsTimedOutDescription",
      l10n_util::GetPluralStringFUTF16(
          IDS_SETTINGS_PASSWORDS_TIMED_OUT_DESCRIPTION,
          syncer::kPasswordNotesAuthValidity.Get().InMinutes()));

  html_source->AddBoolean(
      "autofillAccountProfileStorage",
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillAccountProfileStorage));

  html_source->AddBoolean(
      "syncEnableContactInfoDataType",
      base::FeatureList::IsEnabled(syncer::kSyncEnableContactInfoDataType));

  html_source->AddBoolean(
      "syncEnableContactInfoDataTypeInTransportMode",
      base::FeatureList::IsEnabled(
          syncer::kSyncEnableContactInfoDataTypeInTransportMode));
}

void AddSignOutDialogStrings(content::WebUIDataSource* html_source,
                             Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  bool is_main_profile = profile->IsMainProfile();
#else
  bool is_main_profile = false;
#endif

  if (is_main_profile) {
    static constexpr webui::LocalizedString kTurnOffStrings[] = {
        {"syncDisconnect", IDS_SETTINGS_PEOPLE_SYNC_TURN_OFF},
        {"syncDisconnectTitle", IDS_SETTINGS_TURN_OFF_SYNC_DIALOG_TITLE},
    };
    html_source->AddLocalizedStrings(kTurnOffStrings);
  } else {
    static constexpr webui::LocalizedString kTurnOffStrings[] = {
        {"syncDisconnect", IDS_SETTINGS_PEOPLE_SYNC_TURN_OFF},
        {"syncDisconnectTitle",
         IDS_SETTINGS_TURN_OFF_SYNC_AND_SIGN_OUT_DIALOG_TITLE},
    };
    html_source->AddLocalizedStrings(kTurnOffStrings);
  }

  std::string sync_dashboard_url =
      google_util::AppendGoogleLocaleParam(
          GURL(chrome::kSyncGoogleDashboardURL),
          g_browser_process->GetApplicationLocale())
          .spec();

  if (is_main_profile) {
    static constexpr webui::LocalizedString kSyncDisconnectStrings[] = {
        {"syncDisconnectDeleteProfile",
         IDS_SETTINGS_TURN_OFF_SYNC_DIALOG_CHECKBOX},
        {"syncDisconnectConfirm",
         IDS_SETTINGS_TURN_OFF_SYNC_DIALOG_MANAGED_CONFIRM},
        {"syncDisconnectExplanation",
         IDS_SETTINGS_SYNC_DISCONNECT_MAIN_PROFILE_EXPLANATION},
    };
    html_source->AddLocalizedStrings(kSyncDisconnectStrings);
  } else {
    static constexpr webui::LocalizedString kSyncDisconnectStrings[] = {
        {"syncDisconnectDeleteProfile",
         IDS_SETTINGS_TURN_OFF_SYNC_DIALOG_CHECKBOX},
        {"syncDisconnectConfirm",
         IDS_SETTINGS_TURN_OFF_SYNC_DIALOG_MANAGED_CONFIRM},
        {"syncDisconnectExplanation",
         IDS_SETTINGS_SYNC_DISCONNECT_AND_SIGN_OUT_EXPLANATION},
    };
    html_source->AddLocalizedStrings(kSyncDisconnectStrings);
  }

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  html_source->AddString(
      "syncDisconnectManagedProfileExplanation",
      l10n_util::GetStringFUTF8(
          base::FeatureList::IsEnabled(kDisallowManagedProfileSignout)
              ? IDS_SETTINGS_TURN_OFF_SYNC_MANAGED_PROFILE_EXPLANATION
              : IDS_SETTINGS_SYNC_DISCONNECT_MANAGED_PROFILE_EXPLANATION,
          u"$1", base::ASCIIToUTF16(sync_dashboard_url)));
#endif
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
    {"syncPasswordsNotWorking", IDS_SETTINGS_PEOPLE_SYNC_PASSWORDS_NOT_WORKING},
    {"peopleSignOut", IDS_SETTINGS_PEOPLE_SIGN_OUT},
    {"useAnotherAccount", IDS_SETTINGS_PEOPLE_SYNC_ANOTHER_ACCOUNT},

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
    {"syncAdvancedPageTitle", IDS_SETTINGS_NEW_SYNC_ADVANCED_PAGE_TITLE},
#endif

  };

  html_source->AddLocalizedStrings(kLocalizedStrings);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  html_source->AddLocalizedString(
      "syncAdvancedPageTitle",
      base::FeatureList::IsEnabled(syncer::kSyncChromeOSAppsToggleSharing)
          ? IDS_SETTINGS_NEW_SYNC_ADVANCED_BROWSER_PAGE_TITLE
          : IDS_SETTINGS_NEW_SYNC_ADVANCED_PAGE_TITLE);
#endif
}

void AddPersonalizationOptionsStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"urlKeyedAnonymizedDataCollection",
     IDS_SETTINGS_ENABLE_URL_KEYED_ANONYMIZED_DATA_COLLECTION},
    {"urlKeyedAnonymizedDataCollectionDesc",
     IDS_SETTINGS_ENABLE_URL_KEYED_ANONYMIZED_DATA_COLLECTION_DESC},
    {"spellingPref", IDS_SETTINGS_SPELLING_PREF},
#if !BUILDFLAG(IS_CHROMEOS)
    {"signinAllowedTitle", IDS_SETTINGS_SIGNIN_ALLOWED},
    {"signinAllowedDescription", IDS_SETTINGS_SIGNIN_ALLOWED_DESC},
#endif
    {"enablePersonalizationLogging", IDS_SETTINGS_ENABLE_LOGGING_PREF},
    {"enablePersonalizationLoggingDesc", IDS_SETTINGS_ENABLE_LOGGING_PREF_DESC},
    {"spellingDescription", IDS_SETTINGS_SPELLING_PREF_DESC},
    {"searchSuggestPrefDesc", IDS_SETTINGS_SUGGEST_PREF_DESC},
    {"linkDoctorPref", IDS_SETTINGS_LINKDOCTOR_PREF},
    {"linkDoctorPrefDesc", IDS_SETTINGS_LINKDOCTOR_PREF_DESC},
    {"searchSuggestPref", IDS_SETTINGS_SUGGEST_PREF},
    {"driveSuggestPref", IDS_SETTINGS_DRIVE_SUGGEST_PREF},
    {"driveSuggestPrefDesc", IDS_SETTINGS_DRIVE_SUGGEST_PREF_DESC},
    {"priceEmailNotificationsPref", IDS_PRICE_TRACKING_SETTINGS_TITLE},
    {"priceEmailNotificationsPrefDesc",
     IDS_PRICE_TRACKING_SETTINGS_EMAIL_DESCRIPTION},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

#if BUILDFLAG(IS_CHROMEOS)
std::string BuildOSSettingsUrl(const std::string& sub_page) {
  std::string os_settings_url = chrome::kChromeUIOSSettingsURL;
  os_settings_url.append(sub_page);
  return os_settings_url;
}
#endif

void AddBrowserSyncPageStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"passwordsCheckboxLabel", IDS_SETTINGS_PASSWORDS_CHECKBOX_LABEL},
    {"peopleSignInSyncPagePromptSecondaryWithAccount",
     IDS_SETTINGS_PEOPLE_SIGN_IN_PROMPT_SECONDARY_WITH_ACCOUNT},
    {"peopleSignInSyncPagePromptSecondaryWithNoAccount",
     IDS_SETTINGS_PEOPLE_SIGN_IN_PROMPT_SECONDARY_WITH_ACCOUNT},
    {"bookmarksCheckboxLabel", IDS_SETTINGS_BOOKMARKS_CHECKBOX_LABEL},
    {"readingListCheckboxLabel", IDS_SETTINGS_READING_LIST_CHECKBOX_LABEL},
    {"cancelSync", IDS_SETTINGS_SYNC_SETTINGS_CANCEL_SYNC},
    {"syncSetupCancelDialogTitle", IDS_SETTINGS_SYNC_SETUP_CANCEL_DIALOG_TITLE},
    {"syncSetupCancelDialogBody", IDS_SETTINGS_SYNC_SETUP_CANCEL_DIALOG_BODY},
    {"personalizeGoogleServicesTitle",
     IDS_SETTINGS_PERSONALIZE_GOOGLE_SERVICES_TITLE},
    {"themeCheckboxLabel", IDS_SETTINGS_THEME_CHECKBOX_LABEL},
#if BUILDFLAG(IS_CHROMEOS)
    {"browserSyncFeatureLabel", IDS_BROWSER_SETTINGS_SYNC_FEATURE_LABEL},
#endif
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString("activityControlsUrl",
                         chrome::kGoogleAccountActivityControlsURL);
  html_source->AddString(
      "activityControlsUrlInPrivacyGuide",
      chrome::kGoogleAccountActivityControlsURLInPrivacyGuide);

#if BUILDFLAG(IS_CHROMEOS)
  html_source->AddString(
      "osSyncSetupSettingsUrl",
      BuildOSSettingsUrl(chromeos::settings::mojom::kSyncSetupSubpagePath));
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  html_source->AddString(
      "osPrivacySettingsUrl",
      BuildOSSettingsUrl(
          chromeos::settings::mojom::kPrivacyAndSecuritySectionPath));

  html_source->AddBoolean(
      "osDeprecateSyncMetricsToggle",
      ash::features::IsOsSettingsDeprecateSyncMetricsToggleEnabled());
#endif

#if BUILDFLAG(IS_CHROMEOS)
  html_source->AddString(
      "osSyncSettingsUrl",
      BuildOSSettingsUrl(chromeos::settings::mojom::kSyncSubpagePath));
#endif
}

void AddSyncControlsStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"autofillCheckboxLabel", IDS_SETTINGS_AUTOFILL_CHECKBOX_LABEL},
    {"historyCheckboxLabel", IDS_SETTINGS_HISTORY_CHECKBOX_LABEL},
    {"extensionsCheckboxLabel", IDS_SETTINGS_EXTENSIONS_CHECKBOX_LABEL},
    {"openTabsCheckboxLabel", IDS_SETTINGS_OPEN_TABS_CHECKBOX_LABEL},
    {"savedTabGroupsCheckboxLabel",
     IDS_SETTINGS_SAVED_TAB_GROUPS_CHECKBOX_LABEL},
    {"wifiConfigurationsCheckboxLabel",
     IDS_SETTINGS_WIFI_CONFIGURATIONS_CHECKBOX_LABEL},
    {"syncEverythingCheckboxLabel",
     IDS_SETTINGS_SYNC_EVERYTHING_CHECKBOX_LABEL},
    {"appCheckboxLabel", IDS_SETTINGS_APPS_CHECKBOX_LABEL},
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    {"appCheckboxSublabel", IDS_SETTINGS_APPS_CHECKBOX_SUBLABEL},
#endif
    {"paymentsCheckboxLabel", IDS_SYNC_DATATYPE_PAYMENTS},
    {"nonPersonalizedServicesSectionLabel",
     IDS_SETTINGS_NON_PERSONALIZED_SERVICES_SECTION_LABEL},
    {"customizeSyncLabel", IDS_SETTINGS_CUSTOMIZE_SYNC},
    {"syncData", IDS_SETTINGS_SYNC_DATA},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
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
    {"colorsContainerLabel", IDS_NTP_THEMES_CONTAINER_LABEL},
    {"colorPickerLabel", IDS_NTP_CUSTOMIZE_COLOR_PICKER_LABEL},
    {"defaultColorName", IDS_NTP_CUSTOMIZE_DEFAULT_LABEL},
    {"defaultThemeLabel", IDS_NTP_CUSTOMIZE_DEFAULT_LABEL},
    {"greyDefaultColorName", IDS_NTP_CUSTOMIZE_GREY_DEFAULT_LABEL},
    {"mainColorName", IDS_NTP_CUSTOMIZE_MAIN_COLOR_LABEL},
    {"managedColorsBody", IDS_NTP_THEME_MANAGED_DIALOG_BODY},
    {"managedColorsTitle", IDS_NTP_THEME_MANAGED_DIALOG_TITLE},
    {"themesContainerLabel", IDS_SETTINGS_PICK_A_THEME_COLOR},
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
  auto* profile_entry =
      g_browser_process->profile_manager()
          ? g_browser_process->profile_manager()
                ->GetProfileAttributesStorage()
                .GetProfileAttributesWithPath(profile->GetPath())
          : nullptr;
  html_source->AddBoolean(
      "signinAvailable",
      AccountConsistencyModeManager::IsDiceSignInAllowed(profile_entry));
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Toggles the Chrome OS Account Manager submenu in the People section.
  html_source->AddBoolean("isAccountManagerEnabled",
                          ash::IsAccountManagerAvailable(profile));
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  html_source->AddBoolean(
      "isAccountManagerEnabled",
      AccountConsistencyModeManager::IsMirrorEnabledForProfile(profile));
#endif

  AddSignOutDialogStrings(html_source, profile);
  AddSyncControlsStrings(html_source);
  AddSyncAccountControlStrings(html_source);
#if BUILDFLAG(IS_CHROMEOS)
  AddPasswordPromptDialogStrings(html_source);
#endif
  AddBrowserSyncPageStrings(html_source);
  AddSharedSyncPageStrings(html_source);
}

bool ShouldLinkSecureDnsOsSettings() {
#if BUILDFLAG(IS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

bool IsSecureDnsAvailable() {
  return features::kDnsOverHttpsShowUiParam.Get();
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
    {"httpsOnlyModeTitle", IDS_SETTINGS_HTTPS_ONLY_MODE},
    {"httpsOnlyModeDescription", IDS_SETTINGS_HTTPS_ONLY_MODE_DESCRIPTION},
    {"httpsOnlyModeDescriptionAdvancedProtection",
     IDS_SETTINGS_HTTPS_ONLY_MODE_DESCRIPTION_ADVANCED_PROTECTION},
    {"manageCertificates", IDS_SETTINGS_MANAGE_CERTIFICATES},
    {"manageCertificatesDescription",
     IDS_SETTINGS_MANAGE_CERTIFICATES_DESCRIPTION},
    {"contentSettings", IDS_SETTINGS_CONTENT_SETTINGS},
    {"siteSettings", IDS_SETTINGS_SITE_SETTINGS},
    {"siteSettingsDescription", IDS_SETTINGS_SITE_SETTINGS_DESCRIPTION},
    {"clearData", IDS_SETTINGS_CLEAR_DATA},
    {"clearingData", IDS_SETTINGS_CLEARING_DATA},
    {"clearedData", IDS_SETTINGS_CLEARED_DATA},
    {"clearBrowsingData", IDS_SETTINGS_CLEAR_BROWSING_DATA},
    {"clearBrowsingDataDescription", IDS_SETTINGS_CLEAR_DATA_DESCRIPTION},
    {"titleAndCount", IDS_SETTINGS_TITLE_AND_COUNT},
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
    {"safeBrowsingStandardDescUpdated",
     IDS_SETTINGS_SAFEBROWSING_STANDARD_DESC_UPDATED},
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
    {"networkPredictionEnabled", IDS_SETTINGS_NETWORK_PREDICTION_ENABLED_LABEL},
    {"networkPredictionEnabledDesc",
     IDS_SETTINGS_NETWORK_PREDICTION_ENABLED_DESC},
    {"networkPredictionEnabledDescCookiesPage",
     IDS_SETTINGS_NETWORK_PREDICTION_ENABLED_DESC_COOKIES_PAGE},
    {"preloadingPageTitle", IDS_SETTINGS_PRELOAD_PAGES_TITLE},
    {"preloadingPageSummary", IDS_SETTINGS_PRELOAD_PAGES_SUMMARY},
    {"preloadingPageNoPreloadingTitle",
     IDS_SETTINGS_PRELOAD_PAGES_NO_PRELOADING_TITLE},
    {"preloadingPageNoPreloadingSummary",
     IDS_SETTINGS_PRELOAD_PAGES_NO_PRELOADING_SUMMARY},
    {"preloadingPageStandardPreloadingTitle",
     IDS_SETTINGS_PRELOAD_PAGES_STANDARD_PRELOADING_TITLE},
    {"preloadingPageStandardPreloadingSummary",
     IDS_SETTINGS_PRELOAD_PAGES_STANDARD_PRELOADING_SUMMARY},
    {"preloadingPageStandardPreloadingWhenOnBulletOne",
     IDS_SETTINGS_PRELOAD_PAGES_STANDARD_PRELOADING_WHEN_ON_BULLET_ONE},
    {"preloadingPageStandardPreloadingWhenOnBulletTwo",
     IDS_SETTINGS_PRELOAD_PAGES_STANDARD_PRELOADING_WHEN_ON_BULLET_TWO},
    {"preloadingPageExtendedPreloadingTitle",
     IDS_SETTINGS_PRELOAD_PAGES_EXTENDED_PRELOADING_TITLE},
    {"preloadingPageExtendedPreloadingSummary",
     IDS_SETTINGS_PRELOAD_PAGES_EXTENDED_PRELOADING_SUMMARY},
    {"preloadingPageExtendedPreloadingWhenOnBulletOne",
     IDS_SETTINGS_PRELOAD_PAGES_EXTENDED_PRELOADING_WHEN_ON_BULLET_ONE},
    {"preloadingPageExtendedPreloadingWhenOnBulletTwo",
     IDS_SETTINGS_PRELOAD_PAGES_EXTENDED_PRELOADING_WHEN_ON_BULLET_TWO},
    {"preloadingPageExtendedPreloadingThingsToConsiderBulletTwo",
     IDS_SETTINGS_PRELOAD_PAGES_EXTENDED_PRELOADING_THINGS_TO_CONSIDER_BULLET_TWO},
    {"preloadingPageThingsToConsiderBulletOne",
     IDS_SETTINGS_PRELOAD_PAGES_THINGS_TO_CONSIDER_BULLET_ONE},
#if BUILDFLAG(IS_CHROMEOS)
    {"openChromeOSSecureDnsSettingsLabel",
     IDS_SETTINGS_SECURE_DNS_OPEN_CHROME_OS_SETTINGS_LABEL},
#endif
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
    {"manageDeviceCertificates", IDS_SETTINGS_MANAGE_DEVICE_CERTIFICATES},
    {"manageDeviceCertificatesDescription",
     IDS_SETTINGS_MANAGE_DEVICE_CERTIFICATES_DESCRIPTION},
    {"chromeCertificates", IDS_SETTINGS_CHROME_CERTIFICATES},
    {"chromeCertificatesDescription",
     IDS_SETTINGS_CHROME_CERTIFICATES_DESCRIPTION},
#endif
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString("cookiesSettingsHelpCenterURL",
                         chrome::kCookiesSettingsHelpCenterURL);

  html_source->AddString("firstPartySetsLearnMoreURL",
                         chrome::kFirstPartySetsLearnMoreURL);

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
      "driveSuggestAvailable",
      base::FeatureList::IsEnabled(omnibox::kDocumentProvider));
  html_source->AddBoolean(
      "driveSuggestNoSetting",
      base::FeatureList::IsEnabled(omnibox::kDocumentProviderNoSetting));
  html_source->AddBoolean("driveSuggestNoSyncRequirement",
                          base::FeatureList::IsEnabled(
                              omnibox::kDocumentProviderNoSyncRequirement));

  bool show_secure_dns = IsSecureDnsAvailable();
  bool link_secure_dns = ShouldLinkSecureDnsOsSettings();
  html_source->AddBoolean("showSecureDnsSetting",
                          show_secure_dns && !link_secure_dns);
#if BUILDFLAG(IS_CHROMEOS)
  html_source->AddBoolean("showSecureDnsSettingLink",
                          show_secure_dns && link_secure_dns);
  html_source->AddString(
      "chromeOSPrivacyAndSecuritySectionPath",
      chromeos::settings::mojom::kPrivacyAndSecuritySectionPath);
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  html_source->AddBoolean("showChromeRootStoreCertificates",
#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
                          SystemNetworkContextManager::IsUsingChromeRootStore()
#else
                          true
#endif
  );

  html_source->AddString("chromeRootStoreHelpCenterURL",
                         chrome::kChromeRootStoreSettingsHelpCenterURL);
#endif  // BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)

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

  html_source->AddBoolean("showPreloadingSubPage",
                          base::FeatureList::IsEnabled(
                              features::kPreloadingDesktopSettingsSubPage));

  AddPersonalizationOptionsStrings(html_source);
  AddSecureDnsStrings(html_source);
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
      {"privacySandboxPageHeading", IDS_SETTINGS_PRIVACY_SANDBOX_PAGE_HEADING},
      {"privacySandboxPageDetails", IDS_SETTINGS_PRIVACY_SANDBOX_PAGE_DETAILS},
      // The following strings are used for PrivacySandboxSettings3.
      {"privacySandboxTrialsTitle", IDS_SETTINGS_PRIVACY_SANDBOX_TRIALS_TITLE},
      {"privacySandboxTrialsSummary",
       IDS_SETTINGS_PRIVACY_SANDBOX_TRIALS_SUMMARY},
      {"privacySandboxTrialsSummaryLearnMore",
       IDS_SETTINGS_PRIVACY_SANDBOX_TRIALS_SUMMARY_LEARN_MORE},
      {"privacySandboxAdPersonalizationTitle",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_TITLE},
      {"privacySandboxAdPersonalizationSummary",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_SUMMARY},
      {"privacySandboxAdMeasurementTitle",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_MEASUREMENT_TITLE},
      {"privacySandboxAdMeasurementSummary",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_MEASUREMENT_SUMMARY},
      {"privacySandboxSpamAndFraudTitle",
       IDS_SETTINGS_PRIVACY_SANDBOX_SPAM_AND_FRAUD_TITLE},
      {"privacySandboxSpamAndFraudSummary",
       IDS_SETTINGS_PRIVACY_SANDBOX_SPAM_AND_FRAUD_SUMMARY},
      {"privacySandboxLearnMoreDialogTitle",
       IDS_SETTINGS_PRIVACY_SANDBOX_LEARN_MORE_DIALOG_TITLE},
      {"privacySandboxLearnMoreDialogTopicsTitle",
       IDS_SETTINGS_PRIVACY_SANDBOX_LEARN_MORE_DIALOG_TOPICS_TITLE},
      {"privacySandboxLearnMoreDialogFledgeTitle",
       IDS_SETTINGS_PRIVACY_SANDBOX_LEARN_MORE_DIALOG_FLEDGE_TITLE},
      {"privacySandboxLearnMoreDialogDataTypes",
       IDS_SETTINGS_PRIVACY_SANDBOX_LEARN_MORE_DIALOG_DATA_TYPES},
      {"privacySandboxLearnMoreDialogDataUsage",
       IDS_SETTINGS_PRIVACY_SANDBOX_LEARN_MORE_DIALOG_DATA_USAGE},
      {"privacySandboxLearnMoreDialogDataManagement",
       IDS_SETTINGS_PRIVACY_SANDBOX_LEARN_MORE_DIALOG_DATA_MANAGEMENT},
      {"privacySandboxLearnMoreDialogTopicsDataTypes",
       IDS_SETTINGS_PRIVACY_SANDBOX_LEARN_MORE_DIALOG_TOPICS_DATA_TYPES},
      {"privacySandboxLearnMoreDialogTopicsDataUsage",
       IDS_SETTINGS_PRIVACY_SANDBOX_LEARN_MORE_DIALOG_TOPICS_DATA_USAGE},
      {"privacySandboxLearnMoreDialogTopicsDataManagement",
       IDS_SETTINGS_PRIVACY_SANDBOX_LEARN_MORE_DIALOG_TOPICS_DATA_MANAGEMENT},
      {"privacySandboxLearnMoreDialogFledgeDataTypes",
       IDS_SETTINGS_PRIVACY_SANDBOX_LEARN_MORE_DIALOG_FLEDGE_DATA_TYPES},
      {"privacySandboxLearnMoreDialogFledgeDataUsage",
       IDS_SETTINGS_PRIVACY_SANDBOX_LEARN_MORE_DIALOG_FLEDGE_DATA_USAGE},
      {"privacySandboxLearnMoreDialogFledgeDataManagement",
       IDS_SETTINGS_PRIVACY_SANDBOX_LEARN_MORE_DIALOG_FLEDGE_DATA_MANAGEMENT},
      {"privacySandboxAdPersonalizationDialogTitle",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_DIALOG_TITLE},
      {"privacySandboxAdPersonalizationDialogDescription",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_DIALOG_DESCRIPTION},
      {"privacySandboxAdPersonalizationDialogDescriptionTrialsOff",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_DIALOG_DESCRIPTION_TRIALS_OFF},
      {"privacySandboxAdPersonalizationDialogDescriptionListsEmpty",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_DIALOG_DESCRIPTION_LISTS_EMPTY},
      {"privacySandboxAdPersonalizationRemovedDialogTitle",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_REMOVED_DIALOG_TITLE},
      {"privacySandboxAdPersonalizationRemovedDialogDescription",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_REMOVED_DIALOG_DESCRIPTION},
      {"privacySandboxAdPersonalizationDialogTopicsTitle",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_DIALOG_TOPICS_TITLE},
      {"privacySandboxAdPersonalizationDialogTopicsEmpty",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_DIALOG_TOPICS_EMPTY},
      {"privacySandboxAdPersonalizationDialogRemovedTopicsLabel",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_DIALOG_REMOVED_TOPICS_LABEL},
      {"privacySandboxAdPersonalizationDialogRemovedTopicsEmpty",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_DIALOG_REMOVED_TOPICS_EMPTY},
      {"privacySandboxAdPersonalizationDialogTopicsLearnMore1",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_DIALOG_TOPICS_LEARN_MORE_1},
      {"privacySandboxAdPersonalizationDialogTopicsLearnMore2",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_DIALOG_TOPICS_LEARN_MORE_2},
      {"privacySandboxAdPersonalizationDialogTopicsLearnMore3",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_DIALOG_TOPICS_LEARN_MORE_3},
      {"privacySandboxAdPersonalizationDialogFledgeTitle",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_DIALOG_FLEDGE_TITLE},
      {"privacySandboxAdPersonalizationDialogFledgeEmpty",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_DIALOG_FLEDGE_EMPTY},
      {"privacySandboxAdPersonalizationDialogRemovedFledgeLabel",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_DIALOG_REMOVED_FLEDGE_LABEL},
      {"privacySandboxAdPersonalizationDialogRemovedFledgeEmpty",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_DIALOG_REMOVED_FLEDGE_EMPTY},
      {"privacySandboxAdPersonalizationDialogFledgeLearnMore1",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_DIALOG_FLEDGE_LEARN_MORE_1},
      {"privacySandboxAdPersonalizationDialogFledgeLearnMore2",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_DIALOG_FLEDGE_LEARN_MORE_2},
      {"privacySandboxAdPersonalizationDialogFledgeLearnMore3",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_PERSONALIZATION_DIALOG_FLEDGE_LEARN_MORE_3},
      {"privacySandboxAdMeasurementDialogTitle",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_MEASUREMENT_DIALOG_TITLE},
      {"privacySandboxAdMeasurementDialogDescription",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_MEASUREMENT_DIALOG_DESCRIPTION},
      {"privacySandboxAdMeasurementDialogDescriptionTrialsOff",
       IDS_SETTINGS_PRIVACY_SANDBOX_AD_MEASUREMENT_DIALOG_DESCRIPTION_TRIALS_OFF},
      {"privacySandboxSpamAndFraudDialogTitle",
       IDS_SETTINGS_PRIVACY_SANDBOX_SPAM_AND_FRAUD_DIALOG_TITLE},
      {"privacySandboxSpamAndFraudDialogDescription1",
       IDS_SETTINGS_PRIVACY_SANDBOX_SPAM_AND_FRAUD_DIALOG_DESCRIPTION_1},
      {"privacySandboxSpamAndFraudDialogDescription1TrialsOff",
       IDS_SETTINGS_PRIVACY_SANDBOX_SPAM_AND_FRAUD_DIALOG_DESCRIPTION_1_TRIALS_OFF},
      {"privacySandboxSpamAndFraudDialogDescription2",
       IDS_SETTINGS_PRIVACY_SANDBOX_SPAM_AND_FRAUD_DIALOG_DESCRIPTION_2},
      {"privacySandboxSpamAndFraudDialogDescription3",
       IDS_SETTINGS_PRIVACY_SANDBOX_SPAM_AND_FRAUD_DIALOG_DESCRIPTION_3},
      {"adPrivacyLinkRowLabel", IDS_SETTINGS_AD_PRIVACY_LINK_ROW_LABEL},
      {"adPrivacyLinkRowSubLabel", IDS_SETTINGS_AD_PRIVACY_LINK_ROW_SUB_LABEL},
      {"adPrivacyRestrictedLinkRowSubLabel",
       IDS_SETTINGS_AD_PRIVACY_RESTRICTED_LINK_ROW_SUB_LABEL},
      {"adPrivacyPageTitle", IDS_SETTINGS_AD_PRIVACY_PAGE_TITLE},
      {"adPrivacyPageTopicsLinkRowLabel",
       IDS_SETTINGS_AD_PRIVACY_PAGE_TOPICS_LINK_ROW_LABEL},
      {"adPrivacyPageTopicsLinkRowSubLabelEnabled",
       IDS_SETTINGS_AD_PRIVACY_PAGE_TOPICS_LINK_ROW_SUB_LABEL_ENABLED},
      {"adPrivacyPageTopicsLinkRowSubLabelDisabled",
       IDS_SETTINGS_AD_PRIVACY_PAGE_TOPICS_LINK_ROW_SUB_LABEL_DISABLED},
      {"adPrivacyPageFledgeLinkRowLabel",
       IDS_SETTINGS_AD_PRIVACY_PAGE_FLEDGE_LINK_ROW_LABEL},
      {"adPrivacyPageFledgeLinkRowSubLabelEnabled",
       IDS_SETTINGS_AD_PRIVACY_PAGE_FLEDGE_LINK_ROW_SUB_LABEL_ENABLED},
      {"adPrivacyPageFledgeLinkRowSubLabelDisabled",
       IDS_SETTINGS_AD_PRIVACY_PAGE_FLEDGE_LINK_ROW_SUB_LABEL_DISABLED},
      {"adPrivacyPageAdMeasurementLinkRowLabel",
       IDS_SETTINGS_AD_PRIVACY_PAGE_AD_MEASUREMENT_LINK_ROW_LABEL},
      {"adPrivacyPageAdMeasurementLinkRowSubLabelEnabled",
       IDS_SETTINGS_AD_PRIVACY_PAGE_AD_MEASUREMENT_LINK_ROW_SUB_LABEL_ENABLED},
      {"adPrivacyPageAdMeasurementLinkRowSubLabelDisabled",
       IDS_SETTINGS_AD_PRIVACY_PAGE_AD_MEASUREMENT_LINK_ROW_SUB_LABEL_DISABLED},
      {"topicsPageTitle", IDS_SETTINGS_TOPICS_PAGE_TITLE},
      {"topicsPageToggleLabel", IDS_SETTINGS_TOPICS_PAGE_TOGGLE_LABEL},
      {"topicsPageToggleSubLabel", IDS_SETTINGS_TOPICS_PAGE_TOGGLE_SUB_LABEL},
      {"topicsPageCurrentTopicsHeading",
       IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_HEADING},
      {"topicsPageCurrentTopicsDescription",
       IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION},
      {"topicsPageCurrentTopicsDescriptionLearnMoreLink",
       IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_LEARN_MORE_LINK},
      {"topicsPageCurrentTopicsRegionA11yDescription",
       IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_REGION_A11Y_DESCRIPTION},
      {"topicsPageLearnMoreHeading",
       IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_HEADING},
      {"topicsPageLearnMoreBullet1",
       IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_1},
      {"topicsPageLearnMoreBullet2",
       IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_2},
      {"topicsPageCurrentTopicsDescriptionDisabled",
       IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_DISABLED},
      {"topicsPageCurrentTopicsDescriptionEmpty",
       IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_EMPTY},
      {"topicsPageBlockTopic", IDS_SETTINGS_TOPICS_PAGE_BLOCK_TOPIC},
      {"topicsPageBlockTopicA11yLabel",
       IDS_SETTINGS_TOPICS_PAGE_BLOCK_TOPIC_A11Y_LABEL},
      {"topicsPageBlockedTopicsHeading",
       IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_HEADING},
      {"topicsPageBlockedTopicsDescription",
       IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION},
      {"topicsPageBlockedTopicsDescriptionEmpty",
       IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION_EMPTY},
      {"topicsPageBlockedTopicsRegionA11yDescription",
       IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_REGION_A11Y_DESCRIPTION},
      {"topicsPageAllowTopic", IDS_SETTINGS_TOPICS_PAGE_ALLOW_TOPIC},
      {"topicsPageAllowTopicA11yLabel",
       IDS_SETTINGS_TOPICS_PAGE_ALLOW_TOPIC_A11Y_LABEL},
      {"topicsPageCurrentTopicsDescriptionLearnMoreA11yLabel",
       IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_LEARN_MORE_A11Y_LABEL},
      {"fledgePageTitle", IDS_SETTINGS_FLEDGE_PAGE_TITLE},
      {"fledgePageToggleLabel", IDS_SETTINGS_FLEDGE_PAGE_TOGGLE_LABEL},
      {"fledgePageToggleSubLabel", IDS_SETTINGS_FLEDGE_PAGE_TOGGLE_SUB_LABEL},
      {"fledgePageCurrentSitesHeading",
       IDS_SETTINGS_FLEDGE_PAGE_CURRENT_SITES_HEADING},
      {"fledgePageCurrentSitesDescription",
       IDS_SETTINGS_FLEDGE_PAGE_CURRENT_SITES_DESCRIPTION},
      {"fledgePageCurrentSitesDescriptionLearnMore",
       IDS_SETTINGS_FLEDGE_PAGE_CURRENT_SITES_DESCRIPTION_LEARN_MORE},
      {"fledgePageCurrentSitesDescriptionDisabled",
       IDS_SETTINGS_FLEDGE_PAGE_CURRENT_SITES_DESCRIPTION_DISABLED},
      {"fledgePageCurrentSitesDescriptionEmpty",
       IDS_SETTINGS_FLEDGE_PAGE_CURRENT_SITES_DESCRIPTION_EMPTY},
      {"fledgePageCurrentSitesRegionA11yDescription",
       IDS_SETTINGS_FLEDGE_PAGE_CURRENT_SITES_REGION_A11Y_DESCRIPTION},
      {"fledgePageSeeAllSitesLabel",
       IDS_SETTINGS_FLEDGE_PAGE_SEE_ALL_SITES_LABEL},
      {"fledgePageBlockSite", IDS_SETTINGS_FLEDGE_PAGE_BLOCK_SITE},
      {"fledgePageBlockSiteA11yLabel",
       IDS_SETTINGS_FLEDGE_PAGE_BLOCK_SITE_A11Y_LABEL},
      {"fledgePageBlockedSitesHeading",
       IDS_SETTINGS_FLEDGE_PAGE_BLOCKED_SITES_HEADING},
      {"fledgePageBlockedSitesDescription",
       IDS_SETTINGS_FLEDGE_PAGE_BLOCKED_SITES_DESCRIPTION},
      {"fledgePageBlockedSitesDescriptionEmpty",
       IDS_SETTINGS_FLEDGE_PAGE_BLOCKED_SITES_DESCRIPTION_EMPTY},
      {"fledgePageBlockedSitesRegionA11yDescription",
       IDS_SETTINGS_FLEDGE_PAGE_BLOCKED_SITES_REGION_A11Y_DESCRIPTION},
      {"fledgePageAllowSite", IDS_SETTINGS_FLEDGE_PAGE_ALLOW_SITE},
      {"fledgePageAllowSiteA11yLabel",
       IDS_SETTINGS_FLEDGE_PAGE_ALLOW_SITE_A11Y_LABEL},
      {"fledgePageLearnMoreHeading",
       IDS_SETTINGS_FLEDGE_PAGE_LEARN_MORE_HEADING},
      {"fledgePageLearnMoreBullet1",
       IDS_SETTINGS_FLEDGE_PAGE_LEARN_MORE_BULLET_1},
      {"fledgePageLearnMoreBullet2",
       IDS_SETTINGS_FLEDGE_PAGE_LEARN_MORE_BULLET_2},
      {"fledgePageCurrentSitesDescriptionLearnMoreA11yLabel",
       IDS_SETTINGS_FLEDGE_PAGE_CURRENT_SITES_DESCRIPTION_LEARN_MORE_A11Y_LABEL},
      {"adMeasurementPageTitle", IDS_SETTINGS_AD_MEASUREMENT_PAGE_TITLE},
      {"adMeasurementPageToggleLabel",
       IDS_SETTINGS_AD_MEASUREMENT_PAGE_TOGGLE_LABEL},
      {"adMeasurementPageToggleSubLabel",
       IDS_SETTINGS_AD_MEASUREMENT_PAGE_TOGGLE_SUB_LABEL},
      {"adMeasurementPageEnabledHeading",
       IDS_SETTINGS_AD_MEASUREMENT_PAGE_ENABLED_HEADING},
      {"adMeasurementPageConsiderHeading",
       IDS_SETTINGS_AD_MEASUREMENT_PAGE_CONSIDER_HEADING},
      {"adMeasurementPageEnabledBullet1",
       IDS_SETTINGS_AD_MEASUREMENT_PAGE_ENABLED_BULLET_1},
      {"adMeasurementPageEnabledBullet2",
       IDS_SETTINGS_AD_MEASUREMENT_PAGE_ENABLED_BULLET_2},
      {"adMeasurementPageEnabledBullet3",
       IDS_SETTINGS_AD_MEASUREMENT_PAGE_ENABLED_BULLET_3},
      {"adMeasurementPageConsiderBullet1",
       IDS_SETTINGS_AD_MEASUREMENT_PAGE_CONSIDER_BULLET_1},
      {"adMeasurementPageConsiderBullet2",
       IDS_SETTINGS_AD_MEASUREMENT_PAGE_CONSIDER_BULLET_2},
      {"adMeasurementPageConsiderBullet3",
       IDS_SETTINGS_AD_MEASUREMENT_PAGE_CONSIDER_BULLET_3},

  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString("adPrivacyLearnMoreURL",
                         google_util::AppendGoogleLocaleParam(
                             GURL(chrome::kAdPrivacyLearnMoreURL),
                             g_browser_process->GetApplicationLocale())
                             .spec());
  html_source->AddString(
      "privacySandboxAdMeasurementDialogControlMeasurement",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_PRIVACY_SANDBOX_AD_MEASUREMENT_DIALOG_CONTROL_MEASUREMENT,
          base::ASCIIToUTF16(chrome::kChromeUIHistoryURL)));

  // Topics and fledge link to help center articles in their learn more dialog.
  html_source->AddString(
      "topicsPageLearnMoreBullet3",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_3,
          base::ASCIIToUTF16(google_util::AppendGoogleLocaleParam(
                                 GURL(chrome::kAdPrivacyLearnMoreURL),
                                 g_browser_process->GetApplicationLocale())
                                 .spec())));
  html_source->AddString(
      "fledgePageLearnMoreBullet3",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_FLEDGE_PAGE_LEARN_MORE_BULLET_3,
          base::ASCIIToUTF16(google_util::AppendGoogleLocaleParam(
                                 GURL(chrome::kAdPrivacyLearnMoreURL),
                                 g_browser_process->GetApplicationLocale())
                                 .spec())));

  // Topics and fledge both link to the cookies setting page and cross-link
  // each other in the footers.
  html_source->AddString(
      "topicsPageFooter",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_TOPICS_PAGE_FOOTER,
          base::ASCIIToUTF16(chrome::kChromeUIPrivacySandboxFledgeURL),
          base::ASCIIToUTF16(chrome::kChromeUICookieSettingsURL)));
  html_source->AddString(
      "fledgePageFooter",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_FLEDGE_PAGE_FOOTER,
          base::ASCIIToUTF16(chrome::kChromeUIPrivacySandboxTopicsURL),
          base::ASCIIToUTF16(chrome::kChromeUICookieSettingsURL)));
  html_source->AddBoolean(
      "firstPartySetsUIEnabled",
      base::FeatureList::IsEnabled(
          privacy_sandbox::kPrivacySandboxFirstPartySetsUI));

  html_source->AddString("bluetoothAdapterOffHelpURL",
                         google_util::AppendGoogleLocaleParam(
                             GURL(chrome::kBluetoothAdapterOffHelpURL),
                             g_browser_process->GetApplicationLocale())
                             .spec());

  html_source->AddString("chooserHidOverviewUrl",
                         google_util::AppendGoogleLocaleParam(
                             GURL(chrome::kChooserHidOverviewUrl),
                             g_browser_process->GetApplicationLocale())
                             .spec());

  html_source->AddString("chooserSerialOverviewUrl",
                         google_util::AppendGoogleLocaleParam(
                             GURL(chrome::kChooserSerialOverviewUrl),
                             g_browser_process->GetApplicationLocale())
                             .spec());

  html_source->AddString("chooserUsbOverviewURL",
                         google_util::AppendGoogleLocaleParam(
                             GURL(chrome::kChooserUsbOverviewURL),
                             g_browser_process->GetApplicationLocale())
                             .spec());
}

void AddPrivacyGuideStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"privacyGuideLabel", IDS_SETTINGS_PRIVACY_GUIDE_LABEL},
      {"privacyGuideSublabel", IDS_SETTINGS_PRIVACY_GUIDE_SUBLABEL},
      {"privacyGuidePromoHeader", IDS_SETTINGS_PRIVACY_GUIDE_PROMO_HEADER},
      {"privacyGuidePromoBody", IDS_SETTINGS_PRIVACY_GUIDE_PROMO_BODY},
      {"privacyGuidePromoStartButton",
       IDS_SETTINGS_PRIVACY_GUIDE_PROMO_START_BUTTON},
      {"privacyGuideBackToSettingsAriaLabel",
       IDS_SETTINGS_PRIVACY_GUIDE_BACK_TO_SETTINGS_ARIA_LABEL},
      {"privacyGuideBackToSettingsAriaRoleDescription",
       IDS_SETTINGS_PRIVACY_GUIDE_BACK_TO_SETTINGS_ARIA_ROLE_DESC},
      {"privacyGuideBackButton", IDS_SETTINGS_PRIVACY_GUIDE_BACK_BUTTON},
      {"privacyGuideSteps", IDS_SETTINGS_PRIVACY_GUIDE_STEPS},
      {"privacyGuideNextButton", IDS_SETTINGS_PRIVACY_GUIDE_NEXT_BUTTON},
      {"privacyGuideFeatureDescriptionHeader",
       IDS_SETTINGS_PRIVACY_GUIDE_FEATURE_DESCRIPTION_HEADER},
      {"privacyGuideThingsToConsider",
       IDS_SETTINGS_PRIVACY_GUIDE_THINGS_TO_CONSIDER},
      {"privacyGuideWelcomeCardHeader",
       IDS_SETTINGS_PRIVACY_GUIDE_WELCOME_CARD_HEADER},
      {"privacyGuideWelcomeCardSubHeader",
       IDS_SETTINGS_PRIVACY_GUIDE_WELCOME_CARD_SUB_HEADER},
      {"privacyGuideCompletionCardHeader",
       IDS_SETTINGS_PRIVACY_GUIDE_COMPLETION_CARD_HEADER},
      {"privacyGuideCompletionCardSubHeader",
       IDS_SETTINGS_PRIVACY_GUIDE_COMPLETION_CARD_SUB_HEADER},
      {"privacyGuideCompletionCardSubHeaderNoLinks",
       IDS_SETTINGS_PRIVACY_GUIDE_COMPLETION_CARD_SUB_HEADER_NO_LINKS},
      {"privacyGuideCompletionCardLeaveButton",
       IDS_SETTINGS_PRIVACY_GUIDE_COMPLETION_CARD_LEAVE_BUTTON},
      {"privacyGuideCompletionCardPrivacySandboxLabel",
       IDS_SETTINGS_PRIVACY_GUIDE_COMPLETION_CARD_PRIVACY_SANDBOX_LABEL},
      {"privacyGuideCompletionCardPrivacySandboxSubLabel",
       IDS_SETTINGS_PRIVACY_GUIDE_COMPLETION_CARD_PRIVACY_SANDBOX_SUB_LABEL},
      {"privacyGuideCompletionCardWaaLabel",
       IDS_SETTINGS_PRIVACY_GUIDE_COMPLETION_CARD_WAA_LABEL},
      {"privacyGuideCompletionCardWaaSubLabel",
       IDS_SETTINGS_PRIVACY_GUIDE_COMPLETION_CARD_WAA_SUB_LABEL},
      {"privacyGuideMsbbCardHeader",
       IDS_SETTINGS_PRIVACY_GUIDE_MSBB_CARD_HEADER},
      {"privacyGuideMsbbFeatureDescription1",
       IDS_SETTINGS_PRIVACY_GUIDE_MSBB_FEATURE_DESCRIPTION1},
      {"privacyGuideMsbbFeatureDescription2",
       IDS_SETTINGS_PRIVACY_GUIDE_MSBB_FEATURE_DESCRIPTION2},
      {"privacyGuideMsbbFeatureDescription3",
       IDS_SETTINGS_PRIVACY_GUIDE_MSBB_FEATURE_DESCRIPTION3},
      {"privacyGuideMsbbPrivacyDescription1",
       IDS_SETTINGS_PRIVACY_GUIDE_MSBB_PRIVACY_DESCRIPTION1},
      {"privacyGuideMsbbPrivacyDescription2",
       IDS_SETTINGS_PRIVACY_GUIDE_MSBB_PRIVACY_DESCRIPTION2},
      {"privacyGuideHistorySyncCardHeader",
       IDS_SETTINGS_PRIVACY_GUIDE_HISTORY_SYNC_CARD_HEADER},
      {"privacyGuideHistorySyncSettingLabel",
       IDS_SETTINGS_PRIVACY_GUIDE_HISTORY_SYNC_SETTING_LABEL},
      {"privacyGuideHistorySyncFeatureDescription1",
       IDS_SETTINGS_PRIVACY_GUIDE_HISTORY_SYNC_FEATURE_DESCRIPTION1},
      {"privacyGuideHistorySyncFeatureDescription2",
       IDS_SETTINGS_PRIVACY_GUIDE_HISTORY_SYNC_FEATURE_DESCRIPTION2},
      {"privacyGuideHistorySyncPrivacyDescription1",
       IDS_SETTINGS_PRIVACY_GUIDE_HISTORY_SYNC_PRIVACY_DESCRIPTION1},
      {"privacyGuideCookiesCardHeader",
       IDS_SETTINGS_PRIVACY_GUIDE_COOKIES_CARD_HEADER},
      {"privacyGuideCookiesCardBlockTpcIncognitoSubheader",
       IDS_SETTINGS_PRIVACY_GUIDE_COOKIES_CARD_BLOCK_TPC_INCOGNITO_SUBHEADER},
      {"privacyGuideCookiesCardBlockTpcIncognitoFeatureDescription1",
       IDS_SETTINGS_PRIVACY_GUIDE_COOKIES_CARD_BLOCK_TPC_INCOGNITO_FEATURE_DESCRIPTION1},
      {"privacyGuideCookiesCardBlockTpcIncognitoFeatureDescription2",
       IDS_SETTINGS_PRIVACY_GUIDE_COOKIES_CARD_BLOCK_TPC_INCOGNITO_FEATURE_DESCRIPTION2},
      {"privacyGuideCookiesCardBlockTpcIncognitoPrivacyDescription1",
       IDS_SETTINGS_PRIVACY_GUIDE_COOKIES_CARD_BLOCK_TPC_INCOGNITO_PRIVACY_DESCRIPTION1},
      {"privacyGuideCookiesCardBlockTpcIncognitoPrivacyDescription2",
       IDS_SETTINGS_PRIVACY_GUIDE_COOKIES_CARD_BLOCK_TPC_INCOGNITO_PRIVACY_DESCRIPTION2},
      {"privacyGuideCookiesCardBlockTpcSubheader",
       IDS_SETTINGS_PRIVACY_GUIDE_COOKIES_CARD_BLOCK_TPC_SUBHEADER},
      {"privacyGuideCookiesCardBlockTpcFeatureDescription1",
       IDS_SETTINGS_PRIVACY_GUIDE_COOKIES_CARD_BLOCK_TPC_FEATURE_DESCRIPTION1},
      {"privacyGuideCookiesCardBlockTpcFeatureDescription2",
       IDS_SETTINGS_PRIVACY_GUIDE_COOKIES_CARD_BLOCK_TPC_FEATURE_DESCRIPTION2},
      {"privacyGuideCookiesCardBlockTpcPrivacyDescription1",
       IDS_SETTINGS_PRIVACY_GUIDE_COOKIES_CARD_BLOCK_TPC_PRIVACY_DESCRIPTION1},
      {"privacyGuideSafeBrowsingCardHeader",
       IDS_SETTINGS_PRIVACY_GUIDE_SAFE_BROWSING_CARD_HEADER},
      {"privacyGuideSafeBrowsingCardEnhancedProtectionPrivacyDescription1",
       IDS_SETTINGS_PRIVACY_GUIDE_SAFE_BROWSING_CARD_ENHANCED_PROTECTION_PRIVACY_DESCRIPTION1},
      {"privacyGuideSafeBrowsingCardEnhancedProtectionPrivacyDescription2",
       IDS_SETTINGS_PRIVACY_GUIDE_SAFE_BROWSING_CARD_ENHANCED_PROTECTION_PRIVACY_DESCRIPTION2},
      {"privacyGuideSafeBrowsingCardEnhancedProtectionPrivacyDescription3",
       IDS_SETTINGS_PRIVACY_GUIDE_SAFE_BROWSING_CARD_ENHANCED_PROTECTION_PRIVACY_DESCRIPTION3},
      {"privacyGuideSafeBrowsingCardStandardProtectionFeatureDescription1",
       IDS_SETTINGS_PRIVACY_GUIDE_SAFE_BROWSING_CARD_STANDARD_PROTECTION_FEATURE_DESCRIPTION1},
      {"privacyGuideSafeBrowsingCardStandardProtectionFeatureDescription2",
       IDS_SETTINGS_PRIVACY_GUIDE_SAFE_BROWSING_CARD_STANDARD_PROTECTION_FEATURE_DESCRIPTION2},
      {"privacyGuideSafeBrowsingCardStandardProtectionPrivacyDescription1",
       IDS_SETTINGS_PRIVACY_GUIDE_SAFE_BROWSING_CARD_STANDARD_PROTECTION_PRIVACY_DESCRIPTION1},
      {"privacyGuideSearchSuggestionsCardHeader",
       IDS_SETTINGS_PRIVACY_GUIDE_SEARCH_SUGGESTIONS_CARD_HEADER},
      {"privacyGuideSearchSuggestionsFeatureDescription1",
       IDS_SETTINGS_PRIVACY_SEARCH_SUGGESTIONS_FEATURE_DESCRIPTION1},
      {"privacyGuideSearchSuggestionsPrivacyDescription1",
       IDS_SETTINGS_PRIVACY_SEARCH_SUGGESTIONS_PRIVACY_DESCRIPTION1},
      {"privacyGuideSearchSuggestionsPrivacyDescription2",
       IDS_SETTINGS_PRIVACY_SEARCH_SUGGESTIONS_PRIVACY_DESCRIPTION2},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

void AddSafetyCheckStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"safetyCheckSectionTitle", IDS_SETTINGS_SAFETY_CHECK_SECTION_TITLE},
      {"safetyCheckParentPrimaryLabelBefore",
       IDS_SETTINGS_SAFETY_CHECK_PARENT_PRIMARY_LABEL_BEFORE},
      {"safetyCheckRunning", IDS_SETTINGS_SAFETY_CHECK_RUNNING},
      {"safetyCheckParentPrimaryLabelAfter",
       IDS_SETTINGS_SAFETY_CHECK_PARENT_PRIMARY_LABEL_AFTER},
      {"safetyCheckAriaLiveRunning",
       IDS_SETTINGS_SAFETY_CHECK_ARIA_LIVE_RUNNING},
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
      {"safetyCheckUpdatesButtonAriaLabel",
       IDS_UPDATE_RECOMMENDED_DIALOG_TITLE},
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
      {"safetyCheckNotificationPermissionReviewIgnoredToastLabel",
       IDS_SETTINGS_SAFETY_CHECK_NOTIFICATION_PERMISSION_REVIEW_IGNORED_TOAST_LABEL},
      {"safetyCheckNotificationPermissionReviewBlockedToastLabel",
       IDS_SETTINGS_SAFETY_CHECK_NOTIFICATION_PERMISSION_REVIEW_BLOCKED_TOAST_LABEL},
      {"safetyCheckNotificationPermissionReviewResetToastLabel",
       IDS_SETTINGS_SAFETY_CHECK_NOTIFICATION_PERMISSION_REVIEW_RESET_TOAST_LABEL},
      {"safetyCheckNotificationPermissionReviewDontAllowLabel",
       IDS_SETTINGS_SAFETY_CHECK_REVIEW_NOTIFICATION_PERMISSIONS_DONT_ALLOW_LABEL},
      {"safetyCheckNotificationPermissionReviewDontAllowAriaLabel",
       IDS_SETTINGS_SAFETY_CHECK_REVIEW_NOTIFICATION_PERMISSIONS_DONT_ALLOW_ARIA_LABEL},
      {"safetyCheckNotificationPermissionReviewIgnoreLabel",
       IDS_SETTINGS_SAFETY_CHECK_REVIEW_NOTIFICATION_PERMISSIONS_IGNORE_LABEL},
      {"safetyCheckNotificationPermissionReviewIgnoreAriaLabel",
       IDS_SETTINGS_SAFETY_CHECK_REVIEW_NOTIFICATION_PERMISSIONS_IGNORE_ARIA_LABEL},
      {"safetyCheckNotificationPermissionReviewResetLabel",
       IDS_SETTINGS_SAFETY_CHECK_REVIEW_NOTIFICATION_PERMISSIONS_RESET_LABEL},
      {"safetyCheckNotificationPermissionReviewResetAriaLabel",
       IDS_SETTINGS_SAFETY_CHECK_REVIEW_NOTIFICATION_PERMISSIONS_RESET_ARIA_LABEL},
      {"safetyCheckNotificationPermissionReviewMoreActionsAriaLabel",
       IDS_SETTINGS_SAFETY_CHECK_REVIEW_NOTIFICATION_PERMISSIONS_MORE_ACTIONS_ARIA_LABEL},
      {"safetyCheckNotificationPermissionReviewUndo",
       IDS_SETTINGS_SAFETY_CHECK_TOAST_UNDO_BUTTON_LABEL},
      {"safetyCheckNotificationPermissionReviewDoneLabel",
       IDS_SETTINGS_SAFETY_CHECK_SITE_PERMISSIONS_REVIEW_DONE_LABEL},
      {"safetyCheckNotificationPermissionReviewBlockAllLabel",
       IDS_SETTINGS_SAFETY_CHECK_NOTIFICATION_PERMISSION_REVIEW_BLOCK_ALL_LABEL},
      {"safetyCheckUnusedSitePermissionsHeaderAriaLabel",
       IDS_SETTINGS_SAFETY_CHECK_UNUSED_SITE_PERMISSIONS_HEADER_ARIA_LABEL},
      {"safetyCheckNotificationPermissionReviewButtonAriaLabel",
       IDS_SETTINGS_SAFETY_CHECK_NOTIFICATION_PERMISSIONS_REVIEW_BUTTON_ARIA_LABEL},
      {"safetyCheckUnusedSitePermissionsAllowAgainAriaLabel",
       IDS_SETTINGS_SAFETY_CHECK_UNUSED_SITE_PERMISSIONS_ALLOW_AGAIN_ARIA_LABEL},
      {"safetyCheckUnusedSitePermissionsAllowAgainLabel",
       IDS_SETTINGS_SAFETY_CHECK_UNUSED_SITE_PERMISSIONS_ALLOW_AGAIN_LABEL},
      {"safetyCheckUnusedSitePermissionsDoneLabel",
       IDS_SETTINGS_SAFETY_CHECK_SITE_PERMISSIONS_REVIEW_DONE_LABEL},
      {"safetyCheckUnusedSitePermissionsGotItLabel", IDS_SETTINGS_GOT_IT},
      {"safetyCheckUnusedSitePermissionsRemovedOnePermissionLabel",
       IDS_SETTINGS_SAFETY_CHECK_UNUSED_SITE_PERMISSIONS_REMOVED_ONE_PERMISSION_LABEL},
      {"safetyCheckUnusedSitePermissionsRemovedTwoPermissionsLabel",
       IDS_SETTINGS_SAFETY_CHECK_UNUSED_SITE_PERMISSIONS_REMOVED_TWO_PERMISSIONS_LABEL},
      {"safetyCheckUnusedSitePermissionsRemovedThreePermissionsLabel",
       IDS_SETTINGS_SAFETY_CHECK_UNUSED_SITE_PERMISSIONS_REMOVED_THREE_PERMISSIONS_LABEL},
      {"safetyCheckUnusedSitePermissionsRemovedFourOrMorePermissionsLabel",
       IDS_SETTINGS_SAFETY_CHECK_UNUSED_SITE_PERMISSIONS_REMOVED_FOUR_OR_MORE_PERMISSIONS_LABEL},
      {"safetyCheckUnusedSitePermissionsToastLabel",
       IDS_SETTINGS_SAFETY_CHECK_UNUSED_SITE_PERMISSIONS_TOAST_LABEL},
      {"safetyCheckUnusedSitePermissionsUndoLabel",
       IDS_SETTINGS_SAFETY_CHECK_TOAST_UNDO_BUTTON_LABEL},
      {"safetyCheckUnusedSitePermissionsSettingLabel",
       IDS_SETTINGS_SAFETY_CHECK_UNUSED_SITE_PERMISSIONS_SETTING_LABEL},
      {"safetyCheckUnusedSitePermissionsSettingSublabel",
       IDS_SETTINGS_SAFETY_CHECK_UNUSED_SITE_PERMISSIONS_SETTING_SUBLABEL},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

void AddSafetyHubStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"safetyHub", IDS_SETTINGS_SAFETY_HUB},
      {"safetyHubEntryPointNothingToDo",
       IDS_SETTINGS_SAFETY_HUB_ENTRY_POINT_NOTHING_TO_DO},
      {"safetyHubEntryPointButton", IDS_SETTINGS_SAFETY_HUB_ENTRY_POINT_BUTTON},
      {"safetyHubPageCardSectionHeader",
       IDS_SETTINGS_SAFETY_HUB_PAGE_CARD_SECTION_HEADER},
      {"safetyHubPageModuleSectionHeader",
       IDS_SETTINGS_SAFETY_HUB_PAGE_MODULE_SECTION_HEADER},
      {"safetyHubPageUserEduSectionHeader",
       IDS_SETTINGS_SAFETY_HUB_PAGE_USER_EDU_SECTION_HEADER},
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
      {"searchEnginesManageSiteSearch",
       IDS_SETTINGS_SEARCH_MANAGE_SEARCH_ENGINES_AND_SITE_SEARCH},
      {"searchPageTitle", IDS_SETTINGS_SEARCH},
      {"searchExplanation", IDS_SETTINGS_SEARCH_EXPLANATION},
      {"searchExplanationLearnMoreA11yLabel",
       IDS_SETTINGS_SEARCH_EXPLANATION_ACCESSIBILITY_LABEL},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString("searchExplanationLearnMoreURL",
                         base::ASCIIToUTF16(chrome::kOmniboxLearnMoreURL));
}

void AddSearchEnginesStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"searchEnginesPageExplanation",
       IDS_SETTINGS_SEARCH_ENGINES_PAGE_EXPLANATION},
      {"searchEnginesAddSearchEngine",
       IDS_SETTINGS_SEARCH_ENGINES_ADD_SEARCH_ENGINE},
      {"searchEnginesEditSearchEngine",
       IDS_SETTINGS_SEARCH_ENGINES_EDIT_SEARCH_ENGINE},
      {"searchEnginesDeleteConfirmationTitle",
       IDS_SETTINGS_SEARCH_ENGINES_DELETE_CONFIRMATION_TITLE},
      {"searchEnginesDeleteConfirmationDescription",
       IDS_SETTINGS_SEARCH_ENGINES_DELETE_CONFIRMATION_DESCRIPTION},
      {"searchEngines", IDS_SETTINGS_SEARCH_ENGINES},
      {"searchEnginesSearchEngines",
       IDS_SETTINGS_SEARCH_ENGINES_SEARCH_ENGINES},
      {"searchEnginesSearchEnginesExplanation",
       IDS_SETTINGS_SEARCH_ENGINES_SEARCH_ENGINES_EXPLANATION},
      {"searchEnginesSiteSearch", IDS_SETTINGS_SEARCH_ENGINES_SITE_SEARCH},
      {"searchEnginesSiteSearchExplanation",
       IDS_SETTINGS_SEARCH_ENGINES_SITE_SEARCH_EXPLANATION},
      {"searchEnginesNoSitesAdded", IDS_SETTINGS_SEARCH_ENGINES_NO_SITES_ADDED},
      {"searchEnginesInactiveShortcuts",
       IDS_SETTINGS_SEARCH_ENGINES_INACTIVE_SHORTCUTS},
      {"searchEnginesNoOtherEngines",
       IDS_SETTINGS_SEARCH_ENGINES_NO_OTHER_ENGINES},
      {"searchEnginesExtension", IDS_SETTINGS_SEARCH_ENGINES_EXTENSION_ENGINES},
      {"searchEnginesExtensionExplanation",
       IDS_SETTINGS_SEARCH_ENGINES_EXTENSION_ENGINES_EXPLANATION},
      {"searchEnginesSearch", IDS_SETTINGS_SEARCH_ENGINES_SEARCH},
      {"searchEnginesSearchEngine", IDS_SETTINGS_SEARCH_ENGINES_SEARCH_ENGINE},
      {"searchEnginesSiteOrPage", IDS_SETTINGS_SEARCH_ENGINES_SITE_OR_PAGE},
      {"searchEnginesShortcut", IDS_SETTINGS_SEARCH_ENGINES_SHORTCUT},
      {"searchEnginesQueryURL", IDS_SETTINGS_SEARCH_ENGINES_QUERY_URL},
      {"searchEnginesQueryURLExplanation",
       IDS_SETTINGS_SEARCH_ENGINES_QUERY_URL_EXPLANATION},
      {"searchEnginesMakeDefault", IDS_SETTINGS_SEARCH_ENGINES_MAKE_DEFAULT},
      {"searchEnginesActivate", IDS_SETTINGS_SEARCH_ENGINES_ACTIVATE},
      {"searchEnginesDeactivate", IDS_SETTINGS_SEARCH_ENGINES_DEACTIVATE},
      {"searchEnginesManageExtension",
       IDS_SETTINGS_SEARCH_ENGINES_MANAGE_EXTENSION},
      {"searchEnginesKeyboardShortcutsTitle",
       IDS_SETTINGS_SEARCH_ENGINES_KEYBOARD_SHORTCUTS_TITLE},
      {"searchEnginesKeyboardShortcutsDescription",
       IDS_SETTINGS_SEARCH_ENGINES_KEYBOARD_SHORTCUTS_DESCRIPTION},
      {"searchEnginesKeyboardShortcutsSpaceOrTab",
       IDS_SETTINGS_SEARCH_ENGINES_KEYBOARD_SHORTCUTS_SPACE_OR_TAB},
      {"searchEnginesKeyboardShortcutsTab",
       IDS_SETTINGS_SEARCH_ENGINES_KEYBOARD_SHORTCUTS_TAB},
      {"searchEnginesAdditionalSites",
       IDS_SETTINGS_SEARCH_ENGINES_ADDITIONAL_SITES},
      {"searchEnginesAdditionalInactiveSites",
       IDS_SETTINGS_SEARCH_ENGINES_ADDITIONAL_INACTIVE_SITES},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

void AddSiteSettingsStrings(content::WebUIDataSource* html_source,
                            Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"addSite", IDS_SETTINGS_ADD_SITE},
    {"addSiteTitle", IDS_SETTINGS_ADD_SITE_TITLE},
    {"addSitesTitle", IDS_SETTINGS_ADD_SITES_TITLE},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"androidSmsNote", IDS_SETTINGS_ANDROID_SMS_NOTE},
#endif
    {"embeddedOnAnyHost", IDS_SETTINGS_EXCEPTIONS_EMBEDDED_ON_ANY_HOST},
    {"embeddedOnHost", IDS_SETTINGS_EXCEPTIONS_EMBEDDED_ON_HOST},
    {"editSiteTitle", IDS_SETTINGS_EDIT_SITE_TITLE},
    {"noBluetoothDevicesFound", IDS_SETTINGS_NO_BLUETOOTH_DEVICES_FOUND},
    {"noHidDevicesFound", IDS_SETTINGS_NO_HID_DEVICES_FOUND},
    {"noSerialPortsFound", IDS_SETTINGS_NO_SERIAL_PORTS_FOUND},
    {"noUsbDevicesFound", IDS_SETTINGS_NO_USB_DEVICES_FOUND},
    {"resetBluetoothConfirmation", IDS_SETTINGS_RESET_BLUETOOTH_CONFIRMATION},
    {"resetHidConfirmation", IDS_SETTINGS_RESET_HID_CONFIRMATION},
    {"resetSerialPortsConfirmation",
     IDS_SETTINGS_RESET_SERIAL_PORTS_CONFIRMATION},
    {"resetUsbConfirmation", IDS_SETTINGS_RESET_USB_CONFIRMATION},
    {"siteSettingsRecentPermissionsSectionLabel",
     IDS_SETTINGS_SITE_SETTINGS_RECENT_ACTIVITY},
    {"siteSettingsCategoryCamera", IDS_SITE_SETTINGS_TYPE_CAMERA},
    {"siteSettingsCameraLabel", IDS_SITE_SETTINGS_TYPE_CAMERA},
    {"thirdPartyCookiesPageTitle", IDS_SETTINGS_THIRD_PARTY_COOKIES_PAGE_TITLE},
    {"thirdPartyCookiesLinkRowLabel",
     IDS_SETTINGS_THIRD_PARTY_COOKIES_LINK_ROW_LABEL},
    {"thirdPartyCookiesLinkRowSublabelEnabled",
     IDS_SETTINGS_THIRD_PARTY_COOKIES_LINK_ROW_SUB_LABEL_ENABLED},
    {"thirdPartyCookiesLinkRowSublabelDisabledIncognito",
     IDS_SETTINGS_THIRD_PARTY_COOKIES_LINK_ROW_SUB_LABEL_DISABLED_INCOGNITO},
    {"thirdPartyCookiesLinkRowSublabelDisabled",
     IDS_SETTINGS_THIRD_PARTY_COOKIES_LINK_ROW_SUB_LABEL_DISABLED},
    {"thirdPartyCookiesPageAllowRadioLabel",
     IDS_SETTINGS_THIRD_PARTY_COOKIES_PAGE_ALLOW_RADIO_LABEL},
    {"thirdPartyCookiesPageAllowExpandA11yLabel",
     IDS_SETTINGS_THIRD_PARTY_COOKIES_PAGE_ALLOW_EXPAND_A11Y_LABEL},
    {"thirdPartyCookiesPageAllowBulOne",
     IDS_SETTINGS_THIRD_PARTY_COOKIES_PAGE_ALLOW_BULLET_1},
    {"thirdPartyCookiesPageAllowBulTwo",
     IDS_SETTINGS_THIRD_PARTY_COOKIES_PAGE_ALLOW_BULLET_2},
    {"thirdPartyCookiesPageBlockIncognitoRadioLabel",
     IDS_SETTINGS_THIRD_PARTY_COOKIES_PAGE_BLOCK_INCOGNITO_RADIO_LABEL},
    {"thirdPartyCookiesPageBlockIncognitoExpandA11yLabel",
     IDS_SETTINGS_THIRD_PARTY_COOKIES_PAGE_BLOCK_INCOGNITO_EXPAND_A11Y_LABEL},
    {"thirdPartyCookiesPageBlockIncognitoBulOne",
     IDS_SETTINGS_THIRD_PARTY_COOKIES_PAGE_BLOCK_INCOGNITO_BULLET_1},
    {"thirdPartyCookiesPageBlockIncognitoBulTwo",
     IDS_SETTINGS_THIRD_PARTY_COOKIES_PAGE_BLOCK_INCOGNITO_BULLET_2},
    {"thirdPartyCookiesPageBlockRadioLabel",
     IDS_SETTINGS_THIRD_PARTY_COOKIES_PAGE_BLOCK_RADIO_LABEL},
    {"thirdPartyCookiesPageBlockExpandA11yLabel",
     IDS_SETTINGS_THIRD_PARTY_COOKIES_PAGE_BLOCK_EXPAND_A11Y_LABEL},
    {"thirdPartyCookiesPageBlockBulOne",
     IDS_SETTINGS_THIRD_PARTY_COOKIES_PAGE_BLOCK_BULLET_1},
    {"thirdPartyCookiesPageBlockBulTwo",
     IDS_SETTINGS_THIRD_PARTY_COOKIES_PAGE_BLOCK_BULLET_2},

    {"thirdPartyCookiesPageDescription",
     IDS_SETTINGS_THIRD_PARTY_COOKIES_PAGE_DESCRIPTION},
    {"thirdPartyCookiesPageDefaultBehaviorHeading",
     IDS_SETTINGS_THIRD_PARTY_COOKIES_PAGE_DEFAULT_BEHAVIOR_HEADING},
    {"thirdPartyCookiesPageDefaultBehaviorDescription",
     IDS_SETTINGS_THIRD_PARTY_COOKIES_PAGE_DEFAULT_BEHAVIOR_DESCRIPTION},
    {"thirdPartyCookiesPageCustomizedBehaviorHeading",
     IDS_SETTINGS_THIRD_PARTY_COOKIES_PAGE_CUSTOMIZED_BEHAVIOR_HEADING},
    {"thirdPartyCookiesPageCustomizedBehaviorDescription",
     IDS_SETTINGS_THIRD_PARTY_COOKIES_PAGE_CUSTOMIZED_BEHAVIOR_DESCRIPTION},
    {"thirdPartyCookiesPageAllowExceptionsSubHeading",
     IDS_SETTINGS_THIRD_PARTY_COOKIES_PAGE_ALLOW_EXCEPTIONS_SUB_HEADING},
    {"cookiePageTitle", IDS_SETTINGS_COOKIES_PAGE},
    {"cookiePageGeneralControls", IDS_SETTINGS_COOKIES_CONTROLS},
    {"cookiePageAllowAll", IDS_SETTINGS_COOKIES_ALLOW_ALL},
    {"cookiePageAllowAllExpandA11yLabel",
     IDS_SETTINGS_COOKIES_ALLOW_ALL_EXPAND_A11Y_LABEL},
    {"cookiePageAllowAllBulOne", IDS_SETTINGS_COOKIES_ALLOW_ALL_BULLET_ONE},
    {"cookiePageAllowAllBulTwo", IDS_SETTINGS_COOKIES_ALLOW_ALL_BULLET_TWO},
    {"cookiePageBlockThirdIncognito",
     IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_INCOGNITO},
    {"cookiePageBlockThirdIncognitoExpandA11yLabel",
     IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_INCOGNITO_EXPAND_A11Y_LABEL},
    {"cookiePageBlockThirdIncognitoBulOne",
     IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_INCOGNITO_BULLET_ONE},
    {"cookiePageBlockThirdIncognitoBulTwo",
     IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_INCOGNITO_BULLET_TWO},
    {"cookiePageBlockThirdIncognitoBulTwoFps",
     IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_INCOGNITO_BULLET_TWO_FPS},
    {"cookiePageBlockThird", IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY},
    {"cookiePageBlockThirdExpandA11yLabel",
     IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_EXPAND_A11Y_LABEL},
    {"cookiePageBlockThirdBulOne",
     IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_BULLET_ONE},
    {"cookiePageBlockThirdBulTwo",
     IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_BULLET_TWO},
    {"cookiePageBlockAll", IDS_SETTINGS_COOKIES_BLOCK_ALL},
    {"cookiePageBlockAllExpandA11yLabel",
     IDS_SETTINGS_COOKIES_BLOCK_ALL_EXPAND_A11Y_LABEL},
    {"cookiePageBlockAllBulOne", IDS_SETTINGS_COOKIES_BLOCK_ALL_BULLET_ONE},
    {"cookiePageBlockAllBulTwo", IDS_SETTINGS_COOKIES_BLOCK_ALL_BULLET_TWO},
    {"cookiePageBlockAllBulThree", IDS_SETTINGS_COOKIES_BLOCK_ALL_BULLET_THREE},
    {"cookiePageFpsLabel", IDS_SETTINGS_COOKIES_FIRST_PARTY_SETS_TOGGLE_LABEL},
    {"cookiePageFpsSubLabel",
     IDS_SETTINGS_COOKIES_FIRST_PARTY_SETS_TOGGLE_SUB_LABEL},
    {"cookiePageClearOnExit", IDS_SETTINGS_COOKIES_CLEAR_ON_EXIT},
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    {"cookiePageClearOnExitDesc", IDS_SETTINGS_COOKIES_CLEAR_ON_EXIT_DESC},
#endif
    {"cookiePageAllSitesLink", IDS_SETTINGS_COOKIES_ALL_SITES_LINK},
    {"cookiePageAllowExceptions", IDS_SETTINGS_COOKIES_ALLOW_EXCEPTIONS},
    {"cookiePageBlockExceptions", IDS_SETTINGS_COOKIES_BLOCK_EXCEPTIONS},
    {"cookiePageSessionOnlyExceptions",
     IDS_SETTINGS_COOKIES_SESSION_ONLY_EXCEPTIONS},
    {"siteSettingsCategoryFederatedIdentityApi",
     IDS_SITE_SETTINGS_TYPE_FEDERATED_IDENTITY_API},
    {"siteSettingsCategoryHandlers", IDS_SITE_SETTINGS_TYPE_HANDLERS},
    {"siteSettingsCategoryImages", IDS_SITE_SETTINGS_TYPE_IMAGES},
    {"siteSettingsCategoryInsecureContent",
     IDS_SITE_SETTINGS_TYPE_INSECURE_CONTENT},
    {"siteSettingsCategoryLocation", IDS_SITE_SETTINGS_TYPE_LOCATION},
    {"siteSettingsCategoryJavascript", IDS_SITE_SETTINGS_TYPE_JAVASCRIPT},
    {"siteSettingsCategoryMicrophone", IDS_SITE_SETTINGS_TYPE_MIC},
    {"siteSettingsMicrophoneLabel", IDS_SITE_SETTINGS_TYPE_MIC},
    {"siteSettingsCategoryNotifications", IDS_SITE_SETTINGS_TYPE_NOTIFICATIONS},
    {"siteSettingsCategoryPopups", IDS_SITE_SETTINGS_TYPE_POPUPS_REDIRECTS},
    {"siteSettingsCategoryZoomLevels", IDS_SITE_SETTINGS_TYPE_ZOOM_LEVELS},
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
    {"siteSettingsFileSystemSiteListEditHeader",
     IDS_SETTINGS_SITE_SETTINGS_FILE_SYSTEM_SITE_LIST_EDIT_HEADER},
    {"siteSettingsFileSystemSiteListRemoveGrantLabel",
     IDS_SETTINGS_SITE_SETTINGS_FILE_SYSTEM_SITE_LIST_REMOVE_GRANT_LABEL},
    {"siteSettingsFileSystemSiteListRemoveGrants",
     IDS_SETTINGS_SITE_SETTINGS_FILE_SYSTEM_SITE_LIST_REMOVE_GRANTS},
    {"siteSettingsFileSystemSiteListViewHeader",
     IDS_SETTINGS_SITE_SETTINGS_FILE_SYSTEM_SITE_LIST_VIEW_HEADER},
    {"siteSettingsFileSystemSiteListViewSiteDetails",
     IDS_SETTINGS_SITE_SETTINGS_FILE_SYSTEM_SITE_LIST_VIEW_SITE_DETAILS},
    {"siteSettingsSiteEntryPartitionedLabel",
     IDS_SETTINGS_SITE_SETTINGS_SITE_ENTRY_PARTITIONED_LABEL},
    {"siteSettingsSiteRepresentationSeparator",
     IDS_SETTINGS_SITE_SETTINGS_SITE_REPRESENTATION_SEPARATOR},
    {"siteSettingsAppProtocolHandlers",
     IDS_SETTINGS_SITE_SETTINGS_APP_PROTOCOL_HANDLERS},
    {"siteSettingsAppAllowedProtocolHandlersDescription",
     IDS_SETTINGS_SITE_SETTINGS_APP_ALLOWED_PROTOCOL_HANDLERS_DESCRIPTION},
    {"siteSettingsAppDisallowedProtocolHandlersDescription",
     IDS_SETTINGS_SITE_SETTINGS_APP_DISALLOWED_PROTOCOL_HANDLERS_DESCRIPTION},
    {"siteSettingsAutomaticDownloads",
     IDS_SITE_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS},
    {"siteSettingsAutomaticDownloadsMidSentence",
     IDS_SITE_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS_MID_SENTENCE},
    {"siteSettingsBackgroundSync", IDS_SITE_SETTINGS_TYPE_BACKGROUND_SYNC},
    {"siteSettingsBackgroundSyncMidSentence",
     IDS_SITE_SETTINGS_TYPE_BACKGROUND_SYNC_MID_SENTENCE},
    {"siteSettingsCamera", IDS_SITE_SETTINGS_TYPE_CAMERA},
    {"siteSettingsCameraMidSentence",
     IDS_SITE_SETTINGS_TYPE_CAMERA_MID_SENTENCE},
    {"siteSettingsClipboard", IDS_SITE_SETTINGS_TYPE_CLIPBOARD},
    {"siteSettingsClipboardMidSentence",
     IDS_SITE_SETTINGS_TYPE_CLIPBOARD_MID_SENTENCE},
    {"siteSettingsCookies", IDS_SITE_SETTINGS_TYPE_COOKIES},
    {"siteSettingsCookiesMidSentence",
     IDS_SITE_SETTINGS_TYPE_COOKIES_MID_SENTENCE},
    {"siteSettingsHandlers", IDS_SITE_SETTINGS_TYPE_HANDLERS},
    {"siteSettingsHandlersMidSentence",
     IDS_SITE_SETTINGS_TYPE_HANDLERS_MID_SENTENCE},
    {"siteSettingsLocation", IDS_SITE_SETTINGS_TYPE_LOCATION},
    {"siteSettingsLocationMidSentence",
     IDS_SITE_SETTINGS_TYPE_LOCATION_MID_SENTENCE},
    {"siteSettingsMic", IDS_SITE_SETTINGS_TYPE_MIC},
    {"siteSettingsMicMidSentence", IDS_SITE_SETTINGS_TYPE_MIC_MID_SENTENCE},
    {"siteSettingsNotifications", IDS_SITE_SETTINGS_TYPE_NOTIFICATIONS},
    {"siteSettingsNotificationsMidSentence",
     IDS_SITE_SETTINGS_TYPE_NOTIFICATIONS_MID_SENTENCE},
    {"siteSettingsImages", IDS_SITE_SETTINGS_TYPE_IMAGES},
    {"siteSettingsImagesMidSentence",
     IDS_SITE_SETTINGS_TYPE_IMAGES_MID_SENTENCE},
    {"siteSettingsInsecureContent", IDS_SITE_SETTINGS_TYPE_INSECURE_CONTENT},
    {"siteSettingsInsecureContentMidSentence",
     IDS_SITE_SETTINGS_TYPE_INSECURE_CONTENT_MID_SENTENCE},
    {"siteSettingsInsecureContentBlock",
     IDS_SETTINGS_SITE_SETTINGS_INSECURE_CONTENT_BLOCK},
    {"siteSettingsJavascript", IDS_SITE_SETTINGS_TYPE_JAVASCRIPT},
    {"siteSettingsJavascriptMidSentence",
     IDS_SITE_SETTINGS_TYPE_JAVASCRIPT_MID_SENTENCE},
    {"siteSettingsSound", IDS_SITE_SETTINGS_TYPE_SOUND},
    {"siteSettingsSoundMidSentence", IDS_SITE_SETTINGS_TYPE_SOUND_MID_SENTENCE},
    {"siteSettingsPdfDocuments", IDS_SITE_SETTINGS_TYPE_PDF_DOCUMENTS},
    {"siteSettingsPdfDownloadPdfs",
     IDS_SETTINGS_SITE_SETTINGS_PDF_DOWNLOAD_PDFS},
    {"siteSettingsProtectedContent", IDS_SITE_SETTINGS_TYPE_PROTECTED_MEDIA_ID},
    {"siteSettingsProtectedContentMidSentence",
     IDS_SITE_SETTINGS_TYPE_PROTECTED_MEDIA_ID_MID_SENTENCE},
    {"siteSettingsProtectedContentIdentifiers",
     IDS_SITE_SETTINGS_TYPE_PROTECTED_MEDIA_ID},
    {"siteSettingsProtectedContentDescription",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT_DESCRIPTION},
    {"siteSettingsProtectedContentAllowed",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT_ALLOWED},
    {"siteSettingsProtectedContentBlocked",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT_BLOCKED},
    {"siteSettingsProtectedContentBlockedSubLabel",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT_BLOCKED_SUB_LABEL},
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    {"siteSettingsProtectedContentIdentifiersExplanation",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT_IDENTIFIERS_EXPLANATION},
    {"siteSettingsProtectedContentIdentifiersAllowed",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT_IDENTIFIERS_ALLOWED},
    {"siteSettingsProtectedContentIdentifiersBlocked",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT_IDENTIFIERS_BLOCKED},
    {"siteSettingsProtectedContentIdentifiersBlockedSubLabel",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT_IDENTIFIERS_BLOCKED_SUB_LABEL},
    {"siteSettingsProtectedContentIdentifiersAllowedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT_IDENTIFIERS_ALLOWED_EXCEPTIONS},
    {"siteSettingsProtectedContentIdentifiersBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT_IDENTIFIERS_BLOCKED_EXCEPTIONS},
#endif
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {"siteSettingsProtectedContentIdentifiersAllowedSubLabel",
     IDS_SETTINGS_SITE_SETTINGS_PROTECTED_CONTENT_IDENTIFIERS_ALLOWED_SUB_LABEL},
#endif
    {"siteSettingsPopups", IDS_SITE_SETTINGS_TYPE_POPUPS_REDIRECTS},
    {"siteSettingsPopupsMidSentence",
     IDS_SITE_SETTINGS_TYPE_POPUPS_REDIRECTS_MID_SENTENCE},
    {"siteSettingsHidDevices", IDS_SITE_SETTINGS_TYPE_HID_DEVICES},
    {"siteSettingsHidDevicesMidSentence",
     IDS_SITE_SETTINGS_TYPE_HID_DEVICES_MID_SENTENCE},
    {"siteSettingsHidDevicesAsk", IDS_SETTINGS_SITE_SETTINGS_HID_DEVICES_ASK},
    {"siteSettingsHidDevicesBlock",
     IDS_SETTINGS_SITE_SETTINGS_HID_DEVICES_BLOCK},
    {"siteSettingsMidiDevices", IDS_SITE_SETTINGS_TYPE_MIDI_SYSEX},
    {"siteSettingsMidiDevicesMidSentence",
     IDS_SITE_SETTINGS_TYPE_MIDI_SYSEX_MID_SENTENCE},
    {"siteSettingsSerialPorts", IDS_SITE_SETTINGS_TYPE_SERIAL_PORTS},
    {"siteSettingsSerialPortsMidSentence",
     IDS_SITE_SETTINGS_TYPE_SERIAL_PORTS_MID_SENTENCE},
    {"siteSettingsUsbDevices", IDS_SITE_SETTINGS_TYPE_USB_DEVICES},
    {"siteSettingsUsbDevicesMidSentence",
     IDS_SITE_SETTINGS_TYPE_USB_DEVICES_MID_SENTENCE},
    {"siteSettingsBluetoothDevices", IDS_SITE_SETTINGS_TYPE_BLUETOOTH_DEVICES},
    {"siteSettingsBluetoothDevicesMidSentence",
     IDS_SITE_SETTINGS_TYPE_BLUETOOTH_DEVICES_MID_SENTENCE},
    {"siteSettingsFileSystemWrite",
     IDS_SITE_SETTINGS_TYPE_FILE_SYSTEM_ACCESS_WRITE},
    {"siteSettingsFileSystemWriteMidSentence",
     IDS_SITE_SETTINGS_TYPE_FILE_SYSTEM_ACCESS_WRITE_MID_SENTENCE},
    {"siteSettingsRemoveZoomLevel",
     IDS_SETTINGS_SITE_SETTINGS_REMOVE_ZOOM_LEVEL},
    {"siteSettingsZoomLevels", IDS_SITE_SETTINGS_TYPE_ZOOM_LEVELS},
    {"siteSettingsZoomLevelsMidSentence",
     IDS_SITE_SETTINGS_TYPE_ZOOM_LEVELS_MID_SENTENCE},
    {"siteSettingsNoZoomedSites", IDS_SETTINGS_SITE_SETTINGS_NO_ZOOMED_SITES},
    {"siteSettingsAskBeforeSending",
     IDS_SETTINGS_SITE_SETTINGS_ASK_BEFORE_SENDING},
    {"siteSettingsHandlersAskRecommended",
     IDS_SETTINGS_SITE_SETTINGS_HANDLERS_ASK_RECOMMENDED},
    {"siteSettingsHandlersBlocked",
     IDS_SETTINGS_SITE_SETTINGS_HANDLERS_BLOCKED},
    {"siteSettingsCookiesAllowed",
     IDS_SETTINGS_SITE_SETTINGS_COOKIES_ALLOW_SITES},
    {"siteSettingsAllow", IDS_SETTINGS_SITE_SETTINGS_ALLOW},
    {"siteSettingsBlock", IDS_SETTINGS_SITE_SETTINGS_BLOCK},
    {"siteSettingsSessionOnly", IDS_SETTINGS_SITE_SETTINGS_SESSION_ONLY},
    {"siteSettingsBlocked", IDS_SETTINGS_SITE_SETTINGS_BLOCKED},
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
    {"siteSettingsSourceEmbargo",
     IDS_PAGE_INFO_PERMISSION_AUTOMATICALLY_BLOCKED},
    {"siteSettingsSourceInsecureOrigin",
     IDS_SETTINGS_SITE_SETTINGS_SOURCE_INSECURE_ORIGIN},
    {"siteSettingsSourceKillSwitch",
     IDS_SETTINGS_SITE_SETTINGS_SOURCE_KILL_SWITCH},
    {"siteSettingsReset", IDS_SETTINGS_SITE_SETTINGS_RESET_BUTTON},
    {"siteSettingsCookiesThirdPartyExceptionLabel",
     IDS_SETTINGS_SITE_SETTINGS_THIRD_PARTY_COOKIES_EXCEPTION_LABEL},
    {"siteSettingsCookieRemoveSite",
     IDS_SETTINGS_SITE_SETTINGS_COOKIE_REMOVE_SITE},
    {"siteSettingsDelete", IDS_SETTINGS_SITE_SETTINGS_DELETE},
    {"siteSettingsDeleteAllStorageDialogTitle",
     IDS_SETTINGS_SITE_SETTINGS_DELETE_ALL_STORAGE_DIALOG_TITLE},
    {"siteSettingsDeleteDisplayedStorageDialogTitle",
     IDS_SETTINGS_SITE_SETTINGS_DELETE_DISPLAYED_STORAGE_DIALOG_TITLE},
    {"siteSettingsFirstPartySetsLearnMore",
     IDS_SETTINGS_SITE_SETTINGS_FIRST_PARTY_SETS_LEARN_MORE},
    {"siteSettingsFirstPartySetsLearnMoreAccessibility",
     IDS_SETTINGS_SITE_SETTINGS_FIRST_PARTY_SETS_LEARN_MORE_ACCESSIBILITY},
    {"siteSettingsClearAllStorageDescription",
     IDS_SETTINGS_SITE_SETTINGS_CLEAR_ALL_STORAGE_DESCRIPTION},
    {"siteSettingsClearDisplayedStorageDescription",
     IDS_SETTINGS_SITE_SETTINGS_CLEAR_DISPLAYED_STORAGE_DESCRIPTION},
    {"siteSettingsDeleteAllStorageLabel",
     IDS_SETTINGS_SITE_SETTINGS_DELETE_ALL_STORAGE_LABEL},
    {"siteSettingsDeleteDisplayedStorageLabel",
     IDS_SETTINGS_SITE_SETTINGS_DELETE_DISPLAYED_STORAGE_LABEL},
    {"siteSettingsDeleteAllStorageConfirmation",
     IDS_SETTINGS_SITE_SETTINGS_DELETE_ALL_STORAGE_CONFIRMATION},
    {"siteSettingsDeleteDisplayedStorageConfirmation",
     IDS_SETTINGS_SITE_SETTINGS_DELETE_DISPLAYED_STORAGE_CONFIRMATION},
    {"siteSettingsDeleteAllStorageConfirmationInstalled",
     IDS_SETTINGS_SITE_SETTINGS_DELETE_ALL_STORAGE_CONFIRMATION_INSTALLED},
    {"siteSettingsDeleteDisplayedStorageConfirmationInstalled",
     IDS_SETTINGS_SITE_SETTINGS_DELETE_DISPLAYED_STORAGE_CONFIRMATION_INSTALLED},
    {"siteSettingsClearAllStorageSignOut",
     IDS_SETTINGS_SITE_SETTINGS_CLEAR_ALL_STORAGE_SIGN_OUT},
    {"siteSettingsClearDisplayedStorageSignOut",
     IDS_SETTINGS_SITE_SETTINGS_CLEAR_DISPLAYED_STORAGE_SIGN_OUT},
    {"siteSettingsSiteDetailsSubpageAccessibilityLabel",
     IDS_SETTINGS_SITE_SETTINGS_SITE_DETAILS_SUBPAGE_ACCESSIBILITY_LABEL},
    {"firstPartySetsMoreActionsTitle",
     IDS_SETTINGS_SITE_SETTINGS_FIRST_PARTY_SETS_MORE_ACTIONS_TITLE},
    {"firstPartySetsShowRelatedSitesButton",
     IDS_SETTINGS_SITE_SETTINGS_FIRST_PARTY_SETS_SHOW_RELATED_SITES_BUTTON},
    {"firstPartySetsSiteDeleteStorageButton",
     IDS_SETTINGS_SITE_SETTINGS_FIRST_PARTY_SETS_SITE_DELETE_STORAGE_BUTTON},
    {"siteSettingsSiteClearStorage",
     IDS_SETTINGS_SITE_SETTINGS_SITE_CLEAR_STORAGE},
    {"siteSettingsSiteClearStorageConfirmation",
     IDS_SETTINGS_SITE_SETTINGS_SITE_CLEAR_STORAGE_CONFIRMATION},
    {"siteSettingsSiteClearStorageConfirmationNew",
     IDS_SETTINGS_SITE_SETTINGS_SITE_CLEAR_STORAGE_CONFIRMATION_NEW},
    {"siteSettingsSiteDeleteStorageDialogTitle",
     IDS_SETTINGS_SITE_SETTINGS_SITE_DELETE_STORAGE_DIALOG_TITLE},
    {"siteSettingsSiteClearStorageSignOut",
     IDS_SETTINGS_SITE_SETTINGS_SITE_CLEAR_STORAGE_SIGN_OUT},
    {"siteSettingsSiteDeleteStorageOfflineData",
     IDS_SETTINGS_SITE_SETTINGS_SITE_DELETE_STORAGE_OFFLINE_DATA},
    {"siteSettingsRemoveSiteAdPersonalization",
     IDS_SETTINGS_SITE_SETTINGS_REMOVE_SITE_AD_PERSONALIZATION},
    {"siteSettingsSiteGroupDeleteOfflineData",
     IDS_SETTINGS_SITE_SETTINGS_SITE_GROUP_DELETE_OFFLINE_DATA},
    {"siteSettingsSiteResetAll", IDS_SETTINGS_SITE_SETTINGS_SITE_RESET_ALL},
    {"siteSettingsSiteResetConfirmation",
     IDS_SETTINGS_SITE_SETTINGS_SITE_RESET_CONFIRMATION},
    {"siteSettingsRemoveSite", IDS_SETTINGS_SITE_SETTINGS_COOKIE_REMOVE_SITE},
    {"siteSettingsRemoveSiteOriginDialogTitle",
     IDS_SETTINGS_SITE_SETTINGS_REMOVE_SITE_ORIGIN_DIALOG_TITLE},
    {"siteSettingsRemoveSiteOriginAppDialogTitle",
     IDS_SETTINGS_SITE_SETTINGS_REMOVE_SITE_ORIGIN_APP_DIALOG_TITLE},
    {"siteSettingsRemoveSiteOriginPartitionedDialogTitle",
     IDS_SETTINGS_SITE_SETTINGS_REMOVE_SITE_ORIGIN_PARTITIONED_DIALOG_TITLE},
    {"siteSettingsRemoveSiteGroupDialogTitle",
     IDS_SETTINGS_SITE_SETTINGS_REMOVE_SITE_GROUP_DIALOG_TITLE},
    {"siteSettingsRemoveSiteGroupAppDialogTitle",
     IDS_SETTINGS_SITE_SETTINGS_REMOVE_SITE_GROUP_APP_DIALOG_TITLE},
    {"siteSettingsRemoveSiteGroupAppPluralDialogTitle",
     IDS_SETTINGS_SITE_SETTINGS_REMOVE_SITE_GROUP_APP_PLURAL_DIALOG_TITLE},
    {"siteSettingsRemoveSiteOriginLogout",
     IDS_SETTINGS_SITE_SETTINGS_REMOVE_SITE_ORIGIN_LOGOUT},
    {"siteSettingsRemoveSiteGroupLogout",
     IDS_SETTINGS_SITE_SETTINGS_REMOVE_SITE_GROUP_LOGOUT},
    {"siteSettingsRemoveSiteOfflineData",
     IDS_SETTINGS_SITE_SETTINGS_REMOVE_SITE_OFFLINE_DATA},
    {"siteSettingsRemoveSitePermissions",
     IDS_SETTINGS_SITE_SETTINGS_REMOVE_SITE_PERMISSIONS},
    {"siteSettingsRemoveSiteConfirm",
     IDS_SETTINGS_SITE_SETTINGS_REMOVE_SITE_CONFIRM},
    {"thirdPartyCookie", IDS_NEW_TAB_OTR_THIRD_PARTY_COOKIE},
    {"thirdPartyCookieSublabel", IDS_NEW_TAB_OTR_THIRD_PARTY_COOKIE_SUBLABEL},
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
    {"siteSettingsNotificationsDefaultBehaviorDescription",
     IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS_DEFAULT_BEHAVIOR_DESC},
    {"siteSettingsCustomizedBehaviors",
     IDS_SETTINGS_SITE_SETTINGS_CUSTOMIZED_BEHAVIORS},
    {"siteSettingsCustomizedBehaviorsDescription",
     IDS_SETTINGS_SITE_SETTINGS_CUSTOMIZED_BEHAVIORS_DESCRIPTION},
    {"siteSettingsCustomizedBehaviorsDescriptionShort",
     IDS_SETTINGS_SITE_SETTINGS_CUSTOMIZED_BEHAVIORS_DESCRIPTION_SHORT},
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
    {"siteSettingsBackgroundSyncBlockedSubLabel",
     IDS_SETTINGS_SITE_SETTINGS_BACKGROUND_SYNC_BLOCKED_SUB_LABEL},
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
    {"siteSettingsFederatedIdentityApi",
     IDS_SITE_SETTINGS_TYPE_FEDERATED_IDENTITY_API},
    {"siteSettingsFederatedIdentityApiAllowed",
     IDS_SETTINGS_SITE_SETTINGS_FEDERATED_IDENTITY_API_ALLOWED},
    {"siteSettingsFederatedIdentityApiBlocked",
     IDS_SETTINGS_SITE_SETTINGS_FEDERATED_IDENTITY_API_BLOCKED},
    {"siteSettingsFederatedIdentityApiAllowedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_FEDERATED_IDENTITY_API_ALLOWED_EXCEPTIONS},
    {"siteSettingsFederatedIdentityApiBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_FEDERATED_IDENTITY_API_BLOCKED_EXCEPTIONS},
    {"siteSettingsFederatedIdentityApiDescription",
     IDS_SETTINGS_SITE_SETTINGS_FEDERATED_IDENTITY_API_DESCRIPTION},
    {"siteSettingsFederatedIdentityApiMidSentence",
     IDS_SITE_SETTINGS_TYPE_FEDERATED_IDENTITY_API_MID_SENTENCE},
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
    {"siteSettingsImagesBlockedSubLabel",
     IDS_SETTINGS_SITE_SETTINGS_IMAGES_BLOCKED_SUB_LABEL},
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
    {"siteSettingsSoundBlockedSubLabel",
     IDS_SETTINGS_SITE_SETTINGS_SOUND_BLOCKED_SUB_LABEL},
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
    {"siteSettingsAds", IDS_SITE_SETTINGS_TYPE_ADS},
    {"siteSettingsAdsMidSentence", IDS_SITE_SETTINGS_TYPE_ADS_MID_SENTENCE},
    {"siteSettingsPaymentHandler", IDS_SITE_SETTINGS_TYPE_PAYMENT_HANDLER},
    {"siteSettingsPaymentHandlerMidSentence",
     IDS_SITE_SETTINGS_TYPE_PAYMENT_HANDLER_MID_SENTENCE},
    {"siteSettingsBlockAutoplaySetting",
     IDS_SETTINGS_SITE_SETTINGS_BLOCK_AUTOPLAY},
    {"emptyAllSitesPage", IDS_SETTINGS_SITE_SETTINGS_EMPTY_ALL_SITES_PAGE},
    {"noSitesFound", IDS_SETTINGS_SITE_SETTINGS_NO_SITES_FOUND},
    {"siteSettingsBluetoothScanning",
     IDS_SITE_SETTINGS_TYPE_BLUETOOTH_SCANNING},
    {"siteSettingsBluetoothScanningMidSentence",
     IDS_SITE_SETTINGS_TYPE_BLUETOOTH_SCANNING_MID_SENTENCE},
    {"siteSettingsBluetoothScanningDescription",
     IDS_SETTINGS_SITE_SETTINGS_BLUETOOTH_SCANNING_DESCRIPTION},
    {"siteSettingsBluetoothScanningAsk",
     IDS_SETTINGS_SITE_SETTINGS_BLUETOOTH_SCANNING_ASK},
    {"siteSettingsBluetoothScanningBlock",
     IDS_SETTINGS_SITE_SETTINGS_BLUETOOTH_SCANNING_BLOCK},
    {"siteSettingsBluetoothScanningAllowedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_BLUETOOTH_SCANNING_ALLOWED_EXCEPTIONS},
    {"siteSettingsBluetoothScanningBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_BLUETOOTH_SCANNING_BLOCKED_EXCEPTIONS},
    {"siteSettingsAr", IDS_SITE_SETTINGS_TYPE_AR},
    {"siteSettingsArMidSentence", IDS_SITE_SETTINGS_TYPE_AR_MID_SENTENCE},
    {"siteSettingsArAsk", IDS_SETTINGS_SITE_SETTINGS_AR_ASK},
    {"siteSettingsArBlock", IDS_SETTINGS_SITE_SETTINGS_AR_BLOCK},
    {"siteSettingsVr", IDS_SITE_SETTINGS_TYPE_VR},
    {"siteSettingsVrMidSentence", IDS_SITE_SETTINGS_TYPE_VR_MID_SENTENCE},
    {"siteSettingsWindowManagement", IDS_SITE_SETTINGS_TYPE_WINDOW_MANAGEMENT},
    {"siteSettingsWindowManagementMidSentence",
     IDS_SITE_SETTINGS_TYPE_WINDOW_MANAGEMENT_MID_SENTENCE},
    {"siteSettingsWindowManagementDescription",
     IDS_SETTINGS_SITE_SETTINGS_WINDOW_MANAGEMENT_DESCRIPTION},
    {"siteSettingsWindowManagementAsk",
     IDS_SETTINGS_SITE_SETTINGS_WINDOW_MANAGEMENT_ASK},
    {"siteSettingsWindowManagementBlocked",
     IDS_SETTINGS_SITE_SETTINGS_WINDOW_MANAGEMENT_BLOCKED},
    {"siteSettingsWindowManagementAskExceptions",
     IDS_SETTINGS_SITE_SETTINGS_WINDOW_MANAGEMENT_ASK_EXCEPTIONS},
    {"siteSettingsWindowManagementBlockedExceptions",
     IDS_SETTINGS_SITE_SETTINGS_WINDOW_MANAGEMENT_BLOCKED_EXCEPTIONS},
    {"siteSettingsFontAccessMidSentence",
     IDS_SITE_SETTINGS_TYPE_FONT_ACCESS_MID_SENTENCE},
    {"siteSettingsIdleDetection", IDS_SITE_SETTINGS_TYPE_IDLE_DETECTION},
    {"siteSettingsIdleDetectionMidSentence",
     IDS_SITE_SETTINGS_TYPE_IDLE_DETECTION_MID_SENTENCE},
    {"siteSettingsExtensionIdDescription",
     IDS_SETTINGS_SITE_SETTINGS_EXTENSION_ID_DESCRIPTION},
    {"siteSettingsSiteDataAllowedSubLabel",
     IDS_SETTINGS_SITE_SETTINGS_PAGE_SITE_DATA_ALLOWED_SUB_LABEL},
    {"siteSettingsSiteDataBlockedSubLabel",
     IDS_SETTINGS_SITE_SETTINGS_PAGE_SITE_DATA_BLOCKED_SUB_LABEL},
    {"siteSettingsSiteDataDeleteOnExitSubLabel",
     IDS_SETTINGS_SITE_SETTINGS_PAGE_SITE_DATA_DELETE_ON_EXIT_SUB_LABEL},
    {"siteSettingsAntiAbuse", IDS_SITE_SETTINGS_TYPE_ANTI_ABUSE},
    {"siteSettingsAntiAbuseDescription", IDS_SETTINGS_ANTI_ABUSE_DESCRIPTION},
    {"siteSettingsAntiAbuseEnabledSubLabel",
     IDS_SETTINGS_ANTI_ABUSE_ENABLED_SUB_LABEL},
    {"siteSettingsAntiAbuseDisabledSubLabel",
     IDS_SETTINGS_ANTI_ABUSE_DISABLED_SUB_LABEL},
    {"antiAbuseWhenOnHeader", IDS_SETTINGS_ANTI_ABUSE_WHEN_ON_HEADER},
    {"antiAbuseWhenOnSectionOne", IDS_SETTINGS_ANTI_ABUSE_WHEN_ON_SECTION_ONE},
    {"antiAbuseWhenOnSectionTwo", IDS_SETTINGS_ANTI_ABUSE_WHEN_ON_SECTION_TWO},
    {"antiAbuseWhenOnSectionThree",
     IDS_SETTINGS_ANTI_ABUSE_WHEN_ON_SECTION_THREE},
    {"antiAbuseThingsToConsiderHeader",
     IDS_SETTINGS_ANTI_ABUSE_THINGS_TO_CONSIDER_HEADER},
    {"antiAbuseThingsToConsiderSectionOne",
     IDS_SETTINGS_ANTI_ABUSE_THINGS_TO_CONSIDER_SECTION_ONE},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  // These ones cannot be constexpr because we need to check base::FeatureList.
  static webui::LocalizedString kSensorsLocalizedStrings[] = {
      {"siteSettingsSensors",
       base::FeatureList::IsEnabled(features::kGenericSensorExtraClasses)
           ? IDS_SITE_SETTINGS_TYPE_SENSORS
           : IDS_SITE_SETTINGS_TYPE_MOTION_SENSORS},
      {"siteSettingsSensorsMidSentence",
       base::FeatureList::IsEnabled(features::kGenericSensorExtraClasses)
           ? IDS_SITE_SETTINGS_TYPE_SENSORS_MID_SENTENCE
           : IDS_SITE_SETTINGS_TYPE_MOTION_SENSORS_MID_SENTENCE},
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

  html_source->AddBoolean("enableFederatedIdentityApiContentSetting",
                          base::FeatureList::IsEnabled(features::kFedCm));

  base::CommandLine& cmd = *base::CommandLine::ForCurrentProcess();
  html_source->AddBoolean(
      "enableExperimentalWebPlatformFeatures",
      cmd.HasSwitch(::switches::kEnableExperimentalWebPlatformFeatures));

  html_source->AddBoolean(
      "enableQuietNotificationPromptsSetting",
      base::FeatureList::IsEnabled(features::kQuietNotificationPrompts));

  html_source->AddBoolean("enableWebBluetoothNewPermissionsBackend",
                          base::FeatureList::IsEnabled(
                              features::kWebBluetoothNewPermissionsBackend));

  html_source->AddBoolean(
      "showPersistentPermissions",
      base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions));

  // The exception placeholder should not be translated. See crbug.com/1095878.
  html_source->AddString("addSiteExceptionPlaceholder", "[*.]example.com");
}

void AddStorageAccessStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"siteSettingsStorageAccess", IDS_SITE_SETTINGS_TYPE_STORAGE_ACCESS},
      {"siteSettingsStorageAccessMidSentence",
       IDS_SITE_SETTINGS_TYPE_STORAGE_ACCESS_MID_SENTENCE},
      {"storageAccessDescription", IDS_SETTINGS_STORAGE_ACCESS_DESCRIPTION},
      {"storageAccessAllowed", IDS_SETTINGS_STORAGE_ACCESS_ALLOWED},
      {"storageAccessBlocked", IDS_SETTINGS_STORAGE_ACCESS_BLOCKED},
      {"storageAccessAllowedExceptions",
       IDS_SETTINGS_STORAGE_ACCESS_ALLOWED_EXCEPTIONS},
      {"storageAccessBlockedExceptions",
       IDS_SETTINGS_STORAGE_ACCESS_BLOCKED_EXCEPTIONS},
      {"storageAccessResetAll", IDS_SETTINGS_STORAGE_ACCESS_RESET_ALL},
      {"storageAccessResetSite", IDS_SETTINGS_STORAGE_ACCESS_RESET_SITE},
      {"storageAccessOpenExpand", IDS_SETTINGS_STORAGE_ACCESS_OPEN_EXPAND},
      {"storageAccessCloseExpand", IDS_SETTINGS_STORAGE_ACCESS_CLOSE_EXPAND},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

void AddSiteDataPageStrings(content::WebUIDataSource* html_source,
                            Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"siteDataPageTitle", IDS_SETTINGS_SITE_DATA_PAGE_TITLE},
      {"siteDataPageDescription", IDS_SETTINGS_SITE_DATA_PAGE_DESCRIPTION},
      {"siteDataPageDefaultBehavior",
       IDS_SETTINGS_SITE_DATA_PAGE_DEFAULT_BEHAVIOR_HEADING},
      {"siteDataPagedefaultBehaviorDescription",
       IDS_SETTINGS_SITE_DATA_PAGE_DEFAULT_BEHAVIOR_DESCRIPTION},
      {"siteDataPageAllowRadioLabel",
       IDS_SETTINGS_SITE_DATA_PAGE_ALLOW_RADIO_LABEL},
      {"siteDataPageAllowRadioSubLabel",
       IDS_SETTINGS_SITE_DATA_PAGE_ALLOW_RADIO_SUB_LABEL},
      {"siteDataPageClearOnExitRadioLabel",
       IDS_SETTINGS_SITE_DATA_PAGE_CLEAR_ON_EXIT_RADIO_LABEL},
      {"siteDataPageClearOnExitRadioSubLabel",
       IDS_SETTINGS_SITE_DATA_PAGE_CLEAR_ON_EXIT_RADIO_SUBLABEL},
      {"siteDataPageBlockRadioLabel",
       IDS_SETTINGS_SITE_DATA_PAGE_BLOCK_RADIO_LABEL},
      {"siteDataPageBlockRadioSublabel",
       IDS_SETTINGS_SITE_DATA_PAGE_BLOCK_RADIO_SUB_LABEL},
      {"siteDataPageCustomizedBehaviorHeading",
       IDS_SETTINGS_SITE_DATA_PAGE_CUSTOMIZED_BEHAVIOR_HEADING},
      {"siteDataPageCustomizedBehaviorDescription",
       IDS_SETTINGS_SITE_DATA_PAGE_CUSTOMIZED_BEHAVIOR_DESCRIPTION},
      {"siteDataPageAllowExceptionsSubHeading",
       IDS_SETTINGS_SITE_DATA_PAGE_ALLOW_EXCEPTIONS_SUB_HEADING},
      {"siteDataPageDeleteOnExitExceptionsSubHeading",
       IDS_SETTINGS_SITE_DATA_PAGE_DELETE_ON_EXIT_EXCEPTIONS_SUB_HEADING},
      {"siteDataPageBlockExceptionsSubHeading",
       IDS_SETTINGS_SITE_DATA_PAGE_BLOCK_EXCEPTIONS_SUB_HEADING},
      {"siteDataPageBlockConfirmDialogTitle",
       IDS_SETTINGS_SITE_DATA_PAGE_BLOCK_CONFIRM_DIALOG_TITLE},
      {"siteDataPageBlockConfirmDialogDescription",
       IDS_SETTINGS_SITE_DATA_PAGE_BLOCK_CONFIRM_DIALOG_DESCRIPTION},
      {"siteDataPageBlockConfirmDialogConfirmButton",
       IDS_SETTINGS_SITE_DATA_PAGE_BLOCK_CONFIRM_DIALOG_CONFIRM_BUTTON},
      {"siteDataPageBlockConfirmDialogCancelButton",
       IDS_SETTINGS_SITE_DATA_PAGE_BLOCK_CONFIRM_DIALOG_CANCEL_BUTTON},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void AddSystemStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"systemPageTitle", IDS_SETTINGS_SYSTEM},
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS_LACROS)
    {"backgroundAppsLabel", IDS_SETTINGS_SYSTEM_BACKGROUND_APPS_LABEL},
#endif
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
    {"hardwareAccelerationLabel",
     IDS_SETTINGS_SYSTEM_HARDWARE_ACCELERATION_LABEL},
    {"proxySettingsLabel", IDS_SETTINGS_SYSTEM_PROXY_SETTINGS_LABEL},
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    {"useAshProxyLabel", IDS_SETTINGS_SYSTEM_USE_ASH_PROXY_LABEL},
    {"usesAshProxyLabel", IDS_SETTINGS_SYSTEM_USES_ASH_PROXY_LABEL},
#endif
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
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
#endif

  // TODO(dbeam): we should probably rename anything involving "localized
  // strings" to "load time data" as all primitive types are used now.
  SystemHandler::AddLoadTimeData(html_source);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

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
      {"securityKeysBioEnrollmentStorageFullLabel",
       IDS_SETTINGS_SECURITY_KEYS_BIO_ENROLLMENT_STORAGE_FULL},
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
      {"securityKeysBioEnrollmentNameLabelTooLong",
       IDS_SETTINGS_SECURITY_KEYS_BIO_NAME_LABEL_TOO_LONG},
      {"securityKeysConfirmPIN", IDS_SETTINGS_SECURITY_KEYS_CONFIRM_PIN},
      {"securityKeysCredentialManagementDesc",
       IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_DESC},
      {"securityKeysCredentialManagementConfirmDeleteTitle",
       IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_CONFIRM_DELETE_TITLE},
      {"securityKeysCredentialManagementDialogTitle",
       IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_DIALOG_TITLE},
      {"securityKeysUpdateCredentialDialogTitle",
       IDS_SETTINGS_SECURITY_KEYS_UPDATE_CREDENTIAL_DIALOG_TITLE},
      {"securityKeysCredentialWebsiteLabel",
       IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_WEBSITE_LABEL},
      {"securityKeysCredentialUsernameLabel",
       IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_USERNAME_LABEL},
      {"securityKeysCredentialDisplayNameLabel",
       IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_DISPLAYNAME_LABEL},
      {"securityKeysCredentialManagementLabel",
       IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_LABEL},
      {"securityKeysCredentialManagementConfirmDeleteCredential",
       IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_MANAGEMENT_CONFIRM_DELETE_CREDENTIAL},
      {"securityKeysInputTooLong",
       IDS_SETTINGS_SECURITY_KEYS_INPUT_ERROR_TOO_LONG},
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
      {"securityKeysPhoneEditDialogTitle",
       IDS_SETTINGS_SECURITY_KEYS_PHONE_EDIT_DIALOG_TITLE},
      {"securityKeysPhonesYourDevices",
       IDS_SETTINGS_SECURITY_KEYS_PHONES_YOUR_DEVICES},
      {"securityKeysPhonesSyncedDesc",
       IDS_SETTINGS_SECURITY_KEYS_PHONES_SYNCED_DESC},
      {"securityKeysPhonesLinkedDevices",
       IDS_SETTINGS_SECURITY_KEYS_PHONES_LINKED_DEVICES},
      {"securityKeysPhonesLinkedDesc",
       IDS_SETTINGS_SECURITY_KEYS_PHONES_LINKED_DESC},
      {"securityKeysPhonesManage", IDS_SETTINGS_SECURITY_KEYS_PHONES_MANAGE},
      {"securityKeysPhonesManageDesc",
       IDS_SETTINGS_SECURITY_KEYS_PHONES_MANAGE_DESC},
  };
  html_source->AddLocalizedStrings(kSecurityKeysStrings);
  bool win_native_api_available = false;
#if BUILDFLAG(IS_WIN)
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

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  AddGetTheMostOutOfChromeStrings(html_source);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  AddIncompatibleApplicationsStrings(html_source);
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

  AddClearBrowsingDataStrings(html_source, profile);
  AddCommonStrings(html_source, profile);
  AddDownloadsStrings(html_source);
  AddExtensionsStrings(html_source);
  AddPerformanceStrings(html_source);
  AddLanguagesStrings(html_source, profile);
  AddOnStartupStrings(html_source);
  AddPeopleStrings(html_source, profile);
  AddPrivacySandboxStrings(html_source, profile);
  AddPrivacyGuideStrings(html_source);
  AddPrivacyStrings(html_source, profile);
  AddSafetyCheckStrings(html_source);
  AddSafetyHubStrings(html_source);
  AddResetStrings(html_source, profile);
  AddSearchEnginesStrings(html_source);
  AddSearchInSettingsStrings(html_source);
  AddSearchStrings(html_source);
  AddSiteSettingsStrings(html_source, profile);
  AddSiteDataPageStrings(html_source, profile);
  AddStorageAccessStrings(html_source);

#if !BUILDFLAG(IS_CHROMEOS)
  AddDefaultBrowserStrings(html_source);
  AddImportDataStrings(html_source);
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  AddSystemStrings(html_source);
#endif

#if BUILDFLAG(USE_NSS_CERTS)
  certificate_manager::AddLocalizedStrings(html_source);
#endif

  policy_indicator::AddLocalizedStrings(html_source);
  AddSecurityKeysStrings(html_source);

  html_source->UseStringsJs();
}

}  // namespace settings
