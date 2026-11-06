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
#include "ui/color/color_id.h"
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
  std::optional<ui::ColorId> default_icon_color() const {
    return default_icon_color_;
  }
  // The row's own secondary text (e.g. the search result breadcrumb), shown
  // unless the bound action provides chrome::kActionSecondaryTextKey.
  const std::u16string& default_secondary_text() const {
    return default_secondary_text_;
  }

  // Shows `secondary_title` in place, repainting without relayout. Called from
  // the action binding whenever the bound action changes.
  void UpdateSecondaryTitle(const std::u16string& secondary_title);

  // views::MenuItemView:
  std::unique_ptr<views::ActionViewInterface> GetActionViewInterface() override;

 private:
  std::optional<std::u16string> text_override_;
  std::optional<ui::ImageModel> icon_override_;
  int default_icon_size_ = 0;
  std::optional<ui::ColorId> default_icon_color_;
  std::u16string default_secondary_text_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_ITEM_VIEW_H_
