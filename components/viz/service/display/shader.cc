// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/shader.h"

#include <stddef.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/strings/char_traits.h"
#include "base/strings/stringprintf.h"
#include "components/viz/service/display/static_geometry_binding.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/gfx/color_transform.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

constexpr base::StringPiece StripLambda(base::StringPiece shader) {
  // Must contain at least "[]() {}".
  DCHECK(shader.starts_with("[]() {"));
  DCHECK(shader.ends_with("}"));
  shader.remove_prefix(6);
  shader.remove_suffix(1);
  return shader;
}

// Shaders are passed in with lambda syntax, which tricks clang-format into
// handling them correctly. StripLambda removes this.
#define SHADER0(Src) StripLambda(#Src)

#define HDR(x)        \
  do {                \
    header += x "\n"; \
  } while (0)
#define SRC(x)             \
  do {                     \
    source += "  " x "\n"; \
  } while (0)

using gpu::gles2::GLES2Interface;

namespace viz {

namespace {

static void GetProgramUniformLocations(GLES2Interface* context,
                                       unsigned program,
                                       size_t count,
                                       const char** uniforms,
                                       int* locations,
                                       int* base_uniform_index) {
  for (size_t i = 0; i < count; i++) {
    locations[i] = (*base_uniform_index)++;
    context->BindUniformLocationCHROMIUM(program, locations[i], uniforms[i]);
  }
}

static void SetFragmentTexCoordPrecision(TexCoordPrecision requested_precision,
                                         std::string* shader_string) {
  const char* prefix = "";
  switch (requested_precision) {
    case TEX_COORD_PRECISION_HIGH:
      DCHECK_NE(shader_string->find("TexCoordPrecision"), std::string::npos);
      prefix =
          "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
          "  #define TexCoordPrecision highp\n"
          "#else\n"
          "  #define TexCoordPrecision mediump\n"
          "#endif\n";
      break;
    case TEX_COORD_PRECISION_MEDIUM:
      DCHECK_NE(shader_string->find("TexCoordPrecision"), std::string::npos);
      prefix = "#define TexCoordPrecision mediump\n";
      break;
    case TEX_COORD_PRECISION_NA:
      DCHECK_EQ(shader_string->find("TexCoordPrecision"), std::string::npos);
      DCHECK_EQ(shader_string->find("texture2D"), std::string::npos);
      DCHECK_EQ(shader_string->find("texture2DRect"), std::string::npos);
      break;
    default:
      NOTREACHED();
      break;
  }
  const char* lut_prefix = "#define LutLookup texture2D\n";
  shader_string->insert(0, prefix);
  shader_string->insert(0, lut_prefix);
}

TexCoordPrecision TexCoordPrecisionRequired(GLES2Interface* context,
                                            int* highp_threshold_cache,
                                            int highp_threshold_min,
                                            int x,
                                            int y) {
  if (*highp_threshold_cache == 0) {
    // Initialize range and precision with minimum spec values for when
    // GetShaderPrecisionFormat is a test stub.
    // TODO(brianderson): Implement better stubs of GetShaderPrecisionFormat
    // everywhere.
    GLint range[2] = {14, 14};
    GLint precision = 10;
    context->GetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT,
                                      range, &precision);
    *highp_threshold_cache = 1 << precision;
  }

  int highp_threshold = std::max(*highp_threshold_cache, highp_threshold_min);
  if (x > highp_threshold || y > highp_threshold)
    return TEX_COORD_PRECISION_HIGH;
  return TEX_COORD_PRECISION_MEDIUM;
}

void SetFragmentSamplerType(SamplerType requested_type,
                            std::string* shader_string) {
  const char* prefix = nullptr;
  switch (requested_type) {
    case SAMPLER_TYPE_2D:
      DCHECK_NE(shader_string->find("SamplerType"), std::string::npos);
      DCHECK_NE(shader_string->find("TextureLookup"), std::string::npos);
      prefix =
          "#define SamplerType sampler2D\n"
          "#define TextureLookup texture2D\n";
      break;
    case SAMPLER_TYPE_2D_RECT:
      DCHECK_NE(shader_string->find("SamplerType"), std::string::npos);
      DCHECK_NE(shader_string->find("TextureLookup"), std::string::npos);
      prefix =
          "#extension GL_ARB_texture_rectangle : require\n"
          "#define SamplerType sampler2DRect\n"
          "#define TextureLookup texture2DRect\n";
      break;
    case SAMPLER_TYPE_EXTERNAL_OES:
      DCHECK_NE(shader_string->find("SamplerType"), std::string::npos);
      DCHECK_NE(shader_string->find("TextureLookup"), std::string::npos);
      prefix =
          "#extension GL_OES_EGL_image_external : enable\n"
          "#extension GL_NV_EGL_stream_consumer_external : enable\n"
          "#define SamplerType samplerExternalOES\n"
          "#define TextureLookup texture2D\n";
      break;
    case SAMPLER_TYPE_NA:
      DCHECK_EQ(shader_string->find("SamplerType"), std::string::npos);
      DCHECK_EQ(shader_string->find("TextureLookup"), std::string::npos);
      return;
    default:
      NOTREACHED();
      return;
  }
  shader_string->insert(0, prefix);
}

}  // namespace

TexCoordPrecision TexCoordPrecisionRequired(GLES2Interface* context,
                                            int* highp_threshold_cache,
                                            int highp_threshold_min,
                                            const gfx::Point& max_coordinate) {
  return TexCoordPrecisionRequired(context, highp_threshold_cache,
                                   highp_threshold_min, max_coordinate.x(),
                                   max_coordinate.y());
}

TexCoordPrecision TexCoordPrecisionRequired(GLES2Interface* context,
                                            int* highp_threshold_cache,
                                            int highp_threshold_min,
                                            const gfx::Size& max_size) {
  return TexCoordPrecisionRequired(context, highp_threshold_cache,
                                   highp_threshold_min, max_size.width(),
                                   max_size.height());
}

VertexShader::VertexShader() {}

