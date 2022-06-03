// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METAL_UTIL_HDR_COPIER_LAYER_H_
#define COMPONENTS_METAL_UTIL_HDR_COPIER_LAYER_H_

#include "components/metal_util/metal_util_export.h"

#include <IOSurface/IOSurface.h>

namespace gfx {
class ColorSpace;
}  // namespace gfx

@class CALayer;

namespace metal {

// Return true if we should use the HDRCopier for the specified content.
bool METAL_UTIL_EXPORT ShouldUseHDRCopier(IOSurfaceRef buffer,
                                          const gfx::ColorSpace& color_space);

// Create a layer which may have its contents set an HDR IOSurface via
// UpdateHDRCopierLayer.
CALayer* METAL_UTIL_EXPORT CreateHDRCopierLayer();

// Update the contents of |layer| to the specified IOSurface and color space.
void METAL_UTIL_EXPORT UpdateHDRCopierLayer(CALayer* layer,
                                            IOSurfaceRef buffer,
                                            const gfx::ColorSpace& color_space);

}  // namespace metal

#endif  // COMPONENTS_METAL_UTIL_HDR_COPIER_LAYER_H_
