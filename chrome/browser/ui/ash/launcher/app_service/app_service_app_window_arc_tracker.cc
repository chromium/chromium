// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_service/app_service_app_window_arc_tracker.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "base/feature_list.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/arc/arc_optin_uma.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/app_service/app_service_app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/app_service/app_service_app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/app_window_base.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window_info.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/common/chrome_features.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr int kArcAppWindowIconSize = extension_misc::EXTENSION_ICON_MEDIUM;
}  // namespace

AppServiceAppWindowArcTracker::AppServiceAppWindowArcTracker(
    AppServiceAppWindowLauncherController* app_service_controller)
    : observed_profile_(app_service_controller->owner()->profile()),
      app_service_controller_(app_service_controller) {
  DCHECK(observed_profile_);
  DCHECK(app_service_controller_);

  ArcAppListPrefs* const prefs = ArcAppListPrefs::Get(observed_profile_);
  DCHECK(prefs);
  prefs->AddObserver(this);
}

AppServiceAppWindowArcTracker::~AppServiceAppWindowArcTracker() {
  ArcAppListPrefs* const prefs = ArcAppListPrefs::Get(observed_profile_);
  DCHECK(prefs);
  prefs->RemoveObserver(this);
  observed_windows_.RemoveAll();
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
    for (const auto& it : app_shelf_group_to_controller_map_)
      app_service_controller_->owner()->CloseLauncherItem(
          it.second->shelf_id());
    app_shelf_group_to_controller_map_.clear();
  }
}

void AppServiceAppWindowArcTracker::HandleWindowVisibilityChanged(
    aura::Window* window) {
  const int task_id = arc::GetWindowTaskId(window);
  if (task_id == arc::kNoTaskId || task_id == arc::kSystemWindowTaskId)
    return;

  // Attach window to multi-user manager now to let it manage visibility state
  // of the ARC window correctly.
  MultiUserWindowManagerHelper::GetWindowManager()->SetWindowOwner(
      window,
      user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId());
}

void AppServiceAppWindowArcTracker::HandleWindowDestroying(
    aura::Window* window) {
  app_service_controller_->UnregisterWindow(window);
  // Replace the pointers to the window by nullptr to prevent from using it
  // before OnTaskDestroyed() is called to remove the entry from
  // |task_id_to_arc_app_window_info_|;
  const int task_id = arc::GetWindowTaskId(window);
  auto it = task_id_to_arc_app_window_info_.find(task_id);
  if (it != task_id_to_arc_app_window_info_.end())
    it->second->set_window(nullptr);

  if (observed_windows_.IsObserving(window))
    observed_windows_.Remove(window);
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
}

void AppServiceAppWindowArcTracker::OnTaskCreated(
    int32_t task_id,
    const std::string& package_name,
    const std::string& activity_name,
    const std::string& intent,
    int32_t session_id) {
  DCHECK(task_id_to_arc_app_window_info_.find(task_id) ==
         task_id_to_arc_app_window_info_.end());

  const std::string arc_app_id =
      ArcAppListPrefs::GetAppId(package_name, activity_name);
  const arc::ArcAppShelfId arc_app_shelf_id =
      arc::ArcAppShelfId::FromIntentAndAppId(intent, arc_app_id);
  task_id_to_arc_app_window_info_[task_id] = std::make_unique<ArcAppWindowInfo>(
      arc_app_shelf_id, intent, package_name);
  // Hide from shelf if there already is some task representing the window.
  if (GetTaskIdSharingLogicalWindow(task_id) != arc::kNoTaskId)
    task_id_to_arc_app_window_info_[task_id]->set_hidden_from_shelf(true);

  CheckAndAttachControllers();

  // Some tasks can be started in background and might have no window until
  // pushed to the front. We need its representation on the shelf to give a user
  // control over it.
  AttachControllerToTask(task_id);

  aura::Window* const window =
      task_id_to_arc_app_window_info_[task_id]->window();
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
    const arc::mojom::RawIconPngData& icon) {
  auto it = task_id_to_arc_app_window_info_.find(task_id);
  if (it == task_id_to_arc_app_window_info_.end())
    return;

  if (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon) ||
      icon.icon_png_data.has_value()) {
    // If |icon| is empty, and non-adaptive icon as the default value, don't
    // call ArcRawIconPngDataToImageSkia, because it might return the default
    // play store icon to replace the app icon.
    if (!icon.is_adaptive_icon && (!icon.icon_png_data.has_value() ||
                                   icon.icon_png_data.value().empty())) {
      return;
    }

    apps::ArcRawIconPngDataToImageSkia(
        icon.Clone(), kArcAppWindowIconSize,
        base::BindOnce(&AppServiceAppWindowArcTracker::OnIconLoaded,
                       weak_ptr_factory_.GetWeakPtr(), task_id, label));
  }
}

