// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SETTINGS_RESET_PROMPT_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_SETTINGS_RESET_PROMPT_DIALOG_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/window/dialog_delegate.h"

class Browser;

namespace safe_browsing {
class SettingsResetPromptController;
}

// A dialog intended for prompting users to reset some of their settings to
// their original default values. The dialog has two sections:
// 1. Main section with an explanation text
// 2. An expandable details section containing the details of the reset
//    operation.
class SettingsResetPromptDialog : public views::DialogDelegateView {
 public:
  SettingsResetPromptDialog(
      Browser* browser,
      safe_browsing::SettingsResetPromptController* controller);
  ~SettingsResetPromptDialog() override;

  void Show();

  // views::DialogDelegateView:
  base::string16 GetWindowTitle() const override;

 private:
  Browser* const browser_;
  safe_browsing::SettingsResetPromptController* controller_;

  DISALLOW_COPY_AND_ASSIGN(SettingsResetPromptDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SETTINGS_RESET_PROMPT_DIALOG_H_
