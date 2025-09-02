// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/dappnet/dappnet_settings_ui.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

class DappnetSettingsUIBrowserTest : public InProcessBrowserTest {
 public:
  DappnetSettingsUIBrowserTest() = default;
  ~DappnetSettingsUIBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(DappnetSettingsUIBrowserTest, LoadDappnetConfigPage) {
  // Navigate to the dappnet settings page
  GURL dappnet_url("chrome://dappnet/config");
  
  // This should not crash and should load the page
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), dappnet_url));
  
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  
  // Wait for the page to load
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  
  // Check that we're on the right page
  EXPECT_EQ(dappnet_url, web_contents->GetLastCommittedURL());
  
  // Check that the page has some expected content
  bool has_title = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "domAutomationController.send(document.title.includes('Dappnet'));",
      &has_title));
  EXPECT_TRUE(has_title);
  
  // Check that the page doesn't have any JavaScript errors
  std::string error_count;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "domAutomationController.send(String(window.onerror ? '1' : '0'));",
      &error_count));
  EXPECT_EQ("0", error_count);
}

IN_PROC_BROWSER_TEST_F(DappnetSettingsUIBrowserTest, PageHasBasicElements) {
  GURL dappnet_url("chrome://dappnet/config");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), dappnet_url));
  
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  
  // Check for expected UI elements
  bool has_rpc_section = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "domAutomationController.send(document.body.textContent.includes('RPC'));",
      &has_rpc_section));
  EXPECT_TRUE(has_rpc_section);
  
  bool has_gateway_section = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "domAutomationController.send(document.body.textContent.includes('Gateway'));",
      &has_gateway_section));
  EXPECT_TRUE(has_gateway_section);
}