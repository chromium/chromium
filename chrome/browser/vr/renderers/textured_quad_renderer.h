// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_RENDERERS_TEXTURED_QUAD_RENDERER_H_
#define CHROME_BROWSER_VR_RENDERERS_TEXTURED_QUAD_RENDERER_H_

#include "base/containers/queue.h"
#include "chrome/browser/vr/renderers/base_renderer.h"

#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"

namespace vr {

class TexturedQuadRenderer : public BaseRenderer {
 public:
  TexturedQuadRenderer(const char* vertex_src, const char* fragment_src);
  TexturedQuadRenderer();

  TexturedQuadRenderer(const TexturedQuadRenderer&) = delete;
  TexturedQuadRenderer& operator=(const TexturedQuadRenderer&) = delete;

  ~TexturedQuadRenderer() override;

  // Enqueues a textured quad for rendering. The GL will ultimately be issued
  // in |Flush|.
  void AddQuad(int texture_data_handle,
               int overlay_texture_data_handle,
               const gfx::Transform& model_view_proj_matrix,
               const gfx::RectF& clip_rect,
               float opacity,
               const gfx::SizeF& element_size,
               float corner_radius,
               bool blend);

  void Flush() override;

  static void CreateBuffers();
  static GLuint VertexBuffer();
  static GLuint IndexBuffer();
  static int NumQuadIndices();

 protected:
  virtual GLenum TextureType() const;
  static const char* VertexShader();

 private:
  struct QuadData {
    int texture_data_handle;
    int overlay_texture_data_handle;
    gfx::Transform model_view_proj_matrix;
    gfx::RectF clip_rect;
    float opacity;
    gfx::SizeF element_size;
    float corner_radius;
    bool blend;
  };

  static GLuint vertex_buffer_;
  static GLuint index_buffer_;

  // Uniforms
  GLuint model_view_proj_matrix_handle_;
  GLuint corner_offset_handle_;
  GLuint opacity_handle_;
  GLuint overlay_opacity_handle_;
  GLuint texture_handle_;
  GLuint overlay_texture_handle_;

  // Attributes
  GLuint corner_position_handle_;
  GLuint offset_scale_handle_;
  GLuint uses_overlay_handle_;

  base::queue<QuadData> quad_queue_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_RENDERERS_TEXTURED_QUAD_RENDERER_H_
