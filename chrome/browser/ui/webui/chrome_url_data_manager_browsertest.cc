// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/ui/webui/welcome/helpers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/history_clusters/core/features.h"
#include "components/nacl/common/buildflags.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/search_engines_switches.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "ui/base/ui_base_features.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#else
#include "components/signin/public/base/signin_switches.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#endif

namespace {

class NavigationObserver : public content::WebContentsObserver {
 public:
  enum class Result {
    kNotFinished,
    kErrorPage,
    kSuccess,
  };

  explicit NavigationObserver(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  NavigationObserver(const NavigationObserver&) = delete;
  NavigationObserver& operator=(const NavigationObserver&) = delete;

  ~NavigationObserver() override = default;

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->IsInPrimaryMainFrame()) {
      return;
    }
    navigation_result_ = navigation_handle->IsErrorPage() ? Result::kErrorPage
                                                          : Result::kSuccess;
    net_error_ = navigation_handle->GetNetErrorCode();
    got_navigation_ = true;
    if (navigation_handle->HasCommitted() &&
        !navigation_handle->IsSameDocument() &&
        !navigation_handle->IsErrorPage()) {
      http_status_code_ =
          navigation_handle->GetResponseHeaders()->response_code();
    }
  }

  Result navigation_result() const { return navigation_result_; }
  net::Error net_error() const { return net_error_; }
  bool got_navigation() const { return got_navigation_; }
  int http_status_code() const { return http_status_code_; }

  void Reset() {
    navigation_result_ = Result::kNotFinished;
    net_error_ = net::OK;
  }

 private:
  Result navigation_result_ = Result::kNotFinished;
  net::Error net_error_ = net::OK;
  bool got_navigation_ = false;
  int http_status_code_ = -1;
};

}  // namespace

class ChromeURLDataManagerTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    content::URLDataSource::Add(
        browser()->profile(),
        std::make_unique<ThemeSource>(browser()->profile()));
  }
};

// Makes sure navigating to the new tab page results in a http status code
// of 200.
// TODO(crbug.com/40927037) Test Failing on Mac11 tests
#if BUILDFLAG(IS_MAC)
#define MAYBE_200 DISABLED_200
#else
#define MAYBE_200 200
#endif
IN_PROC_BROWSER_TEST_F(ChromeURLDataManagerTest, MAYBE_200) {
  NavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  EXPECT_TRUE(observer.got_navigation());
  EXPECT_EQ(200, observer.http_status_code());
}

// Makes sure browser does not crash when navigating to an unknown resource.
IN_PROC_BROWSER_TEST_F(ChromeURLDataManagerTest, UnknownResource) {
  // Known resource
  NavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://theme/IDR_SETTINGS_FAVICON")));
  EXPECT_EQ(NavigationObserver::Result::kSuccess, observer.navigation_result());
  EXPECT_EQ(net::OK, observer.net_error());

  // Unknown resource
  observer.Reset();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://theme/IDR_ASDFGHJKL")));
  EXPECT_EQ(NavigationObserver::Result::kErrorPage,
            observer.navigation_result());
  // The presence of net error means that navigation did not commit to the
  // original url.
  EXPECT_NE(net::OK, observer.net_error());
}

