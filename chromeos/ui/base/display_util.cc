// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/base/display_util.h"
#include "ui/display/display.h"

namespace chromeos {

OrientationType GetDisplayNaturalOrientation(const display::Display& display) {
  // ROTATE_0 is natural orientation and primary and the size of the display
  // is updated along with the rotation. Thus, when width > height for ROTATE_0
  // and ROTATE_180, the natural orientation is then landscape. Meanwhile,
  // width > height for ROTATE_90 and ROTATE_270 indicates portrait layout at
  // ROTATE_0 which is the natural orientation.
  display::Display::Rotation rotation = display.rotation();
  bool is_landscape = display.is_landscape();
  if (rotation == display::Display::ROTATE_90 ||
      rotation == display::Display::ROTATE_270) {
    is_landscape = !is_landscape;
  }
  return is_landscape ? OrientationType::kLandscape
                      : OrientationType::kPortrait;
}

OrientationType RotationToOrientation(OrientationType natural,
                                      display::Display::Rotation rotation) {
  if (natural == OrientationType::kLandscape) {
    // To be consistent with Android, the rotation of the primary portrait
    // on naturally landscape device is 270.
    switch (rotation) {
      case display::Display::ROTATE_0:
        return OrientationType::kLandscapePrimary;
      case display::Display::ROTATE_90:
        return OrientationType::kPortraitSecondary;
      case display::Display::ROTATE_180:
        return OrientationType::kLandscapeSecondary;
      case display::Display::ROTATE_270:
        return OrientationType::kPortraitPrimary;
    }
  } else {  // Natural portrait
    switch (rotation) {
      case display::Display::ROTATE_0:
        return OrientationType::kPortraitPrimary;
      case display::Display::ROTATE_90:
        return OrientationType::kLandscapePrimary;
      case display::Display::ROTATE_180:
        return OrientationType::kPortraitSecondary;
      case display::Display::ROTATE_270:
        return OrientationType::kLandscapeSecondary;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return OrientationType::kAny;
}

display::Display::Rotation OrientationToRotation(OrientationType natural,
                                                 OrientationType orientation) {
  if (orientation == OrientationType::kAny)
    return display::Display::ROTATE_0;

  if (natural == OrientationType::kLandscape) {
    // To be consistent with Android, the rotation of the primary portrait
    // on naturally landscape device is 270.
    switch (orientation) {
      case OrientationType::kLandscapePrimary:
        return display::Display::ROTATE_0;
      case OrientationType::kPortraitPrimary:
        return display::Display::ROTATE_270;
      case OrientationType::kLandscapeSecondary:
        return display::Display::ROTATE_180;
      case OrientationType::kPortraitSecondary:
        return display::Display::ROTATE_90;
      default:
        break;
    }
  } else {  // Natural portrait
    switch (orientation) {
      case OrientationType::kPortraitPrimary:
        return display::Display::ROTATE_0;
      case OrientationType::kLandscapePrimary:
        return display::Display::ROTATE_90;
      case OrientationType::kPortraitSecondary:
        return display::Display::ROTATE_180;
      case OrientationType::kLandscapeSecondary:
        return display::Display::ROTATE_270;
      default:
        break;
    }
  }
  NOTREACHED_IN_MIGRATION() << static_cast<int>(orientation);
  return display::Display::ROTATE_0;
}

bool IsPrimaryOrientation(OrientationType type) {
  return type == OrientationType::kLandscapePrimary ||
         type == OrientationType::kPortraitPrimary;
}

bool IsLandscapeOrientation(OrientationType type) {
  return type == OrientationType::kLandscape ||
         type == OrientationType::kLandscapePrimary ||
         type == OrientationType::kLandscapeSecondary;
}

bool IsPortraitOrientation(OrientationType type) {
  return type == OrientationType::kPortrait ||
         type == OrientationType::kPortraitPrimary ||
         type == OrientationType::kPortraitSecondary;
}

OrientationType GetDisplayCurrentOrientation(const display::Display& display) {
  DCHECK(display.is_valid());
  const display::Display::Rotation rotation = display.rotation();
  return RotationToOrientation(GetDisplayNaturalOrientation(display), rotation);
}

bool IsDisplayLayoutPrimary(const display::Display& display) {
  DCHECK(display.is_valid());
  return IsPrimaryOrientation(GetDisplayCurrentOrientation(display));
}

float GetRepresentativeDeviceScaleFactor(
    const std::vector<display::Display>& displays) {
  float largest_device_scale_factor = 1.0f;
  for (const display::Display& display : displays) {
    largest_device_scale_factor =
        std::max(largest_device_scale_factor, display.device_scale_factor());
  }
  return largest_device_scale_factor;
}

}  // namespace chromeos
