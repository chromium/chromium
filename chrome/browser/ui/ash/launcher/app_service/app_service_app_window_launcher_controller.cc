// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_service/app_service_app_window_launcher_controller.h"

#include <memory>

#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/window_properties.h"
#include "base/stl_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/crosapi/browser_util.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_shelf_utils.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/app_service/app_service_app_window_arc_tracker.h"
#include "chrome/browser/ui/ash/launcher/app_service/app_service_app_window_crostini_tracker.h"
#include "chrome/browser/ui/ash/launcher/app_service/app_service_app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/app_window_base.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_util.h"
#include "components/exo/shell_surface_base.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/services/app_service/public/mojom/types.mojom-shared.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "extensions/common/constants.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/widget.h"

namespace {

// Returns the app id from the app id or the shelf group id.
std::string GetAppId(const std::string& id) {
  const arc::ArcAppShelfId arc_app_shelf_id =
      arc::ArcAppShelfId::FromString(id);
  if (!arc_app_shelf_id.valid() || !arc_app_shelf_id.has_shelf_group_id())
    return id;
  return arc_app_shelf_id.app_id();
}

}  // namespace

AppServiceAppWindowLauncherController::AppServiceAppWindowLauncherController(
    ChromeLauncherController* owner)
    : AppWindowLauncherController(owner),
      proxy_(apps::AppServiceProxyFactory::GetForProfile(owner->profile())),
      app_service_instance_helper_(
          std::make_unique<AppServiceInstanceRegistryHelper>(this)) {
  aura::Env::GetInstance()->AddObserver(this);
  Observe(&proxy_->InstanceRegistry());

  if (arc::IsArcAllowedForProfile(owner->profile()))
    arc_tracker_ = std::make_unique<AppServiceAppWindowArcTracker>(this);

  if (crostini::CrostiniFeatures::Get()->IsUIAllowed(owner->profile())) {
    crostini_tracker_ =
        std::make_unique<AppServiceAppWindowCrostiniTracker>(this);
  }

  profile_list_.push_back(owner->profile());

  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser && browser->window() && browser->window()->GetNativeWindow()) {
      observed_windows_.Add(browser->window()->GetNativeWindow());

      // Observe the browser tabs
      TabStripModel* tab_strip = browser->tab_strip_model();
      for (int i = 0; i < tab_strip->count(); ++i) {
        auto* tab = tab_strip->GetWebContentsAt(i);
        if (!tab)
          continue;
        aura::Window* window = tab->GetNativeView();
        if (window) {
          observed_windows_.Add(window);
        }
      }
    }
  }
}

AppServiceAppWindowLauncherController::
    ~AppServiceAppWindowLauncherController() {
  aura::Env::GetInstance()->RemoveObserver(this);

  // We need to remove all Registry observers for added users.
  for (auto* profile : profile_list_) {
    apps::AppServiceProxy* proxy =
        apps::AppServiceProxyFactory::GetForProfile(profile);
    proxy->InstanceRegistry().RemoveObserver(this);
  }

  app_service_instance_helper_.reset();
  observed_windows_.RemoveAll();
}

AppWindowLauncherItemController*
AppServiceAppWindowLauncherController::ControllerForWindow(
    aura::Window* window) {
  if (!window)
    return nullptr;

  auto it = aura_window_to_app_window_.find(window);
  if (it == aura_window_to_app_window_.end())
    return nullptr;

  AppWindowBase* const app_window = it->second.get();
  DCHECK(app_window);
  return app_window->controller();
}

void AppServiceAppWindowLauncherController::ActiveUserChanged(
    const std::string& user_email) {
  proxy_ = apps::AppServiceProxyFactory::GetForProfile(owner()->profile());
  // Deactivates the running app windows in InstanceRegistry for the inactive
  // user, and activates the app windows for the active user.
  for (auto* window : window_list_) {
    ash::ShelfID shelf_id = proxy_->InstanceRegistry().GetShelfId(window);
    if (!shelf_id.IsNull()) {
      RegisterWindow(window, shelf_id);
    } else {
      auto app_window_it = aura_window_to_app_window_.find(window);
      if (app_window_it != aura_window_to_app_window_.end()) {
        RemoveAppWindowFromShelf(app_window_it->second.get());
        aura_window_to_app_window_.erase(app_window_it);
      }
    }
  }
  app_service_instance_helper_->ActiveUserChanged();
  if (arc_tracker_)
    arc_tracker_->ActiveUserChanged(user_email);
}

