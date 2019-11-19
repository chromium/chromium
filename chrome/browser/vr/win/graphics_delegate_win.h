// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_WIN_GRAPHICS_DELEGATE_WIN_H_
#define CHROME_BROWSER_VR_WIN_GRAPHICS_DELEGATE_WIN_H_

#include <string>
#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/vr/graphics_delegate.h"
#include "chrome/browser/vr/render_info.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "mojo/public/cpp/system/handle.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "ui/gfx/geometry/rect_f.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}  // namespace gpu

namespace vr {

class GraphicsDelegateWin : public GraphicsDelegate {
 public:
  using Transform = float[16];
  using TexturesInitializedCallback = base::OnceCallback<
      void(GlTextureLocation, unsigned int, unsigned int, unsigned int)>;
  GraphicsDelegateWin();
  ~GraphicsDelegateWin() override;

  // Called on main UI thread.
  bool InitializeOnMainThread();

  // Called on background GL thread.
  void InitializeOnGLThread();
  void SetVRDisplayInfo(device::mojom::VRDisplayInfoPtr info);
  void Cleanup();
  void PreRender();
  void PostRender();
  mojo::ScopedHandle GetTexture();
  gfx::RectF GetLeft();
  gfx::RectF GetRight();
  void ResetMemoryBuffer();
  bool BindContext();
  void ClearContext();

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
  void PrepareBufferForContentQuadLayer(
      const gfx::Transform& quad_transform) override;
  void PrepareBufferForBrowserUi() override;
  void OnFinishedDrawingBuffer() override;
  void GetWebXrDrawParams(int* texture_id, Transform* uv_transform) override;
  bool IsContentQuadReady() override;
  void ResumeContentRendering() override;
  void BufferBoundsChanged(const gfx::Size& content_buffer_size,
                           const gfx::Size& overlay_buffer_size) override;
  void GetContentQuadDrawParams(Transform* uv_transform,
                                float* border_x,
                                float* border_y) override;
  int GetContentBufferWidth() override;
  bool Initialize(const scoped_refptr<gl::GLSurface>& surface) override;
  bool RunInSkiaContext(base::OnceClosure callback) override;
  void SetFrameDumpFilepathBase(std::string& filepath_base) override;

  // Helpers:
  bool EnsureMemoryBuffer(int width, int height);
  gfx::Rect GetTextureSize();

  device::mojom::VRDisplayInfoPtr info_;

  scoped_refptr<viz::ContextProviderCommandBuffer> context_provider_;
  gpu::gles2::GLES2Interface* gl_ = nullptr;
  int last_width_ = 0;
  int last_height_ = 0;
  GLuint image_id_ = 0;  // Image corresponding to our target GpuMemoryBuffer.
  GLuint dest_texture_id_ = 0;
  GLuint draw_frame_buffer_ = 0;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_ = nullptr;

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
