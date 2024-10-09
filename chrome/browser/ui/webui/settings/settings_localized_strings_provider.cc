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
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/obsolete_system/obsolete_system.h"
#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/account_consistency_mode_manager_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
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
#include "chrome/grit/branded_strings.h"
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
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_features.h"
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
#include "components/permissions/features.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/grit/plus_addresses_strings.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/strings/grit/components_branded_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
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
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "chrome/browser/ui/chrome_pages.h"
#endif

#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS)
#include "ui/display/screen.h"
#endif

#if BUILDFLAG(IS_WIN)
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

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "extensions/common/extension_urls.h"
#endif

#if BUILDFLAG(ENABLE_VR)
#include "device/vr/public/cpp/features.h"
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
      {"columnHeadingWhenOn", IDS_SETTINGS_COLUMN_HEADING_WHEN_ON},
      {"columnHeadingConsider", IDS_SETTINGS_COLUMN_HEADING_CONSIDER},
#if !BUILDFLAG(IS_CHROMEOS_ASH)
      {"relaunchConfirmationDialogTitle",
       IDS_RELAUNCH_CONFIRMATION_DIALOG_TITLE},
#endif
      {"remove", IDS_REMOVE},
      {"restart", IDS_SETTINGS_RESTART},
      {"restartToApplyChanges", IDS_SETTINGS_RESTART_TO_APPLY_CHANGES},
      {"retry", IDS_SETTINGS_RETRY},
      {"save", IDS_SAVE},
      {"searchResultBubbleText", IDS_SEARCH_RESULT_BUBBLE_TEXT},
      {"searchResultsBubbleText", IDS_SEARCH_RESULTS_BUBBLE_TEXT},
      {"sentenceEnd", IDS_SENTENCE_END},
      {"settings", IDS_SETTINGS_SETTINGS},
      {"settingsAltPageTitle", IDS_SETTINGS_ALT_PAGE_TITLE},
      {"subpageArrowRoleDescription", IDS_SETTINGS_SUBPAGE_BUTTON},
      {"subpageBackButtonAriaLabel",
       IDS_SETTINGS_SUBPAGE_BACK_BUTTON_ARIA_LABEL},
      {"subpageBackButtonAriaRoleDescription",
       IDS_SETTINGS_SUBPAGE_BACK_BUTTON_ARIA_ROLE_DESCRIPTION},
      {"subpageLearnMoreAriaLabel", IDS_SETTINGS_SUBPAGE_LEARN_MORE_ARIA_LABEL},
      {"notValid", IDS_SETTINGS_NOT_VALID},
      {"notValidWebAddress", IDS_SETTINGS_NOT_VALID_WEB_ADDRESS},
      {"notValidWebAddressForContentType",
       IDS_SETTINGS_NOT_VALID_WEB_ADDRESS_FOR_CONTENT_TYPE},
      {"inputMaxLengthDescription", IDS_SETTINGS_INPUT_MAX_LENGTH_DESCRIPTION},

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
}

void AddA11yStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"moreFeaturesLink", IDS_SETTINGS_MORE_FEATURES_LINK},
      {"a11yPageTitle", IDS_SETTINGS_ACCESSIBILITY},
      {"a11yWebStore", IDS_SETTINGS_ACCESSIBILITY_WEB_STORE},
      {"moreFeaturesLinkDescription",
       IDS_SETTINGS_MORE_FEATURES_LINK_DESCRIPTION},
      {"accessibleImageLabelsTitle",
       IDS_SETTINGS_ACCESSIBLE_IMAGE_LABELS_TITLE},
      {"accessibleImageLabelsSubtitle",
       IDS_SETTINGS_ACCESSIBLE_IMAGE_LABELS_SUBTITLE},
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
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
      {"overscrollHistoryNavigationTitle",
       IDS_SETTINGS_OVERSCROLL_HISTORY_NAVIGATION_TITLE},
      {"overscrollHistoryNavigationSubtitle",
       IDS_SETTINGS_OVERSCROLL_HISTORY_NAVIGATION_SUBTITLE},
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  AddAxAnnotationsSectionStrings(html_source);
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

  html_source->AddString("managementPage",
                         chrome::GetDeviceManagedUiHelpLabel(profile));
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
      IDS_VERSION_UI_LICENSE, chrome::kChromiumProjectURL,
      chrome::kChromeUICreditsURL16);
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
      {"customizeToolbar", IDS_SETTINGS_CUSTOMIZE_TOOLBAR},
      {"chromeColors", IDS_SETTINGS_CHROME_COLORS},
      {"colorSchemeMode", IDS_SETTINGS_COLOR_SCHEME_MODE},
      {"lightMode", IDS_NTP_CUSTOMIZE_CHROME_COLOR_SCHEME_MODE_LIGHT_LABEL},
      {"darkMode", IDS_NTP_CUSTOMIZE_CHROME_COLOR_SCHEME_MODE_DARK_LABEL},
      {"systemMode", IDS_NTP_CUSTOMIZE_CHROME_COLOR_SCHEME_MODE_SYSTEM_LABEL},
      {"showHomeButton", IDS_SETTINGS_SHOW_HOME_BUTTON},
      {"showBookmarksBar", IDS_SETTINGS_SHOW_BOOKMARKS_BAR},
      {"showTabGroupsInBookmarksBar",
       IDS_SETTINGS_SHOW_TAB_GROUPS_IN_BOOKMARKS_BAR},
      {"autoPinNewTabGroups", IDS_SETTINGS_AUTO_PIN_NEW_TAB_GROUPS},
      {"hoverCardTitle", IDS_SETTINGS_HOVER_CARD_TITLE},
      {"showHoverCardImages", IDS_SETTINGS_SHOW_HOVER_CARD_IMAGES},
      {"showHoverCardMemoryUsage", IDS_SETTINGS_SHOW_HOVER_CARD_MEMORY_USAGE},
      {"showHoverCardMemoryUsageStandalone",
       IDS_SETTINGS_SHOW_HOVER_CARD_MEMORY_USAGE_STANDALONE},
      {"sidePanelPosition", IDS_SETTINGS_SIDE_PANEL_POSITION},
      {"tabSearchPosition", IDS_SETTINGS_TAB_SEARCH_POSITION},
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
      {"uiFeatureAlignLeft", IDS_SETTINGS_UI_FEATURE_ALIGN_LEFT},
      {"uiFeatureAlignRight", IDS_SETTINGS_UI_FEATURE_ALIGN_RIGHT},
      {"resetToDefault", IDS_SETTINGS_RESET_TO_DEFAULT},
#if BUILDFLAG(IS_LINUX)
      {"gtkTheme", IDS_SETTINGS_GTK_THEME},
      {"useGtkTheme", IDS_SETTINGS_USE_GTK_THEME},
      {"qtTheme", IDS_SETTINGS_QT_THEME},
      {"useQtTheme", IDS_SETTINGS_USE_QT_THEME},
      {"classicTheme", IDS_SETTINGS_CLASSIC_THEME},
      {"useClassicTheme", IDS_SETTINGS_USE_CLASSIC_THEME},
#endif
#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS)
      {"showWindowDecorations", IDS_SHOW_WINDOW_DECORATIONS},
#endif
#if BUILDFLAG(IS_MAC)
      {"tabsToLinks", IDS_SETTINGS_TABS_TO_LINKS_PREF},
      {"warnBeforeQuitting", IDS_SETTINGS_WARN_BEFORE_QUITTING_PREF},
#endif
      {"themeManagedDialogTitle", IDS_NTP_THEME_MANAGED_DIALOG_TITLE},
      {"themeManagedDialogBody", IDS_NTP_THEME_MANAGED_DIALOG_BODY},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString("presetZoomFactors",
                         zoom::GetPresetZoomFactorsAsJSON());
  html_source->AddBoolean(
      "showHoverCardImagesOption",
      base::FeatureList::IsEnabled(features::kTabHoverCardImages));
  html_source->AddBoolean("tabGroupsSaveUIUpdateEnabled",
                          tab_groups::IsTabGroupsSaveUIUpdateEnabled());
  html_source->AddBoolean("showTabSearchPositionSettings",
                          tabs::CanShowTabSearchPositionSetting());
  html_source->AddBoolean("tabSearchIsRightAlignedAtStartup",
                          tabs::GetTabSearchTrailingTabstrip(profile));
  html_source->AddBoolean("toolbarPinningEnabled",
                          features::IsToolbarPinningEnabled());

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
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
      {"clearBrowsingDataSignedIn", IDS_SETTINGS_CLEAR_BROWSING_DATA_SIGNED_IN},
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
       IDS_SETTINGS_CLEAR_COOKIES_AND_SITE_DATA_SUMMARY_BASIC_SIGNED_IN_PROFILE},
      {"clearCookiesSummarySyncing",
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
      {"clearPeriod15Minutes", IDS_SETTINGS_CLEAR_PERIOD_15_MINUTES},
      {"clearPeriodNotSelected", IDS_SETTINGS_CLEAR_PERIOD_NOT_SELECTED},
      {"historyDeletionDialogTitle",
       IDS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_TITLE},
      {"historyDeletionDialogOK", IDS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_OK},
      {"passwordsDeletionDialogTitle",
       IDS_CLEAR_BROWSING_DATA_PASSWORDS_NOTICE_TITLE},
      {"passwordsDeletionDialogOK",
       IDS_CLEAR_BROWSING_DATA_PASSWORDS_NOTICE_OK},
      {"notificationWarning", IDS_SETTINGS_NOTIFICATION_WARNING},
  };

  html_source->AddString(
      "clearGoogleSearchHistoryGoogleDse",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CLEAR_GOOGLE_SEARCH_HISTORY_GOOGLE_DSE,
          chrome::kSearchHistoryUrlInClearBrowsingData,
          chrome::kMyActivityUrlInClearBrowsingData));
  html_source->AddString(
      "clearGoogleSearchHistoryNonGoogleDse",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CLEAR_GOOGLE_SEARCH_HISTORY_NON_GOOGLE_DSE,
          chrome::kMyActivityUrlInClearBrowsingData));
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

  html_source->AddBoolean(
      "unoDesktopEnabled",
      switches::IsExplicitBrowserSigninUIOnDesktopEnabled());
#if !BUILDFLAG(IS_CHROMEOS)
  html_source->AddBoolean(
      "isClearPrimaryAccountAllowed",
      !profile->IsGuestSession() &&
          ChromeSigninClientFactory::GetForProfile(profile)
              ->IsClearPrimaryAccountAllowed(/*has_sync_account=*/false));
