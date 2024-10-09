// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/config/coverage/buildflags.h"
#include "chrome/browser/preloading/preloading_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/compose/buildflags.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/content_settings/core/common/features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/performance_manager/public/features.h"
#include "components/permissions/features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "crypto/crypto_buildflags.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/compositor/compositor_switches.h"

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/browser_features.h"
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

class SettingsBrowserTest : public WebUIMochaBrowserTest {
 protected:
  SettingsBrowserTest() { set_test_loader_host(chrome::kChromeUISettingsHost); }
};

using SettingsTest = SettingsBrowserTest;

// Note: Keep tests below in alphabetical ordering.

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(SettingsTest, A11yPage) {
  RunTest("settings/a11y_page_test.js", "mocha.run()");
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)

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
// TODO(crbug.com/40856240) Test is flaky on ChromeOS
IN_PROC_BROWSER_TEST_F(SettingsTest, AppearancePage) {
  RunTest("settings/appearance_page_test.js", "mocha.run()");
}
#endif

IN_PROC_BROWSER_TEST_F(SettingsTest, AutofillAddressValidation) {
  RunTest("settings/autofill_section_address_validation_test.js",
          "mocha.run()");
}

// TODO(crbug.com/40258836): Clean up this test after Password Manager redesign
// is launched.
IN_PROC_BROWSER_TEST_F(SettingsTest, AutofillPage) {
  RunTest("settings/autofill_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, AutofillSection) {
  RunTest("settings/autofill_section_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, AutofillPredictionImprovementsSection) {
  RunTest("settings/autofill_prediction_improvements_section_test.js",
          "mocha.run()");
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(SettingsTest, AxAnnotationsSection) {
  RunTest("settings/ax_annotations_section_test.js", "mocha.run()");
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)

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

IN_PROC_BROWSER_TEST_F(SettingsTest, CrPolicyPrefIndicator) {
  RunTest("settings/cr_policy_pref_indicator_test.js", "mocha.run()");
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

class SettingsAiPageTest : public SettingsBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      optimization_guide::features::kAiSettingsPageRefresh};
};

IN_PROC_BROWSER_TEST_F(SettingsAiPageTest, ExperimentalAdvancedPage) {
  RunTest("settings/ai_page_test.js",
          "runMochaSuite('ExperimentalAdvancedPage')");
}

IN_PROC_BROWSER_TEST_F(SettingsAiPageTest,
                       ExperimentalAdvancedPageRefreshDisabled) {
  RunTest("settings/ai_page_test.js",
          "runMochaSuite('ExperimentalAdvancedPageRefreshDisabled')");
}

IN_PROC_BROWSER_TEST_F(SettingsAiPageTest, TabOrganizationSubpage) {
  RunTest("settings/ai_subpage_test.js",
          "runMochaSuite('TabOrganizationSubpage')");
}

IN_PROC_BROWSER_TEST_F(SettingsAiPageTest, HistorySearchSubpage) {
  RunTest("settings/ai_subpage_test.js",
          "runMochaSuite('HistorySearchSubpage')");
}

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

IN_PROC_BROWSER_TEST_F(SettingsTest, Prefs) {
  RunTest("settings/settings_prefs_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, PrefUtils) {
  RunTest("settings/settings_pref_util_test.js", "mocha.run()");
}

class PeoplePageSyncPageTest : public SettingsBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kLinkedServicesSetting};
};

// Timeout on Linux dbg bots: https://crbug.com/1394737
#if !(BUILDFLAG(IS_LINUX) && !defined(NDEBUG))
IN_PROC_BROWSER_TEST_F(PeoplePageSyncPageTest, SyncSettings) {
  RunTest("settings/people_page_sync_page_test.js",
          "runMochaSuite('SyncSettings')");
}
#endif

IN_PROC_BROWSER_TEST_F(PeoplePageSyncPageTest, EEAChoiceCountry) {
  RunTest("settings/people_page_sync_page_test.js",
          "runMochaSuite('EEAChoiceCountry')");
}

// TODO(crbug.com/324091979): Remove once crbug.com/324091979 launched.
IN_PROC_BROWSER_TEST_F(PeoplePageSyncPageTest, LinkedServicesDisabled) {
  RunTest("settings/people_page_sync_page_test.js",
          "runMochaSuite('LinkedServicesDisabled')");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, PerformanceMenu) {
  RunTest("settings/settings_performance_menu_test.js", "mocha.run()");
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

// TODO(crbug.com/40719198): Flaky on all OSes. Enable the test.
IN_PROC_BROWSER_TEST_F(SettingsTest, DISABLED_ResetPage) {
  RunTest("settings/reset_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ResetProfileBanner) {
  RunTest("settings/reset_profile_banner_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SafetyCheckPage) {
  RunTest("settings/safety_check_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ScrollableMixin) {
  RunTest("settings/scrollable_mixin_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, Search) {
  RunTest("settings/search_settings_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SearchEngineEntry) {
  RunTest("settings/search_engine_entry_test.js", "mocha.run()");
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
// Copied from Polymer 2 test. TODO(crbug.com/41439813): flaky, fix.
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

IN_PROC_BROWSER_TEST_F(SettingsTest, SmartCardReadersPage) {
  RunTest("settings/smart_card_readers_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, SmartCardReaderOriginEntry) {
  RunTest("settings/smart_card_reader_origin_entry_test.js", "mocha.run()");
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
// Flaky on all OSes. TODO(crbug.com/40825327): Enable the test.
IN_PROC_BROWSER_TEST_F(SettingsTest, DISABLED_Subpage) {
  RunTest("settings/settings_subpage_test.js", "mocha.run()");
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SettingsTest, SyncAccountControl) {
  RunTest("settings/sync_account_control_test.js", "mocha.run()");
}
#endif


IN_PROC_BROWSER_TEST_F(SettingsTest, TabDiscardExceptionDialog) {
  RunTest("settings/tab_discard_exception_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ToggleButton) {
  RunTest("settings/settings_toggle_button_test.js", "mocha.run()");
}

#if BUILDFLAG(ENABLE_COMPOSE)
class SettingsComposePageTest : public SettingsBrowserTest {
 public:
  SettingsComposePageTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{optimization_guide::features::
                                  kAiSettingsPageRefresh,
                              compose::features::kEnableComposeProactiveNudge},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SettingsComposePageTest, ComposePage) {
  RunTest("settings/offer_writing_help_page_test.js",
          "runMochaSuite('ComposePage')");
}

IN_PROC_BROWSER_TEST_F(SettingsComposePageTest, ComposePageRefreshDisabled) {
  RunTest("settings/offer_writing_help_page_test.js",
          "runMochaSuite('ComposePageRefreshDisabled')");
}
#endif  // BUILDFLAG(ENABLE_COMPOSE)

IN_PROC_BROWSER_TEST_F(SettingsTest, ZoomLevels) {
  RunTest("settings/zoom_levels_test.js", "mocha.run()");
}

#if !BUILDFLAG(IS_CHROMEOS)
class SettingsSystemPageTest : public SettingsBrowserTest {
 private:
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kRegisterOsUpdateHandlerWin};
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
};

IN_PROC_BROWSER_TEST_F(SettingsSystemPageTest, SystemPage) {
  RunTest("settings/system_page_test.js", "mocha.run()");
}
#endif

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

IN_PROC_BROWSER_TEST_F(SettingsAllSitesTest, EnableRelatedWebsiteSets) {
  RunTest("settings/all_sites_test.js",
          "runMochaSuite('EnableRelatedWebsiteSets')");
}

IN_PROC_BROWSER_TEST_F(SettingsAllSitesTest, DisableRelatedWebsiteSets) {
  RunTest("settings/all_sites_test.js",
          "runMochaSuite('DisableRelatedWebsiteSets')");
}

class SettingsBasicPageTest : public SettingsBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{features::kSafetyHub};
};

// TODO(crbug.com/40823128): Flaky on all platforms.
IN_PROC_BROWSER_TEST_F(SettingsBasicPageTest, DISABLED_BasicPage) {
  RunTest("settings/basic_page_test.js", "runMochaSuite('BasicPage')");
}

IN_PROC_BROWSER_TEST_F(SettingsBasicPageTest, PrivacyGuidePromo) {
  RunTest("settings/basic_page_test.js", "runMochaSuite('PrivacyGuidePromo')");
}

IN_PROC_BROWSER_TEST_F(SettingsBasicPageTest, Performance) {
  RunTest("settings/basic_page_test.js", "runMochaSuite('Performance')");
}

IN_PROC_BROWSER_TEST_F(SettingsBasicPageTest, SafetyHubDisabled) {
  RunTest("settings/basic_page_test.js", "runMochaSuite('SafetyHubDisabled')");
}

IN_PROC_BROWSER_TEST_F(SettingsBasicPageTest, ExperimentalAdvanced) {
  RunTest("settings/basic_page_test.js",
          "runMochaSuite('ExperimentalAdvanced')");
}

using SettingsClearBrowsingDataTest = SettingsBrowserTest;

// TODO(crbug.com/40707011): Flaky on Mac.
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
                       CbdTimeRangeExperiment_ExperimentOn) {
  RunTest("settings/clear_browsing_data_test.js",
          "runMochaSuite('CbdTimeRangeExperiment_ExperimentOn')");
}

IN_PROC_BROWSER_TEST_F(SettingsClearBrowsingDataTest,
                       CbdTimeRangeExperiment_ExperimentOff) {
  RunTest("settings/clear_browsing_data_test.js",
          "runMochaSuite('CbdTimeRangeExperiment_ExperimentOff')");
}

IN_PROC_BROWSER_TEST_F(SettingsClearBrowsingDataTest,
                       ClearBrowsingDataForSupervisedUsers) {
  RunTest("settings/clear_browsing_data_test.js",
          "runMochaSuite('ClearBrowsingDataForSupervisedUsers')");
}

class SettingsCookiesPageTest : public SettingsBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      privacy_sandbox::kPrivacySandboxFirstPartySetsUI};
};

#if ((BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && !defined(NDEBUG)) || \
    BUILDFLAG(IS_MAC)
#define MAYBE_CookiesPageTest DISABLED_CookiesPageTest
#else
#define MAYBE_CookiesPageTest CookiesPageTest
#endif
// TODO(crbug.com/40889245): fix flakiness on Linux and ChromeOS debug builds
// and re-enable.
IN_PROC_BROWSER_TEST_F(SettingsCookiesPageTest, MAYBE_CookiesPageTest) {
  RunTest("settings/cookies_page_test.js", "runMochaSuite('CookiesPageTest')");
}

IN_PROC_BROWSER_TEST_F(SettingsCookiesPageTest, ExceptionsList) {
  RunTest("settings/cookies_page_test.js", "runMochaSuite('ExceptionsList')");
}

IN_PROC_BROWSER_TEST_F(SettingsCookiesPageTest, FirstPartySetsUIDisabled) {
  RunTest("settings/cookies_page_test.js",
          "runMochaSuite('FirstPartySetsUIDisabled')");
}

IN_PROC_BROWSER_TEST_F(SettingsCookiesPageTest, TrackingProtectionSettings) {
  RunTest("settings/cookies_page_test.js",
          "runMochaSuite('TrackingProtectionSettings')");
}

IN_PROC_BROWSER_TEST_F(SettingsCookiesPageTest, ActSettings) {
  RunTest("settings/cookies_page_test.js", "runMochaSuite('ActSettings')");
}

// Test with --enable-pixel-output-in-tests enabled, required by fingerprint
// element test using HTML canvas.
class SettingsWithPixelOutputTest : public SettingsBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(::switches::kEnablePixelOutputInTests);
    SettingsBrowserTest::SetUpCommandLine(command_line);
  }
};

// https://crbug.com/1044390 - maybe flaky on Mac?
#if BUILDFLAG(IS_MAC)
#define MAYBE_FingerprintProgressArc DISABLED_FingerprintProgressArc
#else
#define MAYBE_FingerprintProgressArc FingerprintProgressArc
#endif
IN_PROC_BROWSER_TEST_F(SettingsWithPixelOutputTest,
                       MAYBE_FingerprintProgressArc) {
  RunTest("settings/fingerprint_progress_arc_test.js", "mocha.run()");
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

IN_PROC_BROWSER_TEST_F(SettingsPerformancePageTest, TabDiscardExceptionList) {
  RunTest("settings/performance_page_test.js",
          "runMochaSuite('TabDiscardExceptionList')");
}

IN_PROC_BROWSER_TEST_F(SettingsBrowserTest, DiscardIndicator) {
  RunTest("settings/performance_page_test.js",
          "runMochaSuite('DiscardIndicator')");
}

class SettingsPerformancePagePerformanceInterventionTest
    : public SettingsBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      performance_manager::features::kPerformanceInterventionUI};
};

IN_PROC_BROWSER_TEST_F(SettingsPerformancePagePerformanceInterventionTest,
                       PerformanceIntervention) {
  RunTest("settings/performance_page_test.js",
          "runMochaSuite('PerformanceIntervention')");
}

using SettingsMemoryPageTest = SettingsBrowserTest;

IN_PROC_BROWSER_TEST_F(SettingsMemoryPageTest, MemorySaver) {
  RunTest("settings/memory_page_test.js", "runMochaSuite('MemorySaver')");
}

IN_PROC_BROWSER_TEST_F(SettingsBrowserTest, MemorySaverAggressiveness) {
  RunTest("settings/memory_page_test.js",
          "runMochaSuite('MemorySaverAggressiveness')");
}

class SettingsPersonalizationOptionsTest : public SettingsBrowserTest {
 public:
  SettingsPersonalizationOptionsTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kPageContentOptIn},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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

// Privacy guide page tests.
class SettingsPrivacyGuideTest : public SettingsBrowserTest {
 protected:
  SettingsPrivacyGuideTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kPrivacyGuideForceAvailable,
         content_settings::features::kTrackingProtection3pcd},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, PrivacyGuidePage) {
  RunTest("settings/privacy_guide_page_test.js",
          "runMochaSuite('PrivacyGuidePage')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, FlowLength) {
  RunTest("settings/privacy_guide_page_test.js", "runMochaSuite('FlowLength')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, MsbbCardNavigations) {
  RunTest("settings/privacy_guide_page_test.js",
          "runMochaSuite('MsbbCardNavigations')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, HistorySyncCardNavigations) {
  RunTest("settings/privacy_guide_page_test.js",
          "runMochaSuite('HistorySyncCardNavigations')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, afeBrowsingCardNavigations) {
  RunTest("settings/privacy_guide_page_test.js",
          "runMochaSuite('SafeBrowsingCardNavigations')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, CookiesCardNavigations) {
  RunTest("settings/privacy_guide_page_test.js",
          "runMochaSuite('CookiesCardNavigations')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, AdTopicsCardNavigations) {
  RunTest("settings/privacy_guide_page_test.js",
          "runMochaSuite('AdTopicsCardNavigations')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, PrivacyGuideDialog) {
  RunTest("settings/privacy_guide_page_test.js",
          "runMochaSuite('PrivacyGuideDialog')");
}

// TODO(crbug.com/40942110): Re-enable when no longer flaky.
#if (BUILDFLAG(IS_LINUX) && !defined(NDEBUG))
#define MAYBE_3pcdOff DISABLED_3pcdOff
#else
#define MAYBE_3pcdOff 3pcdOff
#endif
IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, MAYBE_3pcdOff) {
  RunTest("settings/privacy_guide_page_test.js", "runMochaSuite('3pcdOff')");
}

// Privacy guide integration tests.
// TODO(crbug.com/40899379): Re-enable when no longer flaky.
#if (BUILDFLAG(IS_LINUX) && !defined(NDEBUG))
#define MAYBE_Integration DISABLED_Integration
#else
#define MAYBE_Integration Integration
#endif
IN_PROC_BROWSER_TEST_F(SettingsBrowserTest, MAYBE_Integration) {
  RunTest("settings/privacy_guide_integration_test.js", "mocha.run()");
}

// Privacy guide fragment tests.
IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, WelcomeFragment) {
  RunTest("settings/privacy_guide_fragments_test.js",
          "runMochaSuite('WelcomeFragment')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, MsbbFragment) {
  RunTest("settings/privacy_guide_fragments_test.js",
          "runMochaSuite('MsbbFragment')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, HistorySyncFragment) {
  RunTest("settings/privacy_guide_fragments_test.js",
          "runMochaSuite('HistorySyncFragment')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, SafeBrowsingFragment) {
  RunTest("settings/privacy_guide_fragments_test.js",
          "runMochaSuite('SafeBrowsingFragment')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, CookiesFragment) {
  RunTest("settings/privacy_guide_fragments_test.js",
          "runMochaSuite('CookiesFragment')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, CompletionFragment) {
  RunTest("settings/privacy_guide_completion_fragment_test.js",
          "runMochaSuite('CompletionFragment')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest,
                       CompletionFragmentPrivacySandboxRestricted) {
  RunTest("settings/privacy_guide_completion_fragment_test.js",
          "runMochaSuite('CompletionFragmentPrivacySandboxRestricted')");
}

IN_PROC_BROWSER_TEST_F(
    SettingsPrivacyGuideTest,
    CompletionFragmentPrivacySandboxRestrictedWithNoticeEnabled) {
  RunTest("settings/privacy_guide_completion_fragment_test.js",
          "runMochaSuite('"
          "CompletionFragmentPrivacySandboxRestrictedWithNoticeEnabled')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest,
                       CompletionFragmentWithAdTopicsCard) {
  RunTest("settings/privacy_guide_completion_fragment_test.js",
          "runMochaSuite('CompletionFragmentWithAdTopicsCard')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, AdTopicsFragment) {
  RunTest("settings/privacy_guide_ad_topics_fragment_test.js",
          "runMochaSuite('AdTopicsFragment')");
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
        {
#if BUILDFLAG(IS_CHROMEOS)
            blink::features::kWebPrinting,
#endif
            features::kEnableCertManagementUIV2,
            features::kSafetyHub,
        },
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

// TODO(crbug.com/40285326): This fails with the field trial testing config.
class SettingsPrivacyPageTestNoTestingConfig : public SettingsPrivacyPageTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SettingsPrivacyPageTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("disable-field-trial-config");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutomaticFullscreenContentSetting};
};

// Tests that the content settings page for Web Printing is not shown by
// default.
class SettingsPrivacyPageTestWithoutWebPrinting : public SettingsBrowserTest {};

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTestWithoutWebPrinting,
                       WebPrintingNotShown) {
  RunTest("settings/privacy_page_test.js",
          "runMochaSuite('WebPrintingNotShown')");
}

// Flaky on linux debug builds. https://crbug.com/331366001.
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_PrivacyPage DISABLED_PrivacyPage
#else
#define MAYBE_PrivacyPage PrivacyPage
#endif
IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTestNoTestingConfig,
                       MAYBE_PrivacyPage) {
  RunTest("settings/privacy_page_test.js", "runMochaSuite('PrivacyPage')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest, PrivacySandbox) {
  RunTest("settings/privacy_page_test.js", "runMochaSuite('PrivacySandbox')");
}

#if BUILDFLAG(USE_NSS_CERTS)
IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest, CertificateManagementV2) {
  RunTest("settings/privacy_page_test.js",
          "runMochaSuite('CertificateManagementV2')");
}
#endif  // BUILDFLAG(USE_NSS_CERTS)

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest, CookiesSubpage) {
  RunTest("settings/privacy_page_test.js", "runMochaSuite('CookiesSubpage')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest, TrackingProtectionSubpage) {
  RunTest("settings/privacy_page_test.js",
          "runMochaSuite('TrackingProtectionSubpage')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest, TrackingProtectionUxDisabled) {
  RunTest("settings/privacy_page_test.js",
          "runMochaSuite('TrackingProtectionUxDisabled')");
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

// TODO(crbug.com/40669164): flaky crash on Linux Tests (dbg).
IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest, DISABLED_PrivacyPageSound) {
  RunTest("settings/privacy_page_test.js", "runMochaSuite('PrivacyPageSound')");
}

// TODO(crbug.com/40710522): flaky failure on multiple platforms
IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest,
                       DISABLED_HappinessTrackingSurveys) {
  RunTest("settings/privacy_page_test.js",
          "runMochaSuite('HappinessTrackingSurveys')");
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// TODO(crbug.com/40669164): disabling due to failures on several builders.
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

class SettingsPrivacySandboxPageTest : public SettingsBrowserTest {};

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

IN_PROC_BROWSER_TEST_F(SettingsPrivacySandboxPageTest, ManageTopics) {
  RunTest("settings/privacy_sandbox_page_test.js",
          "runMochaSuite('ManageTopics')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacySandboxPageTest, FledgeSubpage) {
  RunTest("settings/privacy_sandbox_page_test.js",
          "runMochaSuite('FledgeSubpage')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacySandboxPageTest,
                       ManageTopicsAndAdTopicsPageState) {
  RunTest("settings/privacy_sandbox_page_test.js",
          "runMochaSuite('ManageTopicsAndAdTopicsPageState')");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ReviewNotificationPermissions) {
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

#if BUILDFLAG(ENABLE_EXTENSIONS)
class SettingsSafetyCheckPermissionsTest : public SettingsBrowserTest {
 protected:
  SettingsSafetyCheckPermissionsTest() {
    scoped_feature_list_.InitWithFeatures(
        {
            content_settings::features::kSafetyCheckUnusedSitePermissions,
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
#endif

class SettingsSafetyHubTest : public SettingsBrowserTest {
 protected:
  SettingsSafetyHubTest() {
    scoped_feature_list_.InitWithFeatures(
        {
            features::kSafetyHub,
            safe_browsing::kSafetyHubAbusiveNotificationRevocation,
        },
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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

#if BUILDFLAG(USE_JAVASCRIPT_COVERAGE)
// TODO(crbug.com/41496635): Webviews don't work properly with JS coverage.
#define MAYBE_SafetyHubPage DISABLED_SafetyHubPage
#else
#define MAYBE_SafetyHubPage SafetyHubPage
#endif
IN_PROC_BROWSER_TEST_F(SettingsSafetyHubTest, MAYBE_SafetyHubPage) {
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
        {
            features::kEnableCertManagementUIV2,
        },
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

// TODO(crbug/338155508): Enable this flaky test. This is flaky on Linux debug
// build.
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
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
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

class SettingsSiteDetailsTest : public SettingsBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutomaticFullscreenContentSetting};
};

// Disabling on debug due to flaky timeout on Win7 Tests (dbg)(1) bot.
// https://crbug.com/825304 - later for other platforms in crbug.com/1021219.
#if !defined(NDEBUG)
#define MAYBE_SiteDetails DISABLED_SiteDetails
#else
#define MAYBE_SiteDetails SiteDetails
#endif
IN_PROC_BROWSER_TEST_F(SettingsSiteDetailsTest, MAYBE_SiteDetails) {
  RunTest("settings/site_details_test.js", "mocha.run()");
}

class SettingsSiteListTest : public SettingsBrowserTest {};

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

// TODO(crbug.com/41439813): When the bug is fixed, merge
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

class SettingsSiteSettingsPageTest : public SettingsBrowserTest {
 protected:
  SettingsSiteSettingsPageTest() {
    scoped_feature_list_.InitWithFeatures(
        {
            content_settings::features::kSafetyCheckUnusedSitePermissions,
            features::kAutomaticFullscreenContentSetting,
            features::kSafetyHub,
        },
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/40884439): Flaky.
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_SiteSettingsPage DISABLED_SiteSettingsPage
#else
#define MAYBE_SiteSettingsPage SiteSettingsPage
#endif
IN_PROC_BROWSER_TEST_F(SettingsSiteSettingsPageTest, MAYBE_SiteSettingsPage) {
  RunTest("settings/site_settings_page_test.js",
          "runMochaSuite('SiteSettingsPage')");
}

// TODO(crbug.com/40884439): Flaky.
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

// TODO(crbug.com/40884439): Flaky.
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

IN_PROC_BROWSER_TEST_F(SettingsSiteSettingsPageTest, SafetyHubDisabled) {
  RunTest("settings/site_settings_page_test.js",
          "runMochaSuite('SafetyHubDisabled')");
}

IN_PROC_BROWSER_TEST_F(
    SettingsSiteSettingsPageTest,
    AbusiveNotificationsEnabledUnusedSitePermissionsDisabled) {
  RunTest("settings/site_settings_page_test.js",
          "runMochaSuite('"
          "AbusiveNotificationsEnabledUnusedSitePermissionsDisabled')");
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
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
