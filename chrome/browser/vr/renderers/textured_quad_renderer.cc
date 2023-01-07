// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/renderers/textured_quad_renderer.h"

#include "device/vr/vr_gl_util.h"
#include "ui/gfx/geometry/transform.h"

namespace vr {

namespace {

// clang-format off
//
// A rounded rect is subdivided into a number of triangles.
// _______________
// | /    _,-' \ |
// |/_,,-'______\|
// |            /|
// |           / |
// |          /  |
// |         /   |
// |        /    |
// |       /     |
// |      /      |
// |     /       |
// |    /        |
// |   /         |
// |  /          |
// | /           |
// |/____________|
// |\     _,-'' /|
// |_\ ,-'____ /_|
//
// Most of these do not contain an arc. To simplify the rendering of those
// that do, we include a "corner position" attribute. The corner position is
// the distance from the center of the nearest "corner circle". Only those
// triangles containing arcs have a non-zero corner position set. The result
// is that for interior triangles, their corner position is uniformly (0, 0).
// I.e., they are always deemed "inside".
//
// A further complication is that different corner radii will require these
// various triangles to be sized differently relative to one another. We
// would prefer not no continually recreate our vertex buffer, so we include
// another attribute, the "offset scalars". These scalars are only ever 1.0,
// 0.0, or -1.0 and control the addition or subtraction of the horizontal
// and vertical corner offset. This lets the corners of the triangles be
// computed in the vertex shader dynamically. It also happens that the
// texture coordinates can also be easily computed in the vertex shader.
//
// So if the the corner offsets are vr and hr where
//     vr = corner_radius / height;
//     hr = corner_radius / width;
//
// Then the full position is then given by
//   p = (x + osx * hr, y + osy * vr, 0.0, 1.0)
//
// And the full texture coordinate is given by
//   (0.5 + p[0], 0.5 - p[1])
//
static constexpr float kVertices[120] = {
    //  x      y   osx   osy  cpx  cpy
    -0.5f,  0.5f,  0.0, -1.0, 0.0, 0.0,
    -0.5f,  0.5f,  1.0,  0.0, 0.0, 0.0,
     0.5f,  0.5f, -1.0,  0.0, 0.0, 0.0,
     0.5f,  0.5f,  0.0, -1.0, 0.0, 0.0,
    -0.5f, -0.5f,  0.0,  1.0, 0.0, 0.0,
     0.5f, -0.5f,  0.0,  1.0, 0.0, 0.0,
    -0.5f, -0.5f,  1.0,  0.0, 0.0, 0.0,
     0.5f, -0.5f, -1.0,  0.0, 0.0, 0.0,

     // These are the corner triangles (note the non-zero cpx and cpy).
    -0.5f,  0.5f,  0.0, -1.0, 1.0, 0.0,
    -0.5f,  0.5f,  0.0,  0.0, 1.0, 1.0,
    -0.5f,  0.5f,  1.0,  0.0, 0.0, 1.0,
     0.5f,  0.5f, -1.0,  0.0, 0.0, 1.0,
     0.5f,  0.5f,  0.0,  0.0, 1.0, 1.0,
     0.5f,  0.5f,  0.0, -1.0, 1.0, 0.0,
    -0.5f, -0.5f,  0.0,  0.0, 1.0, 1.0,
    -0.5f, -0.5f,  0.0,  1.0, 1.0, 0.0,
    -0.5f, -0.5f,  1.0,  0.0, 0.0, 1.0,
     0.5f, -0.5f, -1.0,  0.0, 0.0, 1.0,
     0.5f, -0.5f,  0.0,  1.0, 1.0, 0.0,
     0.5f, -0.5f,  0.0,  0.0, 1.0, 1.0,
};

static constexpr GLushort kIndices[30] = {
    // This is the top trapezoid.
    0,  2,  1,
    0,  3,  2,

    // These are the central triangles (the only triangles that matter if our
    // corner radius is zero).
    4,  3,  0,
    4,  5,  3,

    // This is the bottom trapezoid.
    4,  6,  5,
    6,  7,  5,

    // These are the corners.
    8,  10, 9,
    11, 13, 12,
    14, 16, 15,
    17, 19, 18,
};

static constexpr int kPositionDataSize = 2;
static constexpr size_t kPositionDataOffset = 0;
static constexpr int kOffsetScaleDataSize = 2;
static constexpr size_t kOffsetScaleDataOffset = 2 * sizeof(float);
static constexpr int kCornerPositionDataSize = 2;
static constexpr size_t kCornerPositionDataOffset = 4 * sizeof(float);
static constexpr size_t kDataStride = 6 * sizeof(float);
static constexpr size_t kInnerRectOffset = 6 * sizeof(GLushort);

static constexpr char const* kVertexShader = SHADER(
  precision mediump float;
  uniform mat4 u_ModelViewProjMatrix;
  uniform vec2 u_CornerOffset;
  attribute vec4 a_Position;
  attribute vec2 a_CornerPosition;
  attribute vec2 a_OffsetScale;
  varying vec2 v_TexCoordinate;
  varying vec2 v_CornerPosition;
  uniform bool u_UsesOverlay;

  void main() {
    v_CornerPosition = a_CornerPosition;
    vec4 position = vec4(
        a_Position[0] + u_CornerOffset[0] * a_OffsetScale[0],
        a_Position[1] + u_CornerOffset[1] * a_OffsetScale[1],
        a_Position[2],
        a_Position[3]);
    v_TexCoordinate = vec2(0.5 + position[0], 0.5 - position[1]);
    gl_Position = u_ModelViewProjMatrix * position;
  }
);

static constexpr char const* kFragmentShader = SHADER(
  precision highp float;
  uniform sampler2D u_Texture;
  uniform sampler2D u_OverlayTexture;
  uniform vec2 u_ClipRect[2];
  varying vec2 v_TexCoordinate;
  varying vec2 v_CornerPosition;
  uniform mediump float u_Opacity;
  uniform mediump float u_OverlayOpacity;

  void main() {
    vec2 s = step(u_ClipRect[0], v_TexCoordinate)
        - step(u_ClipRect[1], v_TexCoordinate);
    float insideClip = s.x * s.y;

    lowp vec4 color = texture2D(u_Texture, v_TexCoordinate);
    float mask = 1.0 - step(1.0, length(v_CornerPosition));
    gl_FragColor = insideClip * color * u_Opacity * mask;
  }
);
// clang-format on

}  // namespace

TexturedQuadRenderer::TexturedQuadRenderer()
    : TexturedQuadRenderer(kVertexShader, kFragmentShader) {}

TexturedQuadRenderer::TexturedQuadRenderer(const char* vertex_src,
                                           const char* fragment_src)
    : BaseRenderer(vertex_src, fragment_src) {
  model_view_proj_matrix_handle_ =
      glGetUniformLocation(program_handle_, "u_ModelViewProjMatrix");
  corner_offset_handle_ =
      glGetUniformLocation(program_handle_, "u_CornerOffset");
  corner_position_handle_ =
      glGetAttribLocation(program_handle_, "a_CornerPosition");
  offset_scale_handle_ = glGetAttribLocation(program_handle_, "a_OffsetScale");

  opacity_handle_ = glGetUniformLocation(program_handle_, "u_Opacity");
  overlay_opacity_handle_ =
      glGetUniformLocation(program_handle_, "u_OverlayOpacity");
  texture_handle_ = glGetUniformLocation(program_handle_, "u_Texture");
  overlay_texture_handle_ =
      glGetUniformLocation(program_handle_, "u_OverlayTexture");
  uses_overlay_handle_ = glGetUniformLocation(program_handle_, "u_UsesOverlay");
}

TexturedQuadRenderer::~TexturedQuadRenderer() = default;

void TexturedQuadRenderer::AddQuad(int texture_data_handle,
                                   int overlay_texture_data_handle,
                                   const gfx::Transform& model_view_proj_matrix,
                                   const gfx::RectF& clip_rect,
                                   float opacity,
                                   const gfx::SizeF& element_size,
                                   float corner_radius,
                                   bool blend) {
  if (!clip_rect.Intersects(gfx::RectF(1.0f, 1.0f)))
    return;
  QuadData quad;
  quad.texture_data_handle = texture_data_handle;
  quad.overlay_texture_data_handle = overlay_texture_data_handle;
  quad.model_view_proj_matrix = model_view_proj_matrix;
  quad.clip_rect = clip_rect;
  quad.opacity = opacity;
  quad.element_size = element_size;
  quad.corner_radius = corner_radius;
  quad.blend = blend;
  quad_queue_.push(quad);
}

void TexturedQuadRenderer::Flush() {
  if (quad_queue_.empty())
    return;

  int last_texture = -1;
  int last_overlay_texture = -1;
  float last_opacity = -1.0f;
  gfx::SizeF last_element_size;
  float last_corner_radius = -1.0f;
  gfx::RectF last_clip_rect;
  bool last_blend = true;  // All elements blend by default.

  // Set up GL state that doesn't change between draw calls.
  glUseProgram(program_handle_);

  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);

