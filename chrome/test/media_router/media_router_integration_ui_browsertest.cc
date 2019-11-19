// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/media_router/media_router_integration_browsertest.h"

#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace media_router {

// TODO(https://crbug.com/822231): Flaky in Chromium waterfall.
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest, MANUAL_Dialog_Basic) {
  OpenTestPage(FILE_PATH_LITERAL("basic_test.html"));
  test_ui_->ShowDialog();
  test_ui_->WaitForSinkAvailable(receiver_);
  test_ui_->StartCasting(receiver_);
  test_ui_->WaitForAnyRoute();

  if (!test_ui_->IsDialogShown())
    test_ui_->ShowDialog();

  ASSERT_EQ("Test Route", test_ui_->GetStatusTextForSink(receiver_));

  test_ui_->StopCasting(receiver_);
  test_ui_->WaitUntilNoRoutes();
  // TODO(takumif): Remove the HideDialog() call once the dialog can close on
  // its own.
  test_ui_->HideDialog();
}

// TODO(https://crbug.com/822231): Flaky in Chromium waterfall.
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       MANUAL_Dialog_RouteCreationTimedOut) {
  SetTestData(FILE_PATH_LITERAL("route_creation_timed_out.json"));
  OpenTestPage(FILE_PATH_LITERAL("basic_test.html"));
  test_ui_->ShowDialog();
  test_ui_->WaitForSinkAvailable(receiver_);

  base::TimeTicks start_time(base::TimeTicks::Now());
  test_ui_->StartCasting(receiver_);
  test_ui_->WaitForAnyIssue();

  base::TimeDelta elapsed(base::TimeTicks::Now() - start_time);
  // The hardcoded timeout route creation timeout for the UI.
  // See kCreateRouteTimeoutSeconds in media_router_ui.cc.
  base::TimeDelta expected_timeout(base::TimeDelta::FromSeconds(20));

  EXPECT_GE(elapsed, expected_timeout);
  EXPECT_LE(elapsed - expected_timeout, base::TimeDelta::FromSeconds(5));

  std::string issue_title = test_ui_->GetIssueTextForSink(receiver_);
  // TODO(imcheng): Fix host name for file schemes (crbug.com/560576).
  ASSERT_EQ(
      l10n_util::GetStringFUTF8(IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT,
                                base::UTF8ToUTF16("file:///")),
      issue_title);

  // Route will still get created, it just takes longer than usual.
  test_ui_->WaitForAnyRoute();
  test_ui_->HideDialog();
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       PRE_OpenDialogAfterEnablingMediaRouting) {
  SetEnableMediaRouter(false);
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       OpenDialogAfterEnablingMediaRouting) {
  // Enable media routing and open media router dialog.
  SetEnableMediaRouter(true);
  OpenTestPage(FILE_PATH_LITERAL("basic_test.html"));
  test_ui_->ShowDialog();
  ASSERT_TRUE(test_ui_->IsDialogShown());
  test_ui_->HideDialog();
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest,
                       DisableMediaRoutingWhenDialogIsOpened) {
  // Open media router dialog.
  OpenTestPage(FILE_PATH_LITERAL("basic_test.html"));
  test_ui_->ShowDialog();
  ASSERT_TRUE(test_ui_->IsDialogShown());

  // Disable media routing.
  SetEnableMediaRouter(false);

  ASSERT_FALSE(test_ui_->IsDialogShown());
}

}  // namespace media_router
