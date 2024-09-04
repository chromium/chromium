// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/skia_surface_provider.h"

#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GpuTypes.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/GrRecordingContext.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLTypes.h"
#include "ui/gfx/geometry/size.h"

namespace vr {
namespace {
GLint GetTextureIDFromSurface(SkSurface* surface) {
  GrBackendTexture backend_texture = SkSurfaces::GetBackendTexture(
      surface, SkSurfaces::BackendHandleAccess::kFlushRead);
  DCHECK(backend_texture.isValid());
  GrGLTextureInfo info;
  bool result = GrBackendTextures::GetGLTextureInfo(backend_texture, &info);
  DCHECK(result);
  DCHECK_NE(info.fID, 0u);
  return info.fID;
}
}  // namespace

SkiaSurfaceProvider::Texture::Texture(sk_sp<SkSurface> surface)
    : texture_id_(GetTextureIDFromSurface(surface.get())),
      surface_(std::move(surface)) {}

SkiaSurfaceProvider::Texture::~Texture() = default;

std::unique_ptr<SkiaSurfaceProvider::Texture>
SkiaSurfaceProvider::CreateTextureWithSkiaImpl(
    GrDirectContext* gr_context,
    const gfx::Size& size,
    base::FunctionRef<void(SkCanvas*)> paint) {
  auto surface = SkSurfaces::RenderTarget(
      gr_context, skgpu::Budgeted::kNo,
      SkImageInfo::MakeN32Premul(size.width(), size.height()), 0,
      kTopLeft_GrSurfaceOrigin, nullptr);

  paint(surface->getCanvas());
  gr_context->flush(surface.get());

  return std::make_unique<Texture>(std::move(surface));
}

}  // namespace vr
