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
    tabs::TabHandle context_tab)
    : ui::SimpleMenuModel(this),
      tab_strip_model_(tab_strip_model),
      context_tab_(context_tab) {
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

ContextualTasksTabSubMenuModel::ContextualTasksTabSubMenuModel(
    TabStripModel* tab_strip_model,
    int context_index)
    : ContextualTasksTabSubMenuModel(
          tab_strip_model,
          (tab_strip_model && tab_strip_model->ContainsIndex(context_index))
              ? tab_strip_model->GetTabAtIndex(context_index)->GetHandle()
              : tabs::TabHandle::Null()) {}

ContextualTasksTabSubMenuModel::~ContextualTasksTabSubMenuModel() = default;

bool ContextualTasksTabSubMenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool ContextualTasksTabSubMenuModel::IsCommandIdEnabled(int command_id) const {
  if (!tab_strip_model_ || !context_tab_.Get() ||
      tab_strip_model_->GetIndexOfTab(context_tab_.Get()) ==
          TabStripModel::kNoTab) {
    return false;
  }
  if (command_id >= kMinRecentThreadCommandId &&
      command_id <= kMaxRecentThreadCommandId) {
    const size_t thread_index =
        static_cast<size_t>(command_id - kMinRecentThreadCommandId);
    return thread_index < recent_threads_.size() &&
           tab_strip_model_->IsContextMenuCommandEnabled(
               context_tab_, TabStripModel::CommandAiModeShare);
  }
  return tab_strip_model_->IsContextMenuCommandEnabled(
      context_tab_, static_cast<TabStripModel::ContextMenuCommand>(command_id));
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
    panel_controller->ShareTabsWithNewThread(tab_handles);
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
  if (!tab_strip_model_) {
    return {};
  }

  const std::vector<tabs::TabHandle> tabs =
      tab_strip_model_->GetTabsForCommand(context_tab_);

  std::vector<tabs::TabHandle> tab_handles;
  for (tabs::TabHandle tab_handle : tabs) {
    tabs::TabInterface* tab = tab_handle.Get();
    if (!tab || !tab->GetContents()) {
      continue;
    }
    const GURL& url = tab->GetContents()->GetLastCommittedURL();
    if (IsUrlValidForContextSharing(url)) {
      tab_handles.push_back(tab_handle);
    }
  }
  return tab_handles;
}

}  // namespace contextual_tasks
