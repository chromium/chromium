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

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SplitViewLayoutMenuModel,
                                      kVerticalMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SplitViewLayoutMenuModel,
                                      kHorizontalMenuItem);

SplitViewLayoutMenuModel::SplitViewLayoutMenuModel(
    TabStripModel* tab_strip_model,
    tabs::TabHandle tab_handle)
    : ui::SimpleMenuModel(this),
      tab_strip_model_(tab_strip_model),
      tab_handle_(tab_handle) {
  AddItem(static_cast<int>(CommandId::kVertical), std::u16string());
  AddItem(static_cast<int>(CommandId::kHorizontal), std::u16string());

  SetElementIdentifierAt(
      GetIndexOfCommandId(static_cast<int>(CommandId::kVertical)).value(),
      kVerticalMenuItem);
  SetElementIdentifierAt(
      GetIndexOfCommandId(static_cast<int>(CommandId::kHorizontal)).value(),
      kHorizontalMenuItem);
}

SplitViewLayoutMenuModel::~SplitViewLayoutMenuModel() = default;

bool SplitViewLayoutMenuModel::IsItemForCommandIdDynamic(int command_id) const {
  const CommandId id = static_cast<CommandId>(command_id);
  return id == CommandId::kVertical || id == CommandId::kHorizontal;
}

std::u16string SplitViewLayoutMenuModel::GetLabelForCommandId(
    int command_id) const {
  const CommandId id = static_cast<CommandId>(command_id);
  switch (id) {
    case CommandId::kVertical:
      return l10n_util::GetStringUTF16(IDS_SPLIT_TAB_NEW_VERTICAL);
    case CommandId::kHorizontal:
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
    case CommandId::kVertical:
      icon = &kSplitSceneIcon;
      break;
    case CommandId::kHorizontal:
      icon = &kSplitSceneHorizontalIcon;
      break;
    default:
      NOTREACHED();
  }
  CHECK(icon);
  return ui::ImageModel::FromVectorIcon(*icon, ui::kColorMenuIcon,
                                        ui::SimpleMenuModel::kDefaultIconSize);
}

void SplitViewLayoutMenuModel::ExecuteCommand(int command_id, int event_flags) {
  if (!tab_handle_.Get()) {
    return;
  }

  int context_index = tab_strip_model_->GetIndexOfTab(tab_handle_.Get());
  const CommandId id = static_cast<CommandId>(command_id);
  split_tabs::SplitTabLayout split_layout;
  switch (id) {
    case CommandId::kVertical:
      split_layout = split_tabs::SplitTabLayout::kVertical;
      break;
    case CommandId::kHorizontal:
      split_layout = split_tabs::SplitTabLayout::kHorizontal;
      break;
    default:
      NOTREACHED();
  }

  tab_strip_model_->ExecuteAddToNewSplitCommand(context_index, split_layout);
}
