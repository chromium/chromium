// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_GL_TEST_ENVIRONMENT_H_
#define CHROME_BROWSER_VR_TEST_GL_TEST_ENVIRONMENT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/vr/gl_bindings.h"
#include "ui/gfx/geometry/size.h"

namespace gl {
class GLSurface;
class GLContext;
}  // namespace gl

namespace gpu {
class GLInProcessContext;
}  // namespace gpu

namespace vr {

class GlTestEnvironment {
 public:
  explicit GlTestEnvironment(const gfx::Size frame_buffer_size);
  ~GlTestEnvironment();

  GLuint GetFrameBufferForTesting();
  GLuint CreateTexture(GLenum target);

 private:
  GLuint vao_ = 0;
  GLuint frame_buffer_ = 0;

#if defined(VR_USE_COMMAND_BUFFER)
  std::unique_ptr<gpu::GLInProcessContext> context_;
#else
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
#endif  // defined(USE_COMMAND_BUFFER)
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_GL_TEST_ENVIRONMENT_H_
