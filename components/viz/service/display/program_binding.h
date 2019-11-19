// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_PROGRAM_BINDING_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_PROGRAM_BINDING_H_

#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/service/display/shader.h"
#include "components/viz/service/viz_service_export.h"

namespace gfx {
class ColorTransform;
}

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}  // namespace gpu

namespace viz {

class VIZ_SERVICE_EXPORT ProgramBindingBase {
 public:
  ProgramBindingBase();
  ~ProgramBindingBase();

  bool Init(gpu::gles2::GLES2Interface* context,
            const std::string& vertex_shader,
            const std::string& fragment_shader);
  bool Link(gpu::gles2::GLES2Interface* context);
  void Cleanup(gpu::gles2::GLES2Interface* context);

  unsigned program() const { return program_; }
  bool initialized() const { return initialized_; }

 protected:
  unsigned LoadShader(gpu::gles2::GLES2Interface* context,
                      unsigned type,
                      const std::string& shader_source);
  unsigned CreateShaderProgram(gpu::gles2::GLES2Interface* context,
                               unsigned vertex_shader,
                               unsigned fragment_shader);
  void CleanupShaders(gpu::gles2::GLES2Interface* context);

  bool IsContextLost(gpu::gles2::GLES2Interface* context);

  unsigned program_;
  unsigned vertex_shader_id_;
  unsigned fragment_shader_id_;
  bool initialized_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProgramBindingBase);
};

enum ProgramType {
  PROGRAM_TYPE_DEBUG_BORDER,
  PROGRAM_TYPE_SOLID_COLOR,
  PROGRAM_TYPE_TILE,
  PROGRAM_TYPE_TEXTURE,
  PROGRAM_TYPE_RENDER_PASS,
  PROGRAM_TYPE_VIDEO_STREAM,
  PROGRAM_TYPE_YUV_VIDEO,
};

class VIZ_SERVICE_EXPORT ProgramKey {
 public:
  ProgramKey();
  ProgramKey(const ProgramKey& other);
  ~ProgramKey();

  static ProgramKey DebugBorder();
  static ProgramKey SolidColor(AAMode aa_mode,
                               bool tint_color,
                               bool rounded_corner);
  static ProgramKey Tile(TexCoordPrecision precision,
                         SamplerType sampler,
                         AAMode aa_mode,
                         PremultipliedAlphaMode premultiplied_alpha,
                         bool is_opaque,
                         bool has_tex_clamp_rect,
                         bool tint_color,
                         bool rounded_corner);
  static ProgramKey Texture(TexCoordPrecision precision,
                            SamplerType sampler,
                            PremultipliedAlphaMode premultiplied_alpha,
                            bool has_background_color,
                            bool has_tex_clamp_rect,
                            bool tint_color,
                            bool rounded_corner);

  // TODO(ccameron): Merge |mask_for_background| into MaskMode.
  static ProgramKey RenderPass(TexCoordPrecision precision,
                               SamplerType sampler,
                               BlendMode blend_mode,
                               AAMode aa_mode,
                               MaskMode mask_mode,
                               bool mask_for_background,
                               bool has_color_matrix,
                               bool tint_color,
                               bool rounded_corner);
  static ProgramKey VideoStream(TexCoordPrecision precision,
                                bool rounded_corner);
  static ProgramKey YUVVideo(TexCoordPrecision precision,
                             SamplerType sampler,
                             YUVAlphaTextureMode yuv_alpha_texture_mode,
                             UVTextureMode uv_texture_mode,
                             bool tint_color,
                             bool rounded_corner);

  bool operator==(const ProgramKey& other) const;
  bool operator!=(const ProgramKey& other) const;

  void SetColorTransform(const gfx::ColorTransform* transform);

  bool has_output_color_matrix() const { return has_output_color_matrix_; }
  void set_has_output_color_matrix(bool value) {
    has_output_color_matrix_ = value;
  }
  TexCoordPrecision tex_coord_precision() const { return precision_; }

