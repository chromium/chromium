// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PAGE_ACTION_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace tabs {
class TabInterface;
}  // namespace tabs

// Controller used to trigger the contextual task page action chip to show/hide.
class ContextualTasksPageActionController
    : public contextual_tasks::ContextualTasksService::Observer {
 public:
  DECLARE_USER_DATA(ContextualTasksPageActionController);
  explicit ContextualTasksPageActionController(
      tabs::TabInterface* tab_interface);
  ~ContextualTasksPageActionController() override;

  static ContextualTasksPageActionController* From(
      tabs::TabInterface* tab_interface);

  void OnTaskAdded(
      const contextual_tasks::ContextualTask& task,
      contextual_tasks::ContextualTasksService::TriggerSource source) override;
  void OnTaskUpdated(
      const contextual_tasks::ContextualTask& task,
      contextual_tasks::ContextualTasksService::TriggerSource source) override;
  void OnTaskRemoved(
      const base::Uuid& task_id,
      contextual_tasks::ContextualTasksService::TriggerSource source) override;
  void OnWillBeDestroyed() override;
  void OnTaskAssociatedToTab(const base::Uuid& task_id,
                             SessionID tab_id) override;
  void OnTaskDisassociatedFromTab(const base::Uuid& task_id,
                                  SessionID tab_id) override;

 private:
  void UpdatePageActionVisibility();

  raw_ptr<tabs::TabInterface> tab_interface_ = nullptr;

  ui::ScopedUnownedUserData<ContextualTasksPageActionController>
      scoped_unowned_user_data_;

  base::ScopedObservation<contextual_tasks::ContextualTasksService,
                          contextual_tasks::ContextualTasksService::Observer>
      contextual_task_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PAGE_ACTION_CONTROLLER_H_
