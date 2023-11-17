// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/config/coverage/buildflags.h"
#include "chrome/browser/preloading/preloading_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/content_settings/core/common/features.h"
#include "components/performance_manager/public/features.h"
#include "components/permissions/features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"

class SettingsBrowserTest : public WebUIMochaBrowserTest {
 protected:
  SettingsBrowserTest() { set_test_loader_host(chrome::kChromeUISettingsHost); }
};

using SettingsTest = SettingsBrowserTest;

// Note: Keep tests below in alphabetical ordering.

// Copied from Polymer 2 test:
// Times out on debug builders because the Settings page can take several
// seconds to load in a Release build and several times that in a Debug build.
// See https://crbug.com/558434.
#if !defined(NDEBUG)
#define MAYBE_AdvancedPage DISABLED_AdvancedPage
#else
#define MAYBE_AdvancedPage AdvancedPage
#endif
IN_PROC_BROWSER_TEST_F(SettingsTest, MAYBE_AdvancedPage) {
  RunTest("settings/advanced_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, AntiAbusePage) {
  RunTest("settings/anti_abuse_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, AppearanceFontsPage) {
  RunTest("settings/appearance_fonts_page_test.js", "mocha.run()");
}

#if !BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/1350019) Test is flaky on ChromeOS
IN_PROC_BROWSER_TEST_F(SettingsTest, AppearancePage) {
  RunTest("settings/appearance_page_test.js", "mocha.run()");
}
#endif

IN_PROC_BROWSER_TEST_F(SettingsTest, AutofillAddressValidation) {
  RunTest("settings/autofill_section_address_validation_test.js",
          "mocha.run()");
}

// TODO(crbug.com/1420597): Clean up this test after Password Manager redesign
// is launched.
IN_PROC_BROWSER_TEST_F(SettingsTest, AutofillPage) {
  RunTest("settings/autofill_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, AutofillSection) {
  RunTest("settings/autofill_section_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, BatteryPage) {
  RunTest("settings/battery_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, CategorySettingExceptions) {
  RunTest("settings/category_setting_exceptions_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, Checkbox) {
  RunTest("settings/checkbox_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, CheckboxListEntry) {
  RunTest("settings/checkbox_list_entry_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ChooserExceptionList) {
  RunTest("settings/chooser_exception_list_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ChooserExceptionListEntry) {
  RunTest("settings/chooser_exception_list_entry_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, CollapseRadioButton) {
  RunTest("settings/collapse_radio_button_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ControlledButton) {
  RunTest("settings/controlled_button_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ControlledRadioButton) {
  RunTest("settings/controlled_radio_button_test.js", "mocha.run()");
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SettingsTest, DefaultBrowser) {
  RunTest("settings/default_browser_test.js", "mocha.run()");
}
#endif

IN_PROC_BROWSER_TEST_F(SettingsTest, DoNotTrackToggle) {
  RunTest("settings/do_not_track_toggle_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, DownloadsPage) {
  RunTest("settings/downloads_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, DropdownMenu) {
  RunTest("settings/dropdown_menu_test.js", "mocha.run()");
}

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SettingsTest, EditDictionaryPage) {
  RunTest("settings/edit_dictionary_page_test.js", "mocha.run()");
}
#endif

IN_PROC_BROWSER_TEST_F(SettingsTest, ExtensionControlledIndicator) {
  RunTest("settings/extension_controlled_indicator_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, FileSystemSettingsSiteDetails) {
  RunTest("settings/file_system_site_details_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, FileSystemSettingsList) {
  RunTest("settings/file_system_site_list_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, FileSystemSettingsListEntries) {
  RunTest("settings/file_system_site_entry_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, FileSystemSettingsListEntryItems) {
  RunTest("settings/file_system_site_entry_item_test.js", "mocha.run()");
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_F(SettingsTest, GetMostChromePage) {
  RunTest("settings/get_most_chrome_page_test.js", "mocha.run()");
}
#endif

IN_PROC_BROWSER_TEST_F(SettingsTest, HelpPage) {
  RunTest("settings/help_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, IdleLoad) {
  RunTest("settings/idle_load_test.js", "mocha.run()");
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SettingsTest, ImportDataDialog) {
  RunTest("settings/import_data_dialog_test.js", "mocha.run()");
}
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SettingsTest, Languages) {
  RunTest("settings/languages_test.js", "mocha.run()");
}
#endif

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SettingsTest, LiveCaptionSection) {
  RunTest("settings/live_caption_section_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, LiveTranslateSection) {
  RunTest("settings/live_translate_section_test.js", "mocha.run()");
}
#endif

// Copied from Polymer 2 version of tests:
// Times out on Windows Tests (dbg). See https://crbug.com/651296.
// Times out / crashes on chromium.linux/Linux Tests (dbg) crbug.com/667882
// Flaky everywhere crbug.com/1197768
IN_PROC_BROWSER_TEST_F(SettingsTest, DISABLED_MainPage) {
  RunTest("settings/settings_main_test.js", "mocha.run()");
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SettingsTest, MetricsReporting) {
  RunTest("settings/metrics_reporting_test.js", "mocha.run()");
}
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(SettingsTest, PasskeysSubpage) {
  RunTest("settings/passkeys_subpage_test.js", "mocha.run()");
}
#endif

IN_PROC_BROWSER_TEST_F(SettingsTest, PaymentsSection) {
  RunTest("settings/payments_section_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, PaymentsSectionCardDialogs) {
  RunTest("settings/payments_section_card_dialogs_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, PaymentsSectionCardRows) {
  RunTest("settings/payments_section_card_rows_test.js",
          "runMochaSuite('PaymentsSectionCardRows')");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, PaymentsSectionEditCreditCardLink) {
  RunTest("settings/payments_section_card_rows_test.js",
          "runMochaSuite('PaymentsSectionEditCreditCardLink')");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, PaymentsSectionIban) {
  RunTest("settings/payments_section_iban_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, PeoplePage) {
  RunTest("settings/people_page_test.js", "mocha.run()");
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SettingsTest, PeoplePageChromeOS) {
  RunTest("settings/people_page_test_cros.js", "mocha.run()");
}
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SettingsTest, PeoplePageManageProfile) {
  RunTest("settings/people_page_manage_profile_test.js", "mocha.run()");
}
#endif

IN_PROC_BROWSER_TEST_F(SettingsTest, PeoplePageSyncControls) {
  RunTest("settings/people_page_sync_controls_test.js", "mocha.run()");
}

// Timeout on Linux dbg bots: https://crbug.com/1394737
#if !(BUILDFLAG(IS_LINUX) && !defined(NDEBUG))
IN_PROC_BROWSER_TEST_F(SettingsTest, PeoplePageSyncPage) {
  RunTest("settings/people_page_sync_page_test.js", "mocha.run()");
}
#endif

IN_PROC_BROWSER_TEST_F(SettingsTest, PerformanceMenu) {
  RunTest("settings/settings_performance_menu_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, PreloadingPage) {
  RunTest("settings/preloading_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, PrivacySandbox) {
  RunTest("settings/privacy_sandbox_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ProtocolHandlers) {
  RunTest("settings/protocol_handlers_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, RecentSitePermissions) {
  RunTest("settings/recent_site_permissions_test.js", "mocha.run()");
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SettingsTest, RelaunchConfirmationDialog) {
  RunTest("settings/relaunch_confirmation_dialog_test.js", "mocha.run()");
}
#endif

// TODO(crbug.com/1127733): Flaky on all OSes. Enable the test.
IN_PROC_BROWSER_TEST_F(SettingsTest, DISABLED_ResetPage) {
  RunTest("settings/reset_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ResetProfileBanner) {
  RunTest("settings/reset_profile_banner_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SafetyCheckPage) {
  RunTest("settings/safety_check_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, Search) {
  RunTest("settings/search_settings_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SearchEngines) {
  RunTest("settings/search_engines_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SearchPage) {
  RunTest("settings/search_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, Section) {
  RunTest("settings/settings_section_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SecureDnsInput) {
  RunTest("settings/secure_dns_test.js",
          "runMochaSuite('SettingsSecureDnsInput')");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SecureDns) {
  RunTest("settings/secure_dns_test.js", "runMochaSuite('SettingsSecureDns')");
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SettingsTest, SecureDnsDialog) {
  RunTest("settings/secure_dns_test.js",
          "runMochaSuite('OsSettingsRevampSecureDnsDialog')");
}
#endif

IN_PROC_BROWSER_TEST_F(SettingsTest, SecurityKeysBioEnrollment) {
  RunTest("settings/security_keys_bio_enrollment_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SecurityKeysCredentialManagement) {
  RunTest("settings/security_keys_credential_management_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SecurityKeysPhonesSubpage) {
  RunTest("settings/security_keys_phones_subpage_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SecurityKeysResetDialog) {
  RunTest("settings/security_keys_reset_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SecurityKeysSetPinDialog) {
  RunTest("settings/security_keys_set_pin_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SettingsCategoryDefaultRadioGroup) {
  RunTest("settings/settings_category_default_radio_group_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SettingsMenu) {
  RunTest("settings/settings_menu_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SimpleConfirmationDialog) {
  RunTest("settings/simple_confirmation_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SiteDataTest) {
  RunTest("settings/site_data_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SiteDetailsPermission) {
  RunTest("settings/site_details_permission_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SiteDetailsPermissionDeviceEntry) {
  RunTest("settings/site_details_permission_device_entry_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SiteEntry) {
  RunTest("settings/site_entry_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SiteFavicon) {
  RunTest("settings/site_favicon_test.js", "mocha.run()");
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Copied from Polymer 2 test. TODO(crbug.com/929455): flaky, fix.
IN_PROC_BROWSER_TEST_F(SettingsTest, DISABLED_SiteListChromeOS) {
  RunTest("settings/site_list_tests_cros.js", "mocha.run()");
}
#endif

IN_PROC_BROWSER_TEST_F(SettingsTest, SiteListEntry) {
  RunTest("settings/site_list_entry_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, Slider) {
  RunTest("settings/settings_slider_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SpeedPage) {
  RunTest("settings/speed_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, StartupUrlsPage) {
  RunTest("settings/startup_urls_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, StorageAccessStaticSiteListEntry) {
  RunTest("settings/storage_access_static_site_list_entry_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, StorageAccessSiteListEntry) {
  RunTest("settings/storage_access_site_list_entry_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, StorageAccessSiteList) {
  RunTest("settings/storage_access_site_list_test.js", "mocha.run()");
}
// Flaky on all OSes. TODO(crbug.com/1302405): Enable the test.
IN_PROC_BROWSER_TEST_F(SettingsTest, DISABLED_Subpage) {
  RunTest("settings/settings_subpage_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SyncAccountControl) {
  RunTest("settings/sync_account_control_test.js", "mocha.run()");
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SettingsTest, SystemPage) {
  RunTest("settings/system_page_test.js", "mocha.run()");
}
#endif

// Flaky on all OSes. TODO(charlesmeng): Enable the test.
IN_PROC_BROWSER_TEST_F(SettingsTest, DISABLED_TabDiscardExceptionDialog) {
  RunTest("settings/tab_discard_exception_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ToggleButton) {
  RunTest("settings/settings_toggle_button_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ZoomLevels) {
  RunTest("settings/zoom_levels_test.js", "mocha.run()");
}

using SettingsAboutPageTest = SettingsBrowserTest;

IN_PROC_BROWSER_TEST_F(SettingsAboutPageTest, AllBuilds) {
  RunTest("settings/about_page_test.js", "runMochaSuite('AllBuilds')");
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_F(SettingsAboutPageTest, OfficialBuild) {
  RunTest("settings/about_page_test.js", "runMochaSuite('OfficialBuild')");
}
#endif

using SettingsAllSitesTest = SettingsBrowserTest;

IN_PROC_BROWSER_TEST_F(SettingsAllSitesTest, EnableFirstPartySets) {
  RunTest("settings/all_sites_test.js",
          "runMochaSuite('EnableFirstPartySets')");
}

IN_PROC_BROWSER_TEST_F(SettingsAllSitesTest, DisableFirstPartySets) {
  RunTest("settings/all_sites_test.js",
          "runMochaSuite('DisableFirstPartySets')");
}

class SettingsBasicPageTest : public SettingsBrowserTest {
 protected:
  SettingsBasicPageTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kSafetyHub, {}},
            {features::kPerformanceSettingsPreloadingSubpage,
             {{"use_v2_preloading_subpage", "true"}}},
        },
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/1298753): Flaky on all platforms.
IN_PROC_BROWSER_TEST_F(SettingsBasicPageTest, DISABLED_BasicPage) {
  RunTest("settings/basic_page_test.js", "runMochaSuite('BasicPage')");
}

#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_PrivacyGuidePromo DISABLED_PrivacyGuidePromo
#else
#define MAYBE_PrivacyGuidePromo PrivacyGuidePromo
#endif
IN_PROC_BROWSER_TEST_F(SettingsBasicPageTest, MAYBE_PrivacyGuidePromo) {
  RunTest("settings/basic_page_test.js", "runMochaSuite('PrivacyGuidePromo')");
}

IN_PROC_BROWSER_TEST_F(SettingsBasicPageTest, Performance) {
  RunTest("settings/basic_page_test.js", "runMochaSuite('Performance')");
}

IN_PROC_BROWSER_TEST_F(SettingsBasicPageTest, SafetyHubDisabled) {
  RunTest("settings/basic_page_test.js", "runMochaSuite('SafetyHubDisabled')");
}

using SettingsClearBrowsingDataTest = SettingsBrowserTest;

// TODO(crbug.com/1107652): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ClearBrowsingDataAllPlatforms \
  DISABLED_ClearBrowsingDataAllPlatforms
#else
#define MAYBE_ClearBrowsingDataAllPlatforms ClearBrowsingDataAllPlatforms
#endif
IN_PROC_BROWSER_TEST_F(SettingsClearBrowsingDataTest,
                       MAYBE_ClearBrowsingDataAllPlatforms) {
  RunTest("settings/clear_browsing_data_test.js",
          "runMochaSuite('ClearBrowsingDataAllPlatforms')");
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SettingsClearBrowsingDataTest,
                       ClearBrowsingDataDesktop) {
  RunTest("settings/clear_browsing_data_test.js",
          "runMochaSuite('ClearBrowsingDataDesktop')");
}
#endif

IN_PROC_BROWSER_TEST_F(SettingsClearBrowsingDataTest,
                       ClearBrowsingDataForSupervisedUsers) {
  RunTest("settings/clear_browsing_data_test.js",
          "runMochaSuite('ClearBrowsingDataForSupervisedUsers')");
}

class SettingsCookiesPageTest : public SettingsBrowserTest {
 protected:
  SettingsCookiesPageTest() {
    scoped_feature_list_.InitWithFeatures(
        {
            privacy_sandbox::kPrivacySandboxSettings4,
            privacy_sandbox::kPrivacySandboxFirstPartySetsUI,
        },
        {features::kPerformanceSettingsPreloadingSubpage});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if ((BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && !defined(NDEBUG)) || \
    BUILDFLAG(USE_JAVASCRIPT_COVERAGE) || BUILDFLAG(IS_MAC)
#define MAYBE_CookiesPageTest DISABLED_CookiesPageTest
#else
#define MAYBE_CookiesPageTest CookiesPageTest
#endif
// TODO(crbug.com/1409653): fix flakiness on Linux and ChromeOS debug and
// Javascript code coverage builds and re-enable.
IN_PROC_BROWSER_TEST_F(SettingsCookiesPageTest, MAYBE_CookiesPageTest) {
  RunTest("settings/cookies_page_test.js", "runMochaSuite('CookiesPageTest')");
}

IN_PROC_BROWSER_TEST_F(SettingsCookiesPageTest, FirstPartySetsUIDisabled) {
  RunTest("settings/cookies_page_test.js",
          "runMochaSuite('FirstPartySetsUIDisabled')");
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(SettingsCookiesPageTest, LacrosSecondaryProfile) {
  RunTest("settings/cookies_page_test.js",
          "runMochaSuite('LacrosSecondaryProfile')");
}
#endif

#if (BUILDFLAG(IS_LINUX) && !defined(NDEBUG)) || \
    BUILDFLAG(USE_JAVASCRIPT_COVERAGE)
#define MAYBE_PrivacySandboxSettings4Disabled2 \
  DISABLED_PrivacySandboxSettings4Disabled
#else
#define MAYBE_PrivacySandboxSettings4Disabled2 PrivacySandboxSettings4Disabled
#endif
// TODO(crbug.com/1409653): fix flakiness on Linux debug and Javascript code
// coverage builds and re-enable.
// The "MAYBE..." portion of the test has a 2 at the end because there is
// already a macro with the same name defined in this file.
IN_PROC_BROWSER_TEST_F(SettingsCookiesPageTest,
                       MAYBE_PrivacySandboxSettings4Disabled2) {
  RunTest("settings/cookies_page_test.js",
          "runMochaSuite('PrivacySandboxSettings4Disabled')");
}

IN_PROC_BROWSER_TEST_F(SettingsCookiesPageTest,
                       PreloadingSubpageMovedToPerformanceSettings) {
  RunTest("settings/cookies_page_test.js",
          "runMochaSuite('PreloadingSubpageMovedToPerformanceSettings')");
}

IN_PROC_BROWSER_TEST_F(SettingsCookiesPageTest, TrackingProtectionSettings) {
  RunTest("settings/cookies_page_test.js",
          "runMochaSuite('TrackingProtectionSettings')");
}

IN_PROC_BROWSER_TEST_F(SettingsCookiesPageTest,
                       TrackingProtectionSettingsRollbackNotice) {
  RunTest("settings/cookies_page_test.js",
          "runMochaSuite('TrackingProtectionSettingsRollbackNotice')");
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
using SettingsLanguagePageTest = SettingsBrowserTest;

IN_PROC_BROWSER_TEST_F(SettingsLanguagePageTest, AddLanguagesDialog) {
  RunTest("settings/languages_page_test.js",
          "runMochaSuite('LanguagesPage AddLanguagesDialog')");
}

IN_PROC_BROWSER_TEST_F(SettingsLanguagePageTest, LanguageMenu) {
  RunTest("settings/languages_page_test.js",
          "runMochaSuite('LanguagesPage LanguageMenu')");
}

IN_PROC_BROWSER_TEST_F(SettingsLanguagePageTest, MetricsBrowser) {
  RunTest("settings/languages_page_metrics_test_browser.js", "mocha.run()");
}
#endif

using SettingsPerformancePageTest = SettingsBrowserTest;

IN_PROC_BROWSER_TEST_F(SettingsPerformancePageTest, Controls) {
  RunTest("settings/performance_page_test.js",
          "runMochaSuite('PerformancePage')");
}

IN_PROC_BROWSER_TEST_F(SettingsPerformancePageTest, ExceptionList) {
  RunTest("settings/performance_page_test.js",
          "runMochaSuite('TabDiscardExceptionList')");
}

class SettingsPerformancePageMultistateTest : public SettingsBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      performance_manager::features::kHighEfficiencyMultistateMode};
};

IN_PROC_BROWSER_TEST_F(SettingsPerformancePageMultistateTest, Controls) {
  RunTest("settings/performance_page_test.js",
          "runMochaSuite('PerformancePageMultistate')");
}

IN_PROC_BROWSER_TEST_F(SettingsPerformancePageMultistateTest, ExceptionList) {
  RunTest("settings/performance_page_test.js",
          "runMochaSuite('TabDiscardExceptionList')");
}

class SettingsPerformancePageDiscardExceptionImprovementsTest
    : public SettingsBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      performance_manager::features::kDiscardExceptionsImprovements};
};

IN_PROC_BROWSER_TEST_F(SettingsPerformancePageDiscardExceptionImprovementsTest,
                       Controls) {
  RunTest("settings/performance_page_test.js",
          "runMochaSuite('PerformancePage')");
}

IN_PROC_BROWSER_TEST_F(SettingsPerformancePageDiscardExceptionImprovementsTest,
                       ExceptionList) {
  RunTest("settings/performance_page_test.js",
          "runMochaSuite('TabDiscardExceptionList')");
}

class SettingsPersonalizationOptionsTest : public SettingsBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kPageContentOptIn};
};

IN_PROC_BROWSER_TEST_F(SettingsPersonalizationOptionsTest, AllBuilds) {
  RunTest("settings/personalization_options_test.js",
          "runMochaSuite('AllBuilds')");
}

IN_PROC_BROWSER_TEST_F(SettingsPersonalizationOptionsTest,
                       PageContentSettingOff) {
  RunTest("settings/personalization_options_test.js",
          "runMochaSuite('PageContentSettingOff')");
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_F(SettingsPersonalizationOptionsTest, OfficialBuild) {
  RunTest("settings/personalization_options_test.js",
          "runMochaSuite('OfficialBuild')");
}
#endif

class SettingsPrivacyGuideTest : public SettingsBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{features::kPrivacyGuide3};
};

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, PrivacyGuidePage) {
  RunTest("settings/privacy_guide_page_test.js",
          "runMochaSuite('PrivacyGuidePage')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, SettingsFlowLength) {
  RunTest("settings/privacy_guide_page_test.js",
          "runMochaSuite('SettingsFlowLength')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, PrivacyGuidePagePG3Off) {
  RunTest("settings/privacy_guide_page_test.js",
          "runMochaSuite('PrivacyGuidePagePG3Off')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, 3PCDDisablesCookiesCard) {
  RunTest("settings/privacy_guide_page_test.js",
          "runMochaSuite('3PCDDisablesCookiesCard')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, SettingsFlowLengthPG3Off) {
  RunTest("settings/privacy_guide_page_test.js",
          "runMochaSuite('SettingsFlowLengthPG3Off')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, MsbbCardNavigations) {
  RunTest("settings/privacy_guide_page_test.js",
          "runMochaSuite('MsbbCardNavigations')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, MsbbCardNavigationsPG3Off) {
  RunTest("settings/privacy_guide_page_test.js",
          "runMochaSuite('MsbbCardNavigationsPG3Off')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, HistorySyncCardNavigations) {
  RunTest("settings/privacy_guide_page_test.js",
          "runMochaSuite('HistorySyncCardNavigations')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest,
                       HistorySyncCardNavigationsPG3Off) {
  RunTest("settings/privacy_guide_page_test.js",
          "runMochaSuite('HistorySyncCardNavigationsPG3Off')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, SafeBrowsingCardNavigations) {
  RunTest("settings/privacy_guide_page_test.js",
          "runMochaSuite('SafeBrowsingCardNavigations')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest,
                       SafeBrowsingCardNavigationsPG3Off) {
  RunTest("settings/privacy_guide_page_test.js",
          "runMochaSuite('SafeBrowsingCardNavigationsPG3Off')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, CookiesCardNavigations) {
  RunTest("settings/privacy_guide_page_test.js",
          "runMochaSuite('CookiesCardNavigations')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, CookiesCardNavigationsPG3Off) {
  RunTest("settings/privacy_guide_page_test.js",
          "runMochaSuite('CookiesCardNavigationsPG3Off')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest,
                       SearchSuggestionsCardNavigations) {
  RunTest("settings/privacy_guide_page_test.js",
          "runMochaSuite('SearchSuggestionsCardNavigations')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, PreloadCardNavigations) {
  RunTest("settings/privacy_guide_page_test.js",
          "runMochaSuite('PreloadCardNavigations')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, PrivacyGuideDialog) {
  RunTest("settings/privacy_guide_page_test.js",
          "runMochaSuite('PrivacyGuideDialog')");
}

// TODO(https://crbug.com/1426530): Re-enable when no longer flaky.
#if !BUILDFLAG(IS_LINUX) || defined(NDEBUG)
IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, Integration) {
  RunTest("settings/privacy_guide_integration_test.js", "mocha.run()");
}
#endif

using SettingsPrivacyGuideFragmentsTest = SettingsBrowserTest;

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideFragmentsTest, WelcomeFragment) {
  RunTest("settings/privacy_guide_fragments_test.js",
          "runMochaSuite('WelcomeFragment')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideFragmentsTest, MsbbFragment) {
  RunTest("settings/privacy_guide_fragments_test.js",
          "runMochaSuite('MsbbFragment')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideFragmentsTest, HistorySyncFragment) {
  RunTest("settings/privacy_guide_fragments_test.js",
          "runMochaSuite('HistorySyncFragment')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideFragmentsTest,
                       SafeBrowsingFragment) {
  RunTest("settings/privacy_guide_fragments_test.js",
          "runMochaSuite('SafeBrowsingFragment')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideFragmentsTest, CookiesFragment) {
  RunTest("settings/privacy_guide_fragments_test.js",
          "runMochaSuite('CookiesFragment')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideFragmentsTest, PreloadFragment) {
  RunTest("settings/privacy_guide_fragments_test.js",
          "runMochaSuite('PreloadFragment')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideFragmentsTest, CompletionFragment) {
  RunTest("settings/privacy_guide_fragments_test.js",
          "runMochaSuite('CompletionFragment')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideFragmentsTest,
                       CompletionFragmentPrivacySandboxRestricted) {
  RunTest("settings/privacy_guide_fragments_test.js",
          "runMochaSuite('CompletionFragmentPrivacySandboxRestricted')");
}

class SettingsPrivacyPagePrivacySandboxRestrictedTest
    : public SettingsBrowserTest {
 protected:
  SettingsPrivacyPagePrivacySandboxRestrictedTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        privacy_sandbox::kPrivacySandboxSettings4,
        {
            {"force-restricted-user", "true"},
        });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPagePrivacySandboxRestrictedTest,
                       Restricted) {
  RunTest("settings/privacy_page_test.js",
          "runMochaSuite('PrivacySandbox4EnabledButRestricted')");
}

class SettingsPrivacyPagePrivacySandboxRestrictedWithNoticeTest
    : public SettingsBrowserTest {
 protected:
  SettingsPrivacyPagePrivacySandboxRestrictedWithNoticeTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        privacy_sandbox::kPrivacySandboxSettings4,
        {
            {"force-restricted-user", "true"},
            {"restricted-notice", "true"},
        });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    SettingsPrivacyPagePrivacySandboxRestrictedWithNoticeTest,
    RestrictedWithNotice) {
  RunTest("settings/privacy_page_test.js",
          "runMochaSuite('PrivacySandbox4EnabledButRestrictedWithNotice')");
}

class SettingsPrivacyPageTest : public SettingsBrowserTest {
 protected:
  SettingsPrivacyPageTest() {
    scoped_feature_list1_.InitWithFeatures(
        {privacy_sandbox::kPrivacySandboxSettings4,
         permissions::features::kPermissionStorageAccessAPI,
         features::kSafetyCheckNotificationPermissions, features::kSafetyHub},
        {});
    scoped_feature_list2_.InitAndEnableFeatureWithParameters(
        features::kFedCm, {
                              {"DesktopSettings", "true"},
                          });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list1_;
  base::test::ScopedFeatureList scoped_feature_list2_;
};

// TODO(crbug.com/1491942): This fails with the field trial testing config.
class SettingsPrivacyPageTestNoTestingConfig : public SettingsPrivacyPageTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SettingsPrivacyPageTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("disable-field-trial-config");
  }
};

// TODO(crbug.com/1351019): Flaky on Linux Tests(dbg).
#if BUILDFLAG(IS_LINUX)
#define MAYBE_PrivacyPage DISABLED_PrivacyPage
#else
#define MAYBE_PrivacyPage PrivacyPage
#endif
IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTestNoTestingConfig,
                       MAYBE_PrivacyPage) {
  RunTest("settings/privacy_page_test.js", "runMochaSuite('PrivacyPage')");
}

// TODO(crbug.com/1378703): Remove once PrivacySandboxSettings4 has been rolled
// out.
IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest, PrivacySandbox4Disabled) {
  RunTest("settings/privacy_page_test.js",
          "runMochaSuite('PrivacySandbox4Disabled')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest, PrivacySandbox4Enabled) {
  RunTest("settings/privacy_page_test.js",
          "runMochaSuite('PrivacySandbox4Enabled')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest, PrivacyGuideRow) {
  RunTest("settings/privacy_page_test.js", "runMochaSuite('PrivacyGuideRow')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest, NotificationPermissionReview) {
  RunTest("settings/privacy_page_test.js",
          "runMochaSuite('NotificationPermissionReview')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest,
                       NotificationPermissionReviewSafetyHubDisabled) {
  RunTest("settings/privacy_page_test.js",
          "runMochaSuite('NotificationPermissionReviewSafetyHubDisabled')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest,
                       NotificationPermissionReviewDisabled) {
  RunTest("settings/privacy_page_test.js",
          "runMochaSuite('NotificationPermissionReviewDisabled')");
}

// TODO(crbug.com/1043665): flaky crash on Linux Tests (dbg).
IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest, DISABLED_PrivacyPageSound) {
  RunTest("settings/privacy_page_test.js", "runMochaSuite('PrivacyPageSound')");
}

// TODO(crbug.com/1113912): flaky failure on multiple platforms
IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest,
                       DISABLED_HappinessTrackingSurveys) {
  RunTest("settings/privacy_page_test.js",
          "runMochaSuite('HappinessTrackingSurveys')");
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// TODO(crbug.com/1043665): disabling due to failures on several builders.
IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest, DISABLED_CertificateManager) {
  RunTest("settings/privacy_page_test.js",
          "runMochaSuite('NativeCertificateManager')");
}
#endif

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest,
                       EnableWebBluetoothNewPermissionsBackend) {
  RunTest("settings/privacy_page_test.js",
          "runMochaSuite('EnableWebBluetoothNewPermissionsBackend')");
}

class SettingsPrivacySandboxPageTest : public SettingsBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      privacy_sandbox::kPrivacySandboxSettings4};
};

IN_PROC_BROWSER_TEST_F(SettingsPrivacySandboxPageTest, PrivacySandboxPage) {
  RunTest("settings/privacy_sandbox_page_test.js",
          "runMochaSuite('PrivacySandboxPage')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacySandboxPageTest, RestrictedEnabled) {
  RunTest("settings/privacy_sandbox_page_test.js",
          "runMochaSuite('RestrictedEnabled')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacySandboxPageTest, TopicsSubpage) {
  RunTest("settings/privacy_sandbox_page_test.js",
          "runMochaSuite('TopicsSubpage')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacySandboxPageTest, TopicsSubpageEmpty) {
  RunTest("settings/privacy_sandbox_page_test.js",
          "runMochaSuite('TopicsSubpageEmpty')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacySandboxPageTest, FledgeSubpage) {
  RunTest("settings/privacy_sandbox_page_test.js",
          "runMochaSuite('FledgeSubpage')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacySandboxPageTest, FledgeSubpageEmpty) {
  RunTest("settings/privacy_sandbox_page_test.js",
          "runMochaSuite('FledgeSubpageEmpty')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacySandboxPageTest,
                       FledgeSubpageSeeAllSites) {
  RunTest("settings/privacy_sandbox_page_test.js",
          "runMochaSuite('FledgeSubpageSeeAllSites')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacySandboxPageTest, AdMeasurementSubpage) {
  RunTest("settings/privacy_sandbox_page_test.js",
          "runMochaSuite('AdMeasurementSubpage')");
}

class SettingsReviewNotificationPermissionsTest : public SettingsBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kSafetyCheckNotificationPermissions};
};

IN_PROC_BROWSER_TEST_F(SettingsReviewNotificationPermissionsTest, All) {
  RunTest("settings/review_notification_permissions_test.js", "mocha.run()");
}

using SettingsRouteTest = SettingsBrowserTest;

IN_PROC_BROWSER_TEST_F(SettingsRouteTest, Basic) {
  RunTest("settings/route_test.js", "runMochaSuite('Basic')");
}

IN_PROC_BROWSER_TEST_F(SettingsRouteTest, DynamicParameters) {
  RunTest("settings/route_test.js", "runMochaSuite('DynamicParameters')");
}

IN_PROC_BROWSER_TEST_F(SettingsRouteTest, SafetyHubReachable) {
  RunTest("settings/route_test.js", "runMochaSuite('SafetyHubReachable')");
}

IN_PROC_BROWSER_TEST_F(SettingsRouteTest, SafetyHubNotReachable) {
  RunTest("settings/route_test.js", "runMochaSuite('SafetyHubNotReachable')");
}

// Copied from Polymer 2 test:
// Failing on ChromiumOS dbg. https://crbug.com/709442
#if (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)) && !defined(NDEBUG)
#define MAYBE_NonExistentRoute DISABLED_NonExistentRoute
#else
#define MAYBE_NonExistentRoute NonExistentRoute
#endif
IN_PROC_BROWSER_TEST_F(SettingsRouteTest, MAYBE_NonExistentRoute) {
  RunTest("settings/route_test.js", "runMochaSuite('NonExistentRoute')");
}

class SettingsSafetyCheckPermissionsTest : public SettingsBrowserTest {
 protected:
  SettingsSafetyCheckPermissionsTest() {
    scoped_feature_list_.InitWithFeatures(
        {
            content_settings::features::kSafetyCheckUnusedSitePermissions,
            features::kSafetyCheckNotificationPermissions,
            features::kSafetyCheckExtensions,
        },
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SettingsSafetyCheckPermissionsTest, All) {
  RunTest("settings/safety_check_permissions_test.js", "mocha.run()");
}

class SettingsSafetyHubTest : public SettingsBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{features::kSafetyHub};
};

IN_PROC_BROWSER_TEST_F(SettingsSafetyHubTest, SafetyHubCard) {
  RunTest("settings/safety_hub_card_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsSafetyHubTest, SafetyHubEntryPoint) {
  RunTest("settings/safety_hub_entry_point_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsSafetyHubTest, SafetyHubModule) {
  RunTest("settings/safety_hub_module_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsSafetyHubTest, SafetyHubPage) {
  RunTest("settings/safety_hub_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsSafetyHubTest, UnusedSitePermissionsModule) {
  RunTest("settings/safety_hub_unused_site_permissions_module_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsSafetyHubTest, NotificationPermissionsModule) {
  RunTest("settings/safety_hub_notification_permissions_module_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsSafetyHubTest, SafetyHubExtensions) {
  RunTest("settings/safety_hub_extensions_module_test.js", "mocha.run()");
}

class SettingsSecurityPageTest : public SettingsBrowserTest {
 protected:
  SettingsSecurityPageTest() {
    scoped_feature_list_.InitWithFeatures(
        {safe_browsing::kFriendlierSafeBrowsingSettingsEnhancedProtection,
         safe_browsing::kFriendlierSafeBrowsingSettingsStandardProtection},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SettingsSecurityPageTest, Main) {
  RunTest("settings/security_page_test.js", "runMochaSuite('Main')");
}

IN_PROC_BROWSER_TEST_F(SettingsSecurityPageTest, FlagsDisabled) {
  RunTest("settings/security_page_test.js", "runMochaSuite('FlagsDisabled')");
}

IN_PROC_BROWSER_TEST_F(SettingsSecurityPageTest,
                       SecurityPageHappinessTrackingSurveys) {
  RunTest("settings/security_page_test.js",
          "runMochaSuite('SecurityPageHappinessTrackingSurveys')");
}

// TODO(crbug.com/1403969): SafeBrowsing suite is flaky on Mac.
// TODO(crbug.com/1404109): SafeBrowsing suite is flaky on Linux and LaCrOS.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_SafeBrowsing DISABLED_SafeBrowsing
#else
#define MAYBE_SafeBrowsing SafeBrowsing
#endif
IN_PROC_BROWSER_TEST_F(SettingsSecurityPageTest, MAYBE_SafeBrowsing) {
  RunTest("settings/security_page_test.js", "runMochaSuite('SafeBrowsing')");
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
using SettingsSpellCheckPageTest = SettingsBrowserTest;

IN_PROC_BROWSER_TEST_F(SettingsSpellCheckPageTest, AllBuilds) {
  RunTest("settings/spell_check_page_test.js",
          "runMochaSuite('SpellCheck AllBuilds')");
}

IN_PROC_BROWSER_TEST_F(SettingsSpellCheckPageTest, Metrics) {
  RunTest("settings/spell_check_page_metrics_test_browser.js",
          "runMochaSuite('SpellCheckPageMetricsBrowser Metrics')");
}

#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(SettingsSpellCheckPageTest, MetricsNotMacOS) {
  RunTest("settings/spell_check_page_metrics_test_browser.js",
          "runMochaSuite('SpellCheckPageMetricsBrowser MetricsNotMacOS')");
}
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_F(SettingsSpellCheckPageTest, MetricsOfficialBuild) {
  RunTest("settings/spell_check_page_metrics_test_browser.js",
          "runMochaSuite('SpellCheckPageMetricsBrowser MetricsOfficialBuild')");
}

IN_PROC_BROWSER_TEST_F(SettingsSpellCheckPageTest, OfficialBuild) {
  RunTest("settings/spell_check_page_test.js",
          "runMochaSuite('SpellCheck OfficialBuild')");
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

class SettingsSiteDetailsTest : public SettingsBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      privacy_sandbox::kPrivacySandboxSettings4};
};

// Dabling on debug due to flaky timeout (crbug.com/825304,
// crbug.com/1021219) and win10 x64 (crbug.com/1490294).
#if !defined(NDEBUG) || (BUILDFLAG(IS_WIN) && defined(ARCH_CPU_X86_64))
#define MAYBE_SiteDetails DISABLED_SiteDetails
#else
#define MAYBE_SiteDetails SiteDetails
#endif
IN_PROC_BROWSER_TEST_F(SettingsSiteDetailsTest, MAYBE_SiteDetails) {
  RunTest("settings/site_details_test.js", "mocha.run()");
}

class SettingsSiteListTest : public SettingsBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      privacy_sandbox::kPrivacySandboxSettings4};
};

IN_PROC_BROWSER_TEST_F(SettingsSiteListTest, SiteList) {
  RunTest("settings/site_list_test.js", "runMochaSuite('SiteList')");
}

// TODO(crbug.com/929455, crbug.com/1064002): Flaky test. When it is fixed,
// merge SiteListDisabled back into SiteList.
IN_PROC_BROWSER_TEST_F(SettingsSiteListTest, DISABLED_SiteListDisabled) {
  RunTest("settings/site_list_test.js", "runMochaSuite('DISABLED_SiteList')");
}

IN_PROC_BROWSER_TEST_F(SettingsSiteListTest, SiteListEmbargoedOrigin) {
  RunTest("settings/site_list_test.js",
          "runMochaSuite('SiteListEmbargoedOrigin')");
}

// TODO(crbug.com/929455): When the bug is fixed, merge
// SiteListCookiesExceptionTypes into SiteList.
IN_PROC_BROWSER_TEST_F(SettingsSiteListTest, SiteListCookiesExceptionTypes) {
  RunTest("settings/site_list_test.js",
          "runMochaSuite('SiteListCookiesExceptionTypes')");
}

IN_PROC_BROWSER_TEST_F(SettingsSiteListTest, SiteListSearchTests) {
  RunTest("settings/site_list_test.js", "runMochaSuite('SiteListSearchTests')");
}

IN_PROC_BROWSER_TEST_F(SettingsSiteListTest, EditExceptionDialog) {
  RunTest("settings/site_list_test.js", "runMochaSuite('EditExceptionDialog')");
}

IN_PROC_BROWSER_TEST_F(SettingsSiteListTest, AddExceptionDialog) {
  RunTest("settings/site_list_test.js", "runMochaSuite('AddExceptionDialog')");
}

// TODO(crbug.com/1378703): Remove after crbug/1378703 launched.
IN_PROC_BROWSER_TEST_F(SettingsSiteListTest,
                       AddExceptionDialog_PrivacySandbox4Disabled) {
  RunTest("settings/site_list_test.js",
          "runMochaSuite('AddExceptionDialog_PrivacySandbox4Disabled')");
}

class SettingsSiteSettingsPageTest : public SettingsBrowserTest {
 protected:
  SettingsSiteSettingsPageTest() {
    scoped_feature_list_.InitWithFeatures(
        {
            privacy_sandbox::kPrivacySandboxSettings4,
            content_settings::features::kSafetyCheckUnusedSitePermissions,
            permissions::features::kPermissionStorageAccessAPI,
            features::kSafetyHub,
        },
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/1401833): Flaky.
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_SiteSettingsPage DISABLED_SiteSettingsPage
#else
#define MAYBE_SiteSettingsPage SiteSettingsPage
#endif
IN_PROC_BROWSER_TEST_F(SettingsSiteSettingsPageTest, MAYBE_SiteSettingsPage) {
  RunTest("settings/site_settings_page_test.js",
          "runMochaSuite('SiteSettingsPage')");
}

// TODO(crbug.com/1401833): Flaky.
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_PrivacySandboxSettings4Disabled \
  DISABLED_PrivacySandboxSettings4Disabled
#else
#define MAYBE_PrivacySandboxSettings4Disabled PrivacySandboxSettings4Disabled
#endif
IN_PROC_BROWSER_TEST_F(SettingsSiteSettingsPageTest,
                       MAYBE_PrivacySandboxSettings4Disabled) {
  RunTest("settings/site_settings_page_test.js",
          "runMochaSuite('PrivacySandboxSettings4Disabled')");
}

// TODO(crbug.com/1401833): Flaky.
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_UnusedSitePermissionsReview DISABLED_UnusedSitePermissionsReview
#else
#define MAYBE_UnusedSitePermissionsReview UnusedSitePermissionsReview
#endif
IN_PROC_BROWSER_TEST_F(SettingsSiteSettingsPageTest,
                       MAYBE_UnusedSitePermissionsReview) {
  RunTest("settings/site_settings_page_test.js",
          "runMochaSuite('UnusedSitePermissionsReview')");
}

// TODO(crbug.com/1401833): Flaky.
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_UnusedSitePermissionsReviewDisabled \
  DISABLED_UnusedSitePermissionsReviewDisabled
#else
#define MAYBE_UnusedSitePermissionsReviewDisabled \
  UnusedSitePermissionsReviewDisabled
#endif
IN_PROC_BROWSER_TEST_F(SettingsSiteSettingsPageTest,
                       MAYBE_UnusedSitePermissionsReviewDisabled) {
  RunTest("settings/site_settings_page_test.js",
          "runMochaSuite('UnusedSitePermissionsReviewDisabled')");
}

IN_PROC_BROWSER_TEST_F(SettingsSiteSettingsPageTest,
                       UnusedSitePermissionsReviewSafetyHubDisabled) {
  RunTest("settings/site_settings_page_test.js",
          "runMochaSuite('UnusedSitePermissionsReviewSafetyHubDisabled')");
}

IN_PROC_BROWSER_TEST_F(SettingsSiteSettingsPageTest,
                       PermissionStorageAccessApiDisabled) {
  RunTest("settings/site_settings_page_test.js",
          "runMochaSuite('PermissionStorageAccessApiDisabled')");
}

IN_PROC_BROWSER_TEST_F(SettingsSiteSettingsPageTest, SafetyHubDisabled) {
  RunTest("settings/site_settings_page_test.js",
          "runMochaSuite('SafetyHubDisabled')");
}

using SettingsTranslatePageTest = SettingsBrowserTest;

IN_PROC_BROWSER_TEST_F(SettingsTranslatePageTest, TranslateSettings) {
  RunTest("settings/translate_page_test.js",
          "runMochaSuite('TranslatePage TranslateSettings')");
}

IN_PROC_BROWSER_TEST_F(SettingsTranslatePageTest, AlwaysTranslateDialog) {
  RunTest("settings/translate_page_test.js",
          "runMochaSuite('TranslatePage AlwaysTranslateDialog')");
}

IN_PROC_BROWSER_TEST_F(SettingsTranslatePageTest, NeverTranslateDialog) {
  RunTest("settings/translate_page_test.js",
          "runMochaSuite('TranslatePage NeverTranslateDialog')");
}

IN_PROC_BROWSER_TEST_F(SettingsTranslatePageTest, MetricsBrowser) {
  RunTest("settings/translate_page_metrics_test_browser.js", "mocha.run()");
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

class SettingsUnusedSitePermissionsTest : public SettingsBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      content_settings::features::kSafetyCheckUnusedSitePermissions};
};

IN_PROC_BROWSER_TEST_F(SettingsUnusedSitePermissionsTest, All) {
  RunTest("settings/unused_site_permissions_test.js", "mocha.run()");
}