 private:
  friend struct ProgramKeyHash;
  friend class Program;

  ProgramType type_ = PROGRAM_TYPE_DEBUG_BORDER;
  TexCoordPrecision precision_ = TEX_COORD_PRECISION_NA;
  SamplerType sampler_ = SAMPLER_TYPE_NA;
  BlendMode blend_mode_ = BLEND_MODE_NONE;
  AAMode aa_mode_ = NO_AA;
  bool is_opaque_ = false;

  PremultipliedAlphaMode premultiplied_alpha_ = PREMULTIPLIED_ALPHA;
  bool has_background_color_ = false;

  MaskMode mask_mode_ = NO_MASK;
  bool mask_for_background_ = false;
  bool has_color_matrix_ = false;

  YUVAlphaTextureMode yuv_alpha_texture_mode_ = YUV_NO_ALPHA_TEXTURE;
  UVTextureMode uv_texture_mode_ = UV_TEXTURE_MODE_NA;

  ColorConversionMode color_conversion_mode_ = COLOR_CONVERSION_MODE_NONE;
  const gfx::ColorTransform* color_transform_ = nullptr;

  bool has_tex_clamp_rect_ = false;

  bool has_output_color_matrix_ = false;
  bool has_tint_color_matrix_ = false;
  bool has_rounded_corner_ = false;
};

struct ProgramKeyHash {
  size_t operator()(const ProgramKey& key) const {
    return (static_cast<size_t>(key.type_) << 0) ^
           (static_cast<size_t>(key.precision_) << 3) ^
           (static_cast<size_t>(key.sampler_) << 6) ^
           (static_cast<size_t>(key.blend_mode_) << 9) ^
           (static_cast<size_t>(key.aa_mode_) << 15) ^
           (static_cast<size_t>(key.is_opaque_) << 17) ^
           (static_cast<size_t>(key.premultiplied_alpha_) << 19) ^
           (static_cast<size_t>(key.has_background_color_) << 20) ^
           (static_cast<size_t>(key.mask_mode_) << 21) ^
           (static_cast<size_t>(key.mask_for_background_) << 22) ^
           (static_cast<size_t>(key.has_color_matrix_) << 23) ^
           (static_cast<size_t>(key.yuv_alpha_texture_mode_) << 24) ^
           (static_cast<size_t>(key.uv_texture_mode_) << 25) ^
           (static_cast<size_t>(key.color_conversion_mode_) << 26) ^
           (static_cast<size_t>(key.has_tex_clamp_rect_) << 28) ^
           (static_cast<size_t>(key.has_output_color_matrix_) << 29) ^
           (static_cast<size_t>(key.has_tint_color_matrix_) << 30) ^
           (static_cast<size_t>(key.has_rounded_corner_) << 31);
  }
};

class VIZ_SERVICE_EXPORT Program : public ProgramBindingBase {
 public:
  Program() {}

