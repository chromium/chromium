// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/devtools/device/adb/adb_device_provider.h"
#include "chrome/browser/devtools/device/adb/mock_adb_server.h"
#include "chrome/browser/devtools/device/devtools_android_bridge.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_browser_test.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::WebContents;

namespace {

const char kSharedWorkerTestPage[] = "/workers/workers_ui_shared_worker.html";
const char kSharedWorkerJs[] = "/workers/workers_ui_shared_worker.js";

class InspectUITest : public WebUIBrowserTest {
 public:
  InspectUITest() {}

  InspectUITest(const InspectUITest&) = delete;
  InspectUITest& operator=(const InspectUITest&) = delete;

  void SetUpOnMainThread() override {
    WebUIBrowserTest::SetUpOnMainThread();
    AddLibrary(base::FilePath(FILE_PATH_LITERAL("inspect_ui_test.js")));
  }
};

IN_PROC_BROWSER_TEST_F(InspectUITest, InspectUIPage) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIInspectURL)));
  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testTargetListed", base::Value("#pages"),
      base::Value("populateWebContentsTargets"),
      base::Value(chrome::kChromeUIInspectURL)));
}

IN_PROC_BROWSER_TEST_F(InspectUITest, SharedWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(kSharedWorkerTestPage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUIInspectURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testTargetListed", base::Value("#workers"),
      base::Value("populateWorkerTargets"), base::Value(kSharedWorkerJs)));

  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testTargetListed", base::Value("#pages"),
      base::Value("populateWebContentsTargets"),
      base::Value(kSharedWorkerTestPage)));
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

  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest("testAdbTargetsListed"));

  StopMockAdbServer();
}

IN_PROC_BROWSER_TEST_F(InspectUITest, ReloadCrash) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIInspectURL)));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIInspectURL)));
}

}  // namespace
