// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_GRAPHICS_DELEGATE_H_
#define CHROME_BROWSER_VR_GRAPHICS_DELEGATE_H_

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/vr/fov_rectangle.h"
#include "chrome/browser/vr/frame_type.h"
#include "chrome/browser/vr/vr_export.h"

namespace gfx {
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
  virtual void PrepareBufferForBrowserUi() = 0;
  virtual void OnFinishedDrawingBuffer() = 0;
  virtual void GetWebXrDrawParams(int* texture_id, Transform* uv_transform) = 0;

  // These methods return true when succeeded.
  virtual bool Initialize(const scoped_refptr<gl::GLSurface>& surface) = 0;
  virtual bool RunInSkiaContext(base::OnceClosure callback) = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_GRAPHICS_DELEGATE_H_
