// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_NOTIFICATION_CONTROLLER_PLATFORM_IMPL_DESKTOP_H_
#define CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_NOTIFICATION_CONTROLLER_PLATFORM_IMPL_DESKTOP_H_

#include "base/time/time.h"

#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}

class RelaunchNotificationControllerPlatformImpl
    : public views::WidgetObserver {
 public:
  RelaunchNotificationControllerPlatformImpl();

  // Shows the relaunch recommended notification if it is not already open.
  void NotifyRelaunchRecommended(base::Time detection_time, bool past_deadline);

  // Shows the relaunch required notification if it is not already open.
  void NotifyRelaunchRequired(base::Time deadline);

  // Closes the bubble or dialog if either is still open.
  void CloseRelaunchNotification();

  // Sets the relaunch deadline to |deadline| and refreshes the notification's
  // title accordingly.
  void SetDeadline(base::Time deadline);

  // Checks whether the required dialog is shown or not.
  bool IsRequiredNotificationShown() const;

 protected:
  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;

 private:
  // The widget hosting the bubble or dialog, or nullptr if neither is is
  // currently shown.
  views::Widget* widget_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RelaunchNotificationControllerPlatformImpl);
};

#endif  // CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_NOTIFICATION_CONTROLLER_PLATFORM_IMPL_DESKTOP_H_
