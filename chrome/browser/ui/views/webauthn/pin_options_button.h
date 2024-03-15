// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_PIN_OPTIONS_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_PIN_OPTIONS_BUTTON_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/button/menu_button.h"

namespace views {
class MenuRunner;
}  // namespace views

// Defines a button visible in the GPM pin creation dialog, that upon pressing
// displays a menu with pin format options, allowing to pick one.
class PinOptionsButton : public views::MenuButton,
                         public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(PinOptionsButton, views::MenuButton)

 public:
  PinOptionsButton(const std::u16string& label,
                   base::RepeatingCallback<void(bool)> callback);
  PinOptionsButton(const PinOptionsButton&) = delete;
  PinOptionsButton& operator=(const PinOptionsButton&) = delete;
  ~PinOptionsButton() override;

  // views::MenuButton:
  bool IsGroupFocusTraversable() const override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  void ButtonPressed();

  base::RepeatingCallback<void(bool)> callback_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_PIN_OPTIONS_BUTTON_H_
