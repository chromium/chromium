// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view_ash.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/wm/splitview/layout_divider_controller.h"
#include "ash/wm/window_state.h"
#include "base/check.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/picture_in_picture/picture_in_picture_pin_button.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "chromeos/ui/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/hit_test.h"
#include "ui/views/background.h"

PictureInPictureBrowserFrameViewAsh::PictureInPictureBrowserFrameViewAsh(
    BrowserWidget* browser_widget,
    BrowserView* browser_view)
    : PictureInPictureBrowserFrameView(browser_widget, browser_view) {
  // Creates the pin button, which toggles the window manager's auto layout of
  // the PiP window, and places it before the back-to-tab and close buttons.
  if (browser_view->GetIsPictureInPictureType() &&
      PictureInPicturePinButton::IsEnabled()) {
    pin_button_ = button_container_view()->AddChildViewAt(
        std::make_unique<PictureInPicturePinButton>(),
        content_setting_views_count());
    animation_controller()->SetPinButton(pin_button_);
  }

  aura::Window* frame_window = browser_widget->GetNativeWindow();
  chromeos::wm::InstallResizeHandleWindowTargeterForWindow(frame_window);

  window_observation_.Observe(frame_window);
}

PictureInPictureBrowserFrameViewAsh::~PictureInPictureBrowserFrameViewAsh() =
    default;

void PictureInPictureBrowserFrameViewAsh::OnBrowserViewInitViewsComplete() {
  PictureInPictureBrowserFrameView::OnBrowserViewInitViewsComplete();

  // Let ash manage the Document PiP window in the same way as the video PiP
  // window. This is done after the widget and its views are initialized
  // because entering the ash PiP state updates the window bounds, which
  // requires the frame view. The auto layout is off by default for Document
  // PiP, so the window stays where it is placed.
  if (GetBrowserView()->GetIsPictureInPictureType() &&
      ash::features::IsPipAutoLayoutToggleEnabled()) {
    aura::Window* window = GetWidget()->GetNativeWindow();
    window->SetProperty(ash::kPipParamsKey,
                        ash::PipParams{.auto_layout = false,
                                       .activatable = true,
                                       .double_tap_to_resize = false});
    window->SetProperty(ash::kWindowPipTypeKey, true);
  }
}

int PictureInPictureBrowserFrameViewAsh::NonClientHitTest(
    const gfx::Point& point) {
  if (pin_button_ && animation_controller()->is_top_bar_active() &&
      ConvertTopBarControlViewBounds(pin_button_, button_container_view())
          .Contains(point)) {
    return HTCLIENT;
  }
  return PictureInPictureBrowserFrameView::NonClientHitTest(point);
}

void PictureInPictureBrowserFrameViewAsh::UpdateWindowRoundedCorners() {
  aura::Window* window = GetWidget()->GetNativeWindow();
  const gfx::RoundedCornersF window_radii =
      ash::WindowState::Get(window)->GetWindowRoundedCorners();

  const gfx::RoundedCornersF radii(window_radii.upper_left(),
                                   window_radii.upper_right(), 0, 0);
  top_bar_container_view()->SetBackground(views::CreateRoundedRectBackground(
      kColorPipWindowTopBarBackground, radii));

  GetWidget()->client_view()->UpdateWindowRoundedCorners(window_radii);
}

void PictureInPictureBrowserFrameViewAsh::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  if (key == chromeos::kWindowHasRoundedCornersKey) {
    UpdateWindowRoundedCorners();
  }
}

void PictureInPictureBrowserFrameViewAsh::OnWindowDestroyed(
    aura::Window* window) {
  CHECK(window_observation_.IsObservingSource(GetWidget()->GetNativeWindow()),
        base::NotFatalUntil::M162);
  window_observation_.Reset();
}

gfx::Insets PictureInPictureBrowserFrameViewAsh::ResizeBorderInsets() const {
  return gfx::Insets(chromeos::kResizeInsideBoundsSize);
}
