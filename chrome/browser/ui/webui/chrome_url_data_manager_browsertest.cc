// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_timeouts.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/ui/webui/webui_urls_for_test.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/collaboration/public/features.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/history_clusters/core/features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/variations/variations_switches.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "ui/base/ui_base_features.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#else
#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#include "components/signin/public/base/signin_switches.h"
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

#if BUILDFLAG(IS_CHROMEOS)
class PrefService;
#endif

// URLs known to be slow to load leading to test flakiness.
static constexpr const char* const kSlowChromeUrls[] = {
#if BUILDFLAG(IS_LINUX)
    "chrome://prefs-internals",
#else
    // Placeholder entry to prevent zero-sized array which causes template
    // instantiation failures with std::ranges algorithms in
    // std::ranges::contains.
    "",
#endif
};
class ChromeURLDataManagerWebUITrustedTypesTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<const char*> {
 public:
  ChromeURLDataManagerWebUITrustedTypesTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    enabled_features.push_back(ntp_features::kCustomizeChromeWallpaperSearch);
    enabled_features.push_back(
        optimization_guide::features::kOptimizationGuideModelExecution);
    enabled_features.push_back(collaboration::features::kCollaborationComments);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    enabled_features.push_back(whats_new::kForceEnabled);
#endif

#if BUILDFLAG(IS_CHROMEOS)
    enabled_features.push_back(ash::features::kDriveFsMirroring);
    enabled_features.push_back(ash::features::kShimlessRMAOsUpdate);
    enabled_features.push_back(chromeos::features::kUploadOfficeToCloud);
#endif
    feature_list_.InitWithFeatures(enabled_features, {});
  }

  void CheckNoTrustedTypesViolation(std::string_view url) {
    std::unique_ptr<base::test::ScopedRunLoopTimeout> timeout;
    if (std::ranges::contains(kSlowChromeUrls, url)) {
      timeout = std::make_unique<base::test::ScopedRunLoopTimeout>(
          FROM_HERE, GetSlowTestTimeout());
    }

    const std::string kMessageFilter =
        "*Creating a TrustedTypePolicy * violates the following Content "
        "Security Policy directive *";
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
    std::unique_ptr<base::test::ScopedRunLoopTimeout> timeout;
    if (std::ranges::contains(kSlowChromeUrls, url)) {
      timeout = std::make_unique<base::test::ScopedRunLoopTimeout>(
          FROM_HERE, GetSlowTestTimeout());
    }

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
      command_line->AppendSwitchASCII(
          variations::switches::kVariationsOverrideCountry, "BE");
      command_line->AppendSwitch(switches::kForceSearchEngineChoiceScreen);
      command_line->AppendSwitch(
          switches::kIgnoreNoFirstRunForSearchEngineChoiceScreen);
    }
#if BUILDFLAG(IS_CHROMEOS)
    command_line->AppendSwitchASCII(ash::switches::kSamlPasswordChangeUrl,
                                    "http://password-change.example");
    if (GetParam() == std::string_view("chrome://shimless-rma")) {
      command_line->AppendSwitchASCII(ash::switches::kLaunchRma, "");
    }
#endif
  }

#if BUILDFLAG(IS_CHROMEOS)
  void SetUpOnMainThread() override {
    browser()->profile()->GetPrefs()->SetBoolean(
        ash::prefs::kSamlInSessionPasswordChangeEnabled, true);

    // This is needed to simulate the presence of the ODFS extension, which is
    // checked in `IsMicrosoftOfficeOneDriveIntegrationAllowedAndOdfsInstalled`.
    auto fake_provider =
        ash::file_system_provider::FakeExtensionProvider::Create(
            extension_misc::kODFSExtensionId);
    auto* service =
        ash::file_system_provider::Service::Get(browser()->profile());
    service->RegisterProvider(std::move(fake_provider));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

 private:
  // `BrowserTestBase::ProxyRunTestOnMainThreadLoop()` uses a reduced timeout
  // which can cause some of these tests to be flaky.
  static base::TimeDelta GetSlowTestTimeout() {
    return TestTimeouts::test_launcher_timeout();
  }

  base::test::ScopedFeatureList feature_list_;
#if !BUILDFLAG(IS_CHROMEOS)
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

INSTANTIATE_TEST_SUITE_P(
    ,
    ChromeURLDataManagerWebUITrustedTypesTest,
    ::testing::ValuesIn(kChromeUrls),
    ChromeURLDataManagerWebUITrustedTypesTest::ParamInfoToString);
