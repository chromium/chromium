// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/config/coverage/buildflags.h"
#include "chrome/browser/preloading/preloading_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/compose/buildflags.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/content_settings/core/common/features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/permissions/features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "crypto/crypto_buildflags.h"
#include "device/fido/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/compositor/compositor_switches.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/toasts/toast_features.h"  // nogncheck
#endif

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/browser_features.h"
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(ENABLE_COMPOSE)
#include "chrome/browser/compose/compose_enabling.h"
#endif  // BUILDFLAG(ENABLE_COMPOSE)

class SettingsBrowserTest : public WebUIMochaBrowserTest {
 protected:
  SettingsBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {
#if BUILDFLAG(ENABLE_GLIC)
            features::kGlic, features::kTabstripComboButton,
#endif
            privacy_sandbox::kPrivacySandboxRelatedWebsiteSetsUi,
            privacy_sandbox::kFingerprintingProtectionUx},
        /*disabled_features=*/{});
    set_test_loader_host(chrome::kChromeUISettingsHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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

IN_PROC_BROWSER_TEST_F(SettingsTest, AutofillAiSection) {
  RunTest("settings/autofill_ai_section_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, AutofillAiAddOrEditDialog) {
  RunTest("settings/autofill_ai_add_or_edit_dialog_test.js", "mocha.run()");
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

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SettingsTest, EditDictionaryPage) {
  RunTest("settings/edit_dictionary_page_test.js", "mocha.run()");
}
#endif

IN_PROC_BROWSER_TEST_F(SettingsTest, AiPage) {
  RunTest("settings/ai_page_test.js", "runMochaSuite('AiPage')");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, AiInfoCard) {
  RunTest("settings/ai_info_card_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, TabOrganizationSubpage) {
  RunTest("settings/ai_tab_organization_subpage_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, HistorySearchSubpage) {
  RunTest("settings/ai_history_search_subpage_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, CompareSubpage) {
  RunTest("settings/ai_compare_subpage_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, LoggingInfoBullet) {
  RunTest("settings/ai_logging_info_bullet_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, PolicyIndicator) {
  RunTest("settings/ai_policy_indicator_test.js", "mocha.run()");
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

IN_PROC_BROWSER_TEST_F(SettingsTest, IncognitoTrackingProtectionsPageTest) {
  RunTest("settings/incognito_tracking_protections_page_test.js",
          "runMochaSuite('IncognitoTrackingProtectionsPageTest')");
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SettingsTest, ImportDataDialog) {
  RunTest("settings/import_data_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, Languages) {
  RunTest("settings/languages_test.js", "mocha.run()");
}

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

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)
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

IN_PROC_BROWSER_TEST_F(SettingsTest, PaymentsSectionPayOverTime) {
  RunTest("settings/payments_section_pay_over_time_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, PaymentsSectionPaymentsList) {
  RunTest("settings/payments_section_payments_list_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, PeoplePage) {
  RunTest("settings/people_page_test.js", "mocha.run()");
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SettingsTest, PeoplePageChromeOS) {
  RunTest("settings/people_page_test_cros.js", "mocha.run()");
}
#endif

#if !BUILDFLAG(IS_CHROMEOS)
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

#if BUILDFLAG(ENABLE_GLIC)
IN_PROC_BROWSER_TEST_F(SettingsTest, GlicSettingsPage) {
  RunTest("settings/glic_page_test.js", "runMochaSuite('GlicPage Default')");
}

class SettingsGlicPageLearnMoreTest : public SettingsBrowserTest {
 public:
  SettingsGlicPageLearnMoreTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicLearnMoreURLConfig,
          {
              {"glic-shortcuts-learn-more-url", "https://google.com/"},
          }}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SettingsGlicPageLearnMoreTest,
                       GlicSettingsLearnMoreEnabled) {
  RunTest("settings/glic_page_test.js",
          "runMochaSuite('GlicPage LearnMoreEnabled')");
}

class SettingsGlicPageHeaderLearnMoreTest : public SettingsBrowserTest {
 public:
  SettingsGlicPageHeaderLearnMoreTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicLearnMoreURLConfig,
          {
              {"glic-settings-page-learn-more-url", "https://google.com/"},
          }}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SettingsGlicPageHeaderLearnMoreTest,
                       GlicSettingsHeaderLearnMoreEnabled) {
  RunTest("settings/glic_page_test.js",
          "runMochaSuite('GlicPage HeaderLearnMoreEnabled')");
}

class SettingsGlicPageLauncherToggleLearnMoreTest : public SettingsBrowserTest {
 public:
  SettingsGlicPageLauncherToggleLearnMoreTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicLearnMoreURLConfig,
          {
              {"glic-shortcuts-launcher-toggle-learn-more-url",
               "https://google.com/"},
          }}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SettingsGlicPageLauncherToggleLearnMoreTest,
                       GlicSettingsLauncherToggleLearnMoreEnabled) {
  RunTest("settings/glic_page_test.js",
          "runMochaSuite('GlicPage LauncherToggleLearnMoreEnabled')");
}

class SettingsGlicPageLocationToggleLearnMoreTest : public SettingsBrowserTest {
 public:
  SettingsGlicPageLocationToggleLearnMoreTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicLearnMoreURLConfig,
          {
              {"glic-shortcuts-location-toggle-learn-more-url",
               "https://google.com/"},
          }}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SettingsGlicPageLocationToggleLearnMoreTest,
                       GlicSettingsLocationToggleLearnMoreEnabled) {
  RunTest("settings/glic_page_test.js",
          "runMochaSuite('GlicPage LocationToggleLearnMoreEnabled')");
}

class SettingsGlicPageTabAccessToggleLearnMoreTest
    : public SettingsBrowserTest {
 public:
  SettingsGlicPageTabAccessToggleLearnMoreTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicLearnMoreURLConfig,
          {
              {"glic-shortcuts-tab-access-toggle-learn-more-url",
               "https://google.com/"},
          }}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SettingsGlicPageTabAccessToggleLearnMoreTest,
                       GlicSettingsTabAccessToggleLearnMoreEnabled) {
  RunTest("settings/glic_page_test.js",
          "runMochaSuite('GlicPage TabAccessToggleLearnMoreEnabled')");
}

class SettingsGlicPageClosedCaptionsToggleTest : public SettingsBrowserTest {
 public:
  SettingsGlicPageClosedCaptionsToggleTest() {
    scoped_feature_list_.InitWithFeatures({features::kGlicClosedCaptioning},
                                          /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SettingsGlicPageClosedCaptionsToggleTest,
                       SettingsGlicPageClosedCaptionsToggleEnabled) {
  RunTest("settings/glic_page_test.js",
          "runMochaSuite('GlicPage ClosedCaptionsToggleEnabled')");
}
#endif

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

#if !BUILDFLAG(IS_CHROMEOS)
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

// TODO(crbug.com/372493822): remove when hybrid linking is disabled by default.
class HybridDisabledSettingsTest : public SettingsTest {
 public:
  HybridDisabledSettingsTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{device::kWebAuthnHybridLinking});
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(HybridDisabledSettingsTest, SecurityKeysSubpage) {
  RunTest("settings/security_keys_subpage_test.js", "mocha.run()");
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

#if BUILDFLAG(IS_CHROMEOS)
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

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SettingsTest, SmartCardReadersPage) {
  RunTest("settings/smart_card_readers_page_test.js", "mocha.run()");
}
#endif  // BUILDFLAG(IS_CHROMEOS)

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

#if !BUILDFLAG(IS_CHROMEOS)
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
        /*enabled_features=*/{compose::features::kEnableComposeProactiveNudge},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    SettingsBrowserTest::SetUpOnMainThread();
    scoped_enable_compose_ = ComposeEnabling::ScopedEnableComposeForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  ComposeEnabling::ScopedOverride scoped_enable_compose_;
};

IN_PROC_BROWSER_TEST_F(SettingsComposePageTest, ComposePage) {
  RunTest("settings/offer_writing_help_page_test.js",
          "runMochaSuite('ComposePage')");
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

IN_PROC_BROWSER_TEST_F(SettingsAllSitesTest, WithoutRelatedWebsiteSetsData) {
  RunTest("settings/all_sites_test.js",
          "runMochaSuite('WithoutRelatedWebsiteSetsData')");
}

// TODO(crbug.com/40823128): Flaky on all platforms.
IN_PROC_BROWSER_TEST_F(SettingsTest, DISABLED_BasicPage) {
  RunTest("settings/basic_page_test.js", "runMochaSuite('BasicPage')");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, PrivacyGuidePromo) {
  RunTest("settings/basic_page_test.js", "runMochaSuite('PrivacyGuidePromo')");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, Performance) {
  RunTest("settings/basic_page_test.js", "runMochaSuite('Performance')");
}

IN_PROC_BROWSER_TEST_F(SettingsTest, AiSections) {
  RunTest("settings/basic_page_test.js", "runMochaSuite('AiSections')");
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

#if !BUILDFLAG(IS_CHROMEOS)
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

class SettingsClearBrowsingDataV2Test : public SettingsBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kDbdRevampDesktop};
};

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SettingsClearBrowsingDataV2Test,
                       DeleteBrowsingDataAccountIndicator) {
  RunTest("settings/clear_browsing_data_account_indicator_test.js",
          "runMochaSuite('DeleteBrowsingDataAccountIndicator')");
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(SettingsClearBrowsingDataV2Test,
                       DeleteBrowsingDataDialog) {
  RunTest("settings/clear_browsing_data_dialog_v2_test.js",
          "runMochaSuite('DeleteBrowsingDataDialog')");
}

IN_PROC_BROWSER_TEST_F(SettingsClearBrowsingDataV2Test,
                       DeleteBrowsingDataTimePicker) {
  RunTest("settings/clear_browsing_data_time_picker_test.js",
          "runMochaSuite('DeleteBrowsingDataTimePicker')");
}

using SettingsCookiesPageTest = SettingsBrowserTest;

// TODO(crbug.com/40889245): fix flakiness on almost all platforms and
// re-enable.
IN_PROC_BROWSER_TEST_F(SettingsCookiesPageTest, DISABLED_CookiesPageTest) {
  RunTest("settings/cookies_page_test.js", "runMochaSuite('CookiesPageTest')");
}

IN_PROC_BROWSER_TEST_F(SettingsCookiesPageTest, ExceptionsList) {
  RunTest("settings/cookies_page_test.js", "runMochaSuite('ExceptionsList')");
}

IN_PROC_BROWSER_TEST_F(SettingsCookiesPageTest, TrackingProtectionSettings) {
  RunTest("settings/cookies_page_test.js",
          "runMochaSuite('TrackingProtectionSettings')");
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

IN_PROC_BROWSER_TEST_F(SettingsWithPixelOutputTest, CrLottie) {
  RunTest("settings/cr_lottie_test.js", "mocha.run()");
}

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

#if !BUILDFLAG(IS_CHROMEOS)
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

IN_PROC_BROWSER_TEST_F(SettingsPerformancePageTest, PerformanceIntervention) {
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
};

IN_PROC_BROWSER_TEST_F(SettingsPersonalizationOptionsTest, AllBuilds) {
  RunTest("settings/personalization_options_test.js",
          "runMochaSuite('AllBuilds')");
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
         content_settings::features::kTrackingProtection3pcd,
         optimization_guide::features::kPrivacyGuideAiSettings,
         privacy_sandbox::kAlwaysBlock3pcsIncognito},
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
  RunTest("settings/privacy_guide_welcome_fragment_test.js",
          "runMochaSuite('WelcomeFragment')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, MsbbFragment) {
  RunTest("settings/privacy_guide_msbb_fragment_test.js",
          "runMochaSuite('MsbbFragment')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, HistorySyncFragment) {
  RunTest("settings/privacy_guide_history_sync_fragment_test.js",
          "runMochaSuite('HistorySyncFragment')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, SafeBrowsingFragment) {
  RunTest("settings/privacy_guide_safe_browsing_fragment_test.js",
          "runMochaSuite('SafeBrowsingFragment')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, CookiesFragment) {
  RunTest("settings/privacy_guide_cookies_fragment_test.js",
          "runMochaSuite('CookiesFragment')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest,
                       CookiesFragmentAlwaysBlock3pcsIncognitoDisabled) {
  RunTest("settings/privacy_guide_cookies_fragment_test.js",
          "runMochaSuite('CookiesFragmentAlwaysBlock3pcsIncognitoDisabled')");
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

IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest,
                       CompletionFragmentAiSettingsInPrivacyGuideDisabled) {
  RunTest(
      "settings/privacy_guide_completion_fragment_test.js",
      "runMochaSuite('CompletionFragmentAiSettingsInPrivacyGuideDisabled')");
}

// TODO(crbug.com/410848707): Re-enable this test
#if BUILDFLAG(IS_MAC)
#define MAYBE_AdTopicsFragment DISABLED_AdTopicsFragment
#else
#define MAYBE_AdTopicsFragment AdTopicsFragment
#endif
IN_PROC_BROWSER_TEST_F(SettingsPrivacyGuideTest, MAYBE_AdTopicsFragment) {
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
            features::kDbdRevampDesktop,
            privacy_sandbox::kPrivacySandboxRelatedWebsiteSetsUi,
            permissions::features::kPermissionSiteSettingsRadioButton,
            privacy_sandbox::kFingerprintingProtectionUx,
            safe_browsing::kBundledSecuritySettings,
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

// Tests that the content settings page for Web Printing is not shown by
// default.
class SettingsPrivacyPageTestWithoutWebPrinting : public SettingsBrowserTest {};

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTestWithoutWebPrinting,
                       WebPrintingNotShown) {
  RunTest("settings/privacy_page_test.js",
          "runMochaSuite('WebPrintingNotShown')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest, PrivacyPage) {
  RunTest("settings/privacy_page_test.js", "runMochaSuite('PrivacyPage')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest, ContentSettingsVisibility) {
  RunTest("settings/privacy_page_test.js",
          "runMochaSuite('ContentSettingsVisibility')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest, PrivacySandbox) {
  RunTest("settings/privacy_page_test.js", "runMochaSuite('PrivacySandbox')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest,
                       CookiesSubpageRedesignDisabled) {
  RunTest("settings/privacy_page_test.js",
          "runMochaSuite('CookiesSubpageRedesignDisabled')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest, CookiesSubpage) {
  RunTest("settings/privacy_page_test.js", "runMochaSuite('CookiesSubpage')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest,
                       IncognitoTrackingProtectionsSubpage) {
  RunTest("settings/privacy_page_test.js",
          "runMochaSuite('IncognitoTrackingProtectionsSubpage')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest, AllSitesSubpage) {
  RunTest("settings/privacy_page_test.js", "runMochaSuite('AllSitesSubpage')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest, PrivacyGuideRow) {
  RunTest("settings/privacy_page_test.js", "runMochaSuite('PrivacyGuideRow')");
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

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest,
                       EnableWebBluetoothNewPermissionsBackend) {
  RunTest("settings/privacy_page_test.js",
          "runMochaSuite('EnableWebBluetoothNewPermissionsBackend')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest,
                       DeleteBrowsingDataRevampDisabled) {
  RunTest("settings/privacy_page_test.js",
          "runMochaSuite('DeleteBrowsingDataRevampDisabled')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacyPageTest,
                       BundledSecuritySettingsDisabled) {
  RunTest("settings/privacy_page_test.js",
          "runMochaSuite('BundledSecuritySettingsDisabled')");
}

class SettingsNotificationsPageTest : public SettingsBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      permissions::features::kPermissionSiteSettingsRadioButton};
};

IN_PROC_BROWSER_TEST_F(SettingsNotificationsPageTest, NotificationsPage) {
  RunTest("settings/notifications_page_test.js",
          "runMochaSuite('NotificationsPage')");
}

IN_PROC_BROWSER_TEST_F(SettingsNotificationsPageTest,
                       NotificationsPageWithNestedRadioButton) {
  RunTest("settings/notifications_page_test.js",
          "runMochaSuite('NotificationsPageWithNestedRadioButton')");
}

IN_PROC_BROWSER_TEST_F(SettingsNotificationsPageTest,
                       NotificationPermissionReview) {
  RunTest("settings/notifications_page_test.js",
          "runMochaSuite('NotificationPermissionReview')");
}

class SettingsGeolocationPageTest : public SettingsBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      permissions::features::kPermissionSiteSettingsRadioButton};
};

IN_PROC_BROWSER_TEST_F(SettingsGeolocationPageTest, GeolocationPage) {
  RunTest("settings/geolocation_page_test.js",
          "runMochaSuite('GeolocationPage')");
}

IN_PROC_BROWSER_TEST_F(SettingsGeolocationPageTest,
                       GeolocationPageWithNestedRadioButton) {
  RunTest("settings/geolocation_page_test.js",
          "runMochaSuite('GeolocationPageWithNestedRadioButton')");
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

IN_PROC_BROWSER_TEST_F(SettingsPrivacySandboxPageTest,
                       TopicsSubpageAdsApiUxEnhancementsDisabled) {
  RunTest("settings/privacy_sandbox_page_test.js",
          "runMochaSuite('TopicsSubpageAdsApiUxEnhancementsDisabled')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacySandboxPageTest,
                       TopicsSubpageAdsApiUxEnhancements) {
  RunTest("settings/privacy_sandbox_page_test.js",
          "runMochaSuite('TopicsSubpageAdsApiUxEnhancements')");
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

IN_PROC_BROWSER_TEST_F(SettingsPrivacySandboxPageTest,
                       AdMeasurementSubpageAdsApiUxEnhancements) {
  RunTest("settings/privacy_sandbox_page_test.js",
          "runMochaSuite('AdMeasurementSubpageAdsApiUxEnhancements')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacySandboxPageTest,
                       AdMeasurementSubpageAdsApiUxEnhancementsDisabled) {
  RunTest("settings/privacy_sandbox_page_test.js",
          "runMochaSuite('AdMeasurementSubpageAdsApiUxEnhancementsDisabled')");
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
                       SiteSuggestedAdsSubpageAdsApiUxEnhancement) {
  RunTest("settings/privacy_sandbox_page_test.js",
          "runMochaSuite('SiteSuggestedAdsSubpageAdsApiUxEnhancement')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacySandboxPageTest,
                       ManageTopicsAndAdTopicsPageState) {
  RunTest("settings/privacy_sandbox_page_test.js",
          "runMochaSuite('ManageTopicsAndAdTopicsPageState')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacySandboxPageTest,
                       TopicsSubpageAdTopicsContentParity) {
  RunTest("settings/privacy_sandbox_page_test.js",
          "runMochaSuite('TopicsSubpageAdTopicsContentParity')");
}

IN_PROC_BROWSER_TEST_F(SettingsPrivacySandboxPageTest,
                       TopicsSubpageAdTopicsContentParityDisabled) {
  RunTest("settings/privacy_sandbox_page_test.js",
          "runMochaSuite('TopicsSubpageAdTopicsContentParityDisabled')");
}

IN_PROC_BROWSER_TEST_F(
    SettingsPrivacySandboxPageTest,
    TopicsSubpageAdTopicsContentParityAdsApiUxEnhancementDisabled) {
  RunTest("settings/privacy_sandbox_page_test.js",
          "runMochaSuite('"
          "TopicsSubpageAdTopicsContentParityAdsApiUxEnhancementDisabled')");
}

using SettingsRouteTest = SettingsBrowserTest;

IN_PROC_BROWSER_TEST_F(SettingsRouteTest, Basic) {
  RunTest("settings/route_test.js", "runMochaSuite('Basic')");
}

IN_PROC_BROWSER_TEST_F(SettingsRouteTest, DynamicParameters) {
  RunTest("settings/route_test.js", "runMochaSuite('DynamicParameters')");
}

IN_PROC_BROWSER_TEST_F(SettingsRouteTest, SafetyHub) {
  RunTest("settings/route_test.js", "runMochaSuite('SafetyHub')");
}

// Copied from Polymer 2 test:
// Failing on ChromiumOS dbg. https://crbug.com/709442
#if (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)) && !defined(NDEBUG)
#define MAYBE_NonExistentRoute DISABLED_NonExistentRoute
#else
#define MAYBE_NonExistentRoute NonExistentRoute
#endif
IN_PROC_BROWSER_TEST_F(SettingsRouteTest, MAYBE_NonExistentRoute) {
  RunTest("settings/route_test.js", "runMochaSuite('NonExistentRoute')");
}

class SettingsSafetyHubTest : public SettingsBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      safe_browsing::kSafetyHubAbusiveNotificationRevocation};
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

using SettingsSecurityPageTest = SettingsBrowserTest;

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
// TODO(crbug.com/409069315): Re-enable this test on Mac.
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG) || BUILDFLAG(IS_MAC)
#define MAYBE_SafeBrowsing DISABLED_SafeBrowsing
#else
#define MAYBE_SafeBrowsing SafeBrowsing
#endif
IN_PROC_BROWSER_TEST_F(SettingsSecurityPageTest, MAYBE_SafeBrowsing) {
  RunTest("settings/security_page_test.js", "runMochaSuite('SafeBrowsing')");
}

#if !BUILDFLAG(IS_CHROMEOS)
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
#endif  // !BUILDFLAG(IS_CHROMEOS)

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

#if !BUILDFLAG(IS_CHROMEOS)
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
#endif  // !BUILDFLAG(IS_CHROMEOS)
