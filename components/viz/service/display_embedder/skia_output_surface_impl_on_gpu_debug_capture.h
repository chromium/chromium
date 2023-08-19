// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_ON_GPU_DEBUG_CAPTURE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_ON_GPU_DEBUG_CAPTURE_H_

#include "components/viz/service/display_embedder/image_context_impl.h"

namespace gpu {
class SharedImageRepresentationFactory;
}  // namespace gpu

namespace viz {
void AttemptDebuggerBufferCapture(
    ImageContextImpl* context,
    gpu::SharedContextState* context_state,
    gpu::SharedImageRepresentationFactory* representation_factory);
}

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_ON_GPU_DEBUG_CAPTURE_H_