void VertexShader::Init(GLES2Interface* context,
                        unsigned program,
                        int* base_uniform_index) {
  std::vector<const char*> uniforms;
  std::vector<int> locations;

  switch (tex_coord_transform_) {
    case TEX_COORD_TRANSFORM_NONE:
      break;
    case TEX_COORD_TRANSFORM_VEC4:
    case TEX_COORD_TRANSFORM_TRANSLATED_VEC4:
      uniforms.push_back("vertexTexTransform");
      break;
    case TEX_COORD_TRANSFORM_MATRIX:
      uniforms.push_back("texMatrix");
      break;
  }
  if (is_ya_uv_) {
    uniforms.push_back("yaTexScale");
    uniforms.push_back("yaTexOffset");
    uniforms.push_back("uvTexScale");
    uniforms.push_back("uvTexOffset");
  }
  uniforms.push_back("matrix");
  if (has_vertex_opacity_)
    uniforms.push_back("opacity");
  if (aa_mode_ == USE_AA) {
    uniforms.push_back("viewport");
    uniforms.push_back("edge");
  }
  if (position_source_ == POSITION_SOURCE_ATTRIBUTE_INDEXED_UNIFORM)
    uniforms.push_back("quad");

  locations.resize(uniforms.size());

  GetProgramUniformLocations(context, program, uniforms.size(), uniforms.data(),
                             locations.data(), base_uniform_index);

  size_t index = 0;
  switch (tex_coord_transform_) {
    case TEX_COORD_TRANSFORM_NONE:
      break;
    case TEX_COORD_TRANSFORM_VEC4:
    case TEX_COORD_TRANSFORM_TRANSLATED_VEC4:
      vertex_tex_transform_location_ = locations[index++];
      break;
    case TEX_COORD_TRANSFORM_MATRIX:
      tex_matrix_location_ = locations[index++];
      break;
  }
  if (is_ya_uv_) {
    ya_tex_scale_location_ = locations[index++];
    ya_tex_offset_location_ = locations[index++];
    uv_tex_scale_location_ = locations[index++];
    uv_tex_offset_location_ = locations[index++];
  }
  matrix_location_ = locations[index++];
  if (has_vertex_opacity_)
    vertex_opacity_location_ = locations[index++];
  if (aa_mode_ == USE_AA) {
    viewport_location_ = locations[index++];
    edge_location_ = locations[index++];
  }
  if (position_source_ == POSITION_SOURCE_ATTRIBUTE_INDEXED_UNIFORM)
    quad_location_ = locations[index++];
}

std::string VertexShader::GetShaderString() const {
  // We unconditionally use highp in the vertex shader since
  // we are unlikely to be vertex shader bound when drawing large quads.
  // Also, some vertex shaders mutate the texture coordinate in such a
  // way that the effective precision might be lower than expected.
  std::string header = "#define TexCoordPrecision highp\n";
  std::string source = "void main() {\n";

  // Define the size of quads for attribute indexed uniform arrays.
  if (use_uniform_arrays_) {
    header += base::StringPrintf("#define NUM_QUADS %d\n",
                                 StaticGeometryBinding::NUM_QUADS);
  }

  // Read the index variables.
  if (use_uniform_arrays_ || has_vertex_opacity_ ||
      position_source_ == POSITION_SOURCE_ATTRIBUTE_INDEXED_UNIFORM) {
    HDR("attribute float a_index;");
    SRC("// Compute indices for uniform arrays.");
    SRC("int vertex_index = int(a_index);");
    if (use_uniform_arrays_)
      SRC("int quad_index = int(a_index * 0.25);");
    SRC("");
  }

  // Read the position and compute gl_Position.
  HDR("attribute TexCoordPrecision vec4 a_position;");
  SRC("// Compute the position.");
  switch (position_source_) {
    case POSITION_SOURCE_ATTRIBUTE:
      SRC("vec4 pos = a_position;");
      break;
    case POSITION_SOURCE_ATTRIBUTE_INDEXED_UNIFORM:
      HDR("uniform TexCoordPrecision vec2 quad[4];");
      SRC("vec4 pos = vec4(quad[vertex_index], a_position.z, a_position.w);");
      break;
  }
    if (use_uniform_arrays_) {
      HDR("uniform mat4 matrix[NUM_QUADS];");
      SRC("gl_Position = matrix[quad_index] * pos;");
    } else {
      HDR("uniform mat4 matrix;");
      SRC("gl_Position = matrix * pos;");
    }

  // Compute the anti-aliasing edge distances.
  if (aa_mode_ == USE_AA) {
    HDR("uniform TexCoordPrecision vec3 edge[8];");
    HDR("uniform vec4 viewport;");
    HDR("varying TexCoordPrecision vec4 edge_dist[2];  // 8 edge distances.");
    SRC("// Compute anti-aliasing properties.\n");
    SRC("vec2 ndc_pos = 0.5 * (1.0 + gl_Position.xy / gl_Position.w);");
    SRC("vec3 screen_pos = vec3(viewport.xy + viewport.zw * ndc_pos, 1.0);");
    SRC("edge_dist[0] = vec4(dot(edge[0], screen_pos),");
    SRC("                    dot(edge[1], screen_pos),");
    SRC("                    dot(edge[2], screen_pos),");
    SRC("                    dot(edge[3], screen_pos)) * gl_Position.w;");
    SRC("edge_dist[1] = vec4(dot(edge[4], screen_pos),");
    SRC("                    dot(edge[5], screen_pos),");
    SRC("                    dot(edge[6], screen_pos),");
    SRC("                    dot(edge[7], screen_pos)) * gl_Position.w;");
  }

  // Read, transform, and write texture coordinates.
  if (tex_coord_source_ != TEX_COORD_SOURCE_NONE) {
    if (is_ya_uv_) {
      HDR("varying TexCoordPrecision vec2 v_uvTexCoord;");
      HDR("varying TexCoordPrecision vec2 v_yaTexCoord;");
    } else {
      HDR("varying TexCoordPrecision vec2 v_texCoord;");
    }

    SRC("// Compute texture coordinates.");
    // Read coordinates.
    switch (tex_coord_source_) {
      case TEX_COORD_SOURCE_NONE:
        break;
      case TEX_COORD_SOURCE_POSITION:
        SRC("vec2 texCoord = pos.xy;");
        break;
      case TEX_COORD_SOURCE_ATTRIBUTE:
        HDR("attribute TexCoordPrecision vec2 a_texCoord;");
        SRC("vec2 texCoord = a_texCoord;");
        break;
    }
    // Transform coordinates (except YUV).
    switch (tex_coord_transform_) {
      case TEX_COORD_TRANSFORM_NONE:
        break;
      case TEX_COORD_TRANSFORM_TRANSLATED_VEC4:
        SRC("texCoord = texCoord + vec2(0.5);");
        FALLTHROUGH;
      case TEX_COORD_TRANSFORM_VEC4:
        if (use_uniform_arrays_) {
          HDR("uniform TexCoordPrecision vec4 vertexTexTransform[NUM_QUADS];");
          SRC("TexCoordPrecision vec4 texTrans =");
          SRC("    vertexTexTransform[quad_index];");
          SRC("texCoord = texCoord * texTrans.zw + texTrans.xy;");
        } else {
          HDR("uniform TexCoordPrecision vec4 vertexTexTransform;");
          SRC("texCoord = texCoord * vertexTexTransform.zw +");
          SRC("           vertexTexTransform.xy;");
        }
        break;
      case TEX_COORD_TRANSFORM_MATRIX:
        HDR("uniform TexCoordPrecision mat4 texMatrix;");
        SRC("texCoord = (texMatrix * vec4(texCoord.xy, 0.0, 1.0)).xy;");
        break;
    }
    // Write the output texture coordinates.
    if (is_ya_uv_) {
      HDR("uniform TexCoordPrecision vec2 uvTexOffset;");
      HDR("uniform TexCoordPrecision vec2 uvTexScale;");
      HDR("uniform TexCoordPrecision vec2 yaTexOffset;");
      HDR("uniform TexCoordPrecision vec2 yaTexScale;");
      SRC("v_yaTexCoord = texCoord * yaTexScale + yaTexOffset;");
      SRC("v_uvTexCoord = texCoord * uvTexScale + uvTexOffset;");
    } else {
      SRC("v_texCoord = texCoord;");
    }
  }

  // Write varying vertex opacity.
  if (has_vertex_opacity_) {
    HDR("varying float v_alpha;");
    if (use_uniform_arrays_) {
      HDR("uniform float opacity[NUM_QUADS * 4];");
    } else {
      HDR("uniform float opacity[4];");
    }
    SRC("v_alpha = opacity[vertex_index];");
  }

  // Add cargo-culted dummy variables for Android.
  if (has_dummy_variables_) {
    HDR("uniform TexCoordPrecision vec2 dummy_uniform;");
    HDR("varying TexCoordPrecision vec2 dummy_varying;");
    SRC("dummy_varying = dummy_uniform;");
  }

  source += "}\n";
  return header + source;
}

