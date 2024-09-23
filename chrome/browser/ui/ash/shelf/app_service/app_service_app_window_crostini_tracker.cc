// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/app_service/app_service_app_window_crostini_tracker.h"

#include "ash/components/arc/arc_util.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/wm/window_state.h"
#include "base/containers/flat_tree.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_force_close_watcher.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_shelf_utils.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/ash/shelf/app_service/app_service_app_window_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/app_service/app_service_app_window_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/app_window_base.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/borealis/borealis_util.h"
#include "components/exo/permission.h"
#include "components/exo/shell_surface_util.h"
#include "components/user_manager/user_manager.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/wm/core/window_util.h"

namespace {

// Time allowed for apps to self-activate after launch, see
// go/crostini-self-activate for details.
constexpr base::TimeDelta kSelfActivationTimeout = base::Seconds(5);

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

// Returns true if the crostini tracker should ignore this window. Mainly used
// to exclude other windows that are created by exo. Transient windows may
// actually belong to crostini but we exclude them as well as their IDs will be
// set at a later point.
bool ShouldSkipWindow(aura::Window* window) {
  return wm::GetTransientParent(window) ||
         arc::GetWindowTaskOrSessionId(window).has_value() ||
         crosapi::browser_util::IsLacrosWindow(window) ||
         plugin_vm::IsPluginVmAppWindow(window) ||
         ash::borealis::IsBorealisWindow(window);
}

}  // namespace

AppServiceAppWindowCrostiniTracker::AppServiceAppWindowCrostiniTracker(
    AppServiceAppWindowShelfController* app_service_controller)
    : app_service_controller_(app_service_controller) {}

AppServiceAppWindowCrostiniTracker::~AppServiceAppWindowCrostiniTracker() =
    default;

void AppServiceAppWindowCrostiniTracker::OnWindowVisibilityChanged(
    aura::Window* window,
    const std::string& shelf_app_id) {
  if (ShouldSkipWindow(window))
    return;

  // Handle browser windows.
  Browser* browser = chrome::FindBrowserWithWindow(window);
  if (browser)
    return;

  // Currently Crostini can only be used from the primary profile. In the
  // future, this may be replaced by some way of matching the container that
  // runs this app with the user that owns it.
  const AccountId& primary_account_id =
      user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId();

  Profile* primary_account_profile =
      ash::ProfileHelper::Get()->GetProfileByAccountId(primary_account_id);

  // Windows without an application id set will get filtered out here.
  const std::string& crostini_shelf_app_id = guest_os::GetGuestOsShelfAppId(
      primary_account_profile, exo::GetShellApplicationId(window),
      exo::GetShellStartupId(window));
  if (crostini_shelf_app_id.empty())
    return;

  auto* registry_service =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(
          primary_account_profile);

  // At this point, all remaining windows are Crostini windows. Firstly, we add
  // support for forcibly closing it. We use the registration to retrieve the
  // app's name, but this may be null in the case of apps with no associated
  // launcher entry (i.e. no .desktop file), in which case the app's name is
  // unknown.
  std::optional<guest_os::GuestOsRegistryService::Registration> registration =
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
  if (guest_os::IsUnregisteredCrostiniShelfAppId(shelf_app_id)) {
    ChromeShelfController::instance()
        ->GetShelfSpinnerController()
        ->CloseCrostiniSpinners();
  }

  // Prevent Crostini window from showing up after user switch.
  MultiUserWindowManagerHelper::GetWindowManager()->SetWindowOwner(
      window, primary_account_id);

  // Move the Crostini app window to the right display if necessary.
  int64_t display_id = crostini_app_display_.GetDisplayIdForAppId(shelf_app_id);
  if (display_id == display::kInvalidDisplayId)
    return;

  display::Display registered_display;
  if (!display::Screen::GetScreen()->GetDisplayWithDisplayId(
          display_id, &registered_display)) {
    return;
  }

  display::Display current_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);

  if (registered_display != current_display) {
    auto* state = ash::WindowState::Get(window);
    // 'window' is about to unminimize.
    if (state && !state->IsMinimized()) {
      MoveWindowFromOldDisplayToNewDisplay(window, current_display,
                                           registered_display);
      // 'window' is about to minimize. Therefore we take this opportunity to
      // re-register it to the current display it is shown upon.
    } else {
      crostini_app_display_.Register(shelf_app_id, current_display.id());
    }
  }
}