void AppServiceAppWindowLauncherController::AdditionalUserAddedToSession(
    Profile* profile) {
  // Each users InstanceRegister needs to be observed.
  proxy_ = apps::AppServiceProxyFactory::GetForProfile(profile);
  proxy_->InstanceRegistry().AddObserver(this);
  profile_list_.push_back(profile);

  app_service_instance_helper_->AdditionalUserAddedToSession(profile);
}

void AppServiceAppWindowLauncherController::OnWindowInitialized(
    aura::Window* window) {
  // An app window has type WINDOW_TYPE_NORMAL, a WindowDelegate and
  // is a top level views widget. Tooltips, menus, and other kinds of transient
  // windows that can't activate are filtered out.
  if (window->type() != aura::client::WINDOW_TYPE_NORMAL || !window->delegate())
    return;
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  if (!widget || !widget->is_top_level())
    return;

  observed_windows_.Add(window);
  if (arc_tracker_)
    arc_tracker_->AddCandidateWindow(window);
}

void AppServiceAppWindowLauncherController::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  if (key != ash::kShelfIDKey)
    return;

  ash::ShelfID shelf_id =
      ash::ShelfID::Deserialize(window->GetProperty(ash::kShelfIDKey));
  if (shelf_id.IsNull())
    return;

  if (GetAppType(shelf_id.app_id) != apps::mojom::AppType::kBuiltIn)
    return;

  app_service_instance_helper_->OnInstances(shelf_id.app_id, window,
                                            shelf_id.launch_id,
                                            apps::InstanceState::kUnknown);

  RegisterWindow(window, shelf_id);
}

void AppServiceAppWindowLauncherController::OnWindowVisibilityChanged(
    aura::Window* window,
    bool visible) {
  // Skip OnWindowVisibilityChanged for ancestors/descendants.
  if (!observed_windows_.IsObserving(window))
    return;

  if (arc_tracker_)
    arc_tracker_->OnWindowVisibilityChanged(window);

  ash::ShelfID shelf_id = GetShelfId(window);
  if (shelf_id.IsNull())
    return;

  if (app_service_instance_helper_->IsOpenedInBrowser(GetAppId(shelf_id.app_id),
                                                      window) ||
      shelf_id.app_id == extension_misc::kChromeAppId) {
    app_service_instance_helper_->OnWindowVisibilityChanged(shelf_id, window,
                                                            visible);
    return;
  }

  // Update |state|. The app must be started, and running state. If visible,
  // set it as |kVisible|, otherwise, clear the visible bit.
  apps::InstanceState state =
      app_service_instance_helper_->CalculateVisibilityState(window, visible);
  app_service_instance_helper_->OnInstances(GetAppId(shelf_id.app_id), window,
                                            shelf_id.launch_id, state);

  // Only register the visible non-browser |window| for the active user.
  if (!visible || shelf_id.app_id == extension_misc::kChromeAppId ||
      !proxy_->InstanceRegistry().Exists(window)) {
    return;
  }

  RegisterWindow(window, shelf_id);

  if (crostini_tracker_)
    crostini_tracker_->OnWindowVisibilityChanged(window, shelf_id.app_id);

  // This will match both the Plugin VM App window and installer.
  if (shelf_id.app_id == plugin_vm::kPluginVmShelfAppId) {
    // Plugin VM can only be used on the primary profile.
    MultiUserWindowManagerHelper::GetWindowManager()->SetWindowOwner(
        window,
        user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId());
  }
}

