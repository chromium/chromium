// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_WIN_GRAPHICS_DELEGATE_WIN_H_
#define CHROME_BROWSER_VR_WIN_GRAPHICS_DELEGATE_WIN_H_

#include <string>
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/vr/graphics_delegate.h"
#include "chrome/browser/vr/render_info.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "ui/gfx/geometry/rect_f.h"

namespace gpu {
class SharedImageInterface;
namespace gles2 {
class GLES2Interface;
}
}  // namespace gpu

namespace vr {

class GraphicsDelegateWin : public GraphicsDelegate {
 public:
  using Transform = float[16];
  GraphicsDelegateWin();
  ~GraphicsDelegateWin() override;

  // Called on main UI thread.
  bool InitializeOnMainThread();

  // Called on background GL thread.
  void InitializeOnGLThread();
  void SetXrViews(const std::vector<device::mojom::XRViewPtr>& views);
  bool PreRender();
  void PostRender();
  mojo::PlatformHandle GetTexture();
  const gpu::SyncToken& GetSyncToken();
  gfx::RectF GetLeft();
  gfx::RectF GetRight();
  void ResetMemoryBuffer();
  bool BindContext();
  void ClearContext();
  void UpdateViews(std::vector<device::mojom::XRViewPtr> views);

 private:
  // GraphicsDelegate:
  FovRectangles GetRecommendedFovs() override;
  float GetZNear() override;
  RenderInfo GetRenderInfo(FrameType frame_type,
                           const gfx::Transform& head_pose) override;
  RenderInfo GetOptimizedRenderInfoForFovs(const FovRectangles& fovs) override;
  void InitializeBuffers() override;
  void PrepareBufferForWebXr() override;
  void PrepareBufferForWebXrOverlayElements() override;
  void PrepareBufferForBrowserUi() override;
  void OnFinishedDrawingBuffer() override;
  void GetWebXrDrawParams(int* texture_id, Transform* uv_transform) override;
  bool Initialize(const scoped_refptr<gl::GLSurface>& surface) override;
  bool RunInSkiaContext(base::OnceClosure callback) override;

  // Helpers:
  bool EnsureMemoryBuffer(int width, int height);
  gfx::Rect GetTextureSize();

  device::mojom::XRViewPtr left_;
  device::mojom::XRViewPtr right_;

  scoped_refptr<viz::ContextProviderCommandBuffer> context_provider_;
  raw_ptr<gpu::gles2::GLES2Interface> gl_ = nullptr;
  raw_ptr<gpu::SharedImageInterface> sii_ = nullptr;
  int last_width_ = 0;
  int last_height_ = 0;
  gpu::Mailbox mailbox_;  // Corresponding to our target GpuMemoryBuffer.
  GLuint dest_texture_id_ = 0;
  GLuint draw_frame_buffer_ = 0;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;
  raw_ptr<gpu::GpuMemoryBufferManager> gpu_memory_buffer_manager_ = nullptr;
  // Sync point after access to |gpu_memory_buffer_| is done.
  gpu::SyncToken access_done_sync_token_;

  RenderInfo cached_info_ = {};

  enum class DrawingBufferMode {
    kWebXr,
    kWebXrOverlayElements,
    kContentQuad,
    kBrowserUi,
    kNone,
  };

  DrawingBufferMode prepared_drawing_buffer_ = DrawingBufferMode::kNone;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_WIN_GRAPHICS_DELEGATE_WIN_H_
