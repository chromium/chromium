// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_FOOTER_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_FOOTER_BUTTON_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view_tracker.h"

namespace actions {
class BaseAction;
}  // namespace actions

namespace views {
class BoxLayout;
class ImageView;
class Label;
class MenuItemView;
}  // namespace views

// Button that represents a footer-style menu item in the Action App Menu.
class AppMenuFooterButton : public views::Button {
  METADATA_HEADER(AppMenuFooterButton, views::Button)

 public:
  // parent_menu_item is the menu item that contains this button. It determines
  // if the button's text should show mnemonics. The text override, icon
  // override and element identifier are read from base_action. submenu_item is
  // the submenu that this button opens.
  AppMenuFooterButton(views::MenuItemView* parent_menu_item,
                      actions::BaseAction* base_action,
                      views::MenuItemView* submenu_item);
  AppMenuFooterButton(const AppMenuFooterButton&) = delete;
  AppMenuFooterButton& operator=(const AppMenuFooterButton&) = delete;
  ~AppMenuFooterButton() override;

  // Shows 'text' without its mnemonic marker ('&'), sets the mnemonic,
  // and lets screen readers announce it.
  void SetText(std::u16string_view text);
  void SetImageModel(const ui::ImageModel& image_model);
  void SetUseRowStyle(bool use_row_style);

  const std::optional<std::u16string>& text_override() const {
    return text_override_;
  }
  const std::optional<ui::ImageModel>& icon_override() const {
    return icon_override_;
  }

  // views::Button:
  std::unique_ptr<views::ActionViewInterface> GetActionViewInterface() override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 protected:
  // views::Button:
  void OnEnabledChanged() override;
  void AddedToWidget() override;

 private:
  views::MenuItemView* GetSubmenuItem();
  void UpdateColors();

  const raw_ptr<views::MenuItemView> parent_menu_item_;
  bool use_row_style_ = false;
  raw_ptr<views::BoxLayout> layout_ = nullptr;
  raw_ptr<views::ImageView> icon_view_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::ImageView> submenu_arrow_view_ = nullptr;
  std::optional<std::u16string> text_override_;
  std::optional<ui::ImageModel> icon_override_;
  // Tracker so that the button can still access its submenu when the
  // button is disabled.
  views::ViewTracker submenu_item_tracker_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_FOOTER_BUTTON_H_
