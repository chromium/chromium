// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/split_tab_menu_model.h"

#include <string>

#include "base/check.h"
#include "base/notreached.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/tabs/split_tab_util.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/split_tab_id.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/menus/simple_menu_model.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SplitTabMenuModel,
                                      kReversePositionMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SplitTabMenuModel, kCloseMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SplitTabMenuModel, kExitSplitMenuItem);

SplitTabMenuModel::SplitTabMenuModel(TabStripModel* tab_strip_model,
                                     std::optional<int> split_tab_index)
    : ui::SimpleMenuModel(this),
      tab_strip_model_(tab_strip_model),
      split_tab_index_(split_tab_index) {
  AddItem(static_cast<int>(CommandId::kReversePosition), std::u16string());
  AddSeparator(ui::MenuSeparatorType::NORMAL_SEPARATOR);
  AddItemWithStringIdAndIcon(
      static_cast<int>(CommandId::kClose), IDS_SPLIT_TAB_CLOSE,
      ui::ImageModel::FromVectorIcon(vector_icons::kCloseChromeRefreshIcon,
                                     ui::kColorMenuIcon,
                                     ui::SimpleMenuModel::kDefaultIconSize));
  AddItemWithStringIdAndIcon(
      static_cast<int>(CommandId::kExitSplit), IDS_SPLIT_TAB_SEPARATE_VIEWS,
      ui::ImageModel::FromVectorIcon(kOpenInFullIcon, ui::kColorMenuIcon,
                                     ui::SimpleMenuModel::kDefaultIconSize));

  SetElementIdentifierAt(
      GetIndexOfCommandId(static_cast<int>(CommandId::kReversePosition))
          .value(),
      kReversePositionMenuItem);
  SetElementIdentifierAt(
      GetIndexOfCommandId(static_cast<int>(CommandId::kClose)).value(),
      kCloseMenuItem);
  SetElementIdentifierAt(
      GetIndexOfCommandId(static_cast<int>(CommandId::kExitSplit)).value(),
      kExitSplitMenuItem);
}

SplitTabMenuModel::~SplitTabMenuModel() = default;

bool SplitTabMenuModel::IsItemForCommandIdDynamic(int command_id) const {
  const CommandId id = static_cast<CommandId>(command_id);
  return id == CommandId::kReversePosition;
}

std::u16string SplitTabMenuModel::GetLabelForCommandId(int command_id) const {
  const CommandId id = static_cast<CommandId>(command_id);
  if (id == CommandId::kReversePosition) {
    return l10n_util::GetStringUTF16(IDS_SPLIT_TAB_REVERSE_VIEWS);
  } else {
    NOTREACHED() << "There are no other commands that are dynamic so this case "
                    "should not be reached.";
  }
}

ui::ImageModel SplitTabMenuModel::GetIconForCommandId(int command_id) const {
  const split_tabs::SplitTabActiveLocation active_split_tab_location =
      split_tabs::GetLastActiveTabLocation(tab_strip_model_, GetSplitTabId());

  const CommandId id = static_cast<CommandId>(command_id);
  const gfx::VectorIcon* icon = nullptr;
  if (id == CommandId::kReversePosition) {
    icon = &GetReversePositionIcon(active_split_tab_location);
  }
  CHECK(icon);
  return ui::ImageModel::FromVectorIcon(*icon, ui::kColorMenuIcon,
                                        ui::SimpleMenuModel::kDefaultIconSize);
}

void SplitTabMenuModel::ExecuteCommand(int command_id, int event_flags) {
  split_tabs::SplitTabId split_id = GetSplitTabId();
  switch (static_cast<CommandId>(command_id)) {
    case CommandId::kReversePosition:
      tab_strip_model_->ReverseTabsInSplit(split_id);
      break;
    case CommandId::kClose:
      tab_strip_model_->CloseWebContentsAt(
          tab_strip_model_->active_index(),
          TabCloseTypes::CLOSE_USER_GESTURE |
              TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
      break;
    case CommandId::kExitSplit:
      tab_strip_model_->RemoveSplit(split_id);
      break;
  }
}

split_tabs::SplitTabId SplitTabMenuModel::GetSplitTabId() const {
  tabs::TabInterface* const tab =
      split_tab_index_.has_value()
          ? tab_strip_model_->GetTabAtIndex(split_tab_index_.value())
          : tab_strip_model_->GetActiveTab();
  CHECK(tab->IsSplit());
  return tab->GetSplit().value();
}

const gfx::VectorIcon& SplitTabMenuModel::GetReversePositionIcon(
    split_tabs::SplitTabActiveLocation active_split_tab_location) const {
  switch (active_split_tab_location) {
    case split_tabs::SplitTabActiveLocation::kStart:
      return kSplitSceneRightIcon;
    case split_tabs::SplitTabActiveLocation::kEnd:
      return kSplitSceneLeftIcon;
    case split_tabs::SplitTabActiveLocation::kTop:
      return kSplitSceneDownIcon;
    case split_tabs::SplitTabActiveLocation::kBottom:
      return kSplitSceneUpIcon;
  }
}
