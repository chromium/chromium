// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view_ash.h"

#include "ash/wm/splitview/layout_divider_controller.h"
#include "ash/wm/window_util.h"
#include "base/check.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "ui/aura/client/aura_constants.h"

PictureInPictureBrowserFrameViewAsh::PictureInPictureBrowserFrameViewAsh(
    BrowserFrame* frame,
    BrowserView* browser_view)
    : PictureInPictureBrowserFrameView(frame, browser_view) {
  aura::Window* frame_window = frame->GetNativeWindow();
  ash::window_util::SetChildrenUseExtendedHitRegionForWindow(
      frame_window->parent());
  ash::window_util::InstallResizeHandleWindowTargeterForWindow(frame_window);

  window_observation_.Observe(frame_window);
}

PictureInPictureBrowserFrameViewAsh::~PictureInPictureBrowserFrameViewAsh() =
    default;

void PictureInPictureBrowserFrameViewAsh::UpdateWindowRoundedCorners() {
  if (!chromeos::features::IsRoundedWindowsEnabled()) {
    return;
  }

  aura::Window* frame_window = GetWidget()->GetNativeWindow();
  const int corner_radius = chromeos::GetFrameCornerRadius(frame_window);

  frame_window->SetProperty(aura::client::kWindowCornerRadiusKey,
                            corner_radius);

  const gfx::RoundedCornersF radii(corner_radius, corner_radius, 0, 0);
  top_bar_container_view()->SetPaintToLayer();
  top_bar_container_view()->layer()->SetRoundedCornerRadius(radii);
  top_bar_container_view()->layer()->SetIsFastRoundedCorner(/*enable=*/true);

  GetWidget()->client_view()->UpdateWindowRoundedCorners(corner_radius);
}

void PictureInPictureBrowserFrameViewAsh::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  if (chromeos::CanPropertyEffectFrameRadius(key)) {
    UpdateWindowRoundedCorners();
  }
}

void PictureInPictureBrowserFrameViewAsh::OnWindowDestroyed(
    aura::Window* window) {
  DCHECK(window_observation_.IsObservingSource(frame()->GetNativeWindow()));
  window_observation_.Reset();
}

gfx::Insets PictureInPictureBrowserFrameViewAsh::ResizeBorderInsets() const {
  return gfx::Insets(chromeos::kResizeInsideBoundsSize);
}
