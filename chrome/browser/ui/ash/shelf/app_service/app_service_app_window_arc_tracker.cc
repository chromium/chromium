// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/app_service/app_service_app_window_arc_tracker.h"

#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/components/arc/arc_util.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_list/arc/intent.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler_factory.h"
#include "chrome/browser/ash/app_restore/arc_ghost_window_handler.h"
#include "chrome/browser/ash/arc/arc_optin_uma.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/ash/shelf/app_service/app_service_app_window_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/app_service/app_service_app_window_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/app_window_base.h"
#include "chrome/browser/ui/ash/shelf/app_window_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/arc_app_window.h"
#include "chrome/browser/ui/ash/shelf/arc_app_window_info.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/window_properties.h"
#include "components/exo/window_properties.h"
#include "extensions/common/constants.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr int kArcAppWindowIconSize = extension_misc::EXTENSION_ICON_MEDIUM;
constexpr char kArcPaymentAppPackage[] = "org.chromium.arc.payment_app";
constexpr char kArcPaymentAppInvokePaymentAppActivity[] =
    "org.chromium.arc.payment_app.InvokePaymentAppActivity";

// Calculates time delta from the current time and reference time encoded into
// |intent| and defined by |param_key|. Returns false if could not be parsed or
// not found. Result is returned in |out|.
bool GetTimeDeltaFromIntent(const arc::Intent& intent,
                            const std::string& param_key,
                            base::TimeDelta* out) {
  std::string time_ms_string;
  if (!intent.GetExtraParamValue(param_key, &time_ms_string))
    return false;

  int64_t time_ms;
  if (!base::StringToInt64(time_ms_string, &time_ms)) {
    LOG(ERROR) << "Failed to parse start time value " << time_ms_string;
    return false;
  }

  *out = (base::TimeTicks::Now() - base::TimeTicks()) -
         base::Milliseconds(time_ms);
  DCHECK_GE(*out, base::TimeDelta());
  return true;
}

void HandlePlayStoreLaunch(const arc::Intent& intent) {
  // Don't track initial Play Store launch. We currently shows Play Store in
  // very rare case in-session provisioning.
  if (intent.HasExtraParam(arc::kInitialStartParam))
    return;

  base::TimeDelta launch_time;
  // This param is injected by |arc:: LaunchAppWithIntent|.
  if (!GetTimeDeltaFromIntent(intent, arc::kRequestStartTimeParamKey,
                              &launch_time)) {
    return;
  }
  arc::UpdatePlayStoreLaunchTime(launch_time);
}

void MaybeHandleDeferredLaunch(const arc::Intent& intent) {
  base::TimeDelta launch_time;
  // This param is injected by |arc:: LaunchAppWithIntent|.
  if (!GetTimeDeltaFromIntent(intent, arc::kRequestDeferredStartTimeParamKey,
                              &launch_time)) {
    return;
  }
  arc::UpdateDeferredLaunchTime(launch_time);
}

}  // namespace

AppServiceAppWindowArcTracker::AppServiceAppWindowArcTracker(
    AppServiceAppWindowShelfController* app_service_controller)
    : observed_profile_(app_service_controller->owner()->profile()),
      app_service_controller_(app_service_controller) {
  DCHECK(observed_profile_);
  DCHECK(app_service_controller_);

  ArcAppListPrefs* const prefs = ArcAppListPrefs::Get(observed_profile_);
  DCHECK(prefs);
  prefs->AddObserver(this);

  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  // arc::ArcSessionManager might not be set in tests.
  if (arc_session_manager)
    arc_session_manager->AddObserver(this);

  auto* arc_handler =
      ash::app_restore::AppRestoreArcTaskHandlerFactory::GetForProfile(
          observed_profile_);
  if (arc_handler)
    arc_handler->OnShelfReady();
}

AppServiceAppWindowArcTracker::~AppServiceAppWindowArcTracker() {
  ArcAppListPrefs* const prefs = ArcAppListPrefs::Get(observed_profile_);
  DCHECK(prefs);
  prefs->RemoveObserver(this);

  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  // arc::ArcSessionManager may be released first.
  if (arc_session_manager)
    arc_session_manager->RemoveObserver(this);
}

