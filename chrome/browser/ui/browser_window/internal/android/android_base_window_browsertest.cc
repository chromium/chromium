// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/internal/android/android_base_window.h"

#include <optional>

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"
#include "ui/base/base_window.h"
#include "ui/gfx/geometry/rect.h"

using AndroidBaseWindowBrowserTest = AndroidBrowserTest;

IN_PROC_BROWSER_TEST_F(AndroidBaseWindowBrowserTest,
                       GetNativeWindowReturnsValidWindowAndroid) {
  // Arrange: Obtain AndroidBaseWindow from BrowserWindowInterface.
  BrowserWindowInterface* browser_window =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  ASSERT_NE(browser_window, nullptr);
  ui::BaseWindow* android_base_window = browser_window->GetWindow();
  ASSERT_NE(android_base_window, nullptr);

  // Act.
  ui::WindowAndroid* native_window = android_base_window->GetNativeWindow();

  // Assert: native_window is not null.
  ASSERT_NE(native_window, nullptr);

  // Assert: native_window is a valid pointer.
  // We check this by dereferencing it and calling a WindowAndroid API.
  // This test doesn't care about the return value of that WindowAndroid API, as
  // long as we can get a return value.
  std::optional<gfx::Rect> bounds =
      native_window->GetBoundsInScreenCoordinates();
  EXPECT_TRUE(bounds.has_value());
}
