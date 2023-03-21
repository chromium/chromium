// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_menu_button.h"

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_menu_model.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/common/accessibility/read_anything_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/menu/menu_runner.h"

ReadAnythingMenuButton::ReadAnythingMenuButton(
    base::RepeatingCallback<void()> callback,
    const gfx::VectorIcon& icon,
    const std::u16string& tooltip,
    ReadAnythingMenuModel* menu_model)
    : MenuButton(base::BindRepeating(&ReadAnythingMenuButton::ButtonPressed,
                                     base::Unretained(this))) {
  ConfigureInkDropForToolbar(this);
  views::InstallCircleHighlightPathGenerator(this);
  SetIcon(icon, kIconSize, gfx::kPlaceholderColor);
  SetAccessibleName(tooltip);
  SetTooltipText(tooltip);
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetMenuModel(menu_model);
  if (menu_model_)
    menu_model_->SetCallback(std::move(callback));
}

ReadAnythingMenuButton::~ReadAnythingMenuButton() = default;

void ReadAnythingMenuButton::ButtonPressed() {
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::HAS_MNEMONICS);

  gfx::Point screen_loc;
  views::View::ConvertPointToScreen(this, &screen_loc);
  gfx::Rect bounds(screen_loc, this->size());

  menu_runner_->RunMenuAt(GetWidget()->GetTopLevelWidget(), button_controller(),
                          bounds, views::MenuAnchorPosition::kTopLeft,
                          ui::MENU_SOURCE_NONE);
}

void ReadAnythingMenuButton::SetMenuModel(ReadAnythingMenuModel* menu_model) {
  menu_model_ = menu_model;
}

ReadAnythingMenuModel* ReadAnythingMenuButton::GetMenuModel() const {
  return menu_model_;
}

absl::optional<size_t> ReadAnythingMenuButton::GetSelectedIndex() const {
  if (!menu_model_) {
    return absl::nullopt;
  }
  return menu_model_->GetSelectedIndex();
}

void ReadAnythingMenuButton::SetIcon(const gfx::VectorIcon& icon,
                                     int icon_size,
                                     ui::ColorId icon_color) {
  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(icon, icon_color, icon_size));
  views::InkDrop::Get(this)->SetBaseColor(icon_color);
}

void ReadAnythingMenuButton::SetDropdownColors(ui::ColorId background_color,
                                               ui::ColorId foreground_color) {
  menu_model_->SetSubmenuBackgroundColor(background_color);
  menu_model_->SetForegroundColor(foreground_color);
}

BEGIN_METADATA(ReadAnythingMenuButton, MenuButton)
ADD_PROPERTY_METADATA(ReadAnythingMenuModel*, MenuModel)
END_METADATA
