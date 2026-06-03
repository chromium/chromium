// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EPHEMERAL_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EPHEMERAL_BUTTON_CONTROLLER_H_

#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/uuid.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_observer.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class SidePanelEntry;

namespace content {
class Page;
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

// Controller used to trigger the contextual task toolbar button to show
// while the active tab is associated to a task and hidden otherwise.
class ContextualTasksEphemeralButtonController
    : public contextual_tasks::ContextualTasksService::Observer,
      public SidePanelEntryObserver,
      public content::WebContentsObserver {
 public:
  DECLARE_USER_DATA(ContextualTasksEphemeralButtonController);
  explicit ContextualTasksEphemeralButtonController(
      BrowserWindowInterface* browser_window_interface);
  ~ContextualTasksEphemeralButtonController() override;

  static ContextualTasksEphemeralButtonController* From(
      BrowserWindowInterface* browser_window_interface);

  // contextual_tasks::ContextualTasksService::Observer override:
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

  // AimEligibilityService observation:
  void OnAimEligibilityResponseChanged();

  // SidePanelEntryObserver override:
  void OnEntryShown(SidePanelEntry* entry) override;
  void OnEntryWillHide(SidePanelEntry* entry,
                       SidePanelEntryHideReason reason) override;
  void OnEntryHideCancelled(SidePanelEntry* entry) override;
  void OnEntryHidden(SidePanelEntry* entry) override;

  using ShouldUpdateVisibilityCallbackList =
      base::RepeatingCallbackList<void(bool)>;
  base::CallbackListSubscription RegisterShouldUpdateButtonVisibility(
      ShouldUpdateVisibilityCallbackList::CallbackType callback);

  bool ShouldShowEphemeralButton();

 private:
  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  contextual_tasks::ContextualTasksService* GetContextualTasksService();
  std::optional<SessionID> GetCurrentTabSessionId();
  bool IsActiveTabAssociatedToTask();
  void OnActiveTabChange(BrowserWindowInterface* browser_window_interface);
  void MaybeNotifyVisibilityShouldChange();
  void UpdateActiveTabObservation();
  void OnTabDiscarded(tabs::TabInterface* tab,
                      content::WebContents* old_contents,
                      content::WebContents* new_contents);

  bool is_contextual_tasks_panel_open_ = false;
  bool is_hiding_contextual_tasks_panel_ = false;
  raw_ptr<AimEligibilityService> aim_eligibility_service_;

  std::vector<base::Uuid> ephemeral_button_eligible_tasks_;
  base::ScopedObservation<SidePanelEntry, SidePanelEntryObserver>
      contextual_task_entry_observation_{this};
  raw_ptr<BrowserWindowInterface> browser_window_interface_ = nullptr;

  base::CallbackListSubscription tab_change_subscription_;
  base::CallbackListSubscription tab_discard_subscription_;
  base::CallbackListSubscription aim_eligibility_service_subscription_;
  ui::ScopedUnownedUserData<ContextualTasksEphemeralButtonController>
      scoped_unowned_user_data_;
  base::ScopedObservation<contextual_tasks::ContextualTasksService,
                          contextual_tasks::ContextualTasksService::Observer>
      contextual_task_observation_{this};
  ShouldUpdateVisibilityCallbackList should_update_visibility_callbacks_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EPHEMERAL_BUTTON_CONTROLLER_H_
