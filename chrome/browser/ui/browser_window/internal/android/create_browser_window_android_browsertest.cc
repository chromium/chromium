// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/create_browser_window.h"

#include <utility>

#include "base/command_line.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_list_interface.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class CreateBrowserWindowAndroidBrowserTest : public AndroidBrowserTest {
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    AndroidBrowserTest::SetUpDefaultCommandLine(command_line);

    // Disable the first-run experience (FRE) so that when a function under
    // test launches an Intent for ChromeTabbedActivity, ChromeTabbedActivity
    // will be shown instead of FirstRunActivity.
    command_line->AppendSwitch("disable-fre");
  }
};

IN_PROC_BROWSER_TEST_F(
    CreateBrowserWindowAndroidBrowserTest,
    CreateBrowserWindowReturnsBrowserWindowInterfaceForSupportedWindowType) {
  Profile* profile = GetProfile();
  BrowserWindowCreateParams create_params =
      BrowserWindowCreateParams(BrowserWindowInterface::Type::TYPE_NORMAL,
                                *profile, /*from_user_gesture=*/false);

  BrowserWindowInterface* new_browser_window =
      CreateBrowserWindow(std::move(create_params));

  ASSERT_NE(new_browser_window, nullptr);
  EXPECT_EQ(new_browser_window->GetType(),
            BrowserWindowInterface::Type::TYPE_NORMAL);
  EXPECT_EQ(new_browser_window->GetProfile(), profile);
}

IN_PROC_BROWSER_TEST_F(CreateBrowserWindowAndroidBrowserTest,
                       CreateBrowserWindowReturnsNullForUnsupportedWindowType) {
  Profile* profile = GetProfile();
  BrowserWindowCreateParams create_params = BrowserWindowCreateParams(
      BrowserWindowInterface::Type::TYPE_APP /* not supported on Android */,
      *profile, /*from_user_gesture=*/false);

  BrowserWindowInterface* new_browser_window =
      CreateBrowserWindow(std::move(create_params));

  EXPECT_EQ(new_browser_window, nullptr);
}

IN_PROC_BROWSER_TEST_F(
    CreateBrowserWindowAndroidBrowserTest,
    CreateBrowserWindowAsyncTriggersCallbackWithBrowserWindowInterfaceForSupportedWindowType) {
  Profile* profile = GetProfile();
  BrowserWindowCreateParams create_params =
      BrowserWindowCreateParams(BrowserWindowInterface::Type::TYPE_NORMAL,
                                *profile, /*from_user_gesture=*/false);

  base::test::TestFuture<BrowserWindowInterface*> future;
  CreateBrowserWindow(std::move(create_params), future.GetCallback());
  BrowserWindowInterface* new_browser_window = future.Get();

  ASSERT_NE(new_browser_window, nullptr);
  EXPECT_EQ(new_browser_window->GetType(),
            BrowserWindowInterface::Type::TYPE_NORMAL);
  EXPECT_EQ(new_browser_window->GetProfile(), profile);
}

IN_PROC_BROWSER_TEST_F(
    CreateBrowserWindowAndroidBrowserTest,
    CreateBrowserWindowAsyncTriggersCallbackWithNullForUnsupportedWindowType) {
  Profile* profile = GetProfile();
  BrowserWindowCreateParams create_params = BrowserWindowCreateParams(
      BrowserWindowInterface::Type::TYPE_APP /* not supported on Android */,
      *profile, /*from_user_gesture=*/false);

  base::test::TestFuture<BrowserWindowInterface*> future;
  CreateBrowserWindow(std::move(create_params), future.GetCallback());
  BrowserWindowInterface* new_browser_window = future.Get();

  EXPECT_EQ(new_browser_window, nullptr);
}

IN_PROC_BROWSER_TEST_F(
    CreateBrowserWindowAndroidBrowserTest,
    CreateBrowserWindowAsyncAssociatesTabModelWithBrowserWindow) {
  Profile* profile = GetProfile();
  BrowserWindowCreateParams create_params =
      BrowserWindowCreateParams(BrowserWindowInterface::Type::TYPE_NORMAL,
                                *profile, /*from_user_gesture=*/false);

  base::test::TestFuture<BrowserWindowInterface*> future;
  CreateBrowserWindow(std::move(create_params), future.GetCallback());
  BrowserWindowInterface* new_browser_window = future.Get();

  auto* tab_list_interface = TabListInterface::From(new_browser_window);
  ASSERT_NE(tab_list_interface, nullptr);
}
