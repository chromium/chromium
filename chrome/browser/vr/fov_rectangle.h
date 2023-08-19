// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_FOV_RECTANGLE_H_
#define CHROME_BROWSER_VR_FOV_RECTANGLE_H_

#include <utility>

#include "chrome/browser/vr/vr_base_export.h"

namespace vr {

struct FovRectangle {
  float left;
  float right;
  float bottom;
  float top;
};

using FovRectangles = std::pair<FovRectangle, FovRectangle>;

}  // namespace vr

#endif  // CHROME_BROWSER_VR_FOV_RECTANGLE_H_
