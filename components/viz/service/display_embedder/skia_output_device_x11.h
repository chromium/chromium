// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_X11_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_X11_H_

#include <vector>

#include "base/macros.h"
#include "components/viz/service/display_embedder/skia_output_device_offscreen.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto.h"

namespace viz {

class SkiaOutputDeviceX11 final : public SkiaOutputDeviceOffscreen {
 public:
  SkiaOutputDeviceX11(
      scoped_refptr<gpu::SharedContextState> context_state,
      gfx::AcceleratedWidget widget,
      gpu::MemoryTracker* memory_tracker,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback);
  ~SkiaOutputDeviceX11() override;

  bool Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               gfx::BufferFormat format,
               gfx::OverlayTransform transform) override;
  void SwapBuffers(BufferPresentedCallback feedback,
                   std::vector<ui::LatencyInfo> latency_info) override;
  void PostSubBuffer(const gfx::Rect& rect,
                     BufferPresentedCallback feedback,
                     std::vector<ui::LatencyInfo> latency_info) override;

 private:
  x11::Connection* connection_ = nullptr;
  x11::Window window_ = x11::Window::None;
  x11::GraphicsContext gc_{};
  x11::VisualId visual_{};
  scoped_refptr<base::RefCountedMemory> pixels_;

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputDeviceX11);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_X11_H_
