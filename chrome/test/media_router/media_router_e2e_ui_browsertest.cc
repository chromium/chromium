// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/media_router/media_router_e2e_browsertest.h"

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/test_data_util.h"
#include "net/base/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

// TODO(crbug.com/903016) Disabled due to flakiness.
IN_PROC_BROWSER_TEST_F(MediaRouterE2EBrowserTest,
                       DISABLED_OpenLocalMediaFileFullscreen) {
  GURL file_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("media/bigbuck.webm")));

  // Start at a new tab, the file should open in the same tab.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  // Make sure there is 1 tab.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  test_ui_->ShowDialog();
  test_ui_->WaitForSinkAvailable(receiver_);

  // Mock out file dialog operations, as those can't be simulated.
  test_ui_->SetLocalFile(file_url);
  // Click on the desired mode.
  test_ui_->ChooseSourceType(CastDialogView::kLocalFile);
  test_ui_->WaitForSinkAvailable(receiver_);
  test_ui_->StartCasting(receiver_);

  // Play the file for 10 seconds.
  Wait(base::TimeDelta::FromSeconds(10));

  // Expect that the current tab has the file open in it.
  ASSERT_EQ(file_url, web_contents->GetURL());

  // Expect that fullscreen is active.
  bool is_fullscreen = false;
  std::string is_fullscreen_script =
      "domAutomationController.send"
      "(document.webkitCurrentFullScreenElement != null);";
  CHECK(content::ExecuteScriptAndExtractBool(web_contents, is_fullscreen_script,
                                             &is_fullscreen));

  ASSERT_TRUE(is_fullscreen);
  test_ui_->WaitForSink(receiver_);
  test_ui_->StopCasting(receiver_);
  // Wait 15s for Chromecast to back to home screen and ready to use status.
  Wait(base::TimeDelta::FromSeconds(15));
}

IN_PROC_BROWSER_TEST_F(MediaRouterE2EBrowserTest, MANUAL_MirrorHTML5Video) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  test_ui_ = MediaRouterUiForTest::GetOrCreateForWebContents(web_contents);
  test_ui_->ShowDialog();

  // Wait until the dialog finishes rendering.
  test_ui_->WaitForSinkAvailable(receiver_);
  test_ui_->StartCasting(receiver_);

  // Mirror tab for 10s.
  Wait(base::TimeDelta::FromSeconds(10));

  // Check the mirroring session has started successfully.
  ASSERT_FALSE(test_ui_->GetRouteIdForSink(receiver_).empty());
  OpenMediaPage();

  // Play the video on loop and wait 5s for it to play smoothly.
  std::string script = "document.getElementsByTagName('video')[0].loop=true;";
  ExecuteScript(web_contents, script);
  Wait(base::TimeDelta::FromSeconds(5));

  // Go to full screen and wait 5s for it to play smoothly.
  script =
      "document.getElementsByTagName('video')[0]."
      "webkitRequestFullScreen();";
  ExecuteScript(web_contents, script);
  Wait(base::TimeDelta::FromSeconds(5));
  if (!test_ui_->IsDialogShown()) {
    test_ui_->ShowDialog();
    // Wait 5s for the dialog to be fully loaded and usable.
    Wait(base::TimeDelta::FromSeconds(5));
  }

  // Check the mirroring session is still live.
  ASSERT_FALSE(test_ui_->GetRouteIdForSink(receiver_).empty());
  Wait(base::TimeDelta::FromSeconds(20));
  if (!test_ui_->IsDialogShown())
    test_ui_->ShowDialog();
  test_ui_->WaitForSink(receiver_);
  test_ui_->StopCasting(receiver_);
  test_ui_->WaitUntilNoRoutes();
  test_ui_->HideDialog();
}

}  // namespace media_router
