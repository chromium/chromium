// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_ACTION_ITEM_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_ACTION_ITEM_H_

#include <memory>
#include <optional>
#include <string>

#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/base/class_property.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/color/color_id.h"

// Defines action item display types, class properties, and factory functions
// used across the app menu.
class AppMenuActionItem {
 public:
  enum class DisplayType {
    kRow,
    kBlock,
    kFooter,
    kDivider,
    kSection,
    kHeader,
    kSearch,
    kCustom,
  };

  static const ui::ClassProperty<DisplayType>* const kDisplayTypeKey;
  static const ui::ClassProperty<ui::ColorId>* const kContainerColorKey;
  static const ui::ClassProperty<std::u16string*>* const kTextOverrideKey;
  static const ui::ClassProperty<ui::ImageModel*>* const kIconOverrideKey;
  static const ui::ClassProperty<ui::ImageModel*>* const kMinorIconKey;
  static const ui::ClassProperty<ui::MenuSeparatorType>* const kSeparatorKey;
  static const ui::ClassProperty<std::u16string*>* const kChipTextKey;

  AppMenuActionItem() = delete;
  AppMenuActionItem(const AppMenuActionItem&) = delete;
  AppMenuActionItem& operator=(const AppMenuActionItem&) = delete;

  // Creates the Indirect Action Item which is the basis for the app menu in
  // order to preserve hierarchy in action items.
  static std::unique_ptr<actions::IndirectActionItem> CreateIndirect(
      actions::ActionId action_id,
      actions::ActionItem* scope,
      DisplayType display_type,
      std::optional<ui::ColorId> container_color = std::nullopt,
      std::optional<std::u16string> text_override = std::nullopt,
      std::optional<ui::ImageModel> icon_override = std::nullopt,
      std::optional<std::u16string> chip_text = std::nullopt);

  // Creates the Action Item for the headers of each section in the app menu.
  static std::unique_ptr<actions::ActionItem> CreateHeader(
      std::u16string text,
      std::optional<ui::ColorId> container_color = std::nullopt);

  static std::unique_ptr<actions::ActionItem> CreateDivider(
      ui::MenuSeparatorType separator_type =
          ui::MenuSeparatorType::NORMAL_SEPARATOR);
};

DECLARE_UI_CLASS_PROPERTY_TYPE(AppMenuActionItem::DisplayType)
DECLARE_UI_CLASS_PROPERTY_TYPE(ui::ImageModel*)
DECLARE_UI_CLASS_PROPERTY_TYPE(ui::MenuSeparatorType)

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_ACTION_ITEM_H_
