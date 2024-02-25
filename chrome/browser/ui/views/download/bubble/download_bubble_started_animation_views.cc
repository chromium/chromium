// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_started_animation_views.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/download/download_started_animation_views.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {

// The animation is piecewise linear, composed of 2 phases, phase one is 300 ms
// and phase two is 200 ms.
constexpr base::TimeDelta kAnimationDuration = base::Milliseconds(500);
constexpr double kAnimationPhaseSwitchProgress = 0.6;
constexpr int kIconSize = 26;
constexpr int kIconBackgroundRadius = 32;

ui::ImageModel GetDownloadIconImageModel(SkColor image_foreground_color,
                                         SkColor image_background_color) {
  gfx::ImageSkia icon = gfx::CreateVectorIcon(gfx::IconDescription(
      kDownloadToolbarButtonIcon, kIconSize, image_foreground_color));
  gfx::ImageSkia image =
      gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
          kIconBackgroundRadius, image_background_color, icon);
  return ui::ImageModel::FromImageSkia(image);
}

// Rescales the current progress value of the animation such that the progress
// goes from 0.0 to 1.0 within phase one.
double GetPhaseOneProgress(double current_value) {
  DCHECK_LE(current_value, kAnimationPhaseSwitchProgress);
  return current_value / kAnimationPhaseSwitchProgress;
}

// Rescales the current progress value of the animation such that the progress
// goes from 0.0 to 1.0 within phase two.
double GetPhaseTwoProgress(double current_value) {
  DCHECK_GE(current_value, kAnimationPhaseSwitchProgress);
  return (current_value - kAnimationPhaseSwitchProgress) /
         (1.0 - kAnimationPhaseSwitchProgress);
}

// Breaks down the y-axis movement for each phase for simplicity.
int GetYForPhaseOne(double phase_one_progress,
                    const gfx::Rect& web_contents_bounds,
                    const gfx::Rect& toolbar_icon_bounds,
                    const gfx::Size& image_size) {
  // Start centered at 50% of viewport height.
  const int start =
      web_contents_bounds.CenterPoint().y() - image_size.height() / 2;
  // End phase one at 40 px below the center of the toolbar icon.
  const int end = toolbar_icon_bounds.bottom_center().y() + 40;
  return static_cast<int>(start + (end - start) * phase_one_progress);
}

int GetYForPhaseTwo(double phase_two_progress,
                    const gfx::Rect& web_contents_bounds,
                    const gfx::Rect& toolbar_icon_bounds,
                    const gfx::Size& image_size) {
  // Start where phase one ended.
  const int start = toolbar_icon_bounds.bottom_center().y() + 40;
  // End centered over the toolbar icon.
  const int end =
      toolbar_icon_bounds.CenterPoint().y() - image_size.height() / 2;
  return static_cast<int>(start + (end - start) * phase_two_progress);
}

// Breaks down the opacity changes for each phase for simplicity.
float GetOpacityForPhaseOne(double phase_one_progress) {
  // Go from 0 to 1 opacity.
  return static_cast<float>(phase_one_progress);
}

float GetOpacityForPhaseTwo(double phase_two_progress) {
  // Go from 1 to 0 opacity.
  return static_cast<float>(1 - phase_two_progress);
}

}  // namespace

DownloadBubbleStartedAnimationViews::DownloadBubbleStartedAnimationViews(
    content::WebContents* web_contents,
    const gfx::Rect& toolbar_icon_bounds,
    SkColor image_foreground_color,
    SkColor image_background_color)
    : DownloadStartedAnimationViews(
          web_contents,
          kAnimationDuration,
          GetDownloadIconImageModel(image_foreground_color,
                                    image_background_color)),
      toolbar_icon_bounds_(toolbar_icon_bounds) {}

DownloadBubbleStartedAnimationViews::~DownloadBubbleStartedAnimationViews() =
    default;

int DownloadBubbleStartedAnimationViews::GetX() const {
  // Align the centers of the animation and the toolbar icon.
  return toolbar_icon_bounds_.CenterPoint().x() -
         GetPreferredSize().width() / 2;
}

int DownloadBubbleStartedAnimationViews::GetY() const {
  if (GetCurrentValue() <= kAnimationPhaseSwitchProgress) {
    return GetYForPhaseOne(GetPhaseOneProgress(GetCurrentValue()),
                           web_contents_bounds(), toolbar_icon_bounds_,
                           GetPreferredSize());
  }
  return GetYForPhaseTwo(GetPhaseTwoProgress(GetCurrentValue()),
                         web_contents_bounds(), toolbar_icon_bounds_,
                         GetPreferredSize());
}

float DownloadBubbleStartedAnimationViews::GetOpacity() const {
  if (GetCurrentValue() <= kAnimationPhaseSwitchProgress) {
    return GetOpacityForPhaseOne(GetPhaseOneProgress(GetCurrentValue()));
  }
  return GetOpacityForPhaseTwo(GetPhaseTwoProgress(GetCurrentValue()));
}

bool DownloadBubbleStartedAnimationViews::WebContentsTooSmall(
    const gfx::Size& image_size) const {
  return web_contents_bounds().height() < image_size.height() + 40;
}

BEGIN_METADATA(DownloadBubbleStartedAnimationViews)
END_METADATA