void AppServiceAppWindowArcTracker::ActiveUserChanged(
    const std::string& user_email) {
  const std::string& primary_user_email = user_manager::UserManager::Get()
                                              ->GetPrimaryUser()
                                              ->GetAccountId()
                                              .GetUserEmail();
  if (user_email == primary_user_email) {
    // Make sure that we created items for all apps, not only which have a
    // window.
    for (const auto& info : task_id_to_arc_app_window_info_)
      AttachControllerToTask(info.first);

    // Update active status.
    OnTaskSetActive(active_task_id_);
  } else {
    // Some controllers might have no windows attached, for example background
    // task when foreground tasks is in full screen.
    for (const auto& it : app_shelf_group_to_controller_map_) {
      app_service_controller_->owner()->ReplaceWithAppShortcutOrRemove(
          it.second->shelf_id());
    }
    app_shelf_group_to_controller_map_.clear();
  }
}

void AppServiceAppWindowArcTracker::HandleWindowVisibilityChanged(
    aura::Window* window) {
  auto task_or_session_id = arc::GetWindowTaskOrSessionId(window);
  if (!task_or_session_id.has_value() ||
      *task_or_session_id == arc::kSystemWindowTaskId) {
    return;
  }

  // Attach window to multi-user manager now to let it manage visibility state
  // of the ARC window correctly.
  MultiUserWindowManagerHelper::GetWindowManager()->SetWindowOwner(
      window,
      user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId());
}

void AppServiceAppWindowArcTracker::HandleWindowActivatedChanged(
    aura::Window* window) {
  OnTaskSetActive(active_task_id_);
  active_session_id_ = arc::GetWindowSessionId(window).value_or(arc::kNoTaskId);
}

void AppServiceAppWindowArcTracker::HandleWindowDestroying(
    aura::Window* window) {
  app_service_controller_->UnregisterWindow(window);

  // Replace the pointers to the window by nullptr to prevent from using it
  // before OnTaskDestroyed() is called to remove the entry from
  // |task_id_to_arc_app_window_info_|;
  ArcAppWindowInfo* info = GetArcAppWindowInfo(window);
  if (info)
    info->set_window(nullptr);

  auto session_id = arc::GetWindowSessionId(window);
  if (session_id.has_value()) {
    OnSessionDestroyed(*session_id);
    session_id_to_arc_app_window_info_.erase(*session_id);
    if (session_id == active_session_id_)
      active_session_id_ = arc::kNoTaskId;
  }
}

void AppServiceAppWindowArcTracker::CloseWindows(const std::string& app_id) {
  const std::vector<int> task_ids = GetTaskIdsForApp(app_id);
  for (const auto task_id : task_ids)
    arc::CloseTask(task_id);
}

void AppServiceAppWindowArcTracker::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  if (key != exo::kApplicationIdKey || old == 0)
    return;
  const std::string* old_val = reinterpret_cast<std::string*>(old);
  const auto maybe_session_id = arc::GetSessionIdFromWindowAppId(*old_val);
  const auto maybe_task_id = arc::GetWindowTaskId(window);
  if (maybe_session_id.has_value() && maybe_task_id.has_value()) {
    session_id_to_task_id_map_.erase(maybe_session_id.value());
  }
}

void AppServiceAppWindowArcTracker::OnAppStatesChanged(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  if (!app_info.ready)
    OnAppRemoved(app_id);
}

void AppServiceAppWindowArcTracker::OnAppRemoved(const std::string& app_id) {
  const std::vector<int> task_ids_to_remove = GetTaskIdsForApp(app_id);
  for (const auto task_id : task_ids_to_remove)
    OnTaskDestroyed(task_id);
  DCHECK(GetTaskIdsForApp(app_id).empty());

  const std::vector<int> session_ids_to_remove = GetSessionIdsForApp(app_id);
  for (const auto session_id : session_ids_to_remove)
    OnSessionDestroyed(session_id);
  DCHECK(GetSessionIdsForApp(app_id).empty());
}

