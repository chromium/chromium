// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_controller.h"

#include <string>
#include <utility>

#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/window_properties.h"
#include "base/bind.h"
#include "chrome/browser/chromeos/arc/arc_optin_uma.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/user_manager/user_manager.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/base/base_window.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr size_t kMaxIconPngSize = 64 * 1024;  // 64 kb

// Map any ARC Camera app to internal Camera app.
ash::ShelfID MaybeMapShelfId(const arc::ArcAppShelfId& arc_app_shelf_id) {
  if (IsCameraApp(arc_app_shelf_id.app_id()))
    return ash::ShelfID(ash::kInternalAppIdCamera);
  return ash::ShelfID(arc_app_shelf_id.ToString());
}

}  // namespace

// The information about the arc application window which has to be kept
// even when its AppWindow is not present.
class ArcAppWindowLauncherController::AppWindowInfo {
 public:
  explicit AppWindowInfo(const arc::ArcAppShelfId& app_shelf_id,
                         const std::string& launch_intent,
                         const std::string& package_name)
      : app_shelf_id_(app_shelf_id),
        launch_intent_(launch_intent),
        package_name_(package_name) {}
  ~AppWindowInfo() = default;

  void SetDescription(const std::string& title,
                      const std::vector<uint8_t>& icon_data_png) {
    if (base::IsStringUTF8(title))
      title_ = title;
    else
      VLOG(1) << "Task label is not UTF-8 string.";
    // Chrome has custom Play Store icon. Don't overwrite it.
    if (app_shelf_id_.app_id() != arc::kPlayStoreAppId) {
      if (icon_data_png.size() < kMaxIconPngSize)
        icon_data_png_ = icon_data_png;
      else
        VLOG(1) << "Task icon size is too big " << icon_data_png.size() << ".";
    }
  }

  void set_app_window(std::unique_ptr<ArcAppWindow> window) {
    app_window_ = std::move(window);
  }

  const arc::ArcAppShelfId& app_shelf_id() const { return app_shelf_id_; }

  ArcAppWindow* app_window() { return app_window_.get(); }

  const std::string& launch_intent() { return launch_intent_; }

  const std::string& package_name() { return package_name_; }

  const std::string& title() const { return title_; }

  const std::vector<uint8_t>& icon_data_png() const { return icon_data_png_; }

 private:
  const arc::ArcAppShelfId app_shelf_id_;
  const std::string launch_intent_;
  const std::string package_name_;
  // Keeps overridden window title.
  std::string title_;
  // Keeps overridden window icon.
  std::vector<uint8_t> icon_data_png_;
  std::unique_ptr<ArcAppWindow> app_window_;

  DISALLOW_COPY_AND_ASSIGN(AppWindowInfo);
};

ArcAppWindowLauncherController::ArcAppWindowLauncherController(
    ChromeLauncherController* owner)
    : AppWindowLauncherController(owner) {
  if (arc::IsArcAllowedForProfile(owner->profile())) {
    observed_profile_ = owner->profile();
    StartObserving(observed_profile_);

    arc::ArcSessionManager::Get()->AddObserver(this);
  }
}

ArcAppWindowLauncherController::~ArcAppWindowLauncherController() {
  if (observed_profile_)
    StopObserving(observed_profile_);
  if (arc::ArcSessionManager::Get())
    arc::ArcSessionManager::Get()->RemoveObserver(this);
}

void ArcAppWindowLauncherController::ActiveUserChanged(
    const std::string& user_email) {
  const std::string& primary_user_email = user_manager::UserManager::Get()
                                              ->GetPrimaryUser()
                                              ->GetAccountId()
                                              .GetUserEmail();
  if (user_email == primary_user_email) {
    // Restore existing ARC window and create controllers for them.
    AttachControllerToWindowsIfNeeded();

    // Make sure that we created items for all apps, not only which have a
    // window.
    for (const auto& info : task_id_to_app_window_info_)
      AttachControllerToTask(info.first, *info.second);

    // Update active status.
    OnTaskSetActive(active_task_id_);
  } else {
    // Remove all ARC apps and destroy its controllers. There is no mapping
    // task id to app window because it is not safe when controller is missing.
    for (auto& it : task_id_to_app_window_info_)
      UnregisterApp(it.second.get());

    // Some controllers might have no windows attached, for example background
    // task when foreground tasks is in full screen.
    for (const auto& it : app_shelf_group_to_controller_map_)
      owner()->CloseLauncherItem(it.second->shelf_id());
    app_shelf_group_to_controller_map_.clear();
  }
}