#endif  // !BUILDFLAG(IS_CHROMEOS)

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
  // TODO(crbug.com/40192052): Remove this flag from the JS.
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
  // TODO(crbug.com/339250758): Clean up unused strings now that multistate mode
  // UI no longer exists.
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"performancePageTitle", IDS_SETTINGS_PERFORMANCE_PAGE_TITLE},
      {"generalPageTitle", IDS_SETTINGS_PERFORMANCE_GENERAL_PAGE_TITLE},
      {"memoryPageTitle", IDS_SETTINGS_PERFORMANCE_MEMORY_PAGE_TITLE},
      {"speedPageTitle", IDS_SETTINGS_PERFORMANCE_SPEED_PAGE_TITLE},
      {"memorySaverModeLabel",
       IDS_SETTINGS_PERFORMANCE_MEMORY_SAVER_MODE_SETTING},
      {"memorySaverModeDescription",
       IDS_SETTINGS_PERFORMANCE_MEMORY_SAVER_MODE_SETTING_DESCRIPTION},
      {"memorySaverModeHeuristicsLabel",
       IDS_SETTINGS_PERFORMANCE_MEMORY_SAVER_MODE_HEURISTICS_LABEL},
      {"memorySaverModeRecommendedBadge",
       IDS_SETTINGS_PERFORMANCE_MEMORY_SAVER_MODE_RECOMMENDED_BADGE},
      {"memorySaverModeOnTimerLabel",
       IDS_SETTINGS_PERFORMANCE_MEMORY_SAVER_MODE_ON_TIMER_LABEL},
      {"memorySaverModeRadioGroupAriaLabel",
       IDS_SETTINGS_PERFORMANCE_MEMORY_SAVER_MODE_RADIO_GROUP_ARIA_LABEL},
      {"memorySaverChooseDiscardTimeAriaLabel",
       IDS_SETTINGS_PERFORMANCE_MEMORY_SAVER_MODE_CHOOSE_DISCARD_TIME_ARIA_LABEL},
      {"memorySaverModeConservativeLabel",
       IDS_SETTINGS_PERFORMANCE_MEMORY_SAVER_MODE_CONSERVATIVE_LABEL},
      {"memorySaverModeMediumLabel",
       IDS_SETTINGS_PERFORMANCE_MEMORY_SAVER_MODE_MEDIUM_LABEL},
      {"memorySaverModeAggressiveLabel",
       IDS_SETTINGS_PERFORMANCE_MEMORY_SAVER_MODE_AGGRESSIVE_LABEL},
      {"memorySaverModeConservativeDescription",
       IDS_SETTINGS_PERFORMANCE_MEMORY_SAVER_MODE_CONSERVATIVE_DESCRIPTION},
      {"memorySaverModeMediumDescription",
       IDS_SETTINGS_PERFORMANCE_MEMORY_SAVER_MODE_MEDIUM_DESCRIPTION},
      {"memorySaverModeAggressiveDescription",
       IDS_SETTINGS_PERFORMANCE_MEMORY_SAVER_MODE_AGGRESSIVE_DESCRIPTION},
      {"batteryPageTitle", IDS_SETTINGS_PERFORMANCE_BATTERY_PAGE_TITLE},
      {"batterySaverModeLabel",
       IDS_SETTINGS_PERFORMANCE_BATTERY_SAVER_MODE_SETTING},
      {"batterySaverModeDescription",
       IDS_SETTINGS_PERFORMANCE_BATTERY_SAVER_MODE_SETTING_DESCRIPTION},
      {"batterySaverModeLinkOsDescription",
       IDS_SETTINGS_PERFORMANCE_BATTERY_SAVER_MODE_LINK_OS_SETTING_DESCRIPTION},
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
      {"tabDiscardingExceptionsDescription",
       IDS_SETTINGS_PERFORMANCE_TAB_DISCARDING_EXCEPTIONS_DESCRIPTION},
      {"tabDiscardingExceptionsAdditionalSites",
       IDS_SETTINGS_PERFORMANCE_TAB_DISCARDING_EXCEPTIONS_ADDITIONAL_SITES},
      {"tabDiscardingExceptionsAddDialogCurrentTabs",
       IDS_SETTINGS_PERFORMANCE_TAB_DISCARDING_EXCEPTIONS_ADD_DIALOG_CURRENT_TABS},
      {"tabDiscardingExceptionsAddDialogCurrentTabsEmpty",
       IDS_SETTINGS_PERFORMANCE_TAB_DISCARDING_EXCEPTIONS_ADD_DIALOG_CURRENT_TABS_EMPTY},
      {"tabDiscardingExceptionsAddDialogManual",
       IDS_SETTINGS_PERFORMANCE_TAB_DISCARDING_EXCEPTIONS_ADD_DIALOG_MANUAL},
      {"tabDiscardingExceptionsActiveSiteAriaDescription",
       IDS_SETTINGS_PERFORMANCE_TAB_DISCARDING_EXCEPTIONS_ACTIVE_SITE_ARIA_DESCRIPTION},
      {"preloadingToggleSummary",
       IDS_SETTINGS_PERFORMANCE_PRELOAD_TOGGLE_SUMMARY},
      {"discardRingTreatmentEnabledLabel",
       IDS_SETTINGS_PERFORMANCE_DISCARD_RING_TREATMENT_ENABLED_LABEL},
      {"tabHoverPreviewCardLinkTitle",
       IDS_SETTINGS_PERFORMANCE_TAB_HOVER_PREVIEW_CARD_LINK_TITLE},
      {"tabHoverPreviewCardLinkSubtitle",
       IDS_SETTINGS_PERFORMANCE_TAB_HOVER_PREVIEW_CARD_LINK_SUBTITLE},
      {"performanceInterventionEnabledLabel",
       IDS_SETTINGS_PERFORMANCE_INTERVENTION_NOTIFICATION_ENABLED_LABEL},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString(
      "discardRingTreatmentEnabledDescriptionWithLearnLink",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_PERFORMANCE_DISCARD_RING_TREATMENT_ENABLED_DESCRIPTION_WITH_LEARN_LINK,
          chrome::kDiscardRingTreatmentLearnMoreUrl,
          l10n_util::GetStringUTF16(IDS_SETTINGS_OPENS_IN_NEW_TAB)));

  html_source->AddString(
      "performanceInterventionEnabledDescription",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_PERFORMANCE_INTERVENTION_NOTIFICATION_ENABLED_DESCRIPTION,
          chrome::kPerformanceInterventionLearnMoreUrl,
          l10n_util::GetStringUTF16(IDS_SETTINGS_OPENS_IN_NEW_TAB)));

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
          chrome::kMemorySaverModeTabDiscardingHelpUrl));

  html_source->AddString("discardRingTreatmentLearnMoreUrl",
                         chrome::kDiscardRingTreatmentLearnMoreUrl);
  html_source->AddString("memorySaverLearnMoreUrl",
                         chrome::kMemorySaverModeLearnMoreUrl);
  html_source->AddString("batterySaverLearnMoreUrl",
                         chrome::kBatterySaverModeLearnMoreUrl);
  html_source->AddString("preloadingLearnMoreUrl",
                         chrome::kPreloadingLearnMoreUrl);
  html_source->AddString("performanceInterventionLearnMoreUrl",
                         chrome::kPerformanceInterventionLearnMoreUrl);

#if BUILDFLAG(IS_CHROMEOS)
  html_source->AddString(
      "osPowerSettingsUrl",
      chrome::GetOSSettingsUrl(chromeos::settings::mojom::kPowerSubpagePath)
          .spec());
#endif
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
      {"preferredLanguagesHeader",
       IDS_SETTINGS_LANGUAGES_PREFERRED_LANGUAGES_HEADER},
      {"preferredLanguagesDesc",
       IDS_SETTINGS_LANGUAGES_PREFERRED_LANGUAGES_DESC},
      {"moveToTop", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_TO_TOP},
      {"moveUp", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_UP},
      {"moveDown", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_DOWN},
      {"removeLanguage", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_REMOVE},
      {"addLanguages", IDS_SETTINGS_LANGUAGES_LANGUAGES_ADD},
      {"addLanguagesDialogTitle",
       IDS_SETTINGS_LANGUAGES_MANAGE_LANGUAGES_TITLE},
#if BUILDFLAG(IS_WIN)
      {"isDisplayedInThisLanguage",
       IDS_SETTINGS_LANGUAGES_IS_DISPLAYED_IN_THIS_LANGUAGE},
      {"displayInThisLanguage",
       IDS_SETTINGS_LANGUAGES_DISPLAY_IN_THIS_LANGUAGE},
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
      {"languageManagedDialogTitle",
       IDS_SETTINGS_LANGUAGES_MANAGED_DIALOG_TITLE},
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
      "osSettingsLanguagesPageUrl",
      chrome::GetOSSettingsUrl(
          ash::features::IsOsSettingsRevampWayfindingEnabled()
              ? chromeos::settings::mojom::kLanguagesSubpagePath
              : chromeos::settings::mojom::kLanguagesAndInputSectionPath)
          .spec());
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

