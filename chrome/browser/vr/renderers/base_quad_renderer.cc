// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/renderers/base_quad_renderer.h"

#include "device/vr/vr_gl_util.h"
#include "ui/gfx/geometry/transform.h"

namespace vr {

namespace {

static constexpr float kQuadVertices[8] = {
    -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f, -0.5f,
};
static constexpr GLushort kQuadIndices[6] = {0, 1, 2, 1, 3, 2};
static constexpr int kQuadPositionDataSize = 2;

}  // namespace

BaseQuadRenderer::BaseQuadRenderer(const char* vertex_src,
                                   const char* fragment_src)
    : BaseRenderer(vertex_src, fragment_src) {}

BaseQuadRenderer::~BaseQuadRenderer() = default;

void BaseQuadRenderer::PrepareToDraw(GLuint view_proj_matrix_handle,
                                     const gfx::Transform& view_proj_matrix) {
  glUseProgram(program_handle_);

  // Pass in model view project matrix.
  glUniformMatrix4fv(view_proj_matrix_handle, 1, false,
                     MatrixToGLArray(view_proj_matrix).data());

  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);

  // Set up position attribute.
  glVertexAttribPointer(position_handle_, kQuadPositionDataSize, GL_FLOAT,
                        false, 0, 0);
  glEnableVertexAttribArray(position_handle_);
}

GLuint BaseQuadRenderer::vertex_buffer_ = 0;
GLuint BaseQuadRenderer::index_buffer_ = 0;

void BaseQuadRenderer::CreateBuffers() {
  GLuint buffers[2];
  glGenBuffers(2, buffers);
  vertex_buffer_ = buffers[0];
  index_buffer_ = buffers[1];

  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER, std::size(kQuadVertices) * sizeof(float),
               kQuadVertices, GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               std::size(kQuadIndices) * sizeof(GLushort), kQuadIndices,
               GL_STATIC_DRAW);
}

int BaseQuadRenderer::NumQuadIndices() {
  return std::size(kQuadIndices);
}

}  // namespace vr
