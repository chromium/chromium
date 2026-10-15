// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_GRAPHICS_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_VR_GRAPHICS_DELEGATE_ANDROID_H_

#include <array>
#include <cstddef>
#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/vr/graphics_delegate.h"
#include "device/vr/android/mailbox_to_surface_bridge.h"
#include "device/vr/android/web_xr_presentation_state.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface.h"

namespace vr {

class GraphicsDelegateAndroid : public GraphicsDelegate {
 public:
  GraphicsDelegateAndroid();
  ~GraphicsDelegateAndroid() override;

  // GraphicsDelegate:
  void Initialize(base::OnceClosure on_initialized) override;
  bool PreRender() override;
  void PostRender() override;
  scoped_refptr<gpu::ClientSharedImage> GetTexture() override;
  gpu::SyncToken GetSyncToken() override;
  void ResetMemoryBuffer() override;
  bool BindContext() override;
  void ClearContext() override;

 private:
  struct BufferSlot {
    BufferSlot();
    ~BufferSlot();

    std::unique_ptr<device::WebXrSharedBuffer> shared_buffer;
    // Cached unowned copy without a MappableBuffer so subsequent frames do not
    // clone the AHardwareBuffer handle on every Export().
    scoped_refptr<gpu::ClientSharedImage> unowned_shared_image;
    GLuint draw_frame_buffer = 0;
    // True when `shared_buffer->scoped_ahb_handle` was newly allocated and
    // needs to be exported once via `GetTexture()`.
    bool needs_handle_export = false;
  };

  static constexpr size_t kNumBuffers = 2;

  // Helpers:
  void OnMailboxBridgeReady(base::OnceClosure on_initialized);
  bool EnsureAndBindMemoryBuffer();
  void ClearBufferToBlack() override;

  std::unique_ptr<device::MailboxToSurfaceBridge> mailbox_bridge_;
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;

  std::array<BufferSlot, kNumBuffers> buffers_ = {};
  size_t current_buffer_index_ = 0;
  gfx::Size last_size_;

  base::WeakPtrFactory<GraphicsDelegateAndroid> weak_ptr_factory_{this};
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_GRAPHICS_DELEGATE_ANDROID_H_
