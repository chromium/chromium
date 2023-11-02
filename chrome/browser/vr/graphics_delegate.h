// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_GRAPHICS_DELEGATE_H_
#define CHROME_BROWSER_VR_GRAPHICS_DELEGATE_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/vr/fov_rectangle.h"
#include "chrome/browser/vr/frame_type.h"
#include "chrome/browser/vr/gl_texture_location.h"
#include "chrome/browser/vr/vr_export.h"

namespace gfx {
class Size;
class Transform;
}  // namespace gfx

namespace gl {
class GLSurface;
}  // namespace gl

namespace vr {

struct RenderInfo;

// The GraphicsDelegate manages surfaces, buffers and viewports, preparing
// them for drawing browser UI. It provides projection and view matrices for the
// viewports.
class VR_EXPORT GraphicsDelegate {
 public:
  using Transform = float[16];
  using TexturesInitializedCallback = base::OnceCallback<
      void(GlTextureLocation, unsigned int, unsigned int, unsigned int)>;
  virtual ~GraphicsDelegate() {}

  virtual FovRectangles GetRecommendedFovs() = 0;
  virtual float GetZNear() = 0;
  virtual RenderInfo GetRenderInfo(FrameType frame_type,
                                   const gfx::Transform& head_pose) = 0;
  virtual RenderInfo GetOptimizedRenderInfoForFovs(
      const FovRectangles& fovs) = 0;
  virtual void InitializeBuffers() = 0;
  virtual void PrepareBufferForWebXr() = 0;
  virtual void PrepareBufferForWebXrOverlayElements() = 0;
  virtual void PrepareBufferForContentQuadLayer(
      const gfx::Transform& quad_transform) = 0;
  virtual void PrepareBufferForBrowserUi() = 0;
  virtual void OnFinishedDrawingBuffer() = 0;
  virtual void GetWebXrDrawParams(int* texture_id, Transform* uv_transform) = 0;
  virtual bool IsContentQuadReady() = 0;
  virtual void ResumeContentRendering() = 0;
  virtual void BufferBoundsChanged(const gfx::Size& content_buffer_size,
                                   const gfx::Size& overlay_buffer_size) = 0;
  virtual void GetContentQuadDrawParams(Transform* uv_transform,
                                        float* border_x,
                                        float* border_y) = 0;
  virtual int GetContentBufferWidth() = 0;

  // These methods return true when succeeded.
  virtual bool Initialize(const scoped_refptr<gl::GLSurface>& surface) = 0;
  virtual bool RunInSkiaContext(base::OnceClosure callback) = 0;

  virtual void SetFrameDumpFilepathBase(std::string& filepath_base) = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_GRAPHICS_DELEGATE_H_