// Makes sure browser does not crash when the resource scale is very large.
IN_PROC_BROWSER_TEST_F(ChromeURLDataManagerTest, LargeResourceScale) {
  // Valid scale
  NavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://theme/IDR_SETTINGS_FAVICON@2x")));
  EXPECT_EQ(NavigationObserver::Result::kSuccess, observer.navigation_result());
  EXPECT_EQ(net::OK, observer.net_error());

  // Unreasonably large scale
  observer.Reset();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://theme/IDR_SETTINGS_FAVICON@99999x")));
  EXPECT_EQ(NavigationObserver::Result::kErrorPage,
            observer.navigation_result());
  // The presence of net error means that navigation did not commit to the
  // original url.
  EXPECT_NE(net::OK, observer.net_error());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
class PrefService;
#endif

class ChromeURLDataManagerWebUITrustedTypesTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<const char*> {
 public:
  ChromeURLDataManagerWebUITrustedTypesTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    enabled_features.push_back(history_clusters::kSidePanelJourneys);
    enabled_features.push_back(features::kSupportTool);
    enabled_features.push_back(ntp_features::kCustomizeChromeWallpaperSearch);
    enabled_features.push_back(
        optimization_guide::features::kOptimizationGuideModelExecution);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    enabled_features.push_back(whats_new::kForceEnabled);
#endif

#if !BUILDFLAG(IS_CHROMEOS)
    if (GetParam() == std::string_view("chrome://welcome")) {
      enabled_features.push_back(welcome::kForceEnabled);
    }
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    enabled_features.push_back(ash::features::kDriveFsMirroring);
    enabled_features.push_back(ash::features::kShimlessRMAOsUpdate);
    enabled_features.push_back(chromeos::features::kUploadOfficeToCloud);
#endif
    feature_list_.InitWithFeatures(enabled_features, {});
  }

  void CheckNoTrustedTypesViolation(std::string_view url) {
    const std::string kMessageFilter =
        "*Refused to create a TrustedTypePolicy*";
    content::WebContents* content =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::WebContentsConsoleObserver console_observer(content);
    console_observer.SetPattern(kMessageFilter);

    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));
    ASSERT_TRUE(content::WaitForLoadStop(content));
    EXPECT_TRUE(console_observer.messages().empty());
  }

  void CheckTrustedTypesEnabled(std::string_view url) {
    content::WebContents* content =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));
    ASSERT_TRUE(content::WaitForLoadStop(content));

    const char kIsTrustedTypesEnabled[] =
        "(function isTrustedTypesEnabled() {"
        "  try {"
        "    document.body.innerHTML = 'foo';"
        "  } catch(e) {"
        "    return true;"
        "  }"
        "  return false;"
        "})();";

    EXPECT_EQ(true, EvalJs(content, kIsTrustedTypesEnabled,
                           content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                           1 /* world_id */));
  }

  static std::string ParamInfoToString(
      const ::testing::TestParamInfo<const char*>& info) {
    std::string name(info.param);
    std::replace_if(
        name.begin(), name.end(),
        [](unsigned char c) { return !absl::ascii_isalnum(c); }, '_');
    return name;
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    if (GetParam() ==
        std::string_view(chrome::kChromeUISearchEngineChoiceURL)) {
      // Command line arguments needed to render chrome://search-engine-choice.
      command_line->AppendSwitchASCII(switches::kSearchEngineChoiceCountry,
                                      "BE");
      command_line->AppendSwitch(switches::kForceSearchEngineChoiceScreen);
      command_line->AppendSwitch(
          switches::kIgnoreNoFirstRunForSearchEngineChoiceScreen);
    }
#if BUILDFLAG(IS_CHROMEOS_ASH)
    command_line->AppendSwitchASCII(ash::switches::kSamlPasswordChangeUrl,
                                    "http://password-change.example");
    if (GetParam() == std::string_view("chrome://shimless-rma")) {
      command_line->AppendSwitchASCII(ash::switches::kLaunchRma, "");
    }
#endif
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetUpOnMainThread() override {
    browser()->profile()->GetPrefs()->SetBoolean(
        ash::prefs::kSamlInSessionPasswordChangeEnabled, true);

#if BUILDFLAG(IS_CHROMEOS)
    // This is needed to simulate the presence of the ODFS extension, which is
    // checked in `IsMicrosoftOfficeOneDriveIntegrationAllowedAndOdfsInstalled`.
    auto fake_provider =
        ash::file_system_provider::FakeExtensionProvider::Create(
            extension_misc::kODFSExtensionId);
    auto* service =
        ash::file_system_provider::Service::Get(browser()->profile());
    service->RegisterProvider(std::move(fake_provider));
#endif  // BUILDFLAG(IS_CHROMEOS)
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

 private:
  base::test::ScopedFeatureList feature_list_;
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  policy::FakeBrowserDMTokenStorage fake_dm_token_storage_;
#endif
};

// Verify that there's no Trusted Types violation in any `kChromeUrls`.
IN_PROC_BROWSER_TEST_P(ChromeURLDataManagerWebUITrustedTypesTest,
                       NoTrustedTypesViolation) {
  CheckNoTrustedTypesViolation(GetParam());
}

// Verify that Trusted Types checks are actually enabled for all `kChromeUrls`.
IN_PROC_BROWSER_TEST_P(ChromeURLDataManagerWebUITrustedTypesTest,
                       TrustedTypesEnabled) {
  CheckTrustedTypesEnabled(GetParam());
}

// Non-exhaustive list of chrome:// URLs to test for
//  1) TrustedTypes violations (see NoTrustedTypesViolation test).
//  2) Presence of TrustedTypes checks (see TrustedTypesEnabled test).
static constexpr const char* const kChromeUrls[] = {
    "chrome://accessibility",
// TODO:(https://crbug.com/1439754): Flakily crashes on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
    "chrome://app-service-internals",
#endif
    "chrome://attribution-internals",
    "chrome://autofill-internals",
    "chrome://bookmarks",
    "chrome://bookmarks-side-panel.top-chrome",
    "chrome://chrome-urls",
    "chrome://components",
    "chrome://connection-help",
    "chrome://connection-monitoring-detected",
// TODO(crbug.com/40913109): Re-enable this test
#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)
    "chrome://credits",
#endif
    "chrome://customize-chrome-side-panel.top-chrome",
    "chrome://device-log",
    // TODO(crbug.com/40710256): Test failure due to excessive output.
    // "chrome://discards",
    "chrome://download-internals",
    "chrome://downloads",
    "chrome://extensions",
    "chrome://extensions-internals",
    "chrome://flags",
    "chrome://gcm-internals",
    "chrome://gpu",
    "chrome://histograms",
    "chrome://history",
    "chrome://history-clusters-side-panel.top-chrome",
    "chrome://identity-internals",
    "chrome://indexeddb-internals",
    "chrome://inspect",
    "chrome://internals",
    "chrome://internals/session-service",
    "chrome://internals/user-education",
    "chrome://interstitials/ssl",
    "chrome://local-state",
    "chrome://management",
    "chrome://media-engagement",
    "chrome://media-internals",
    "chrome://media-router-internals",
    "chrome://metrics-internals",
    // TODO(crbug.com/40185163): DCHECK failure
    // "chrome://memory-internals",
    "chrome://net-export",
    "chrome://net-internals",
    "chrome://network-errors",
    "chrome://new-tab-page",
    "chrome://new-tab-page-third-party",
    "chrome://newtab",
    "chrome://ntp-tiles-internals",
    "chrome://omnibox",
    "chrome://password-manager",
    "chrome://password-manager-internals",
    "chrome://policy",
    "chrome://predictors",
    "chrome://prefs-internals",
    "chrome://privacy-sandbox-dialog/?debug",
    "chrome://process-internals",
    "chrome://quota-internals",
    "chrome://read-later.top-chrome",
    "chrome://reset-password",
    "chrome://safe-browsing",
    "chrome://search-engine-choice",
    "chrome://serviceworker-internals",
    "chrome://segmentation-internals",
    "chrome://settings",
    "chrome://signin-internals",
    "chrome://site-engagement",
    "chrome://support-tool",
    // TODO(crbug.com/40137561): Navigating to chrome://sync-confirmation and
    // quickly navigating away cause DCHECK failure.
    // "chrome://sync-confirmation",
    "chrome://sync-internals",
    "chrome://system",
    "chrome://tab-search.top-chrome",
    // TODO(crbug.com/40137562): Navigating to chrome://tab-strip and quickly
    // navigating away cause DCHECK failure.
    // "chrome://tab-strip",
    "chrome://terms",
    "chrome://topics-internals",
    "chrome://translate-internals",
    "chrome://ukm",
    "chrome://usb-internals",
    "chrome://user-actions",
    "chrome://version",
    "chrome://web-app-internals",
    "chrome://webrtc-internals",
    "chrome://webrtc-logs",
    "chrome://webui-gallery",

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    "chrome://whats-new",
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    "chrome://cast-feedback",
#endif

#if BUILDFLAG(IS_ANDROID)
    "chrome://explore-sites-internals",
    "chrome://internals/notifications",
    "chrome://internals/query-tiles",
    "chrome://offline-internals",
    "chrome://snippets-internals",
    "chrome://webapks",
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // TODO(crbug.com/40250441): Add CrOS-only WebUI URLs here as TrustedTypes
    // are deployed to more WebUIs.

    "chrome://accessory-update",
    "chrome://account-manager-error",
    "chrome://account-migration-welcome",
    "chrome://add-supervision/",
    "chrome://app-disabled",
    "chrome://camera-app/views/main.html",
    "chrome://assistant-optin/",
    "chrome://bluetooth-pairing",
    "chrome://certificate-manager/",

    // Crashes because message handler is not registered outside of the dialog
    // for confirm password change UI.
    // "chrome://confirm-password-change",

    // TODO(b/300875336): Navigating to chrome://cloud-upload causes an
    // assertion failure because there are no dialog args.
    "chrome://cloud-upload",

    "chrome://connectivity-diagnostics",
    "chrome://crostini-installer",
    "chrome://crostini-upgrader",
    "chrome://cryptohome",
    "chrome://diagnostics",
    "chrome://drive-internals",
    "chrome://emoji-picker",
    "chrome://family-link-user-internals",
    "chrome://file-manager",
    "chrome://help-app",
    "chrome://linux-proxy-config",
    "chrome://manage-mirrorsync",
    "chrome://multidevice-internals",
    "chrome://multidevice-setup",
    "chrome://nearby",
    "chrome://nearby-internals",
    "chrome://network",
    "chrome://office-fallback/",
    "chrome://os-feedback",
    "chrome-untrusted://os-feedback",
    "chrome://os-settings",
    "chrome://parent-access",
    "chrome://password-change",
    "chrome://personalization",
    "chrome://power",
    "chrome://print-management",
    "chrome-untrusted://projector",
    "chrome://proximity-auth/proximity_auth.html",
    "chrome://scanning",
    "chrome://set-time",
    "chrome://shimless-rma",
    "chrome://shortcut-customization",
    "chrome://slow",
    "chrome://smb-credentials-dialog/",
    "chrome://smb-share-dialog/",
    "chrome://urgent-password-expiry-notification/",
    "chrome://sys-internals",
#endif
#if !BUILDFLAG(IS_CHROMEOS)
    "chrome://apps",
    "chrome://browser-switch",
    "chrome://browser-switch/internals",
    "chrome://profile-picker",
    "chrome://welcome",
#endif
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    // Note: Disabled because a DCHECK fires when directly visiting the URL.
    // "chrome://managed-user-profile-notice",
    "chrome://intro",
    "chrome://profile-customization/?debug",
    "chrome://signin-email-confirmation",
#endif
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
    "chrome://connectors-internals",
    "chrome://crashes",
#endif
#if !BUILDFLAG(IS_MAC)
    "chrome://sandbox",
#endif  // !BUILDFLAG(IS_MAC)
#if BUILDFLAG(ENABLE_NACL)
    "chrome://nacl",
#endif
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS_LACROS)
    // TODO(crbug.com/40772380): this test is flaky on mac.
    "chrome://bluetooth-internals",
#endif
#if BUILDFLAG(IS_WIN)
    "chrome://conflicts",
#endif
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    "chrome://signin-dice-web-intercept.top-chrome/?debug",
    // Note: Disabled because a DCHECK fires when directly visiting the URL.
    // "chrome://signin-reauth",
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
// TODO(crbug.com/40250068): Uncomment when TrustedTypes are enabled.
// "chrome://chrome-signin",
#endif
#if BUILDFLAG(ENABLE_DICE_SUPPORT) && !BUILDFLAG(IS_CHROMEOS_ASH)
// TODO(crbug.com/40250068): Uncomment when TrustedTypes are enabled.
// "chrome://chrome-signin/?reason=5",
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    "chrome://webuijserror",
#endif
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
    "chrome://print",
#endif
};

INSTANTIATE_TEST_SUITE_P(
    ,
    ChromeURLDataManagerWebUITrustedTypesTest,
    ::testing::ValuesIn(kChromeUrls),
    ChromeURLDataManagerWebUITrustedTypesTest::ParamInfoToString);
