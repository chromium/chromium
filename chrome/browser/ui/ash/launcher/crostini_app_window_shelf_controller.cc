// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/ash/launcher/crostini_app_window_shelf_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "base/bind.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/crostini/crostini_force_close_watcher.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/app_window_base.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_controller.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/ash/session_controller_client_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_features.h"
#include "components/arc/arc_util.h"
#include "components/exo/shell_surface_util.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/app_window/app_window.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/base/base_window.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace {

void MoveWindowFromOldDisplayToNewDisplay(aura::Window* window,
                                          display::Display& old_display,
                                          display::Display& new_display) {
  // Adjust the window size and origin in proportion to the relative size of the
  // display.
  int old_width = old_display.bounds().width();
  int new_width = new_display.bounds().width();
  int old_height = old_display.bounds().height();
  int new_height = new_display.bounds().height();
  gfx::Rect old_bounds = window->bounds();
  gfx::Rect new_bounds(old_bounds.x() * new_width / old_width,
                       old_bounds.y() * new_height / old_height,
                       old_bounds.width() * new_width / old_width,
                       old_bounds.height() * new_height / old_height);

  // Transform the bounds in display to that in screen.
  gfx::Point new_origin = new_display.bounds().origin();
  new_origin.Offset(new_bounds.x(), new_bounds.y());
  new_bounds.set_origin(new_origin);
  window->SetBoundsInScreen(new_bounds, new_display);
}

}  // namespace

CrostiniAppWindowShelfController::CrostiniAppWindowShelfController(
    ChromeLauncherController* owner)
    : AppWindowLauncherController(owner) {
  aura::Env::GetInstance()->AddObserver(this);
}

CrostiniAppWindowShelfController::~CrostiniAppWindowShelfController() {
  for (auto* window : observed_windows_)
    window->RemoveObserver(this);
  aura::Env::GetInstance()->RemoveObserver(this);
}

void CrostiniAppWindowShelfController::AddToShelf(aura::Window* window,
                                                  AppWindowBase* app_window) {
  window->SetProperty<int>(ash::kShelfItemTypeKey, ash::TYPE_APP);
  ash::ShelfID shelf_id = app_window->shelf_id();
  AppWindowLauncherItemController* item_controller =
      owner()->shelf_model()->GetAppWindowLauncherItemController(shelf_id);
  if (item_controller == nullptr) {
    auto controller =
        std::make_unique<AppWindowLauncherItemController>(shelf_id);
    item_controller = controller.get();
    if (!owner()->GetItem(shelf_id)) {
      owner()->CreateAppLauncherItem(std::move(controller),
                                     ash::STATUS_RUNNING);
    } else {
      owner()->shelf_model()->SetShelfItemDelegate(shelf_id,
                                                   std::move(controller));
      owner()->SetItemStatus(shelf_id, ash::STATUS_RUNNING);
    }
  }

  window->SetProperty(ash::kShelfIDKey, shelf_id.Serialize());
  item_controller->AddWindow(app_window);
  app_window->SetController(item_controller);
}

ash::ShelfID CrostiniAppWindowShelfController::RemoveFromShelf(
    aura::Window* window,
    AppWindowBase* app_window) {
  UnregisterAppWindow(app_window);

  // Check if we may close controller now, at this point we can safely remove
  // controllers without window.
  AppWindowLauncherItemController* item_controller =
      owner()->shelf_model()->GetAppWindowLauncherItemController(
          app_window->shelf_id());

  if (item_controller != nullptr && item_controller->window_count() == 0) {
    ash::ShelfID shelf_id = item_controller->shelf_id();
    owner()->CloseLauncherItem(shelf_id);
    return shelf_id;
  }
  return ash::ShelfID();
}

void CrostiniAppWindowShelfController::ActiveUserChanged(
    const std::string& user_email) {
  for (auto& w : aura_window_to_app_window_) {
    if (MultiUserWindowManagerHelper::GetWindowManager()
            ->GetWindowOwner(w.first)
            .GetUserEmail() == user_email) {
      AddToShelf(w.first, w.second.get());
    } else {
      RemoveFromShelf(w.first, w.second.get());
    }
  }
}

