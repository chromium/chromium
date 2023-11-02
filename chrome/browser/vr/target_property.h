// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TARGET_PROPERTY_H_
#define CHROME_BROWSER_VR_TARGET_PROPERTY_H_

namespace vr {

// Must be zero-based as this will be stored in a bitset.
enum TargetProperty {
  TRANSFORM = 0,
  LAYOUT_OFFSET,
  OPACITY,
  BOUNDS,
  BACKGROUND_COLOR,
  FOREGROUND_COLOR,
  GRID_COLOR,
  SPINNER_ANGLE_START,
  SPINNER_ANGLE_SWEEP,
  SPINNER_ROTATION,
  CIRCLE_GROW,
  NORMAL_COLOR_FACTOR,
  INCOGNITO_COLOR_FACTOR,
  FULLSCREEN_COLOR_FACTOR,
  LOCAL_OPACITY,

  // This must be last.
  NUM_TARGET_PROPERTIES
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TARGET_PROPERTY_H_
