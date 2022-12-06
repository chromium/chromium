// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SETTINGS_RESET_PROMPT_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_SETTINGS_RESET_PROMPT_DIALOG_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
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
  METADATA_HEADER(SettingsResetPromptDialog);
  SettingsResetPromptDialog(
      Browser* browser,
      safe_browsing::SettingsResetPromptController* controller);
  SettingsResetPromptDialog(const SettingsResetPromptDialog&) = delete;
  SettingsResetPromptDialog& operator=(const SettingsResetPromptDialog&) =
      delete;
  ~SettingsResetPromptDialog() override;

  void Show();

  // views::DialogDelegateView:
  std::u16string GetWindowTitle() const override;

 private:
  const raw_ptr<Browser> browser_;
  raw_ptr<safe_browsing::SettingsResetPromptController, DanglingUntriaged>
      controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SETTINGS_RESET_PROMPT_DIALOG_H_