void AppServiceAppWindowLauncherController::OnWindowDestroying(
    aura::Window* window) {
  DCHECK(observed_windows_.IsObserving(window));
  observed_windows_.Remove(window);
  if (arc_tracker_)
    arc_tracker_->RemoveCandidateWindow(window);
  if (crostini_tracker_)
    crostini_tracker_->OnWindowDestroying(window);

  // When the window is destroyed, we should search all proxies, because the
  // window could be teleported from the inactive user, and isn't saved in the
  // proxy of the active user's profile, but it should still be removed from
  // the controller, and the shelf, so search all the proxies.
  std::string app_id = GetShelfId(window).app_id;
  if (app_id.empty()) {
    // For Crostini apps, it could be run from the command line, and not saved
    // in AppService, so GetShelfId could return null when the window is
    // destroyed, but it should still be deleted from instance and remove the
    // app window from the shelf. So if we can get the window from
    // InstanceRegistry, we should still destroy it from InstanceRegistry and
    // remove the app window from the shelf
    app_id = app_service_instance_helper_->GetAppId(window);
    if (app_id.empty())
      return;
  }

  if (app_service_instance_helper_->IsOpenedInBrowser(GetAppId(app_id),
                                                      window) ||
      app_id == extension_misc::kChromeAppId) {
    return;
  }

  // Delete the instance from InstanceRegistry.
  app_service_instance_helper_->OnInstances(
      GetAppId(app_id), window, std::string(), apps::InstanceState::kDestroyed);

  auto app_window_it = aura_window_to_app_window_.find(window);
  if (app_window_it == aura_window_to_app_window_.end())
    return;

  // Note, for ARC apps, window may be recreated in some cases, so do not close
  // controller on window destroying. Controller will be closed onTaskDestroyed
  // event which is generated when actual task is destroyed.
  if (arc_tracker_ && arc::GetWindowTaskId(window) != arc::kNoTaskId) {
    arc_tracker_->OnWindowDestroying(window);
    aura_window_to_app_window_.erase(app_window_it);
    return;
  }

  RemoveAppWindowFromShelf(app_window_it->second.get());

  aura_window_to_app_window_.erase(app_window_it);
}

void AppServiceAppWindowLauncherController::OnWindowActivated(
    wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* new_active,
    aura::Window* old_active) {
  AppWindowLauncherController::OnWindowActivated(reason, new_active,
                                                 old_active);

  if (arc_tracker_)
    arc_tracker_->OnTaskSetActive(arc_tracker_->active_task_id());

  SetWindowActivated(new_active, /*active*/ true);
  SetWindowActivated(old_active, /*active*/ false);
}

void AppServiceAppWindowLauncherController::OnInstanceUpdate(
    const apps::InstanceUpdate& update) {
  if (update.StateChanged() &&
      update.State() == apps::InstanceState::kDestroyed) {
    // For Chrome apps edge case, it could be added for the inactive users, and
    // then removed. Since it is not registered we don't need to do anything
    // anyways. As such, all which is left to do here is to get rid of our own
    // reference.
    WindowList::iterator it =
        std::find(window_list_.begin(), window_list_.end(), update.Window());
    if (it != window_list_.end())
      window_list_.erase(it);
    return;
  }

  aura::Window* window = update.Window();
  if (!observed_windows_.IsObserving(window))
    return;

  ash::ShelfID shelf_id(update.AppId(), update.LaunchId());

  // This is the first update for the given window.
  if (update.StateIsNull() &&
      (update.State() & apps::InstanceState::kDestroyed) ==
          apps::InstanceState::kUnknown) {
    std::string app_id = update.AppId();
    if (GetAppType(app_id) == apps::mojom::AppType::kCrostini ||
        crostini::IsUnmatchedCrostiniShelfAppId(app_id)) {
      window->SetProperty(aura::client::kAppType,
                          static_cast<int>(ash::AppType::CROSTINI_APP));
    }
    window->SetProperty(ash::kAppIDKey, update.AppId());
    window->SetProperty(ash::kShelfIDKey, shelf_id.Serialize());
    window->SetProperty<int>(ash::kShelfItemTypeKey, ash::TYPE_APP);

    // When an extension app window is added, calls UserHasAppOnActiveDesktop to
    // handle teleport function.
    if (update.BrowserContext() &&
        (update.State() == apps::InstanceState::kStarted)) {
      UserHasAppOnActiveDesktop(window, shelf_id, update.BrowserContext());
    }
    // Apps opened in browser are managed by browser, so skip them.
    if (app_service_instance_helper_->IsOpenedInBrowser(
            GetAppId(shelf_id.app_id), window) ||
        shelf_id.app_id == extension_misc::kChromeAppId) {
      return;
    }
    window_list_.push_back(window);
    return;
  }

  // Launch id is updated, so constructs a new shelf id.
  if (update.LaunchIdChanged()) {
    window->SetProperty(ash::kShelfIDKey, shelf_id.Serialize());
    window->SetProperty<int>(ash::kShelfItemTypeKey, ash::TYPE_APP);
  }

  if (update.State() == apps::InstanceState::kHidden) {
    // When the app window is hidden, it should be removed from Shelf.
    //
    // The window is teleported to the current user could be hidden as
    // well. But we only remove the window added for the active user, and skip
    // the window teleported to the current user, because
    // MultiUserWindowManagerHelper manages those windows.
    auto app_window_it = aura_window_to_app_window_.find(window);
    if (app_window_it != aura_window_to_app_window_.end() &&
        proxy_->InstanceRegistry().Exists(window)) {
      RemoveAppWindowFromShelf(app_window_it->second.get());
      aura_window_to_app_window_.erase(app_window_it);
    }
  }
}