void ArcAppWindowLauncherController::AdditionalUserAddedToSession(
    Profile* profile) {
  DCHECK(!arc::IsArcAllowedForProfile(profile));
}

void ArcAppWindowLauncherController::OnWindowInitialized(aura::Window* window) {
  // An arc window has type WINDOW_TYPE_NORMAL, a WindowDelegate and
  // is a top level views widget.
  if (window->type() != aura::client::WINDOW_TYPE_NORMAL || !window->delegate())
    return;
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  if (!widget || !widget->is_top_level())
    return;
  observed_windows_.push_back(window);
  window->AddObserver(this);
}

void ArcAppWindowLauncherController::OnWindowVisibilityChanged(
    aura::Window* window,
    bool visible) {
  const int task_id = arc::GetWindowTaskId(window);
  if (task_id == arc::kNoTaskId)
    return;

  // Attach window to multi-user manager now to let it manage visibility state
  // of the ARC window correctly.
  if (task_id != arc::kSystemWindowTaskId) {
    MultiUserWindowManagerHelper::GetWindowManager()->SetWindowOwner(
        window,
        user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId());
  }

  // The application id property should be set at this time. It is important to
  // have window->IsVisible set to true before attaching to a controller because
  // the window is registered in multi-user manager and this manager may
  // consider this new window as hidden for current profile. Multi-user manager
  // uses OnWindowVisibilityChanging event to update window state.
  if (visible && observed_profile_ == owner()->profile())
    AttachControllerToWindowIfNeeded(window);
}

void ArcAppWindowLauncherController::OnWindowDestroying(aura::Window* window) {
  auto it =
      std::find(observed_windows_.begin(), observed_windows_.end(), window);
  DCHECK(it != observed_windows_.end());
  observed_windows_.erase(it);
  window->RemoveObserver(this);

  auto info_it = std::find_if(
      task_id_to_app_window_info_.begin(), task_id_to_app_window_info_.end(),
      [window](TaskIdToAppWindowInfo::value_type& pair) {
        return pair.second->app_window() &&
               pair.second->app_window()->GetNativeWindow() == window;
      });
  if (info_it != task_id_to_app_window_info_.end()) {
    // Note, window may be recreated in some cases, so do not close controller
    // on window destroying. Controller will be closed onTaskDestroyed event
    // which is generated when actual task is destroyed.
    UnregisterApp(info_it->second.get());
  }
}

ArcAppWindowLauncherController::AppWindowInfo*
ArcAppWindowLauncherController::GetAppWindowInfoForTask(int task_id) {
  const auto it = task_id_to_app_window_info_.find(task_id);
  return it == task_id_to_app_window_info_.end() ? nullptr : it->second.get();
}

ArcAppWindow* ArcAppWindowLauncherController::GetAppWindowForTask(int task_id) {
  AppWindowInfo* info = GetAppWindowInfoForTask(task_id);
  return info ? info->app_window() : nullptr;
}

void ArcAppWindowLauncherController::AttachControllerToWindowsIfNeeded() {
  for (auto* window : observed_windows_)
    AttachControllerToWindowIfNeeded(window);
}

