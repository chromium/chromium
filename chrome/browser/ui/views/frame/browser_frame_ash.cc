// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_ash.h"

#include <memory>

#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_util.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "components/app_restore/app_restore_info.h"
#include "components/app_restore/app_restore_utils.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/views/view.h"

namespace {

// BrowserWindowStateDelegate class handles a user's fullscreen
// request (Shift+F4/F4).
class BrowserWindowStateDelegate : public ash::WindowStateDelegate {
 public:
  explicit BrowserWindowStateDelegate(Browser* browser) : browser_(browser) {
    DCHECK(browser_);
  }

  BrowserWindowStateDelegate(const BrowserWindowStateDelegate&) = delete;
  BrowserWindowStateDelegate& operator=(const BrowserWindowStateDelegate&) =
      delete;

  ~BrowserWindowStateDelegate() override {}

  // Overridden from ash::WindowStateDelegate.
  bool ToggleFullscreen(ash::WindowState* window_state) override {
    DCHECK(window_state->IsFullscreen() || window_state->CanMaximize());
    // Windows which cannot be maximized should not be fullscreened.
    if (!window_state->IsFullscreen() && !window_state->CanMaximize())
      return true;
    chrome::ToggleFullscreenMode(browser_);
    return true;
  }

  // Overridden from ash::WindowStateDelegate.
  void ToggleLockedFullscreen(ash::WindowState* window_state) override {
    ash::Shell::Get()->shell_delegate()->SetUpEnvironmentForLockedFullscreen(
        *window_state);
  }

