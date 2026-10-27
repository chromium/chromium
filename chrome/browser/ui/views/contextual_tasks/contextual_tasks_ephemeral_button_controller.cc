// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_ephemeral_button_controller.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_ephemeral_button_menu_model.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_ephemeral_button_utils.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/view_class_properties.h"

namespace {
bool IsContextualTasksPage(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) &&
         url.host() == chrome::kChromeUIContextualTasksHost;
}
}  // namespace

DEFINE_USER_DATA(ContextualTasksEphemeralButtonController);

ContextualTasksEphemeralButtonController::
    ContextualTasksEphemeralButtonController(
        BrowserWindowInterface* browser_window_interface)
    : browser_window_interface_(browser_window_interface),
      scoped_unowned_user_data_(
          browser_window_interface->GetUnownedUserDataHost(),
          *this) {
  Profile* const profile = browser_window_interface_->GetProfile();
  contextual_tasks::ContextualTasksService* const contextual_tasks_service =
      contextual_tasks::ContextualTasksServiceFactory::GetForProfile(profile);
  contextual_task_observation_.Observe(contextual_tasks_service);
  tab_change_subscription_ =
      browser_window_interface_->RegisterActiveTabDidChange(base::BindRepeating(
          &ContextualTasksEphemeralButtonController::OnActiveTabChange,
          base::Unretained(this)));

  contextual_task_entry_observation_.Observe(
      SidePanelRegistry::From(browser_window_interface_)
          ->GetEntryForKey(
              SidePanelEntryKey(SidePanelEntryId::kContextualTasks)));

  aim_eligibility_service_ =
      AimEligibilityServiceFactory::GetForProfile(profile);
  if (aim_eligibility_service_) {
    aim_eligibility_service_subscription_ =
        aim_eligibility_service_->RegisterEligibilityChangedCallback(
            base::BindRepeating(&ContextualTasksEphemeralButtonController::
                                    OnAimEligibilityResponseChanged,
                                base::Unretained(this)));
  }
  if (auto* pinned_model = PinnedToolbarActionsModel::Get(profile)) {
    pinned_toolbar_observation_.Observe(pinned_model);
  }
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kSidePanelHorizontalAlignment,
      base::BindRepeating(&ContextualTasksEphemeralButtonController::
                              OnSidePanelAlignmentChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kSidePanelAlignmentOverrides,
      base::BindRepeating(&ContextualTasksEphemeralButtonController::
                              OnSidePanelAlignmentChanged,
                          base::Unretained(this)));
  if (BrowserActions* const browser_actions =
          BrowserActions::From(browser_window_interface_);
      browser_actions && browser_actions->root_action_item()) {
    if (actions::ActionItem* const action_item =
            actions::ActionManager::Get().FindAction(
                kActionSidePanelShowContextualTasks,
                browser_actions->root_action_item())) {
      if (contextual_tasks::
              IsContextualTasksPinnedButtonActiveIndicatorEnabled()) {
        action_item->SetProperty(
            kActionItemEngagedIndicatorStyleKey,
            contextual_tasks::
                    IsContextualTasksPinnedButtonActiveIndicatorCircleHighlightEnabled()
                ? ActionItemEngagedIndicatorStyle::kCircleHighlight
                : ActionItemEngagedIndicatorStyle::kUnderline);
      }
      if (contextual_tasks::
              ShouldShowCloseSidePanelInPinnedButtonContextMenu()) {
        const int close_label_id =
            contextual_tasks::
                    ShouldUseCloseSidePanelForEphemeralButtonContextMenu()
                ? IDS_CONTEXTUAL_TASKS_EPHEMERAL_BUTTON_CONTEXT_MENU_CLOSE
                : IDS_CONTEXTUAL_TASKS_EPHEMERAL_BUTTON_CONTEXT_MENU_REMOVE;
        close_side_panel_action_item_ = action_item->AddChild(
            actions::ActionItem::Builder(
                base::BindRepeating(
                    [](base::WeakPtr<ContextualTasksEphemeralButtonController>
                           controller,
                       actions::ActionItem* item,
                       actions::ActionInvocationContext context) {
                      if (controller) {
                        controller->RemoveEphemeralButtonForCurrentTask();
                      }
                    },
                    weak_ptr_factory_.GetWeakPtr()))
                .SetActionId(kActionContextualTasksCloseSidePanel)
                .SetText(l10n_util::GetStringUTF16(close_label_id))
                .SetImage(ui::ImageModel::FromVectorIcon(
                    features::IsRoundedIconsEnabled()
                        ? vector_icons::kCloseIcon
                        : vector_icons::kCloseOldIcon,
                    ui::kColorIcon, ui::SimpleMenuModel::kDefaultIconSize))
                .SetProperty(
                    views::kElementIdentifierKey,
                    ContextualTasksEphemeralButtonMenuModel::
                        kRemoveForTaskMenuItem)
                .SetVisible(false)
                .SetEnabled(false)
                .Build());
      }
    }
  }
  UpdateActiveTabObservation();
  UpdatePinnedButtonActiveIndicator();
}

