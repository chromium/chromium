// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_DIALOG_H_

#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"

namespace ash::extended_updates {

// Dialog which displays the extended updates opt-in flow, which allows users
// on certain devices that have special constraints to opt-in to receiving
// extended automatic updates.
class ExtendedUpdatesDialog : public SystemWebDialogDelegate {
 public:
  ExtendedUpdatesDialog(const ExtendedUpdatesDialog&) = delete;
  ExtendedUpdatesDialog& operator=(const ExtendedUpdatesDialog&) = delete;
  ~ExtendedUpdatesDialog() override;

  // Shows the dialog by creating a new one or focusing on an existing one.
  static void Show();

  // Returns the dialog instance currently displayed, otherwise nullptr.
  static ExtendedUpdatesDialog* Get();

  // ui::WebDialogDelegate overrides.
  void GetDialogSize(gfx::Size* size) const override;
  bool ShouldShowCloseButton() const override;

 protected:
  ExtendedUpdatesDialog();
};

}  // namespace ash::extended_updates

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_DIALOG_H_
