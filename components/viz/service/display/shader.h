// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_SHADER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_SHADER_H_

#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "components/viz/service/viz_service_export.h"

namespace gfx {
class ColorTransform;
class Point;
class Size;
}  // namespace gfx

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}  // namespace gpu

namespace viz {

enum TexCoordPrecision {
  TEX_COORD_PRECISION_NA = 0,
  TEX_COORD_PRECISION_MEDIUM = 1,
  TEX_COORD_PRECISION_HIGH = 2,
};

// Texture coordinate sources for the vertex shader.
enum TexCoordSource {
  // Vertex shader does not populate a texture coordinate.
  TEX_COORD_SOURCE_NONE,
  // Texture coordinate is set to the untransformed position.
  TEX_COORD_SOURCE_POSITION,
  // Texture coordinate has its own attribute.
  TEX_COORD_SOURCE_ATTRIBUTE,
};

// Texture coordinate transformation modes for the vertex shader.
enum TexCoordTransform {
  // Texture coordinates are not transformed.
  TEX_COORD_TRANSFORM_NONE,
  // Texture coordinates are transformed by a uniform vec4, scaling by zw and
  // then translating by xy.
  TEX_COORD_TRANSFORM_VEC4,
  // Same as the above, but add vec2(0.5) to the texture coordinate first.
  TEX_COORD_TRANSFORM_TRANSLATED_VEC4,
  // Texture coordiantes are transformed by a uniform mat4.
  TEX_COORD_TRANSFORM_MATRIX,
};

// Position source for the vertex shader.
enum PositionSource {
  // The position is read directly from the position attribute.
  POSITION_SOURCE_ATTRIBUTE,
  // The position is read by attribute index into a uniform array for xy, and
  // getting zw from the attribute.
  POSITION_SOURCE_ATTRIBUTE_INDEXED_UNIFORM,
};

enum AAMode {
  NO_AA = 0,
  USE_AA = 1,
};

enum PremultipliedAlphaMode {
  PREMULTIPLIED_ALPHA = 0,
  NON_PREMULTIPLIED_ALPHA = 1,
};

enum SamplerType {
  SAMPLER_TYPE_NA = 0,
  SAMPLER_TYPE_2D = 1,
  SAMPLER_TYPE_2D_RECT = 2,
  SAMPLER_TYPE_EXTERNAL_OES = 3,
};

enum BlendMode {
  BLEND_MODE_NONE,
  BLEND_MODE_NORMAL,
  BLEND_MODE_DESTINATION_IN,
  BLEND_MODE_SCREEN,
  BLEND_MODE_OVERLAY,
  BLEND_MODE_DARKEN,
  BLEND_MODE_LIGHTEN,
  BLEND_MODE_COLOR_DODGE,
  BLEND_MODE_COLOR_BURN,
  BLEND_MODE_HARD_LIGHT,
  BLEND_MODE_SOFT_LIGHT,
  BLEND_MODE_DIFFERENCE,
  BLEND_MODE_EXCLUSION,
  BLEND_MODE_MULTIPLY,
  BLEND_MODE_HUE,
  BLEND_MODE_SATURATION,
  BLEND_MODE_COLOR,
  BLEND_MODE_LUMINOSITY,
  LAST_BLEND_MODE = BLEND_MODE_LUMINOSITY
};

enum InputColorSource {
  // This includes RGB and RGBA textures.
  INPUT_COLOR_SOURCE_RGBA_TEXTURE,
  // This includes Y and either UV or U-and-V textures.
  INPUT_COLOR_SOURCE_YUV_TEXTURES,
  // A solid color specified as a uniform value.
  INPUT_COLOR_SOURCE_UNIFORM,
};

enum UVTextureMode {
  // Shader does not use YUV textures.
  UV_TEXTURE_MODE_NA,
  // UV plane is a single texture.
  UV_TEXTURE_MODE_UV,
  // U and V planes have separate textures.
  UV_TEXTURE_MODE_U_V,
};

enum YUVAlphaTextureMode {
  YUV_ALPHA_TEXTURE_MODE_NA,
  YUV_NO_ALPHA_TEXTURE,
  YUV_HAS_ALPHA_TEXTURE,
};

enum ColorConversionMode {
  // No color conversion is performed.
  COLOR_CONVERSION_MODE_NONE,
  // Conversion is done directly from input RGB space (or YUV space if
  // applicable) to output RGB space, via a 3D texture represented as a 2D
  // texture.
  COLOR_CONVERSION_MODE_LUT,
  // Conversion is done analytically in the shader.
  COLOR_CONVERSION_MODE_SHADER,
};

// TODO(ccameron): Merge this with BlendMode.
enum FragColorMode {
  FRAG_COLOR_MODE_DEFAULT,
  FRAG_COLOR_MODE_OPAQUE,
  FRAG_COLOR_MODE_APPLY_BLEND_MODE,
};

enum MaskMode {
  NO_MASK = 0,
  HAS_MASK = 1,
};

// Note: The highp_threshold_cache must be provided by the caller to make
// the caching multi-thread/context safe in an easy low-overhead manner.
// The caller must make sure to clear highp_threshold_cache to 0, so it can be
// reinitialized, if a new or different context is used.
VIZ_SERVICE_EXPORT TexCoordPrecision
TexCoordPrecisionRequired(gpu::gles2::GLES2Interface* context,
                          int* highp_threshold_cache,
                          int highp_threshold_min,
                          const gfx::Point& max_coordinate);

VIZ_SERVICE_EXPORT TexCoordPrecision
TexCoordPrecisionRequired(gpu::gles2::GLES2Interface* context,
                          int* highp_threshold_cache,
                          int highp_threshold_min,
                          const gfx::Size& max_size);

class VIZ_SERVICE_EXPORT VertexShader {
 public:
  VertexShader();
  void Init(gpu::gles2::GLES2Interface* context,
            unsigned program,
            int* base_uniform_index);
  std::string GetShaderString() const;

