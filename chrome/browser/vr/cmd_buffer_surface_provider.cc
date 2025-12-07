// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/cmd_buffer_surface_provider.h"

#include "base/check_op.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/skia_bindings/gl_bindings_skia_cmd_buffer.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLInterface.h"
#include "ui/gfx/geometry/size.h"

namespace vr {

CmdBufferSurfaceProvider::CmdBufferSurfaceProvider() {
  auto* gles2_implementation =
      static_cast<gpu::gles2::GLES2Implementation*>(gles2::GetGLContext());
  DCHECK(gles2_implementation);
  sk_sp<const GrGLInterface> gr_interface =
      skia_bindings::CreateGLES2InterfaceBindings(gles2_implementation,
                                                  gles2_implementation);
  gr_context_ = GrDirectContexts::MakeGL(std::move(gr_interface));
  DCHECK(gr_context_);
}

CmdBufferSurfaceProvider::~CmdBufferSurfaceProvider() = default;

std::unique_ptr<SkiaSurfaceProvider::Texture>
CmdBufferSurfaceProvider::CreateTextureWithSkia(
    const gfx::Size& size,
    base::FunctionRef<void(SkCanvas*)> paint) {
  // We need to store and restore previous FBO after skia draw.
  GLint prev_fbo = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);

  auto texture = CreateTextureWithSkiaImpl(gr_context_.get(), size, paint);

  gr_context_->resetContext();
  glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);

  return texture;
}

}  // namespace vr
