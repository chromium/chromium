// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/arc_resize_lock_manager.h"

#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/arc_resize_lock_type.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/bind.h"
#include "base/memory/singleton.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/default_frame_header.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/compat_mode/arc_splash_screen_dialog_view.h"
#include "components/arc/compat_mode/resize_toggle_menu.h"
#include "components/arc/compat_mode/resize_util.h"
#include "components/arc/vector_icons/vector_icons.h"
#include "components/exo/client_controlled_shell_surface.h"
#include "components/exo/shell_surface_util.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/vector_icons.h"

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

  window_observations_.AddObservation(new_window);
}

void ArcResizeLockManager::OnWindowPropertyChanged(aura::Window* window,
                                                   const void* key,
                                                   intptr_t old) {
  if (key != ash::kArcResizeLockTypeKey && key != ash::kAppIDKey)
    return;

  if (window->GetProperty(ash::kAppIDKey) == nullptr)
    return;

  const ash::ArcResizeLockType current_resize_lock_value =
      window->GetProperty(ash::kArcResizeLockTypeKey);
  const bool resize_lock_changed =
      (key == ash::kArcResizeLockTypeKey &&
       current_resize_lock_value != static_cast<ash::ArcResizeLockType>(old));
  const bool app_id_changed = key == ash::kAppIDKey;

  // Both the resize lock value and app id are needed to enable resize lock.
  if (current_resize_lock_value != ash::ArcResizeLockType::RESIZABLE &&
      (app_id_changed || resize_lock_changed)) {
    EnableResizeLock(window);
  }

  if (resize_lock_changed &&
      current_resize_lock_value == ash::ArcResizeLockType::RESIZABLE) {
    DisableResizeLock(window);
  }
}

void ArcResizeLockManager::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  UpdateCompatModeButton(window);
}

void ArcResizeLockManager::OnWindowDestroying(aura::Window* window) {
  if (window_observations_.IsObservingSource(window))
    window_observations_.RemoveObservation(window);
}

void ArcResizeLockManager::EnableResizeLock(aura::Window* window) {
  const std::string* app_id = window->GetProperty(ash::kAppIDKey);
  // The state is |ArcResizeLockState::READY| only when we enable the resize
  // lock for an app for the first time.
  if (app_id && pref_delegate_->GetResizeLockState(*app_id) ==
                    mojom::ArcResizeLockState::READY) {
    pref_delegate_->SetResizeLockState(*app_id, mojom::ArcResizeLockState::ON);

    // Show the splash screen in current window. The splash screen is an
    // overlay covering the entire window. User can only remove the overlay
    // before closing the window.
    MayShowSplashScreen(window);
  }

  UpdateCompatModeButton(window);
}

void ArcResizeLockManager::DisableResizeLock(aura::Window* window) {
  UpdateCompatModeButton(window);
}

void ArcResizeLockManager::UpdateCompatModeButton(aura::Window* window) {
  DCHECK(ash::IsArcWindow(window));

  const std::string* app_id = window->GetProperty(ash::kAppIDKey);
  if (!app_id)
    return;
  auto* frame_view = ash::NonClientFrameViewAsh::Get(window);
  if (!frame_view)
    return;
  auto* frame_header = frame_view->GetHeaderView()->GetFrameHeader();
  const auto resize_lock_state = pref_delegate_->GetResizeLockState(*app_id);
  if (resize_lock_state == mojom::ArcResizeLockState::UNDEFINED ||
      resize_lock_state == mojom::ArcResizeLockState::READY) {
    frame_header->SetCenterButton(nullptr);
    return;
  }
  auto* compat_mode_button = frame_header->GetCenterButton();
  if (!compat_mode_button) {
    // The ownership is transferred implicitly with AddChildView in HeaderView,
    // but ideally we want to explicitly manage the lifecycle of this resource.
    compat_mode_button = new chromeos::FrameCenterButton(base::BindRepeating(
        &ArcResizeLockManager::ToggleResizeToggleMenu, base::Unretained(this),
        base::Unretained(frame_view->frame())));
    compat_mode_button->SetSubImage(views::kMenuDropArrowIcon);
    frame_header->SetCenterButton(compat_mode_button);
    const auto* surface = static_cast<exo::ClientControlledShellSurface*>(
        exo::GetShellSurfaceBaseForWindow(window));
    DCHECK(surface);
    // TODO(b:185720086): Set the button to WideFrameView.
  }

  const auto currentMode =
      PredictCurrentMode(frame_view->frame(), pref_delegate_);
  if (!currentMode)
    return;

  const auto resize_lock_type = window->GetProperty(ash::kArcResizeLockTypeKey);

  switch (*currentMode) {
    case ResizeCompatMode::kPhone:
      compat_mode_button->SetImage(views::CAPTION_BUTTON_ICON_CENTER,
                                   views::FrameCaptionButton::Animate::kNo,
                                   ash::kSystemMenuPhoneIcon);
      compat_mode_button->SetText(l10n_util::GetStringUTF16(
          IDS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_PHONE));
      if (resize_lock_type == ash::ArcResizeLockType::FULLY_LOCKED) {
        compat_mode_button->SetTooltipText(l10n_util::GetStringUTF16(
            IDS_ASH_ARC_APP_COMPAT_DISABLED_COMPAT_MODE_BUTTON_TOOLTIP_PHONE));
      }
      break;
    case ResizeCompatMode::kTablet:
      compat_mode_button->SetImage(views::CAPTION_BUTTON_ICON_CENTER,
                                   views::FrameCaptionButton::Animate::kNo,
                                   ash::kSystemMenuTabletIcon);
      compat_mode_button->SetText(l10n_util::GetStringUTF16(
          IDS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_TABLET));
      break;
    case ResizeCompatMode::kResizable:
      compat_mode_button->SetImage(views::CAPTION_BUTTON_ICON_CENTER,
                                   views::FrameCaptionButton::Animate::kNo,
                                   kResizableIcon);
      compat_mode_button->SetText(l10n_util::GetStringUTF16(
          IDS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_RESIZABLE));
      break;
  }

  switch (resize_lock_type) {
    case ash::ArcResizeLockType::RESIZE_LIMITED:
      compat_mode_button->SetEnabled(true);
      break;
    case ash::ArcResizeLockType::FULLY_LOCKED:
      compat_mode_button->SetEnabled(false);
      break;
    case ash::ArcResizeLockType::RESIZABLE:
      NOTREACHED();
      break;
  }
}

void ArcResizeLockManager::ToggleResizeToggleMenu(views::Widget* widget) {
  resize_toggle_menu_.reset();
  resize_toggle_menu_ =
      std::make_unique<ResizeToggleMenu>(widget, pref_delegate_);
}

void ArcResizeLockManager::MayShowSplashScreen(aura::Window* window) {
  if (ShouldShowSplashScreenDialog(pref_delegate_)) {
    ArcSplashScreenDialogView::Show(window);
  }
}

}  // namespace arc
