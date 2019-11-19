// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/native_gl_surface_provider.h"

#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/init/create_gr_gl_interface.h"

namespace vr {

NativeGlSurfaceProvider::NativeGlSurfaceProvider() {
  const char* version_str =
      reinterpret_cast<const char*>(glGetString(GL_VERSION));
  const char* renderer_str =
      reinterpret_cast<const char*>(glGetString(GL_RENDERER));
  std::string extensions_string(gl::GetGLExtensionsFromCurrentContext());
  gfx::ExtensionSet extensions(gfx::MakeExtensionSet(extensions_string));
  gl::GLVersionInfo gl_version_info(version_str, renderer_str, extensions);
  const bool use_version_es2 = false;
  sk_sp<const GrGLInterface> gr_interface =
      gl::init::CreateGrGLInterface(gl_version_info, use_version_es2);
  DCHECK(gr_interface.get());
  gr_context_ = GrContext::MakeGL(std::move(gr_interface));
  DCHECK(gr_context_.get());
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &main_fbo_);
}

NativeGlSurfaceProvider::~NativeGlSurfaceProvider() = default;

sk_sp<SkSurface> NativeGlSurfaceProvider::MakeSurface(const gfx::Size& size) {
  return SkSurface::MakeRenderTarget(
      gr_context_.get(), SkBudgeted::kNo,
      SkImageInfo::MakeN32Premul(size.width(), size.height()), 0,
      kTopLeft_GrSurfaceOrigin, nullptr);
}

GLuint NativeGlSurfaceProvider::FlushSurface(SkSurface* surface,
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
  glBindFramebufferEXT(GL_FRAMEBUFFER, main_fbo_);

  return texture_id;
}

}  // namespace vr
