// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/picture_in_picture_tucker.h"

#include "chrome/browser/ui/views/picture_in_picture/picture_in_picture_bounds_change_animation.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr int kTuckEdgeLength = 50;

bool VerticallyIntersects(const gfx::Rect& r1, const gfx::Rect& r2) {
  return std::max(r1.y(), r2.y()) < std::min(r1.bottom(), r2.bottom());
}

}  // namespace

PictureInPictureTucker::PictureInPictureTucker(views::Widget& pip_widget)
    : pip_widget_(pip_widget) {}

PictureInPictureTucker::~PictureInPictureTucker() = default;

void PictureInPictureTucker::Tuck() {
  gfx::Rect current_bounds = pip_widget_->GetWindowBoundsInScreen();

  if (!tucking_) {
    pretuck_location_ = current_bounds.origin();
  }

  tucking_ = true;

  // Get a list of all displays that we could move into if we only move
  // horizontally.
  std::vector<display::Display> possible_displays;
  for (const auto& display : display::Screen::Get()->GetAllDisplays()) {
    if (VerticallyIntersects(display.work_area(), current_bounds)) {
      possible_displays.push_back(display);
    }
  }

  // If somehow that list is empty (in most cases this shouldn't happen since
  // the display we're on should count, but in certain test scenarios this could
  // be the case), then just add the display we're currently on.
  if (possible_displays.empty()) {
    possible_displays.push_back(display::Screen::Get()->GetDisplayNearestWindow(
        pip_widget_->GetNativeWindow()));
  }

  gfx::Rect combined_work_area;
  for (const auto& display : possible_displays) {
    combined_work_area.Union(display.work_area());
  }

  gfx::Rect destination_bounds = current_bounds;

  // Tuck the picture-in-picture window to the closest left or right edge of the
  // combined screen area of displays.
  if (current_bounds.CenterPoint().x() < combined_work_area.CenterPoint().x()) {
    destination_bounds.set_x(combined_work_area.x() + kTuckEdgeLength -
                             current_bounds.width());
  } else {
    destination_bounds.set_x(combined_work_area.x() +
                             combined_work_area.width() - kTuckEdgeLength);
  }

  animation_ = std::make_unique<PictureInPictureBoundsChangeAnimation>(
      *pip_widget_, destination_bounds);
  animation_->Start();
  if (pip_widget_->IsActive()) {
    pip_widget_->Deactivate();
  }
}

void PictureInPictureTucker::Untuck() {
  // It's possible to be told to untuck before tucking has actually occurred if
  // tucking begins and ends before a `PictureInPictureBrowserFrameView` becomes
  // visible. In that case, there's nothing to do.
  if (!tucking_) {
    return;
  }
  tucking_ = false;
  gfx::Rect destination_bounds = pip_widget_->GetWindowBoundsInScreen();
  destination_bounds.set_origin(pretuck_location_);
  animation_ = std::make_unique<PictureInPictureBoundsChangeAnimation>(
      *pip_widget_, destination_bounds);
  animation_->Start();
}

void PictureInPictureTucker::FinishAnimationForTesting() {
  if (animation_) {
    animation_->End();
  }
}
