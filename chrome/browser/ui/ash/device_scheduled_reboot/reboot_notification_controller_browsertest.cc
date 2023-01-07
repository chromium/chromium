// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/ash/device_scheduled_reboot/reboot_notification_controller.h"

#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"

class RebootNotificationControllerForTest
    : public RebootNotificationController {
 public:
  RebootNotificationControllerForTest() = default;
  RebootNotificationControllerForTest(
      const RebootNotificationControllerForTest&) = delete;
  RebootNotificationControllerForTest& operator=(
      const RebootNotificationControllerForTest&) = delete;
  ~RebootNotificationControllerForTest() = default;

 protected:
  bool ShouldNotifyUser() const override { return true; }
};

class RebootNotificationControllerDialogTest : public DialogBrowserTest {
 public:
  RebootNotificationControllerDialogTest(
      const RebootNotificationControllerDialogTest&) = delete;
  RebootNotificationControllerDialogTest& operator=(
      const RebootNotificationControllerDialogTest&) = delete;

 protected:
  RebootNotificationControllerDialogTest() = default;
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    base::Time deadline = base::Time::Now() + base::Minutes(3);
    notification_controller_.MaybeShowPendingRebootDialog(deadline,
                                                          base::DoNothing());
  }
  // DialogBrowserTest:
  void DismissUi() override { notification_controller_.CloseRebootDialog(); }

 private:
  RebootNotificationControllerForTest notification_controller_;
};

IN_PROC_BROWSER_TEST_F(RebootNotificationControllerDialogTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}
