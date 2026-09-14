// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_action_item.h"

#include <memory>
#include <optional>
#include <string>

#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/base/class_property.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/color/color_id.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(AppMenuActionItem::DisplayType)
DEFINE_UI_CLASS_PROPERTY_TYPE(ui::ImageModel*)
DEFINE_UI_CLASS_PROPERTY_TYPE(ui::MenuSeparatorType)

DEFINE_UI_CLASS_PROPERTY_KEY(AppMenuActionItem::DisplayType,
                             kAppMenuDisplayTypeInternal,
                             AppMenuActionItem::DisplayType::kRow)

DEFINE_UI_CLASS_PROPERTY_KEY(ui::ColorId,
                             kAppMenuContainerColorInternal,
                             ui::kColorMenuBackground)

DEFINE_UI_CLASS_PROPERTY_KEY(ui::MenuSeparatorType,
                             kAppMenuSeparatorInternal,
                             ui::NORMAL_SEPARATOR)

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::u16string, kAppMenuTextOverrideInternal)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(ui::ImageModel, kAppMenuIconOverrideInternal)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::u16string, kAppMenuChipTextInternal)

const ui::ClassProperty<AppMenuActionItem::DisplayType>* const
    AppMenuActionItem::kDisplayTypeKey = kAppMenuDisplayTypeInternal;

const ui::ClassProperty<ui::ColorId>* const
    AppMenuActionItem::kContainerColorKey = kAppMenuContainerColorInternal;

const ui::ClassProperty<std::u16string*>* const
    AppMenuActionItem::kTextOverrideKey = kAppMenuTextOverrideInternal;

const ui::ClassProperty<ui::ImageModel*>* const
    AppMenuActionItem::kIconOverrideKey = kAppMenuIconOverrideInternal;

const ui::ClassProperty<ui::MenuSeparatorType>* const
    AppMenuActionItem::kSeparatorKey = kAppMenuSeparatorInternal;

const ui::ClassProperty<std::u16string*>* const
    AppMenuActionItem::kChipTextKey = kAppMenuChipTextInternal;

std::unique_ptr<actions::IndirectActionItem> AppMenuActionItem::CreateIndirect(
    actions::ActionId action_id,
    actions::ActionItem* scope,
    DisplayType display_type,
    std::optional<ui::ColorId> container_color,
    std::optional<std::u16string> text_override,
    std::optional<ui::ImageModel> icon_override,
    std::optional<std::u16string> chip_text) {
  actions::ActionItem* action =
      actions::ActionManager::Get().FindAction(action_id, scope);
  if (!action) {
    return nullptr;
  }

  action->SetProperty(kDisplayTypeKey, display_type);

  if (container_color.has_value()) {
    action->SetProperty(kContainerColorKey, container_color.value());
  }

  auto item = std::make_unique<actions::IndirectActionItem>(action);

  if (text_override.has_value()) {
    item->SetProperty(kTextOverrideKey,
                      std::make_unique<std::u16string>(text_override.value()));
  }

  if (icon_override.has_value()) {
    item->SetProperty(kIconOverrideKey,
                      std::make_unique<ui::ImageModel>(icon_override.value()));
  }

  if (chip_text.has_value()) {
    item->SetProperty(kChipTextKey,
                      std::make_unique<std::u16string>(chip_text.value()));
  }

  return item;
}

std::unique_ptr<actions::ActionItem> AppMenuActionItem::CreateHeader(
    std::u16string text,
    std::optional<ui::ColorId> container_color) {
  auto header_item = actions::ActionItem::Builder().SetText(text).Build();

  header_item->SetProperty(kDisplayTypeKey, DisplayType::kHeader);

  if (container_color.has_value()) {
    header_item->SetProperty(kContainerColorKey, container_color.value());
  }

  return header_item;
}

std::unique_ptr<actions::ActionItem> AppMenuActionItem::CreateDivider(
    ui::MenuSeparatorType separator_type) {
  auto item = actions::ActionItem::Builder().Build();
  item->SetProperty(kDisplayTypeKey, DisplayType::kDivider);
  item->SetProperty(kSeparatorKey, separator_type);
  return item;
}