void AppServiceAppWindowLauncherController::OnInstanceRegistryWillBeDestroyed(
    apps::InstanceRegistry* instance_registry) {
  Observe(nullptr);
}

int AppServiceAppWindowLauncherController::GetActiveTaskId() const {
  if (arc_tracker_)
    return arc_tracker_->active_task_id();
  return arc::kNoTaskId;
}

void AppServiceAppWindowLauncherController::UnregisterWindow(
    aura::Window* window) {
  auto app_window_it = aura_window_to_app_window_.find(window);
  if (app_window_it == aura_window_to_app_window_.end())
    return;
  UnregisterAppWindow(app_window_it->second.get());
}

void AppServiceAppWindowLauncherController::AddWindowToShelf(
    aura::Window* window,
    const ash::ShelfID& shelf_id) {
  if (base::Contains(aura_window_to_app_window_, window))
    return;

  AppWindowBase* app_window;
  if (arc::GetWindowTaskId(window) != arc::kNoTaskId) {
    std::unique_ptr<ArcAppWindow> app_window_ptr =
        std::make_unique<ArcAppWindow>(
            arc::GetWindowTaskId(window),
            arc::ArcAppShelfId::FromString(shelf_id.app_id),
            views::Widget::GetWidgetForNativeWindow(window), this,
            owner()->profile());
    app_window = app_window_ptr.get();
    aura_window_to_app_window_[window] = std::move(app_window_ptr);
  } else {
    auto app_window_ptr = std::make_unique<AppWindowBase>(
        shelf_id, views::Widget::GetWidgetForNativeWindow(window));
    app_window = app_window_ptr.get();
    aura_window_to_app_window_[window] = std::move(app_window_ptr);
  }
  AddAppWindowToShelf(app_window);
}

AppWindowBase* AppServiceAppWindowLauncherController::GetAppWindow(
    aura::Window* window) {
  if (!base::Contains(aura_window_to_app_window_, window))
    return nullptr;
  return aura_window_to_app_window_[window].get();
}

void AppServiceAppWindowLauncherController::ObserveWindow(
    aura::Window* window) {
  if (!window || observed_windows_.IsObserving(window))
    return;
  observed_windows_.Add(window);
}

bool AppServiceAppWindowLauncherController::IsObservingWindow(
    aura::Window* window) {
  DCHECK(window);
  return observed_windows_.IsObserving(window);
}

std::vector<aura::Window*>
AppServiceAppWindowLauncherController::GetArcWindows() {
  std::vector<aura::Window*> arc_windows;
  std::copy_if(window_list_.begin(), window_list_.end(),
               std::inserter(arc_windows, arc_windows.end()),
               [](aura::Window* w) { return arc::IsArcAppWindow(w); });
  return arc_windows;
}