  // Link texture data with texture unit.
  glUniform1i(texture_handle_, 0);

  glUniform1i(overlay_texture_handle_, 1);

  // Set up position attribute.
  glVertexAttribPointer(position_handle_, kPositionDataSize, GL_FLOAT, false,
                        kDataStride, VOID_OFFSET(kPositionDataOffset));
  glEnableVertexAttribArray(position_handle_);

  // Set up offset scale attribute.
  glVertexAttribPointer(offset_scale_handle_, kOffsetScaleDataSize, GL_FLOAT,
                        false, kDataStride,
                        VOID_OFFSET(kOffsetScaleDataOffset));
  glEnableVertexAttribArray(offset_scale_handle_);

  // Set up corner position attribute.
  glVertexAttribPointer(corner_position_handle_, kCornerPositionDataSize,
                        GL_FLOAT, false, kDataStride,
                        VOID_OFFSET(kCornerPositionDataOffset));
  glEnableVertexAttribArray(corner_position_handle_);

  glUniform1i(uses_overlay_handle_, false);

  // TODO(bajones): This should eventually be changed to use instancing so that
  // the entire queue can be processed in one draw call. For now this still
  // significantly reduces the amount of state changes made per draw.
  while (!quad_queue_.empty()) {
    const QuadData& quad = quad_queue_.front();

    if (last_blend != quad.blend) {
      last_blend = quad.blend;
      if (quad.blend) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      } else {
        glDisable(GL_BLEND);
      }
    }

