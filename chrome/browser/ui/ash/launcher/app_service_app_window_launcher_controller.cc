// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_service_app_window_launcher_controller.h"

#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/window_properties.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ui/ash/launcher/app_window_base.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/services/app_service/public/cpp/instance.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

AppServiceAppWindowLauncherController::AppServiceAppWindowLauncherController(
    ChromeLauncherController* owner)
    : AppWindowLauncherController(owner) {
  aura::Env::GetInstance()->AddObserver(this);
  proxy_ = apps::AppServiceProxyFactory::GetForProfile(owner->profile());
  DCHECK(proxy_);
  Observer::Observe(&proxy_->InstanceRegistry());
}

AppServiceAppWindowLauncherController::
    ~AppServiceAppWindowLauncherController() {
  aura::Env::GetInstance()->RemoveObserver(this);
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

  DCHECK(proxy_);
  if (proxy_->AppRegistryCache().GetAppType(shelf_id.app_id) !=
      apps::mojom::AppType::kBuiltIn)
    return;

  std::vector<std::unique_ptr<apps::Instance>> deltas;
  std::unique_ptr<apps::Instance> instance =
      std::make_unique<apps::Instance>(shelf_id.app_id, window);
  instance->SetLaunchId(shelf_id.launch_id);
  deltas.push_back(std::move(instance));
  proxy_->InstanceRegistry().OnInstances(std::move(deltas));

  RegisterAppWindow(window, shelf_id);
}