void AppServiceAppWindowArcTracker::OnTaskCreated(
    int32_t task_id,
    const std::string& package_name,
    const std::string& activity_name,
    const std::string& intent,
    int32_t session_id) {
  base::AutoReset<int> auto_reset(&task_id_being_created_, task_id);

  DCHECK(task_id_to_arc_app_window_info_.find(task_id) ==
         task_id_to_arc_app_window_info_.end());

  // If there is a ghost window for `session_id`, reuse the ghost window info,
  // and clear the ghost window info from `session_id_to_arc_app_window_info_`,
  // and reset `active_session_id_`.
  auto it = session_id_to_arc_app_window_info_.find(session_id);
  if (it != session_id_to_arc_app_window_info_.end()) {
    const auto app_shelf_id = it->second->app_shelf_id();
    task_id_to_arc_app_window_info_[task_id] =
        std::make_unique<ArcAppWindowInfo>(app_shelf_id, intent, package_name);

    session_id_to_task_id_map_[session_id] = task_id;
    task_id_to_arc_app_window_info_[task_id]->set_window(it->second->window());

    auto it_controller = app_shelf_group_to_controller_map_.find(app_shelf_id);
    if (it_controller != app_shelf_group_to_controller_map_.end())
      it_controller->second->RemoveSessionId(it->first);

    session_id_to_arc_app_window_info_.erase(it);
    if (session_id == active_session_id_)
      active_session_id_ = arc::kNoTaskId;
  } else {
    const std::string arc_app_id =
        ArcAppListPrefs::GetAppId(package_name, activity_name);
    const arc::ArcAppShelfId arc_app_shelf_id =
        arc::ArcAppShelfId::FromIntentAndAppId(intent, arc_app_id);
    task_id_to_arc_app_window_info_[task_id] =
        std::make_unique<ArcAppWindowInfo>(arc_app_shelf_id, intent,
                                           package_name);
  }

  // Hide from shelf if there already is some task representing the window.
  if (GetTaskIdSharingLogicalWindow(task_id) != arc::kNoTaskId) {
    task_id_to_arc_app_window_info_[task_id]->set_window_hidden_from_shelf(
        true);
  }

  // Hide any activities created from the ARC Payment activitity from the shelf
  // (they become overlays of TWA apps already on the shelf)
  if ((package_name == kArcPaymentAppPackage) &&
      (activity_name == kArcPaymentAppInvokePaymentAppActivity)) {
    task_id_to_arc_app_window_info_[task_id]->set_task_hidden_from_shelf();
  }

  CheckAndAttachControllers();

  // Some tasks can be started in background and might have no window until
  // pushed to the front. We need its representation on the shelf to give a user
  // control over it.
  AttachControllerToTask(task_id);

  // TODO(crbug.com/40808991): Investigate why `task_id_to_arc_app_window_info_`
  // doesn't have the `task_id` or why it->second is null.
  auto task_id_it = task_id_to_arc_app_window_info_.find(task_id);
  if (task_id_it == task_id_to_arc_app_window_info_.end() ||
      !task_id_it->second) {
    return;
  }

  aura::Window* const window = task_id_it->second->window();
  if (!window)
    return;

  // If we found the window, update AppService InstanceRegistry to add the
  // window information.
  // Update |state|. The app must be started, and running state. If visible,
  // set it as |kVisible|, otherwise, clear the visible bit.
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(observed_profile_);
  apps::InstanceState state = proxy->InstanceRegistry().GetState(window);
  state = static_cast<apps::InstanceState>(
      state | apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  app_service_controller_->app_service_instance_helper()->OnInstances(
      task_id_to_arc_app_window_info_[task_id]->app_shelf_id().app_id(), window,
      std::string(), state);
  arc_window_candidates_.erase(window);
}

void AppServiceAppWindowArcTracker::OnTaskDescriptionChanged(
    int32_t task_id,
    const std::string& label,
    const arc::mojom::RawIconPngData& icon,
    uint32_t primary_color,
    uint32_t status_bar_color) {
  auto it = task_id_to_arc_app_window_info_.find(task_id);
  if (it == task_id_to_arc_app_window_info_.end())
    return;

  // If |icon| is empty, and non-adaptive icon as the default value, don't
  // call ArcRawIconPngDataToImageSkia, because it might return the default
  // play store icon to replace the app icon.
  if (!icon.is_adaptive_icon &&
      (!icon.icon_png_data.has_value() || icon.icon_png_data.value().empty())) {
    return;
  }

  apps::ArcRawIconPngDataToImageSkia(
      icon.Clone(), kArcAppWindowIconSize,
      base::BindOnce(&AppServiceAppWindowArcTracker::OnIconLoaded,
                     weak_ptr_factory_.GetWeakPtr(), task_id, label));
}

void AppServiceAppWindowArcTracker::OnTaskDestroyed(int32_t task_id) {
  // Update crbug.com/1276603 with crash stack if this CHECK fires.
  CHECK_NE(task_id_being_created_, task_id);

  auto it = task_id_to_arc_app_window_info_.find(task_id);
  if (it == task_id_to_arc_app_window_info_.end())
    return;

  if (!it->second->logical_window_id().empty()) {
    const int other_id = GetTaskIdSharingLogicalWindow(task_id);
    if (other_id != arc::kNoTaskId)
      task_id_to_arc_app_window_info_[other_id]->set_window_hidden_from_shelf(
          false);
  }

  aura::Window* const window = it->second.get()->window();
  if (window) {
    // For ARC apps, window may be recreated in some cases, and OnTaskSetActive
    // could be called after the window is destroyed, so controller is not
    // closed on window destroying. Controller will be closed onTaskDestroyed
    // event which is generated when the actual task is destroyed. So when the
    // task is destroyed, delete the instance, otherwise, we might have an
    // instance though the window has been closed, and the task has been
    // destroyed.
    app_service_controller_->app_service_instance_helper()->OnInstances(
        it->second.get()->app_shelf_id().app_id(), window, std::string(),
        apps::InstanceState::kDestroyed);
    app_service_controller_->UnregisterWindow(window);
  }

  // Check if we may close controller now, at this point we can safely remove
  // controllers without window.
  const auto app_shelf_id = it->second->app_shelf_id();
  auto it_controller = app_shelf_group_to_controller_map_.find(app_shelf_id);
  if (it_controller != app_shelf_group_to_controller_map_.end()) {
    it_controller->second->RemoveTaskId(task_id);
    if (!it_controller->second->HasAnyTasks() &&
        !it_controller->second->HasAnySessions()) {
      app_service_controller_->owner()->ReplaceWithAppShortcutOrRemove(
          it_controller->second->shelf_id());
      app_shelf_group_to_controller_map_.erase(app_shelf_id);
    }
  }
  task_id_to_arc_app_window_info_.erase(task_id);
}

void AppServiceAppWindowArcTracker::OnTaskSetActive(int32_t task_id) {
  if (observed_profile_ != app_service_controller_->owner()->profile()) {
    active_task_id_ = task_id;
    return;
  }

  if (task_id == active_task_id_)
    return;

  auto* helper = app_service_controller_->app_service_instance_helper();
  auto it = task_id_to_arc_app_window_info_.find(active_task_id_);
  if (it != task_id_to_arc_app_window_info_.end()) {
    ArcAppWindowInfo* const previous_arc_app_window_info = it->second.get();
    DCHECK(previous_arc_app_window_info);
    app_service_controller_->owner()->SetItemStatus(
        previous_arc_app_window_info->shelf_id(), ash::STATUS_RUNNING);
    auto* window = previous_arc_app_window_info->window();
    AppWindowBase* previous_app_window =
        app_service_controller_->GetAppWindow(window);
    if (previous_app_window) {
      previous_app_window->SetFullscreenMode(
          previous_app_window->widget() &&
                  previous_app_window->widget()->IsFullscreen()
              ? ArcAppWindow::FullScreenMode::kActive
              : ArcAppWindow::FullScreenMode::kNonActive);
    }
    if (window) {
      apps::InstanceState state =
          helper->CalculateActivatedState(window, false /* active */);
      helper->OnInstances(previous_arc_app_window_info->app_shelf_id().app_id(),
                          window, std::string(), state);
    }
  }

  active_task_id_ = task_id;
  it = task_id_to_arc_app_window_info_.find(active_task_id_);
  if (it == task_id_to_arc_app_window_info_.end())
    return;
  ArcAppWindowInfo* const current_arc_app_window_info = it->second.get();
  if (!current_arc_app_window_info || !current_arc_app_window_info->window())
    return;
  aura::Window* const window = current_arc_app_window_info->window();
  views::Widget* const widget = views::Widget::GetWidgetForNativeWindow(window);
  DCHECK(widget);
  if (widget && widget->IsActive()) {
    auto* controller = app_service_controller_->ControllerForWindow(window);
    if (controller)
      controller->SetActiveWindow(window);
  }
  app_service_controller_->owner()->SetItemStatus(
      current_arc_app_window_info->shelf_id(), ash::STATUS_RUNNING);

  apps::InstanceState state =
      helper->CalculateActivatedState(window, true /* active */);
  helper->OnInstances(current_arc_app_window_info->app_shelf_id().app_id(),
                      window, std::string(), state);
}

void AppServiceAppWindowArcTracker::AttachControllerToWindow(
    aura::Window* window) {
  auto task_or_session_id = arc::GetWindowTaskOrSessionId(window);
  if (!task_or_session_id.has_value())
    return;

  // System windows are also arc apps.
  window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::ARC_APP);

  if (*task_or_session_id == arc::kSystemWindowTaskId)
    return;

  ArcAppWindowInfo* const info = GetArcAppWindowInfo(window);
  if (!info)
    return;

  window->SetProperty(ash::kArcPackageNameKey, info->package_name());
  window->SetProperty<int>(ash::kShelfItemTypeKey, ash::TYPE_APP);

  // Check if we have set the AppWindowBase for this task. If it was a session
  // window (ARC ghost window) and replace by real task window, function will
  // returen here.
  if (app_service_controller_->GetAppWindow(window))
    return;

  views::Widget* const widget = views::Widget::GetWidgetForNativeWindow(window);
  DCHECK(widget);
  info->set_window(window);
  const ash::ShelfID shelf_id = info->shelf_id();

  const auto task_id = arc::GetWindowTaskId(window);
  const auto session_id = arc::GetWindowSessionId(window);
  if (task_id.has_value())
    AttachControllerToTask(*task_id);
  else if (session_id.has_value())
    AttachControllerToSession(*session_id, *info);

  if (!info->task_hidden_from_shelf())
    app_service_controller_->AddWindowToShelf(window, shelf_id);
  AppWindowBase* app_window = app_service_controller_->GetAppWindow(window);
  if (app_window)
    app_window->SetDescription(info->title(), info->icon());

  window->SetProperty(ash::kShelfIDKey, shelf_id.Serialize());
  window->SetProperty(ash::kAppIDKey, shelf_id.app_id);
  window->SetProperty(aura::client::kSkipImeProcessing, true);

  if (info->launch_intent().empty())
    return;

  auto intent = arc::Intent::Get(info->launch_intent());
  if (!intent) {
    LOG(ERROR) << "Failed to parse launch intent: " << info->launch_intent();
    return;
  }

  if (info->app_shelf_id().app_id() == arc::kPlayStoreAppId)
    HandlePlayStoreLaunch(*intent);
  MaybeHandleDeferredLaunch(*intent);
}