void CrostiniAppWindowShelfController::OnWindowInitialized(
    aura::Window* window) {
  // An Crostini window has type WINDOW_TYPE_NORMAL, a WindowDelegate and
  // is a top level views widget. Tooltips, menus, and other kinds of transient
  // windows that can't activate are filtered out. The transient child is set
  // up after window Init so add it here but remove it later.
  if (window->type() != aura::client::WINDOW_TYPE_NORMAL || !window->delegate())
    return;
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  if (!widget || !widget->is_top_level())
    return;
  if (!widget->CanActivate())
    return;

  observed_windows_.emplace(window);
  window->AddObserver(this);
}

void CrostiniAppWindowShelfController::OnWindowVisibilityChanging(
    aura::Window* window,
    bool visible) {
  if (!visible)
    return;

  // Transient windows are set up after window Init, so remove them here.
  // TODO(timloh): ARC and Plugin VM are similar, but Crostini shouldn't need to
  // know about them.
  if (wm::GetTransientParent(window) ||
      arc::GetWindowTaskId(window) != arc::kNoTaskId ||
      plugin_vm::IsPluginVmWindow(window)) {
    DCHECK(aura_window_to_app_window_.find(window) ==
           aura_window_to_app_window_.end());
    auto it = observed_windows_.find(window);
    DCHECK(it != observed_windows_.end());
    observed_windows_.erase(it);
    window->RemoveObserver(this);
    return;
  }

  // Skip when this window has been handled. This can happen when the window
  // becomes visible again.
  auto app_window_it = aura_window_to_app_window_.find(window);
  if (app_window_it != aura_window_to_app_window_.end())
    return;

  // Handle browser windows, such as the Crostini terminal.
  Browser* browser = chrome::FindBrowserWithWindow(window);
  if (browser) {
    base::Optional<std::string> app_id =
        crostini::CrostiniAppIdFromAppName(browser->app_name());
    if (!app_id)
      return;
    RegisterAppWindow(window, app_id.value());
    return;
  }

  // Currently Crostini can only be used from the primary profile. In the
  // future, this may be replaced by some way of matching the container that
  // runs this app with the user that owns it.
  const AccountId& primary_account_id =
      user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId();

  crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(
          chromeos::ProfileHelper::Get()->GetProfileByAccountId(
              primary_account_id));
  const std::string& shelf_app_id = registry_service->GetCrostiniShelfAppId(
      exo::GetShellApplicationId(window), exo::GetShellStartupId(window));
  // Windows without an application id set will get filtered out here.
  if (shelf_app_id.empty())
    return;

  // At this point, all remaining windows are Crostini windows. Firstly, we add
  // support for forcibly closing it. We use the registration to retrieve the
  // app's name, but this may be null in the case of apps with no associated
  // launcher entry (i.e. no .desktop file), in which case the app's name is
  // unknown.
  base::Optional<crostini::CrostiniRegistryService::Registration> registration =
      registry_service->GetRegistration(shelf_app_id);
  RegisterCrostiniWindowForForceClose(
      window, registration.has_value() ? registration->Name() : "");

  // Failed to uniquely identify the Crostini app that this window is for.
  // The spinners on the shelf have internal app IDs which are valid
  // extensions IDs. If the ID here starts with "crostini:" then it implies
  // that it has failed to identify the exact app that's starting.
  // The existing spinner that fails to be linked back should be closed,
  // otherwise it will be left on the shelf indefinetely until it is closed
  // manually by the user.
  // When the condition is triggered here, the container is up and at least
  // one app is starting. It's safe to close all the spinners since their
  // respective apps take at most another few seconds to start.
  // Work is ongoing to make this occur as infrequently as possible.
  // See https://crbug.com/854911.
  if (base::StartsWith(shelf_app_id, crostini::kCrostiniAppIdPrefix,
                       base::CompareCase::SENSITIVE)) {
    owner()->GetShelfSpinnerController()->CloseCrostiniSpinners();
  }

  // Prevent Crostini window from showing up after user switch.
  MultiUserWindowManagerHelper::GetWindowManager()->SetWindowOwner(
      window, primary_account_id);

  RegisterAppWindow(window, shelf_app_id);

  // Move the Crostini app window to the right display if necessary.
  int64_t display_id = crostini_app_display_.GetDisplayIdForAppId(shelf_app_id);
  if (display_id == display::kInvalidDisplayId)
    return;

  display::Display new_display;
  if (!display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id,
                                                             &new_display))
    return;
  display::Display old_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);

  if (new_display != old_display)
    MoveWindowFromOldDisplayToNewDisplay(window, old_display, new_display);
}