void ArcAppWindowLauncherController::AttachControllerToWindowIfNeeded(
    aura::Window* window) {
  const int task_id = arc::GetWindowTaskId(window);
  if (task_id == arc::kNoTaskId)
    return;

  // System windows are also arc apps.
  window->SetProperty(aura::client::kAppType,
                      static_cast<int>(ash::AppType::ARC_APP));

  if (task_id == arc::kSystemWindowTaskId)
    return;

  // Check if we have controller for this task.
  if (GetAppWindowForTask(task_id))
    return;

  // TODO(msw): Set shelf item types earlier to avoid ShelfWindowWatcher races.
  window->SetProperty<int>(ash::kShelfItemTypeKey, ash::TYPE_APP);

  // Create controller if we have task info.
  AppWindowInfo* info = GetAppWindowInfoForTask(task_id);
  if (!info) {
    VLOG(1) << "Could not find AppWindowInfo for task:" << task_id;
    return;
  }

  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  DCHECK(widget);
  DCHECK(!info->app_window());
  info->set_app_window(std::make_unique<ArcAppWindow>(
      task_id, info->app_shelf_id(), widget, this, observed_profile_));
  info->app_window()->SetDescription(info->title(), info->icon_data_png());
  RegisterApp(info);
  DCHECK(info->app_window()->controller());
  const ash::ShelfID shelf_id(info->app_window()->shelf_id());
  window->SetProperty(ash::kShelfIDKey, shelf_id.Serialize());
  window->SetProperty(ash::kArcPackageNameKey,
                      new std::string(info->package_name()));
  window->SetProperty(ash::kAppIDKey, new std::string(shelf_id.app_id));
}

void ArcAppWindowLauncherController::OnAppStatesChanged(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  if (!app_info.ready)
    OnAppRemoved(app_id);
}

std::vector<int> ArcAppWindowLauncherController::GetTaskIdsForApp(
    const std::string& arc_app_id) const {
  // Note, ArcAppWindow is optional part for a task and it may be not created if
  // another full screen Android app is currently active. Use
  // |task_id_to_app_window_info_| that keeps currently running tasks info.
  std::vector<int> task_ids;
  for (const auto& it : task_id_to_app_window_info_) {
    const AppWindowInfo* app_window_info = it.second.get();
    if (app_window_info->app_shelf_id().app_id() == arc_app_id)
      task_ids.push_back(it.first);
  }

  return task_ids;
}

void ArcAppWindowLauncherController::OnAppRemoved(
    const std::string& arc_app_id) {
  const std::vector<int> task_ids_to_remove = GetTaskIdsForApp(arc_app_id);
  for (const auto task_id : task_ids_to_remove)
    OnTaskDestroyed(task_id);
  DCHECK(GetTaskIdsForApp(arc_app_id).empty());
}

void ArcAppWindowLauncherController::OnTaskCreated(
    int task_id,
    const std::string& package_name,
    const std::string& activity_name,
    const std::string& intent) {
  DCHECK(!GetAppWindowForTask(task_id));
  const std::string arc_app_id =
      ArcAppListPrefs::GetAppId(package_name, activity_name);
  const arc::ArcAppShelfId arc_app_shelf_id =
      arc::ArcAppShelfId::FromIntentAndAppId(intent, arc_app_id);
  task_id_to_app_window_info_[task_id] =
      std::make_unique<AppWindowInfo>(arc_app_shelf_id, intent, package_name);
  // Don't create shelf icon for non-primary user.
  if (observed_profile_ != owner()->profile())
    return;

  AttachControllerToWindowsIfNeeded();

  // Some tasks can be started in background and might have no window until
  // pushed to the front. We need its representation on the shelf to give a user
  // control over it.
  AttachControllerToTask(task_id, *task_id_to_app_window_info_[task_id]);
}

void ArcAppWindowLauncherController::OnTaskDescriptionUpdated(
    int32_t task_id,
    const std::string& label,
    const std::vector<uint8_t>& icon_png_data) {
  AppWindowInfo* info = GetAppWindowInfoForTask(task_id);
  if (info) {
    info->SetDescription(label, icon_png_data);
    if (info->app_window())
      info->app_window()->SetDescription(label, icon_png_data);
  }
}