void AppServiceAppWindowArcTracker::AddCandidateWindow(aura::Window* window) {
  arc_window_candidates_.insert(window);
}

void AppServiceAppWindowArcTracker::RemoveCandidateWindow(
    aura::Window* window) {
  arc_window_candidates_.erase(window);
}

void AppServiceAppWindowArcTracker::OnItemDelegateDiscarded(
    const ash::ShelfID& shelf_id,
    ash::ShelfItemDelegate* delegate) {
  arc::ArcAppShelfId app_shelf_id =
      arc::ArcAppShelfId::FromString(shelf_id.app_id);
  auto it = app_shelf_group_to_controller_map_.find(app_shelf_id);
  if (it != app_shelf_group_to_controller_map_.end() &&
      static_cast<ash::ShelfItemDelegate*>(it->second) == delegate) {
    app_shelf_group_to_controller_map_.erase(it);
  }
}

ash::ShelfID AppServiceAppWindowArcTracker::GetShelfId(aura::Window* window) {
  if (observed_profile_ != app_service_controller_->owner()->profile())
    return ash::ShelfID();

  ArcAppWindowInfo* info = GetArcAppWindowInfo(window);
  if (!info)
    return ash::ShelfID();

  return info->shelf_id();
}

void AppServiceAppWindowArcTracker::CheckAndAttachControllers() {
  for (aura::Window* window : arc_window_candidates_) {
    AttachControllerToWindow(window);
  }
}