FragmentShader::FragmentShader() {}

std::string FragmentShader::GetShaderString() const {
  TexCoordPrecision precision = tex_coord_precision_;
  // The AA shader values will use TexCoordPrecision.
  if (aa_mode_ == USE_AA && precision == TEX_COORD_PRECISION_NA)
    precision = TEX_COORD_PRECISION_MEDIUM;
  std::string shader = GetShaderSource();
  SetBlendModeFunctions(&shader);
  SetRoundedCornerFunctions(&shader);
  SetFragmentSamplerType(sampler_type_, &shader);
  SetFragmentTexCoordPrecision(precision, &shader);
  return shader;
}

void FragmentShader::Init(GLES2Interface* context,
                          unsigned program,
                          int* base_uniform_index) {
  std::vector<const char*> uniforms;
  std::vector<int> locations;
  if (has_blend_mode()) {
    uniforms.push_back("s_backdropTexture");
    uniforms.push_back("s_originalBackdropTexture");
    uniforms.push_back("backdropRect");
  }
  if (mask_mode_ != NO_MASK) {
    uniforms.push_back("s_mask");
    uniforms.push_back("maskTexCoordScale");
    uniforms.push_back("maskTexCoordOffset");
  }
  if (has_color_matrix_) {
    uniforms.push_back("colorMatrix");
    uniforms.push_back("colorOffset");
  }
  if (has_uniform_alpha_)
    uniforms.push_back("alpha");
  if (has_background_color_)
    uniforms.push_back("background_color");
  if (has_tex_clamp_rect_)
    uniforms.push_back("tex_clamp_rect");
  switch (input_color_type_) {
    case INPUT_COLOR_SOURCE_RGBA_TEXTURE:
      uniforms.push_back("s_texture");
      if (has_rgba_fragment_tex_transform_)
        uniforms.push_back("fragmentTexTransform");
      break;
    case INPUT_COLOR_SOURCE_YUV_TEXTURES:
      uniforms.push_back("y_texture");
      if (uv_texture_mode_ == UV_TEXTURE_MODE_UV)
        uniforms.push_back("uv_texture");
      if (uv_texture_mode_ == UV_TEXTURE_MODE_U_V) {
        uniforms.push_back("u_texture");
        uniforms.push_back("v_texture");
      }
      if (yuv_alpha_texture_mode_ == YUV_HAS_ALPHA_TEXTURE)
        uniforms.push_back("a_texture");
      uniforms.push_back("ya_clamp_rect");
      uniforms.push_back("uv_clamp_rect");
      uniforms.push_back("resource_multiplier");
      uniforms.push_back("resource_offset");
      break;
    case INPUT_COLOR_SOURCE_UNIFORM:
      uniforms.push_back("color");
      break;
  }
  if (color_conversion_mode_ == COLOR_CONVERSION_MODE_LUT) {
    uniforms.push_back("lut_texture");
    uniforms.push_back("lut_size");
  }
  if (has_output_color_matrix_)
    uniforms.emplace_back("output_color_matrix");

  if (has_tint_color_matrix_)
    uniforms.emplace_back("tint_color_matrix");

  if (has_rounded_corner_) {
    uniforms.emplace_back("roundedCornerRect");
    uniforms.emplace_back("roundedCornerRadius");
  }

  locations.resize(uniforms.size());

  GetProgramUniformLocations(context, program, uniforms.size(), uniforms.data(),
                             locations.data(), base_uniform_index);

  size_t index = 0;
  if (has_blend_mode()) {
    backdrop_location_ = locations[index++];
    original_backdrop_location_ = locations[index++];
    backdrop_rect_location_ = locations[index++];
  }
  if (mask_mode_ != NO_MASK) {
    mask_sampler_location_ = locations[index++];
    mask_tex_coord_scale_location_ = locations[index++];
    mask_tex_coord_offset_location_ = locations[index++];
  }
  if (has_color_matrix_) {
    color_matrix_location_ = locations[index++];
    color_offset_location_ = locations[index++];
  }
  if (has_uniform_alpha_)
    alpha_location_ = locations[index++];
  if (has_background_color_)
    background_color_location_ = locations[index++];
  if (has_tex_clamp_rect_)
    tex_clamp_rect_location_ = locations[index++];
  switch (input_color_type_) {
    case INPUT_COLOR_SOURCE_RGBA_TEXTURE:
      sampler_location_ = locations[index++];
      if (has_rgba_fragment_tex_transform_)
        fragment_tex_transform_location_ = locations[index++];
      break;
    case INPUT_COLOR_SOURCE_YUV_TEXTURES:
      y_texture_location_ = locations[index++];
      if (uv_texture_mode_ == UV_TEXTURE_MODE_UV)
        uv_texture_location_ = locations[index++];
      if (uv_texture_mode_ == UV_TEXTURE_MODE_U_V) {
        u_texture_location_ = locations[index++];
        v_texture_location_ = locations[index++];
      }
      if (yuv_alpha_texture_mode_ == YUV_HAS_ALPHA_TEXTURE)
        a_texture_location_ = locations[index++];
      ya_clamp_rect_location_ = locations[index++];
      uv_clamp_rect_location_ = locations[index++];
      resource_multiplier_location_ = locations[index++];
      resource_offset_location_ = locations[index++];
      break;
    case INPUT_COLOR_SOURCE_UNIFORM:
      color_location_ = locations[index++];
      break;
  }
  if (color_conversion_mode_ == COLOR_CONVERSION_MODE_LUT) {
    lut_texture_location_ = locations[index++];
    lut_size_location_ = locations[index++];
  }

  if (has_output_color_matrix_)
    output_color_matrix_location_ = locations[index++];

  if (has_tint_color_matrix_)
    tint_color_matrix_location_ = locations[index++];

  if (has_rounded_corner_) {
    rounded_corner_rect_location_ = locations[index++];
    rounded_corner_radius_location_ = locations[index++];
  }

  DCHECK_EQ(index, locations.size());
}

