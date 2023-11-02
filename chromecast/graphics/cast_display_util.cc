// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_display_util.h"

#include <math.h>
#include <algorithm>

namespace chromecast {

float GetDeviceScaleFactor(const gfx::Size& display_resolution) {
  int smaller_dimension =
      std::min(display_resolution.width(), display_resolution.height());
  float ratio = smaller_dimension / 720.f;
  if (ratio >= 2.f)
    return floorf(ratio);
  if (ratio >= 1.5f)
    return 1.5f;
  return 1.f;
}

}  // namespace chromecast