 protected:
  friend class Program;

  // Use arrays of uniforms for matrix, texTransform, and opacity.
  bool use_uniform_arrays_ = false;

  PositionSource position_source_ = POSITION_SOURCE_ATTRIBUTE;
  TexCoordSource tex_coord_source_ = TEX_COORD_SOURCE_NONE;
  TexCoordTransform tex_coord_transform_ = TEX_COORD_TRANSFORM_NONE;

  // Used only with TEX_COORD_TRANSFORM_VEC4.
  int vertex_tex_transform_location_ = -1;

  // Used only with TEX_COORD_TRANSFORM_MATRIX.
  int tex_matrix_location_ = -1;

  // Uniforms for YUV textures.
  bool is_ya_uv_ = false;
  int ya_tex_scale_location_ = -1;
  int ya_tex_offset_location_ = -1;
  int uv_tex_scale_location_ = -1;
  int uv_tex_offset_location_ = -1;

  // Matrix to transform the position.
  int matrix_location_ = -1;

  // Used only with POSITION_SOURCE_ATTRIBUTE_INDEXED_UNIFORM.
  int quad_location_ = -1;

  // Extra dummy variables to work around bugs on Android.
  // TODO(ccameron): This is likley unneeded cargo-culting.
  // http://crbug.com/240602
  bool has_dummy_variables_ = false;

  bool has_vertex_opacity_ = false;
  int vertex_opacity_location_ = -1;

  AAMode aa_mode_ = NO_AA;
  int viewport_location_ = -1;
  int edge_location_ = -1;
};

class VIZ_SERVICE_EXPORT FragmentShader {
 public:
  virtual void Init(gpu::gles2::GLES2Interface* context,
                    unsigned program,
                    int* base_uniform_index);
  std::string GetShaderString() const;

 protected:
  FragmentShader();
  virtual std::string GetShaderSource() const;
  bool has_blend_mode() const { return blend_mode_ != BLEND_MODE_NONE; }

