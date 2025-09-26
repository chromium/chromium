// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/create_browser_window.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using CreateBrowserWindowAndroidBrowserTest = AndroidBrowserTest;

IN_PROC_BROWSER_TEST_F(CreateBrowserWindowAndroidBrowserTest,
                       CreateBrowserWindowReturnsBrowserWindowInterface) {
  Profile* profile = GetProfile();
  BrowserWindowCreateParams create_params =
      BrowserWindowCreateParams(BrowserWindowInterface::Type::TYPE_NORMAL,
                                *profile, /*from_user_gesture=*/false);

  BrowserWindowInterface* new_browser_window =
      CreateBrowserWindow(std::move(create_params));

  // TODO(http://crbug.com/444744965): verify the new browser window.
  //
  // For comprehensive testing, you may need to downcast the new browser window
  // to AndroidBrowserWindow and use APIs not available in
  // BrowserWindowInterface, such as AndroidBrowserWindow::GetActivity().
  ASSERT_EQ(new_browser_window, nullptr);
}
