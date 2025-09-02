// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

class DappnetSettingsBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL("chrome://dappnet/config")));
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

IN_PROC_BROWSER_TEST_F(DappnetSettingsBrowserTest, PageLoads) {
  content::WebContents* web_contents = GetWebContents();
  
  EXPECT_EQ(GURL("chrome://dappnet/config"), web_contents->GetURL());
  
  // Check that the page title is correct
  std::string title;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "domAutomationController.send(document.title)",
      &title));
  EXPECT_EQ("Dappnet Settings", title);
}

IN_PROC_BROWSER_TEST_F(DappnetSettingsBrowserTest, PageStructure) {
  content::WebContents* web_contents = GetWebContents();
  
  // Check that main sections are present
  bool has_rpc_section;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "domAutomationController.send("
      "  !!document.getElementById('rpc-section')"
      ")",
      &has_rpc_section));
  EXPECT_TRUE(has_rpc_section);
  
  bool has_gateway_section;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "domAutomationController.send("
      "  !!document.getElementById('gateway-section')"
      ")",
      &has_gateway_section));
  EXPECT_TRUE(has_gateway_section);
  
  bool has_ipfs_section;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "domAutomationController.send("
      "  !!document.getElementById('ipfs-section')"
      ")",
      &has_ipfs_section));
  EXPECT_TRUE(has_ipfs_section);
}

IN_PROC_BROWSER_TEST_F(DappnetSettingsBrowserTest, AddRpcModalOpens) {
  content::WebContents* web_contents = GetWebContents();
  
  // Click the add RPC button
  EXPECT_TRUE(content::ExecJs(
      web_contents,
      "document.getElementById('add-rpc-btn').click()"));
  
  // Check that the modal is now visible
  bool modal_visible;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "domAutomationController.send("
      "  document.getElementById('add-rpc-modal').classList.contains('show')"
      ")",
      &modal_visible));
  EXPECT_TRUE(modal_visible);
}

IN_PROC_BROWSER_TEST_F(DappnetSettingsBrowserTest, ApiObjectExists) {
  content::WebContents* web_contents = GetWebContents();
  
  // Wait for the page to fully load
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  
  // Check that the DappnetSettingsApi is available
  bool api_exists;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "domAutomationController.send("
      "  typeof window.DappnetSettingsApi !== 'undefined'"
      ")",
      &api_exists));
  EXPECT_TRUE(api_exists);
}