void AppServiceAppWindowArcTracker::OnTaskDestroyed(int32_t task_id) {
  auto it = task_id_to_arc_app_window_info_.find(task_id);
  if (it == task_id_to_arc_app_window_info_.end())
    return;

  if (!it->second->logical_window_id().empty()) {
    const int other_id = GetTaskIdSharingLogicalWindow(task_id);
    if (other_id != arc::kNoTaskId)
      task_id_to_arc_app_window_info_[other_id]->set_hidden_from_shelf(false);
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
  auto it_controller =
      app_shelf_group_to_controller_map_.find(app_shelf_id);
  if (it_controller != app_shelf_group_to_controller_map_.end()) {
    it_controller->second->RemoveTaskId(task_id);
    if (!it_controller->second->HasAnyTasks()) {
      app_service_controller_->owner()->CloseLauncherItem(
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

  auto it = task_id_to_arc_app_window_info_.find(active_task_id_);
  if (it != task_id_to_arc_app_window_info_.end()) {
    ArcAppWindowInfo* const previous_arc_app_window_info = it->second.get();
    DCHECK(previous_arc_app_window_info);
    app_service_controller_->owner()->SetItemStatus(
        previous_arc_app_window_info->shelf_id(), ash::STATUS_RUNNING);
    AppWindowBase* previous_app_window = app_service_controller_->GetAppWindow(
        previous_arc_app_window_info->window());
    if (previous_app_window) {
      previous_app_window->SetFullscreenMode(
          previous_app_window->widget() &&
                  previous_app_window->widget()->IsFullscreen()
              ? ArcAppWindow::FullScreenMode::kActive
              : ArcAppWindow::FullScreenMode::kNonActive);
    }
    if (previous_arc_app_window_info->window()) {
      apps::InstanceState state =
          app_service_controller_->app_service_instance_helper()
              ->CalculateActivatedState(previous_arc_app_window_info->window(),
                                        false /* active */);
      app_service_controller_->app_service_instance_helper()->OnInstances(
          previous_arc_app_window_info->app_shelf_id().app_id(),
          previous_arc_app_window_info->window(), std::string(), state);
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
      app_service_controller_->app_service_instance_helper()
          ->CalculateActivatedState(window, true /* active */);
  app_service_controller_->app_service_instance_helper()->OnInstances(
      current_arc_app_window_info->app_shelf_id().app_id(), window,
      std::string(), state);
}

void AppServiceAppWindowArcTracker::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  if (key != ash::kArcResizeLockKey)
    return;
  const auto new_resize_lock_state =
      window->GetProperty(ash::kArcResizeLockKey);
  const auto* app_id = window->GetProperty(ash::kAppIDKey);
  if (!app_id)
    return;
  ArcAppListPrefs* const prefs = ArcAppListPrefs::Get(observed_profile_);
  DCHECK(prefs);
  const auto current_resize_lock_state = prefs->GetResizeLockState(*app_id);
  if (new_resize_lock_state &&
      current_resize_lock_state == arc::mojom::ArcResizeLockState::READY) {
    prefs->SetResizeLockState(*app_id, arc::mojom::ArcResizeLockState::ON);
    // TODO(b/180253004): Show the splash screen.
  }
}

void AppServiceAppWindowArcTracker::AttachControllerToWindow(
    aura::Window* window) {
  const int task_id = arc::GetWindowTaskId(window);
  if (task_id == arc::kNoTaskId)
    return;

  // System windows are also arc apps.
  window->SetProperty(aura::client::kAppType,
                      static_cast<int>(ash::AppType::ARC_APP));

  if (task_id == arc::kSystemWindowTaskId)
    return;

  auto it = task_id_to_arc_app_window_info_.find(task_id);
  if (it == task_id_to_arc_app_window_info_.end())
    return;

  window->SetProperty<int>(ash::kShelfItemTypeKey, ash::TYPE_APP);

  ArcAppWindowInfo* const info = it->second.get();
  DCHECK(info);

  // Check if we have set the AppWindowBase for this task.
  if (app_service_controller_->GetAppWindow(window))
    return;

  views::Widget* const widget = views::Widget::GetWidgetForNativeWindow(window);
  DCHECK(widget);
  info->set_window(window);
  const ash::ShelfID shelf_id = info->shelf_id();
  AttachControllerToTask(task_id);
  app_service_controller_->AddWindowToShelf(window, shelf_id);
  AppWindowBase* app_window = app_service_controller_->GetAppWindow(window);
  if (app_window)
    app_window->SetDescription(info->title(), info->icon());

  window->SetProperty(ash::kShelfIDKey, shelf_id.Serialize());
  window->SetProperty(ash::kArcPackageNameKey,
                      new std::string(info->package_name()));
  window->SetProperty(ash::kAppIDKey, new std::string(shelf_id.app_id));
  if (base::FeatureList::IsEnabled(
          chromeos::features::kArcPreImeKeyEventSupport)) {
    window->SetProperty(aura::client::kSkipImeProcessing, true);
  }
  observed_windows_.Add(window);

  if (info->app_shelf_id().app_id() == arc::kPlayStoreAppId)
    HandlePlayStoreLaunch(info);
}

void AppServiceAppWindowArcTracker::AddCandidateWindow(aura::Window* window) {
  arc_window_candidates_.insert(window);
}

void AppServiceAppWindowArcTracker::RemoveCandidateWindow(
    aura::Window* window) {
  arc_window_candidates_.erase(window);
  if (observed_windows_.IsObserving(window))
    observed_windows_.Remove(window);
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

ash::ShelfID AppServiceAppWindowArcTracker::GetShelfId(int task_id) const {
  if (observed_profile_ != app_service_controller_->owner()->profile())
    return ash::ShelfID();

  const auto it = task_id_to_arc_app_window_info_.find(task_id);
  if (it == task_id_to_arc_app_window_info_.end())
    return ash::ShelfID();

  return it->second->shelf_id();
}

void AppServiceAppWindowArcTracker::CheckAndAttachControllers() {
  for (auto* window : arc_window_candidates_)
    AttachControllerToWindow(window);
}

void AppServiceAppWindowArcTracker::AttachControllerToTask(int task_id) {
  ArcAppWindowInfo* const app_window_info =
      task_id_to_arc_app_window_info_[task_id].get();
  const arc::ArcAppShelfId& app_shelf_id = app_window_info->app_shelf_id();
  if (base::Contains(app_shelf_group_to_controller_map_, app_shelf_id)) {
    app_shelf_group_to_controller_map_[app_shelf_id]->AddTaskId(task_id);
    return;
  }

  const ash::ShelfID shelf_id(app_shelf_id.ToString());
  std::unique_ptr<AppServiceAppWindowLauncherItemController> controller =
      std::make_unique<AppServiceAppWindowLauncherItemController>(
          shelf_id, app_service_controller_);
  AppServiceAppWindowLauncherItemController* item_controller = controller.get();

  if (!app_service_controller_->owner()->GetItem(shelf_id)) {
    app_service_controller_->owner()->CreateAppLauncherItem(
        std::move(controller), ash::STATUS_RUNNING);
  } else {
    app_service_controller_->owner()->shelf_model()->SetShelfItemDelegate(
        shelf_id, std::move(controller));
    app_service_controller_->owner()->SetItemStatus(shelf_id,
                                                    ash::STATUS_RUNNING);
  }
  item_controller->AddTaskId(task_id);
  app_shelf_group_to_controller_map_[app_shelf_id] = item_controller;
}

void AppServiceAppWindowArcTracker::OnArcOptInManagementCheckStarted() {
  // In case of retry this time is updated and we measure only successful run.
  opt_in_management_check_start_time_ = base::Time::Now();
}

void AppServiceAppWindowArcTracker::OnArcSessionStopped(
    arc::ArcStopReason stop_reason) {
  opt_in_management_check_start_time_ = base::Time();
}

void AppServiceAppWindowArcTracker::HandlePlayStoreLaunch(
    ArcAppWindowInfo* app_window_info) {
  arc::Intent intent;
  if (!arc::ParseIntent(app_window_info->launch_intent(), &intent))
    return;

  if (!opt_in_management_check_start_time_.is_null()) {
    if (intent.HasExtraParam(arc::kInitialStartParam)) {
      DCHECK(!arc::IsRobotOrOfflineDemoAccountMode());
      arc::UpdatePlayStoreShownTimeDeprecated(
          base::Time::Now() - opt_in_management_check_start_time_,
          app_service_controller_->owner()->profile());
      VLOG(1) << "Play Store is initially shown.";
    }
    opt_in_management_check_start_time_ = base::Time();
    return;
  }

  for (const auto& param : intent.extra_params()) {
    int64_t start_request_ms;
    if (sscanf(param.c_str(), arc::kRequestStartTimeParamTemplate,
               &start_request_ms) != 1)
      continue;
    const base::TimeDelta launch_time =
        base::TimeTicks::Now() - base::TimeTicks() -
        base::TimeDelta::FromMilliseconds(start_request_ms);
    DCHECK_GE(launch_time, base::TimeDelta());
    arc::UpdatePlayStoreLaunchTime(launch_time);
  }
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

void AppServiceAppWindowArcTracker::SetDescription(int32_t task_id,
                                                   const std::string& title,
                                                   gfx::ImageSkia icon) {
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

void AppServiceAppWindowArcTracker::OnIconLoaded(int32_t task_id,
                                                 const std::string& title,
                                                 const gfx::ImageSkia& icon) {
  gfx::ImageSkia image = icon;
  SetDescription(task_id, title, image);
}
