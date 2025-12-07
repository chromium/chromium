// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/split_tab_swap_menu_model.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SplitTabSwapMenuModel,
                                      kSwapStartTabMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SplitTabSwapMenuModel,
                                      kSwapEndTabMenuItem);

SplitTabSwapMenuModel::SplitTabSwapMenuModel(TabStripModel* tab_strip_model,
                                             int tab_index)
    : ui::SimpleMenuModel(this),
      tab_strip_model_(tab_strip_model),
      tab_index_(tab_index) {
  AddItem(static_cast<int>(CommandId::kSwapStartTab), std::u16string());
  AddItem(static_cast<int>(CommandId::kSwapEndTab), std::u16string());

  SetElementIdentifierAt(
      GetIndexOfCommandId(static_cast<int>(CommandId::kSwapStartTab)).value(),
      kSwapStartTabMenuItem);
  SetElementIdentifierAt(
      GetIndexOfCommandId(static_cast<int>(CommandId::kSwapEndTab)).value(),
      kSwapEndTabMenuItem);
}

SplitTabSwapMenuModel::~SplitTabSwapMenuModel() = default;

bool SplitTabSwapMenuModel::IsItemForCommandIdDynamic(int command_id) const {
  const CommandId id = static_cast<CommandId>(command_id);
  return id == CommandId::kSwapStartTab || id == CommandId::kSwapEndTab;
}

std::u16string SplitTabSwapMenuModel::GetLabelForCommandId(
    int command_id) const {
  const CommandId id = static_cast<CommandId>(command_id);

  if (id == CommandId::kSwapStartTab) {
    return l10n_util::GetStringUTF16(
        GetSplitLayout() == split_tabs::SplitTabLayout::kVertical
            ? IDS_SPLIT_TAB_SWAP_LEFT_VIEW
            : IDS_SPLIT_TAB_SWAP_TOP_VIEW);
  } else if (id == CommandId::kSwapEndTab) {
    return l10n_util::GetStringUTF16(
        GetSplitLayout() == split_tabs::SplitTabLayout::kVertical
            ? IDS_SPLIT_TAB_SWAP_RIGHT_VIEW
            : IDS_SPLIT_TAB_SWAP_BOTTOM_VIEW);
  } else {
    NOTREACHED() << "There are no other commands that are dynamic so this case "
                    "should not be reached.";
  }
}

ui::ImageModel SplitTabSwapMenuModel::GetIconForCommandId(
    int command_id) const {
  const CommandId id = static_cast<CommandId>(command_id);
  const gfx::VectorIcon* icon = nullptr;
  if (id == CommandId::kSwapStartTab) {
    icon = GetSplitLayout() == split_tabs::SplitTabLayout::kVertical
               ? &kSplitSceneLeftIcon
               : &kSplitSceneUpIcon;
  } else if (id == CommandId::kSwapEndTab) {
    icon = GetSplitLayout() == split_tabs::SplitTabLayout::kVertical
               ? &kSplitSceneRightIcon
               : &kSplitSceneDownIcon;
  }
  CHECK(icon);
  return ui::ImageModel::FromVectorIcon(*icon, ui::kColorMenuIcon,
                                        ui::SimpleMenuModel::kDefaultIconSize);
}

void SplitTabSwapMenuModel::ExecuteCommand(int command_id, int event_flags) {
  const CommandId id = static_cast<CommandId>(command_id);
  const split_tabs::SplitTabId split_id = GetSplitTabId();
  split_tabs::SplitTabData* const split_tab_data =
      tab_strip_model_->GetSplitData(split_id);
  std::vector<tabs::TabInterface*> tabs_in_split = split_tab_data->ListTabs();
  CHECK_EQ(tabs_in_split.size(), 2U);

  if (id == CommandId::kSwapStartTab) {
    tab_strip_model_->UpdateTabInSplit(tabs_in_split[0], tab_index_,
                                       TabStripModel::SplitUpdateType::kSwap);
  } else if (id == CommandId::kSwapEndTab) {
    tab_strip_model_->UpdateTabInSplit(tabs_in_split[1], tab_index_,
                                       TabStripModel::SplitUpdateType::kSwap);
  }
}

split_tabs::SplitTabId SplitTabSwapMenuModel::GetSplitTabId() const {
  tabs::TabInterface* const tab = tab_strip_model_->GetActiveTab();
  CHECK(tab->IsSplit());
  return tab->GetSplit().value();
}

split_tabs::SplitTabLayout SplitTabSwapMenuModel::GetSplitLayout() const {
  split_tabs::SplitTabVisualData* const visual_data =
      tab_strip_model_->GetSplitData(GetSplitTabId())->visual_data();
  return visual_data->split_layout();
}