    // Only change texture ID or opacity when they differ between quads.
    if (last_texture != quad.texture_data_handle) {
      last_texture = quad.texture_data_handle;
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(TextureType(), last_texture);
      SetTexParameters(TextureType());
    }

    if (last_overlay_texture != quad.overlay_texture_data_handle) {
      last_overlay_texture = quad.overlay_texture_data_handle;
      glUniform1i(uses_overlay_handle_, quad.overlay_texture_data_handle != 0);
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(TextureType(), last_overlay_texture);
      SetTexParameters(TextureType());
      glUniform1f(overlay_opacity_handle_, last_overlay_texture ? 1.0f : 0.0f);
    }

    if (last_opacity != quad.opacity) {
      last_opacity = quad.opacity;
      glUniform1f(opacity_handle_, last_opacity);
    }

    bool corner_attributes_changed = quad.corner_radius != last_corner_radius ||
                                     quad.element_size != last_element_size;

    if (corner_attributes_changed) {
      last_corner_radius = quad.corner_radius;
      last_element_size = quad.element_size;
      if (quad.corner_radius == 0.0f) {
        glUniform2f(corner_offset_handle_, 0.0, 0.0);
      } else {
        glUniform2f(corner_offset_handle_,
                    quad.corner_radius / quad.element_size.width(),
                    quad.corner_radius / quad.element_size.height());
      }
    }

    // Pass in model view project matrix.
    glUniformMatrix4fv(model_view_proj_matrix_handle_, 1, false,
                       MatrixToGLArray(quad.model_view_proj_matrix).data());

    if (last_clip_rect != quad.clip_rect) {
      last_clip_rect = quad.clip_rect;
      const GLfloat clip_rect_data[4] = {quad.clip_rect.x(), quad.clip_rect.y(),
                                         quad.clip_rect.right(),
                                         quad.clip_rect.bottom()};
      glUniform2fv(clip_rect_handle_, 2, clip_rect_data);
    }

    if (quad.corner_radius == 0.0f) {
      glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                     VOID_OFFSET(kInnerRectOffset));
    } else {
      glDrawElements(GL_TRIANGLES, std::size(kIndices), GL_UNSIGNED_SHORT, 0);
    }

    quad_queue_.pop();
  }

  if (!last_blend) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  }

  glDisableVertexAttribArray(position_handle_);
  glDisableVertexAttribArray(offset_scale_handle_);
  glDisableVertexAttribArray(corner_position_handle_);
}

GLuint TexturedQuadRenderer::vertex_buffer_ = 0;
GLuint TexturedQuadRenderer::index_buffer_ = 0;

void TexturedQuadRenderer::CreateBuffers() {
  GLuint buffers[2];
  glGenBuffers(2, buffers);
  vertex_buffer_ = buffers[0];
  index_buffer_ = buffers[1];

  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER, std::size(kVertices) * sizeof(float), kVertices,
               GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, std::size(kIndices) * sizeof(GLushort),
               kIndices, GL_STATIC_DRAW);
}

GLenum TexturedQuadRenderer::TextureType() const {
  return GL_TEXTURE_2D;
}

const char* TexturedQuadRenderer::VertexShader() {
  return kVertexShader;
}

GLuint TexturedQuadRenderer::VertexBuffer() {
  return vertex_buffer_;
}

GLuint TexturedQuadRenderer::IndexBuffer() {
  return index_buffer_;
}

int TexturedQuadRenderer::NumQuadIndices() {
  return std::size(kIndices);
}

}  // namespace vr
