// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METAL_UTIL_HDR_COPIER_LAYER_H_
#define COMPONENTS_METAL_UTIL_HDR_COPIER_LAYER_H_

#include <IOSurface/IOSurfaceRef.h>

#include <optional>

#include "components/metal_util/metal_util_export.h"
#include "ui/gfx/hdr_metadata.h"

namespace gfx {
class ColorSpace;
struct HDRMetadata;
}  // namespace gfx

@class CALayer;
@protocol MTLDevice;

namespace metal {

// Return true if we should use the HDRCopier for the specified content.
bool METAL_UTIL_EXPORT ShouldUseHDRCopier(IOSurfaceRef buffer,
                                          const gfx::HDRMetadata& hdr_metadata,
                                          const gfx::ColorSpace& color_space);

// Create a layer which may have its contents set an HDR IOSurface via
// UpdateHDRCopierLayer.
CALayer* METAL_UTIL_EXPORT MakeHDRCopierLayer();

// Update the contents of |layer| to the specified IOSurface and color space.
// If `metal_device` is non-zero, then it is the MTLDevice that the
// CAMetaLLayer should be set to. Set |screen_hdr_headroom| to the HDR headroom
// of the screen this layer is being displayed on.
void METAL_UTIL_EXPORT
UpdateHDRCopierLayer(CALayer* layer,
                     IOSurfaceRef buffer,
                     id<MTLDevice> device,
                     float screen_hdr_headroom,
                     const gfx::ColorSpace& color_space,
                     const std::optional<gfx::HDRMetadata>& hdr_metadata);

}  // namespace metal

#endif  // COMPONENTS_METAL_UTIL_HDR_COPIER_LAYER_H_