  void Initialize(ContextProvider* context_provider, const ProgramKey& key) {
    // Set parameters that are common to all sub-classes.
    vertex_shader_.aa_mode_ = key.aa_mode_;
    fragment_shader_.aa_mode_ = key.aa_mode_;
    fragment_shader_.blend_mode_ = key.blend_mode_;
    fragment_shader_.tex_coord_precision_ = key.precision_;
    fragment_shader_.sampler_type_ = key.sampler_;
    fragment_shader_.premultiply_alpha_mode_ = key.premultiplied_alpha_;
    fragment_shader_.mask_mode_ = key.mask_mode_;
    fragment_shader_.mask_for_background_ = key.mask_for_background_;
    fragment_shader_.color_conversion_mode_ = key.color_conversion_mode_;
    fragment_shader_.color_transform_ = key.color_transform_;
    fragment_shader_.has_output_color_matrix_ = key.has_output_color_matrix_;
    fragment_shader_.has_tint_color_matrix_ = key.has_tint_color_matrix_;
    fragment_shader_.has_rounded_corner_ = key.has_rounded_corner_;

    switch (key.type_) {
      case PROGRAM_TYPE_DEBUG_BORDER:
        InitializeDebugBorderProgram();
        break;
      case PROGRAM_TYPE_SOLID_COLOR:
        InitializeSolidColorProgram(key);
        break;
      case PROGRAM_TYPE_TILE:
        InitializeTileProgram(key);
        break;
      case PROGRAM_TYPE_TEXTURE:
        InitializeTextureProgram(key);
        break;
      case PROGRAM_TYPE_RENDER_PASS:
        InitializeRenderPassProgram(key);
        break;
      case PROGRAM_TYPE_VIDEO_STREAM:
        InitializeVideoStreamProgram(key);
        break;
      case PROGRAM_TYPE_YUV_VIDEO:
        InitializeYUVVideo(key);
        break;
    }
    InitializeInternal(context_provider);
  }

  const VertexShader& vertex_shader() const { return vertex_shader_; }
  const FragmentShader& fragment_shader() const { return fragment_shader_; }

  // Functions for querying uniform locations.
  int vertex_tex_transform_location() const {
    return vertex_shader_.vertex_tex_transform_location_;
  }
  int tex_matrix_location() const {
    return vertex_shader_.tex_matrix_location_;
  }
  int ya_tex_scale_location() const {
    return vertex_shader_.ya_tex_scale_location_;
  }
  int ya_tex_offset_location() const {
    return vertex_shader_.ya_tex_offset_location_;
  }
  int uv_tex_scale_location() const {
    return vertex_shader_.uv_tex_scale_location_;
  }
  int uv_tex_offset_location() const {
    return vertex_shader_.uv_tex_offset_location_;
  }
  int matrix_location() const { return vertex_shader_.matrix_location_; }
  int vertex_opacity_location() const {
    return vertex_shader_.vertex_opacity_location_;
  }
  int viewport_location() const { return vertex_shader_.viewport_location_; }
  int edge_location() const { return vertex_shader_.edge_location_; }
  int quad_location() const { return vertex_shader_.quad_location_; }

  int sampler_location() const { return fragment_shader_.sampler_location_; }
  int alpha_location() const { return fragment_shader_.alpha_location_; }
  int color_location() const { return fragment_shader_.color_location_; }
  int background_color_location() const {
    return fragment_shader_.background_color_location_;
  }
  int fragment_tex_transform_location() const {
    return fragment_shader_.fragment_tex_transform_location_;
  }
  int backdrop_location() const { return fragment_shader_.backdrop_location_; }
  int backdrop_rect_location() const {
    return fragment_shader_.backdrop_rect_location_;
  }
  int original_backdrop_location() const {
    return fragment_shader_.original_backdrop_location_;
  }
  int mask_sampler_location() const {
    return fragment_shader_.mask_sampler_location_;
  }
  int mask_tex_coord_scale_location() const {
    return fragment_shader_.mask_tex_coord_scale_location_;
  }
  int mask_tex_coord_offset_location() const {
    return fragment_shader_.mask_tex_coord_offset_location_;
  }
  int color_matrix_location() const {
    return fragment_shader_.color_matrix_location_;
  }
  int color_offset_location() const {
    return fragment_shader_.color_offset_location_;
  }
  int tex_clamp_rect_location() const {
    return fragment_shader_.tex_clamp_rect_location_;
  }
  int y_texture_location() const {
    return fragment_shader_.y_texture_location_;
  }
  int u_texture_location() const {
    return fragment_shader_.u_texture_location_;
  }
  int v_texture_location() const {
    return fragment_shader_.v_texture_location_;
  }
  int uv_texture_location() const {
    return fragment_shader_.uv_texture_location_;
  }
  int a_texture_location() const {
    return fragment_shader_.a_texture_location_;
  }
  int lut_texture_location() const {
    return fragment_shader_.lut_texture_location_;
  }
  int lut_size_location() const { return fragment_shader_.lut_size_location_; }
  int resource_multiplier_location() const {
    return fragment_shader_.resource_multiplier_location_;
  }
  int resource_offset_location() const {
    return fragment_shader_.resource_offset_location_;
  }
  int ya_clamp_rect_location() const {
    return fragment_shader_.ya_clamp_rect_location_;
  }
  int uv_clamp_rect_location() const {
    return fragment_shader_.uv_clamp_rect_location_;
  }
  int output_color_matrix_location() const {
    return fragment_shader_.output_color_matrix_location_;
  }
  int tint_color_matrix_location() const {
    return fragment_shader_.tint_color_matrix_location_;
  }
  int rounded_corner_rect_location() const {
    return fragment_shader_.rounded_corner_rect_location_;
  }
  int rounded_corner_radius_location() const {
    return fragment_shader_.rounded_corner_radius_location_;
  }