void FragmentShader::SetRoundedCornerFunctions(
    std::string* shader_string) const {
  if (!has_rounded_corner_)
    return;

  static constexpr base::StringPiece kUniforms = SHADER0([]() {
    uniform vec4 roundedCornerRect;
    uniform vec4 roundedCornerRadius;
  });

  static constexpr base::StringPiece kFunctionRcUtility = SHADER0([]() {
    // Returns a vector of size 4. Each component of a vector is set to 1 or 0
    // representing whether |rcCoord| is a part of the respective corner or
    // not.
    // The component ordering is:
    //     [Top left, Top right, Bottom right, Bottom left]
    vec4 IsCorner(vec2 rcCoord) {
      // Top left corner
      if (rcCoord.x < roundedCornerRadius.x &&
          rcCoord.y < roundedCornerRadius.x) {
        return vec4(1.0, 0.0, 0.0, 0.0);
      }

      // Top right corner
      if (rcCoord.x > roundedCornerRect.z - roundedCornerRadius.y &&
          rcCoord.y < roundedCornerRadius.y) {
        return vec4(0.0, 1.0, 0.0, 0.0);
      }

      // Bottom right corner
      if (rcCoord.x > roundedCornerRect.z - roundedCornerRadius.z &&
          rcCoord.y > roundedCornerRect.w - roundedCornerRadius.z) {
        return vec4(0.0, 0.0, 1.0, 0.0);
      }

      // Bottom left corner
      if (rcCoord.x < roundedCornerRadius.w &&
          rcCoord.y > roundedCornerRect.w - roundedCornerRadius.w) {
        return vec4(0.0, 0.0, 0.0, 1.0);
      }
      return vec4(0.0, 0.0, 0.0, 0.0);
    }

    // Returns the center of the rounded corner. |corner| holds the info on
    // which corner the center is requested for.
    vec2 GetCenter(vec4 corner, float radius) {
      if (corner.x == 1.0) {
        // Top left corner
        return vec2(radius, radius);
      } else if (corner.y == 1.0) {
        // Top right corner
        return vec2(roundedCornerRect.z - radius, radius);
      } else if (corner.z == 1.0) {
        // Bottom right corner
        return vec2(roundedCornerRect.z - radius, roundedCornerRect.w - radius);
      } else {
        // Bottom left corner
        return vec2(radius, roundedCornerRect.w - radius);
      }
    }
  });

  static constexpr base::StringPiece kFunctionApplyRoundedCorner =
      SHADER0([]() {
        vec4 ApplyRoundedCorner(vec4 src) {
          vec2 rcCoord = gl_FragCoord.xy - roundedCornerRect.xy;

          vec4 isCorner = IsCorner(rcCoord);

          // Get the radius to use based on the corner this fragment lies in.
          float r = dot(isCorner, roundedCornerRadius);

          // If the radius is 0, then there is no rounded corner here. We can do
          // an early return.
          if (r == 0.0)
            return src;

          // Vector to the corner's center this frag is in.
          vec2 cornerCenter = GetCenter(isCorner, r);

          // Vector from the center of the corner to the current fragment center
          vec2 cxy = rcCoord - cornerCenter;

          // Compute the distance of the fragment's center from the corner's
          // center.
          float fragDst = length(cxy);

          float alpha = smoothstep(r - 1.0, r + 1.0, fragDst);
          return vec4(0.0) * alpha + src * (1.0 - alpha);
        }
      });

  std::string shader;
  shader.reserve(shader_string->size() + 2048);
  shader += "precision mediump float;";
  kUniforms.AppendToString(&shader);
  kFunctionRcUtility.AppendToString(&shader);
  kFunctionApplyRoundedCorner.AppendToString(&shader);
  shader += *shader_string;
  *shader_string = std::move(shader);
}

