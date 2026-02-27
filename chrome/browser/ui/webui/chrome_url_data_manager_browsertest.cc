// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>

#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_timeouts.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/ui/webui/web_ui_all_urls_browser_test.h"
#include "chrome/browser/ui/webui/webui_urls_for_test.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

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
    : public WebUIAllUrlsBrowserTest {
 public:
  ChromeURLDataManagerWebUITrustedTypesTest() = default;

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

 private:
  // `BrowserTestBase::ProxyRunTestOnMainThreadLoop()` uses a reduced timeout
  // which can cause some of these tests to be flaky.
  static base::TimeDelta GetSlowTestTimeout() {
    return TestTimeouts::test_launcher_timeout();
  }
};

// Verify that there's no Trusted Types violation in any `kChromeUrls`.
IN_PROC_BROWSER_TEST_P(ChromeURLDataManagerWebUITrustedTypesTest,
                       NoTrustedTypesViolation) {
  CheckNoTrustedTypesViolation(GetParam());
  WaitBeforeNavigation();
}

// TODO(crbug.com/488078915): Re-enable this test on MacOS.
#if BUILDFLAG(IS_MAC)
#define MAYBE_TrustedTypesEnabled DISABLED_TrustedTypesEnabled
#else
#define MAYBE_TrustedTypesEnabled TrustedTypesEnabled
#endif
// Verify that Trusted Types checks are actually enabled for all `kChromeUrls`.
IN_PROC_BROWSER_TEST_P(ChromeURLDataManagerWebUITrustedTypesTest,
                       MAYBE_TrustedTypesEnabled) {
  CheckTrustedTypesEnabled(GetParam());
  WaitBeforeNavigation();
}

INSTANTIATE_TEST_SUITE_P(,
                         ChromeURLDataManagerWebUITrustedTypesTest,
                         ::testing::ValuesIn(kChromeUrls),
                         WebUIAllUrlsBrowserTest::ParamInfoToString);
