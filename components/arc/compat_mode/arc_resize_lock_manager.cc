// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/arc_resize_lock_manager.h"

#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/arc_resize_lock_type.h"
#include "ash/public/cpp/resize_shadow_type.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/wm/resize_shadow_controller.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/memory/singleton.h"
#include "base/notreached.h"
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
#include "ui/aura/window_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/vector_icons.h"
#include "ui/wm/public/activation_client.h"

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

// A self-deleting window activation observer that runs the given callback when
// its associated window gets activated.
class WindowActivationObserver : public wm::ActivationChangeObserver,
                                 public aura::WindowObserver {
 public:
  WindowActivationObserver(const WindowActivationObserver&) = delete;
  WindowActivationObserver& operator=(const WindowActivationObserver&) = delete;

  static void RunOnActivated(aura::Window* window,
                             base::OnceClosure on_activated) {
    // ash::Shell can be null in unittests.
    if (!ash::Shell::HasInstance())
      return;

    if (ash::Shell::Get()->activation_client()->GetActiveWindow() == window) {
      std::move(on_activated).Run();
      return;
    }

    // The following instance self-destructs when the window gets activated or
    // destroyed before getting activated.
    new WindowActivationObserver(window, std::move(on_activated));
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    DCHECK(observer_.IsObservingSource(window));
    delete this;
  }

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {
    if (gained_active != window_)
      return;
    RemoveAllObservers();
    std::move(on_activated_).Run();
    delete this;
  }

 private:
  WindowActivationObserver(aura::Window* window, base::OnceClosure on_activated)
      : window_(window), on_activated_(std::move(on_activated)) {
    DCHECK(!on_activated_.is_null());
    ash::Shell::Get()->activation_client()->AddObserver(this);
    observer_.Observe(window_);
  }

  ~WindowActivationObserver() override { RemoveAllObservers(); }

  void RemoveAllObservers() {
    observer_.Reset();
    ash::Shell::Get()->activation_client()->RemoveObserver(this);
  }

  aura::Window* const window_;
  base::OnceClosure on_activated_;
  base::ScopedObservation<aura::Window, aura::WindowObserver> observer_{this};
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

  UpdateCompatModeButton(window);

  const ash::ArcResizeLockType current_resize_lock_value =
      window->GetProperty(ash::kArcResizeLockTypeKey);
  const bool resize_lock_changed =
      (key == ash::kArcResizeLockTypeKey &&
       current_resize_lock_value != static_cast<ash::ArcResizeLockType>(old));
  const bool app_id_changed = key == ash::kAppIDKey;

  // Both the resize lock value and app id are needed to enable resize lock.
  if (current_resize_lock_value != ash::ArcResizeLockType::RESIZABLE &&
      (app_id_changed || resize_lock_changed)) {
    window->SetProperty(ash::kResizeShadowTypeKey,
                        ash::ResizeShadowType::kLock);
    EnableResizeLock(window);
  }

  if (resize_lock_changed &&
      current_resize_lock_value == ash::ArcResizeLockType::RESIZABLE) {
    window->SetProperty(ash::kResizeShadowTypeKey,
                        ash::ResizeShadowType::kUnlock);
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
  bool is_first_launch = false;

  const std::string* app_id = window->GetProperty(ash::kAppIDKey);
  DCHECK(app_id);
  // The state is |ArcResizeLockState::READY| only when we enable the resize
  // lock for an app for the first time.
  if (pref_delegate_->GetResizeLockState(*app_id) ==
      mojom::ArcResizeLockState::READY) {
    const ash::ArcResizeLockType resize_lock_value =
        window->GetProperty(ash::kArcResizeLockTypeKey);
    switch (resize_lock_value) {
      case ash::ArcResizeLockType::RESIZE_LIMITED:
        pref_delegate_->SetResizeLockState(*app_id,
                                           mojom::ArcResizeLockState::ON);
        break;
      case ash::ArcResizeLockType::FULLY_LOCKED:
        pref_delegate_->SetResizeLockState(
            *app_id, mojom::ArcResizeLockState::FULLY_LOCKED);
        break;
      case ash::ArcResizeLockType::RESIZABLE:
        NOTREACHED();
    }
    is_first_launch = true;
  }

  // Show lock shadow effect on window. ash::Shell may not exist in tests.
  if (ash::Shell::HasInstance())
    ash::Shell::Get()->resize_shadow_controller()->ShowShadow(window);

  // Because we use the compat mode button as the "anchor" in the splash, we
  // need to show it after the setup of the compat mode button.
  if (is_first_launch && ShouldShowSplashScreenDialog(pref_delegate_)) {
    // Compat-mode button must exist as the anchoring target of the splash.
    UpdateCompatModeButton(window);
    const bool is_for_unresizable =
        window->GetProperty(ash::kArcResizeLockTypeKey) ==
        ash::ArcResizeLockType::FULLY_LOCKED;
    WindowActivationObserver::RunOnActivated(
        window, base::BindOnce(&ArcSplashScreenDialogView::Show, window,
                               is_for_unresizable));
  }
}

void ArcResizeLockManager::DisableResizeLock(aura::Window* window) {
  // Hide shadow effect on window. ash::Shell may not exist in tests.
  if (ash::Shell::HasInstance())
    ash::Shell::Get()->resize_shadow_controller()->HideShadow(window);
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
    // Ideally, we want HeaderView to update properties, but as currently
    // the center button is set to FrameHeader, we need to call this explicitly.
    frame_view->GetHeaderView()->UpdateCaptionButtons();
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
      compat_mode_button->SetAccessibleName(l10n_util::GetStringUTF16(
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
      compat_mode_button->SetAccessibleName(l10n_util::GetStringUTF16(
          IDS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_TABLET));
      break;
    case ResizeCompatMode::kResizable:
      compat_mode_button->SetImage(views::CAPTION_BUTTON_ICON_CENTER,
                                   views::FrameCaptionButton::Animate::kNo,
                                   kResizableIcon);
      compat_mode_button->SetText(l10n_util::GetStringUTF16(
          IDS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_RESIZABLE));
      compat_mode_button->SetAccessibleName(l10n_util::GetStringUTF16(
          IDS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_RESIZABLE));
      break;
  }

  switch (resize_lock_type) {
    case ash::ArcResizeLockType::RESIZE_LIMITED:
    case ash::ArcResizeLockType::RESIZABLE:
      compat_mode_button->SetEnabled(true);
      break;
    case ash::ArcResizeLockType::FULLY_LOCKED:
      compat_mode_button->SetEnabled(false);
      break;
  }
}

void ArcResizeLockManager::ToggleResizeToggleMenu(views::Widget* widget) {
  resize_toggle_menu_.reset();
  resize_toggle_menu_ =
      std::make_unique<ResizeToggleMenu>(widget, pref_delegate_);
}

}  // namespace arc
