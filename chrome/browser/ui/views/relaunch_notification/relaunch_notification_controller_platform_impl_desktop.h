// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_NOTIFICATION_CONTROLLER_PLATFORM_IMPL_DESKTOP_H_
#define CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_NOTIFICATION_CONTROLLER_PLATFORM_IMPL_DESKTOP_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}

class RelaunchNotificationControllerPlatformImpl : public views::WidgetObserver,
                                                   public BrowserListObserver {
 public:
  RelaunchNotificationControllerPlatformImpl();

  RelaunchNotificationControllerPlatformImpl(
      const RelaunchNotificationControllerPlatformImpl&) = delete;
  RelaunchNotificationControllerPlatformImpl& operator=(
      const RelaunchNotificationControllerPlatformImpl&) = delete;

  ~RelaunchNotificationControllerPlatformImpl() override;

  // Shows the relaunch recommended notification if it is not already open.
  void NotifyRelaunchRecommended(base::Time detection_time, bool past_deadline);

  // Shows the relaunch required notification in the most recently active
  // browser. window if it is not already open.  |on_visible| is run when the
  // notification is potentially seen to push the deadline back if the remaining
  // time is less than the grace period.
  void NotifyRelaunchRequired(base::Time deadline,
                              base::OnceCallback<base::Time()> on_visible);

  // Closes the bubble or dialog if either is still open.
  void CloseRelaunchNotification();

  // Sets the relaunch deadline to |deadline| and refreshes the notification's
  // title accordingly.
  void SetDeadline(base::Time deadline);

  // Checks whether the required dialog is shown or not.
  bool IsRequiredNotificationShown() const;

  views::Widget* GetWidgetForTesting() { return widget_; }

 protected:
  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override;

 private:
  // Shows the notification in |browser| for a relaunch that will take place
  // at |deadline|.
  void ShowRequiredNotification(Browser* browser, base::Time deadline);

  // The widget hosting the bubble or dialog, or nullptr if neither is
  // currently shown.
  raw_ptr<views::Widget> widget_ = nullptr;

  // A callback run when the relaunch required notification first becomes
  // visible to the user. This callback is valid only while the instance is
  // waiting for a tabbed browser to become active.
  base::OnceCallback<base::Time()> on_visible_;

  // A boolean to record if the relaunch notification has been shown or not.
  bool has_shown_ = false;

  // The last relaunch deadline if the relaunch notification has_shown_.
  base::Time last_relaunch_deadline_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_NOTIFICATION_CONTROLLER_PLATFORM_IMPL_DESKTOP_H_
