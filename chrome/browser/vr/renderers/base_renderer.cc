// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/renderers/base_renderer.h"

#include <ostream>

#include "base/check.h"
#include "device/vr/vr_gl_util.h"

namespace vr {

BaseRenderer::BaseRenderer(const char* vertex_src, const char* fragment_src) {
  std::string error;
  GLuint vertex_shader_handle =
      CompileShader(GL_VERTEX_SHADER, vertex_src, error);
  CHECK(vertex_shader_handle) << error << "\nvertex_src\n" << vertex_src;

  GLuint fragment_shader_handle =
      CompileShader(GL_FRAGMENT_SHADER, fragment_src, error);
  CHECK(fragment_shader_handle) << error << "\nfragment_src\n" << fragment_src;

  program_handle_ =
      CreateAndLinkProgram(vertex_shader_handle, fragment_shader_handle, error);
  CHECK(program_handle_) << error;

  // Once the program is linked the shader objects are no longer needed
  glDeleteShader(vertex_shader_handle);
  glDeleteShader(fragment_shader_handle);

  position_handle_ = glGetAttribLocation(program_handle_, "a_Position");
  clip_rect_handle_ = glGetUniformLocation(program_handle_, "u_ClipRect");
}

BaseRenderer::~BaseRenderer() = default;

void BaseRenderer::Flush() {}

}  // namespace vr