void ArcAppWindowLauncherController::OnTaskDestroyed(int task_id) {
  auto it = task_id_to_app_window_info_.find(task_id);
  if (it == task_id_to_app_window_info_.end())
    return;

  UnregisterApp(it->second.get());

  // Check if we may close controller now, at this point we can safely remove
  // controllers without window.
  auto it_controller =
      app_shelf_group_to_controller_map_.find(it->second->app_shelf_id());
  if (it_controller != app_shelf_group_to_controller_map_.end()) {
    it_controller->second->RemoveTaskId(task_id);
    if (!it_controller->second->HasAnyTasks()) {
      owner()->CloseLauncherItem(it_controller->second->shelf_id());
      app_shelf_group_to_controller_map_.erase(it_controller);
    }
  }

  task_id_to_app_window_info_.erase(it);
}

void ArcAppWindowLauncherController::OnTaskSetActive(int32_t task_id) {
  if (observed_profile_ != owner()->profile()) {
    active_task_id_ = task_id;
    return;
  }

  ArcAppWindow* previous_app_window = GetAppWindowForTask(active_task_id_);
  if (previous_app_window) {
    owner()->SetItemStatus(previous_app_window->shelf_id(),
                           ash::STATUS_RUNNING);
    previous_app_window->SetFullscreenMode(
        previous_app_window->widget() &&
                previous_app_window->widget()->IsFullscreen()
            ? ArcAppWindow::FullScreenMode::ACTIVE
            : ArcAppWindow::FullScreenMode::NON_ACTIVE);
  }

  active_task_id_ = task_id;

  ArcAppWindow* current_app_window = GetAppWindowForTask(task_id);
  if (current_app_window) {
    if (current_app_window->widget() && current_app_window->IsActive()) {
      current_app_window->controller()->SetActiveWindow(
          current_app_window->GetNativeWindow());
    }
    owner()->SetItemStatus(current_app_window->shelf_id(), ash::STATUS_RUNNING);
    // TODO(reveman): Figure out how to support fullscreen in interleaved
    // window mode.
    // if (new_active_app_it->second->widget()) {
    //   new_active_app_it->second->widget()->SetFullscreen(
    //       new_active_app_it->second->fullscreen_mode() ==
    //       ArcAppWindow::FullScreenMode::ACTIVE);
    // }
  }
}

AppWindowLauncherItemController*
ArcAppWindowLauncherController::ControllerForWindow(aura::Window* window) {
  if (!window)
    return nullptr;

  ArcAppWindow* app_window = GetAppWindowForTask(active_task_id_);
  if (app_window &&
      app_window->widget() == views::Widget::GetWidgetForNativeWindow(window)) {
    return app_window->controller();
  }

  for (auto& it : task_id_to_app_window_info_) {
    ArcAppWindow* app_window = it.second->app_window();
    if (app_window && app_window->widget() ==
                          views::Widget::GetWidgetForNativeWindow(window)) {
      return it.second->app_window()->controller();
    }
  }

  return nullptr;
}

void ArcAppWindowLauncherController::OnItemDelegateDiscarded(
    ash::ShelfItemDelegate* delegate) {
  for (auto& it : task_id_to_app_window_info_) {
    ArcAppWindow* app_window = it.second->app_window();
    if (!app_window || app_window->controller() != delegate)
      continue;

    VLOG(1) << "Item controller was released externally for the app "
            << delegate->shelf_id().app_id << ".";

    auto it_controller =
        app_shelf_group_to_controller_map_.find(app_window->app_shelf_id());
    if (it_controller != app_shelf_group_to_controller_map_.end())
      app_shelf_group_to_controller_map_.erase(it_controller);

    UnregisterApp(it.second.get());
  }
}

void ArcAppWindowLauncherController::OnArcOptInManagementCheckStarted() {
  // In case of retry this time is updated and we measure only successful run.
  opt_in_management_check_start_time_ = base::Time::Now();
}

void ArcAppWindowLauncherController::OnArcSessionStopped(
    arc::ArcStopReason stop_reason) {
  opt_in_management_check_start_time_ = base::Time();
}