void AppServiceAppWindowLauncherController::OnWindowVisibilityChanging(
    aura::Window* window,
    bool visible) {
  // Skip OnWindowVisibilityChanged for ancestors/descendants.
  if (!observed_windows_.IsObserving(window))
    return;

  ash::ShelfID shelf_id =
      ash::ShelfID::Deserialize(window->GetProperty(ash::kShelfIDKey));

  std::vector<std::unique_ptr<apps::Instance>> deltas;
  std::unique_ptr<apps::Instance> instance;

  if (shelf_id.IsNull()) {
    if (!plugin_vm::IsPluginVmWindow(window))
      return;
    shelf_id = ash::ShelfID(plugin_vm::kPluginVmAppId);
    instance =
        std::make_unique<apps::Instance>(plugin_vm::kPluginVmAppId, window);
  } else {
    instance = std::make_unique<apps::Instance>(shelf_id.app_id, window);
    instance->SetLaunchId(shelf_id.launch_id);
  }

  // Update |state|. The app must be started, and running state. If visible, set
  // it as |kVisible|, otherwise, clear the visible bit.
  apps::InstanceState state = apps::InstanceState::kUnknown;
  proxy_->InstanceRegistry().ForOneInstance(
      window,
      [&state](const apps::InstanceUpdate& update) { state = update.State(); });
  state = static_cast<apps::InstanceState>(
      state | apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  state = (visible) ? static_cast<apps::InstanceState>(
                          state | apps::InstanceState::kVisible)
                    : static_cast<apps::InstanceState>(
                          state & ~(apps::InstanceState::kVisible));
  instance->UpdateState(state, base::Time::Now());

  deltas.push_back(std::move(instance));
  proxy_->InstanceRegistry().OnInstances(std::move(deltas));

  if (!visible)
    return;

  RegisterAppWindow(window, shelf_id);
}

void AppServiceAppWindowLauncherController::OnWindowDestroying(
    aura::Window* window) {
  DCHECK(observed_windows_.IsObserving(window));
  observed_windows_.Remove(window);

  DCHECK(proxy_);
  ash::ShelfID shelf_id =
      ash::ShelfID::Deserialize(window->GetProperty(ash::kShelfIDKey));
  if (proxy_->AppRegistryCache().GetAppType(shelf_id.app_id) !=
      apps::mojom::AppType::kUnknown) {
    // Delete the instance from InstanceRegistry.
    std::vector<std::unique_ptr<apps::Instance>> deltas;
    std::unique_ptr<apps::Instance> instance =
        std::make_unique<apps::Instance>(shelf_id.app_id, window);
    instance->UpdateState(apps::InstanceState::kDestroyed, base::Time::Now());
    deltas.push_back(std::move(instance));
    proxy_->InstanceRegistry().OnInstances(std::move(deltas));
  }

  auto app_window_it = aura_window_to_app_window_.find(window);
  if (app_window_it == aura_window_to_app_window_.end())
    return;

  RemoveFromShelf(app_window_it->second.get());

  aura_window_to_app_window_.erase(app_window_it);
}

void AppServiceAppWindowLauncherController::OnWindowActivated(
    wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* new_active,
    aura::Window* old_active) {
  AppWindowLauncherController::OnWindowActivated(reason, new_active,
                                                 old_active);

  if (!new_active)
    return;

  // For |gained_active| window, set the activate bit.
  ash::ShelfID shelf_id =
      ash::ShelfID::Deserialize(new_active->GetProperty(ash::kShelfIDKey));
  if (!shelf_id.IsNull()) {
    std::vector<std::unique_ptr<apps::Instance>> deltas;
    std::unique_ptr<apps::Instance> instance =
        std::make_unique<apps::Instance>(shelf_id.app_id, new_active);

    apps::InstanceState state = apps::InstanceState::kUnknown;
    proxy_->InstanceRegistry().ForOneInstance(
        new_active, [&state](const apps::InstanceUpdate& update) {
          state = update.State();
        });
    state = static_cast<apps::InstanceState>(
        state | apps::InstanceState::kStarted | apps::InstanceState::kRunning);
    state =
        static_cast<apps::InstanceState>(state | apps::InstanceState::kActive);
    instance->UpdateState(state, base::Time::Now());
    deltas.push_back(std::move(instance));
    proxy_->InstanceRegistry().OnInstances(std::move(deltas));
  }

  if (!old_active)
    return;

  // For |lost_active| window , clear the activate bit.
  shelf_id =
      ash::ShelfID::Deserialize(old_active->GetProperty(ash::kShelfIDKey));
  if (shelf_id.IsNull())
    return;

  std::vector<std::unique_ptr<apps::Instance>> deltas;
  std::unique_ptr<apps::Instance> instance =
      std::make_unique<apps::Instance>(shelf_id.app_id, old_active);

  apps::InstanceState state = apps::InstanceState::kUnknown;
  proxy_->InstanceRegistry().ForOneInstance(
      old_active,
      [&state](const apps::InstanceUpdate& update) { state = update.State(); });
  state = static_cast<apps::InstanceState>(
      state | apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  state =
      static_cast<apps::InstanceState>(state & ~apps::InstanceState::kActive);
  instance->UpdateState(state, base::Time::Now());
  deltas.push_back(std::move(instance));
  proxy_->InstanceRegistry().OnInstances(std::move(deltas));
}

void AppServiceAppWindowLauncherController::OnInstanceUpdate(
    const apps::InstanceUpdate& update) {
  aura::Window* window = update.Window();

  // This is the first update for the given window.
  if (update.StateIsNull() &&
      (update.State() & apps::InstanceState::kDestroyed) ==
          apps::InstanceState::kUnknown) {
    std::string app_id = update.AppId();
    window->SetProperty(ash::kAppIDKey, update.AppId());
    ash::ShelfID shelf_id(update.AppId(), update.LaunchId());
    window->SetProperty(ash::kShelfIDKey, shelf_id.Serialize());
    window->SetProperty<int>(ash::kShelfItemTypeKey, ash::TYPE_APP);
    return;
  }

  // Launch id is updated, so constructs a new shelf id.
  if (update.LaunchIdChanged()) {
    ash::ShelfID shelf_id(update.AppId(), update.LaunchId());
    window->SetProperty(ash::kShelfIDKey, shelf_id.Serialize());
    window->SetProperty<int>(ash::kShelfItemTypeKey, ash::TYPE_APP);
  }
}

void AppServiceAppWindowLauncherController::OnInstanceRegistryWillBeDestroyed(
    apps::InstanceRegistry* instance_registry) {
  Observe(nullptr);
}

void AppServiceAppWindowLauncherController::RegisterAppWindow(
    aura::Window* window,
    const ash::ShelfID& shelf_id) {
  // Skip when this window has been handled. This can happen when the window
  // becomes visible again.
  auto app_window_it = aura_window_to_app_window_.find(window);
  if (app_window_it != aura_window_to_app_window_.end())
    return;

  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  auto app_window_ptr = std::make_unique<AppWindowBase>(shelf_id, widget);
  AppWindowBase* app_window = app_window_ptr.get();
  aura_window_to_app_window_[window] = std::move(app_window_ptr);

  AddToShelf(app_window);
}

void AppServiceAppWindowLauncherController::UnregisterAppWindow(
    AppWindowBase* app_window) {
  if (!app_window)
    return;

  AppWindowLauncherItemController* controller = app_window->controller();
  if (controller)
    controller->RemoveWindow(app_window);

  app_window->SetController(nullptr);
}

void AppServiceAppWindowLauncherController::AddToShelf(
    AppWindowBase* app_window) {
  const ash::ShelfID shelf_id = app_window->shelf_id();
  // Internal Camera app does not have own window. Either ARC or extension
  // window controller would add window to controller.
  if (shelf_id.app_id == ash::kInternalAppIdCamera)
    return;

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

  item_controller->AddWindow(app_window);
  app_window->SetController(item_controller);
}

void AppServiceAppWindowLauncherController::RemoveFromShelf(
    AppWindowBase* app_window) {
  const ash::ShelfID shelf_id = app_window->shelf_id();
  // Internal Camera app does not have own window. Either ARC or extension
  // window controller would remove window from controller.
  if (shelf_id.app_id == ash::kInternalAppIdCamera)
    return;

  UnregisterAppWindow(app_window);

  // Check if we may close controller now, at this point we can safely remove
  // controllers without window.
  AppWindowLauncherItemController* item_controller =
      owner()->shelf_model()->GetAppWindowLauncherItemController(
          app_window->shelf_id());

  if (item_controller != nullptr && item_controller->window_count() == 0)
    owner()->CloseLauncherItem(item_controller->shelf_id());
}

AppWindowLauncherItemController*
AppServiceAppWindowLauncherController::ControllerForWindow(
    aura::Window* window) {
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

void AppServiceAppWindowLauncherController::OnItemDelegateDiscarded(
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
