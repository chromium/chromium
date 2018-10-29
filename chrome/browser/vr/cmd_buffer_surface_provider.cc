// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/cmd_buffer_surface_provider.h"

#include "base/logging.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/skia_bindings/gl_bindings_skia_cmd_buffer.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"
#include "ui/gfx/geometry/size.h"

namespace vr {

CmdBufferSurfaceProvider::CmdBufferSurfaceProvider() {
  auto* gles2_implementation =
      static_cast<gpu::gles2::GLES2Implementation*>(gles2::GetGLContext());
  DCHECK(gles2_implementation);
  sk_sp<const GrGLInterface> gr_interface =
      skia_bindings::CreateGLES2InterfaceBindings(gles2_implementation,
                                                  gles2_implementation);
  gr_context_ = GrContext::MakeGL(std::move(gr_interface));
  DCHECK(gr_context_);
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &main_fbo_);
}

CmdBufferSurfaceProvider::~CmdBufferSurfaceProvider() = default;

sk_sp<SkSurface> CmdBufferSurfaceProvider::MakeSurface(const gfx::Size& size) {
  return SkSurface::MakeRenderTarget(
      gr_context_.get(), SkBudgeted::kNo,
      SkImageInfo::MakeN32Premul(size.width(), size.height()), 0,
      kTopLeft_GrSurfaceOrigin, nullptr);
}

GLuint CmdBufferSurfaceProvider::FlushSurface(SkSurface* surface,
                                              GLuint reuse_texture_id) {
  surface->getCanvas()->flush();
  GrBackendTexture backend_texture =
      surface->getBackendTexture(SkSurface::kFlushRead_BackendHandleAccess);
  DCHECK(backend_texture.isValid());
  GrGLTextureInfo info;
  bool result = backend_texture.getGLTextureInfo(&info);
  DCHECK(result);
  GLuint texture_id = info.fID;
  DCHECK_NE(texture_id, 0u);
  surface->getCanvas()->getGrContext()->resetContext();
  glBindFramebuffer(GL_FRAMEBUFFER, main_fbo_);

  return texture_id;
}

}  // namespace vr
