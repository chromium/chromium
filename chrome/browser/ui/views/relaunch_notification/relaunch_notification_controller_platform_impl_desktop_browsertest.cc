// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/relaunch_notification/relaunch_notification_controller_platform_impl_desktop.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

using RelaunchNotificationControllerPlatformImplDesktopBrowserTest =
    InProcessBrowserTest;

// This test closes and reopens the relaunch recommended and relaunch required
// notification to ensure that there are no crashes.
IN_PROC_BROWSER_TEST_F(
    RelaunchNotificationControllerPlatformImplDesktopBrowserTest,
    NotifyRelaunchTest) {
  BrowserView::GetBrowserViewForBrowser(browser())->Show();
  // Allow UI tasks to run so that the browser becomes fully active/inactive.
  base::RunLoop().RunUntilIdle();
  RelaunchNotificationControllerPlatformImpl impl;
  base::Time deadline = base::Time::Now() + base::Hours(1);

  impl.NotifyRelaunchRecommended(deadline, false);
  views::test::WidgetDestroyedWaiter first_destroyed_waiter(
      impl.GetWidgetForTesting());
  impl.CloseRelaunchNotification();
  first_destroyed_waiter.Wait();

  impl.NotifyRelaunchRecommended(deadline, false);
  views::test::WidgetDestroyedWaiter second_destroyed_waiter(
      impl.GetWidgetForTesting());
  // Simulate widget is closed outside of RelaunchNotificationController.
  impl.GetWidgetForTesting()->Close();
  second_destroyed_waiter.Wait();

  impl.NotifyRelaunchRecommended(deadline, false);
  views::test::WidgetDestroyedWaiter third_destroyed_waiter(
      impl.GetWidgetForTesting());
  impl.CloseRelaunchNotification();
  third_destroyed_waiter.Wait();

  // Switch to a relaunch required notification.
  impl.NotifyRelaunchRequired(deadline, base::OnceCallback<base::Time()>());
  views::test::WidgetDestroyedWaiter fourth_destroyed_waiter(
      impl.GetWidgetForTesting());
  impl.CloseRelaunchNotification();
  fourth_destroyed_waiter.Wait();
  impl.NotifyRelaunchRequired(deadline, base::OnceCallback<base::Time()>());
  views::test::WidgetDestroyedWaiter fifth_destroyed_waiter(
      impl.GetWidgetForTesting());
  impl.CloseRelaunchNotification();
  // Wait for the notification to be closed before exiting the test and
  // destroying  RelaunchNotificationControllerPlatformImpl.
  fifth_destroyed_waiter.Wait();
}
