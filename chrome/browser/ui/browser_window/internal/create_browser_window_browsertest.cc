// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/create_browser_window.h"

#include "base/functional/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using CreateBrowserWindowBrowserTest = InProcessBrowserTest;

// Smoke test for the synchronous version of CreateBrowserWindow(). Since
// CreateBrowserWindow() really just forwards to Browser::Create(), we don't
// exercise it in much depth yet.
IN_PROC_BROWSER_TEST_F(CreateBrowserWindowBrowserTest, CreateNewBrowserWindow) {
  BrowserWindowInterface* new_browser =
      CreateBrowserWindow(BrowserWindowCreateParams(
          *browser()->GetProfile(), /*from_user_gesture=*/false));

  ASSERT_TRUE(new_browser);
  ASSERT_NE(new_browser, browser());
  EXPECT_EQ(new_browser->GetProfile(), browser()->GetProfile());
  EXPECT_EQ(BrowserWindowInterface::TYPE_NORMAL, new_browser->GetType());
}

// Smoke test for the asynchronous version of CreateBrowserWindow().
IN_PROC_BROWSER_TEST_F(CreateBrowserWindowBrowserTest,
                       CreateNewBrowserWindowWithCallback) {
  BrowserWindowCreateParams create_params(*browser()->GetProfile(),
                                          /*from_user_gesture=*/false);

  base::test::TestFuture<BrowserWindowInterface*> future;
  CreateBrowserWindow(std::move(create_params), future.GetCallback());
  BrowserWindowInterface* new_browser = future.Get();

  ASSERT_TRUE(new_browser);
  ASSERT_NE(new_browser, browser());
  EXPECT_EQ(new_browser->GetProfile(), browser()->GetProfile());
  EXPECT_EQ(BrowserWindowInterface::TYPE_NORMAL, new_browser->GetType());
}

IN_PROC_BROWSER_TEST_F(CreateBrowserWindowBrowserTest,
                       CreateAppBrowserWindowAndClone) {
  BrowserWindowCreateParams create_params =
      BrowserWindowCreateParams::CreateForApp(
          "TestApp", /*trusted_source=*/true, gfx::Rect(0, 0, 800, 600),
          browser()->GetProfile(), /*user_gesture=*/true);
  create_params.omit_from_session_restore = true;

  BrowserWindowCreateParams cloned_params = create_params.Clone();
  EXPECT_EQ(create_params.app_name, cloned_params.app_name);
  EXPECT_EQ(create_params.is_trusted_source, cloned_params.is_trusted_source);
  EXPECT_EQ(create_params.initial_bounds, cloned_params.initial_bounds);
  EXPECT_EQ(create_params.omit_from_session_restore,
            cloned_params.omit_from_session_restore);

  BrowserWindowInterface* app_browser =
      CreateBrowserWindow(std::move(create_params));
  ASSERT_TRUE(app_browser);
  EXPECT_EQ(BrowserWindowInterface::TYPE_APP, app_browser->GetType());
}

IN_PROC_BROWSER_TEST_F(CreateBrowserWindowBrowserTest,
                       CreateDevToolsBrowserWindow) {
  BrowserWindowCreateParams create_params =
      BrowserWindowCreateParams::CreateForDevTools(browser()->GetProfile());

  BrowserWindowInterface* devtools_browser =
      CreateBrowserWindow(std::move(create_params));
  ASSERT_TRUE(devtools_browser);
  EXPECT_EQ(BrowserWindowInterface::TYPE_DEVTOOLS, devtools_browser->GetType());
}
