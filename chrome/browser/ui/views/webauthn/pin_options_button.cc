// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/pin_options_button.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/menu/menu_runner.h"

PinOptionsButton::PinOptionsButton(const std::u16string& label,
                                   base::RepeatingCallback<void(bool)> callback)
    : MenuButton(base::BindRepeating(&PinOptionsButton::ButtonPressed,
                                     base::Unretained(this)),
                 label),
      callback_(std::move(callback)),
      menu_model_(std::make_unique<ui::SimpleMenuModel>(this)) {
  SetAccessibleName(label);
  SetFocusBehavior(FocusBehavior::ALWAYS);

  // TODO(rgod): Add border and icon.
  // TODO(rgod): Define actual command ids instead of integers.
  menu_model_->AddItem(1, u"6 digits (UT)");
  menu_model_->AddItem(2, u"Alphanumeric (UT)");
}

PinOptionsButton::~PinOptionsButton() = default;

bool PinOptionsButton::IsGroupFocusTraversable() const {
  return false;
}

void PinOptionsButton::ButtonPressed() {
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(),
      views::MenuRunner::COMBOBOX | views::MenuRunner::HAS_MNEMONICS);

  gfx::Point screen_loc;
  views::View::ConvertPointToScreen(this, &screen_loc);
  gfx::Rect bounds(screen_loc, this->size());

  menu_runner_->RunMenuAt(GetWidget()->GetTopLevelWidget(), button_controller(),
                          bounds, views::MenuAnchorPosition::kTopLeft,
                          ui::MENU_SOURCE_NONE);
}

void PinOptionsButton::ExecuteCommand(int command_id, int event_flags) {
  // TODO(rgod): Use actual commands.
  if (command_id == 1) {
    callback_.Run(/*is_arbitrary=*/false);
  } else if (command_id == 2) {
    callback_.Run(/*is_arbitrary=*/true);
  }
}

BEGIN_METADATA(PinOptionsButton)
END_METADATA
