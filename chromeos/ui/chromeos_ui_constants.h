// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_CHROMEOS_UI_CONSTANTS_H_
#define CHROMEOS_UI_CHROMEOS_UI_CONSTANTS_H_

#include "third_party/skia/include/core/SkColor.h"

namespace chromeos {

// In the window corners, the resize areas don't actually expand bigger, but the
// 16 px at the end of each edge triggers diagonal resizing.
constexpr int kResizeAreaCornerSize = 16;

// Ash windows do not have a traditional visible window frame. Window content
// extends to the edge of the window. We consider a small region outside the
// window bounds and an even smaller region overlapping the window to be the
// "non-client" area and use it for resizing.
constexpr int kResizeOutsideBoundsSize = 6;
constexpr int kResizeOutsideBoundsScaleForTouch = 5;
constexpr int kResizeInsideBoundsSize = 1;

// The default frame color.
constexpr SkColor kDefaultFrameColor = SkColorSetRGB(0xFD, 0xFE, 0xFF);

}  // namespace chromeos

#endif  // CHROMEOS_UI_CHROMEOS_UI_CONSTANTS_H_