bool CheckDeviceAuthAvailability(content::WebContents* web_contents) {
  // If `client` is not available, then don't show toggle switch.
  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(web_contents);
  if (!client) {
    return false;
  }

  return autofill::IsDeviceAuthAvailable(
      client->GetDeviceAuthenticator().get());
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
      {"genericCreditCard", IDS_AUTOFILL_CC_GENERIC},
      {"creditCards", IDS_AUTOFILL_PAYMENT_METHODS},
      {"paymentsMethodsTableAriaLabel",
       IDS_AUTOFILL_PAYMENT_METHODS_TABLE_ARIA_LABEL},
      {"noPaymentMethodsFound", IDS_SETTINGS_PAYMENT_METHODS_NONE},
      {"googlePayments", IDS_SETTINGS_GOOGLE_PAYMENTS},
      {"enableProfilesLabel", IDS_AUTOFILL_ENABLE_PROFILES_TOGGLE_LABEL},
      {"autofillSyncToggleLabel", IDS_AUTOFILL_SYNC_TOGGLE_LABEL},
      {"enableProfilesSublabel", IDS_AUTOFILL_ENABLE_PROFILES_TOGGLE_SUBLABEL},
      {"enableCreditCardsLabel", IDS_AUTOFILL_ENABLE_CREDIT_CARDS_TOGGLE_LABEL},
      {"enableCreditCardsSublabel",
       IDS_AUTOFILL_ENABLE_CREDIT_CARDS_TOGGLE_SUBLABEL},
      {"enableCvcStorageLabel",
       IDS_AUTOFILL_SETTINGS_PAGE_ENABLE_CVC_STORAGE_LABEL},
      {"enableCvcStorageAriaLabelForNoCvcSaved",
       IDS_AUTOFILL_SETTINGS_PAGE_ENABLE_CVC_STORAGE_ARIA_LABEL_FOR_NO_CVC_SAVED},
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
      {"addressesSublabel", IDS_AUTOFILL_ADDRESSES_SETTINGS_SUBLABEL},
      {"addAddressTitle", IDS_SETTINGS_AUTOFILL_ADDRESSES_ADD_TITLE},
      {"editAddressTitle", IDS_SETTINGS_AUTOFILL_ADDRESSES_EDIT_TITLE},
      {"localAddressIconA11yLabel", IDS_AUTOFILL_LOCAL_ADDRESS_ICON_A11Y_LABEL},
      {"newAccountAddressRecordTypeNotice",
       IDS_AUTOFILL_ADDRESS_WILL_BE_SAVED_IN_ACCOUNT_RECORD_TYPE_NOTICE},
      {"editAccountAddressRecordTypeNotice",
       IDS_AUTOFILL_ADDRESS_ALREADY_SAVED_IN_ACCOUNT_RECORD_TYPE_NOTICE},
      {"deleteAccountAddressRecordTypeNotice",
       IDS_AUTOFILL_DELETE_ACCOUNT_ADDRESS_RECORD_TYPE_NOTICE},
      {"addressCountry", IDS_SETTINGS_AUTOFILL_ADDRESSES_COUNTRY},
      {"addressPhone", IDS_SETTINGS_AUTOFILL_ADDRESSES_PHONE},
      {"addressEmail", IDS_SETTINGS_AUTOFILL_ADDRESSES_EMAIL},
      {"creditCardDescription", IDS_SETTINGS_AUTOFILL_CARD_DESCRIPTION},
      {"creditCardA11yLabeled", IDS_SETTINGS_AUTOFILL_CARD_A11Y_LABELED},
      {"creditCardExpDateA11yLabeled",
       IDS_SETTINGS_AUTOFILL_CARD_EXP_DATE_A11Y_LABELED},
      {"moreActionsForAddress", IDS_SETTINGS_AUTOFILL_MORE_ACTIONS_FOR_ADDRESS},
      {"moreActionsForCreditCard",
       IDS_SETTINGS_AUTOFILL_MORE_ACTIONS_FOR_CREDIT_CARD},
      {"moreActionsForCreditCardWithCvc",
       IDS_SETTINGS_AUTOFILL_MORE_ACTIONS_FOR_CREDIT_CARD_WITH_CVC},
      {"removeAddress", IDS_SETTINGS_ADDRESS_REMOVE},
      {"removeAddressConfirmationTitle",
       IDS_SETTINGS_ADDRESS_REMOVE_CONFIRMATION_TITLE},
      {"removeSyncAddressConfirmationDescription",
       IDS_AUTOFILL_DELETE_SYNC_ADDRESS_RECORD_TYPE_NOTICE},
      {"removeLocalAddressConfirmationDescription",
       IDS_AUTOFILL_DELETE_LOCAL_ADDRESS_RECORD_TYPE_NOTICE},
      {"removeLocalCreditCardConfirmationTitle",
       IDS_SETTINGS_LOCAL_CARD_REMOVE_CONFIRMATION_TITLE},
      {"removeLocalPaymentMethodConfirmationDescription",
       IDS_SETTINGS_LOCAL_PAYMENT_METHOD_REMOVE_CONFIRMATION_DESCRIPTION},
      {"addressRemovedMessage", IDS_SETTINGS_ADDRESS_REMOVED_MESSAGE},
      {"editAddressRequiredFieldError",
       IDS_AUTOFILL_EDIT_ADDRESS_REQUIRED_FIELD_FORM_ERROR},
      {"editAddressRequiredFieldsError",
       IDS_AUTOFILL_EDIT_ADDRESS_REQUIRED_FIELDS_FORM_ERROR},
      {"creditCardExpiration", IDS_SETTINGS_CREDIT_CARD_EXPIRATION_DATE},
      {"creditCardName", IDS_SETTINGS_NAME_ON_CREDIT_CARD},
      {"creditCardNickname", IDS_SETTINGS_CREDIT_CARD_NICKNAME},
      {"creditCardNicknameInvalid", IDS_SETTINGS_CREDIT_CARD_NICKNAME_INVALID},
      {"creditCardNumberInvalid",
       IDS_PAYMENTS_CARD_NUMBER_INVALID_VALIDATION_MESSAGE},
      {"creditCardCvcInputTitle", IDS_SETTINGS_CREDIT_CARD_CVC_TITLE},
      {"creditCardCvcImageTitle", IDS_SETTINGS_CREDIT_CARD_CVC_IMAGE_TITLE},
      {"creditCardCvcAmexImageTitle",
       IDS_SETTINGS_CREDIT_CARD_CVC_IMAGE_TITLE_AMEX},
      {"creditCardCvcInputPlaceholder",
       IDS_SETTINGS_CREDIT_CARD_CVC_PLACEHOLDER},
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
      {"ibanSavedToThisDeviceOnly",
       IDS_SETTINGS_IBAN_SAVED_TO_THIS_DEVICE_ONLY},
      {"addIbanTitle", IDS_SETTINGS_ADD_IBAN_TITLE},
      {"editIbanTitle", IDS_SETTINGS_EDIT_IBAN_TITLE},
      {"ibanInvalid", IDS_SETTINGS_IBAN_INVALID_VALIDATION_MESSAGE},
      {"ibanNickname", IDS_IBAN_NICKNAME},
      {"moreActionsForIban", IDS_SETTINGS_AUTOFILL_MORE_ACTIONS_FOR_IBAN},
      {"a11yIbanDescription", IDS_SETTINGS_AUTOFILL_A11Y_IBAN_DESCRIPTION},
      {"editIban", IDS_SETTINGS_IBAN_EDIT},
      {"removeLocalIbanConfirmationTitle",
       IDS_SETTINGS_LOCAL_IBAN_REMOVE_CONFIRMATION_TITLE},
      {"migrateCreditCardsLabel", IDS_SETTINGS_MIGRATABLE_CARDS_LABEL},
      {"migratableCardsInfoSingle", IDS_SETTINGS_SINGLE_MIGRATABLE_CARD_INFO},
      {"migratableCardsInfoMultiple",
       IDS_SETTINGS_MULTIPLE_MIGRATABLE_CARDS_INFO},
      {"remotePaymentMethodsLinkLabel",
       IDS_SETTINGS_REMOTE_PAYMENT_METHODS_LINK_LABEL},
      {"canMakePaymentToggleLabel", IDS_SETTINGS_CAN_MAKE_PAYMENT_TOGGLE_LABEL},
      {"autofillDetail", IDS_SETTINGS_AUTOFILL_DETAIL},
      {"passwords", IDS_SETTINGS_PASSWORD_MANAGER},
      {"passwordsLeakDetectionLabel",
       IDS_SETTINGS_PASSWORDS_LEAK_DETECTION_LABEL},
      {"passwordsLeakDetectionGeneralDescription",
       IDS_PASSWORD_MANAGER_LEAK_HELP_MESSAGE},
      {"passwordsLeakDetectionSignedOutEnabledDescription",
       IDS_SETTINGS_PASSWORDS_LEAK_DETECTION_SIGNED_OUT_ENABLED_DESC},
      {"editPasskeySiteLabel", IDS_SETTINGS_PASSKEYS_SITE_LABEL},
      {"editPasskeyUsernameLabel",
       IDS_SETTINGS_SECURITY_KEYS_CREDENTIAL_USERNAME_LABEL},
      {"benefitsTermsAriaLabel",
       IDS_AUTOFILL_SETTINGS_PAGE_BENEFITS_TERMS_ARIA_LABEL},
#if BUILDFLAG(IS_MAC)
      {"passkeyLengthError", IDS_SETTINGS_PASSKEYS_LENGTH_ERROR},
      {"editPasskeyDialogTitle", IDS_SETTINGS_PASSKEYS_DIALOG_TITLE},
      {"passkeyEditDialogFootnote", IDS_SETTINGS_PASSKEYS_EDIT_DIALOG_FOOTNOTE},
#endif
      {"noAddressesFound", IDS_SETTINGS_ADDRESS_NONE},
      {"noSearchResults", IDS_SEARCH_NO_RESULTS},
      {"searchResultsPlural", IDS_SEARCH_RESULTS_PLURAL},
      {"searchResultsSingular", IDS_SEARCH_RESULTS_SINGULAR},
      {"addVirtualCard", IDS_AUTOFILL_ADD_VIRTUAL_CARD},
      {"savedToThisDeviceOnly",
       IDS_SETTINGS_PAYMENTS_SAVED_TO_THIS_DEVICE_ONLY},
      {"localPasswordManager",
       IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SAVING_ON_DEVICE},
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
      {"managePasskeysSubTitle", IDS_AUTOFILL_MANAGE_PASSKEYS_SUB_TITLE_MAC},
#elif BUILDFLAG(IS_WIN)
      {"managePasskeysSubTitle", IDS_AUTOFILL_MANAGE_PASSKEYS_SUB_TITLE_WIN},
#endif
      {"plusAddressSettings", IDS_PLUS_ADDRESS_SETTINGS_LABEL},
      {"plusAddressSettingsSublabel", IDS_PLUS_ADDRESS_SETTINGS_SUBLABEL},
      {"cvcTagForCreditCardListEntry",
       IDS_AUTOFILL_SETTINGS_PAGE_CVC_TAG_FOR_CREDIT_CARD_LIST_ENTRY},
      {"benefitsTermsTagForCreditCardListEntry",
       IDS_AUTOFILL_SETTINGS_PAGE_BENEFITS_TERMS_TAG_FOR_CREDIT_CARD_LIST_ENTRY},
      {"cardBenefitsLabel", IDS_AUTOFILL_SETTINGS_PAGE_CARD_BENEFITS_LABEL},
      {"aiPageTitle", IDS_SETTINGS_AI_PAGE_TITLE},
      {"aiPageMainLabel", IDS_SETTINGS_AI_PAGE_MAIN_LABEL},
      {"aiPageMainSublabel", IDS_SETTINGS_AI_PAGE_MAIN_SUBLABEL},
      {"aiComposeLabel", IDS_SETTINGS_AI_COMPOSE_LABEL},
      {"aiComposeSublabel", IDS_SETTINGS_AI_COMPOSE_SUBLABEL},
      {"experimentalAdvancedFeature2Label",
       IDS_SETTINGS_EXPERIMENTAL_ADVANCED_FEATURE2_LABEL},
      {"experimentalAdvancedFeature2Sublabel",
       IDS_SETTINGS_EXPERIMENTAL_ADVANCED_FEATURE2_SUBLABEL},
      {"experimentalAdvancedFeature3Label",
       IDS_SETTINGS_EXPERIMENTAL_ADVANCED_FEATURE3_LABEL},
      {"experimentalAdvancedFeature3Sublabel",
       IDS_SETTINGS_EXPERIMENTAL_ADVANCED_FEATURE3_SUBLABEL},
      {"autofillPredictionImprovementsPageTitle",
       IDS_SETTINGS_AUTOFILL_PREDICTION_IMPROVEMENTS_PAGE_TITLE},
      {"autofillPredictionImprovementsWhenOnSavedInfo",
       IDS_SETTINGS_AUTOFILL_PREDICTION_IMPROVEMENTS_WHEN_ON_SAVED_INFO},
      {"autofillPredictionImprovementsUseToFill",
       IDS_SETTINGS_AUTOFILL_PREDICTION_IMPROVEMENTS_WHEN_ON_USE_TO_FILL},
      {"autofillPredictionImprovementsNewFeature",
       IDS_SETTINGS_AUTOFILL_PREDICTION_IMPROVEMENTS_TO_CONSIDER_NEW_FEATURE},
      {"autofillPredictionImprovementsToConsiderDataUsage",
       IDS_SETTINGS_AUTOFILL_PREDICTION_IMPROVEMENTS_TO_CONSIDER_DATA_USAGE},
      {"autofillPredictionImprovementsToConsiderDataStorage",
       IDS_SETTINGS_AUTOFILL_PREDICTION_IMPROVEMENTS_TO_CONSIDER_STORAGE},
      {"autofillPredictionImprovementsToConsiderDataImprovement",
       IDS_SETTINGS_AUTOFILL_PREDICTION_IMPROVEMENTS_TO_CONSIDER_IMPROVEMENT},
      {"autofillPredictionImprovementsUserAnnotationsHeader",
       IDS_SETTINGS_AUTOFILL_PREDICTION_IMPROVEMENTS_USER_ANNOTATIONS_HEADER},
      {"autofillPredictionImprovementsUserAnnotationsNone",
       IDS_SETTINGS_AUTOFILL_PREDICTION_IMPROVEMENTS_USER_ANNOTATIONS_NONE},
      {"autofillPredictionImprovementsDeleteEntryDialogTitle",
       IDS_SETTINGS_AUTOFILL_PREDICTION_IMPROVEMENTS_DELETE_ENTRY_DIALOG_TITLE},
      {"autofillPredictionImprovementsDeleteEntryDialogText",
       IDS_SETTINGS_AUTOFILL_PREDICTION_IMPROVEMENTS_DELETE_ENTRY_DIALOG_TEXT},
      {"autofillPredictionImprovementsDeleteAllEntriesButtonLabel",
       IDS_SETTINGS_AUTOFILL_PREDICTION_IMPROVEMENTS_DELETE_ALL_ENTRIES_BUTTON_LABEL},
      {"autofillPredictionImprovementsDeleteAllEntriesDialogTitle",
       IDS_SETTINGS_AUTOFILL_PREDICTION_IMPROVEMENTS_DELETE_ALL_ENTRIES_DIALOG_TITLE},
      {"autofillPredictionImprovementsDeleteAllEntriesDialogText",
       IDS_SETTINGS_AUTOFILL_PREDICTION_IMPROVEMENTS_DELETE_ALL_ENTRIES_DIALOG_TEXT},
  };

  html_source->AddString("manageAddressesUrl",
                         autofill::payments::GetManageAddressesUrl().spec());
  html_source->AddString(
      "manageCreditCardsLabel",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_PAYMENTS_MANAGE_CREDIT_CARDS,
          base::UTF8ToUTF16(
              autofill::payments::GetManageInstrumentsUrl().spec())));
  html_source->AddString("managePaymentMethodsUrl",
                         autofill::payments::GetManageInstrumentsUrl().spec());
  html_source->AddString("addressesAndPaymentMethodsLearnMoreURL",
                         chrome::kAddressesAndPaymentMethodsLearnMoreURL);
  html_source->AddString(
      "signedOutUserLabel",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_SIGNED_OUT_USER_LABEL,
                                 chrome::kSyncLearnMoreURL));
  html_source->AddString("trustedVaultOptInUrl",
                         chrome::kSyncTrustedVaultOptInURL);
  html_source->AddString("trustedVaultLearnMoreUrl",
                         chrome::kSyncTrustedVaultLearnMoreURL);
  html_source->AddString("wallpaperSearchLearnMoreUrl",
                         chrome::kWallpaperSearchLearnMorePageURL);
  html_source->AddString("tabOrganizationLearnMoreUrl",
                         chrome::kTabOrganizationLearnMorePageURL);

  bool is_guest_mode = false;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  is_guest_mode =
      user_manager::UserManager::Get()->IsLoggedInAsGuest() ||
      user_manager::UserManager::Get()->IsLoggedInAsManagedGuestSession();
#else   // !BUILDFLAG(IS_CHROMEOS_ASH)
  is_guest_mode = profile->IsOffTheRecord();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForBrowserContext(profile);
  html_source->AddBoolean(
      "migrationEnabled",
      !is_guest_mode &&
          autofill::IsCreditCardMigrationEnabled(
              personal_data, SyncServiceFactory::GetForProfile(profile),
              /*is_test_mode=*/false,
              /*log_manager=*/nullptr));

  html_source->AddBoolean("showIbansSettings",
                          autofill::ShouldShowIbanOnSettingsPage(
                              personal_data->payments_data_manager()
                                  .GetCountryCodeForExperimentGroup(),
                              profile->GetPrefs()));

  html_source->AddBoolean("deviceAuthAvailable",
                          CheckDeviceAuthAvailability(web_contents));

  html_source->AddBoolean("cvcStorageAvailable", CheckCvcStorageAvailability());

  html_source->AddBoolean(
      "autofillCardBenefitsAvailable",
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableCardBenefitsForAmericanExpress) ||
          base::FeatureList::IsEnabled(
              autofill::features::kAutofillEnableCardBenefitsForCapitalOne));

  html_source->AddString(
      "cardBenefitsToggleSublabel",
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_SETTINGS_PAGE_CARD_BENEFITS_TOGGLE_SUBLABEL_WITH_LEARN_LINK,
          l10n_util::GetStringUTF16(IDS_SETTINGS_OPENS_IN_NEW_TAB)));

  ui::Accelerator undo_accelerator(ui::VKEY_Z, ui::EF_PLATFORM_ACCELERATOR);
  html_source->AddString(
      "undoDescription",
      l10n_util::GetStringFUTF16(IDS_UNDO_DESCRIPTION,
                                 undo_accelerator.GetShortcutText()));
  html_source->AddString(
      "unenrollVirtualCardDialogLabel",
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_UNENROLL_DIALOG_LABEL,
          base::UTF8ToUTF16(
              autofill::payments::GetVirtualCardEnrollmentSupportUrl()
                  .spec())));

  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddBoolean(
      "syncEnableContactInfoDataTypeInTransportMode",
      base::FeatureList::IsEnabled(
          syncer::kSyncEnableContactInfoDataTypeInTransportMode));

  plus_addresses::PlusAddressService* plus_address_service =
      PlusAddressServiceFactory::GetInstance()->GetForBrowserContext(profile);
  html_source->AddBoolean(
      "plusAddressEnabled",
      plus_address_service && plus_address_service->IsEnabled());
  html_source->AddString(
      "plusAddressManagementUrl",
      plus_addresses::features::kPlusAddressManagementUrl.Get());

  html_source->AddBoolean(
      "requireValidLocalCards",
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillRequireValidLocalCardsInSettings));

  html_source->AddBoolean(
      "autofillPredictionImprovementsEnabled",
      base::FeatureList::IsEnabled(
          autofill_prediction_improvements::kAutofillPredictionImprovements));
  html_source->AddString(
      "autofillPredictionImprovementsToggleSubLabel",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_AUTOFILL_PREDICTION_IMPROVEMENTS_TOGGLE_SUB_LABEL,
          base::ASCIIToUTF16(
              std::string(chrome::kAddressesAndPaymentMethodsLearnMoreURL))));
}

