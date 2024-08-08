// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_PIN_OPTIONS_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_PIN_OPTIONS_BUTTON_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/button/md_text_button_with_down_arrow.h"

namespace views {
class MenuRunner;
}  // namespace views

// Defines a button visible in the GPM pin creation dialog, that upon pressing
// displays a menu with pin format options, allowing to pick one.
class PinOptionsButton : public views::MdTextButtonWithDownArrow,
                         public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(PinOptionsButton, views::MdTextButtonWithDownArrow)

 public:
  enum CommandId {
    CHOOSE_SIX_DIGIT_PIN = 0,
    CHOOSE_ARBITRARY_PIN,
    COMMAND_ID_COUNT,
  };

  PinOptionsButton(const std::u16string& label,
                   CommandId checked_command_id,
                   base::RepeatingCallback<void(bool)> callback);
  PinOptionsButton(const PinOptionsButton&) = delete;
  PinOptionsButton& operator=(const PinOptionsButton&) = delete;
  ~PinOptionsButton() override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsCommandIdChecked(int command_id) const override;

 private:
  void ButtonPressed();

  const CommandId checked_command_id_;
  base::RepeatingCallback<void(bool)> callback_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_PIN_OPTIONS_BUTTON_H_