void AppServiceAppWindowArcTracker::AttachControllerToTask(int task_id) {
  // TODO(crbug.com/40808991): Investigate why `task_id_to_arc_app_window_info_`
  // doesn't have the `task_id` or why it->second is null.
  auto it = task_id_to_arc_app_window_info_.find(task_id);
  if (it == task_id_to_arc_app_window_info_.end() || !it->second)
    return;

  ArcAppWindowInfo* const app_window_info = it->second.get();
  if (app_window_info->task_hidden_from_shelf())
    return;

  const arc::ArcAppShelfId& app_shelf_id = app_window_info->app_shelf_id();
  if (base::Contains(app_shelf_group_to_controller_map_, app_shelf_id)) {
    app_shelf_group_to_controller_map_[app_shelf_id]->AddTaskId(task_id);
    return;
  }

  const ash::ShelfID shelf_id(app_shelf_id.ToString());
  std::unique_ptr<AppServiceAppWindowShelfItemController> controller =
      std::make_unique<AppServiceAppWindowShelfItemController>(
          shelf_id, app_service_controller_);
  AppServiceAppWindowShelfItemController* item_controller = controller.get();

  if (!app_service_controller_->owner()->GetItem(shelf_id)) {
    app_service_controller_->owner()->CreateAppItem(
        std::move(controller), ash::STATUS_RUNNING, /*pinned=*/false);
  } else {
    app_service_controller_->owner()->shelf_model()->ReplaceShelfItemDelegate(
        shelf_id, std::move(controller));
    app_service_controller_->owner()->SetItemStatus(shelf_id,
                                                    ash::STATUS_RUNNING);
  }
  item_controller->AddTaskId(task_id);
  app_shelf_group_to_controller_map_[app_shelf_id] = item_controller;
}