void CrostiniAppWindowShelfController::RegisterAppWindow(
    aura::Window* window,
    const std::string& shelf_app_id) {
  window->SetProperty(aura::client::kAppType,
                      static_cast<int>(ash::AppType::CROSTINI_APP));
  window->SetProperty(ash::kAppIDKey, shelf_app_id);

  const ash::ShelfID shelf_id(shelf_app_id);
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  aura_window_to_app_window_[window] =
      std::make_unique<AppWindowBase>(shelf_id, widget);
  AppWindowBase* app_window = aura_window_to_app_window_[window].get();

  // Only add an app to the shelf if it's associated with the currently active
  // user (which should always be the primary user at this time).
  if (SessionControllerClientImpl::IsMultiProfileAvailable() &&
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId() !=
          MultiUserWindowManagerHelper::GetWindowManager()->GetWindowOwner(
              window))
    return;
  AddToShelf(window, app_window);
}

void CrostiniAppWindowShelfController::OnWindowDestroying(
    aura::Window* window) {
  auto it = observed_windows_.find(window);
  DCHECK(it != observed_windows_.end());
  observed_windows_.erase(it);
  window->RemoveObserver(this);

  auto app_window_it = aura_window_to_app_window_.find(window);
  if (app_window_it == aura_window_to_app_window_.end())
    return;

  ash::ShelfID shelf_id = RemoveFromShelf(window, app_window_it->second.get());
  if (!shelf_id.IsNull()) {
    const std::string& app_id = shelf_id.app_id;
    if (app_id == app_id_to_restart_) {
      crostini::LaunchCrostiniApp(
          ChromeLauncherController::instance()->profile(), app_id,
          display_id_to_restart_in_);
      app_id_to_restart_.clear();
    }
  }

  aura_window_to_app_window_.erase(app_window_it);
}

AppWindowLauncherItemController*
CrostiniAppWindowShelfController::ControllerForWindow(aura::Window* window) {
  if (!window)
    return nullptr;

  auto app_window_it = aura_window_to_app_window_.find(window);
  if (app_window_it == aura_window_to_app_window_.end())
    return nullptr;

  AppWindowBase* app_window = app_window_it->second.get();

  if (app_window == nullptr)
    return nullptr;

  return app_window->controller();
}

void CrostiniAppWindowShelfController::UnregisterAppWindow(
    AppWindowBase* app_window) {
  if (!app_window)
    return;

  AppWindowLauncherItemController* controller = app_window->controller();
  if (controller)
    controller->RemoveWindow(app_window);
  app_window->SetController(nullptr);
}

void CrostiniAppWindowShelfController::RegisterCrostiniWindowForForceClose(
    aura::Window* window,
    const std::string& app_name) {
  if (!base::FeatureList::IsEnabled(features::kCrostiniForceClose))
    return;
  exo::ShellSurfaceBase* surface = exo::GetShellSurfaceBaseForWindow(window);
  if (!surface)
    return;
  crostini::ForceCloseWatcher::Watch(
      std::make_unique<crostini::ShellSurfaceForceCloseDelegate>(surface,
                                                                 app_name));
}

void CrostiniAppWindowShelfController::OnItemDelegateDiscarded(
    ash::ShelfItemDelegate* delegate) {
  for (auto& it : aura_window_to_app_window_) {
    AppWindowBase* app_window = it.second.get();
    if (!app_window || app_window->controller() != delegate)
      continue;

    VLOG(1) << "Item controller was released externally for the app "
            << delegate->shelf_id().app_id << ".";

    UnregisterAppWindow(it.second.get());
  }
}

void CrostiniAppWindowShelfController::OnAppLaunchRequested(
    const std::string& app_id,
    int64_t display_id) {
  crostini_app_display_.Register(app_id, display_id);
}

void CrostiniAppWindowShelfController::Restart(const ash::ShelfID& shelf_id,
                                               int64_t display_id) {
  app_id_to_restart_ = shelf_id.app_id;
  display_id_to_restart_in_ = display_id;
  ChromeLauncherController::instance()->Close(shelf_id);
}