void FragmentShader::SetBlendModeFunctions(std::string* shader_string) const {
  if (!has_blend_mode()) {
    return;
  }

  static constexpr base::StringPiece kUniforms = SHADER0([]() {
    uniform sampler2D s_backdropTexture;
    uniform sampler2D s_originalBackdropTexture;
    uniform TexCoordPrecision vec4 backdropRect;
  });

  base::StringPiece function_apply_blend_mode;
  if (mask_for_background_) {
    static constexpr base::StringPiece kFunctionApplyBlendMode = SHADER0([]() {
      vec4 ApplyBlendMode(vec4 src, float mask) {
        TexCoordPrecision vec2 bgTexCoord = gl_FragCoord.xy - backdropRect.xy;
        bgTexCoord *= backdropRect.zw;
        vec4 backdrop = texture2D(s_backdropTexture, bgTexCoord);
        vec4 original_backdrop =
            texture2D(s_originalBackdropTexture, bgTexCoord);
        vec4 dst = mix(original_backdrop, backdrop, mask);
        return Blend(src, dst);
      }
    });
    function_apply_blend_mode = kFunctionApplyBlendMode;
  } else {
    static constexpr base::StringPiece kFunctionApplyBlendMode = SHADER0([]() {
      vec4 ApplyBlendMode(vec4 src) {
        TexCoordPrecision vec2 bgTexCoord = gl_FragCoord.xy - backdropRect.xy;
        bgTexCoord *= backdropRect.zw;
        vec4 dst = texture2D(s_backdropTexture, bgTexCoord);
        return Blend(src, dst);
      }
    });
    function_apply_blend_mode = kFunctionApplyBlendMode;
  }

  std::string shader;
  shader.reserve(shader_string->size() + 1024);
  shader += "precision mediump float;";
  AppendHelperFunctions(&shader);
  AppendBlendFunction(&shader);
  kUniforms.AppendToString(&shader);
  function_apply_blend_mode.AppendToString(&shader);
  shader += *shader_string;
  *shader_string = std::move(shader);
}

void FragmentShader::AppendHelperFunctions(std::string* buffer) const {
  static constexpr base::StringPiece kFunctionHardLight = SHADER0([]() {
    vec3 hardLight(vec4 src, vec4 dst) {
      vec3 result;
      result.r =
          (2.0 * src.r <= src.a)
              ? (2.0 * src.r * dst.r)
              : (src.a * dst.a - 2.0 * (dst.a - dst.r) * (src.a - src.r));
      result.g =
          (2.0 * src.g <= src.a)
              ? (2.0 * src.g * dst.g)
              : (src.a * dst.a - 2.0 * (dst.a - dst.g) * (src.a - src.g));
      result.b =
          (2.0 * src.b <= src.a)
              ? (2.0 * src.b * dst.b)
              : (src.a * dst.a - 2.0 * (dst.a - dst.b) * (src.a - src.b));
      result.rgb += src.rgb * (1.0 - dst.a) + dst.rgb * (1.0 - src.a);
      return result;
    }
  });

  static constexpr base::StringPiece kFunctionColorDodgeComponent =
      SHADER0([]() {
        float getColorDodgeComponent(float srcc, float srca, float dstc,
                                     float dsta) {
          if (0.0 == dstc)
            return srcc * (1.0 - dsta);
          float d = srca - srcc;
          if (0.0 == d)
            return srca * dsta + srcc * (1.0 - dsta) + dstc * (1.0 - srca);
          d = min(dsta, dstc * srca / d);
          return d * srca + srcc * (1.0 - dsta) + dstc * (1.0 - srca);
        }
      });

  static constexpr base::StringPiece kFunctionColorBurnComponent =
      SHADER0([]() {
        float getColorBurnComponent(float srcc, float srca, float dstc,
                                    float dsta) {
          if (dsta == dstc)
            return srca * dsta + srcc * (1.0 - dsta) + dstc * (1.0 - srca);
          if (0.0 == srcc)
            return dstc * (1.0 - srca);
          float d = max(0.0, dsta - (dsta - dstc) * srca / srcc);
          return srca * d + srcc * (1.0 - dsta) + dstc * (1.0 - srca);
        }
      });

  static constexpr base::StringPiece kFunctionSoftLightComponentPosDstAlpha =
      SHADER0([]() {
        float getSoftLightComponent(float srcc, float srca, float dstc,
                                    float dsta) {
          if (2.0 * srcc <= srca) {
            return (dstc * dstc * (srca - 2.0 * srcc)) / dsta +
                   (1.0 - dsta) * srcc + dstc * (-srca + 2.0 * srcc + 1.0);
          } else if (4.0 * dstc <= dsta) {
            float DSqd = dstc * dstc;
            float DCub = DSqd * dstc;
            float DaSqd = dsta * dsta;
            float DaCub = DaSqd * dsta;
            return (-DaCub * srcc +
                    DaSqd * (srcc - dstc * (3.0 * srca - 6.0 * srcc - 1.0)) +
                    12.0 * dsta * DSqd * (srca - 2.0 * srcc) -
                    16.0 * DCub * (srca - 2.0 * srcc)) /
                   DaSqd;
          } else {
            return -sqrt(dsta * dstc) * (srca - 2.0 * srcc) - dsta * srcc +
                   dstc * (srca - 2.0 * srcc + 1.0) + srcc;
          }
        }
      });

  static constexpr base::StringPiece kFunctionLum = SHADER0([]() {
    float luminance(vec3 color) { return dot(vec3(0.3, 0.59, 0.11), color); }

    vec3 set_luminance(vec3 hueSat, float alpha, vec3 lumColor) {
      float diff = luminance(lumColor - hueSat);
      vec3 outColor = hueSat + diff;
      float outLum = luminance(outColor);
      float minComp = min(min(outColor.r, outColor.g), outColor.b);
      float maxComp = max(max(outColor.r, outColor.g), outColor.b);
      if (minComp < 0.0 && outLum != minComp) {
        outColor =
            outLum + ((outColor - vec3(outLum, outLum, outLum)) * outLum) /
                         (outLum - minComp);
      }
      if (maxComp > alpha && maxComp != outLum) {
        outColor = outLum + ((outColor - vec3(outLum, outLum, outLum)) *
                             (alpha - outLum)) /
                                (maxComp - outLum);
      }
      return outColor;
    }
  });

  static constexpr base::StringPiece kFunctionSat = SHADER0([]() {
    float saturation(vec3 color) {
      return max(max(color.r, color.g), color.b) -
             min(min(color.r, color.g), color.b);
    }

    vec3 set_saturation_helper(float minComp, float midComp, float maxComp,
                               float sat) {
      if (minComp < maxComp) {
        vec3 result;
        result.r = 0.0;
        result.g = sat * (midComp - minComp) / (maxComp - minComp);
        result.b = sat;
        return result;
      } else {
        return vec3(0, 0, 0);
      }
    }

    vec3 set_saturation(vec3 hueLumColor, vec3 satColor) {
      float sat = saturation(satColor);
      if (hueLumColor.r <= hueLumColor.g) {
        if (hueLumColor.g <= hueLumColor.b) {
          hueLumColor.rgb = set_saturation_helper(hueLumColor.r, hueLumColor.g,
                                                  hueLumColor.b, sat);
        } else if (hueLumColor.r <= hueLumColor.b) {
          hueLumColor.rbg = set_saturation_helper(hueLumColor.r, hueLumColor.b,
                                                  hueLumColor.g, sat);
        } else {
          hueLumColor.brg = set_saturation_helper(hueLumColor.b, hueLumColor.r,
                                                  hueLumColor.g, sat);
        }
      } else if (hueLumColor.r <= hueLumColor.b) {
        hueLumColor.grb = set_saturation_helper(hueLumColor.g, hueLumColor.r,
                                                hueLumColor.b, sat);
      } else if (hueLumColor.g <= hueLumColor.b) {
        hueLumColor.gbr = set_saturation_helper(hueLumColor.g, hueLumColor.b,
                                                hueLumColor.r, sat);
      } else {
        hueLumColor.bgr = set_saturation_helper(hueLumColor.b, hueLumColor.g,
                                                hueLumColor.r, sat);
      }
      return hueLumColor;
    }
  });

  switch (blend_mode_) {
    case BLEND_MODE_OVERLAY:
    case BLEND_MODE_HARD_LIGHT:
      kFunctionHardLight.AppendToString(buffer);
      return;
    case BLEND_MODE_COLOR_DODGE:
      kFunctionColorDodgeComponent.AppendToString(buffer);
      return;
    case BLEND_MODE_COLOR_BURN:
      kFunctionColorBurnComponent.AppendToString(buffer);
      return;
    case BLEND_MODE_SOFT_LIGHT:
      kFunctionSoftLightComponentPosDstAlpha.AppendToString(buffer);
      return;
    case BLEND_MODE_HUE:
    case BLEND_MODE_SATURATION:
      kFunctionLum.AppendToString(buffer);
      kFunctionSat.AppendToString(buffer);
      return;
    case BLEND_MODE_COLOR:
    case BLEND_MODE_LUMINOSITY:
      kFunctionLum.AppendToString(buffer);
      return;
    default:
      return;
  }
}

