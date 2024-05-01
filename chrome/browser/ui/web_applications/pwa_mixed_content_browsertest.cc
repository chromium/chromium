// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/ssl_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace {

constexpr const char kImagePath[] = "/ssl/google_files/logo.gif";

// Returns a path string that points to a page with the
// "REPLACE_WITH_HOST_AND_PORT" string replaced with |host_port_pair|.
// The page at |original_path| should contain the string
// "REPLACE_WITH_HOST_AND_PORT".
std::string GetPathWithHostAndPortReplaced(
    const std::string& original_path,
    const net::HostPortPair& host_port_pair) {
  base::StringPairs replacement_text = {
      {"REPLACE_WITH_HOST_AND_PORT", host_port_pair.ToString()}};
  LOG(ERROR) << "host_port_pair.ToString() " << host_port_pair.ToString();
  return net::test_server::GetFilePathWithReplacements(original_path,
                                                       replacement_text);
}

// Tries to load an image at |image_url| and returns whether or not it loaded
// successfully.
//
// The image could fail to load because it was blocked from being loaded or
// because |image_url| doesn't exist. Therefore, it failing to load is not a
// reliable indicator of insecure content being blocked. Users of the function
// should check the state of security indicators.
bool TryToLoadImage(const content::ToRenderFrameHost& adapter,
                    const GURL& image_url) {
  const std::string script = base::StringPrintf(
      "let i = document.createElement('img');"
      "document.body.appendChild(i);"
      "new Promise(resolve => {"
      "  i.addEventListener('load', () => resolve(true));"
      "  i.addEventListener('error', () => resolve(false));"
      "  i.src = '%s';"
      "});",
      image_url.spec().c_str());

  return content::EvalJs(adapter, script).ExtractBool();
}

}  // anonymous namespace

namespace web_app {

class PWAMixedContentBrowserTest : public WebAppBrowserTestBase {
 public:
  GURL GetMixedContentAppURL() {
    return https_server()->GetURL("app.com",
                                  "/ssl/page_displays_insecure_content.html");
  }

  // This URL is on app.com, and the page contains a secure iframe that points
  // to foo.com/simple.html.
  GURL GetSecureIFrameAppURL() {
    net::HostPortPair host_port_pair = net::HostPortPair::FromURL(
        https_server()->GetURL("foo.com", "/simple.html"));
    const std::string path = GetPathWithHostAndPortReplaced(
        "/ssl/page_with_cross_site_frame.html", host_port_pair);

    return https_server()->GetURL("app.com", path);
  }
};

class PWAMixedContentBrowserTestWithAutoupgradesDisabled
    : public PWAMixedContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PWAMixedContentBrowserTest::SetUpCommandLine(command_line);
    feature_list.InitAndDisableFeature(
        blink::features::kMixedContentAutoupgrade);
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

// Tests that creating a shortcut app but not installing a PWA is available for
// a non-installable site, unless the universal install feature flag is enabled.
IN_PROC_BROWSER_TEST_F(PWAMixedContentBrowserTest,
                       ShortcutMenuOptionsForNonInstallableSite) {
  EXPECT_FALSE(
      NavigateAndAwaitInstallabilityCheck(browser(), GetMixedContentAppURL()));

  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, browser()), kEnabled);

  AppMenuCommandState expected_command_state =
      base::FeatureList::IsEnabled(features::kWebAppUniversalInstall)
          ? kEnabled
          : kNotPresent;
  EXPECT_EQ(GetAppMenuCommandState(IDC_INSTALL_PWA, browser()),
            expected_command_state);
}

// Tests that mixed content is loaded inside PWA windows.
IN_PROC_BROWSER_TEST_F(PWAMixedContentBrowserTestWithAutoupgradesDisabled,
                       MixedContentInPWA) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL app_url = GetMixedContentAppURL();
  const webapps::AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);
  CHECK(app_browser);
  web_app::CheckMixedContentLoaded(app_browser);
}

// Tests that when calling OpenInChrome, mixed content can be loaded in the new
// tab.
IN_PROC_BROWSER_TEST_F(PWAMixedContentBrowserTestWithAutoupgradesDisabled,
                       MixedContentOpenInChrome) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL app_url = GetMixedContentAppURL();
  const webapps::AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  // Mixed content should be able to load in web app windows.
  CheckMixedContentLoaded(app_browser);

  chrome::OpenInChrome(app_browser);
  ASSERT_EQ(browser(), chrome::FindLastActive());
  ASSERT_EQ(GetMixedContentAppURL(), browser()
                                         ->tab_strip_model()
                                         ->GetActiveWebContents()
                                         ->GetLastCommittedURL());

  // The WebContents is just reparented, so mixed content is still loaded.
  CheckMixedContentLoaded(browser());
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, browser()),
            kEnabled);

  ui_test_utils::UrlLoadObserver url_observer(GetMixedContentAppURL());
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  url_observer.Wait();

  // After reloading, mixed content should successfully load.
  CheckMixedContentLoaded(browser());
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, browser()),
            kEnabled);
  EXPECT_NE(ReparentWebAppForActiveTab(browser()), nullptr);
}