ContextualTasksEphemeralButtonController::
    ~ContextualTasksEphemeralButtonController() {
  if (actions::ActionItem* const item =
          std::exchange(close_side_panel_action_item_, nullptr)) {
    if (actions::BaseAction* const parent = item->GetParent()) {
      parent->RemoveChild(item);
    }
  }
}

// static:
ContextualTasksEphemeralButtonController*
ContextualTasksEphemeralButtonController::From(
    BrowserWindowInterface* browser_window_interface) {
  return Get(browser_window_interface->GetUnownedUserDataHost());
}

void ContextualTasksEphemeralButtonController::OnTaskAdded(
    const contextual_tasks::ContextualTask& task,
    contextual_tasks::ContextualTasksService::TriggerSource source) {
  MaybeNotifyVisibilityShouldChange();
}

void ContextualTasksEphemeralButtonController::OnTaskUpdated(
    const contextual_tasks::ContextualTask& task,
    contextual_tasks::ContextualTasksService::TriggerSource source) {
  OnTaskAdded(task, source);
}

void ContextualTasksEphemeralButtonController::OnTaskRemoved(
    const base::Uuid& task_id,
    contextual_tasks::ContextualTasksService::TriggerSource source) {
  std::optional<base::AutoReset<std::optional<RemovalReason>>> reset_reason;
  if (!removal_reason_.has_value()) {
    reset_reason.emplace(&removal_reason_, RemovalReason::kTaskDisassociated);
  }
  MaybeNotifyVisibilityShouldChange();
}

void ContextualTasksEphemeralButtonController::OnWillBeDestroyed() {
  should_update_visibility_callbacks_.Notify(false);
  ephemeral_button_auto_dismiss_timers_.clear();
  contextual_task_observation_.Reset();
  pinned_toolbar_observation_.Reset();
  pref_change_registrar_.Reset();
  tab_discard_subscription_ = base::CallbackListSubscription();
  Observe(nullptr);
  UpdatePinnedButtonActiveIndicator();
}

void ContextualTasksEphemeralButtonController::OnTaskAssociatedToTab(
    const base::Uuid& task_id,
    SessionID tab_id) {
  if (contextual_tasks::IsContextualTasksPinningV2Enabled() && IsPanelOpen() &&
      GetCurrentTabSessionId() == tab_id) {
    MarkActiveTaskTabsEligible();
  }
  MaybeNotifyVisibilityShouldChange();
}

void ContextualTasksEphemeralButtonController::OnTaskDisassociatedFromTab(
    const base::Uuid& task_id,
    SessionID tab_id) {
  ClearTabEligibility(tab_id);
  std::optional<base::AutoReset<std::optional<RemovalReason>>> reset_reason;
  if (!removal_reason_.has_value()) {
    reset_reason.emplace(&removal_reason_, RemovalReason::kTaskDisassociated);
  }
  MaybeNotifyVisibilityShouldChange();
}

void ContextualTasksEphemeralButtonController::
    OnAimEligibilityResponseChanged() {
  MaybeNotifyVisibilityShouldChange();
}

void ContextualTasksEphemeralButtonController::OnEntryShown(
    SidePanelEntry* entry) {
  is_contextual_tasks_panel_open_ = true;
  is_hiding_contextual_tasks_panel_ = false;
  if (contextual_tasks::kShowEntryPoint.Get() ==
      contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    if (contextual_tasks::IsContextualTasksPinningV2Enabled()) {
      MarkActiveTaskTabsEligible();
    }
    CancelAutoDismissTimersForActiveTask();
    MaybeNotifyVisibilityShouldChange();
  } else {
    UpdatePinnedButtonActiveIndicator();
  }
}