void AppServiceAppWindowCrostiniTracker::OnWindowDestroying(
    aura::Window* window) {
  activation_permissions_.erase(window);
}

void AppServiceAppWindowCrostiniTracker::OnAppLaunchRequested(
    const std::string& app_id,
    int64_t display_id) {
  crostini_app_display_.Register(app_id, display_id);
  // Remove the old permissions and add a permission for every window the app
  // currently has open.
  for (aura::Window* window : activation_permissions_)
    exo::RevokePermissionToActivate(window);
  activation_permissions_.clear();
  ash::ShelfModel* model = app_service_controller_->owner()->shelf_model();
  int index = model->ItemIndexByAppID(app_id);
  if (index >= static_cast<int>(model->items().size()) || index < 0)
    return;

  AppWindowShelfItemController* item_controller =
      model->GetAppWindowShelfItemController(model->items()[index].id);

  // Apps run for the first time won't have a launcher controller yet, return
  // early because they won't have windows either so permissions aren't
  // necessary.
  if (!item_controller)
    return;
  for (AppWindowBase* app_window : item_controller->windows()) {
    exo::GrantPermissionToActivate(app_window->GetNativeWindow(),
                                   kSelfActivationTimeout);
    activation_permissions_.insert(app_window->GetNativeWindow());
  }
}

std::string AppServiceAppWindowCrostiniTracker::GetShelfAppId(
    aura::Window* window) const {
  if (ShouldSkipWindow(window))
    return std::string();

  // Handle browser windows.
  Browser* browser = chrome::FindBrowserWithWindow(window);
  if (browser)
    return std::string();

  // Currently Crostini can only be used from the primary profile. In the
  // future, this may be replaced by some way of matching the container that
  // runs this app with the user that owns it.
  Profile* primary_account_profile =
      ash::ProfileHelper::Get()->GetProfileByAccountId(
          user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId());
  std::string shelf_app_id = guest_os::GetGuestOsShelfAppId(
      primary_account_profile, exo::GetShellApplicationId(window),
      exo::GetShellStartupId(window));

  // When install a new Crostini app and run it directly, Crostini might not get
  // the correct app id yet when `window` is created, but use an unregistered
  // app id for a short term. Then the unregistered app id is saved in
  // InstanceRegistry for `window`. So when the app id is set for `window`
  // later, the app id inconsistent DCHECK is hit, which could affect the
  // instance saved in InstanceRegistry. To prevent the updating for `window` in
  // InstanceRegistry, call MaybeModifyInstance to check the saved app id and
  // the expected shelf_app_id, and if they are not consistent, modify the app
  // id to use `shelf_app_id`.
  if (!shelf_app_id.empty())
    MaybeModifyInstance(primary_account_profile, window, shelf_app_id);
  return shelf_app_id;
}

void AppServiceAppWindowCrostiniTracker::RegisterCrostiniWindowForForceClose(
    aura::Window* window,
    const std::string& app_name) {
  exo::ShellSurfaceBase* surface = exo::GetShellSurfaceBaseForWindow(window);
  if (!surface)
    return;
  crostini::ForceCloseWatcher::Watch(
      std::make_unique<crostini::ShellSurfaceForceCloseDelegate>(surface,
                                                                 app_name));
}

void AppServiceAppWindowCrostiniTracker::MaybeModifyInstance(
    Profile* profile,
    aura::Window* window,
    const std::string& app_id) const {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  DCHECK(proxy);
  auto& instance_registry = proxy->InstanceRegistry();
  std::string old_app_id = instance_registry.GetShelfId(window).app_id;
  if (old_app_id.empty() || app_id == old_app_id)
    return;

  auto* app_service_instance_helper =
      app_service_controller_->app_service_instance_helper();
  DCHECK(app_service_instance_helper);
  auto state = instance_registry.GetState(window);
  app_service_instance_helper->OnInstances(old_app_id, window, std::string(),
                                           apps::InstanceState::kDestroyed);
  app_service_instance_helper->OnInstances(app_id, window, std::string(),
                                           state);
}