// Tests that when calling ReparentWebContentsIntoAppBrowser, mixed
// content cannot be loaded in the new app window.
IN_PROC_BROWSER_TEST_F(PWAMixedContentBrowserTestWithAutoupgradesDisabled,
                       MixedContentReparentWebContentsIntoAppBrowser) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL app_url = GetMixedContentAppURL();
  const webapps::AppId app_id = InstallPWA(app_url);

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GetMixedContentAppURL()));
  content::WebContents* tab_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(tab_contents->GetLastCommittedURL(), GetMixedContentAppURL());

  // A regular tab should be able to load mixed content.
  CheckMixedContentLoaded(browser());
  EXPECT_EQ(GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, browser()),
            kEnabled);

  Browser* app_browser =
      ReparentWebContentsIntoAppBrowser(tab_contents, app_id);

  ASSERT_NE(app_browser, browser());
  ASSERT_EQ(GetMixedContentAppURL(), app_browser->tab_strip_model()
                                         ->GetActiveWebContents()
                                         ->GetLastCommittedURL());

  // After reparenting, the WebContents should still have its mixed content
  // loaded.
  CheckMixedContentLoaded(app_browser);

  ui_test_utils::UrlLoadObserver url_observer(GetMixedContentAppURL());
  chrome::Reload(app_browser, WindowOpenDisposition::CURRENT_TAB);
  url_observer.Wait();

  // Mixed content should be able to load in web app windows.
  CheckMixedContentLoaded(app_browser);
}

// Tests that mixed content is not loaded inside iframes in PWA windows.
IN_PROC_BROWSER_TEST_F(PWAMixedContentBrowserTest, IFrameMixedContentInPWA) {
  const GURL app_url = GetSecureIFrameAppURL();
  const webapps::AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  CheckMixedContentFailedToLoad(app_browser);
}

// Tests that iframes can't dynamically load mixed content in a PWA window, when
// the iframe was created in a regular tab.
IN_PROC_BROWSER_TEST_F(
    PWAMixedContentBrowserTestWithAutoupgradesDisabled,
    IFrameDynamicMixedContentInPWAReparentWebContentsIntoAppBrowser) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL app_url = GetSecureIFrameAppURL();
  const webapps::AppId app_id = InstallPWA(app_url);

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));
  CheckMixedContentFailedToLoad(browser());

  Browser* const app_browser = ReparentWebContentsIntoAppBrowser(
      browser()->tab_strip_model()->GetActiveWebContents(), app_id);
  CheckMixedContentFailedToLoad(app_browser);

  // Change the mixed content to be acceptable.
  content::RenderFrameHost* main_frame = app_browser->tab_strip_model()
                                             ->GetActiveWebContents()
                                             ->GetPrimaryMainFrame();
  content::RenderFrameHost* iframe = content::ChildFrameAt(main_frame, 0);
  EXPECT_TRUE(TryToLoadImage(
      iframe, embedded_test_server()->GetURL("foo.com", kImagePath)));

  CheckMixedContentLoaded(app_browser);
}

// Tests that iframes can't dynamically load mixed content in a regular browser
// tab, when the iframe was created in a PWA window.
// https://crbug.com/1087382: Flaky on Windows, CrOS and ASAN
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH) || \
    defined(ADDRESS_SANITIZER)
#define MAYBE_IFrameDynamicMixedContentInPWAOpenInChrome \
  DISABLED_IFrameDynamicMixedContentInPWAOpenInChrome
#else
#define MAYBE_IFrameDynamicMixedContentInPWAOpenInChrome \
  IFrameDynamicMixedContentInPWAOpenInChrome
#endif
IN_PROC_BROWSER_TEST_F(PWAMixedContentBrowserTestWithAutoupgradesDisabled,
                       MAYBE_IFrameDynamicMixedContentInPWAOpenInChrome) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL app_url = GetSecureIFrameAppURL();
  const webapps::AppId app_id = InstallPWA(app_url);
  Browser* const app_browser = LaunchWebAppBrowserAndWait(app_id);

  chrome::OpenInChrome(app_browser);

  content::RenderFrameHost* main_frame = browser()
                                             ->tab_strip_model()
                                             ->GetActiveWebContents()
                                             ->GetPrimaryMainFrame();
  content::RenderFrameHost* iframe = content::ChildFrameAt(main_frame, 0);

  EXPECT_TRUE(TryToLoadImage(
      iframe, embedded_test_server()->GetURL("foo.com", kImagePath)));

  CheckMixedContentLoaded(browser());
}

}  // namespace web_app