void ContextualTasksEphemeralButtonController::OnEntryWillHide(
    SidePanelEntry* entry,
    SidePanelEntryHideReason reason) {
  is_hiding_contextual_tasks_panel_ = true;
  if (!IsActiveTabAssociatedToTask() ||
      reason == SidePanelEntryHideReason::kBackgrounded) {
    if (contextual_tasks::IsContextualTasksPinningV2Enabled()) {
      MaybeNotifyVisibilityShouldChange();
    } else {
      UpdatePinnedButtonActiveIndicator();
    }
    return;
  }

  contextual_tasks::ContextualTasksService* service =
      GetContextualTasksService();
  std::optional<contextual_tasks::ContextualTask> current_task =
      service->GetContextualTaskForTab(GetCurrentTabSessionId().value());

  if (!HasPanelResults()) {
    for (SessionID tab_id :
         service->GetTabsAssociatedWithTask(current_task->GetTaskId())) {
      ClearTabEligibility(tab_id);
    }
    MaybeNotifyVisibilityShouldChange();
    return;
  }

  // Mark every tab associated to the task, not just the active one, so the
  // remaining tabs keep the button if the dismissing tab goes away.
  for (SessionID tab_id :
       service->GetTabsAssociatedWithTask(current_task->GetTaskId())) {
    if (!std::ranges::contains(ephemeral_button_eligible_tabs_, tab_id)) {
      ephemeral_button_eligible_tabs_.emplace_back(tab_id);
    }
    StartAutoDismissTimerForTab(tab_id);
  }
  MaybeNotifyVisibilityShouldChange();
}

void ContextualTasksEphemeralButtonController::OnEntryHideCancelled(
    SidePanelEntry* entry) {
  is_hiding_contextual_tasks_panel_ = false;
  is_contextual_tasks_panel_open_ = true;
  if (contextual_tasks::kShowEntryPoint.Get() ==
      contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    CancelAutoDismissTimersForActiveTask();
    MaybeNotifyVisibilityShouldChange();
  } else {
    UpdatePinnedButtonActiveIndicator();
  }
}

void ContextualTasksEphemeralButtonController::OnEntryHidden(
    SidePanelEntry* entry) {
  is_hiding_contextual_tasks_panel_ = false;
  is_contextual_tasks_panel_open_ = false;
  if (contextual_tasks::kShowEntryPoint.Get() ==
      contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    MaybeNotifyVisibilityShouldChange();
  } else {
    UpdatePinnedButtonActiveIndicator();
  }
}

void ContextualTasksEphemeralButtonController::OnActionsChanged() {
  MaybeNotifyVisibilityShouldChange();
}

base::CallbackListSubscription
ContextualTasksEphemeralButtonController::RegisterShouldUpdateButtonVisibility(
    ShouldUpdateVisibilityCallbackList::CallbackType callback) {
  return should_update_visibility_callbacks_.Add(std::move(callback));
}

base::CallbackListSubscription
ContextualTasksEphemeralButtonController::RegisterShouldUpdateButtonPosition(
    ShouldUpdateButtonPositionCallbackList::CallbackType callback) {
  return should_update_position_callbacks_.Add(std::move(callback));
}

void ContextualTasksEphemeralButtonController::OnSidePanelAlignmentChanged() {
  should_update_position_callbacks_.Notify();
  MaybeNotifyVisibilityShouldChange();
}

