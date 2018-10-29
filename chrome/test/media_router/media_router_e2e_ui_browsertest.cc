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

IN_PROC_BROWSER_TEST_F(MediaRouterE2EBrowserTest,
                       MANUAL_OpenLocalMediaFileFullscreen) {
  GURL file_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("media/bigbuck.webm")));

  // Start at a new tab, the file should open in the same tab.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  // Make sure there is 1 tab.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::WebContents* dialog_contents = OpenMRDialog(web_contents);
  ASSERT_TRUE(dialog_contents);
  // Wait util the dialog finishes rendering.
  WaitUntilDialogFullyLoaded(dialog_contents);

  // Get the media router UI
  MediaRouterUI* media_router_ui = GetMediaRouterUI(dialog_contents);

  // Mock out file dialog operations, as those can't be simulated.
  FileDialogSelectsFile(media_router_ui, file_url);
  // Open the Cast mode list.
  ClickHeader(dialog_contents);
  // Click on the desired mode.
  ClickCastMode(dialog_contents, MediaCastMode::LOCAL_FILE);
  WaitUntilSinkDiscoveredOnUI();
  ChooseSink(web_contents, receiver());

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
  if (IsDialogClosed(web_contents))
    OpenMRDialog(web_contents);
  CloseRouteOnUI();
  // Wait 15s for Chromecast to back to home screen and ready to use status.
  Wait(base::TimeDelta::FromSeconds(15));
}

IN_PROC_BROWSER_TEST_F(MediaRouterE2EBrowserTest, MANUAL_MirrorHTML5Video) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::WebContents* dialog_contents = OpenMRDialog(web_contents);
  ASSERT_TRUE(dialog_contents);

  // Wait until the dialog finishes rendering.
  WaitUntilDialogFullyLoaded(dialog_contents);
  WaitUntilSinkDiscoveredOnUI();
  ChooseSink(web_contents, receiver());

  // Mirror tab for 10s.
  Wait(base::TimeDelta::FromSeconds(10));
  if (IsDialogClosed(web_contents))
    dialog_contents = OpenMRDialog(web_contents);
  WaitUntilDialogFullyLoaded(dialog_contents);

  // Check the mirroring session has started successfully.
  ASSERT_TRUE(!GetRouteId(receiver()).empty());
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
  if (IsDialogClosed(web_contents))
    dialog_contents = OpenMRDialog(web_contents);
  WaitUntilDialogFullyLoaded(dialog_contents);

  // Check the mirroring session is still live.
  ASSERT_TRUE(!GetRouteId(receiver()).empty());
  Wait(base::TimeDelta::FromSeconds(20));
  if (IsDialogClosed(web_contents))
    OpenMRDialog(web_contents);
  CloseRouteOnUI();
}

}  // namespace media_router
