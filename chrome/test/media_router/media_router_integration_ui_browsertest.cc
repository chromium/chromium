// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/media_router/media_router_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/media_router/media_router_integration_browsertest.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace media_router {

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest, MANUAL_Dialog_Basic) {
  OpenTestPage(FILE_PATH_LITERAL("basic_test.html"));

  // The video needs to be playing before the GMC button will show up.
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ExecuteJavaScriptAPI(web_contents, "playVideo();");

  test_ui_->ShowDialog();
  test_ui_->WaitForSinkAvailable(receiver_);

  // This is expected to close the dialog.
  test_ui_->StartCasting(receiver_);
  test_ui_->WaitForAnyRoute();

  EXPECT_FALSE(test_ui_->IsDialogShown());
  test_ui_->ShowDialog();
  test_ui_->WaitForSinkAvailable(receiver_);

  // NOTE: There is no status or issue text for the GMC button.
  // TODO(issuetracker.google.com/388289776): re-enable once the GMC test
  // UI supports stopping a cast session.
  // test_ui_->StopCasting(receiver_);
  // WaitUntilNoRoutes(GetActiveWebContents());
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       MANUAL_Dialog_RouteCreationTimedOut) {
  const base::TimeDelta route_creation_timeout =
      GetRouteRequestTimeout(PRESENTATION);
  test_provider_->set_delay(route_creation_timeout);
  OpenTestPage(FILE_PATH_LITERAL("basic_test.html"));

  // The video needs to be playing before the GMC button will show up.
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ExecuteJavaScriptAPI(web_contents, "playVideo();");

  test_ui_->ShowDialog();
  test_ui_->WaitForSinkAvailable(receiver_);

  const auto start_time = base::TimeTicks::Now();
  test_ui_->StartCasting(receiver_);

  // TODO(issuetracker.google.com/388289776): once we can observe the dialog
  // state, we should be able to actually wait for the timeout and then
  // ensure it took a reasonable amount of time to fail.
  test_ui_->WaitForAnyIssue();

  const base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
  EXPECT_GT(elapsed, base::TimeDelta::Min());
}

}  // namespace media_router