void AppServiceAppWindowArcTracker::AttachControllerToSession(
    int session_id,
    const ArcAppWindowInfo& app_window_info) {
  // Pass the ArcAppWindowInfo as the parameter, since in some case the window
  // is the task window but still associated with session id due to async issue.
  // In that case, we cannot use the info from the
  // `session_id_to_arc_app_window_info_` directly.
  // TODO(b/274950968): Add a test for this case.
  const arc::ArcAppShelfId& app_shelf_id = app_window_info.app_shelf_id();
  if (base::Contains(app_shelf_group_to_controller_map_, app_shelf_id)) {
    app_shelf_group_to_controller_map_[app_shelf_id]->AddSessionId(session_id);
    return;
  }

  const ash::ShelfID shelf_id(app_shelf_id.ToString());
  std::unique_ptr<AppServiceAppWindowShelfItemController> controller =
      std::make_unique<AppServiceAppWindowShelfItemController>(
          shelf_id, app_service_controller_);
  AppServiceAppWindowShelfItemController* item_controller = controller.get();

  if (!app_service_controller_->owner()->GetItem(shelf_id)) {
    app_service_controller_->owner()->CreateAppItem(
        std::move(controller), ash::STATUS_RUNNING, /*pinned=*/false);
  } else {
    app_service_controller_->owner()->shelf_model()->ReplaceShelfItemDelegate(
        shelf_id, std::move(controller));
    app_service_controller_->owner()->SetItemStatus(shelf_id,
                                                    ash::STATUS_RUNNING);
  }
  item_controller->AddSessionId(session_id);
  app_shelf_group_to_controller_map_[app_shelf_id] = item_controller;
}

void AppServiceAppWindowArcTracker::OnArcPlayStoreEnabledChanged(bool enabled) {
  if (enabled)
    return;

  // If ARC was disabled, close the ghost window.
  std::vector<int> session_ids;
  for (const auto& it : session_id_to_arc_app_window_info_)
    session_ids.push_back(it.first);

  for (const auto session_id : session_ids)
    OnSessionDestroyed(session_id);

  DCHECK(session_id_to_arc_app_window_info_.empty());
}

int AppServiceAppWindowArcTracker::GetTaskIdSharingLogicalWindow(int task_id) {
  auto fixed_it = task_id_to_arc_app_window_info_.find(task_id);
  if (fixed_it == task_id_to_arc_app_window_info_.end())
    return arc::kNoTaskId;
  if (fixed_it->second->logical_window_id().empty())
    return arc::kNoTaskId;
  for (auto it = task_id_to_arc_app_window_info_.begin();
       it != task_id_to_arc_app_window_info_.end(); it++) {
    if (task_id == it->first)
      continue;
    if (fixed_it->second->logical_window_id() ==
            it->second->logical_window_id() &&
        fixed_it->second->shelf_id() == it->second->shelf_id()) {
      return it->first;
    }
  }
  return arc::kNoTaskId;
}

std::vector<int> AppServiceAppWindowArcTracker::GetTaskIdsForApp(
    const std::string& app_id) const {
  std::vector<int> task_ids;
  for (const auto& it : task_id_to_arc_app_window_info_) {
    const ArcAppWindowInfo* app_window_info = it.second.get();
    if (app_window_info->app_shelf_id().app_id() == app_id)
      task_ids.push_back(it.first);
  }

  return task_ids;
}

