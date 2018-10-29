// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GL_OUTPUT_SURFACE_BUFFER_QUEUE_ANDROID_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GL_OUTPUT_SURFACE_BUFFER_QUEUE_ANDROID_H_

#include "components/viz/service/display_embedder/gl_output_surface_buffer_queue.h"
#include "ui/gfx/buffer_types.h"

namespace viz {

class GLOutputSurfaceBufferQueueAndroid : public GLOutputSurfaceBufferQueue {
 public:
  GLOutputSurfaceBufferQueueAndroid(
      scoped_refptr<VizProcessContextProvider> context_provider,
      gpu::SurfaceHandle surface_handle,
      SyntheticBeginFrameSource* synthetic_begin_frame_source,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      gfx::BufferFormat buffer_format);
  ~GLOutputSurfaceBufferQueueAndroid() override;

  // GLOutputSurfaceBufferQueue implementation:
  void HandlePartialSwap(
      const gfx::Rect& sub_buffer_rect,
      uint32_t flags,
      gpu::ContextSupport::SwapCompletedCallback swap_callback,
      gpu::ContextSupport::PresentationCallback presentation_callback) override;
  OverlayCandidateValidator* GetOverlayCandidateValidator() const override;

 private:
  std::unique_ptr<OverlayCandidateValidator> overlay_candidate_validator_;

  DISALLOW_COPY_AND_ASSIGN(GLOutputSurfaceBufferQueueAndroid);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GL_OUTPUT_SURFACE_BUFFER_QUEUE_ANDROID_H_
