// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EPHEMERAL_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EPHEMERAL_BUTTON_CONTROLLER_H_

#include <map>
#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "base/uuid.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class SidePanelEntry;

namespace actions {
class ActionItem;
}  // namespace actions

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
      public content::WebContentsObserver,
      public PinnedToolbarActionsModel::Observer {
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

  // PinnedToolbarActionsModel::Observer override:
  void OnActionsChanged() override;

  using ShouldUpdateVisibilityCallbackList =
      base::RepeatingCallbackList<void(bool)>;
  base::CallbackListSubscription RegisterShouldUpdateButtonVisibility(
      ShouldUpdateVisibilityCallbackList::CallbackType callback);

  using ShouldUpdateButtonPositionCallbackList = base::RepeatingClosureList;
  base::CallbackListSubscription RegisterShouldUpdateButtonPosition(
      ShouldUpdateButtonPositionCallbackList::CallbackType callback);

  enum class RemovalReason {
    kContextMenu,
    kAutoDismissed,
    kTaskDisassociated,
  };

  bool ShouldShowEphemeralButton();
  bool ShouldShowPinnedButtonActiveIndicator();
  bool IsActiveTabAssociatedToTask();
  bool IsPanelOpen() const {
    return is_contextual_tasks_panel_open_ &&
           !is_hiding_contextual_tasks_panel_;
  }
  void RemoveEphemeralButtonForCurrentTask();
  std::optional<RemovalReason> removal_reason() const {
    return removal_reason_;
  }
  bool HasAutoDismissTimerForTesting(SessionID tab_id) const;
  void SetTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

 private:
  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  contextual_tasks::ContextualTasksService* GetContextualTasksService();
  std::optional<SessionID> GetCurrentTabSessionId();
  bool HasPanelResults() const;
  void OnActiveTabChange(BrowserWindowInterface* browser_window_interface);
  void OnSidePanelAlignmentChanged();
  void MaybeNotifyVisibilityShouldChange();
  void UpdatePinnedButtonActiveIndicator();
  void UpdateActiveTabObservation();
  void OnTabDiscarded(tabs::TabInterface* tab,
                      content::WebContents* old_contents,
                      content::WebContents* new_contents);
  base::TimeDelta GetAutoDismissDuration() const;
  void StartAutoDismissTimerForTab(SessionID tab_id);
  void OnAutoDismissTimerFired(SessionID tab_id);
  void ClearTabEligibility(SessionID tab_id);
  bool IsTaskEligibleForEphemeralButton(const base::Uuid& task_id);
  bool IsActiveTabEligible();
  void HideTaskSidePanel();
  void MaybeCleanUpUnusedSidePanelWebContents();
  void DismissEphemeralButtonForTab(SessionID tab_id);
  void DisassociateNavigatedTab();
  void CancelAutoDismissTimersForActiveTask();
  void MarkActiveTaskTabsEligible();

  bool is_contextual_tasks_panel_open_ = false;
  bool is_hiding_contextual_tasks_panel_ = false;
  std::optional<RemovalReason> removal_reason_;
  raw_ptr<AimEligibilityService> aim_eligibility_service_;

  // Tabs associated to a task whose side panel was shown or dismissed. All
  // of the task's tabs are recorded rather than the task itself, because a
  // zero-state task with no thread is destroyed as soon as it loses its last
  // tab, which would otherwise take the button down with it.
  std::vector<SessionID> ephemeral_button_eligible_tabs_;
  std::map<SessionID, base::OneShotTimer>
      ephemeral_button_auto_dismiss_timers_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_for_testing_;
  base::ScopedObservation<SidePanelEntry, SidePanelEntryObserver>
      contextual_task_entry_observation_{this};
  raw_ptr<BrowserWindowInterface> browser_window_interface_ = nullptr;
  raw_ptr<actions::ActionItem> close_side_panel_action_item_ = nullptr;

  PrefChangeRegistrar pref_change_registrar_;
  base::CallbackListSubscription tab_change_subscription_;
  base::CallbackListSubscription tab_discard_subscription_;
  base::CallbackListSubscription aim_eligibility_service_subscription_;
  ui::ScopedUnownedUserData<ContextualTasksEphemeralButtonController>
      scoped_unowned_user_data_;
  base::ScopedObservation<contextual_tasks::ContextualTasksService,
                          contextual_tasks::ContextualTasksService::Observer>
      contextual_task_observation_{this};
  base::ScopedObservation<PinnedToolbarActionsModel,
                          PinnedToolbarActionsModel::Observer>
      pinned_toolbar_observation_{this};
  ShouldUpdateVisibilityCallbackList should_update_visibility_callbacks_;
  ShouldUpdateButtonPositionCallbackList should_update_position_callbacks_;
  base::WeakPtrFactory<ContextualTasksEphemeralButtonController>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EPHEMERAL_BUTTON_CONTROLLER_H_
