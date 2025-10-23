// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/split_tab_menu_model.h"

#include <string>

#include "base/check.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/split_tab_util.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/split_tab_id.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/menus/simple_menu_model.h"

namespace {
std::string GetMetricsSuffixForSource(
    SplitTabMenuModel::MenuSource menu_source) {
  // These strings are persisted to logs. Entries should not be changed.
  switch (menu_source) {
    case SplitTabMenuModel::MenuSource::kToolbarButton:
      return "ToolbarButton";
    case SplitTabMenuModel::MenuSource::kMiniToolbar:
      return "MiniToolbar";
    case SplitTabMenuModel::MenuSource::kTabContextMenu:
      return "TabContextMenu";
  }
}

int GetCommandIdInt(SplitTabMenuModel::CommandId command_id) {
  // Start command IDs at 1701 to avoid conflicts with other submenus.
  return ExistingBaseSubMenuModel::kMinSplitTabMenuModelCommandId +
         static_cast<int>(command_id);
}

SplitTabMenuModel::CommandId GetCommandIdEnum(int command_id) {
  return static_cast<SplitTabMenuModel::CommandId>(
      command_id - ExistingBaseSubMenuModel::kMinSplitTabMenuModelCommandId);
}

BrowserWindowInterface* GetBrowserWithTabStripModel(
    TabStripModel* tab_strip_model) {
  BrowserWindowInterface* result = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [tab_strip_model, &result](BrowserWindowInterface* browser) {
        if (browser->GetTabStripModel() == tab_strip_model) {
          result = browser;
        }
        return !result;
      });
  return result;
}

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SplitTabMenuModel,
                                      kReversePositionMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SplitTabMenuModel, kCloseMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SplitTabMenuModel,
                                      kCloseStartTabMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SplitTabMenuModel, kCloseEndTabMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SplitTabMenuModel, kExitSplitMenuItem);

SplitTabMenuModel::SplitTabMenuModel(TabStripModel* tab_strip_model,
                                     MenuSource menu_source,
                                     std::optional<int> split_tab_index)
    : ui::SimpleMenuModel(this),
      tab_strip_model_(tab_strip_model),
      menu_source_(menu_source),
      split_tab_index_(split_tab_index) {
  AddItemWithStringIdAndIcon(
      GetCommandIdInt(CommandId::kExitSplit), IDS_SPLIT_TAB_SEPARATE_VIEWS,
      ui::ImageModel::FromVectorIcon(kOpenInFullIcon, ui::kColorMenuIcon,
                                     ui::SimpleMenuModel::kDefaultIconSize));
  AddSeparator(ui::MenuSeparatorType::NORMAL_SEPARATOR);

  if (menu_source == MenuSource::kMiniToolbar) {
    CHECK(split_tab_index.has_value());
    AddItemWithStringIdAndIcon(
        GetCommandIdInt(CommandId::kCloseSpecifiedTab), IDS_SPLIT_TAB_CLOSE,
        ui::ImageModel::FromVectorIcon(vector_icons::kCloseChromeRefreshIcon,
                                       ui::kColorMenuIcon,
                                       ui::SimpleMenuModel::kDefaultIconSize));
    SetElementIdentifierAt(
        GetIndexOfCommandId(GetCommandIdInt(CommandId::kCloseSpecifiedTab))
            .value(),
        kCloseMenuItem);
  } else if (menu_source == MenuSource::kTabContextMenu ||
             menu_source == MenuSource::kToolbarButton) {
    AddItem(GetCommandIdInt(CommandId::kCloseStartTab), std::u16string());
    AddItem(GetCommandIdInt(CommandId::kCloseEndTab), std::u16string());
    AddSeparator(ui::MenuSeparatorType::NORMAL_SEPARATOR);

    SetElementIdentifierAt(
        GetIndexOfCommandId(GetCommandIdInt(CommandId::kCloseStartTab)).value(),
        kCloseStartTabMenuItem);
    SetElementIdentifierAt(
        GetIndexOfCommandId(GetCommandIdInt(CommandId::kCloseEndTab)).value(),
        kCloseEndTabMenuItem);
  } else {
    NOTREACHED() << "Unknown close menu item option";
  }

  AddItem(GetCommandIdInt(CommandId::kReversePosition), std::u16string());

  SetElementIdentifierAt(
      GetIndexOfCommandId(GetCommandIdInt(CommandId::kReversePosition)).value(),
      kReversePositionMenuItem);
  SetElementIdentifierAt(
      GetIndexOfCommandId(GetCommandIdInt(CommandId::kExitSplit)).value(),
      kExitSplitMenuItem);

  // Only render feedback in the toolbar button menu.
  if (menu_source == MenuSource::kToolbarButton &&
      tab_strip_model->profile()->GetPrefs()->GetBoolean(
          prefs::kUserFeedbackAllowed)) {
    AddSeparator(ui::MenuSeparatorType::NORMAL_SEPARATOR);
    AddItemWithStringIdAndIcon(GetCommandIdInt(CommandId::kSendFeedback),
                               IDS_SPLIT_TAB_SEND_FEEDBACK,
                               ui::ImageModel::FromVectorIcon(kReportIcon));
  }
}