void FragmentShader::AppendBlendFunction(std::string* buffer) const {
  *buffer +=
      "vec4 Blend(vec4 src, vec4 dst) {"
      "    vec4 result;";
  GetBlendFunctionBodyForAlpha().AppendToString(buffer);
  GetBlendFunctionBodyForRGB().AppendToString(buffer);
  *buffer +=
      "    return result;"
      "}";
}

base::StringPiece FragmentShader::GetBlendFunctionBodyForAlpha() const {
  if (blend_mode_ == BLEND_MODE_DESTINATION_IN)
    return "result.a = src.a * dst.a;";
  else
    return "result.a = src.a + (1.0 - src.a) * dst.a;";
}

base::StringPiece FragmentShader::GetBlendFunctionBodyForRGB() const {
  switch (blend_mode_) {
    case BLEND_MODE_NORMAL:
      return "result.rgb = src.rgb + dst.rgb * (1.0 - src.a);";
    case BLEND_MODE_DESTINATION_IN:
      return "result.rgb = dst.rgb * src.a;";
    case BLEND_MODE_SCREEN:
      return "result.rgb = src.rgb + (1.0 - src.rgb) * dst.rgb;";
    case BLEND_MODE_LIGHTEN:
      return "result.rgb = max((1.0 - src.a) * dst.rgb + src.rgb,"
             "                 (1.0 - dst.a) * src.rgb + dst.rgb);";
    case BLEND_MODE_OVERLAY:
      return "result.rgb = hardLight(dst, src);";
    case BLEND_MODE_DARKEN:
      return "result.rgb = min((1.0 - src.a) * dst.rgb + src.rgb,"
             "                 (1.0 - dst.a) * src.rgb + dst.rgb);";
    case BLEND_MODE_COLOR_DODGE:
      return "result.r = getColorDodgeComponent(src.r, src.a, dst.r, dst.a);"
             "result.g = getColorDodgeComponent(src.g, src.a, dst.g, dst.a);"
             "result.b = getColorDodgeComponent(src.b, src.a, dst.b, dst.a);";
    case BLEND_MODE_COLOR_BURN:
      return "result.r = getColorBurnComponent(src.r, src.a, dst.r, dst.a);"
             "result.g = getColorBurnComponent(src.g, src.a, dst.g, dst.a);"
             "result.b = getColorBurnComponent(src.b, src.a, dst.b, dst.a);";
    case BLEND_MODE_HARD_LIGHT:
      return "result.rgb = hardLight(src, dst);";
    case BLEND_MODE_SOFT_LIGHT:
      return "if (0.0 == dst.a) {"
             "  result.rgb = src.rgb;"
             "} else {"
             "  result.r = getSoftLightComponent(src.r, src.a, dst.r, dst.a);"
             "  result.g = getSoftLightComponent(src.g, src.a, dst.g, dst.a);"
             "  result.b = getSoftLightComponent(src.b, src.a, dst.b, dst.a);"
             "}";
    case BLEND_MODE_DIFFERENCE:
      return "result.rgb = src.rgb + dst.rgb -"
             "    2.0 * min(src.rgb * dst.a, dst.rgb * src.a);";
    case BLEND_MODE_EXCLUSION:
      return "result.rgb = dst.rgb + src.rgb - 2.0 * dst.rgb * src.rgb;";
    case BLEND_MODE_MULTIPLY:
      return "result.rgb = (1.0 - src.a) * dst.rgb +"
             "    (1.0 - dst.a) * src.rgb + src.rgb * dst.rgb;";
    case BLEND_MODE_HUE:
      return "vec4 dstSrcAlpha = dst * src.a;"
             "result.rgb ="
             "    set_luminance(set_saturation(src.rgb * dst.a,"
             "                                 dstSrcAlpha.rgb),"
             "                  dstSrcAlpha.a,"
             "                  dstSrcAlpha.rgb);"
             "result.rgb += (1.0 - src.a) * dst.rgb + (1.0 - dst.a) * src.rgb;";
    case BLEND_MODE_SATURATION:
      return "vec4 dstSrcAlpha = dst * src.a;"
             "result.rgb = set_luminance(set_saturation(dstSrcAlpha.rgb,"
             "                                          src.rgb * dst.a),"
             "                           dstSrcAlpha.a,"
             "                           dstSrcAlpha.rgb);"
             "result.rgb += (1.0 - src.a) * dst.rgb + (1.0 - dst.a) * src.rgb;";
    case BLEND_MODE_COLOR:
      return "vec4 srcDstAlpha = src * dst.a;"
             "result.rgb = set_luminance(srcDstAlpha.rgb,"
             "                           srcDstAlpha.a,"
             "                           dst.rgb * src.a);"
             "result.rgb += (1.0 - src.a) * dst.rgb + (1.0 - dst.a) * src.rgb;";
    case BLEND_MODE_LUMINOSITY:
      return "vec4 srcDstAlpha = src * dst.a;"
             "result.rgb = set_luminance(dst.rgb * src.a,"
             "                           srcDstAlpha.a,"
             "                           srcDstAlpha.rgb);"
             "result.rgb += (1.0 - src.a) * dst.rgb + (1.0 - dst.a) * src.rgb;";
    case BLEND_MODE_NONE:
      NOTREACHED();
  }
  return "result = vec4(1.0, 0.0, 0.0, 1.0);";
}

