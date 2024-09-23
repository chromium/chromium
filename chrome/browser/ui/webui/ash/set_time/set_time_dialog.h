// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SET_TIME_SET_TIME_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SET_TIME_SET_TIME_DIALOG_H_

#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "ui/gfx/native_widget_types.h"

namespace ash {

// Set Time dialog for setting the system time, date and time zone.
class SetTimeDialog : public SystemWebDialogDelegate {
 public:
  SetTimeDialog(const SetTimeDialog&) = delete;
  SetTimeDialog& operator=(const SetTimeDialog&) = delete;

  // Shows the set time/date dialog. If |parent| is not null, shows the dialog
  // as a child of |parent|, e.g. the Settings window.
  static void ShowDialog(gfx::NativeWindow parent = gfx::NativeWindow());

  // Returns true if the dialog should show the timezone <select>.
  static bool ShouldShowTimezone();

 private:
  SetTimeDialog();
  ~SetTimeDialog() override;

  // SystemWebDialogDelegate
  void GetDialogSize(gfx::Size* size) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SET_TIME_SET_TIME_DIALOG_H_
