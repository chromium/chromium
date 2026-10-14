// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/contextual_tasks_tab_sub_menu_model.h"

#include <vector>

#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace contextual_tasks {

ContextualTasksTabSubMenuModel::ContextualTasksTabSubMenuModel(
    TabStripModel* tab_strip_model,
    int context_index)
    : ui::SimpleMenuModel(this),
      tab_strip_model_(tab_strip_model),
      context_index_(context_index) {
  AddItem(TabStripModel::CommandAiModeCreateNewThread,
          l10n_util::GetStringUTF16(IDS_TAB_CXMENU_AI_MODE_START_NEW_THREAD));
}

ContextualTasksTabSubMenuModel::~ContextualTasksTabSubMenuModel() = default;

bool ContextualTasksTabSubMenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool ContextualTasksTabSubMenuModel::IsCommandIdEnabled(int command_id) const {
  return tab_strip_model_->IsContextMenuCommandEnabled(
      context_index_,
      static_cast<TabStripModel::ContextMenuCommand>(command_id));
}

void ContextualTasksTabSubMenuModel::ExecuteCommand(int command_id,
                                                    int event_flags) {
  std::vector<tabs::TabHandle> tab_handles = GetTargetTabHandles();
  if (tab_handles.empty()) {
    return;
  }

  BrowserWindowInterface* browser =
      tab_strip_model_->delegate()->GetBrowserWindowInterface();
  if (!browser) {
    return;
  }

  auto* panel_controller = ContextualTasksPanelController::From(browser);
  if (!panel_controller) {
    return;
  }

  if (command_id == TabStripModel::CommandAiModeCreateNewThread) {
    panel_controller->ShareTabsWithActiveOrNewThread(tab_handles);
  }
}

std::vector<tabs::TabHandle>
ContextualTasksTabSubMenuModel::GetTargetTabHandles() const {
  if (!tab_strip_model_->ContainsIndex(context_index_)) {
    return {};
  }

  std::vector<tabs::TabInterface*> tabs;
  if (tab_strip_model_->IsTabSelected(context_index_)) {
    const auto& selected_tabs =
        tab_strip_model_->selection_model().selected_tabs();
    tabs.assign(selected_tabs.begin(), selected_tabs.end());
  } else {
    tabs = {tab_strip_model_->GetTabAtIndex(context_index_)};
  }

  std::vector<tabs::TabHandle> tab_handles;
  for (tabs::TabInterface* tab : tabs) {
    if (!tab || !tab->GetContents()) {
      continue;
    }
    const GURL& url = tab->GetContents()->GetLastCommittedURL();
    if (IsUrlValidForContextSharing(url)) {
      tab_handles.push_back(tab->GetHandle());
    }
  }
  return tab_handles;
}

}  // namespace contextual_tasks
