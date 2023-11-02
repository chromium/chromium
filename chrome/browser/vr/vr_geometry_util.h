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
class SizeF;
class Transform;
}  // namespace gfx

namespace vr {

VR_BASE_EXPORT gfx::Rect CalculatePixelSpaceRect(
    const gfx::Size& texture_size,
    const gfx::RectF& texture_rect);

// Returns the normalized size of the element projected into screen space.
// If (1, 1) the element fills the entire buffer.
VR_BASE_EXPORT gfx::SizeF CalculateScreenSize(const gfx::Transform& proj_matrix,
                                              float distance,
                                              const gfx::SizeF& size);

}  // namespace vr

#endif  // CHROME_BROWSER_VR_VR_GEOMETRY_UTIL_H_
