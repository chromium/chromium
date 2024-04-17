// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/adb/adb_device_provider.h"
#include "chrome/browser/devtools/device/adb/mock_adb_server.h"
#include "chrome/browser/devtools/device/devtools_android_bridge.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/scoped_privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"

using content::WebContents;

namespace {

const char kSharedWorkerTestPage[] = "/workers/workers_ui_shared_worker.html";

class InspectUITest : public WebUIMochaBrowserTest {
 public:
  InspectUITest() {}

  InspectUITest(const InspectUITest&) = delete;
  InspectUITest& operator=(const InspectUITest&) = delete;

  testing::AssertionResult RunTestCase(const std::string& testCase) {
    return RunTestOnWebContents(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "inspect/inspect_ui_test.js",
        base::StringPrintf("runMochaTest('InspectUITest', '%s');",
                           testCase.c_str()),
        /* skip_test_loader= */ true);
  }

  content::WebContents* LaunchUIDevtools(content::WebUI* web_ui) {
    content::TestNavigationObserver new_tab_observer(nullptr);
    new_tab_observer.StartWatchingNewWebContents();

    // Fake clicking the "Inspect Native UI" button.
    web_ui->ProcessWebUIMessage(GURL(), "launch-ui-devtools",
                                base::Value::List());

    new_tab_observer.Wait();
    EXPECT_EQ(2, browser()->tab_strip_model()->count());

    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

IN_PROC_BROWSER_TEST_F(InspectUITest, InspectUIPage) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIInspectURL)));
  ASSERT_TRUE(RunTestCase("InspectUIPage"));
}

IN_PROC_BROWSER_TEST_F(InspectUITest, SharedWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(kSharedWorkerTestPage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUIInspectURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ASSERT_TRUE(RunTestCase("SharedWorker"));
}

// Flaky due to failure to bind a hardcoded port. crbug.com/566057
IN_PROC_BROWSER_TEST_F(InspectUITest, DISABLED_AndroidTargets) {
  DevToolsAndroidBridge* android_bridge =
      DevToolsAndroidBridge::Factory::GetForProfile(browser()->profile());
  AndroidDeviceManager::DeviceProviders providers;
  providers.push_back(new AdbDeviceProvider());
  android_bridge->set_device_providers_for_test(providers);

  StartMockAdbServer(FlushWithSize);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIInspectURL)));

  ASSERT_TRUE(RunTestCase("AdbTargetsListed"));

  StopMockAdbServer();
}

IN_PROC_BROWSER_TEST_F(InspectUITest, ReloadCrash) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIInspectURL)));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIInspectURL)));
}

// Disabled due to excessive flakiness. http://crbug.com/1304812
#if BUILDFLAG(IS_MAC)
#define MAYBE_LaunchUIDevtools DISABLED_LaunchUIDevtools
#else
#define MAYBE_LaunchUIDevtools LaunchUIDevtools
#endif
IN_PROC_BROWSER_TEST_F(InspectUITest, MAYBE_LaunchUIDevtools) {
  ASSERT_TRUE(embedded_test_server()->Start());
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIInspectURL)));
  content::WebContents* inspect_ui_contents =
      tab_strip_model->GetActiveWebContents();
  const int inspect_ui_tab_idx = tab_strip_model->active_index();

  content::WebContents* front_end_tab =
      LaunchUIDevtools(tab_strip_model->GetActiveWebContents()->GetWebUI());

  tab_strip_model->ActivateTabAt(inspect_ui_tab_idx);

  // Run an empty test, to load the mocha test file on the page.
  ASSERT_TRUE(RunTestCase("Empty"));
  // Ensure that "Inspect Native UI" button is disabled.
  ASSERT_TRUE(ExecJs(inspect_ui_contents->GetPrimaryMainFrame(),
                     "assertNativeUIButtonDisabled(true);"));

  // Navigate away from the front-end page.
  ASSERT_TRUE(NavigateToURL(front_end_tab,
                            embedded_test_server()->GetURL("/title1.html")));

  // Ensure that "Inspect Native UI" button is enabled.
  ASSERT_TRUE(ExecJs(inspect_ui_contents->GetPrimaryMainFrame(),
                     "assertNativeUIButtonDisabled(false);"));
}

class InspectUISharedStorageTest : public InspectUITest {
 public:
  InspectUISharedStorageTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kSharedStorageAPI,
                              features::kPrivacySandboxAdsAPIsOverride,
                              privacy_sandbox::
                                  kOverridePrivacySandboxSettingsLocalTesting},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    content::SetupCrossSiteRedirector(&https_server_);

    ASSERT_TRUE(https_server_.Start());

    InspectUITest::SetUpOnMainThread();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(InspectUISharedStorageTest, SharedStorageWorklet) {
  privacy_sandbox::ScopedPrivacySandboxAttestations scoped_attestations(
      privacy_sandbox::PrivacySandboxAttestations::CreateForTesting());
  // Mark all Privacy Sandbox APIs as attested since the test case is testing
  // behaviors not related to attestations.
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()
      ->SetAllPrivacySandboxAttestedForTesting(true);

  GURL main_frame_url = https_server_.GetURL("a.test", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  const char kModuleScriptFile[] = "/shared_storage/simple_module.js";

  GURL module_script_url = https_server_.GetURL("a.test", kModuleScriptFile);
  EXPECT_TRUE(
      content::ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      content::JsReplace("sharedStorage.worklet.addModule($1)",
                                         module_script_url)));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUIInspectURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ASSERT_TRUE(RunTestCase("SharedStorageWorklet"));
}

class InspectUIFencedFrameTest : public InspectUITest {
 public:
  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 protected:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

// TODO(crbug.com/40227465): Re-enable this test
IN_PROC_BROWSER_TEST_F(InspectUIFencedFrameTest,
                       DISABLED_FencedFrameInFrontEnd) {
  ASSERT_TRUE(embedded_test_server()->Start());
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIInspectURL)));

  const int inspect_ui_tab_idx = tab_strip_model->active_index();
  content::WebContents* inspect_ui_contents =
      tab_strip_model->GetActiveWebContents();

  content::WebContents* front_end_tab =
      LaunchUIDevtools(tab_strip_model->GetActiveWebContents()->GetWebUI());

  tab_strip_model->ActivateTabAt(inspect_ui_tab_idx);

  // Run an empty test, to load the mocha test file on the page.
  ASSERT_TRUE(RunTestCase("Empty"));
  // Ensure that "Inspect Native UI" button is disabled.
  ASSERT_TRUE(ExecJs(inspect_ui_contents->GetPrimaryMainFrame(),
                     "assertNativeUIButtonDisabled(true);"));

  // Create a fenced frame into the front-end page.
  const GURL fenced_frame_url =
      embedded_test_server()->GetURL("/fenced_frames/title1.html");
  ASSERT_TRUE(fenced_frame_test_helper().CreateFencedFrame(
      front_end_tab->GetPrimaryMainFrame(), fenced_frame_url));

  // Ensure that the fenced frame doesn't affect to the the front-end observer.
  // "Inspect Native UI" button is still disabled.
  ASSERT_TRUE(ExecJs(inspect_ui_contents->GetPrimaryMainFrame(),
                     "assertNativeUIButtonDisabled(true);"));
}

}  // namespace