void AddSignOutDialogStrings(content::WebUIDataSource* html_source,
                             Profile* profile) {
  if (base::FeatureList::IsEnabled(
          supervised_user::kCustomProfileStringsForSupervisedUsers) &&
      profile->IsChild()) {
    static constexpr webui::LocalizedString kTurnOffStrings[] = {
        {"syncDisconnect", IDS_SETTINGS_PEOPLE_SYNC_TURN_OFF},
        {"syncDisconnectTitle",
         IDS_SETTINGS_TURN_OFF_SYNC_AND_SIGN_OUT_DIALOG_TITLE_SUPERVISED_PROFILE},
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

  if (base::FeatureList::IsEnabled(
          supervised_user::kCustomProfileStringsForSupervisedUsers) &&
      profile->IsChild()) {
    static constexpr webui::LocalizedString kSyncDisconnectStrings[] = {
        {"syncDisconnectDeleteProfile",
         IDS_SETTINGS_TURN_OFF_SYNC_DIALOG_CHECKBOX},
        {"syncDisconnectConfirm",
         IDS_SETTINGS_TURN_OFF_SYNC_DIALOG_MANAGED_CONFIRM},
        {"syncDisconnectExplanation",
         IDS_SETTINGS_SYNC_DISCONNECT_AND_SIGN_OUT_EXPLANATION_SUPERVISED_PROFILE},
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
      {"signedInTo", IDS_SETTINGS_PEOPLE_SIGNED_IN_TO_ACCOUNT},
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

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
      {"syncAdvancedPageTitle", IDS_SETTINGS_NEW_SYNC_ADVANCED_PAGE_TITLE},
#endif
      {"verifyAccount", IDS_SETTINGS_PEOPLE_VERIFY_ACCOUNT_BUTTON},
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
      {"enablePersonalizationLoggingDesc",
       IDS_SETTINGS_ENABLE_LOGGING_PREF_DESC},
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
      {"pageContentLinkRowSublabelOn",
       IDS_SETTINGS_PAGE_CONTENT_LINK_ROW_SUBLABEL_ON},
      {"pageContentLinkRowSublabelOff",
       IDS_SETTINGS_PAGE_CONTENT_LINK_ROW_SUBLABEL_OFF},
      {"pageContentPageTitle", IDS_SETTINGS_PAGE_CONTENT_PAGE_TITLE},
      {"pageContentToggleLabel", IDS_SETTINGS_PAGE_CONTENT_TOGGLE_LABEL},
      {"pageContentToggleSublabel", IDS_SETTINGS_PAGE_CONTENT_TOGGLE_SUBLABEL},
      {"pageContentWhenOnBulletOne",
       IDS_SETTINGS_PAGE_CONTENT_WHEN_ON_BULLET_ONE},
      {"pageContentThingsToConsiderBulletOne",
       IDS_SETTINGS_PAGE_CONTENT_THINGS_TO_CONSIDER_BULLET_ONE},
      {"pageContentThingsToConsiderBulletTwo",
       IDS_SETTINGS_PAGE_CONTENT_THINGS_TO_CONSIDER_BULLET_TWO},
      {"pageContentThingsToConsiderBulletThree",
       IDS_SETTINGS_PAGE_CONTENT_THINGS_TO_CONSIDER_BULLET_THREE},
      {"chromeSigninChoiceTitle",
       IDS_SETTINGS_SIGNIN_CHROME_SIGNIN_OPTIONS_TITLE},
      {"chromeSigninChoiceDescription",
       IDS_SETTINGS_SIGNIN_CHROME_SIGNIN_OPTIONS_DESC},
      {"chromeSigninChoiceSelectOptionPlaceholder",
       IDS_SETTINGS_SIGNIN_CHROME_SIGNIN_OPTION_PLACEHOLDER},
      {"chromeSigninChoiceSignin",
       IDS_SETTINGS_SIGNIN_CHROME_SIGNIN_OPTION_SIGNIN},
      {"chromeSigninChoiceDoNotSignin",
       IDS_SETTINGS_SIGNIN_CHROME_SIGNIN_OPTION_DO_NOT_SIGNIN},
      {"chromeSigninChoiceAlwaysAsk",
       IDS_SETTINGS_SIGNIN_CHROME_SIGNIN_OPTION_ALWAYS_ASK},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

void AddBrowserSyncPageStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"peopleSignInSyncPagePromptSecondaryWithAccount",
       IDS_SETTINGS_PEOPLE_SIGN_IN_PROMPT_SECONDARY_WITH_ACCOUNT},
      {"peopleSignInSyncPagePromptSecondaryWithNoAccount",
       IDS_SETTINGS_PEOPLE_SIGN_IN_PROMPT_SECONDARY_WITH_ACCOUNT},
      {"bookmarksCheckboxLabel", IDS_SETTINGS_BOOKMARKS_CHECKBOX_LABEL},
      {"readingListCheckboxLabel", IDS_SETTINGS_READING_LIST_CHECKBOX_LABEL},
      {"cancelSync", IDS_SETTINGS_SYNC_SETTINGS_CANCEL_SYNC},
      {"syncSetupCancelDialogTitle",
       IDS_SETTINGS_SYNC_SETUP_CANCEL_DIALOG_TITLE},
      {"syncSetupCancelDialogBody", IDS_SETTINGS_SYNC_SETUP_CANCEL_DIALOG_BODY},
      {"personalizeGoogleServicesTitle",
       IDS_SETTINGS_PERSONALIZE_GOOGLE_SERVICES_TITLE},
      {"personalizeGoogleServicesTitleV2",
       IDS_SETTINGS_PERSONALIZE_GOOGLE_SERVICES_TITLE_V2},
      {"personalizeGoogleServicesDesc",
       IDS_SETTINGS_PERSONALIZE_GOOGLE_SERVICES_DESC},
      {"personalizeGoogleServicesDescWithLinkedServices",
       IDS_SETTINGS_PERSONALIZE_GOOGLE_SERVICES_DESC_WITH_LINKED_SERVICES},
      {"personalizeGoogleServicesWaaTitle",
       IDS_SETTINGS_PERSONALIZE_GOOGLE_SERVICES_WAA_TITLE},
      {"personalizeGoogleServicesLinkedServicesTitle",
       IDS_SETTINGS_PERSONALIZE_GOOGLE_SERVICES_LINKED_SERVICES_TITLE},
      {"personalizeGoogleServicesLinkedServicesDesc",
       IDS_SETTINGS_PERSONALIZE_GOOGLE_SERVICES_LINKED_SERVICES_DESC},
      {"themeCheckboxLabel", IDS_SETTINGS_THEME_CHECKBOX_LABEL},
#if BUILDFLAG(IS_CHROMEOS)
      {"browserSyncFeatureLabel", IDS_BROWSER_SETTINGS_SYNC_FEATURE_LABEL},
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
      {"cookiesCheckboxLabel", IDS_SETTINGS_COOKIES_CHECKBOX_LABEL},
#endif
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString("activityControlsUrl",
                         chrome::kGoogleAccountActivityControlsURL);
  html_source->AddString(
      "activityControlsUrlInPrivacyGuide",
      chrome::kGoogleAccountActivityControlsURLInPrivacyGuide);

  html_source->AddString("linkedServicesUrl",
                         chrome::kGoogleAccountLinkedServicesURL);

  html_source->AddLocalizedString(
      "passwordsCheckboxLabel",
      base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials)
          ? IDS_SETTINGS_PASSWORDS_AND_PASSKEYS_CHECKBOX_LABEL
          : IDS_SETTINGS_PASSWORDS_CHECKBOX_LABEL);

#if BUILDFLAG(IS_CHROMEOS)
  html_source->AddString(
      "osSyncSetupSettingsUrl",
      chrome::GetOSSettingsUrl(chromeos::settings::mojom::kSyncSetupSubpagePath)
          .spec());
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  html_source->AddString("osSettingsPrivacyHubSubpageUrl",
                         chrome::GetOSSettingsUrl(
                             chromeos::settings::mojom::kPrivacyHubSubpagePath)
                             .spec());
#endif

#if BUILDFLAG(IS_CHROMEOS)
  html_source->AddString(
      "osSyncSettingsUrl",
      chrome::GetOSSettingsUrl(chromeos::settings::mojom::kSyncSubpagePath)
          .spec());
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
      {"productComparisonsCheckboxLabel",
       IDS_SETTINGS_PRODUCT_COMPARISONS_CHECKBOX_LABEL},
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
      {"accountManagerSubMenuLabel",
       IDS_SETTINGS_ACCOUNT_MANAGER_SUBMENU_LABEL},
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
      {"greyDefaultColorName", IDS_NTP_CUSTOMIZE_GREY_DEFAULT_LABEL},
      {"hueSliderTitle", IDS_NTP_CUSTOMIZE_COLOR_HUE_SLIDER_TITLE},
      {"hueSliderAriaLabel", IDS_NTP_CUSTOMIZE_COLOR_HUE_SLIDER_ARIA_LABEL},
      {"managedColorsBody", IDS_NTP_THEME_MANAGED_DIALOG_BODY},
      {"managedColorsTitle", IDS_NTP_THEME_MANAGED_DIALOG_TITLE},

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

      // History search strings:
      {"historySearchSettingLabel", IDS_SETTINGS_HISTORY_SEARCH_SETTING_LABEL},
      {"historySearchSettingSublabel",
       IDS_SETTINGS_HISTORY_SEARCH_SETTING_SUBLABEL},
      {"historySearchWhenOnBulletOne",
       IDS_SETTINGS_HISTORY_SEARCH_WHEN_ON_BULLET_ONE},
      {"historySearchConsiderBulletOne",
       IDS_SETTINGS_HISTORY_SEARCH_CONSIDER_BULLET_ONE},
      {"historySearchConsiderBulletTwo",
       IDS_SETTINGS_HISTORY_SEARCH_CONSIDER_BULLET_TWO},
      {"historySearchLearnMoreA11yLabel",
       IDS_SETTINGS_HISTORY_SEARCH_LEARN_MORE_A11Y_LABEL},
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
  html_source->AddString("historySearchLearnMoreUrl",
                         chrome::kHistorySearchLearnMorePageURL);
  html_source->AddString("historySearchDataHomeUrl",
                         chrome::kChromeUIHistoryURL);

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
  html_source->AddString(
      "osSettingsAccountsPageUrl",
      chrome::GetOSSettingsUrl(
          ash::features::IsOsSettingsRevampWayfindingEnabled()
              ? chromeos::settings::mojom::kPeopleSectionPath
              : chromeos::settings::mojom::kMyAccountsSubpagePath)
          .spec());
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

void AddPrivacyStrings(content::WebUIDataSource* html_source,
                       Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"privacyPageTitle", IDS_SETTINGS_PRIVACY},
      {"privacyPageMore", IDS_SETTINGS_PRIVACY_MORE},
      {"doNotTrack", IDS_SETTINGS_ENABLE_DO_NOT_TRACK},
      {"doNotTrackDialogTitle", IDS_SETTINGS_ENABLE_DO_NOT_TRACK_DIALOG_TITLE},
      {"doNotTrackDialogMessage", IDS_SETTINGS_ENABLE_DO_NOT_TRACK_DIALOG_TEXT},
      {"doNotTrackDialogLearnMoreA11yLabel",
       IDS_SETTINGS_ENABLE_DO_NOT_TRACK_DIALOG_LEARN_MORE_ACCESSIBILITY_LABEL},
      // TODO(crbug.com/40122957): This string is no longer used. Remove.
      {"permissionsPageTitle", IDS_SETTINGS_PERMISSIONS},
      {"permissionsPageDescription", IDS_SETTINGS_PERMISSIONS_DESCRIPTION},
      {"securityPageTitle", IDS_SETTINGS_SECURITY},
      {"securityPageDescription", IDS_SETTINGS_SECURITY_DESCRIPTION},
      {"advancedProtectionProgramTitle",
       IDS_SETTINGS_ADVANCED_PROTECTION_PROGRAM},
      {"advancedProtectionProgramDesc",
       IDS_SETTINGS_ADVANCED_PROTECTION_PROGRAM_DESC},
      {"secureConnectionsSectionTitle",
       IDS_SETTINGS_SECURE_CONNECTIONS_SECTION_TITLE},
      {"secureConnectionsSectionDescription",
       IDS_SETTINGS_SECURE_CONNECTIONS_SECTION_DESCRIPTION},
      {"httpsOnlyModeTitle", IDS_SETTINGS_HTTPS_ONLY_MODE},
      {"httpsOnlyModeDescription", IDS_SETTINGS_HTTPS_ONLY_MODE_DESCRIPTION},
      {"httpsOnlyModeDescriptionAdvancedProtection",
       IDS_SETTINGS_HTTPS_ONLY_MODE_DESCRIPTION_ADVANCED_PROTECTION},
      {"httpsFirstModeSectionLabel", IDS_SETTINGS_HTTPS_FIRST_MODE_TITLE},
      {"httpsFirstModeSectionSubLabel", IDS_SETTINGS_HTTPS_FIRST_MODE_SUBTITLE},
      {"httpsFirstModeEnabledFullLabel",
       IDS_SETTINGS_HTTPS_FIRST_MODE_ENABLED_FULL_LABEL},
      {"httpsFirstModeEnabledFullSubLabel",
       IDS_SETTINGS_HTTPS_FIRST_MODE_ENABLED_FULL_SUBLABEL},
      {"httpsFirstModeSectionTitle", IDS_SETTINGS_HTTPS_FIRST_MODE_TITLE},
      {"httpsFirstModeSectionDescription",
       IDS_SETTINGS_HTTPS_FIRST_MODE_DESCRIPTION},
      {"httpsFirstModeDescriptionAdvancedProtection",
       IDS_SETTINGS_HTTPS_FIRST_MODE_DESCRIPTION_ADVANCED_PROTECTION},
      {"httpsFirstModeEnabledStrictLabel",
       IDS_SETTINGS_HTTPS_FIRST_MODE_ENABLED_STRICT_LABEL},
      {"httpsFirstModeEnabledStrictSubLabel",
       IDS_SETTINGS_HTTPS_FIRST_MODE_ENABLED_STRICT_SUBLABEL},
      {"httpsFirstModeEnabledBalancedLabel",
       IDS_SETTINGS_HTTPS_FIRST_MODE_ENABLED_BALANCED_LABEL},
      {"httpsFirstModeEnabledBalancedSubLabel",
       IDS_SETTINGS_HTTPS_FIRST_MODE_ENABLED_BALANCED_SUBLABEL},
      {"httpsFirstModeEnabledIncognitoLabel",
       IDS_SETTINGS_HTTPS_FIRST_MODE_ENABLED_INCOGNITO_LABEL},
      {"httpsFirstModeEnabledIncognitoSubLabel",
       IDS_SETTINGS_HTTPS_FIRST_MODE_ENABLED_INCOGNITO_SUBLABEL},
      {"httpsFirstModeDisabledLabel",
       IDS_SETTINGS_HTTPS_FIRST_MODE_DISABLED_LABEL},
      {"httpsFirstModeDisabledSubLabel",
       IDS_SETTINGS_HTTPS_FIRST_MODE_DISABLED_SUBLABEL},
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
      {"safeBrowsingEnhancedWhenOnBulOne",
       IDS_SETTINGS_SAFEBROWSING_ENHANCED_WHEN_ON_BULLET_ONE},
      {"safeBrowsingEnhancedWhenOnBulTwo",
       IDS_SETTINGS_SAFEBROWSING_ENHANCED_WHEN_ON_BULLET_TWO},
      {"safeBrowsingEnhancedWhenOnBulThree",
       IDS_SETTINGS_SAFEBROWSING_ENHANCED_WHEN_ON_BULLET_THREE},
      {"safeBrowsingEnhancedWhenOnBulFour",
       IDS_SETTINGS_SAFEBROWSING_ENHANCED_WHEN_ON_BULLET_FOUR},
      {"safeBrowsingEnhancedWhenOnBulFive",
       IDS_SETTINGS_SAFEBROWSING_ENHANCED_WHEN_ON_BULLET_FIVE},
      {"safeBrowsingEnhancedThingsToConsiderBulOne",
       IDS_SETTINGS_SAFEBROWSING_ENHANCED_THINGS_TO_CONSIDER_BULLET_ONE},
      {"safeBrowsingEnhancedThingsToConsiderBulTwo",
       IDS_SETTINGS_SAFEBROWSING_ENHANCED_THINGS_TO_CONSIDER_BULLET_TWO},
      {"safeBrowsingEnhancedThingsToConsiderBulThree",
       IDS_SETTINGS_SAFEBROWSING_ENHANCED_THINGS_TO_CONSIDER_BULLET_THREE},
      {"safeBrowsingStandard", IDS_SETTINGS_SAFEBROWSING_STANDARD},
      {"safeBrowsingStandardDesc", IDS_SETTINGS_SAFEBROWSING_STANDARD_DESC},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      {"safeBrowsingStandardDescProxy",
       IDS_SETTINGS_SAFEBROWSING_STANDARD_DESC_PROXY},
#endif
      {"safeBrowsingStandardExpandA11yLabel",
       IDS_SETTINGS_SAFEBROWSING_STANDARD_EXPAND_ACCESSIBILITY_LABEL},
      {"safeBrowsingStandardBulOne",
       IDS_SETTINGS_SAFEBROWSING_STANDARD_BULLET_ONE},
      {"safeBrowsingStandardBulTwo",
       IDS_SETTINGS_SAFEBROWSING_STANDARD_BULLET_TWO},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      {"safeBrowsingStandardBulTwoProxy",
       IDS_SETTINGS_SAFEBROWSING_STANDARD_BULLET_TWO_PROXY},
#endif
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
      {"networkPredictionEnabledDesc",
       IDS_SETTINGS_NETWORK_PREDICTION_ENABLED_DESC},
      {"preloadingPageTitle", IDS_SETTINGS_PRELOAD_PAGES_TITLE},
      {"preloadingPageStandardPreloadingTitle",
       IDS_SETTINGS_PRELOAD_PAGES_STANDARD_PRELOADING_TITLE},
      {"preloadingPageStandardPreloadingSummary",
       IDS_SETTINGS_PRELOAD_PAGES_STANDARD_PRELOADING_SUMMARY},
      {"preloadingPageStandardPreloadingExpandA11yLabel",
       IDS_SETTINGS_PRELOAD_PAGES_STANDARD_PRELOADING_EXPAND_A11Y_LABEL},
      {"preloadingPageStandardPreloadingWhenOnBulletOne",
       IDS_SETTINGS_PRELOAD_PAGES_STANDARD_PRELOADING_WHEN_ON_BULLET_ONE},
      {"preloadingPageStandardPreloadingWhenOnBulletTwo",
       IDS_SETTINGS_PRELOAD_PAGES_STANDARD_PRELOADING_WHEN_ON_BULLET_TWO},
      {"preloadingPageExtendedPreloadingTitle",
       IDS_SETTINGS_PRELOAD_PAGES_EXTENDED_PRELOADING_TITLE},
      {"preloadingPageExtendedPreloadingSummary",
       IDS_SETTINGS_PRELOAD_PAGES_EXTENDED_PRELOADING_SUMMARY},
      {"preloadingPageExtendedPreloadingExpandA11yLabel",
       IDS_SETTINGS_PRELOAD_PAGES_EXTENDED_PRELOADING_EXPAND_A11Y_LABEL},
      {"preloadingPageExtendedPreloadingWhenOnBulletOne",
       IDS_SETTINGS_PRELOAD_PAGES_EXTENDED_PRELOADING_WHEN_ON_BULLET_ONE},
      {"preloadingPageExtendedPreloadingWhenOnBulletTwo",
       IDS_SETTINGS_PRELOAD_PAGES_EXTENDED_PRELOADING_WHEN_ON_BULLET_TWO},
      {"preloadingPageExtendedPreloadingThingsToConsiderBulletTwo",
       IDS_SETTINGS_PRELOAD_PAGES_EXTENDED_PRELOADING_THINGS_TO_CONSIDER_BULLET_TWO},
      {"preloadingPageThingsToConsiderBulletOne",
       IDS_SETTINGS_PRELOAD_PAGES_THINGS_TO_CONSIDER_BULLET_ONE},
      {"securityV8LinkTitle", IDS_SETTINGS_SECURITY_V8_LINK_TITLE},
      {"securityV8LinkDescription", IDS_SETTINGS_SECURITY_V8_LINK_DESCRIPTION},
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
      {"safeBrowsingEnhancedLearnMoreLabel",
       IDS_SETTINGS_SAFEBROWSING_ENHANCED_LEARN_MORE_LABEL}};
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString("cookiesSettingsHelpCenterURL",
                         chrome::kCookiesSettingsHelpCenterURL);

  html_source->AddString("trackingProtectionHelpCenterURL",
                         chrome::kTrackingProtectionHelpCenterURL);

  html_source->AddString("relatedWebsiteSetsLearnMoreURL",
                         chrome::kRelatedWebsiteSetsLearnMoreURL);

  html_source->AddString("safeBrowsingHelpCenterURL",
                         chrome::kSafeBrowsingHelpCenterUpdatedURL);

  html_source->AddString("syncAndGoogleServicesLearnMoreURL",
                         chrome::kSyncAndGoogleServicesLearnMoreURL);
  html_source->AddString("composeLearnMorePageURL",
                         chrome::kComposeLearnMorePageURL);
  html_source->AddString("doNotTrackLearnMoreURL",
                         chrome::kDoNotTrackLearnMoreURL);
  html_source->AddString("exceptionsLearnMoreURL",
                         chrome::kContentSettingsExceptionsLearnMoreURL);
  html_source->AddBoolean(
      "driveSuggestAvailable",
      base::FeatureList::IsEnabled(omnibox::kDocumentProvider));
  html_source->AddBoolean(
      "driveSuggestNoSetting",
      base::FeatureList::IsEnabled(omnibox::kDocumentProviderNoSetting));
  html_source->AddBoolean("driveSuggestNoSyncRequirement",
                          base::FeatureList::IsEnabled(
                              omnibox::kDocumentProviderNoSyncRequirement));
  html_source->AddString("enhancedProtectionHelpCenterURL",
                         chrome::kSafeBrowsingInChromeHelpCenterURL);

  // TODO(crbug.com/349860796): Add a learn-more link for HTTPS-First Mode for
  // the new Settings UI, which can be used by the settings-toggle-button.

  bool link_secure_dns = ShouldLinkSecureDnsOsSettings();
  html_source->AddBoolean("showSecureDnsSetting", !link_secure_dns);
#if BUILDFLAG(IS_CHROMEOS)
  html_source->AddBoolean("showSecureDnsSettingLink", link_secure_dns);
  html_source->AddString(
      "chromeOSPrivacyAndSecuritySectionPath",
      chromeos::settings::mojom::kPrivacyAndSecuritySectionPath);
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  html_source->AddString("chromeRootStoreHelpCenterURL",
                         chrome::kChromeRootStoreSettingsHelpCenterURL);
#endif  // BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
  html_source->AddString("certManagementV2URL",
                         chrome::kChromeUICertificateManagerDialogURL);
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

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
  AddSecureDnsStrings(html_source);

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
      {"privacyGuideCompletionCardPrivacySandboxSubLabelAdTopics",
       IDS_SETTINGS_PRIVACY_GUIDE_COMPLETION_CARD_PRIVACY_SANDBOX_SUB_LABEL_AD_TOPICS},
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
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      {"privacyGuideSafeBrowsingCardStandardProtectionFeatureDescription2Proxy",
       IDS_SETTINGS_PRIVACY_GUIDE_SAFE_BROWSING_CARD_STANDARD_PROTECTION_FEATURE_DESCRIPTION2_PROXY},
#endif
      {"privacyGuideSafeBrowsingCardStandardProtectionPrivacyDescription1",
       IDS_SETTINGS_PRIVACY_GUIDE_SAFE_BROWSING_CARD_STANDARD_PROTECTION_PRIVACY_DESCRIPTION1},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      {"privacyGuideSafeBrowsingCardStandardProtectionPrivacyDescription1Proxy",
       IDS_SETTINGS_PRIVACY_GUIDE_SAFE_BROWSING_CARD_STANDARD_PROTECTION_PRIVACY_DESCRIPTION1_PROXY},
#endif
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
      {"safetyHubUnusedSitePermissionsRemovedOnePermissionLabel",
       IDS_SETTINGS_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_REMOVED_ONE_PERMISSION_LABEL},
      {"safetyHubUnusedSitePermissionsRemovedTwoPermissionsLabel",
       IDS_SETTINGS_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_REMOVED_TWO_PERMISSIONS_LABEL},
      {"safetyHubUnusedSitePermissionsRemovedThreePermissionsLabel",
       IDS_SETTINGS_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_REMOVED_THREE_PERMISSIONS_LABEL},
      {"safetyHubUnusedSitePermissionsRemovedFourOrMorePermissionsLabel",
       IDS_SETTINGS_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_REMOVED_FOUR_OR_MORE_PERMISSIONS_LABEL},
      {"safetyCheckUnusedSitePermissionsToastLabel",
       IDS_SETTINGS_SAFETY_CHECK_UNUSED_SITE_PERMISSIONS_TOAST_LABEL},
      {"safetyCheckUnusedSitePermissionsUndoLabel",
       IDS_SETTINGS_SAFETY_CHECK_TOAST_UNDO_BUTTON_LABEL},
      {"safetyCheckUnusedSitePermissionsSettingLabel",
       IDS_SETTINGS_SAFETY_CHECK_UNUSED_SITE_PERMISSIONS_SETTING_LABEL},
      {"safetyCheckUnusedSitePermissionsSettingSublabel",
       IDS_SETTINGS_SAFETY_CHECK_UNUSED_SITE_PERMISSIONS_SETTING_SUBLABEL},
      {"safetyHubAbusiveNotificationPermissionsSettingSublabel",
       IDS_SETTINGS_SAFETY_HUB_ABUSIVE_NOTIFICATION_PERMISSIONS_SETTING_SUBLABEL},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

void AddSafetyHubStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"safetyHub", IDS_SETTINGS_SAFETY_HUB},
      {"safetyHubEntryPointHeader", IDS_SETTINGS_SAFETY_HUB_ENTRY_POINT_HEADER},
      {"safetyHubEntryPointNothingToDo",
       IDS_SETTINGS_SAFETY_HUB_ENTRY_POINT_NOTHING_TO_DO},
      {"safetyHubEntryPointButtonLabel",
       IDS_SETTINGS_SAFETY_HUB_ENTRY_POINT_BUTTON_LABEL},
      {"safetyHubPageCardSectionHeader",
       IDS_SETTINGS_SAFETY_HUB_PAGE_CARD_SECTION_HEADER},
      {"safetyHubPageModuleSectionHeader",
       IDS_SETTINGS_SAFETY_HUB_PAGE_MODULE_SECTION_HEADER},
      {"safetyHubEmptyStateModuleHeader",
       IDS_SETTINGS_SAFETY_HUB_EMPTY_STATE_MODULE_HEADER},
      {"safetyHubEmptyStateModuleSubheader",
       IDS_SETTINGS_SAFETY_HUB_EMPTY_STATE_MODULE_SUBHEADER},
      {"safetyHubGoSiteSettingsItem",
       IDS_SETTINGS_SAFETY_HUB_GO_SITE_SETTINGS_ITEM},
      {"safetyHubGoNotificationSettingsItem",
       IDS_SETTINGS_SAFETY_HUB_GO_NOTIFICATION_SETTINGS_ITEM},
      {"safetyHubUserEduModuleHeader",
       IDS_SETTINGS_SAFETY_HUB_USER_EDU_MODULE_HEADER},
      {"safetyHubUserEduDataHeader",
       IDS_SETTINGS_SAFETY_HUB_USER_EDU_DATA_HEADER},
      {"safetyHubUserEduIncognitoHeader",
       IDS_SETTINGS_SAFETY_HUB_USER_EDU_INCOGNITO_HEADER},
      {"safetyHubUserEduSafeBrowsingHeader",
       IDS_SETTINGS_SAFETY_HUB_USER_EDU_SAFE_BROWSING_HEADER},
      {"safetyHubPasswordNavigationAriaLabel",
       IDS_SETTINGS_SAFETY_HUB_PASSWORD_NAVIGATION_ARIA_LABEL},
      {"safetyHubVersionNavigationAriaLabel",
       IDS_SETTINGS_SAFETY_HUB_VERSION_NAVIGATION_ARIA_LABEL},
      {"safetyHubVersionRelaunchAriaLabel",
       IDS_SETTINGS_SAFETY_HUB_VERSION_RELAUNCH_ARIA_LABEL},
      {"safetyHubSBNavigationAriaLabel",
       IDS_SETTINGS_SAFETY_HUB_SB_NAVIGATION_ARIA_LABEL},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString("safetyHubUserEduDataSubheader",
                         l10n_util::GetStringFUTF16(
                             IDS_SETTINGS_SAFETY_HUB_USER_EDU_DATA_SUBHEADER,
                             chrome::kChromeSafePageURL));

  html_source->AddString(
      "safetyHubUserEduIncognitoSubheader",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_SAFETY_HUB_USER_EDU_INCOGNITO_SUBHEADER,
          chrome::kIncognitoHelpCenterURL));

  html_source->AddString(
      "safetyHubUserEduSafeBrowsingSubheader",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_SAFETY_HUB_USER_EDU_SAFE_BROWSING_SUBHEADER,
          chrome::kSafeBrowsingUseInChromeURL));

  html_source->AddString("safetyHubHelpCenterURL",
                         chrome::kSafetyHubHelpCenterURL);
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
      IDS_SETTINGS_SEARCH_NO_RESULTS_HELP, chrome::kSettingsSearchHelpURL);
  html_source->AddString("searchNoResultsHelp", help_text);
}