SplitTabMenuModel::~SplitTabMenuModel() = default;

bool SplitTabMenuModel::IsItemForCommandIdDynamic(int command_id) const {
  const CommandId id = GetCommandIdEnum(command_id);
  return id == CommandId::kReversePosition || id == CommandId::kCloseStartTab ||
         id == CommandId::kCloseEndTab;
}

std::u16string SplitTabMenuModel::GetLabelForCommandId(int command_id) const {
  const CommandId id = GetCommandIdEnum(command_id);

  if (id == CommandId::kReversePosition) {
    return l10n_util::GetStringUTF16(IDS_SPLIT_TAB_REVERSE_VIEWS);
  } else if (id == CommandId::kCloseStartTab) {
    return l10n_util::GetStringUTF16(
        GetSplitLayout() == split_tabs::SplitTabLayout::kVertical
            ? IDS_SPLIT_TAB_CLOSE_LEFT_VIEW
            : IDS_SPLIT_TAB_CLOSE_TOP_VIEW);
  } else if (id == CommandId::kCloseEndTab) {
    return l10n_util::GetStringUTF16(
        GetSplitLayout() == split_tabs::SplitTabLayout::kVertical
            ? IDS_SPLIT_TAB_CLOSE_RIGHT_VIEW
            : IDS_SPLIT_TAB_CLOSE_BOTTOM_VIEW);
  } else {
    NOTREACHED() << "There are no other commands that are dynamic so this case "
                    "should not be reached.";
  }
}

ui::ImageModel SplitTabMenuModel::GetIconForCommandId(int command_id) const {
  const split_tabs::SplitTabActiveLocation active_split_tab_location =
      split_tabs::GetLastActiveTabLocation(tab_strip_model_, GetSplitTabId());

  const CommandId id = GetCommandIdEnum(command_id);
  const gfx::VectorIcon* icon = nullptr;
  if (id == CommandId::kReversePosition) {
    icon = &GetReversePositionIcon(active_split_tab_location);
  } else if (id == CommandId::kCloseStartTab) {
    icon = GetSplitLayout() == split_tabs::SplitTabLayout::kVertical
               ? &kLeftPanelCloseIcon
               : &kTopPanelCloseIcon;
  } else if (id == CommandId::kCloseEndTab) {
    icon = GetSplitLayout() == split_tabs::SplitTabLayout::kVertical
               ? &kRightPanelCloseIcon
               : &kBottomPanelCloseIcon;
  }
  CHECK(icon);
  return ui::ImageModel::FromVectorIcon(*icon, ui::kColorMenuIcon,
                                        ui::SimpleMenuModel::kDefaultIconSize);
}

void SplitTabMenuModel::ExecuteCommand(int command_id, int event_flags) {
  const split_tabs::SplitTabId split_id = GetSplitTabId();
  split_tabs::SplitTabData* const split_tab_data =
      tab_strip_model_->GetSplitData(split_id);
  std::vector<tabs::TabInterface*> tabs_in_split = split_tab_data->ListTabs();
  CHECK_EQ(tabs_in_split.size(), 2U);
  CommandId split_command_id = GetCommandIdEnum(command_id);
  switch (split_command_id) {
    case CommandId::kReversePosition:
      tab_strip_model_->ReverseTabsInSplit(split_id);
      break;
    case CommandId::kCloseSpecifiedTab:
      CloseTabAtIndex(split_tab_index_.value());
      break;
    case CommandId::kCloseStartTab: {
      int startIndex = base::i18n::IsRTL() ? 1 : 0;
      CloseTabAtIndex(
          tab_strip_model_->GetIndexOfTab(tabs_in_split[startIndex]));
      break;
    }
    case CommandId::kCloseEndTab: {
      int endIndex = base::i18n::IsRTL() ? 0 : 1;
      CloseTabAtIndex(tab_strip_model_->GetIndexOfTab(tabs_in_split[endIndex]));
      break;
    }
    case CommandId::kExitSplit:
      tab_strip_model_->RemoveSplit(split_id);
      break;
    case CommandId::kSendFeedback:
      SendFeedback();
      break;
  }

  base::UmaHistogramEnumeration(
      "Tabs.SplitViewMenu." + GetMetricsSuffixForSource(menu_source_),
      split_command_id);
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

split_tabs::SplitTabLayout SplitTabMenuModel::GetSplitLayout() const {
  split_tabs::SplitTabVisualData* const visual_data =
      tab_strip_model_->GetSplitData(GetSplitTabId())->visual_data();
  return visual_data->split_layout();
}

void SplitTabMenuModel::CloseTabAtIndex(int index) {
  tab_strip_model_->CloseWebContentsAt(
      index, TabCloseTypes::CLOSE_USER_GESTURE |
                 TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
}

void SplitTabMenuModel::SendFeedback() {
  BrowserWindowInterface* const browser =
      GetBrowserWithTabStripModel(tab_strip_model_);
  CHECK(browser);
  chrome::ShowFeedbackPage(browser, feedback::kFeedbackSourceSplitView, "", "",
                           "split_view", "");
}
