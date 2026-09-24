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
#include "ui/views/view_class_properties.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(AppMenuActionItem::DisplayType)
DEFINE_UI_CLASS_PROPERTY_TYPE(AppMenuActionItem::ItemHeight)
DEFINE_UI_CLASS_PROPERTY_TYPE(const base::Feature*)
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

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kAppMenuIsCheckableInternal, false)
DEFINE_UI_CLASS_PROPERTY_KEY(AppMenuActionItem::ItemHeight,
                             kAppMenuItemHeightInternal,
                             AppMenuActionItem::ItemHeight::kDefault)
DEFINE_UI_CLASS_PROPERTY_KEY(const base::Feature*,
                             kAppMenuNewBadgeFeatureInternal,
                             nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kAppMenuIsAlertedInternal, false)

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::u16string, kAppMenuTextOverrideInternal)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(ui::ImageModel, kAppMenuIconOverrideInternal)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(ui::ImageModel, kAppMenuMinorIconInternal)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::u16string, kAppMenuMinorTextInternal)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::u16string, kAppMenuChipTextInternal)

const ui::ClassProperty<AppMenuActionItem::DisplayType>* const
    AppMenuActionItem::kDisplayTypeKey = kAppMenuDisplayTypeInternal;

const ui::ClassProperty<ui::ColorId>* const
    AppMenuActionItem::kContainerColorKey = kAppMenuContainerColorInternal;

const ui::ClassProperty<std::u16string*>* const
    AppMenuActionItem::kTextOverrideKey = kAppMenuTextOverrideInternal;

const ui::ClassProperty<ui::ImageModel*>* const
    AppMenuActionItem::kIconOverrideKey = kAppMenuIconOverrideInternal;

const ui::ClassProperty<ui::ImageModel*>* const
    AppMenuActionItem::kMinorIconKey = kAppMenuMinorIconInternal;

const ui::ClassProperty<std::u16string*>* const
    AppMenuActionItem::kMinorTextKey = kAppMenuMinorTextInternal;

const ui::ClassProperty<ui::MenuSeparatorType>* const
    AppMenuActionItem::kSeparatorKey = kAppMenuSeparatorInternal;

const ui::ClassProperty<std::u16string*>* const
    AppMenuActionItem::kChipTextKey = kAppMenuChipTextInternal;

const ui::ClassProperty<bool>* const AppMenuActionItem::kIsCheckableKey =
    kAppMenuIsCheckableInternal;

const ui::ClassProperty<AppMenuActionItem::ItemHeight>* const
    AppMenuActionItem::kItemHeightKey = kAppMenuItemHeightInternal;

const ui::ClassProperty<const base::Feature*>* const
    AppMenuActionItem::kNewBadgeFeatureKey = kAppMenuNewBadgeFeatureInternal;

const ui::ClassProperty<bool>* const AppMenuActionItem::kIsAlertedKey =
    kAppMenuIsAlertedInternal;

std::unique_ptr<actions::IndirectActionItem> AppMenuActionItem::CreateIndirect(
    actions::ActionId action_id,
    actions::ActionItem* scope,
    ActionParams params) {
  actions::ActionItem* action =
      actions::ActionManager::Get().FindAction(action_id, scope);
  if (!action) {
    return nullptr;
  }

  action->SetProperty(kDisplayTypeKey,
                      params.display_type.value_or(DisplayType::kRow));

  if (params.container_color.has_value()) {
    action->SetProperty(kContainerColorKey, params.container_color.value());
  }

  if (params.is_checkable.has_value()) {
    action->SetProperty(kIsCheckableKey, params.is_checkable.value());
  }

  if (params.item_height.has_value()) {
    action->SetProperty(kItemHeightKey, params.item_height.value());
  }

  action->SetProperty(kNewBadgeFeatureKey, params.new_badge_feature.get());

  auto item = std::make_unique<actions::IndirectActionItem>(action);

  if (params.element_id) {
    item->SetProperty(views::kElementIdentifierKey, params.element_id);
  }

  if (params.is_alerted.has_value()) {
    item->SetProperty(kIsAlertedKey, params.is_alerted.value());
  }

  if (params.text_override.has_value()) {
    item->SetProperty(kTextOverrideKey, std::make_unique<std::u16string>(
                                            params.text_override.value()));
  }

  if (params.icon_override.has_value()) {
    item->SetProperty(kIconOverrideKey, std::make_unique<ui::ImageModel>(
                                            params.icon_override.value()));
  }

  if (params.minor_text.has_value()) {
    item->SetProperty(kMinorTextKey, std::make_unique<std::u16string>(
                                         params.minor_text.value()));
  }

  if (params.chip_text.has_value()) {
    item->SetProperty(kChipTextKey, std::make_unique<std::u16string>(
                                        params.chip_text.value()));
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
