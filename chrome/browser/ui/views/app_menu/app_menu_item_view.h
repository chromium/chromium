// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_ITEM_VIEW_H_

#include <memory>
#include <optional>
#include <string>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/controls/menu/menu_item_view.h"

class BrowserWindowInterface;

namespace actions {
class BaseAction;
}  // namespace actions

// Custom MenuItemView for ActionAppMenu that reads menu-specific properties
// from `actions::BaseAction` in addition to the underlying `ActionItem`.
class AppMenuItemView : public views::MenuItemView {
  METADATA_HEADER(AppMenuItemView, views::MenuItemView)

 public:
  AppMenuItemView(views::MenuItemView* parent,
                  int command,
                  Type type,
                  actions::BaseAction* base_action,
                  BrowserWindowInterface* browser_window);
  AppMenuItemView(const AppMenuItemView&) = delete;
  AppMenuItemView& operator=(const AppMenuItemView&) = delete;
  ~AppMenuItemView() override;

  const std::optional<std::u16string>& text_override() const {
    return text_override_;
  }
  const std::optional<ui::ImageModel>& icon_override() const {
    return icon_override_;
  }
  int default_icon_size() const { return default_icon_size_; }

  // views::MenuItemView:
  std::unique_ptr<views::ActionViewInterface> GetActionViewInterface() override;

 private:
  std::optional<std::u16string> text_override_;
  std::optional<ui::ImageModel> icon_override_;
  int default_icon_size_ = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_ITEM_VIEW_H_
