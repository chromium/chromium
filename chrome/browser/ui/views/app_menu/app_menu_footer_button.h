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
#include "ui/views/view_tracker.h"

namespace views {
class BoxLayout;
class ImageView;
class Label;
class MenuItemView;
}  // namespace views

namespace ui {
class ImageModel;
}  // namespace ui

// Button that represents a footer-style menu item in the Action App Menu.
class AppMenuFooterButton : public views::Button {
  METADATA_HEADER(AppMenuFooterButton, views::Button)

 public:
  explicit AppMenuFooterButton(views::MenuItemView* submenu_item = nullptr);
  AppMenuFooterButton(const AppMenuFooterButton&) = delete;
  AppMenuFooterButton& operator=(const AppMenuFooterButton&) = delete;
  ~AppMenuFooterButton() override;

  void SetText(std::u16string_view text);
  void SetImageModel(const ui::ImageModel& image_model);
  void SetUseRowStyle(bool use_row_style);

  // views::Button:
  std::unique_ptr<views::ActionViewInterface> GetActionViewInterface() override;

 protected:
  // views::Button:
  void OnEnabledChanged() override;

 private:
  views::MenuItemView* GetSubmenuItem();
  void UpdateColors();

  bool use_row_style_ = false;
  raw_ptr<views::BoxLayout> layout_ = nullptr;
  raw_ptr<views::ImageView> icon_view_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::ImageView> submenu_arrow_view_ = nullptr;
  // Tracker so that the button can still access its submenu when the
  // button is disabled.
  views::ViewTracker submenu_item_tracker_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_FOOTER_BUTTON_H_
