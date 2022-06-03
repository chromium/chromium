// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GL_OUTPUT_SURFACE_ANDROID_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GL_OUTPUT_SURFACE_ANDROID_H_

#include "components/viz/service/display_embedder/gl_output_surface.h"

namespace viz {
class GLOutputSurfaceAndroid : public GLOutputSurface {
 public:
  GLOutputSurfaceAndroid(
      scoped_refptr<VizProcessContextProvider> context_provider,
      gpu::SurfaceHandle surface_handle);

  GLOutputSurfaceAndroid(const GLOutputSurfaceAndroid&) = delete;
  GLOutputSurfaceAndroid& operator=(const GLOutputSurfaceAndroid&) = delete;

  ~GLOutputSurfaceAndroid() override;

  // GLOutputSurface implementation:
  void HandlePartialSwap(
      const gfx::Rect& sub_buffer_rect,
      uint32_t flags,
      gpu::ContextSupport::SwapCompletedCallback swap_callback,
      gpu::ContextSupport::PresentationCallback presentation_callback) override;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GL_OUTPUT_SURFACE_ANDROID_H_
