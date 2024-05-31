// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_X11_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_X11_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "components/viz/service/display_embedder/skia_output_device_offscreen.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto.h"

namespace viz {

class SkiaOutputDeviceX11 final : public SkiaOutputDeviceOffscreen {
 public:
  SkiaOutputDeviceX11(
      base::PassKey<SkiaOutputDeviceX11> pass_key,
      scoped_refptr<gpu::SharedContextState> context_state,
      x11::Window window,
      x11::VisualId visual,
      gpu::MemoryTracker* memory_tracker,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback);

  SkiaOutputDeviceX11(const SkiaOutputDeviceX11&) = delete;
  SkiaOutputDeviceX11& operator=(const SkiaOutputDeviceX11&) = delete;

  ~SkiaOutputDeviceX11() override;

  static std::unique_ptr<SkiaOutputDeviceX11> Create(
      scoped_refptr<gpu::SharedContextState> context_state,
      gfx::AcceleratedWidget widget,
      gpu::MemoryTracker* memory_tracker,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback);

  bool Reshape(const ReshapeParams& params) override;
  void Present(const std::optional<gfx::Rect>& update_rect,
               BufferPresentedCallback feedback,
               OutputSurfaceFrame frame) override;

 private:
  const raw_ptr<x11::Connection> connection_;
  const x11::Window window_;
  const x11::VisualId visual_;
  const x11::GraphicsContext gc_;
  scoped_refptr<base::RefCountedMemory> pixels_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_X11_H_
