// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/split_view_layout_menu_model.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SplitViewLayoutMenuModel,
                                      kVerticalMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SplitViewLayoutMenuModel,
                                      kHorizontalMenuItem);

SplitViewLayoutMenuModel::SplitViewLayoutMenuModel(
    ExecuteCommandCallback execute_command_callback)
    : ui::SimpleMenuModel(this),
      execute_command_callback_(std::move(execute_command_callback)) {
  AddItem(static_cast<int>(CommandId::kSideBySide), std::u16string());
  AddItem(static_cast<int>(CommandId::kStacked), std::u16string());

  SetElementIdentifierAt(
      GetIndexOfCommandId(static_cast<int>(CommandId::kSideBySide)).value(),
      kVerticalMenuItem);
  SetElementIdentifierAt(
      GetIndexOfCommandId(static_cast<int>(CommandId::kStacked)).value(),
      kHorizontalMenuItem);
}

SplitViewLayoutMenuModel::~SplitViewLayoutMenuModel() = default;

bool SplitViewLayoutMenuModel::IsItemForCommandIdDynamic(int command_id) const {
  const CommandId id = static_cast<CommandId>(command_id);
  return id == CommandId::kSideBySide || id == CommandId::kStacked;
}

std::u16string SplitViewLayoutMenuModel::GetLabelForCommandId(
    int command_id) const {
  const CommandId id = static_cast<CommandId>(command_id);
  switch (id) {
    case CommandId::kSideBySide:
      return l10n_util::GetStringUTF16(IDS_SPLIT_TAB_NEW_VERTICAL);
    case CommandId::kStacked:
      return l10n_util::GetStringUTF16(IDS_SPLIT_TAB_NEW_HORIZONTAL);
    default:
      NOTREACHED();
  }
}

ui::ImageModel SplitViewLayoutMenuModel::GetIconForCommandId(
    int command_id) const {
  const CommandId id = static_cast<CommandId>(command_id);
  const gfx::VectorIcon* icon = nullptr;
  switch (id) {
    case CommandId::kSideBySide:
      icon = &(features::IsRoundedIconsEnabled() ? kSplitSceneIcon
                                                 : kSplitSceneOldIcon);
      break;
    case CommandId::kStacked:
      icon = &kSplitSceneHorizontalCustomIcon;
      break;
    default:
      NOTREACHED();
  }
  CHECK(icon);
  return ui::ImageModel::FromVectorIcon(*icon, ui::kColorMenuIcon,
                                        ui::SimpleMenuModel::kDefaultIconSize);
}

void SplitViewLayoutMenuModel::ExecuteCommand(int command_id, int event_flags) {
  const CommandId id = static_cast<CommandId>(command_id);
  split_tabs::SplitTabLayout split_layout;
  switch (id) {
    case CommandId::kSideBySide:
      split_layout = split_tabs::SplitTabLayout::kSideBySide;
      break;
    case CommandId::kStacked:
      split_layout = split_tabs::SplitTabLayout::kStacked;
      break;
    default:
      NOTREACHED();
  }

  std::move(execute_command_callback_).Run(split_layout);
}
