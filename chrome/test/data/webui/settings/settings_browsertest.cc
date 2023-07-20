// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class SettingsBrowserTest : public WebUIMochaBrowserTest {
 protected:
  SettingsBrowserTest() { set_test_loader_host(chrome::kChromeUISettingsHost); }
};

using SettingsTest = SettingsBrowserTest;

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

IN_PROC_BROWSER_TEST_F(SettingsTest, BatteryPage) {
  RunTest("settings/battery_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, CategorySettingExceptions) {
  RunTest("settings/category_setting_exceptions_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, Checkbox) {
  RunTest("settings/checkbox_test.js", "mocha.run()");
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
  RunTest("settings/payments_section_card_rows_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, PaymentsSectionIban) {
  RunTest("settings/payments_section_iban_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, PaymentsSectionUpi) {
  RunTest("settings/payments_section_upi_test.js", "mocha.run()");
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

IN_PROC_BROWSER_TEST_F(SettingsTest, SafetyHub) {
  RunTest("settings/safety_hub_test.js", "mocha.run()");
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

IN_PROC_BROWSER_TEST_F(SettingsTest, SecureDns) {
  RunTest("settings/secure_dns_test.js", "mocha.run()");
}

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

IN_PROC_BROWSER_TEST_F(SettingsTest, StartupUrlsPage) {
  RunTest("settings/startup_urls_page_test.js", "mocha.run()");
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

IN_PROC_BROWSER_TEST_F(SettingsTest, TabDiscardExceptionDialog) {
  RunTest("settings/tab_discard_exception_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ToggleButton) {
  RunTest("settings/settings_toggle_button_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, ZoomLevels) {
  RunTest("settings/zoom_levels_test.js", "mocha.run()");
}