void ArcAppWindowLauncherController::OnWindowActivated(
    wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  AppWindowLauncherController::OnWindowActivated(reason, gained_active,
                                                 lost_active);
  OnTaskSetActive(active_task_id_);
}

void ArcAppWindowLauncherController::StartObserving(Profile* profile) {
  aura::Env::GetInstance()->AddObserver(this);
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile);
  DCHECK(prefs);
  prefs->AddObserver(this);
}

void ArcAppWindowLauncherController::StopObserving(Profile* profile) {
  for (auto* window : observed_windows_)
    window->RemoveObserver(this);
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile);
  prefs->RemoveObserver(this);
  aura::Env::GetInstance()->RemoveObserver(this);
}

ArcAppWindowLauncherItemController*
ArcAppWindowLauncherController::AttachControllerToTask(
    int task_id,
    const AppWindowInfo& app_window_info) {
  const arc::ArcAppShelfId& app_shelf_id = app_window_info.app_shelf_id();
  const auto it = app_shelf_group_to_controller_map_.find(app_shelf_id);
  if (it != app_shelf_group_to_controller_map_.end()) {
    DCHECK(IsCameraApp(app_shelf_id.ToString()) ||
           it->second->app_id() == app_shelf_id.ToString());
    it->second->AddTaskId(task_id);
    return it->second;
  }

  const ash::ShelfID shelf_id = MaybeMapShelfId(app_shelf_id);
  std::unique_ptr<ArcAppWindowLauncherItemController> controller =
      std::make_unique<ArcAppWindowLauncherItemController>(shelf_id);
  ArcAppWindowLauncherItemController* item_controller = controller.get();
  if (!owner()->GetItem(shelf_id)) {
    owner()->CreateAppLauncherItem(std::move(controller), ash::STATUS_RUNNING);
  } else {
    owner()->shelf_model()->SetShelfItemDelegate(shelf_id,
                                                 std::move(controller));
    owner()->SetItemStatus(shelf_id, ash::STATUS_RUNNING);
  }
  item_controller->AddTaskId(task_id);
  app_shelf_group_to_controller_map_[app_shelf_id] = item_controller;
  return item_controller;
}

void ArcAppWindowLauncherController::RegisterApp(
    AppWindowInfo* app_window_info) {
  ArcAppWindow* app_window = app_window_info->app_window();
  ArcAppWindowLauncherItemController* controller =
      AttachControllerToTask(app_window->task_id(), *app_window_info);
  DCHECK(!controller->app_id().empty());
  const ash::ShelfID shelf_id(controller->app_id());
  DCHECK(owner()->GetItem(shelf_id));

  controller->AddWindow(app_window);
  owner()->SetItemStatus(shelf_id, ash::STATUS_RUNNING);
  app_window->SetController(controller);
  app_window->set_shelf_id(shelf_id);

  if (app_window_info->app_shelf_id().app_id() == arc::kPlayStoreAppId)
    HandlePlayStoreLaunch(app_window_info);
}

void ArcAppWindowLauncherController::UnregisterApp(
    AppWindowInfo* app_window_info) {
  ArcAppWindow* app_window = app_window_info->app_window();
  if (!app_window)
    return;

  AppWindowLauncherItemController* controller = app_window->controller();
  if (controller)
    controller->RemoveWindow(app_window);
  app_window->SetController(nullptr);
  app_window_info->set_app_window(nullptr);
}

void ArcAppWindowLauncherController::HandlePlayStoreLaunch(
    AppWindowInfo* app_window_info) {
  arc::Intent intent;
  if (!arc::ParseIntent(app_window_info->launch_intent(), &intent))
    return;

  if (!opt_in_management_check_start_time_.is_null()) {
    if (intent.HasExtraParam(arc::kInitialStartParam)) {
      DCHECK(!arc::IsRobotOrOfflineDemoAccountMode());
      arc::UpdatePlayStoreShownTimeDeprecated(
          base::Time::Now() - opt_in_management_check_start_time_,
          owner()->profile());
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