bool ContextualTasksEphemeralButtonController::ShouldShowEphemeralButton() {
  Profile* const profile = browser_window_interface_->GetProfile();
  if (!profile || profile->IsOffTheRecord()) {
    return false;
  }

  // TabInterface can be null on browser shutdown.
  tabs::TabInterface* const tab_interface =
      browser_window_interface_->GetActiveTabInterface();

  if (!tab_interface) {
    return false;
  }

  const bool is_pinned = contextual_tasks::GetEffectivePinState(
      browser_window_interface_->GetProfile());
  if (!base::FeatureList::IsEnabled(
          contextual_tasks::kEphemeralPinningVisibleWhenPermanentlyPinned) &&
      is_pinned) {
    return false;
  }

  if (contextual_tasks::kShowEntryPoint.Get() ==
          contextual_tasks::EntryPointOption::kToolbarEphemeralBranded &&
      IsContextualTasksPage(tab_interface->GetURL())) {
    return false;
  }

  if (!contextual_tasks::IsContextualTasksUIEnabled()) {
    return false;
  }

  // The trailing edge is only used when the right-hand ephemeral button param
  // is enabled.
  if (!contextual_tasks::ShouldPlaceCircularButtonBeforeProfile(
          browser_window_interface_) &&
      contextual_tasks::IsEphemeralButtonTrailing(browser_window_interface_) &&
      !contextual_tasks::GetEnableRightHandContextualTasksEphemeralButton()) {
    return false;
  }

  // The ephemeral toolbar button should show if the contextual task side panel
  // was shown or closed on this tab, or on any other tab sharing its task.
  bool should_show_button = IsActiveTabEligible();

  if (contextual_tasks::kShowEntryPoint.Get() ==
          contextual_tasks::EntryPointOption::kToolbarEphemeralBranded &&
      (!contextual_tasks::IsContextualTasksPinningV2Enabled() || is_pinned)) {
    should_show_button &= !IsPanelOpen();
  }

  return should_show_button;
}

bool ContextualTasksEphemeralButtonController::
    ShouldShowPinnedButtonActiveIndicator() {
  if (!contextual_tasks::
          IsContextualTasksPinnedButtonActiveIndicatorEnabled() ||
      !contextual_tasks::IsContextualTasksUIEnabled() ||
      !contextual_task_observation_.IsObserving()) {
    return false;
  }

  if (!contextual_tasks::GetEffectivePinState(
          browser_window_interface_->GetProfile())) {
    return false;
  }

  if (contextual_tasks::
          IsContextualTasksPinnedButtonActiveIndicatorCircleHighlightEnabled() &&
      IsPanelOpen()) {
    return false;
  }

  tabs::TabInterface* const tab_interface =
      browser_window_interface_->GetActiveTabInterface();
  if (!tab_interface || IsContextualTasksPage(tab_interface->GetURL())) {
    return false;
  }

  return IsActiveTabEligible();
}

void ContextualTasksEphemeralButtonController::
    RemoveEphemeralButtonForCurrentTask() {
  if (!base::FeatureList::IsEnabled(
          contextual_tasks::kContextualTasksEphemeralButtonContextMenu) &&
      !contextual_tasks::ShouldShowCloseSidePanelInPinnedButtonContextMenu()) {
    return;
  }

  base::RecordAction(base::UserMetricsAction(
      "ContextualTasks.EphemeralToolbarButton.UserAction."
      "RemoveByContextMenu"));
  base::UmaHistogramBoolean(
      "ContextualTasks.EphemeralToolbarButton.UserAction."
      "RemoveByContextMenu",
      true);

  base::AutoReset<std::optional<RemovalReason>> reset_reason(
      &removal_reason_, RemovalReason::kContextMenu);

  std::optional<SessionID> current_tab_session_id = GetCurrentTabSessionId();
  if (current_tab_session_id.has_value()) {
    DismissEphemeralButtonForTab(current_tab_session_id.value());
    return;
  }

  HideTaskSidePanel();
  MaybeNotifyVisibilityShouldChange();
}

bool ContextualTasksEphemeralButtonController::HasAutoDismissTimerForTesting(
    SessionID tab_id) const {
  auto it = ephemeral_button_auto_dismiss_timers_.find(tab_id);
  return it != ephemeral_button_auto_dismiss_timers_.end() &&
         it->second.IsRunning();
}

void ContextualTasksEphemeralButtonController::SetTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  task_runner_for_testing_ = std::move(task_runner);
}

base::TimeDelta
ContextualTasksEphemeralButtonController::GetAutoDismissDuration() const {
  const int minutes =
      contextual_tasks::GetContextualTasksEphemeralButtonAutoDismissTimeMinutes();
  return minutes > 0 ? base::Minutes(minutes) : base::TimeDelta();
}

