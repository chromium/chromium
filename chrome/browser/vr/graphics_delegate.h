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
#include "ui/gfx/gpu_memory_buffer.h"

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
  static std::unique_ptr<GraphicsDelegate> Create();

  using Transform = float[16];
  GraphicsDelegate();
  virtual ~GraphicsDelegate();

  float GetZNear();
  void SetXrViews(const std::vector<device::mojom::XRViewPtr>& views);
  gfx::RectF GetRight();
  gfx::RectF GetLeft();

  FovRectangles GetRecommendedFovs();
  RenderInfo GetRenderInfo(FrameType frame_type,
                           const gfx::Transform& head_pose);
  RenderInfo GetOptimizedRenderInfoForFovs(const FovRectangles& fovs);

  virtual void Initialize(base::OnceClosure on_initialized) = 0;
  virtual bool PreRender() = 0;
  virtual void PostRender() = 0;
  virtual gfx::GpuMemoryBufferHandle GetTexture() = 0;
  virtual gpu::SyncToken GetSyncToken() = 0;
  virtual void ResetMemoryBuffer() = 0;
  virtual bool BindContext() = 0;
  virtual void ClearContext() = 0;

 protected:
  gfx::Size GetTextureSize();

  virtual void ClearBufferToBlack() = 0;

 private:
  device::mojom::XRViewPtr left_;
  device::mojom::XRViewPtr right_;

  RenderInfo cached_info_ = {};
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_GRAPHICS_DELEGATE_H_