 private:
  raw_ptr<Browser> browser_;  // not owned.
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameAsh, public:

BrowserFrameAsh::BrowserFrameAsh(BrowserFrame* browser_frame,
                                 BrowserView* browser_view)
    : views::NativeWidgetAura(browser_frame), browser_view_(browser_view) {
  GetNativeWindow()->SetName("BrowserFrameAsh");
  Browser* browser = browser_view->browser();

  created_from_drag_ = browser_frame->tab_drag_kind() != TabDragKind::kNone;

  // Turn on auto window management if we don't need an explicit bounds.
  // This way the requested bounds are honored.
  if (!browser->bounds_overridden() && !browser->is_session_restore())
    SetWindowAutoManaged();
}

BrowserFrameAsh::~BrowserFrameAsh() {}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameAsh, views::NativeWidgetAura overrides:

void BrowserFrameAsh::OnWidgetInitDone() {
  Browser* browser = browser_view_->browser();
  ash::WindowState* window_state = ash::WindowState::Get(GetNativeWindow());
  window_state->SetDelegate(
      std::make_unique<BrowserWindowStateDelegate>(browser));
  // For legacy reasons v1 apps (like Secure Shell) are allowed to consume keys
  // like brightness, volume, etc. Otherwise these keys are handled by the
  // Ash window manager.
  window_state->SetCanConsumeSystemKeys(browser->is_type_app() ||
                                        browser->is_type_app_popup());

  app_restore::AppRestoreInfo::GetInstance()->OnWidgetInitialized(GetWidget());
}

void BrowserFrameAsh::OnWindowTargetVisibilityChanged(bool visible) {
  if (visible) {
    // Once the window has been shown we know the requested bounds
    // (if provided) have been honored and we can switch on window management.
    SetWindowAutoManaged();
  }
  views::NativeWidgetAura::OnWindowTargetVisibilityChanged(visible);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameAsh, NativeBrowserFrame implementation:

bool BrowserFrameAsh::ShouldSaveWindowPlacement() const {
  return nullptr == GetWidget()->GetNativeWindow()->GetProperty(
                        ash::kRestoreBoundsOverrideKey);
}

void BrowserFrameAsh::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::mojom::WindowShowState* show_state) const {
  aura::Window* window = GetWidget()->GetNativeWindow();
  gfx::Rect* override_bounds =
      window->GetProperty(ash::kRestoreBoundsOverrideKey);
  if (override_bounds && !override_bounds->IsEmpty()) {
    *bounds = *override_bounds;
    *show_state = chromeos::ToWindowShowState(
        window->GetProperty(ash::kRestoreWindowStateTypeOverrideKey));
  } else {
    // Snapped state is a ash only state which is normally not restored except
    // when the full restore feature is turned on. `Widget::GetRestoreBounds()`
    // will not return the restore bounds for a snapped window because to
    // Widget/NativeWidgetAura the window is a normal window, so get the restore
    // bounds directly from the ash window state.
    bool used_window_state_restore_bounds = false;
    auto* window_state = ash::WindowState::Get(window);
    if (window_state->IsSnapped() && window_state->HasRestoreBounds()) {
      // Additionally, if the window is closed, and not from logging out we
      // want to use the regular restore bounds, otherwise the next time the
      // user opens a window it will be in a different place than closed,
      // since session restore does not restore ash snapped state.
      if (browser_shutdown::IsTryingToQuit() || !GetWidget()->IsClosed()) {
        used_window_state_restore_bounds = true;
        *bounds = window_state->GetRestoreBoundsInScreen();
      }
    }

    if (!used_window_state_restore_bounds)
      *bounds = GetWidget()->GetRestoredBounds();
    *show_state = window->GetProperty(aura::client::kShowStateKey);
  }

  // Session restore might be unable to correctly restore other states.
  // For the record, https://crbug.com/396272
  if (*show_state != ui::mojom::WindowShowState::kMaximized &&
      *show_state != ui::mojom::WindowShowState::kMinimized) {
    *show_state = ui::mojom::WindowShowState::kNormal;
  }
}

content::KeyboardEventProcessingResult BrowserFrameAsh::PreHandleKeyboardEvent(
    const input::NativeWebKeyboardEvent& event) {
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool BrowserFrameAsh::HandleKeyboardEvent(
    const input::NativeWebKeyboardEvent& event) {
  return false;
}

views::Widget::InitParams BrowserFrameAsh::GetWidgetParams() {
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  params.native_widget = this;
  params.context = ash::Shell::GetPrimaryRootWindow();

  Browser* browser = browser_view_->browser();
  const int32_t restore_id = browser->create_params().restore_id;
  params.init_properties_container.SetProperty(app_restore::kWindowIdKey,
                                               browser->session_id().id());
  params.init_properties_container.SetProperty(app_restore::kRestoreWindowIdKey,
                                               restore_id);

  params.init_properties_container.SetProperty(
      app_restore::kAppTypeBrowser,
      (browser->is_type_app() || browser->is_type_app_popup()));

  params.init_properties_container.SetProperty(app_restore::kBrowserAppNameKey,
                                               browser->app_name());
  params.init_properties_container.SetProperty(
      chromeos::kShouldHaveHighlightBorderOverlay, true);

  // This is only needed for ash. For lacros, Exo tags the associated
  // ShellSurface as being of AppType::LACROS.
  bool is_app = browser->is_type_app() || browser->is_type_app_popup();
  web_app::AppBrowserController* controller = browser->app_controller();
  if (controller && controller->system_app()) {
    params.init_properties_container.SetProperty(chromeos::kAppTypeKey,
                                                 chromeos::AppType::SYSTEM_APP);
  } else {
    params.init_properties_container.SetProperty(
        chromeos::kAppTypeKey,
        is_app ? chromeos::AppType::CHROME_APP : chromeos::AppType::BROWSER);
  }

  app_restore::ModifyWidgetParams(restore_id, &params);
  // Override session restore bounds with Full Restore bounds if they exist.
  if (!params.bounds.IsEmpty()) {
    browser->set_override_bounds(params.bounds);
  } else {
    params.bounds = browser->create_params().initial_bounds;
  }
  params.display_id = browser->create_params().display_id;

  return params;
}

bool BrowserFrameAsh::UseCustomFrame() const {
  return true;
}

bool BrowserFrameAsh::UsesNativeSystemMenu() const {
  return false;
}

int BrowserFrameAsh::GetMinimizeButtonOffset() const {
  return 0;
}

bool BrowserFrameAsh::ShouldRestorePreviousBrowserWidgetState() const {
  // If there is no window info from full restore, maybe use the session
  // restore.
  const int32_t restore_id =
      browser_view_->browser()->create_params().restore_id;
  // Don't restore unresizable browser apps, because they can get stuck at a
  // broken size, or the browser being dragged because it should use the
  // specified bounds.
  return !app_restore::HasWindowInfo(restore_id) &&
         browser_view_->browser()->create_params().can_resize &&
         !browser_view_->browser()->create_params().in_tab_dragging;
}

bool BrowserFrameAsh::ShouldUseInitialVisibleOnAllWorkspaces() const {
  return !created_from_drag_;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameAsh, private:

void BrowserFrameAsh::SetWindowAutoManaged() {
  // For browser window in Chrome OS, we should only enable the auto window
  // management logic for tabbed browser.
  if (browser_view_->browser()->is_type_normal())
    GetNativeWindow()->SetProperty(ash::kWindowPositionManagedTypeKey, true);
}