  const gfx::ColorTransform* color_transform_for_testing() const {
    return fragment_shader_.color_transform_;
  }

 private:
  void InitializeDebugBorderProgram() {
    // Initialize fragment program.
    fragment_shader_.input_color_type_ = INPUT_COLOR_SOURCE_UNIFORM;
    fragment_shader_.frag_color_mode_ = FRAG_COLOR_MODE_DEFAULT;
  }

  void InitializeSolidColorProgram(const ProgramKey& key) {
    // Initialize vertex program.
    vertex_shader_.position_source_ = POSITION_SOURCE_ATTRIBUTE_INDEXED_UNIFORM;
#if defined(OS_ANDROID)
    if (key.aa_mode_ == NO_AA)
      vertex_shader_.has_dummy_variables_ = true;
#endif

    // Initialize fragment program.
    fragment_shader_.input_color_type_ = INPUT_COLOR_SOURCE_UNIFORM;
    fragment_shader_.frag_color_mode_ = FRAG_COLOR_MODE_DEFAULT;
  }

  void InitializeTileProgram(const ProgramKey& key) {
    // Initialize vertex program.
    vertex_shader_.position_source_ = POSITION_SOURCE_ATTRIBUTE_INDEXED_UNIFORM;
    vertex_shader_.tex_coord_transform_ = TEX_COORD_TRANSFORM_VEC4;
    vertex_shader_.tex_coord_source_ = TEX_COORD_SOURCE_ATTRIBUTE;

    // Initialize fragment program.
    fragment_shader_.has_tex_clamp_rect_ = key.has_tex_clamp_rect_;
    if (key.is_opaque_) {
      DCHECK_EQ(key.aa_mode_, NO_AA);
      fragment_shader_.frag_color_mode_ = FRAG_COLOR_MODE_OPAQUE;
    } else {
      // TODO(ccameron): This branch shouldn't be needed (this is always
      // BLEND_MODE_NONE).
      if (key.aa_mode_ == NO_AA)
        fragment_shader_.frag_color_mode_ = FRAG_COLOR_MODE_APPLY_BLEND_MODE;
      fragment_shader_.has_uniform_alpha_ = true;
    }
    // AA changes the texture coordinate mode (affecting both shaders).
    if (key.aa_mode_ == USE_AA) {
      vertex_shader_.tex_coord_source_ = TEX_COORD_SOURCE_POSITION;
      vertex_shader_.aa_mode_ = USE_AA;
      fragment_shader_.has_rgba_fragment_tex_transform_ = true;
      // Tiles that have AA do their own clamping.
      DCHECK(!fragment_shader_.has_tex_clamp_rect_);
    }
  }

