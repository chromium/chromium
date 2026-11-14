// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DEVICE_SCHEDULED_REBOOT_SCHEDULED_REBOOT_DIALOG_H_
#define CHROME_BROWSER_UI_ASH_DEVICE_SCHEDULED_REBOOT_SCHEDULED_REBOOT_DIALOG_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/relaunch_notification/relaunch_required_timer.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

// Dialog view notifying the user that the reboot will happen soon (within few
// minutes or seconds). User can close the dialog or choose to reboot at that
// moment. Window title of the dialog counts down remaining minutes or seconds
// until the reboot if the dialog is left open.
class ScheduledRebootDialog {
 public:
  ScheduledRebootDialog(const base::Time& reboot_time,
                        gfx::NativeView native_view,
                        base::OnceClosure reboot_callback);
  ScheduledRebootDialog(const ScheduledRebootDialog&) = delete;
  ScheduledRebootDialog& operator=(const ScheduledRebootDialog&) = delete;
  ~ScheduledRebootDialog();

  // Returns the dialog delegate of |widget_|, or null once it is closed.
  views::DialogDelegate* GetDialogDelegate() const;

 private:
  // Show bubble dialog and set |widget_|.
  void ShowBubble(const base::Time& reboot_time,
                  gfx::NativeView native_view,
                  base::OnceClosure reboot_callback);

  // Destroys |widget_| when it is closed.
  void CloseWidget(views::Widget::ClosedReason reason);

  // Invoked when the timer fires to refresh the title text.
  void UpdateWindowTitle();

  // Build title string.
  const std::u16string BuildTitle() const;

  // Timer that schedules title refreshes.
  RelaunchRequiredTimer title_refresh_timer_;

  // Widget created in ShowBubble(). Its dialog delegate contains the view.
  std::unique_ptr<views::Widget> widget_;

  base::WeakPtrFactory<ScheduledRebootDialog> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_DEVICE_SCHEDULED_REBOOT_SCHEDULED_REBOOT_DIALOG_H_