void AppServiceAppWindowLauncherController::SetWindowActivated(
    aura::Window* window,
    bool active) {
  if (!window || !observed_windows_.IsObserving(window))
    return;

  const ash::ShelfID shelf_id = GetShelfId(window);
  if (shelf_id.IsNull())
    return;

  if (app_service_instance_helper_->IsOpenedInBrowser(GetAppId(shelf_id.app_id),
                                                      window) ||
      shelf_id.app_id == extension_misc::kChromeAppId) {
    app_service_instance_helper_->SetWindowActivated(shelf_id, window, active);
    return;
  }

  apps::InstanceState state =
      app_service_instance_helper_->CalculateActivatedState(window, active);
  app_service_instance_helper_->OnInstances(GetAppId(shelf_id.app_id), window,
                                            std::string(), state);
}

void AppServiceAppWindowLauncherController::RegisterWindow(
    aura::Window* window,
    const ash::ShelfID& shelf_id) {
  // Skip when this window has been handled. This can happen when the window
  // becomes visible again.
  auto app_window_it = aura_window_to_app_window_.find(window);
  if (app_window_it != aura_window_to_app_window_.end())
    return;

  // For the ARC apps window, AttachControllerToWindow calls AddWindowToShelf,
  // so we don't need to call AddWindowToShelf again.
  if (arc_tracker_ && arc::GetWindowTaskId(window) != arc::kNoTaskId) {
    arc_tracker_->AttachControllerToWindow(window);
  } else {
    // The window for ARC Play Store is a special window, which is created by
    // both Extensions and ARC. If Extensions's window is generated after
    // ARC window, calls OnItemDelegateDiscarded to remove the ARC apps
    // window.
    if (shelf_id.app_id == arc::kPlayStoreAppId) {
      AppWindowLauncherItemController* item_controller =
          owner()->shelf_model()->GetAppWindowLauncherItemController(shelf_id);
      if (item_controller && shelf_id.app_id == arc::kPlayStoreAppId &&
          arc_tracker_) {
        OnItemDelegateDiscarded(item_controller);
      }
    }

    AddWindowToShelf(window, shelf_id);

    if (plugin_vm::IsPluginVmAppWindow(window)) {
      // Set an icon for the Plugin VM app window.
      static_cast<exo::ShellSurfaceBase*>(
          views::Widget::GetWidgetForNativeWindow(window)->widget_delegate())
          ->SetIcon(*ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_LOGO_PLUGIN_VM_DEFAULT_192));
    }
  }
}

void AppServiceAppWindowLauncherController::UnregisterAppWindow(
    AppWindowBase* app_window) {
  if (!app_window)
    return;

  AppWindowLauncherItemController* const controller = app_window->controller();
  if (controller)
    controller->RemoveWindow(app_window);

  app_window->SetController(nullptr);
}

