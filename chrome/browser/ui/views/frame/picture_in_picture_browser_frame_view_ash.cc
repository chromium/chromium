// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view_ash.h"

#include "ash/wm/splitview/layout_divider_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/check.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/views/background.h"

PictureInPictureBrowserFrameViewAsh::PictureInPictureBrowserFrameViewAsh(
    BrowserWidget* browser_widget,
    BrowserView* browser_view)
    : PictureInPictureBrowserFrameView(browser_widget, browser_view) {
  aura::Window* frame_window = browser_widget->GetNativeWindow();
  ash::window_util::SetChildrenUseExtendedHitRegionForWindow(
      frame_window->parent());
  ash::window_util::InstallResizeHandleWindowTargeterForWindow(frame_window);

  window_observation_.Observe(frame_window);
}

PictureInPictureBrowserFrameViewAsh::~PictureInPictureBrowserFrameViewAsh() =
    default;

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
  DCHECK(window_observation_.IsObservingSource(GetWidget()->GetNativeWindow()));
  window_observation_.Reset();
}

gfx::Insets PictureInPictureBrowserFrameViewAsh::ResizeBorderInsets() const {
  return gfx::Insets(chromeos::kResizeInsideBoundsSize);
}
