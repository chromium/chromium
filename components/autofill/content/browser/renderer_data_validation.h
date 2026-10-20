// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_RENDERER_DATA_VALIDATION_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_RENDERER_DATA_VALIDATION_H_

#include "ui/gfx/geometry/rect_f.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace autofill {

// Transforms `r` from `rfh`'s local root's coordinate system to the main
// frame's coordinate system, clamping it to `rfh`'s view's bounds and all
// ancestor views' bounds.
gfx::RectF TransformToRootCoordinates(content::RenderFrameHost& rfh,
                                      gfx::RectF r);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_RENDERER_DATA_VALIDATION_H_