void AddSearchStrings(content::WebUIDataSource* html_source, Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"searchEnginesManageSiteSearch",
       IDS_SETTINGS_SEARCH_MANAGE_SEARCH_ENGINES_AND_SITE_SEARCH},
      {"searchPageTitle", IDS_SETTINGS_SEARCH},
      {"searchExplanationLearnMoreA11yLabel",
       IDS_SETTINGS_SEARCH_EXPLANATION_ACCESSIBILITY_LABEL},
      {"searchEngineChoiceEntryPointSubtitle",
       IDS_SEARCH_ENGINE_CHOICE_SETTINGS_ENTRY_POINT_SUBTITLE},
      {"searchEnginesChange",
       IDS_SEARCH_ENGINE_CHOICE_SETTINGS_CHANGE_DEFAULT_ENGINE},
      {"searchEnginesSetAsDefaultButton",
       IDS_SEARCH_ENGINE_CHOICE_BUTTON_TITLE},
      {"searchEnginesCancelButton", IDS_CANCEL},
      {"searchEnginesConfirmationToastLabel",
       IDS_SEARCH_ENGINE_CHOICE_SETTINGS_CONFIRMATION_TOAST_LABEL},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
  html_source->AddString("searchExplanationLearnMoreURL",
                         chrome::kOmniboxLearnMoreURL);

  search_engines::SearchEngineChoiceService* search_engine_choice_service =
      search_engines::SearchEngineChoiceServiceFactory::GetForProfile(profile);
  int country_id = search_engine_choice_service
                       ? search_engine_choice_service->GetCountryId()
                       : country_codes::GetCurrentCountryID();
  html_source->AddLocalizedString(
      "searchEnginesSettingsDialogSubtitle",
      search_engines::IsEeaChoiceCountry(country_id)
          ? IDS_SEARCH_ENGINE_CHOICE_SETTINGS_SUBTITLE
          : IDS_SEARCH_ENGINE_CHOICE_SETTINGS_SUBTITLE_NON_EEA);

  html_source->AddLocalizedString(
      "saveGuestChoiceText", IDS_SEARCH_ENGINE_CHOICE_GUEST_SESSION_CHECKBOX);
}

void AddSearchEnginesStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"searchEnginesPageExplanation",
       IDS_SETTINGS_SEARCH_ENGINES_PAGE_EXPLANATION},
      {"searchEnginesAddSiteSearch",
       IDS_SETTINGS_SEARCH_ENGINES_ADD_SITE_SEARCH},
      {"searchEnginesEditSearchEngine",
       IDS_SETTINGS_SEARCH_ENGINES_EDIT_SEARCH_ENGINE},
      {"searchEnginesEditSiteSearch",
       IDS_SETTINGS_SEARCH_ENGINES_EDIT_SITE_SEARCH},
      {"searchEnginesViewSiteSearch",
       IDS_SETTINGS_SEARCH_ENGINES_VIEW_SITE_SEARCH},
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
      {"searchEnginesName", IDS_SETTINGS_SEARCH_ENGINES_NAME},
      {"searchEnginesShortcut", IDS_SETTINGS_SEARCH_ENGINES_SHORTCUT},
      {"searchEnginesQueryURL", IDS_SETTINGS_SEARCH_ENGINES_QUERY_URL},
      {"searchEnginesQueryURLExplanation",
       IDS_SETTINGS_SEARCH_ENGINES_QUERY_URL_EXPLANATION},
      {"searchEnginesMakeDefault", IDS_SETTINGS_SEARCH_ENGINES_MAKE_DEFAULT},
      {"searchEnginesActivate", IDS_SETTINGS_SEARCH_ENGINES_ACTIVATE},
      {"searchEnginesDeactivate", IDS_SETTINGS_SEARCH_ENGINES_DEACTIVATE},
      {"searchEnginesViewDetails", IDS_SETTINGS_SEARCH_ENGINES_VIEW_DETAILS},
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
      {"searchEnginesMoreActionsAriaLabel",
       IDS_SETTINGS_SEARCH_ENGINES_MORE_ACTIONS_ARIA_LABEL},
      {"searchEnginesActivateButtonAriaLabel",
       IDS_SETTINGS_SEARCH_ENGINES_ACTIVATE_BUTTON_ARIA_LABEL},
      {"searchEnginesAddButtonAriaLabel",
       IDS_SETTINGS_SEARCH_ENGINES_ADD_SITE_SEARCH_BUTTON_ARIA_LABEL},
      {"searchEnginesEditButtonAriaLabel",
       IDS_SETTINGS_SEARCH_ENGINES_EDIT_SEARCH_ENGINE_BUTTON_ARIA_LABEL},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

