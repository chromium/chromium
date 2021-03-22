// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_ash.h"

#include <memory>

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_util.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chromeos/ui/base/window_state_type.h"
#include "components/full_restore/full_restore_info.h"
#include "components/full_restore/full_restore_utils.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/views/view.h"

namespace {

// BrowserWindowStateDelegate class handles a user's fullscreen
// request (Shift+F4/F4).
class BrowserWindowStateDelegate : public ash::WindowStateDelegate {
 public:
  explicit BrowserWindowStateDelegate(Browser* browser)
      : browser_(browser) {
    DCHECK(browser_);
  }
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

 private:
  Browser* browser_;  // not owned.

  DISALLOW_COPY_AND_ASSIGN(BrowserWindowStateDelegate);
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameAsh, public:

BrowserFrameAsh::BrowserFrameAsh(BrowserFrame* browser_frame,
                                 BrowserView* browser_view)
    : views::NativeWidgetAura(browser_frame),
      browser_view_(browser_view) {
  GetNativeWindow()->SetName("BrowserFrameAsh");
  Browser* browser = browser_view->browser();

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
  window_state->SetCanConsumeSystemKeys(browser->deprecated_is_app());

  full_restore::FullRestoreInfo::GetInstance()->OnWidgetInitialized(
      GetWidget());
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
    ui::WindowShowState* show_state) const {
  gfx::Rect* override_bounds = GetWidget()->GetNativeWindow()->GetProperty(
                                   ash::kRestoreBoundsOverrideKey);
  if (override_bounds && !override_bounds->IsEmpty()) {
    *bounds = *override_bounds;
    *show_state =
        chromeos::ToWindowShowState(GetWidget()->GetNativeWindow()->GetProperty(
            ash::kRestoreWindowStateTypeOverrideKey));
  } else {
    *bounds = GetWidget()->GetRestoredBounds();
    *show_state = GetWidget()->GetNativeWindow()->GetProperty(
                      aura::client::kShowStateKey);
  }

  // Session restore might be unable to correctly restore other states.
  // For the record, https://crbug.com/396272
  if (*show_state != ui::SHOW_STATE_MAXIMIZED &&
      *show_state != ui::SHOW_STATE_MINIMIZED) {
    *show_state = ui::SHOW_STATE_NORMAL;
  }
}

content::KeyboardEventProcessingResult BrowserFrameAsh::PreHandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool BrowserFrameAsh::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  return false;
}

views::Widget::InitParams BrowserFrameAsh::GetWidgetParams() {
  views::Widget::InitParams params;
  params.native_widget = this;
  params.context = ash::Shell::GetPrimaryRootWindow();

  Browser* browser = browser_view_->browser();
  const int32_t restore_id = browser->create_params().restore_id;
  params.init_properties_container.SetProperty(full_restore::kWindowIdKey,
                                               browser->session_id().id());
  params.init_properties_container.SetProperty(
      full_restore::kRestoreWindowIdKey, restore_id);

  // This is only needed for ash. For lacros, Exo tags the associated
  // ShellSurface as being of AppType::LACROS.
  params.init_properties_container.SetProperty(
      aura::client::kAppType,
      static_cast<int>(browser->deprecated_is_app() ? ash::AppType::CHROME_APP
                                                    : ash::AppType::BROWSER));

  full_restore::ModifyWidgetParams(restore_id, &params);
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
  return !full_restore::HasWindowInfo(restore_id);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameAsh, private:

void BrowserFrameAsh::SetWindowAutoManaged() {
  // For browser window in Chrome OS, we should only enable the auto window
  // management logic for tabbed browser.
  if (browser_view_->browser()->is_type_normal())
    GetNativeWindow()->SetProperty(ash::kWindowPositionManagedTypeKey, true);
}
