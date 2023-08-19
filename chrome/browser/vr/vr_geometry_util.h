// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_VR_GEOMETRY_UTIL_H_
#define CHROME_BROWSER_VR_VR_GEOMETRY_UTIL_H_

#include "chrome/browser/vr/vr_base_export.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class RectF;
class Size;
}  // namespace gfx

namespace vr {

VR_BASE_EXPORT gfx::Rect CalculatePixelSpaceRect(
    const gfx::Size& texture_size,
    const gfx::RectF& texture_rect);

}  // namespace vr

#endif  // CHROME_BROWSER_VR_VR_GEOMETRY_UTIL_H_
