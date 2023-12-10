// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_BASE_DISPLAY_UTIL_H_
#define CHROMEOS_UI_BASE_DISPLAY_UTIL_H_

#include "ui/display/display_util.h"

namespace chromeos {

enum class OrientationType {
  kAny,
  kNatural,
  kCurrent,
  kPortrait,
  kLandscape,
  kPortraitPrimary,
  kPortraitSecondary,
  kLandscapePrimary,
  kLandscapeSecondary,
};

// Returns the orientation of a |display| at a rotation of 0.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
OrientationType GetDisplayNaturalOrientation(const display::Display& display);

// Returns orientation type when rotating a display with |natural|
// orientation with |rotation|.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
OrientationType RotationToOrientation(OrientationType natural,
                                      display::Display::Rotation rotation);

// Returns the rotation that matches the orientation type.
// Returns ROTATE_0 if the given orientation is ANY, which is used
// to indicate that user didn't lock orientation.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
display::Display::Rotation OrientationToRotation(OrientationType natural,
                                                 OrientationType orientation);

// Test if the orientation type is primary/landscape/portrait.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
bool IsPrimaryOrientation(OrientationType type);
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
bool IsLandscapeOrientation(OrientationType type);
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
bool IsPortraitOrientation(OrientationType type);

// Returns the current orientation of `display`.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
OrientationType GetDisplayCurrentOrientation(const display::Display& display);

// Returns true if the current layout of |display| is primary.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
bool IsDisplayLayoutPrimary(const display::Display& display);

// Given a list of displays, selects one DSF to represent them all.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
float GetRepresentativeDeviceScaleFactor(
    const std::vector<display::Display>& displays);

}  // namespace chromeos

#endif  // CHROMEOS_UI_BASE_DISPLAY_UTIL_H_