std::string FragmentShader::GetShaderSource() const {
  std::string header = "precision mediump float;\n";
  std::string source = "void main() {\n";

  // Read the input into vec4 texColor.
  switch (input_color_type_) {
    case INPUT_COLOR_SOURCE_RGBA_TEXTURE:
      if (ignore_sampler_type_)
        HDR("uniform sampler2D s_texture;");
      else
        HDR("uniform SamplerType s_texture;");
      HDR("varying TexCoordPrecision vec2 v_texCoord;");
      if (has_rgba_fragment_tex_transform_) {
        HDR("uniform TexCoordPrecision vec4 fragmentTexTransform;");
        SRC("// Transformed texture lookup");
        SRC("TexCoordPrecision vec2 texCoord =");
        SRC("    clamp(v_texCoord, 0.0, 1.0) * fragmentTexTransform.zw +");
        SRC("   fragmentTexTransform.xy;");
        SRC("vec4 texColor = TextureLookup(s_texture, texCoord);");
        DCHECK(!ignore_sampler_type_);
        DCHECK(!has_tex_clamp_rect_);
      } else {
        SRC("// Texture lookup");
        if (ignore_sampler_type_) {
          SRC("vec4 texColor = texture2D(s_texture, v_texCoord);");
          DCHECK(!has_tex_clamp_rect_);
        } else {
          SRC("TexCoordPrecision vec2 texCoord = v_texCoord;");
          if (has_tex_clamp_rect_) {
            HDR("uniform vec4 tex_clamp_rect;");
            SRC("texCoord = max(tex_clamp_rect.xy,");
            SRC("    min(tex_clamp_rect.zw, texCoord));");
          }
          SRC("vec4 texColor = TextureLookup(s_texture, texCoord);");
        }
      }
      break;
    case INPUT_COLOR_SOURCE_YUV_TEXTURES:
      DCHECK(!has_tex_clamp_rect_);
      // Compute the clamped texture coordinates for the YA and UV textures.
      HDR("uniform SamplerType y_texture;");
      SRC("// YUV texture lookup and conversion to RGB.");
      SRC("vec2 ya_clamped =");
      SRC("    max(ya_clamp_rect.xy, min(ya_clamp_rect.zw, v_yaTexCoord));");
      SRC("vec2 uv_clamped =");
      SRC("    max(uv_clamp_rect.xy, min(uv_clamp_rect.zw, v_uvTexCoord));");
      // Read the Y and UV or U and V textures into |yuv|.
      SRC("vec4 texColor;");
      SRC("texColor.w = 1.0;");
      SRC("texColor.x = TextureLookup(y_texture, ya_clamped).x;");
      if (uv_texture_mode_ == UV_TEXTURE_MODE_UV) {
        HDR("uniform SamplerType uv_texture;");
        SRC("texColor.yz = TextureLookup(uv_texture, uv_clamped).xy;");
      }
      if (uv_texture_mode_ == UV_TEXTURE_MODE_U_V) {
        HDR("uniform SamplerType u_texture;");
        HDR("uniform SamplerType v_texture;");
        SRC("texColor.y = TextureLookup(u_texture, uv_clamped).x;");
        SRC("texColor.z = TextureLookup(v_texture, uv_clamped).x;");
      }
      if (yuv_alpha_texture_mode_ == YUV_HAS_ALPHA_TEXTURE)
        HDR("uniform SamplerType a_texture;");
      HDR("uniform vec4 ya_clamp_rect;");
      HDR("uniform vec4 uv_clamp_rect;");
      HDR("uniform float resource_multiplier;");
      HDR("uniform float resource_offset;");
      HDR("varying TexCoordPrecision vec2 v_yaTexCoord;");
      HDR("varying TexCoordPrecision vec2 v_uvTexCoord;");
      SRC("texColor.xyz -= vec3(resource_offset);");
      SRC("texColor.xyz *= resource_multiplier;");
      break;
    case INPUT_COLOR_SOURCE_UNIFORM:
      DCHECK(!ignore_sampler_type_);
      DCHECK(!has_rgba_fragment_tex_transform_);
      DCHECK(!has_tex_clamp_rect_);
      HDR("uniform vec4 color;");
      SRC("// Uniform color");
      SRC("vec4 texColor = color;");
      break;
  }

  // Apply color conversion.
  switch (color_conversion_mode_) {
    case COLOR_CONVERSION_MODE_LUT:
      HDR("uniform sampler2D lut_texture;");
      HDR("uniform float lut_size;");
      HDR("vec4 LUT(sampler2D sampler, vec3 pos, float size) {");
      HDR("  pos *= size - 1.0;");
      HDR("  // Select layer");
      HDR("  float layer = min(floor(pos.z), size - 2.0);");
      HDR("  // Compress the xy coordinates so they stay within");
      HDR("  // [0.5 .. 31.5] / N (assuming a LUT size of 17^3)");
      HDR("  pos.xy = (pos.xy + vec2(0.5)) / size;");
      HDR("  pos.y = (pos.y + layer) / size;");
      HDR("  return mix(LutLookup(sampler, pos.xy),");
      HDR("             LutLookup(sampler, pos.xy + vec2(0, 1.0 / size)),");
      HDR("             pos.z - layer);");
      HDR("}");
      // Un-premultiply by alpha.
      if (premultiply_alpha_mode_ != NON_PREMULTIPLIED_ALPHA) {
        SRC("// un-premultiply alpha");
        SRC("if (texColor.a > 0.0) texColor.rgb /= texColor.a;");
      }
      SRC("texColor.rgb = LUT(lut_texture, texColor.xyz, lut_size).xyz;");
      SRC("texColor.rgb *= texColor.a;");
      break;
    case COLOR_CONVERSION_MODE_SHADER:
      header += color_transform_->GetShaderSource();
      // Un-premultiply by alpha.
      if (premultiply_alpha_mode_ != NON_PREMULTIPLIED_ALPHA) {
        SRC("// un-premultiply alpha");
        SRC("if (texColor.a > 0.0) texColor.rgb /= texColor.a;");
      }
      SRC("texColor.rgb = DoColorConversion(texColor.xyz);");
      SRC("texColor.rgb *= texColor.a;");
      break;
    case COLOR_CONVERSION_MODE_NONE:
      // Premultiply by alpha.
      if (premultiply_alpha_mode_ == NON_PREMULTIPLIED_ALPHA) {
        SRC("// Premultiply alpha");
        SRC("texColor.rgb *= texColor.a;");
      }
      break;
  }

  // Apply the color matrix to texColor.
  if (has_color_matrix_) {
    HDR("uniform mat4 colorMatrix;");
    HDR("uniform vec4 colorOffset;");
    SRC("// Apply color matrix");
    SRC("float nonZeroAlpha = max(texColor.a, 0.00001);");
    SRC("texColor = vec4(texColor.rgb / nonZeroAlpha, nonZeroAlpha);");
    SRC("texColor = colorMatrix * texColor + colorOffset;");
    SRC("texColor.rgb *= texColor.a;");
    SRC("texColor = clamp(texColor, 0.0, 1.0);");
  }

  // Read the mask texture.
  if (mask_mode_ != NO_MASK) {
    HDR("uniform SamplerType s_mask;");
    HDR("uniform vec2 maskTexCoordScale;");
    HDR("uniform vec2 maskTexCoordOffset;");
    SRC("// Read the mask");
    SRC("TexCoordPrecision vec2 maskTexCoord =");
    SRC("    vec2(maskTexCoordOffset.x + v_texCoord.x * maskTexCoordScale.x,");
    SRC("         maskTexCoordOffset.y + v_texCoord.y * maskTexCoordScale.y);");
    SRC("vec4 maskColor = TextureLookup(s_mask, maskTexCoord);");
  }

  // Compute AA.
  if (aa_mode_ == USE_AA) {
    HDR("varying TexCoordPrecision vec4 edge_dist[2];  // 8 edge distances.");
    SRC("// Compute AA");
    SRC("vec4 d4 = min(edge_dist[0], edge_dist[1]);");
    SRC("vec2 d2 = min(d4.xz, d4.yw);");
    SRC("float aa = clamp(gl_FragCoord.w * min(d2.x, d2.y), 0.0, 1.0);");
  }

  // Apply background texture.
  if (has_background_color_) {
    HDR("uniform vec4 background_color;");
    SRC("// Apply uniform background color blending");
    SRC("texColor += background_color * (1.0 - texColor.a);");
  }

  // Finally apply the output color matrix to texColor.
  if (has_output_color_matrix_) {
    HDR("uniform mat4 output_color_matrix;");
    SRC("// Apply the output color matrix");
    SRC("texColor = output_color_matrix * texColor;");
  }

  // Tint the final color. Used for debugging composited content.
  if (has_tint_color_matrix_) {
    HDR("uniform mat4 tint_color_matrix;");
    SRC("// Apply the tint color matrix");
    SRC("texColor = tint_color_matrix * texColor;");
  }

  // Include header text for alpha.
  if (has_uniform_alpha_) {
    HDR("uniform float alpha;");
  }
  if (has_varying_alpha_) {
    HDR("varying float v_alpha;");
  }

  // Apply uniform alpha, aa, varying alpha, and the mask.
  if (has_varying_alpha_ || aa_mode_ == USE_AA || has_uniform_alpha_ ||
      mask_mode_ != NO_MASK) {
    SRC("// Apply alpha from uniform, varying, aa, and mask.");
    std::string line = "  texColor = texColor";
    if (has_varying_alpha_)
      line += " * v_alpha";
    if (has_uniform_alpha_)
      line += " * alpha";
    if (aa_mode_ == USE_AA)
      line += " * aa";
    if (mask_mode_ != NO_MASK)
      line += " * maskColor.a";
    if (yuv_alpha_texture_mode_ == YUV_HAS_ALPHA_TEXTURE)
      line += " * TextureLookup(a_texture, ya_clamped).x";
    line += ";\n";
    source += line;
  }

  // Write the fragment color.
  SRC("// Write the fragment color");
  switch (frag_color_mode_) {
    case FRAG_COLOR_MODE_DEFAULT:
      DCHECK_EQ(blend_mode_, BLEND_MODE_NONE);
      SRC("gl_FragColor = texColor;");
      break;
    case FRAG_COLOR_MODE_OPAQUE:
      DCHECK_EQ(blend_mode_, BLEND_MODE_NONE);
      SRC("gl_FragColor = vec4(texColor.rgb, 1.0);");
      break;
    case FRAG_COLOR_MODE_APPLY_BLEND_MODE:
      if (!has_blend_mode()) {
        SRC("gl_FragColor = texColor;");
      } else if (mask_mode_ != NO_MASK) {
        if (mask_for_background_)
          SRC("gl_FragColor = ApplyBlendMode(texColor, maskColor.w);");
        else
          SRC("gl_FragColor = ApplyBlendMode(texColor);");
      } else {
        SRC("gl_FragColor = ApplyBlendMode(texColor);");
      }
      break;
  }

  if (has_rounded_corner_)
    SRC("gl_FragColor = ApplyRoundedCorner(gl_FragColor);");

  source += "}\n";

  return header + source;
}

}  // namespace viz
