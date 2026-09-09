// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_RENDER_PASS_BACKING_SHARED_IMAGE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_RENDER_PASS_BACKING_SHARED_IMAGE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/client/client_shared_image.h"

namespace gpu {
struct Mailbox;
}

namespace viz {

class SkiaOutputSurface;

// A wrapper around a `gpu::ClientSharedImage` that notifies the owning output
// surface when it's destroyed.
class VIZ_SERVICE_EXPORT RenderPassBackingSharedImage {
 public:
  RenderPassBackingSharedImage(
      SkiaOutputSurface* skia_output_surface,
      scoped_refptr<gpu::ClientSharedImage> shared_image);
  ~RenderPassBackingSharedImage();

  // This type cannot be made copyable because we must ensure that the output
  // surface is notified of destruction exactly once.
  RenderPassBackingSharedImage(const RenderPassBackingSharedImage&) = delete;
  RenderPassBackingSharedImage& operator=(const RenderPassBackingSharedImage&) =
      delete;

  RenderPassBackingSharedImage(RenderPassBackingSharedImage&&);
  RenderPassBackingSharedImage& operator=(RenderPassBackingSharedImage&&);

  const gpu::Mailbox& mailbox() const;

 private:
  void Destroy();

  raw_ptr<SkiaOutputSurface> skia_output_surface_ = nullptr;
  scoped_refptr<gpu::ClientSharedImage> shared_image_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_RENDER_PASS_BACKING_SHARED_IMAGE_H_
