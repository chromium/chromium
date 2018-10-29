// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_WIN_SIMPLE_OVERLAY_RENDERER_WIN_H_
#define CHROME_BROWSER_VR_WIN_SIMPLE_OVERLAY_RENDERER_WIN_H_

#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "mojo/public/cpp/system/handle.h"
#include "services/ws/public/cpp/gpu/context_provider_command_buffer.h"
#include "ui/gfx/geometry/rect_f.h"

namespace vr {

// This class renders an simple solid-color overlay that can be submitted to
// be composited on top of WebXR content.  Note that it is not used outside
// manual testing (requires build changes to enable), and will be replaced with
// VR-UI overlays.
class SimpleOverlayRenderer {
 public:
  SimpleOverlayRenderer();
  ~SimpleOverlayRenderer();

  // Called on main UI thread.
  bool InitializeOnMainThread();

  // Called on background GL thread.
  void InitializeOnGLThread();
  void Cleanup();
  void Render();
  mojo::ScopedHandle GetTexture();
  gfx::RectF GetLeft();
  gfx::RectF GetRight();
  void ResetMemoryBuffer();

 private:
  bool EnsureMemoryBuffer(int width, int height);

  scoped_refptr<ws::ContextProviderCommandBuffer> context_provider_;
  gpu::gles2::GLES2Interface* gl_ = nullptr;
  int last_width_ = 0;
  int last_height_ = 0;
  GLuint image_id_ = 0;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_ = nullptr;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_WIN_SIMPLE_OVERLAY_RENDERER_WIN_H_
