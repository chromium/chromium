// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/contextual_tasks_tab_sub_menu_model.h"

#include <algorithm>
#include <vector>

#include "base/check_op.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_separator_types.h"

namespace contextual_tasks {

ContextualTasksTabSubMenuModel::ContextualTasksTabSubMenuModel(
    TabStripModel* tab_strip_model,
    int context_index)
    : ui::SimpleMenuModel(this),
      tab_strip_model_(tab_strip_model),
      context_index_(context_index) {
  AddItem(TabStripModel::CommandAiModeCreateNewThread,
          l10n_util::GetStringUTF16(IDS_TAB_CXMENU_AI_MODE_START_NEW_THREAD));

  if (!tab_strip_model_ || !tab_strip_model_->delegate()) {
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

  recent_threads_ = panel_controller->GetRecentThreads();
  if (!recent_threads_.empty()) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    const size_t max_threads =
        kMaxRecentThreadCommandId - kMinRecentThreadCommandId + 1;
    const size_t num_threads = std::min(recent_threads_.size(), max_threads);
    for (size_t i = 0; i < num_threads; ++i) {
      AddItem(kMinRecentThreadCommandId + i, recent_threads_[i].title);
    }
  }
}

ContextualTasksTabSubMenuModel::~ContextualTasksTabSubMenuModel() = default;

bool ContextualTasksTabSubMenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool ContextualTasksTabSubMenuModel::IsCommandIdEnabled(int command_id) const {
  if (!tab_strip_model_ || !tab_strip_model_->ContainsIndex(context_index_)) {
    return false;
  }
  if (command_id >= kMinRecentThreadCommandId &&
      command_id <= kMaxRecentThreadCommandId) {
    const size_t thread_index =
        static_cast<size_t>(command_id - kMinRecentThreadCommandId);
    return thread_index < recent_threads_.size() &&
           tab_strip_model_->IsContextMenuCommandEnabled(
               context_index_, TabStripModel::CommandAiModeShare);
  }
  return tab_strip_model_->IsContextMenuCommandEnabled(
      context_index_,
      static_cast<TabStripModel::ContextMenuCommand>(command_id));
}

void ContextualTasksTabSubMenuModel::ExecuteCommand(int command_id,
                                                    int event_flags) {
  if (!tab_strip_model_ || !tab_strip_model_->delegate()) {
    return;
  }

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
  } else if (command_id >= kMinRecentThreadCommandId &&
             command_id <= kMaxRecentThreadCommandId) {
    const size_t thread_index =
        static_cast<size_t>(command_id - kMinRecentThreadCommandId);
    CHECK_LT(thread_index, recent_threads_.size());
    // TODO(crbug.com/571073862): Pass a dedicated tab context menu
    // `ChromeAimEntryPoint` once exported and wired end-to-end.
    panel_controller->ShareTabsWithExistingThread(
        recent_threads_[thread_index].task_id, tab_handles,
        omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
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
