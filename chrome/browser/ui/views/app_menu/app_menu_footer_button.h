// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_FOOTER_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_FOOTER_BUTTON_H_

#include <memory>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ui {
class ImageModel;
}  // namespace ui

// Button that represents a footer-style menu item in the Action App Menu.
class AppMenuFooterButton : public views::Button {
  METADATA_HEADER(AppMenuFooterButton, views::Button)

 public:
  explicit AppMenuFooterButton(PressedCallback callback = PressedCallback());
  AppMenuFooterButton(const AppMenuFooterButton&) = delete;
  AppMenuFooterButton& operator=(const AppMenuFooterButton&) = delete;
  ~AppMenuFooterButton() override;

  void SetText(std::u16string_view text);
  void SetImageModel(const ui::ImageModel& image_model);
  void SetHasSubmenu(bool has_submenu);

  // views::Button:
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void StateChanged(ButtonState old_state) override;
  void OnFocus() override;
  void OnBlur() override;
  std::unique_ptr<views::ActionViewInterface> GetActionViewInterface() override;

 private:
  raw_ptr<views::ImageView> icon_view_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::ImageView> submenu_arrow_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_FOOTER_BUTTON_H_