void ContextualTasksEphemeralButtonController::StartAutoDismissTimerForTab(
    SessionID tab_id) {
  const base::TimeDelta auto_dismiss_duration = GetAutoDismissDuration();
  if (!auto_dismiss_duration.is_positive()) {
    return;
  }
  base::OneShotTimer& timer = ephemeral_button_auto_dismiss_timers_[tab_id];
  if (task_runner_for_testing_) {
    timer.Stop();
    timer.SetTaskRunner(task_runner_for_testing_);
  }
  timer.Start(
      FROM_HERE, auto_dismiss_duration,
      base::BindOnce(
          &ContextualTasksEphemeralButtonController::OnAutoDismissTimerFired,
          base::Unretained(this), tab_id));
}

void ContextualTasksEphemeralButtonController::OnAutoDismissTimerFired(
    SessionID tab_id) {
  base::RecordAction(base::UserMetricsAction(
      "ContextualTasks.EphemeralToolbarButton.AutoDismissed"));
  base::UmaHistogramSparse(
      "ContextualTasks.EphemeralToolbarButton.AutoDismissed",
      GetAutoDismissDuration().InMinutes());
  base::AutoReset<std::optional<RemovalReason>> reset_reason(
      &removal_reason_, RemovalReason::kAutoDismissed);
  DismissEphemeralButtonForTab(tab_id);
}

void ContextualTasksEphemeralButtonController::ClearTabEligibility(
    SessionID tab_id) {
  std::erase(ephemeral_button_eligible_tabs_, tab_id);
  ephemeral_button_auto_dismiss_timers_.erase(tab_id);
}

bool ContextualTasksEphemeralButtonController::IsTaskEligibleForEphemeralButton(
    const base::Uuid& task_id) {
  contextual_tasks::ContextualTasksService* service =
      GetContextualTasksService();
  if (!service) {
    return false;
  }
  return std::ranges::any_of(
      service->GetTabsAssociatedWithTask(task_id), [this](SessionID tab_id) {
        return std::ranges::contains(ephemeral_button_eligible_tabs_, tab_id);
      });
}

bool ContextualTasksEphemeralButtonController::IsActiveTabEligible() {
  // Do not show if the active tab is not associated to a task.
  std::optional<SessionID> current_tab_session_id = GetCurrentTabSessionId();
  if (!current_tab_session_id.has_value()) {
    return false;
  }

  // Do not show if the active tab's task is not eligible for the ephemeral
  // button.
  std::optional<contextual_tasks::ContextualTask> current_task =
      GetContextualTasksService()->GetContextualTaskForTab(
          current_tab_session_id.value());
  if (!current_task.has_value()) {
    return false;
  }

  return IsTaskEligibleForEphemeralButton(current_task->GetTaskId());
}

void ContextualTasksEphemeralButtonController::HideTaskSidePanel() {
  if (auto* ui_service =
          contextual_tasks::ContextualTasksUiServiceFactory::
              GetForBrowserContext(browser_window_interface_->GetProfile())) {
    ui_service->DestroyClosedSidePanel(browser_window_interface_);
  }
  if (contextual_tasks::IsContextualTasksPinningV2Enabled() ||
      contextual_tasks::ShouldShowCloseSidePanelInPinnedButtonContextMenu()) {
    if (auto* panel_controller =
            contextual_tasks::ContextualTasksPanelController::From(
                browser_window_interface_);
        panel_controller && panel_controller->IsPanelOpenForContextualTask()) {
      panel_controller->Close();
    }
  }
}

void ContextualTasksEphemeralButtonController::
    MaybeCleanUpUnusedSidePanelWebContents() {
  // `CleanUpUnusedWebContents()` checks all tabs in the browser window and only
  // destroys cached WebContents for tasks with no remaining associated tabs.
  if (auto* panel_controller =
          contextual_tasks::ContextualTasksPanelController::From(
              browser_window_interface_)) {
    panel_controller->CleanUpUnusedWebContents();
  }
}

void ContextualTasksEphemeralButtonController::DismissEphemeralButtonForTab(
    SessionID tab_id) {
  ClearTabEligibility(tab_id);

  contextual_tasks::ContextualTasksService* service =
      GetContextualTasksService();
  std::optional<contextual_tasks::ContextualTask> task =
      service ? service->GetContextualTaskForTab(tab_id) : std::nullopt;
  if (task.has_value()) {
    for (SessionID associated_tab_id :
         service->GetTabsAssociatedWithTask(task->GetTaskId())) {
      ClearTabEligibility(associated_tab_id);
    }
  }

  if (GetCurrentTabSessionId() == tab_id) {
    HideTaskSidePanel();
  } else if (task.has_value()) {
    service->DisassociateAllTabsFromTask(task->GetTaskId());
    MaybeCleanUpUnusedSidePanelWebContents();
  }

  MaybeNotifyVisibilityShouldChange();
}

