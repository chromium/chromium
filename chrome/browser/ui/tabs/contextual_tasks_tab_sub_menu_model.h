// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_CONTEXTUAL_TASKS_TAB_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TABS_CONTEXTUAL_TASKS_TAB_SUB_MENU_MODEL_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/menus/simple_menu_model.h"

class TabStripModel;

namespace contextual_tasks {

// A sub menu model for sharing tabs with AI Mode in the Contextual Tasks side
// panel.
class ContextualTasksTabSubMenuModel : public ui::SimpleMenuModel,
                                       public ui::SimpleMenuModel::Delegate {
 public:
  // Command IDs for recent threads.
  static constexpr int kMinRecentThreadCommandId = 2101;
  static constexpr int kMaxRecentThreadCommandId = 2105;

  ContextualTasksTabSubMenuModel(TabStripModel* tab_strip_model,
                                 int context_index);
  ContextualTasksTabSubMenuModel(const ContextualTasksTabSubMenuModel&) =
      delete;
  ContextualTasksTabSubMenuModel& operator=(
      const ContextualTasksTabSubMenuModel&) = delete;
  ~ContextualTasksTabSubMenuModel() override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  std::vector<tabs::TabHandle> GetTargetTabHandles() const;

  const raw_ptr<TabStripModel> tab_strip_model_;
  const int context_index_;
  std::vector<ContextualTasksPanelController::RecentThreadInfo> recent_threads_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_UI_TABS_CONTEXTUAL_TASKS_TAB_SUB_MENU_MODEL_H_
