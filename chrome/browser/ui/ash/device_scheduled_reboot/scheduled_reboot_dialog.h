// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DEVICE_SCHEDULED_REBOOT_SCHEDULED_REBOOT_DIALOG_H_
#define CHROME_BROWSER_UI_ASH_DEVICE_SCHEDULED_REBOOT_SCHEDULED_REBOOT_DIALOG_H_

#include <string>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/relaunch_notification/relaunch_required_timer.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

// Dialog view notifying the user that the reboot will happen soon (within few
// minutes or seconds). User can close the dialog or choose to reboot at that
// moment. Window title of the dialog counts down remaining minutes or seconds
// until the reboot if the dialog is left open.
class ScheduledRebootDialog : public views::WidgetObserver {
 public:
  ScheduledRebootDialog(base::Time reboot_time);
  ScheduledRebootDialog(const ScheduledRebootDialog&) = delete;
  ScheduledRebootDialog& operator=(const ScheduledRebootDialog&) = delete;
  ~ScheduledRebootDialog() override;

  // Sets the timer deadline to |reboot_time| and refreshes the view's title
  // accordingly.
  void SetRebootTime(base::Time reboot_time);

  // Show bubble dialog and set |dialog_delegate_|.
  void ShowBubble(gfx::NativeView native_view,
                  base::OnceClosure reboot_callback);

  // Returns |dialog_delegate_|.
  views::DialogDelegate* GetDialogDelegate();

 protected:
  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;

 private:
  // Invoked when the timer fires to refresh the title text.
  void UpdateWindowTitle();

  // Build title string.
  std::u16string BuildTitle();

  // Time of the scheduled reboot.
  base::Time reboot_time_;

  // Timer that schedules title refreshes.
  RelaunchRequiredTimer title_refresh_timer_;

  // Dialog delegate containing the view. Owned by widget created in
  // ShowBubble().
  raw_ptr<views::DialogDelegate> dialog_delegate_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_ASH_DEVICE_SCHEDULED_REBOOT_SCHEDULED_REBOOT_DIALOG_H_