std::vector<int> AppServiceAppWindowArcTracker::GetSessionIdsForApp(
    const std::string& app_id) const {
  std::vector<int> session_ids;
  for (const auto& it : session_id_to_arc_app_window_info_) {
    const ArcAppWindowInfo* app_window_info = it.second.get();
    if (app_window_info->app_shelf_id().app_id() == app_id)
      session_ids.push_back(it.first);
  }

  return session_ids;
}

void AppServiceAppWindowArcTracker::OnIconLoaded(int32_t task_id,
                                                 const std::string& title,
                                                 const gfx::ImageSkia& icon) {
  auto it = task_id_to_arc_app_window_info_.find(task_id);
  if (it == task_id_to_arc_app_window_info_.end())
    return;

  ArcAppWindowInfo* const info = it->second.get();
  DCHECK(info);
  info->SetDescription(title, icon);
  AppWindowBase* app_window =
      app_service_controller_->GetAppWindow(it->second->window());
  if (app_window)
    app_window->SetDescription(title, icon);
}

ArcAppWindowInfo* AppServiceAppWindowArcTracker::GetArcAppWindowInfo(
    aura::Window* window) {
  const auto task_id = arc::GetWindowTaskId(window);
  if (task_id.has_value()) {
    auto it = task_id_to_arc_app_window_info_.find(*task_id);
    if (it != task_id_to_arc_app_window_info_.end())
      return it->second.get();
  }

  const auto session_id = arc::GetWindowSessionId(window);
  if (!session_id.has_value())
    return nullptr;

  // Since OnTaskCreated is async with corresponding aura window create, so in
  // some cases, the task has beend created but window property in aura window
  // haven't been updated and still kept "session_id". In this case, if the
  // session's task created, just return the latest task info.
  if (base::Contains(session_id_to_task_id_map_, *session_id)) {
    auto mapped_task_id = session_id_to_task_id_map_[*session_id];
    if (base::Contains(task_id_to_arc_app_window_info_, mapped_task_id))
      return task_id_to_arc_app_window_info_[mapped_task_id].get();
  }

  const std::string* arc_app_id = window->GetProperty(app_restore::kAppIdKey);
  if (!arc_app_id || arc_app_id->empty())
    return nullptr;

  auto session_id_it = session_id_to_arc_app_window_info_.find(*session_id);
  if (session_id_it != session_id_to_arc_app_window_info_.end())
    return session_id_it->second.get();

  const arc::ArcAppShelfId arc_app_shelf_id =
      arc::ArcAppShelfId::FromIntentAndAppId(/*intent=*/std::string(),
                                             *arc_app_id);
  session_id_to_arc_app_window_info_[*session_id] =
      std::make_unique<ArcAppWindowInfo>(arc_app_shelf_id,
                                         /*intent=*/std::string(),
                                         /*package_name=*/std::string());

  return session_id_to_arc_app_window_info_[*session_id].get();
}

void AppServiceAppWindowArcTracker::OnSessionDestroyed(int32_t session_id) {
  auto it = session_id_to_arc_app_window_info_.find(session_id);
  if (it == session_id_to_arc_app_window_info_.end())
    return;

  aura::Window* const window = it->second.get()->window();
  if (window) {
    app_service_controller_->app_service_instance_helper()->OnInstances(
        it->second.get()->app_shelf_id().app_id(), window, std::string(),
        apps::InstanceState::kDestroyed);
    app_service_controller_->UnregisterWindow(window);
  }

  // Check if we may close controller now, at this point we can safely remove
  // controllers without window.
  const auto app_shelf_id = it->second->app_shelf_id();
  auto it_controller = app_shelf_group_to_controller_map_.find(app_shelf_id);
  if (it_controller != app_shelf_group_to_controller_map_.end()) {
    it_controller->second->RemoveSessionId(session_id);
    if (!it_controller->second->HasAnyTasks() &&
        !it_controller->second->HasAnySessions()) {
      app_service_controller_->owner()->ReplaceWithAppShortcutOrRemove(
          it_controller->second->shelf_id());
      app_shelf_group_to_controller_map_.erase(app_shelf_id);
    }
  }
  session_id_to_arc_app_window_info_.erase(session_id);

  // Close the ghost window.
  auto* arc_handler =
      ash::app_restore::AppRestoreArcTaskHandlerFactory::GetForProfile(
          observed_profile_);
  if (arc_handler && arc_handler->window_handler())
    arc_handler->window_handler()->CloseWindow(session_id);
}