void ContextualTasksEphemeralButtonController::
    CancelAutoDismissTimersForActiveTask() {
  std::optional<SessionID> current_tab_session_id = GetCurrentTabSessionId();
  if (!current_tab_session_id.has_value()) {
    return;
  }
  ephemeral_button_auto_dismiss_timers_.erase(current_tab_session_id.value());

  contextual_tasks::ContextualTasksService* service =
      GetContextualTasksService();
  if (!service) {
    return;
  }
  std::optional<contextual_tasks::ContextualTask> current_task =
      service->GetContextualTaskForTab(current_tab_session_id.value());
  if (!current_task.has_value()) {
    return;
  }
  for (SessionID tab_id :
       service->GetTabsAssociatedWithTask(current_task->GetTaskId())) {
    ephemeral_button_auto_dismiss_timers_.erase(tab_id);
  }
}

void ContextualTasksEphemeralButtonController::MarkActiveTaskTabsEligible() {
  if (!IsActiveTabAssociatedToTask()) {
    return;
  }

  contextual_tasks::ContextualTasksService* service =
      GetContextualTasksService();
  std::optional<contextual_tasks::ContextualTask> current_task =
      service->GetContextualTaskForTab(GetCurrentTabSessionId().value());
  if (!current_task.has_value()) {
    return;
  }

  for (SessionID tab_id :
       service->GetTabsAssociatedWithTask(current_task->GetTaskId())) {
    if (!std::ranges::contains(ephemeral_button_eligible_tabs_, tab_id)) {
      ephemeral_button_eligible_tabs_.emplace_back(tab_id);
    }
  }
}

contextual_tasks::ContextualTasksService*
ContextualTasksEphemeralButtonController::GetContextualTasksService() {
  return contextual_tasks::ContextualTasksServiceFactory::GetForProfile(
      browser_window_interface_->GetProfile());
}

std::optional<SessionID>
ContextualTasksEphemeralButtonController::GetCurrentTabSessionId() {
  tabs::TabInterface* const active_tab =
      browser_window_interface_->GetActiveTabInterface();
  if (active_tab) {
    return sessions::SessionTabHelper::IdForTab(active_tab->GetContents());
  } else {
    return std::nullopt;
  }
}

bool ContextualTasksEphemeralButtonController::IsActiveTabAssociatedToTask() {
  std::optional<SessionID> current_tab_session_id = GetCurrentTabSessionId();
  if (!current_tab_session_id.has_value()) {
    return false;
  }

  std::optional<contextual_tasks::ContextualTask> current_task =
      GetContextualTasksService()->GetContextualTaskForTab(
          current_tab_session_id.value());
  return current_task.has_value();
}

bool ContextualTasksEphemeralButtonController::HasPanelResults() const {
  auto* const panel_controller =
      contextual_tasks::ContextualTasksPanelController::From(
          browser_window_interface_);
  return panel_controller && panel_controller->HasResults();
}

void ContextualTasksEphemeralButtonController::OnActiveTabChange(
    BrowserWindowInterface* browser_window_interface) {
  UpdateActiveTabObservation();
  MaybeNotifyVisibilityShouldChange();
}

void ContextualTasksEphemeralButtonController::
    MaybeNotifyVisibilityShouldChange() {
  UpdatePinnedButtonActiveIndicator();
  should_update_visibility_callbacks_.Notify(ShouldShowEphemeralButton());
}

