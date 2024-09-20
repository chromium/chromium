// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_OFFSCREEN_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_OFFSCREEN_H_

#include <vector>

#include "components/viz/service/display_embedder/skia_output_device.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/gpu/graphite/BackendTexture.h"

namespace viz {

class SkiaOutputDeviceOffscreen : public SkiaOutputDevice {
 public:
  SkiaOutputDeviceOffscreen(
      scoped_refptr<gpu::SharedContextState> context_state,
      gfx::SurfaceOrigin origin,
      bool has_alpha,
      gpu::MemoryTracker* memory_tracker,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback);

  SkiaOutputDeviceOffscreen(const SkiaOutputDeviceOffscreen&) = delete;
  SkiaOutputDeviceOffscreen& operator=(const SkiaOutputDeviceOffscreen&) =
      delete;

  ~SkiaOutputDeviceOffscreen() override;

  // SkiaOutputDevice implementation:
  bool Reshape(const ReshapeParams& params) override;
  void Present(const std::optional<gfx::Rect>& update_rect,
               BufferPresentedCallback feedback,
               OutputSurfaceFrame frame) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  SkSurface* BeginPaint(
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void EndPaint() override;
  void ReadbackForTesting(base::OnceCallback<void(SkBitmap)> callback) override;

 protected:
  scoped_refptr<gpu::SharedContextState> context_state_;
  const bool has_alpha_;
  sk_sp<SkSurface> sk_surface_;
  GrBackendTexture backend_texture_;
  skgpu::graphite::BackendTexture graphite_texture_;
  bool supports_rgbx_ = true;
  gfx::Size size_;
  SkColorType sk_color_type_ = kUnknown_SkColorType;
  sk_sp<SkColorSpace> sk_color_space_;
  int sample_count_ = 1;

 private:
  uint64_t backbuffer_estimated_size_ = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_OFFSCREEN_H_
