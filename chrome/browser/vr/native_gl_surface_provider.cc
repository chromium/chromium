// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/native_gl_surface_provider.h"

#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLDirectContext.h"
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
  sk_sp<const GrGLInterface> gr_interface =
      gl::init::CreateGrGLInterface(gl_version_info);
  DCHECK(gr_interface.get());
  gr_context_ = GrDirectContexts::MakeGL(std::move(gr_interface));
  DCHECK(gr_context_.get());
}

NativeGlSurfaceProvider::~NativeGlSurfaceProvider() = default;

std::unique_ptr<SkiaSurfaceProvider::Texture>
NativeGlSurfaceProvider::CreateTextureWithSkia(
    const gfx::Size& size,
    base::FunctionRef<void(SkCanvas*)> paint) {
  // We need to store and restore previous FBO after skia draw.
  GLint prev_fbo = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);

  auto texture = CreateTextureWithSkiaImpl(gr_context_.get(), size, paint);

  gr_context_->resetContext();
  glBindFramebufferEXT(GL_FRAMEBUFFER, prev_fbo);

  return texture;
}

}  // namespace vr