void ContextualTasksEphemeralButtonController::
    UpdatePinnedButtonActiveIndicator() {
  if (!contextual_tasks::
          IsContextualTasksPinnedButtonActiveIndicatorEnabled() ||
      !browser_window_interface_) {
    return;
  }
  const bool should_show = ShouldShowPinnedButtonActiveIndicator();
  if (close_side_panel_action_item_) {
    const bool is_pinned = contextual_tasks::GetEffectivePinState(
        browser_window_interface_->GetProfile());
    const bool should_show_close_side_panel =
        should_show && !is_pinned && !IsPanelOpen();
    close_side_panel_action_item_->SetVisible(should_show_close_side_panel);
    close_side_panel_action_item_->SetEnabled(should_show_close_side_panel);
  }
  BrowserActions* const browser_actions =
      BrowserActions::From(browser_window_interface_);
  if (!browser_actions || !browser_actions->root_action_item()) {
    return;
  }
  actions::ActionItem* const action_item =
      actions::ActionManager::Get().FindAction(
          kActionSidePanelShowContextualTasks,
          browser_actions->root_action_item());
  if (action_item) {
    action_item->SetProperty(kActionItemUnderlineIndicatorKey, should_show);
  }
}

void ContextualTasksEphemeralButtonController::DisassociateNavigatedTab() {
  // Only disassociate the navigated tab when the ephemeral button is eligible
  // to show (e.g., not permanently pinned without
  // `kEphemeralPinningVisibleWhenPermanentlyPinned`), so navigation only
  // dismisses an active ephemeral entry point.
  if (!base::FeatureList::IsEnabled(
          contextual_tasks::kEphemeralPinningVisibleWhenPermanentlyPinned) &&
      contextual_tasks::GetEffectivePinState(
          browser_window_interface_->GetProfile())) {
    return;
  }

  std::optional<SessionID> current_tab_session_id = GetCurrentTabSessionId();
  contextual_tasks::ContextualTasksService* service =
      GetContextualTasksService();
  if (!current_tab_session_id.has_value() || !service) {
    return;
  }

  std::optional<contextual_tasks::ContextualTask> task =
      service->GetContextualTaskForTab(*current_tab_session_id);
  if (!task.has_value() ||
      !IsTaskEligibleForEphemeralButton(task->GetTaskId())) {
    return;
  }

  // Clear synchronously before `MaybeNotifyVisibilityShouldChange()`, since
  // `OnTaskDisassociatedFromTab()` is posted asynchronously by the service.
  ClearTabEligibility(*current_tab_session_id);

  for (SessionID tab_id :
       service->GetTabsAssociatedWithTask(task->GetTaskId())) {
    if (tab_id != *current_tab_session_id &&
        !std::ranges::contains(ephemeral_button_eligible_tabs_, tab_id)) {
      ephemeral_button_eligible_tabs_.emplace_back(tab_id);
      StartAutoDismissTimerForTab(tab_id);
    }
  }
  service->DisassociateTabFromTask(task->GetTaskId(), *current_tab_session_id);
  MaybeCleanUpUnusedSidePanelWebContents();
}

void ContextualTasksEphemeralButtonController::PrimaryPageChanged(
    content::Page& page) {
  if (contextual_tasks::kShowEntryPoint.Get() ==
      contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
    std::optional<base::AutoReset<std::optional<RemovalReason>>> reset_reason;
    if (!IsPanelOpen()) {
      DisassociateNavigatedTab();
      if (!removal_reason_.has_value()) {
        reset_reason.emplace(&removal_reason_,
                             RemovalReason::kTaskDisassociated);
      }
    }
    MaybeNotifyVisibilityShouldChange();
  } else if (contextual_tasks::
                 IsContextualTasksPinnedButtonActiveIndicatorEnabled()) {
    MaybeNotifyVisibilityShouldChange();
  }
}

void ContextualTasksEphemeralButtonController::UpdateActiveTabObservation() {
  if (contextual_tasks::kShowEntryPoint.Get() !=
          contextual_tasks::EntryPointOption::kToolbarEphemeralBranded &&
      !contextual_tasks::
          IsContextualTasksPinnedButtonActiveIndicatorEnabled()) {
    return;
  }
  tabs::TabInterface* const active_tab =
      browser_window_interface_->GetActiveTabInterface();
  if (active_tab) {
    Observe(active_tab->GetContents());
    tab_discard_subscription_ =
        active_tab->RegisterWillDiscardContents(base::BindRepeating(
            &ContextualTasksEphemeralButtonController::OnTabDiscarded,
            base::Unretained(this)));
  } else {
    Observe(nullptr);
    tab_discard_subscription_ = base::CallbackListSubscription();
  }
}

void ContextualTasksEphemeralButtonController::OnTabDiscarded(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  Observe(new_contents);
  MaybeNotifyVisibilityShouldChange();
}
