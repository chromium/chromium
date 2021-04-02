// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share_dialog_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

namespace {

class TestSharesheetController : public sharesheet::SharesheetController {
 public:
  // sharesheet::SharesheetController
  Profile* GetProfile() override { return nullptr; }
  void SetSharesheetSize(const int& width, const int& height) override {}
  void CloseSharesheet() override { close_called = true; }

  bool close_called = false;
};

class NearbyShareDialogUITest : public InProcessBrowserTest {
 public:
  NearbyShareDialogUITest() {
    scoped_feature_list_.InitWithFeatures({features::kNearbySharing}, {});
  }
  ~NearbyShareDialogUITest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestSharesheetController sharesheet_controller_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(NearbyShareDialogUITest, RendersComponent) {
  // First, check that navigation succeeds.
  GURL kUrl(content::GetWebUIURL(chrome::kChromeUINearbyShareHost));
  ui_test_utils::NavigateToURL(browser(), kUrl);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_EQ(kUrl, web_contents->GetLastCommittedURL());
  EXPECT_FALSE(web_contents->IsCrashed());

  // Assert that we render the nearby-share-app component.
  int num_nearby_share_app = -1;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      web_contents,
      "domAutomationController.send("
      "document.getElementsByTagName('nearby-share-app').length)",
      &num_nearby_share_app));
  EXPECT_EQ(1, num_nearby_share_app);
}

IN_PROC_BROWSER_TEST_F(NearbyShareDialogUITest,
                       SharesheetControllerGetsCalledOnClose) {
  // First, check that navigation succeeds.
  GURL kUrl(content::GetWebUIURL(chrome::kChromeUINearbyShareHost));
  ui_test_utils::NavigateToURL(browser(), kUrl);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_EQ(kUrl, web_contents->GetLastCommittedURL());
  EXPECT_FALSE(web_contents->IsCrashed());

  auto* webui = web_contents->GetWebUI();
  ASSERT_TRUE(webui);

  // Add a test observer and verify it gets called when 'close' is sent.
  auto* nearby_ui =
      webui->GetController()->GetAs<nearby_share::NearbyShareDialogUI>();
  ASSERT_TRUE(nearby_ui);

  // Calling 'close' before a Sharesheet controller is registered via
  // |SetSharesheetController| does not result in a crash.
  EXPECT_TRUE(content::ExecuteScript(web_contents, "chrome.send('close');"));
  EXPECT_FALSE(sharesheet_controller_.close_called);

  // The Sharesheet controller gets called on 'close' if it's been registered.
  nearby_ui->SetSharesheetController(&sharesheet_controller_);
  EXPECT_TRUE(content::ExecuteScript(web_contents, "chrome.send('close');"));
  EXPECT_TRUE(sharesheet_controller_.close_called);

  // Any subsequent calls to 'close' do not call the Sharesheet controller,
  // since that would result in a crash.
  sharesheet_controller_.close_called = false;
  EXPECT_TRUE(content::ExecuteScript(web_contents, "chrome.send('close');"));
  EXPECT_FALSE(sharesheet_controller_.close_called);
}