void AppServiceAppWindowLauncherController::AddAppWindowToShelf(
    AppWindowBase* app_window) {
  const ash::ShelfID shelf_id = app_window->shelf_id();

  AppWindowLauncherItemController* item_controller =
      owner()->shelf_model()->GetAppWindowLauncherItemController(shelf_id);
  if (item_controller == nullptr) {
    auto controller =
        std::make_unique<AppServiceAppWindowLauncherItemController>(shelf_id,
                                                                    this);
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

  item_controller->AddWindow(app_window);
  app_window->SetController(item_controller);
}

void AppServiceAppWindowLauncherController::RemoveAppWindowFromShelf(
    AppWindowBase* app_window) {
  const ash::ShelfID shelf_id = app_window->shelf_id();

  UnregisterAppWindow(app_window);

  // Check if we may close controller now, at this point we can safely remove
  // controllers without window.
  AppWindowLauncherItemController* item_controller =
      owner()->shelf_model()->GetAppWindowLauncherItemController(
          app_window->shelf_id());

  if (item_controller && item_controller->window_count() == 0)
    owner()->CloseLauncherItem(item_controller->shelf_id());
}

void AppServiceAppWindowLauncherController::OnItemDelegateDiscarded(
    ash::ShelfItemDelegate* delegate) {
  for (auto& it : aura_window_to_app_window_) {
    AppWindowBase* app_window = it.second.get();
    if (!app_window)
      continue;

    if (arc_tracker_)
      arc_tracker_->OnItemDelegateDiscarded(app_window->shelf_id(), delegate);

    if (!app_window || app_window->controller() != delegate)
      continue;

    VLOG(1) << "Item controller was released externally for the app "
            << delegate->shelf_id().app_id << ".";

    UnregisterAppWindow(it.second.get());
  }
}

ash::ShelfID AppServiceAppWindowLauncherController::GetShelfId(
    aura::Window* window) const {
  if (crosapi::browser_util::IsLacrosWindow(window))
    return ash::ShelfID(extension_misc::kLacrosAppId);

  if (crostini_tracker_) {
    std::string shelf_app_id;
    shelf_app_id = crostini_tracker_->GetShelfAppId(window);
    if (!shelf_app_id.empty())
      return ash::ShelfID(shelf_app_id);
  }

  if (plugin_vm::IsPluginVmAppWindow(window))
    return ash::ShelfID(plugin_vm::kPluginVmShelfAppId);

  ash::ShelfID shelf_id;
  if (arc_tracker_)
    shelf_id = arc_tracker_->GetShelfId(arc::GetWindowTaskId(window));

  if (!shelf_id.IsNull())
    return shelf_id;

  // If the window exists in InstanceRegistry, get the shelf id from
  // InstanceRegistry.
  for (auto* profile : profile_list_) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
    shelf_id = proxy->InstanceRegistry().GetShelfId(window);
    if (!shelf_id.IsNull())
      break;
  }
  if (shelf_id.IsNull()) {
    shelf_id = ash::ShelfID::Deserialize(window->GetProperty(ash::kShelfIDKey));
  }
  if (!shelf_id.IsNull() &&
      GetAppType(shelf_id.app_id) != apps::mojom::AppType::kUnknown) {
    return shelf_id;
  }
  return ash::ShelfID();
}

apps::mojom::AppType AppServiceAppWindowLauncherController::GetAppType(
    const std::string& app_id) const {
  for (auto* profile : profile_list_) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
    auto app_type = proxy->AppRegistryCache().GetAppType(app_id);
    if (app_type != apps::mojom::AppType::kUnknown) {
      return app_type;
    }
  }
  return apps::mojom::AppType::kUnknown;
}

void AppServiceAppWindowLauncherController::UserHasAppOnActiveDesktop(
    aura::Window* window,
    const ash::ShelfID& shelf_id,
    content::BrowserContext* browser_context) {
  // If the window was created for the active user, register it to show an item
  // on the shelf.
  if (proxy_->InstanceRegistry().Exists(window)) {
    RegisterWindow(window, shelf_id);
    return;
  }

  // If the window was created for the inactive user and it has been teleported
  // to the current user's desktop, register it to show an item on the shelf.
  const AccountId current_account_id = multi_user_util::GetCurrentAccountId();
  MultiUserWindowManagerHelper* helper =
      MultiUserWindowManagerHelper::GetInstance();
  aura::Window* other_window = nullptr;
  for (auto* it : profile_list_) {
    apps::AppServiceProxy* proxy =
        apps::AppServiceProxyFactory::GetForProfile(it);
    if (proxy == proxy_)
      continue;
    proxy->InstanceRegistry().ForEachInstance(
        [&other_window, &window, &shelf_id, &browser_context, &helper,
         &current_account_id](const apps::InstanceUpdate& update) {
          if (helper->IsWindowOnDesktopOfUser(update.Window(),
                                              current_account_id) &&
              (update.AppId() == shelf_id.app_id) &&
              (update.BrowserContext() == browser_context) &&
              update.Window() != window) {
            other_window = update.Window();
          }
        });
    if (other_window)
      break;
  }
  if (other_window) {
    MultiUserWindowManagerHelper::GetWindowManager()->ShowWindowForUser(
        window, multi_user_util::GetCurrentAccountId());
    RegisterWindow(window, shelf_id);
  }
}
