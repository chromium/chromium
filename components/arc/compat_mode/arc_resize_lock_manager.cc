// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/arc_resize_lock_manager.h"

#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/window_properties.h"
#include "base/bind.h"
#include "base/memory/singleton.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"

namespace arc {

namespace {

// Singleton factory for ArcResizeLockManager.
class ArcResizeLockManagerFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcResizeLockManager,
          ArcResizeLockManagerFactory> {
 public:
  static constexpr const char* kName = "ArcResizeLockManagerFactory";

  static ArcResizeLockManagerFactory* GetInstance() {
    return base::Singleton<ArcResizeLockManagerFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<ArcResizeLockManagerFactory>;
  ArcResizeLockManagerFactory() = default;
  ~ArcResizeLockManagerFactory() override = default;
};

}  // namespace

// static
ArcResizeLockManager* ArcResizeLockManager::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcResizeLockManagerFactory::GetForBrowserContext(context);
}

ArcResizeLockManager::ArcResizeLockManager(
    content::BrowserContext* browser_context,
    ArcBridgeService* arc_bridge_service) {
  if (aura::Env::HasInstance())
    env_observation.Observe(aura::Env::GetInstance());
}

ArcResizeLockManager::~ArcResizeLockManager() = default;

void ArcResizeLockManager::OnWindowInitialized(aura::Window* new_window) {
  if (!ash::IsArcWindow(new_window))
    return;

  if (window_observations_.IsObservingSource(new_window))
    return;

  if (new_window->GetProperty(ash::kArcResizeLockKey))
    EnableResizeLock(new_window);

  window_observations_.AddObservation(new_window);
}

void ArcResizeLockManager::OnWindowPropertyChanged(aura::Window* window,
                                                   const void* key,
                                                   intptr_t old) {
  if (key != ash::kArcResizeLockKey)
    return;

  const bool current_value = window->GetProperty(ash::kArcResizeLockKey);
  if (current_value == static_cast<bool>(old))
    return;

  if (current_value)
    EnableResizeLock(window);
  else
    DisableResizeLock(window);
}

void ArcResizeLockManager::OnWindowDestroying(aura::Window* window) {
  if (window_observations_.IsObservingSource(window))
    window_observations_.RemoveObservation(window);
}

void ArcResizeLockManager::EnableResizeLock(aura::Window* window) {
  auto* frame_view = ash::NonClientFrameViewAsh::Get(window);

  // Resize Lock feature only supports non-client frame view, and doesn't
  // browser windows.
  DCHECK(frame_view);

  frame_view->GetHeaderView()
      ->caption_button_container()
      ->SetOnSizeButtonPressedCallback(
          base::BindRepeating(&ArcResizeLockManager::OnResizeButtonPressed,
                              base::Unretained(this), frame_view->frame()));
}

void ArcResizeLockManager::DisableResizeLock(aura::Window* window) {
  auto* frame_view = ash::NonClientFrameViewAsh::Get(window);
  DCHECK(frame_view);
  frame_view->GetHeaderView()
      ->caption_button_container()
      ->ClearOnSizeButtonPressedCallback();
}

bool ArcResizeLockManager::OnResizeButtonPressed(views::Widget* widget) {
  if (widget->IsFullscreen() || widget->IsMaximized()) {
    // Use default behavior for "restore" operations.
    return false;
  }

  resize_toggle_menu_.reset();
  resize_toggle_menu_ =
      std::make_unique<ResizeToggleMenu>(widget, pref_delegate_);

  return true;
}

}  // namespace arc
