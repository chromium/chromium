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
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/default_frame_header.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/compat_mode/arc_splash_screen_dialog_view.h"
#include "components/arc/compat_mode/arc_window_property_util.h"
#include "components/arc/compat_mode/metrics.h"
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
    // To avoid nested-activation, here we post the task to the queue.
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     std::move(on_activated_));
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

// A self-deleting window property observer that runs the given callback when
// its ash::kAppIDKey is set to non-null value.
class AppIdObserver : public aura::WindowObserver {
 public:
  AppIdObserver(const AppIdObserver&) = delete;
  AppIdObserver& operator=(const AppIdObserver&) = delete;

  static void RunOnReady(aura::Window* window,
                         base::OnceCallback<void(aura::Window*)> on_ready) {
    if (GetAppId(window)) {
      std::move(on_ready).Run(window);
      return;
    }

    // The following instance self-destructs when the window gets activated or
    // destroyed before getting activated.
    new AppIdObserver(window, std::move(on_ready));
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    DCHECK(observer_.IsObservingSource(window));
    delete this;
  }
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    DCHECK(observer_.IsObservingSource(window));
    if (key != ash::kAppIDKey)
      return;
    if (!GetAppId(window))
      return;
    observer_.Reset();
    std::move(on_ready_).Run(window);
    delete this;
  }

 private:
  AppIdObserver(aura::Window* window,
                base::OnceCallback<void(aura::Window*)> on_ready)
      : window_(window), on_ready_(std::move(on_ready)) {
    DCHECK(!on_ready_.is_null());
    observer_.Observe(window_);
  }

  ~AppIdObserver() override { observer_.Reset(); }

  aura::Window* const window_;
  base::OnceCallback<void(aura::Window*)> on_ready_;
  base::ScopedObservation<aura::Window, aura::WindowObserver> observer_{this};
};

bool ShouldEnableResizeLock(ash::ArcResizeLockType type) {
  return type != ash::ArcResizeLockType::RESIZABLE;
}

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

  AppIdObserver::RunOnReady(
      new_window,
      base::BindOnce(
          [](base::WeakPtr<ArcResizeLockManager> manager,
             aura::Window* window) {
            if (!manager)
              return;
            if (!manager->pref_delegate_)
              return;
            const auto state =
                manager->pref_delegate_->GetResizeLockState(*GetAppId(window));
            RecordResizeLockStateHistogram(
                ResizeLockStateHistogramType::InitialState, state);
          },
          weak_ptr_factory_.GetWeakPtr()));
}

void ArcResizeLockManager::OnWindowPropertyChanged(aura::Window* window,
                                                   const void* key,
                                                   intptr_t old) {
  if (key != ash::kArcResizeLockTypeKey)
    return;

  // We need to always trigger UpdateCompatModeButton regardless of value
  // change because it need to be called even when the property is set to
  // ArcResizeLockType::RESIZABLE, which is the the default value of
  // kArcResizeLockTypeKey, and the new value is the same as |old| in that case.
  AppIdObserver::RunOnReady(
      window, base::BindOnce(&ArcResizeLockManager::UpdateCompatModeButton,
                             weak_ptr_factory_.GetWeakPtr()));

  const auto new_value = window->GetProperty(ash::kArcResizeLockTypeKey);
  const auto old_value = static_cast<ash::ArcResizeLockType>(old);

  if (new_value == old_value)
    return;

  if (ShouldEnableResizeLock(new_value)) {
    // Both the resize lock value and app id are needed to enable resize lock.
    AppIdObserver::RunOnReady(
        window, base::BindOnce(
                    [](base::WeakPtr<ArcResizeLockManager> manager,
                       aura::Window* window) {
                      if (!manager)
                        return;
                      if (!ShouldEnableResizeLock(window->GetProperty(
                              ash::kArcResizeLockTypeKey))) {
                        return;
                      }
                      manager->EnableResizeLock(window);
                    },
                    weak_ptr_factory_.GetWeakPtr()));
  } else {
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
  resize_lock_enabled_windows_.erase(window);
  if (window_observations_.IsObservingSource(window))
    window_observations_.RemoveObservation(window);
}

void ArcResizeLockManager::EnableResizeLock(aura::Window* window) {
  const bool inserted = resize_lock_enabled_windows_.insert(window).second;
  if (!inserted)
    return;

  const auto app_id = GetAppId(window);
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
    // As we updated the resize lock state above, we need to update compat mode
    // button.
    UpdateCompatModeButton(window);

    if (ShouldShowSplashScreenDialog(pref_delegate_)) {
      const bool is_for_unresizable =
          window->GetProperty(ash::kArcResizeLockTypeKey) ==
          ash::ArcResizeLockType::FULLY_LOCKED;
      WindowActivationObserver::RunOnActivated(
          window, base::BindOnce(&ArcSplashScreenDialogView::Show, window,
                                 is_for_unresizable));
    }
  }

  window->SetProperty(ash::kResizeShadowTypeKey, ash::ResizeShadowType::kLock);
  // Show lock shadow effect on window. ash::Shell may not exist in tests.
  if (ash::Shell::HasInstance())
    ash::Shell::Get()->resize_shadow_controller()->ShowShadow(window);
}

void ArcResizeLockManager::DisableResizeLock(aura::Window* window) {
  const bool erased = resize_lock_enabled_windows_.erase(window);
  if (!erased)
    return;

  window->SetProperty(ash::kResizeShadowTypeKey,
                      ash::ResizeShadowType::kUnlock);
  // Hide shadow effect on window. ash::Shell may not exist in tests.
  if (ash::Shell::HasInstance())
    ash::Shell::Get()->resize_shadow_controller()->HideShadow(window);
}

void ArcResizeLockManager::UpdateCompatModeButton(aura::Window* window) {
  DCHECK(ash::IsArcWindow(window));

  const auto app_id = GetAppId(window);
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

  const auto resize_lock_type = window->GetProperty(ash::kArcResizeLockTypeKey);

  switch (PredictCurrentMode(frame_view->frame())) {
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
      frame_view->SetToggleResizeLockMenuCallback(
          base::BindRepeating(&ArcResizeLockManager::ToggleResizeToggleMenu,
                              base::Unretained(this), frame_view->frame()));
      break;
    case ash::ArcResizeLockType::FULLY_LOCKED:
      compat_mode_button->SetEnabled(false);
      frame_view->ClearToggleResizeLockMenuCallback();
      break;
  }
}

void ArcResizeLockManager::ToggleResizeToggleMenu(views::Widget* widget) {
  aura::Window* window = widget->GetNativeWindow();
  if (!window || !ash::IsArcWindow(window))
    return;

  auto* frame_view = ash::NonClientFrameViewAsh::Get(window);
  DCHECK(frame_view);
  const auto* compat_mode_button =
      frame_view->GetHeaderView()->GetFrameHeader()->GetCenterButton();
  if (!compat_mode_button || !compat_mode_button->GetEnabled())
    return;
  resize_toggle_menu_.reset();
  resize_toggle_menu_ =
      std::make_unique<ResizeToggleMenu>(widget, pref_delegate_);
}

}  // namespace arc
