// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/gl_test_environment.h"

#include "base/run_loop.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/ipc/gl_in_process_context.h"
#include "gpu/ipc/test_gpu_thread_holder.h"

namespace {

GLuint CreateTexture(GLenum target) {
  // Create the texture object.
  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(target, texture);
  glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  return texture;
}

GLuint SetupFramebuffer(int width, int height) {
  GLuint color_buffer_texture = CreateTexture(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, color_buffer_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);
  GLuint framebuffer = 0;
  glGenFramebuffers(1, &framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         color_buffer_texture, 0);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    DCHECK_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
              glCheckFramebufferStatus(GL_FRAMEBUFFER))
        << "Error setting up framebuffer";
    glDeleteFramebuffers(1, &framebuffer);
    framebuffer = 0;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDeleteTextures(1, &color_buffer_texture);
  return framebuffer;
}

}  // namespace

namespace vr {

GlTestEnvironment::GlTestEnvironment(const gfx::Size frame_buffer_size) {
  gpu::ContextCreationAttribs attributes;
  attributes.bind_generates_resource = false;

  context_ = std::make_unique<gpu::GLInProcessContext>();
  auto result =
      context_->Initialize(gpu::GetTestGpuThreadHolder()->GetTaskExecutor(),
                           attributes, gpu::SharedMemoryLimits());
  DCHECK_EQ(result, gpu::ContextResult::kSuccess);
  gles2::SetGLContext(context_->GetImplementation());

  // To avoid glGetVertexAttribiv(0, ...) failing.
  glGenVertexArraysOES(1, &vao_);
  glBindVertexArrayOES(vao_);

  frame_buffer_ =
      SetupFramebuffer(frame_buffer_size.width(), frame_buffer_size.height());
  glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer_);
}

GlTestEnvironment::~GlTestEnvironment() {
  glDeleteFramebuffers(1, &frame_buffer_);
  if (vao_) {
    glDeleteVertexArraysOES(1, &vao_);
  }
}

GLuint GlTestEnvironment::GetFrameBufferForTesting() {
  return frame_buffer_;
}

GLuint GlTestEnvironment::CreateTexture(GLenum target) {
  return ::CreateTexture(target);
}

}  // namespace vr
