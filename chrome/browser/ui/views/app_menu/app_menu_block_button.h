// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_BLOCK_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_BLOCK_BUTTON_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/controls/button/button.h"

namespace actions {
class BaseAction;
}  // namespace actions

namespace views {
class ImageView;
class Label;
class MenuItemView;
}  // namespace views

// Button that represents a block-style menu item in the ChroMenu.
class AppMenuBlockButton : public views::Button {
  METADATA_HEADER(AppMenuBlockButton, views::Button)

 public:
  // parent_menu_item is the menu item that contains this button. It tells
  // whether the menu shows mnemonics. The text override, icon override and
  // element identifier are read from base_action.
  AppMenuBlockButton(views::MenuItemView* parent_menu_item,
                     actions::BaseAction* base_action);
  AppMenuBlockButton(const AppMenuBlockButton&) = delete;
  AppMenuBlockButton& operator=(const AppMenuBlockButton&) = delete;
  ~AppMenuBlockButton() override;

  // Shows text without its mnemonic marker ('&'), sets the mnemonic,
  // and lets screen readers announce it.
  void SetText(std::u16string_view text);
  void SetImageModel(const ui::ImageModel& image_model);

  const std::optional<std::u16string>& text_override() const {
    return text_override_;
  }
  const std::optional<ui::ImageModel>& icon_override() const {
    return icon_override_;
  }

  // views::Button:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  std::unique_ptr<views::ActionViewInterface> GetActionViewInterface() override;

 protected:
  // views::Button:
  void OnEnabledChanged() override;
  void AddedToWidget() override;

 private:
  void UpdateColors();

  const raw_ptr<views::MenuItemView> parent_menu_item_;
  raw_ptr<views::ImageView> icon_view_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;
  std::optional<std::u16string> text_override_;
  std::optional<ui::ImageModel> icon_override_;
  int corner_radius_ = 0;
};

using AppMenuBlockStyleButton = AppMenuBlockButton;

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_BLOCK_BUTTON_H_
