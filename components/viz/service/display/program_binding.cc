// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/program_binding.h"

#include "base/trace_event/trace_event.h"
#include "components/viz/service/display/geometry_binding.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/gfx/color_transform.h"

using gpu::gles2::GLES2Interface;

namespace viz {

ProgramKey::ProgramKey() = default;

ProgramKey::ProgramKey(const ProgramKey& other) = default;

ProgramKey::~ProgramKey() = default;

bool ProgramKey::operator==(const ProgramKey& other) const {
  return type_ == other.type_ && precision_ == other.precision_ &&
         sampler_ == other.sampler_ && blend_mode_ == other.blend_mode_ &&
         aa_mode_ == other.aa_mode_ && is_opaque_ == other.is_opaque_ &&
         premultiplied_alpha_ == other.premultiplied_alpha_ &&
         has_background_color_ == other.has_background_color_ &&
         has_tex_clamp_rect_ == other.has_tex_clamp_rect_ &&
         mask_mode_ == other.mask_mode_ &&
         mask_for_background_ == other.mask_for_background_ &&
         has_color_matrix_ == other.has_color_matrix_ &&
         yuv_alpha_texture_mode_ == other.yuv_alpha_texture_mode_ &&
         uv_texture_mode_ == other.uv_texture_mode_ &&
         color_conversion_mode_ == other.color_conversion_mode_ &&
         color_transform_ == other.color_transform_ &&
         has_output_color_matrix_ == other.has_output_color_matrix_ &&
         has_rounded_corner_ == other.has_rounded_corner_;
}

bool ProgramKey::operator!=(const ProgramKey& other) const {
  return !(*this == other);
}

// static
ProgramKey ProgramKey::DebugBorder() {
  ProgramKey result;
  result.type_ = PROGRAM_TYPE_DEBUG_BORDER;
  return result;
}

// static
ProgramKey ProgramKey::SolidColor(AAMode aa_mode,
                                  bool tint_color,
                                  bool rounded_corner) {
  ProgramKey result;
  result.type_ = PROGRAM_TYPE_SOLID_COLOR;
  result.aa_mode_ = aa_mode;
  result.has_tint_color_matrix_ = tint_color;
  result.has_rounded_corner_ = rounded_corner;
  return result;
}

// static
ProgramKey ProgramKey::Tile(TexCoordPrecision precision,
                            SamplerType sampler,
                            AAMode aa_mode,
                            PremultipliedAlphaMode premultiplied_alpha,
                            bool is_opaque,
                            bool has_tex_clamp_rect,
                            bool tint_color,
                            bool rounded_corner) {
  ProgramKey result;
  result.type_ = PROGRAM_TYPE_TILE;
  result.precision_ = precision;
  result.sampler_ = sampler;
  result.aa_mode_ = aa_mode;
  result.is_opaque_ = is_opaque;
  result.has_tex_clamp_rect_ = has_tex_clamp_rect;
  result.has_tint_color_matrix_ = tint_color;
  result.premultiplied_alpha_ = premultiplied_alpha;
  result.has_rounded_corner_ = rounded_corner;
  return result;
}

// static
ProgramKey ProgramKey::Texture(TexCoordPrecision precision,
                               SamplerType sampler,
                               PremultipliedAlphaMode premultiplied_alpha,
                               bool has_background_color,
                               bool has_tex_clamp_rect,
                               bool tint_color,
                               bool rounded_corner) {
  ProgramKey result;
  result.type_ = PROGRAM_TYPE_TEXTURE;
  result.precision_ = precision;
  result.sampler_ = sampler;
  result.premultiplied_alpha_ = premultiplied_alpha;
  result.has_background_color_ = has_background_color;
  result.has_tex_clamp_rect_ = has_tex_clamp_rect;
  result.has_tint_color_matrix_ = tint_color;
  result.has_rounded_corner_ = rounded_corner;
  return result;
}

// static
ProgramKey ProgramKey::RenderPass(TexCoordPrecision precision,
                                  SamplerType sampler,
                                  BlendMode blend_mode,
                                  AAMode aa_mode,
                                  MaskMode mask_mode,
                                  bool mask_for_background,
                                  bool has_color_matrix,
                                  bool tint_color,
                                  bool rounded_corner) {
  ProgramKey result;
  result.type_ = PROGRAM_TYPE_RENDER_PASS;
  result.precision_ = precision;
  result.sampler_ = sampler;
  result.blend_mode_ = blend_mode;
  result.aa_mode_ = aa_mode;
  result.mask_mode_ = mask_mode;
  result.mask_for_background_ = mask_for_background;
  result.has_color_matrix_ = has_color_matrix;
  result.has_tint_color_matrix_ = tint_color;
  result.has_rounded_corner_ = rounded_corner;
  return result;
}

// static
ProgramKey ProgramKey::VideoStream(TexCoordPrecision precision,
                                   bool rounded_corner) {
  ProgramKey result;
  result.type_ = PROGRAM_TYPE_VIDEO_STREAM;
  result.precision_ = precision;
  result.sampler_ = SAMPLER_TYPE_EXTERNAL_OES;
  result.has_rounded_corner_ = rounded_corner;
  return result;
}

// static
ProgramKey ProgramKey::YUVVideo(TexCoordPrecision precision,
                                SamplerType sampler,
                                YUVAlphaTextureMode yuv_alpha_texture_mode,
                                UVTextureMode uv_texture_mode,
                                bool tint_color,
                                bool rounded_corner) {
  ProgramKey result;
  result.type_ = PROGRAM_TYPE_YUV_VIDEO;
  result.precision_ = precision;
  result.sampler_ = sampler;
  result.yuv_alpha_texture_mode_ = yuv_alpha_texture_mode;
  DCHECK(yuv_alpha_texture_mode == YUV_NO_ALPHA_TEXTURE ||
         yuv_alpha_texture_mode == YUV_HAS_ALPHA_TEXTURE);
  result.uv_texture_mode_ = uv_texture_mode;
  DCHECK(uv_texture_mode == UV_TEXTURE_MODE_UV ||
         uv_texture_mode == UV_TEXTURE_MODE_U_V);
  result.has_tint_color_matrix_ = tint_color;
  result.has_rounded_corner_ = rounded_corner;
  return result;
}

void ProgramKey::SetColorTransform(const gfx::ColorTransform* transform) {
  color_transform_ = nullptr;
  if (transform->IsIdentity()) {
    color_conversion_mode_ = COLOR_CONVERSION_MODE_NONE;
  } else if (transform->CanGetShaderSource()) {
    color_conversion_mode_ = COLOR_CONVERSION_MODE_SHADER;
    color_transform_ = transform;
  } else {
    color_conversion_mode_ = COLOR_CONVERSION_MODE_LUT;
  }
}

ProgramBindingBase::ProgramBindingBase()
    : program_(0),
      vertex_shader_id_(0),
      fragment_shader_id_(0),
      initialized_(false) {}

ProgramBindingBase::~ProgramBindingBase() {
  // If you hit these asserts, you initialized but forgot to call Cleanup().
  DCHECK(!program_);
  DCHECK(!vertex_shader_id_);
  DCHECK(!fragment_shader_id_);
  DCHECK(!initialized_);
}

bool ProgramBindingBase::Init(GLES2Interface* context,
                              const std::string& vertex_shader,
                              const std::string& fragment_shader) {
  TRACE_EVENT0("viz", "ProgramBindingBase::init");
  vertex_shader_id_ = LoadShader(context, GL_VERTEX_SHADER, vertex_shader);
  if (!vertex_shader_id_)
    return false;

  fragment_shader_id_ =
      LoadShader(context, GL_FRAGMENT_SHADER, fragment_shader);
  if (!fragment_shader_id_) {
    context->DeleteShader(vertex_shader_id_);
    vertex_shader_id_ = 0;
    return false;
  }

  program_ =
      CreateShaderProgram(context, vertex_shader_id_, fragment_shader_id_);
  return !!program_;
}

bool ProgramBindingBase::Link(GLES2Interface* context) {
  context->LinkProgram(program_);
  CleanupShaders(context);
  if (!program_)
    return false;
#ifndef NDEBUG
  int linked = 0;
  context->GetProgramiv(program_, GL_LINK_STATUS, &linked);
  if (!linked) {
    char buffer[1024] = "";
    context->GetProgramInfoLog(program_, sizeof(buffer), nullptr, buffer);
    DLOG(ERROR) << "Error compiling shader: " << buffer;
    return false;
  }
#endif
  return true;
}

void ProgramBindingBase::Cleanup(GLES2Interface* context) {
  initialized_ = false;
  if (!program_)
    return;

  DCHECK(context);
  context->DeleteProgram(program_);
  program_ = 0;

  CleanupShaders(context);
}

unsigned ProgramBindingBase::LoadShader(GLES2Interface* context,
                                        unsigned type,
                                        const std::string& shader_source) {
  unsigned shader = context->CreateShader(type);
  if (!shader)
    return 0u;

  const char* shader_source_str[] = {shader_source.data()};
  int shader_length[] = {static_cast<int>(shader_source.length())};
  context->ShaderSource(shader, 1, shader_source_str, shader_length);
  context->CompileShader(shader);
#if DCHECK_IS_ON()
  int compiled = 0;
  context->GetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (!compiled) {
    char buffer[1024] = "";
    context->GetShaderInfoLog(shader, sizeof(buffer), nullptr, buffer);
    DLOG(ERROR) << "Error compiling shader: " << buffer
                << "\n shader program: " << shader_source;
    return 0u;
  }
#endif
  return shader;
}

unsigned ProgramBindingBase::CreateShaderProgram(GLES2Interface* context,
                                                 unsigned vertex_shader,
                                                 unsigned fragment_shader) {
  unsigned program_object = context->CreateProgram();
  if (!program_object)
    return 0;

  context->AttachShader(program_object, vertex_shader);
  context->AttachShader(program_object, fragment_shader);

  // Bind the common attrib locations.
  context->BindAttribLocation(
      program_object, GeometryBinding::PositionAttribLocation(), "a_position");
  context->BindAttribLocation(
      program_object, GeometryBinding::TexCoordAttribLocation(), "a_texCoord");
  context->BindAttribLocation(program_object,
                              GeometryBinding::TriangleIndexAttribLocation(),
                              "a_index");

  return program_object;
}

void ProgramBindingBase::CleanupShaders(GLES2Interface* context) {
  if (vertex_shader_id_) {
    context->DeleteShader(vertex_shader_id_);
    vertex_shader_id_ = 0;
  }
  if (fragment_shader_id_) {
    context->DeleteShader(fragment_shader_id_);
    fragment_shader_id_ = 0;
  }
}

bool ProgramBindingBase::IsContextLost(GLES2Interface* context) {
  return context->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
}

}  // namespace viz
