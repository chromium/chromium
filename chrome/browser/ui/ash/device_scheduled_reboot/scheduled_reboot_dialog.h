// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DEVICE_SCHEDULED_REBOOT_SCHEDULED_REBOOT_DIALOG_H_
#define CHROME_BROWSER_UI_ASH_DEVICE_SCHEDULED_REBOOT_SCHEDULED_REBOOT_DIALOG_H_

#include <string>

#include "base/functional/callback.h"
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
  ScheduledRebootDialog(const base::Time& reboot_time,
                        gfx::NativeView native_view,
                        base::OnceClosure reboot_callback);
  ScheduledRebootDialog(const ScheduledRebootDialog&) = delete;
  ScheduledRebootDialog& operator=(const ScheduledRebootDialog&) = delete;
  ~ScheduledRebootDialog() override;

  // Returns |dialog_delegate_|.
  views::DialogDelegate* GetDialogDelegate() const;

 protected:
  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  // Show bubble dialog and set |dialog_delegate_|.
  void ShowBubble(const base::Time& reboot_time,
                  gfx::NativeView native_view,
                  base::OnceClosure reboot_callback);

  // Invoked when the timer fires to refresh the title text.
  void UpdateWindowTitle();

  // Build title string.
  const std::u16string BuildTitle() const;

  // Timer that schedules title refreshes.
  RelaunchRequiredTimer title_refresh_timer_;

  // Dialog delegate containing the view. Owned by widget created in
  // ShowBubble().
  raw_ptr<views::DialogDelegate, DanglingUntriaged> dialog_delegate_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_ASH_DEVICE_SCHEDULED_REBOOT_SCHEDULED_REBOOT_DIALOG_H_
