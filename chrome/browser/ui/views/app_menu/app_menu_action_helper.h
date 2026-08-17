// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_ACTION_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_ACTION_HELPER_H_

#include <memory>
#include <optional>
#include <string>

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/app_menu/app_menu_section_action_item.h"
#include "ui/actions/actions.h"
#include "ui/base/class_property.h"
#include "ui/color/color_id.h"

namespace app_menu {

enum class DisplayType {
  kRow,
  kBlock,
  kFooter,
  kCustom,
};

extern const ui::ClassProperty<DisplayType>* const kAppMenuDisplayTypeKey;
extern const ui::ClassProperty<ui::ColorId>* const kAppMenuContainerColorKey;

std::unique_ptr<actions::IndirectActionItem> CreateAppMenuIndirectActionItem(
    actions::ActionId action_id,
    DisplayType display_type,
    std::optional<ui::ColorId> container_color);

std::unique_ptr<AppMenuSectionActionItem> CreateAppMenuSectionActionItem(
    std::u16string text,
    DisplayType display_type,
    std::optional<ui::ColorId> container_color);

actions::ActionItem* GetAppMenuRoot(
    BrowserWindowInterface* browser_window_interface);

}  // namespace app_menu

DECLARE_UI_CLASS_PROPERTY_TYPE(app_menu::DisplayType)

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_ACTION_HELPER_H_
