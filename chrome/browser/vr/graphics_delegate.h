// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_GRAPHICS_DELEGATE_H_
#define CHROME_BROWSER_VR_GRAPHICS_DELEGATE_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/vr/fov_rectangle.h"
#include "chrome/browser/vr/frame_type.h"
#include "chrome/browser/vr/render_info.h"
#include "chrome/browser/vr/vr_export.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "mojo/public/cpp/platform/platform_handle.h"

namespace gfx {
class Transform;
class RectF;
class Size;
}  // namespace gfx

namespace gpu {
struct SyncToken;
}  // namespace gpu

namespace vr {

// The GraphicsDelegate manages surfaces, buffers and viewports, preparing
// them for drawing browser UI. It provides projection and view matrices for the
// viewports.
class VR_EXPORT GraphicsDelegate {
 public:
  using Transform = float[16];
  GraphicsDelegate();
  virtual ~GraphicsDelegate();

  float GetZNear();
  void SetXrViews(const std::vector<device::mojom::XRViewPtr>& views);
  gfx::RectF GetRight();
  gfx::RectF GetLeft();

  // TODO(https://crbug.com/1493735): Make non-virtual once GVR is removed.
  virtual FovRectangles GetRecommendedFovs();
  virtual RenderInfo GetRenderInfo(FrameType frame_type,
                                   const gfx::Transform& head_pose);
  virtual RenderInfo GetOptimizedRenderInfoForFovs(const FovRectangles& fovs);

  // TODO(https://crbug.com/1493735): Consider removing these methods once GVR
  // is removed.
  virtual void InitializeBuffers() {}
  virtual void PrepareBufferForWebXr();
  virtual void PrepareBufferForWebXrOverlayElements();
  virtual void PrepareBufferForBrowserUi();
  virtual void OnFinishedDrawingBuffer();
  virtual void GetWebXrDrawParams(int* texture_id, Transform* uv_transform);
  // This method returns true when succeeded.
  virtual bool RunInSkiaContext(base::OnceClosure callback);

  virtual bool PreRender() = 0;
  virtual void PostRender() = 0;
  virtual mojo::PlatformHandle GetTexture() = 0;
  virtual const gpu::SyncToken& GetSyncToken() = 0;
  virtual void ResetMemoryBuffer() = 0;
  virtual bool BindContext() = 0;
  virtual void ClearContext() = 0;

 protected:
  gfx::Size GetTextureSize();

  // TODO(https://crbug.com/1493735): Make pure virtual once GVR is removed.
  virtual void ClearBufferToBlack() {}

  // TODO(https://crbug.com/1493735): Remove once GVR is removed.
  float GetZFar();

 private:
  device::mojom::XRViewPtr left_;
  device::mojom::XRViewPtr right_;

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

#endif  // CHROME_BROWSER_VR_GRAPHICS_DELEGATE_H_
