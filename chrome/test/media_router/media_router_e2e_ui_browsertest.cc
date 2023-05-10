// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/media_router/media_router_e2e_browsertest.h"

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/media_router/media_router_cast_ui_for_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/test_data_util.h"
#include "net/base/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

IN_PROC_BROWSER_TEST_P(MediaRouterE2EBrowserTest, MANUAL_MirrorHTML5Video) {
  MEDIA_ROUTER_INTEGRATION_BROWER_TEST_CAST_ONLY();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  test_ui_->ShowDialog();

  // Wait until the dialog finishes rendering.
  test_ui_->WaitForSinkAvailable(receiver_);
  test_ui_->StartCasting(receiver_);

  // Mirror tab for 10s.
  Wait(base::Seconds(10));

  // Check the mirroring session has started successfully.
  ASSERT_FALSE(test_ui_->GetRouteIdForSink(receiver_).empty());
  OpenMediaPage();

  // Play the video on loop and wait 5s for it to play smoothly.
  std::string script = "document.getElementsByTagName('video')[0].loop=true;";
  EXPECT_TRUE(ExecJs(web_contents, script));
  Wait(base::Seconds(5));

  // Go to full screen and wait 5s for it to play smoothly.
  script =
      "document.getElementsByTagName('video')[0]."
      "webkitRequestFullScreen();";
  EXPECT_TRUE(ExecJs(web_contents, script));
  Wait(base::Seconds(5));
  if (!test_ui_->IsDialogShown()) {
    test_ui_->ShowDialog();
    // Wait 5s for the dialog to be fully loaded and usable.
    Wait(base::Seconds(5));
  }

  // Check the mirroring session is still live.
  ASSERT_FALSE(test_ui_->GetRouteIdForSink(receiver_).empty());
  Wait(base::Seconds(20));
  if (!test_ui_->IsDialogShown())
    test_ui_->ShowDialog();
  test_ui_->WaitForSink(receiver_);
  test_ui_->StopCasting(receiver_);
  WaitUntilNoRoutes(web_contents);
  test_ui_->HideDialog();
}

}  // namespace media_router
