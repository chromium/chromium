// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_GRAPHICS_DELEGATE_WIN_H_
#define CHROME_BROWSER_VR_GRAPHICS_DELEGATE_WIN_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/vr/graphics_delegate.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {
class SharedImageInterface;
namespace gles2 {
class GLES2Interface;
}
}  // namespace gpu

namespace vr {

class GraphicsDelegateWin : public GraphicsDelegate {
 public:
  GraphicsDelegateWin();
  ~GraphicsDelegateWin() override;

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
  void ClearBufferToBlack() override;

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host_;
  scoped_refptr<viz::ContextProviderCommandBuffer> context_provider_;
  raw_ptr<gpu::gles2::GLES2Interface> gl_ = nullptr;
  raw_ptr<gpu::SharedImageInterface> sii_ = nullptr;
  gfx::Size last_size_;
  scoped_refptr<gpu::ClientSharedImage> client_shared_image_;
  std::unique_ptr<gpu::SharedImageTexture> shared_image_texture_;
  std::unique_ptr<gpu::SharedImageTexture::ScopedAccess>
      scoped_shared_image_access_;
  GLuint draw_frame_buffer_ = 0;
  // Sync point after access to the buffer is done.
  gpu::SyncToken access_done_sync_token_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_GRAPHICS_DELEGATE_WIN_H_