void AddSiteSettingsStrings(content::WebUIDataSource* html_source,
                            Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"addSite", IDS_SETTINGS_ADD_SITE},
      {"addSiteTitle", IDS_SETTINGS_ADD_SITE_TITLE},
      {"addSitesTitle", IDS_SETTINGS_ADD_SITES_TITLE},
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
      {"thirdPartyCookiesPageTitle",
       IDS_SETTINGS_THIRD_PARTY_COOKIES_PAGE_TITLE},
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
      {"trackingProtectionLinkRowLabel",
       IDS_SETTINGS_TRACKING_PROTECTION_LINK_ROW_LABEL},
      {"trackingProtectionLinkRowSubLabel",
       IDS_SETTINGS_TRACKING_PROTECTION_LINK_ROW_SUB_LABEL},
      {"thirdPartyCookiesLinkRowSublabelLimited",
       IDS_SETTINGS_THIRD_PARTY_COOKIES_LINK_ROW_SUB_LABEL_LIMITED},
      {"thirdPartyCookiesAlignedPageDescription",
       IDS_SETTINGS_THIRD_PARTY_COOKIES_ALIGNED_PAGE_DESCRIPTION},
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
      {"cookiePageBlockThirdIncognitoBulTwoRws",
       IDS_SETTINGS_COOKIES_BLOCK_THIRD_PARTY_INCOGNITO_BULLET_TWO_RWS},
      {"cookiePageRwsLabel",
       IDS_SETTINGS_COOKIES_RELATED_WEBSITE_SETS_TOGGLE_LABEL},
      {"cookiePageRwsSubLabel",
       IDS_SETTINGS_COOKIES_RELATED_WEBSITE_SETS_TOGGLE_SUB_LABEL},
      {"cookiePageAllSitesLink", IDS_SETTINGS_COOKIES_ALL_SITES_LINK},
      {"trackingProtectionPageTitle",
       IDS_SETTINGS_TRACKING_PROTECTION_PAGE_TITLE},
      {"trackingProtectionPageDescription",
       IDS_SETTINGS_TRACKING_PROTECTION_PAGE_DESCRIPTION},
      {"trackingProtectionBulletOne",
       IDS_SETTINGS_TRACKING_PROTECTION_BULLET_ONE},
      {"trackingProtectionBulletOneDescription",
       IDS_SETTINGS_TRACKING_PROTECTION_BULLET_ONE_DESCRIPTION},
      {"trackingProtectionBulletTwo",
       IDS_SETTINGS_TRACKING_PROTECTION_BULLET_TWO},
      {"trackingProtectionAdvancedLabel",
       IDS_SETTINGS_TRACKING_PROTECTION_ADVANCED_LABEL},
      {"trackingProtectionThirdPartyCookiesToggleLabel",
       IDS_SETTINGS_TRACKING_PROTECTION_THIRD_PARTY_COOKIES_TOGGLE_LABEL},
      {"trackingProtectionThirdPartyCookiesToggleSubLabel",
       IDS_SETTINGS_TRACKING_PROTECTION_THIRD_PARTY_COOKIES_TOGGLE_SUB_LABEL},
      {"trackingProtectionThirdPartyCookiesLearnMoreAriaLabel",
       IDS_SETTINGS_TRACKING_PROTECTION_THIRD_PARTY_COOKIES_LEARN_MORE_ARIA_LABEL},
      {"trackingProtectionIpProtectionToggleLabel",
       IDS_SETTINGS_TRACKING_PROTECTION_IP_PROTECTION_TOGGLE_LABEL},
      {"trackingProtectionFingerprintingProtectionToggleLabel",
       IDS_SETTINGS_TRACKING_PROTECTION_FINGERPRINTING_PROTECTION_TOGGLE_LABEL},
      {"trackingProtectionFingerprintingProtectionToggleSubLabel",
       IDS_SETTINGS_TRACKING_PROTECTION_FINGERPRINTING_PROTECTION_TOGGLE_SUB_LABEL},
      {"trackingProtectionDoNotTrackToggleSubLabel",
       IDS_SETTINGS_TRACKING_PROTECTION_DO_NOT_TRACK_TOGGLE_SUB_LABEL},
      {"trackingProtectionSitesAllowedCookiesTitle",
       IDS_SETTINGS_TRACKING_PROTECTION_SITES_ALLOWED_COOKIES_TITLE},
      {"trackingProtectionSitesAllowedCookiesDescription",
       IDS_SETTINGS_TRACKING_PROTECTION_SITES_ALLOWED_COOKIES_DESCRIPTION},
      {"siteSettingsCategoryAutomaticFullscreen",
       IDS_SITE_SETTINGS_TYPE_AUTOMATIC_FULLSCREEN},
      {"siteSettingsCategoryFederatedIdentityApi",
       IDS_SITE_SETTINGS_TYPE_FEDERATED_IDENTITY_API},
      {"siteSettingsCategoryHandlers", IDS_SITE_SETTINGS_TYPE_HANDLERS},
      {"siteSettingsCategoryImages", IDS_SITE_SETTINGS_TYPE_IMAGES},
      {"siteSettingsCategoryInsecureContent",
       IDS_SITE_SETTINGS_TYPE_INSECURE_CONTENT},
      {"siteSettingsCategoryLocation", IDS_SITE_SETTINGS_TYPE_LOCATION},
      {"siteSettingsCategoryJavascript", IDS_SITE_SETTINGS_TYPE_JAVASCRIPT},
      {"siteSettingsCategoryJavascriptOptimizer",
       IDS_SITE_SETTINGS_TYPE_JAVASCRIPT_OPTIMIZER},
      {"siteSettingsCategoryMicrophone", IDS_SITE_SETTINGS_TYPE_MIC},
      {"siteSettingsMicrophoneLabel", IDS_SITE_SETTINGS_TYPE_MIC},
      {"siteSettingsCategoryNotifications",
       IDS_SITE_SETTINGS_TYPE_NOTIFICATIONS},
      {"siteSettingsCategoryPopups", IDS_SITE_SETTINGS_TYPE_POPUPS_REDIRECTS},
      {"siteSettingsCategoryZoomLevels", IDS_SITE_SETTINGS_TYPE_ZOOM_LEVELS},
      {"siteSettingsAllSites", IDS_SETTINGS_SITE_SETTINGS_ALL_SITES},
      {"siteSettingsAllSitesDescription",
       IDS_SETTINGS_SITE_SETTINGS_ALL_SITES_DESCRIPTION},
      {"siteSettingsAllSitesSearch",
       IDS_SETTINGS_SITE_SETTINGS_ALL_SITES_SEARCH},
      {"siteSettingsAllSitesSort", IDS_SETTINGS_SITE_SETTINGS_ALL_SITES_SORT},
      {"siteSettingsAllSitesSortMethodMostVisited",
       IDS_SETTINGS_SITE_SETTINGS_ALL_SITES_SORT_METHOD_MOST_VISITED},
      {"siteSettingsAllSitesSortMethodStorage",
       IDS_SETTINGS_SITE_SETTINGS_ALL_SITES_SORT_METHOD_STORAGE},
      {"siteSettingsAllSitesSortMethodName",
       IDS_SETTINGS_SITE_SETTINGS_ALL_SITES_SORT_METHOD_NAME},
      {"siteSettingsFileSystemSiteListHeader",
       IDS_SETTINGS_SITE_SETTINGS_FILE_SYSTEM_SITE_LIST_HEADER},
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
      {"siteSettingsAutoPictureInPicture",
       IDS_SITE_SETTINGS_TYPE_AUTO_PICTURE_IN_PICTURE},
      {"siteSettingsAutoPictureInPictureMidSentence",
       IDS_SITE_SETTINGS_TYPE_AUTO_PICTURE_IN_PICTURE_MID_SENTENCE},
      {"siteSettingsBackgroundSync", IDS_SITE_SETTINGS_TYPE_BACKGROUND_SYNC},
      {"siteSettingsBackgroundSyncMidSentence",
       IDS_SITE_SETTINGS_TYPE_BACKGROUND_SYNC_MID_SENTENCE},
      {"siteSettingsCamera", IDS_SITE_SETTINGS_TYPE_CAMERA},
      {"siteSettingsCameraMidSentence",
       IDS_SITE_SETTINGS_TYPE_CAMERA_MID_SENTENCE},
      {"siteSettingsCapturedSurfaceControl",
       IDS_SITE_SETTINGS_TYPE_CAPTURED_SURFACE_CONTROL},
      {"siteSettingsCapturedSurfaceControlMidSentence",
       IDS_SITE_SETTINGS_TYPE_CAPTURED_SURFACE_CONTROL_MID_SENTENCE},
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
      {"siteSettingsJavascriptOptimizer",
       IDS_SITE_SETTINGS_TYPE_JAVASCRIPT_OPTIMIZER},
      {"siteSettingsJavascriptOptimizerMidsentence",
       IDS_SITE_SETTINGS_TYPE_JAVASCRIPT_OPTIMIZER},  // Deliberately the same
                                                      // form.
      {"siteSettingsSound", IDS_SITE_SETTINGS_TYPE_SOUND},
      {"siteSettingsSoundMidSentence",
       IDS_SITE_SETTINGS_TYPE_SOUND_MID_SENTENCE},
      {"siteSettingsPdfDocuments", IDS_SITE_SETTINGS_TYPE_PDF_DOCUMENTS},
      {"siteSettingsPdfDownloadPdfs",
       IDS_SETTINGS_SITE_SETTINGS_PDF_DOWNLOAD_PDFS},
      {"siteSettingsProtectedContent",
       IDS_SITE_SETTINGS_TYPE_PROTECTED_MEDIA_ID},
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
      {"siteSettingsBluetoothDevices",
       IDS_SITE_SETTINGS_TYPE_BLUETOOTH_DEVICES},
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
      {"siteSettingsRelatedWebsiteSetsLearnMore",
       IDS_SETTINGS_SITE_SETTINGS_RELATED_WEBSITE_SETS_LEARN_MORE},
      {"siteSettingsRelatedWebsiteSetsLearnMoreAccessibility",
       IDS_SETTINGS_SITE_SETTINGS_RELATED_WEBSITE_SETS_LEARN_MORE_ACCESSIBILITY},
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
      {"relatedWebsiteSetsMoreActionsTitle",
       IDS_SETTINGS_SITE_SETTINGS_RELATED_WEBSITE_SETS_MORE_ACTIONS_TITLE},
      {"relatedWebsiteSetsShowRelatedSitesButton",
       IDS_SETTINGS_SITE_SETTINGS_RELATED_WEBSITE_SETS_SHOW_RELATED_SITES_BUTTON},
      {"relatedWebsiteSetsSiteDeleteStorageButton",
       IDS_SETTINGS_SITE_SETTINGS_RELATED_WEBSITE_SETS_SITE_DELETE_STORAGE_BUTTON},
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
      {"siteSettingsSiteResetDialogTitle",
       IDS_SETTINGS_SITE_SETTINGS_SITE_RESET_DIALOG_TITLE},
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
      {"siteSettingsAdsDescription",
       IDS_SETTINGS_SITE_SETTINGS_ADS_DESCRIPTION},
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
      {"siteSettingsAutomaticFullscreen",
       IDS_SITE_SETTINGS_TYPE_AUTOMATIC_FULLSCREEN},
      {"siteSettingsAutomaticFullscreenMidSentence",
       IDS_SITE_SETTINGS_TYPE_AUTOMATIC_FULLSCREEN_MID_SENTENCE},
      {"siteSettingsAutomaticFullscreenDescription",
       IDS_SETTINGS_SITE_SETTINGS_AUTOMATIC_FULLSCREEN_DESCRIPTION},
      {"siteSettingsAutomaticFullscreenAllowedExceptions",
       IDS_SETTINGS_SITE_SETTINGS_AUTOMATIC_FULLSCREEN_ALLOWED_EXCEPTIONS},
      {"siteSettingsAutomaticFullscreenBlockedExceptions",
       IDS_SETTINGS_SITE_SETTINGS_AUTOMATIC_FULLSCREEN_BLOCKED_EXCEPTIONS},
      {"siteSettingsAutoPictureInPictureDescription",
       IDS_SETTINGS_SITE_SETTINGS_AUTO_PICTURE_IN_PICTURE_DESCRIPTION},
      {"siteSettingsAutoPictureInPictureAllowed",
       IDS_SETTINGS_SITE_SETTINGS_AUTO_PICTURE_IN_PICTURE_ALLOWED},
      {"siteSettingsAutoPictureInPictureBlocked",
       IDS_SETTINGS_SITE_SETTINGS_AUTO_PICTURE_IN_PICTURE_BLOCKED},
      {"siteSettingsAutoPictureInPictureAllowedExceptions",
       IDS_SETTINGS_SITE_SETTINGS_AUTO_PICTURE_IN_PICTURE_ALLOWED_EXCEPTIONS},
      {"siteSettingsAutoPictureInPictureBlockedExceptions",
       IDS_SETTINGS_SITE_SETTINGS_AUTO_PICTURE_IN_PICTURE_BLOCKED_EXCEPTIONS},
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
      {"siteSettingsContentCameraBlockedByOs",
       IDS_SETTINGS_SITE_SETTINGS_CAMERA_BLOCKED_BY_OS},
      {"siteSettingsCameraBlockedSubLabel",
       IDS_SETTINGS_SITE_SETTINGS_CAMERA_BLOCKED_SUB_LABEL},
      {"siteSettingsCameraAllowedExceptions",
       IDS_SETTINGS_SITE_SETTINGS_CAMERA_ALLOWED_EXCEPTIONS},
      {"siteSettingsCameraBlockedExceptions",
       IDS_SETTINGS_SITE_SETTINGS_CAMERA_BLOCKED_EXCEPTIONS},
      {"siteSettingsCapturedSurfaceControlDescription",
       IDS_SETTINGS_SITE_SETTINGS_CAPTURED_SURFACE_CONTROL_DESCRIPTION},
      {"siteSettingsCapturedSurfaceControlAllowed",
       IDS_SETTINGS_SITE_SETTINGS_CAPTURED_SURFACE_CONTROL_ALLOWED},
      {"siteSettingsCapturedSurfaceControlBlocked",
       IDS_SETTINGS_SITE_SETTINGS_CAPTURED_SURFACE_CONTROL_BLOCKED},
      {"siteSettingsCapturedSurfaceControlAllowedExceptions",
       IDS_SETTINGS_SITE_SETTINGS_CAPTURED_SURFACE_CONTROL_ALLOWED_EXCEPTIONS},
      {"siteSettingsCapturedSurfaceControlBlockedExceptions",
       IDS_SETTINGS_SITE_SETTINGS_CAPTURED_SURFACE_CONTROL_BLOCKED_EXCEPTIONS},
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
      {"siteSettingsHandTrackingDescription",
       IDS_SETTINGS_SITE_SETTINGS_HAND_TRACKING_DESCRIPTION},
      {"siteSettingsHandTracking", IDS_SITE_SETTINGS_TYPE_HAND_TRACKING},
      {"siteSettingsHandTrackingMidSentence",
       IDS_SITE_SETTINGS_TYPE_HAND_TRACKING_MID_SENTENCE},
      {"siteSettingsHandTrackingAsk",
       IDS_SETTINGS_SITE_SETTINGS_HAND_TRACKING_ASK},
      {"siteSettingsHandTrackingBlock",
       IDS_SETTINGS_SITE_SETTINGS_HAND_TRACKING_BLOCK},
      {"siteSettingsHandTrackingAllowedExceptions",
       IDS_SETTINGS_SITE_SETTINGS_HAND_TRACKING_ALLOWED_EXCEPTIONS},
      {"siteSettingsHandTrackingBlockedExceptions",
       IDS_SETTINGS_SITE_SETTINGS_HAND_TRACKING_BLOCKED_EXCEPTIONS},
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
      {"siteSettingsJavascriptOptimizerDescription",
       IDS_SETTINGS_SITE_SETTINGS_JAVASCRIPT_OPTIMIZER_DESCRIPTION},
      {"siteSettingsJavascriptOptimizerAllowed",
       IDS_SETTINGS_SITE_SETTINGS_JAVASCRIPT_OPTIMIZER_ALLOWED},
      {"siteSettingsJavascriptOptimizerAllowedSubLabel",
       IDS_SETTINGS_SITE_SETTINGS_JAVASCRIPT_OPTIMIZER_ALLOWED_SUB_LABEL},
      {"siteSettingsJavascriptOptimizerBlocked",
       IDS_SETTINGS_SITE_SETTINGS_JAVASCRIPT_OPTIMIZER_BLOCKED},
      {"siteSettingsJavascriptOptimizerBlockedSubLabel",
       IDS_SETTINGS_SITE_SETTINGS_JAVASCRIPT_OPTIMIZER_BLOCKED_SUB_LABEL},
      {"siteSettingsJavascriptOptimizerAllowedExceptions",
       IDS_SETTINGS_SITE_SETTINGS_JAVASCRIPT_OPTIMIZER_ALLOWED_EXCEPTIONS},
      {"siteSettingsJavascriptOptimizerBlockedExceptions",
       IDS_SETTINGS_SITE_SETTINGS_JAVASCRIPT_OPTIMIZER_BLOCKED_EXCEPTIONS},
      {"siteSettingsKeyboardLock", IDS_SITE_SETTINGS_TYPE_KEYBOARD_LOCK},
      {"siteSettingsKeyboardLockAllowed",
       IDS_SETTINGS_SITE_SETTINGS_KEYBOARD_LOCK_ALLOWED},
      {"siteSettingsKeyboardLockAllowedExceptions",
       IDS_SETTINGS_SITE_SETTINGS_KEYBOARD_LOCK_ALLOWED_EXCEPTIONS},
      {"siteSettingsKeyboardLockBlocked",
       IDS_SETTINGS_SITE_SETTINGS_KEYBOARD_LOCK_BLOCKED},
      {"siteSettingsKeyboardLockBlockedExceptions",
       IDS_SETTINGS_SITE_SETTINGS_KEYBOARD_LOCK_BLOCKED_EXCEPTIONS},
      {"siteSettingsKeyboardLockDescription",
       IDS_SETTINGS_SITE_SETTINGS_KEYBOARD_LOCK_DESCRIPTION},
      {"siteSettingsKeyboardLockMidSentence",
       IDS_SITE_SETTINGS_TYPE_KEYBOARD_LOCK_MID_SENTENCE},
      {"siteSettingsLocationDescription",
       IDS_SETTINGS_SITE_SETTINGS_LOCATION_DESCRIPTION},
      {"siteSettingsLocationAllowed",
       IDS_SETTINGS_SITE_SETTINGS_LOCATION_ALLOWED},
      {"siteSettingsLocationAskQuiet",
       IDS_SETTINGS_SITE_SETTINGS_PERMISSION_QUIET},
      {"siteSettingsLocationAskCPSS",
       IDS_SETTINGS_SITE_SETTINGS_PERMISSION_CPSS},
      {"siteSettingsLocationAskLoud",
       IDS_SETTINGS_SITE_SETTINGS_PERMISSION_LOUD},
      {"siteSettingsLocationBlocked",
       IDS_SETTINGS_SITE_SETTINGS_LOCATION_BLOCKED},
      {"siteSettingsContentLocationBlockedByOs",
       IDS_SETTINGS_SITE_SETTINGS_LOCATION_BLOCKED_BY_OS},
      {"siteSettingsLocationBlockedSubLabel",
       IDS_SETTINGS_SITE_SETTINGS_LOCATION_BLOCKED_SUB_LABEL},
      {"siteSettingsLocationAllowedExceptions",
       IDS_SETTINGS_SITE_SETTINGS_LOCATION_ALLOWED_EXCEPTIONS},
      {"siteSettingsLocationBlockedExceptions",
       IDS_SETTINGS_SITE_SETTINGS_LOCATION_BLOCKED_EXCEPTIONS},
      {"siteSettingsMicDescription",
       IDS_SETTINGS_SITE_SETTINGS_MIC_DESCRIPTION},
      {"siteSettingsMicAllowed", IDS_SETTINGS_SITE_SETTINGS_MIC_ALLOWED},
      {"siteSettingsMicBlocked", IDS_SETTINGS_SITE_SETTINGS_MIC_BLOCKED},
      {"siteSettingsContentMicBlockedByOs",
       IDS_SETTINGS_SITE_SETTINGS_MIC_BLOCKED_BY_OS},
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
      {"siteSettingsNotificationsAllowed",
       IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS_ALLOWED},
      {"siteSettingsNotificationsPartial",
       IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS_PARTIAL},
      {"siteSettingsNotificationsPartialSubLabel",
       IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS_PARTIAL_SUB_LABEL},
      {"siteSettingsNotificationsAskState",
       IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS_ASK_STATE},
      {"siteSettingsNotificationsAskQuiet",
       IDS_SETTINGS_SITE_SETTINGS_PERMISSION_QUIET},
      {"siteSettingsNotificationsAskCPSS",
       IDS_SETTINGS_SITE_SETTINGS_PERMISSION_CPSS},
      {"siteSettingsNotificationsAskLoud",
       IDS_SETTINGS_SITE_SETTINGS_PERMISSION_LOUD},
      {"siteSettingsNotificationsBlocked",
       IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS_BLOCKED},
      {"siteSettingsNotificationsBlockedSubLabel",
       IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS_BLOCKED_SUB_LABEL},
      {"siteSettingsNotificationsAllowedExceptions",
       IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS_ALLOWED_EXCEPTIONS},
      {"siteSettingsNotificationsBlockedExceptions",
       IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS_BLOCKED_EXCEPTIONS},
      {"siteSettingsSystemSettingsLink",
       IDS_PAGE_INFO_SETTINGS_OF_A_SYSTEM_LINK},
      {"siteSettingsCameraBlockedByOs",
       IDS_SETTINGS_SITE_SITE_DETAILS_CAMERA_BLOCKED_BY_OS},
      {"siteSettingsMicrophoneBlockedByOs",
       IDS_SETTINGS_SITE_SITE_DETAILS_MICROPHONE_BLOCKED_BY_OS},
      {"siteSettingsLocationBlockedByOs",
       IDS_SETTINGS_SITE_SITE_DETAILS_LOCATION_BLOCKED_BY_OS},
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
      {"siteSettingsPointerLock", IDS_SITE_SETTINGS_TYPE_POINTER_LOCK},
      {"siteSettingsPointerLockAllowed",
       IDS_SETTINGS_SITE_SETTINGS_POINTER_LOCK_ALLOWED},
      {"siteSettingsPointerLockAllowedExceptions",
       IDS_SETTINGS_SITE_SETTINGS_POINTER_LOCK_ALLOWED_EXCEPTIONS},
      {"siteSettingsPointerLockBlocked",
       IDS_SETTINGS_SITE_SETTINGS_POINTER_LOCK_BLOCKED},
      {"siteSettingsPointerLockBlockedExceptions",
       IDS_SETTINGS_SITE_SETTINGS_POINTER_LOCK_BLOCKED_EXCEPTIONS},
      {"siteSettingsPointerLockDescription",
       IDS_SETTINGS_SITE_SETTINGS_POINTER_LOCK_DESCRIPTION},
      {"siteSettingsPointerLockMidSentence",
       IDS_SITE_SETTINGS_TYPE_POINTER_LOCK_MID_SENTENCE},
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
      {"siteSettingsUsbDescription",
       IDS_SETTINGS_SITE_SETTINGS_USB_DESCRIPTION},
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
      {"siteSettingsWebAppInstallation",
       IDS_SITE_SETTINGS_TYPE_WEB_APP_INSTALLATION},
      {"siteSettingsWebAppInstallationMidSentence",
       IDS_SITE_SETTINGS_TYPE_WEB_APP_INSTALLATION_MID_SENTENCE},
      {"siteSettingsWebAppInstallationDescription",
       IDS_SETTINGS_SITE_SETTINGS_WEB_APP_INSTALLATION_DESCRIPTION},
      {"siteSettingsWebAppInstallationAsk",
       IDS_SETTINGS_SITE_SETTINGS_WEB_APP_INSTALLATION_ASK},
      {"siteSettingsWebAppInstallationBlock",
       IDS_SETTINGS_SITE_SETTINGS_WEB_APP_INSTALLATION_BLOCK},
      {"siteSettingsWebAppInstallationAllowedExceptions",
       IDS_SETTINGS_SITE_SETTINGS_WEB_APP_INSTALLATION_ALLOWED_EXCEPTIONS},
      {"siteSettingsWebAppInstallationBlockedExceptions",
       IDS_SETTINGS_SITE_SETTINGS_WEB_APP_INSTALLATION_BLOCKED_EXCEPTIONS},
      {"siteSettingsWebPrinting", IDS_SITE_SETTINGS_TYPE_WEB_PRINTING},
      {"siteSettingsWebPrintingMidSentence",
       IDS_SITE_SETTINGS_TYPE_WEB_PRINTING_MID_SENTENCE},
      {"siteSettingsWebPrintingDescription",
       IDS_SETTINGS_SITE_SETTINGS_WEB_PRINTING_DESCRIPTION},
      {"siteSettingsWebPrintingAsk",
       IDS_SETTINGS_SITE_SETTINGS_WEB_PRINTING_ASK},
      {"siteSettingsWebPrintingBlock",
       IDS_SETTINGS_SITE_SETTINGS_WEB_PRINTING_BLOCK},
      {"siteSettingsWebPrintingAllowedExceptions",
       IDS_SETTINGS_SITE_SETTINGS_WEB_PRINTING_ALLOWED_EXCEPTIONS},
      {"siteSettingsWebPrintingBlockedExceptions",
       IDS_SETTINGS_SITE_SETTINGS_WEB_PRINTING_BLOCKED_EXCEPTIONS},
      {"siteSettingsWindowManagement",
       IDS_SITE_SETTINGS_TYPE_WINDOW_MANAGEMENT},
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
      {"antiAbuseWhenOnSectionOne",
       IDS_SETTINGS_ANTI_ABUSE_WHEN_ON_SECTION_ONE},
      {"antiAbuseWhenOnSectionTwo",
       IDS_SETTINGS_ANTI_ABUSE_WHEN_ON_SECTION_TWO},
      {"antiAbuseWhenOnSectionThree",
       IDS_SETTINGS_ANTI_ABUSE_WHEN_ON_SECTION_THREE},
      {"antiAbuseThingsToConsiderSectionOne",
       IDS_SETTINGS_ANTI_ABUSE_THINGS_TO_CONSIDER_SECTION_ONE},
      {"siteSettingsPerformance", IDS_SITE_SETTINGS_TYPE_PERFORMANCE},
      {"siteSettingsPerformanceSublabel",
       IDS_SITE_SETTINGS_TYPE_PERFORMANCE_SUBLABEL},
      {"siteSettingsOfferWritingHelp",
       IDS_SITE_SETTINGS_TYPE_OFFER_WRITING_HELP},
      {"offerWritingHelpToggleLabel",
       IDS_SETTINGS_OFFER_WRITING_HELP_TOGGLE_LABEL},
      {"offerWritingHelpToggleSublabel",
       IDS_SETTINGS_OFFER_WRITING_HELP_TOGGLE_SUB_LABEL},
      {"siteSettingsOfferWritingHelpEnabledSublabel",
       IDS_SETTINGS_OFFER_WRITING_HELP_ENABLED_SUB_LABEL},
      {"siteSettingsOfferWritingHelpDisabledSublabel",
       IDS_SETTINGS_OFFER_WRITING_HELP_DISABLED_SUB_LABEL},
      {"offerWritingHelpDisabledSitesLabel",
       IDS_SETTINGS_OFFER_WRITING_HELP_DISABLED_SITES_LABEL},
      {"offerWritingHelpNoDisabledSites",
       IDS_SETTINGS_OFFER_WRITING_HELP_NO_DISABLED_SITES},
      {"offerWritingHelpRemoveDisabledSiteAriaLabel",
       IDS_SETTINGS_OFFER_WRITING_HELP_REMOVE_SITE_ARIA_LABEL},
      {"siteSettingsSmartCardReaders", IDS_SITE_SETTINGS_SMART_CARD_READERS},
      {"siteSettingsSmartCardReadersDescription",
       IDS_SITE_SETTINGS_SMART_CARD_READERS_DESCRIPTION},
      {"siteSettingsSmartCardReadersDefaultDescription",
       IDS_SITE_SETTINGS_SMART_CARDS_DEFAULT_DESCRIPTION},
      {"siteSettingsSmartCardReadersAllowed",
       IDS_SITE_SETTINGS_SMART_CARDS_ALLOWED},
      {"siteSettingsSmartCardReadersBlocked",
       IDS_SITE_SETTINGS_SMART_CARDS_BLOCKED},
      {"siteSettingsNoSmartCardReadersFound",
       IDS_SITE_SETTINGS_NO_SMART_CARD_READERS_FOUND},
      {"siteSettingsResetSmartCardConfirmation",
       IDS_SITE_SETTINGS_RESET_SMART_CARD_CONFIRMATION}};
  html_source->AddLocalizedStrings(kLocalizedStrings);

  // Tracking protection learn more links.
  html_source->AddString(
      "trackingProtectionBulletTwoDescription",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_TRACKING_PROTECTION_BULLET_TWO_DESCRIPTION,
          chrome::kUserBypassHelpCenterURL,
          l10n_util::GetStringUTF16(
              IDS_SETTINGS_TRACKING_PROTECTION_BULLET_TWO_LEARN_MORE_ARIA_LABEL),
          l10n_util::GetStringUTF16(IDS_SETTINGS_OPENS_IN_NEW_TAB)));
  html_source->AddString("trackingProtectionThirdPartyCookiesLearnMoreUrl",
                         chrome::kManage3pcHelpCenterURL);
  html_source->AddString(
      "trackingProtectionIpProtectionToggleSubLabel",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_TRACKING_PROTECTION_IP_PROTECTION_TOGGLE_SUB_LABEL,
          l10n_util::GetStringUTF16(
              IDS_SETTINGS_TRACKING_PROTECTION_IP_PROTECTION_TOGGLE_LEARN_MORE_ARIA_LABEL),
          l10n_util::GetStringUTF16(IDS_SETTINGS_OPENS_IN_NEW_TAB)));
  html_source->AddString("ipProtectionLearnMoreUrl",
                         chrome::kIpProtectionHelpCenterURL);

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

  html_source->AddBoolean("enableHandTrackingContentSetting",
#if BUILDFLAG(ENABLE_VR)
                          device::features::IsHandTrackingEnabled());