  void InitializeTextureProgram(const ProgramKey& key) {
    // Initialize vertex program.
    vertex_shader_.tex_coord_source_ = TEX_COORD_SOURCE_ATTRIBUTE;
    vertex_shader_.tex_coord_transform_ = TEX_COORD_TRANSFORM_VEC4;
    vertex_shader_.has_vertex_opacity_ = true;
    vertex_shader_.use_uniform_arrays_ = !key.has_tex_clamp_rect_;

    // Initialize fragment program.
    fragment_shader_.has_varying_alpha_ = true;
    fragment_shader_.has_background_color_ = key.has_background_color_;
    fragment_shader_.has_tex_clamp_rect_ = key.has_tex_clamp_rect_;
  }

  void InitializeRenderPassProgram(const ProgramKey& key) {
    // Initialize vertex program.
    if (key.aa_mode_ == NO_AA) {
      vertex_shader_.tex_coord_source_ = TEX_COORD_SOURCE_ATTRIBUTE;
      vertex_shader_.tex_coord_transform_ = TEX_COORD_TRANSFORM_VEC4;
      vertex_shader_.has_vertex_opacity_ = true;
      vertex_shader_.use_uniform_arrays_ = true;
    } else {
      vertex_shader_.position_source_ =
          POSITION_SOURCE_ATTRIBUTE_INDEXED_UNIFORM;
      vertex_shader_.tex_coord_source_ = TEX_COORD_SOURCE_POSITION;
      vertex_shader_.tex_coord_transform_ = TEX_COORD_TRANSFORM_TRANSLATED_VEC4;
    }

    // Initialize fragment program.
    fragment_shader_.frag_color_mode_ = FRAG_COLOR_MODE_APPLY_BLEND_MODE;
    fragment_shader_.has_uniform_alpha_ = true;
    fragment_shader_.has_color_matrix_ = key.has_color_matrix_;
    if (key.mask_mode_ == HAS_MASK) {
      fragment_shader_.ignore_sampler_type_ = true;
    } else {
      DCHECK(!key.mask_for_background_);
    }
  }

  void InitializeVideoStreamProgram(const ProgramKey& key) {
    vertex_shader_.tex_coord_source_ = TEX_COORD_SOURCE_ATTRIBUTE;
    vertex_shader_.tex_coord_transform_ = TEX_COORD_TRANSFORM_MATRIX;
    DCHECK_EQ(key.sampler_, SAMPLER_TYPE_EXTERNAL_OES);
  }

  void InitializeYUVVideo(const ProgramKey& key) {
    vertex_shader_.tex_coord_source_ = TEX_COORD_SOURCE_ATTRIBUTE;
    vertex_shader_.is_ya_uv_ = true;

    fragment_shader_.input_color_type_ = INPUT_COLOR_SOURCE_YUV_TEXTURES;
    fragment_shader_.has_uniform_alpha_ = true;
    fragment_shader_.yuv_alpha_texture_mode_ = key.yuv_alpha_texture_mode_;
    fragment_shader_.uv_texture_mode_ = key.uv_texture_mode_;
  }

  void InitializeInternal(ContextProvider* context_provider) {
    DCHECK(context_provider);
    DCHECK(!initialized_);

    if (IsContextLost(context_provider->ContextGL()))
      return;

    if (!ProgramBindingBase::Init(context_provider->ContextGL(),
                                  vertex_shader_.GetShaderString(),
                                  fragment_shader_.GetShaderString())) {
      DCHECK(IsContextLost(context_provider->ContextGL()));
      return;
    }

    int base_uniform_index = 0;
    vertex_shader_.Init(context_provider->ContextGL(), program_,
                        &base_uniform_index);
    fragment_shader_.Init(context_provider->ContextGL(), program_,
                          &base_uniform_index);

    // Link after binding uniforms
    if (!Link(context_provider->ContextGL())) {
      DCHECK(IsContextLost(context_provider->ContextGL()));
      return;
    }

    initialized_ = true;
  }

  VertexShader vertex_shader_;
  FragmentShader fragment_shader_;

  DISALLOW_COPY_AND_ASSIGN(Program);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_PROGRAM_BINDING_H_
