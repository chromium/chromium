// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_metadata/window_metadata_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

using WindowMetadataControllerBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(WindowMetadataControllerBrowserTest,
                       UserTitleEmptyByDefault) {
  WindowMetadataController* controller =
      WindowMetadataController::From(browser());
  ASSERT_TRUE(controller);
  EXPECT_TRUE(controller->user_title().empty());
}

IN_PROC_BROWSER_TEST_F(WindowMetadataControllerBrowserTest,
                       SetAndGetUserTitle) {
  WindowMetadataController* controller =
      WindowMetadataController::From(browser());
  ASSERT_TRUE(controller);

  controller->SetWindowUserTitle("My Window");
  EXPECT_EQ("My Window", controller->user_title());
}

IN_PROC_BROWSER_TEST_F(WindowMetadataControllerBrowserTest,
                       ClearUserTitleWithEmptyString) {
  WindowMetadataController* controller =
      WindowMetadataController::From(browser());
  ASSERT_TRUE(controller);

  controller->SetWindowUserTitle("My Window");
  EXPECT_EQ("My Window", controller->user_title());

  controller->SetWindowUserTitle("");
  EXPECT_TRUE(controller->user_title().empty());
}

IN_PROC_BROWSER_TEST_F(WindowMetadataControllerBrowserTest,
                       GetWindowTitleForCurrentTabWithUserTitle) {
  WindowMetadataController* controller =
      WindowMetadataController::From(browser());
  ASSERT_TRUE(controller);

  controller->SetWindowUserTitle("Custom Title");
  EXPECT_EQ(u"Custom Title",
            controller->GetWindowTitleForCurrentTab(/*include_app_name=*/true));
  EXPECT_EQ(u"Custom Title", controller->GetWindowTitleForCurrentTab(
                                 /*include_app_name=*/false));
}

IN_PROC_BROWSER_TEST_F(WindowMetadataControllerBrowserTest,
                       GetWindowTitleForCurrentTabDefaultsToPageTitle) {
  WindowMetadataController* controller =
      WindowMetadataController::From(browser());
  ASSERT_TRUE(controller);

  // Without a user title set, the window title should include something
  // (either the page title or default title). It should not be empty.
  std::u16string title =
      controller->GetWindowTitleForCurrentTab(/*include_app_name=*/true);
  EXPECT_FALSE(title.empty());
}

IN_PROC_BROWSER_TEST_F(WindowMetadataControllerBrowserTest,
                       BrowserDelegatesMatchController) {
  // Verify that the Browser wrapper methods delegate to the controller.
  WindowMetadataController* controller =
      WindowMetadataController::From(browser());
  ASSERT_TRUE(controller);

  browser()->SetWindowUserTitle("Delegate Test");
  EXPECT_EQ("Delegate Test", controller->user_title());
  EXPECT_EQ("Delegate Test", browser()->user_title());
}