  void SetBlendModeFunctions(std::string* shader_string) const;
  void SetRoundedCornerFunctions(std::string* shader_string) const;

  // Settings that are modified by sub-classes.
  AAMode aa_mode_ = NO_AA;
  bool has_varying_alpha_ = false;
  PremultipliedAlphaMode premultiply_alpha_mode_ = PREMULTIPLIED_ALPHA;
  FragColorMode frag_color_mode_ = FRAG_COLOR_MODE_DEFAULT;
  InputColorSource input_color_type_ = INPUT_COLOR_SOURCE_RGBA_TEXTURE;

  // Used only if |blend_mode_| is not BLEND_MODE_NONE.
  int backdrop_location_ = -1;
  int original_backdrop_location_ = -1;
  int backdrop_rect_location_ = -1;

  // Used only if |input_color_type_| is INPUT_COLOR_SOURCE_RGBA_TEXTURE.
  bool has_rgba_fragment_tex_transform_ = false;
  int sampler_location_ = -1;
  int fragment_tex_transform_location_ = -1;

  // Always use sampler2D and texture2D for the RGBA texture, regardless of the
  // specified SamplerType.
  // TODO(ccameron): Change GLRenderer to always specify the correct
  // SamplerType.
  bool ignore_sampler_type_ = false;

  // Used only if |input_color_type_| is INPUT_COLOR_SOURCE_UNIFORM.
  int color_location_ = -1;

  MaskMode mask_mode_ = NO_MASK;
  int mask_sampler_location_ = -1;
  int mask_tex_coord_scale_location_ = -1;
  int mask_tex_coord_offset_location_ = -1;

  bool has_color_matrix_ = false;
  int color_matrix_location_ = -1;
  int color_offset_location_ = -1;

  bool has_uniform_alpha_ = false;
  int alpha_location_ = -1;

  bool has_background_color_ = false;
  int background_color_location_ = -1;

  bool has_tex_clamp_rect_ = false;
  int tex_clamp_rect_location_ = -1;

  TexCoordPrecision tex_coord_precision_ = TEX_COORD_PRECISION_NA;
  SamplerType sampler_type_ = SAMPLER_TYPE_NA;

  BlendMode blend_mode_ = BLEND_MODE_NONE;
  bool mask_for_background_ = false;

  // YUV-only parameters.
  YUVAlphaTextureMode yuv_alpha_texture_mode_ = YUV_ALPHA_TEXTURE_MODE_NA;
  UVTextureMode uv_texture_mode_ = UV_TEXTURE_MODE_UV;

  ColorConversionMode color_conversion_mode_ = COLOR_CONVERSION_MODE_NONE;
  const gfx::ColorTransform* color_transform_ = nullptr;

  bool has_output_color_matrix_ = false;
  int output_color_matrix_location_ = -1;

  bool has_tint_color_matrix_ = false;
  int tint_color_matrix_location_ = -1;

  // YUV uniform locations.
  int y_texture_location_ = -1;
  int u_texture_location_ = -1;
  int v_texture_location_ = -1;
  int uv_texture_location_ = -1;
  int a_texture_location_ = -1;
  int ya_clamp_rect_location_ = -1;
  int uv_clamp_rect_location_ = -1;

  // Rounded corner locations
  bool has_rounded_corner_ = false;
  int rounded_corner_rect_location_ = -1;
  int rounded_corner_radius_location_ = -1;

  // The resource offset and multiplier to adjust for bit depth.
  int resource_multiplier_location_ = -1;
  int resource_offset_location_ = -1;

  // LUT YUV to color-converted RGB.
  int lut_texture_location_ = -1;
  int lut_size_location_ = -1;

 private:
  friend class Program;

  void AppendHelperFunctions(std::string* buffer) const;
  void AppendBlendFunction(std::string* buffer) const;
  base::StringPiece GetBlendFunctionBodyForAlpha() const;
  base::StringPiece GetBlendFunctionBodyForRGB() const;

  DISALLOW_COPY_AND_ASSIGN(FragmentShader);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_SHADER_H_
