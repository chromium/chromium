// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_GRAPHICS_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_VR_GRAPHICS_DELEGATE_ANDROID_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/vr/graphics_delegate.h"
#include "device/vr/android/mailbox_to_surface_bridge.h"
#include "device/vr/android/web_xr_presentation_state.h"
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
  gfx::GpuMemoryBufferHandle GetTexture() override;
  gpu::SyncToken GetSyncToken() override;
  void ResetMemoryBuffer() override;
  bool BindContext() override;
  void ClearContext() override;

 private:
  // Helpers:
  bool EnsureMemoryBuffer();
  void OnMailboxBridgeReady(base::OnceClosure on_initialized);
  void ClearBufferToBlack() override;

  std::unique_ptr<device::WebXrSharedBuffer> shared_buffer_;
  GLuint draw_frame_buffer_ = 0;

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;

  std::unique_ptr<device::MailboxToSurfaceBridge> mailbox_bridge_;

  base::WeakPtrFactory<GraphicsDelegateAndroid> weak_ptr_factory_{this};
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_GRAPHICS_DELEGATE_ANDROID_H_