#else
                          false);
#endif

  html_source->AddBoolean(
      "enableWebPrintingContentSetting",
      base::FeatureList::IsEnabled(blink::features::kWebPrinting));

  html_source->AddBoolean("enableFederatedIdentityApiContentSetting",
                          base::FeatureList::IsEnabled(features::kFedCm));

  base::CommandLine& cmd = *base::CommandLine::ForCurrentProcess();
  html_source->AddBoolean(
      "enableExperimentalWebPlatformFeatures",
      cmd.HasSwitch(::switches::kEnableExperimentalWebPlatformFeatures));

  html_source->AddBoolean("enableWebBluetoothNewPermissionsBackend",
                          base::FeatureList::IsEnabled(
                              features::kWebBluetoothNewPermissionsBackend));

  html_source->AddBoolean(
      "showPersistentPermissions",
      base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions));

  // The exception placeholder should not be translated. See
  // crbug.com/1095878.
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
      {"siteDataPageAddSiteToAllowListLabel",
       IDS_SETTINGS_ADD_SITE_TO_ALLOW_LIST_LABEL},
      {"siteDataPageAddSiteToBlockListLabel",
       IDS_SETTINGS_ADD_SITE_TO_BLOCK_LIST_LABEL},
      {"siteDataPageAddSiteContextMenuLabel",
       IDS_SETTINGS_ADD_SITE_CONTEXT_MENU_LABEL},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
  html_source->AddLocalizedString(
      "siteDataPageClearOnExitRadioSubLabel",
      switches::IsExplicitBrowserSigninUIOnDesktopEnabled()
          ? IDS_SETTINGS_SITE_DATA_PAGE_CLEAR_ON_EXIT_WITH_EXCEPTION_RADIO_SUBLABEL
          : IDS_SETTINGS_SITE_DATA_PAGE_CLEAR_ON_EXIT_RADIO_SUBLABEL);
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
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
      {"featureNotificationsLabel",
       IDS_SETTINGS_SYSTEM_FEATURE_NOTIFICATIONS_LABEL},
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
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

extern void AddPrivacySandboxStrings(content::WebUIDataSource* html_source,
                                     Profile* profile);

void AddLocalizedStrings(content::WebUIDataSource* html_source,
                         Profile* profile,
                         content::WebContents* web_contents) {
  AddA11yStrings(html_source);
  AddAboutStrings(html_source, profile);
  AddAutofillStrings(html_source, profile, web_contents);
  AddAppearanceStrings(html_source, profile);

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
  AddSearchStrings(html_source, profile);
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
