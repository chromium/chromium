// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gl_scaler.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <string>

#include "base/logging.h"
#include "components/viz/common/gpu/context_provider.h"
#include "gpu/GLES2/gl2chromium.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "ui/gfx/color_transform.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {

namespace {

// The code in GLScaler that computes the ScalerStages is greatly simplified by
// being able to access the X and Y components by index (instead of
// Vector2d::x() or Vector2d::y()). Thus, define a helper class to represent the
// relative size as a 2-element std::array and convert to/from Vector2d.
struct RelativeSize : public std::array<int, 2> {
  using std::array<int, 2>::operator[];

  RelativeSize(int width, int height) : std::array<int, 2>{{width, height}} {}
  explicit RelativeSize(const gfx::Vector2d& v)
      : std::array<int, 2>{{v.x(), v.y()}} {}

  gfx::Vector2d AsVector2d() const {
    return gfx::Vector2d((*this)[0], (*this)[1]);
  }
};

std::ostream& operator<<(std::ostream& out, const RelativeSize& size) {
  return (out << size[0] << 'x' << size[1]);
}

}  // namespace

GLScaler::GLScaler(ContextProvider* context_provider)
    : context_provider_(context_provider) {
  if (context_provider_) {
    DCHECK(context_provider_->ContextGL());
    context_provider_->AddObserver(this);
  }
}

GLScaler::~GLScaler() {
  OnContextLost();  // Ensures destruction in dependency order.
}

bool GLScaler::SupportsPreciseColorManagement() const {
  if (!context_provider_) {
    return false;
  }
  if (!supports_half_floats_.has_value()) {
    supports_half_floats_ = AreAllGLExtensionsPresent(
        context_provider_->ContextGL(),
        {"GL_EXT_color_buffer_half_float", "GL_OES_texture_half_float_linear"});
  }
  return supports_half_floats_.value();
}

int GLScaler::GetMaxDrawBuffersSupported() const {
  if (!context_provider_) {
    return 0;
  }

  if (max_draw_buffers_ < 0) {
    // Query the GL context for the multiple draw buffers extension and, if
    // present, the actual platform-supported maximum.
    GLES2Interface* const gl = context_provider_->ContextGL();
    DCHECK(gl);
    if (AreAllGLExtensionsPresent(gl, {"GL_EXT_draw_buffers"})) {
      gl->GetIntegerv(GL_MAX_DRAW_BUFFERS_EXT, &max_draw_buffers_);
    }

    if (max_draw_buffers_ < 1) {
      max_draw_buffers_ = 1;
    }
  }

  return max_draw_buffers_;
}

bool GLScaler::Configure(const Parameters& new_params) {
  chain_.reset();
  shader_programs_.clear();

  if (!context_provider_) {
    return false;
  }
  GLES2Interface* const gl = context_provider_->ContextGL();
  DCHECK(gl);

  params_ = new_params;

  // Ensure the client has provided valid scaling vectors.
  if (params_.scale_from.x() == 0 || params_.scale_from.y() == 0 ||
      params_.scale_to.x() == 0 || params_.scale_to.y() == 0) {
    // The caller computed invalid scale_from and/or scale_to values.
    DVLOG(1) << __func__ << ": Invalid scaling vectors: scale_from="
             << params_.scale_from.ToString()
             << ", scale_to=" << params_.scale_to.ToString();
    return false;
  }

  // Resolve the color spaces according to the rules described in the header
  // file.
  if (!params_.source_color_space.IsValid()) {
    params_.source_color_space = gfx::ColorSpace::CreateSRGB();
  }
  if (!params_.output_color_space.IsValid()) {
    params_.output_color_space = params_.source_color_space;
  }

  // Check that 16-bit half floats are supported if precise color management is
  // being requested.
  if (params_.enable_precise_color_management) {
    if (!SupportsPreciseColorManagement()) {
      DVLOG(1) << __func__
               << ": GL context does not support the half-floats "
                  "required for precise color management.";
      return false;
    }
  }

  // Check that MRT support is available if certain export formats were
  // specified in the Parameters.
  if (params_.export_format == Parameters::ExportFormat::NV61 ||
      params_.export_format ==
          Parameters::ExportFormat::DEINTERLEAVE_PAIRWISE) {
    if (GetMaxDrawBuffersSupported() < 2) {
      DVLOG(1) << __func__ << ": GL context does not support 2+ draw buffers.";
      return false;
    }
  }

  // Color space transformation is meaningless when using the deinterleaver
  // because it only deals with two color channels. This also means precise
  // color management must be disabled.
  if (params_.export_format ==
          Parameters::ExportFormat::DEINTERLEAVE_PAIRWISE &&
      (params_.source_color_space != params_.output_color_space ||
       params_.enable_precise_color_management)) {
    NOTIMPLEMENTED();
    return false;
  }

  // Check that one of the two implemented output swizzles has been specified.
  for (GLenum s : params_.swizzle) {
    if (s != GL_RGBA && s != GL_BGRA_EXT) {
      NOTIMPLEMENTED();
      return false;
    }
  }

  // Create the chain of ScalerStages. If the quality setting is FAST or there
  // is no scaling to be done, just create a single stage.
  std::unique_ptr<ScalerStage> chain;
  if (params_.quality == Parameters::Quality::FAST ||
      params_.scale_from == params_.scale_to) {
    chain = std::make_unique<ScalerStage>(gl, Shader::BILINEAR, HORIZONTAL,
                                          params_.scale_from, params_.scale_to);
  } else if (params_.quality == Parameters::Quality::GOOD) {
    chain = CreateAGoodScalingChain(gl, params_.scale_from, params_.scale_to);
  } else if (params_.quality == Parameters::Quality::BEST) {
    chain = CreateTheBestScalingChain(gl, params_.scale_from, params_.scale_to);
  } else {
    NOTREACHED();
  }
  chain = MaybeAppendExportStage(gl, std::move(chain), params_.export_format);

  // Determine the color space and the data type of the pixels in the
  // intermediate textures, depending on whether precise color management is
  // enabled. Note that nothing special need be done here if no scaling will be
  // performed.
  GLenum intermediate_texture_type;
  if (params_.enable_precise_color_management &&
      params_.scale_from != params_.scale_to) {
    // Ensure the scaling color space is using a linear transfer function.
    constexpr auto kLinearFunction = std::make_tuple(1, 0, 1, 0, 0, 0, 1);
    skcms_TransferFunction fn;
    if (params_.source_color_space.GetTransferFunction(&fn) &&
        std::make_tuple(fn.a, fn.b, fn.c, fn.d, fn.e, fn.f, fn.g) ==
            kLinearFunction) {
      scaling_color_space_ = params_.source_color_space;
    } else {
      // Use the source color space, but with a linear transfer function.
      skcms_Matrix3x3 to_XYZD50;
      params_.source_color_space.GetPrimaryMatrix(&to_XYZD50);
      std::tie(fn.a, fn.b, fn.c, fn.d, fn.e, fn.f, fn.g) = kLinearFunction;
      scaling_color_space_ = gfx::ColorSpace::CreateCustom(to_XYZD50, fn);
    }
    intermediate_texture_type = GL_HALF_FLOAT_OES;
  } else {
    scaling_color_space_ = params_.source_color_space;
    intermediate_texture_type = GL_UNSIGNED_BYTE;
  }

  // Set the shader program on the final stage. Include color space
  // transformation and swizzling, if necessary.
  std::unique_ptr<gfx::ColorTransform> transform;
  if (scaling_color_space_ != params_.output_color_space) {
    transform = gfx::ColorTransform::NewColorTransform(
        scaling_color_space_, params_.output_color_space,
        gfx::ColorTransform::Intent::INTENT_PERCEPTUAL);
  }
  ScalerStage* const final_stage = chain.get();
  final_stage->set_shader_program(
      GetShaderProgram(final_stage->shader(), intermediate_texture_type,
                       transform.get(), params_.swizzle));

  // Set the shader program on all prior stages. These stages are all operating
  // in the same color space, |scaling_color_space_|.
  static const GLenum kNoSwizzle[2] = {GL_RGBA, GL_RGBA};
  ScalerStage* input_stage = final_stage;
  while (input_stage->input_stage()) {
    input_stage = input_stage->input_stage();
    input_stage->set_shader_program(GetShaderProgram(
        input_stage->shader(), intermediate_texture_type, nullptr, kNoSwizzle));
  }
  // From this point, |input_stage| points to the first ScalerStage (i.e., the
  // one that will be reading from the source).

  // If necessary, prepend an extra "import stage" that color-converts the input
  // before any scaling occurs. It's important not to merge color space
  // conversion of the source with any other steps because the texture sampler
  // must not linearly interpolate until after the colors have been mapped to a
  // linear color space.
  if (params_.source_color_space != scaling_color_space_) {
    input_stage->set_input_stage(std::make_unique<ScalerStage>(
        gl, Shader::BILINEAR, HORIZONTAL, input_stage->scale_from(),
        input_stage->scale_from()));
    input_stage = input_stage->input_stage();
    transform = gfx::ColorTransform::NewColorTransform(
        params_.source_color_space, scaling_color_space_,
        gfx::ColorTransform::Intent::INTENT_PERCEPTUAL);
    input_stage->set_shader_program(
        GetShaderProgram(input_stage->shader(), intermediate_texture_type,
                         transform.get(), kNoSwizzle));
  }

  // If the source content is Y-flipped, the input scaler stage will perform
  // math to account for this. It also will flip the content during scaling so
  // that all following stages may assume the content is not flipped. Then, the
  // final stage must ensure the final output is correctly flipped-back (or not)
  // based on what the first stage did PLUS what is being requested by the
  // client code.
  if (params_.is_flipped_source) {
    input_stage->set_is_flipped_source(true);
    input_stage->set_flip_output(true);
  }
  if (input_stage->flip_output() != params_.flip_output) {
    final_stage->set_flip_output(!final_stage->flip_output());
  }

  chain_ = std::move(chain);
  VLOG(2) << __func__ << " built this: " << *this;
  return true;
}

bool GLScaler::ScaleToMultipleOutputs(GLuint src_texture,
                                      const gfx::Size& src_texture_size,
                                      const gfx::Vector2d& src_offset,
                                      GLuint dest_texture_0,
                                      GLuint dest_texture_1,
                                      const gfx::Rect& output_rect) {
  if (!chain_) {
    return false;
  }

  // Bind the vertex attributes used to sweep the entire source area when
  // executing the shader programs.
  GLES2Interface* const gl = context_provider_->ContextGL();
  DCHECK(gl);
  if (vertex_attributes_buffer_) {
    gl->BindBuffer(GL_ARRAY_BUFFER, vertex_attributes_buffer_);
  } else {
    gl->GenBuffers(1, &vertex_attributes_buffer_);
    gl->BindBuffer(GL_ARRAY_BUFFER, vertex_attributes_buffer_);
    gl->BufferData(GL_ARRAY_BUFFER, sizeof(ShaderProgram::kVertexAttributes),
                   ShaderProgram::kVertexAttributes, GL_STATIC_DRAW);
  }

  // Disable GL clipping/blending features that interfere with assumptions made
  // by the implementation. Only those known to possibly be enabled elsewhere in
  // Chromium code are disabled here, while the remainder are sanity-DCHECK'ed.
  gl->Disable(GL_SCISSOR_TEST);
  gl->Disable(GL_STENCIL_TEST);
  gl->Disable(GL_BLEND);
  DCHECK_NE(gl->IsEnabled(GL_CULL_FACE), GL_TRUE);
  DCHECK_NE(gl->IsEnabled(GL_DEPTH_TEST), GL_TRUE);
  DCHECK_NE(gl->IsEnabled(GL_POLYGON_OFFSET_FILL), GL_TRUE);
  DCHECK_NE(gl->IsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE), GL_TRUE);
  DCHECK_NE(gl->IsEnabled(GL_SAMPLE_COVERAGE), GL_TRUE);
  DCHECK_NE(gl->IsEnabled(GL_SCISSOR_TEST), GL_TRUE);
  DCHECK_NE(gl->IsEnabled(GL_STENCIL_TEST), GL_TRUE);

  chain_->ScaleToMultipleOutputs(src_texture, src_texture_size, src_offset,
                                 dest_texture_0, dest_texture_1, output_rect);

  gl->BindBuffer(GL_ARRAY_BUFFER, 0);
  return true;
}

// static
bool GLScaler::ParametersHasSameScaleRatio(const GLScaler::Parameters& params,
                                           const gfx::Vector2d& from,
                                           const gfx::Vector2d& to) {
  // Returns true iff a_num/a_denom == b_num/b_denom.
  const auto AreRatiosEqual = [](int32_t a_num, int32_t a_denom, int32_t b_num,
                                 int32_t b_denom) -> bool {
    // The math (for each dimension):
    //   If: a_num/a_denom == b_num/b_denom
    //   Then: a_num*b_denom == b_num*a_denom
    //
    // ...and cast to int64_t to guarantee no overflow from the multiplications.
    return (static_cast<int64_t>(a_num) * b_denom) ==
           (static_cast<int64_t>(b_num) * a_denom);
  };

  return AreRatiosEqual(params.scale_from.x(), params.scale_to.x(), from.x(),
                        to.x()) &&
         AreRatiosEqual(params.scale_from.y(), params.scale_to.y(), from.y(),
                        to.y());
}

// static
bool GLScaler::ParametersAreEquivalent(const Parameters& a,
                                       const Parameters& b) {
  if (!ParametersHasSameScaleRatio(a, b.scale_from, b.scale_to) ||
      a.enable_precise_color_management != b.enable_precise_color_management ||
      a.quality != b.quality || a.is_flipped_source != b.is_flipped_source ||
      a.flip_output != b.flip_output || a.export_format != b.export_format ||
      a.swizzle[0] != b.swizzle[0] || a.swizzle[1] != b.swizzle[1]) {
    return false;
  }

  const gfx::ColorSpace source_color_space_a =
      a.source_color_space.IsValid() ? a.source_color_space
                                     : gfx::ColorSpace::CreateSRGB();
  const gfx::ColorSpace source_color_space_b =
      b.source_color_space.IsValid() ? b.source_color_space
                                     : gfx::ColorSpace::CreateSRGB();
  if (source_color_space_a != source_color_space_b) {
    return false;
  }

  const gfx::ColorSpace output_color_space_a = a.output_color_space.IsValid()
                                                   ? a.output_color_space
                                                   : source_color_space_a;
  const gfx::ColorSpace output_color_space_b = b.output_color_space.IsValid()
                                                   ? b.output_color_space
                                                   : source_color_space_b;
  return output_color_space_a == output_color_space_b;
}

void GLScaler::OnContextLost() {
  // The destruction order here is important due to data dependencies.
  chain_.reset();
  shader_programs_.clear();
  if (vertex_attributes_buffer_) {
    if (auto* gl = context_provider_->ContextGL()) {
      gl->DeleteBuffers(1, &vertex_attributes_buffer_);
    }
    vertex_attributes_buffer_ = 0;
  }
  if (context_provider_) {
    context_provider_->RemoveObserver(this);
    context_provider_ = nullptr;
  }
}

GLScaler::ShaderProgram* GLScaler::GetShaderProgram(
    Shader shader,
    GLenum texture_type,
    const gfx::ColorTransform* color_transform,
    const GLenum swizzle[2]) {
  const ShaderCacheKey key{
      shader,
      texture_type,
      color_transform ? color_transform->GetSrcColorSpace() : gfx::ColorSpace(),
      color_transform ? color_transform->GetDstColorSpace() : gfx::ColorSpace(),
      swizzle[0],
      swizzle[1]};
  auto it = shader_programs_.find(key);
  if (it == shader_programs_.end()) {
    GLES2Interface* const gl = context_provider_->ContextGL();
    DCHECK(gl);
    it = shader_programs_
             .emplace(std::piecewise_construct, std::forward_as_tuple(key),
                      std::forward_as_tuple(gl, shader, texture_type,
                                            color_transform, swizzle))
             .first;
  }
  return &it->second;
}

// static
std::unique_ptr<GLScaler::ScalerStage> GLScaler::CreateAGoodScalingChain(
    gpu::gles2::GLES2Interface* gl,
    const gfx::Vector2d& scale_from,
    const gfx::Vector2d& scale_to) {
  DCHECK(scale_from.x() != 0 && scale_from.y() != 0)
      << "Bad scale_from: " << scale_from.ToString();
  DCHECK(scale_to.x() != 0 && scale_to.y() != 0)
      << "Bad scale_to: " << scale_to.ToString();
  DCHECK(scale_from != scale_to);

  // The GOOD quality chain performs one bilinear upscale followed by N bilinear
  // halvings, and does this is both directions. Exception: No upscale is needed
  // when |scale_from| is a power of two multiple of |scale_to|.
  //
  // Since all shaders use bilinear filtering, the heuristics below attempt to
  // greedily merge steps wherever possible to minimize GPU memory usage and
  // processing time. This also means that it will be extremely rare for the
  // stage doing the initial upscale to actually require a larger output texture
  // than the source texture (a downscale will be merged into the same stage).

  // Determine the initial upscaled-to size, as the minimum number of doublings
  // to make |scale_to| greater than |scale_from|.
  const RelativeSize from(scale_from);
  const RelativeSize to(scale_to);
  RelativeSize upscale_to = to;
  for (Axis x_or_y : std::array<Axis, 2>{HORIZONTAL, VERTICAL}) {
    while (upscale_to[x_or_y] < from[x_or_y]) {
      upscale_to[x_or_y] *= 2;
    }
  }

  // Create the stages in order from first-to-last, taking the greediest path
  // each time. Something like an A* algorithm would be better for discovering
  // an optimal sequence of operations, and would allow using the BILINEAR3
  // shader as well, but the run-time performance to compute the stages would be
  // too prohibitive.
  std::unique_ptr<ScalerStage> chain;
  struct CandidateOp {
    Shader shader;
    Axis primary_axis;
    RelativeSize output_size;
  };
  std::vector<CandidateOp> candidates;
  for (RelativeSize cur = from; cur != to;
       cur = RelativeSize(chain->scale_to())) {
    candidates.clear();

    // Determine whether it's possible to do exactly 2 bilinear passes in both
    // directions.
    RelativeSize output_size_2x2 = {0, 0};
    for (Axis x_or_y : std::array<Axis, 2>{VERTICAL, HORIZONTAL}) {
      if (cur[x_or_y] == from[x_or_y]) {
        // For the first stage, the 2 bilinear passes must be the initial
        // upscale followed by one downscale. If there is no initial upscale,
        // then the 2 passes must both be downscales.
        if (upscale_to[x_or_y] != from[x_or_y] &&
            upscale_to[x_or_y] / 2 >= to[x_or_y]) {
          output_size_2x2[x_or_y] = upscale_to[x_or_y] / 2;
        } else if (upscale_to[x_or_y] == from[x_or_y] &&
                   upscale_to[x_or_y] / 4 >= to[x_or_y]) {
          output_size_2x2[x_or_y] = cur[x_or_y] / 4;
        }
      } else {
        // For all later stages, the 2 bilinear passes must be 2 halvings.
        if (cur[x_or_y] / 4 >= to[x_or_y]) {
          output_size_2x2[x_or_y] = cur[x_or_y] / 4;
        }
      }
    }
    if (output_size_2x2[HORIZONTAL] != 0 && output_size_2x2[VERTICAL] != 0) {
      candidates.push_back(
          CandidateOp{Shader::BILINEAR2X2, HORIZONTAL, output_size_2x2});
    }

    // Determine the valid set of Ops that do 1 to 3 bilinear passes in one
    // direction and 0 or 1 pass in the other direction.
    for (Axis x_or_y : std::array<Axis, 2>{VERTICAL, HORIZONTAL}) {
      // The first bilinear pass in x_or_y must be an upscale or a halving.
      Shader shader = Shader::BILINEAR;
      RelativeSize output_size = cur;
      if (cur[x_or_y] == from[x_or_y] && upscale_to[x_or_y] != from[x_or_y]) {
        output_size[x_or_y] = upscale_to[x_or_y];
      } else if (cur[x_or_y] / 2 >= to[x_or_y]) {
        output_size[x_or_y] /= 2;
      } else {
        DCHECK_EQ(cur[x_or_y], to[x_or_y]);
        continue;
      }

      // Determine whether 1 or 2 additional passes can be made in the same
      // direction.
      if (output_size[x_or_y] / 4 >= to[x_or_y]) {
        shader = Shader::BILINEAR4;  // 2 more passes == 3 total.
        output_size[x_or_y] /= 4;
      } else if (output_size[x_or_y] / 2 >= to[x_or_y]) {
        shader = Shader::BILINEAR2;  // 1 more pass == 2 total.
        output_size[x_or_y] /= 2;
      } else {
        DCHECK_EQ(output_size[x_or_y], to[x_or_y]);
      }

      // Determine whether 0 or 1 bilinear passes can be made in the other
      // direction at the same time.
      const Axis y_or_x = TheOtherAxis(x_or_y);
      if (cur[y_or_x] == from[y_or_x] && upscale_to[y_or_x] != from[y_or_x]) {
        output_size[y_or_x] = upscale_to[y_or_x];
      } else if (cur[y_or_x] / 2 >= to[y_or_x]) {
        output_size[y_or_x] /= 2;
      } else {
        DCHECK_EQ(cur[y_or_x], to[y_or_x]);
      }

      candidates.push_back(CandidateOp{shader, x_or_y, output_size});
    }

    // From the candidates, pick the one that produces the fewest number of
    // output pixels, and append a new ScalerStage. There are opportunities to
    // improve the "cost function" here (e.g., pixels in the Y direction
    // probably cost more to process than pixels in the X direction), but that
    // would require more research.
    const auto best_candidate = std::min_element(
        candidates.begin(), candidates.end(),
        [](const CandidateOp& a, const CandidateOp& b) {
          static_assert(sizeof(a.output_size[0]) <= sizeof(int32_t),
                        "Overflow issue in the math here.");
          const int64_t cost_of_a =
              int64_t{a.output_size[HORIZONTAL]} * a.output_size[VERTICAL];
          const int64_t cost_of_b =
              int64_t{b.output_size[HORIZONTAL]} * b.output_size[VERTICAL];
          return cost_of_a < cost_of_b;
        });
    DCHECK(best_candidate != candidates.end());
    DCHECK(cur != best_candidate->output_size)
        << "Best candidate's output size (" << best_candidate->output_size
        << ") should not equal the input size.";
    auto next_stage = std::make_unique<ScalerStage>(
        gl, best_candidate->shader, best_candidate->primary_axis,
        cur.AsVector2d(), best_candidate->output_size.AsVector2d());
    next_stage->set_input_stage(std::move(chain));
    chain = std::move(next_stage);
  }

  return chain;
}

// static
std::unique_ptr<GLScaler::ScalerStage> GLScaler::CreateTheBestScalingChain(
    gpu::gles2::GLES2Interface* gl,
    const gfx::Vector2d& scale_from,
    const gfx::Vector2d& scale_to) {
  // The BEST quality chain performs one bicubic upscale followed by N bicubic
  // halvings, and does this is both directions. Exception: No upscale is needed
  // when |scale_from| is a power of two multiple of |scale_to|.

  // Determine the initial upscaled-to size, as the minimum number of doublings
  // to make |scale_to| greater than |scale_from|.
  const RelativeSize from(scale_from);
  const RelativeSize to(scale_to);
  RelativeSize upscale_to = to;
  for (Axis x_or_y : std::array<Axis, 2>{HORIZONTAL, VERTICAL}) {
    while (upscale_to[x_or_y] < from[x_or_y]) {
      upscale_to[x_or_y] *= 2;
    }
  }

  // Create the stages in order from first-to-last.
  RelativeSize cur = from;
  std::unique_ptr<ScalerStage> chain;
  for (Axis x_or_y : std::array<Axis, 2>{VERTICAL, HORIZONTAL}) {
    if (upscale_to[x_or_y] != from[x_or_y]) {
      RelativeSize next = cur;
      next[x_or_y] = upscale_to[x_or_y];
      auto upscale_stage =
          std::make_unique<ScalerStage>(gl, Shader::BICUBIC_UPSCALE, x_or_y,
                                        cur.AsVector2d(), next.AsVector2d());
      upscale_stage->set_input_stage(std::move(chain));
      chain = std::move(upscale_stage);
      cur = next;
    }
    while (cur[x_or_y] > to[x_or_y]) {
      RelativeSize next = cur;
      next[x_or_y] /= 2;
      auto next_stage =
          std::make_unique<ScalerStage>(gl, Shader::BICUBIC_HALF_1D, x_or_y,
                                        cur.AsVector2d(), next.AsVector2d());
      next_stage->set_input_stage(std::move(chain));
      chain = std::move(next_stage);
      cur = next;
    }
  }
  DCHECK_EQ(cur, to);

  return chain;
}

// static
std::unique_ptr<GLScaler::ScalerStage> GLScaler::MaybeAppendExportStage(
    gpu::gles2::GLES2Interface* gl,
    std::unique_ptr<GLScaler::ScalerStage> chain,
    GLScaler::Parameters::ExportFormat export_format) {
  DCHECK(chain);

  if (export_format == Parameters::ExportFormat::INTERLEAVED_QUADS) {
    return chain;  // No format change.
  }

  // If the final stage uses the BILINEAR shader that is not upscaling, the
  // export stage can replace it with no change in the results. Otherwise, a
  // separate export stage will be appended.
  gfx::Vector2d scale_from = chain->scale_from();
  const gfx::Vector2d scale_to = chain->scale_to();
  if (chain->shader() == Shader::BILINEAR && scale_from.x() >= scale_to.x() &&
      scale_from.y() >= scale_to.y()) {
    chain = chain->take_input_stage();
  } else {
    scale_from = scale_to;
  }

  Shader shader = Shader::BILINEAR;
  scale_from.set_x(scale_from.x() * 4);
  switch (export_format) {
    case Parameters::ExportFormat::INTERLEAVED_QUADS:
      NOTREACHED();
      break;
    case Parameters::ExportFormat::CHANNEL_0:
      shader = Shader::PLANAR_CHANNEL_0;
      break;
    case Parameters::ExportFormat::CHANNEL_1:
      shader = Shader::PLANAR_CHANNEL_1;
      break;
    case Parameters::ExportFormat::CHANNEL_2:
      shader = Shader::PLANAR_CHANNEL_2;
      break;
    case Parameters::ExportFormat::CHANNEL_3:
      shader = Shader::PLANAR_CHANNEL_3;
      break;
    case Parameters::ExportFormat::NV61:
      shader = Shader::I422_NV61_MRT;
      break;
    case Parameters::ExportFormat::DEINTERLEAVE_PAIRWISE:
      shader = Shader::DEINTERLEAVE_PAIRWISE_MRT;
      // Horizontal scale is only 0.5X, not 0.25X like all the others.
      scale_from.set_x(scale_from.x() / 2);
      break;
  }

  auto export_stage = std::make_unique<ScalerStage>(gl, shader, HORIZONTAL,
                                                    scale_from, scale_to);
  export_stage->set_input_stage(std::move(chain));
  return export_stage;
}

// static
GLScaler::Axis GLScaler::TheOtherAxis(GLScaler::Axis x_or_y) {
  return x_or_y == HORIZONTAL ? VERTICAL : HORIZONTAL;
}

// static
const char* GLScaler::GetShaderName(GLScaler::Shader shader) {
  switch (shader) {
#define CASE_RETURN_SHADER_STR(x) \
  case Shader::x:                 \
    return #x
    CASE_RETURN_SHADER_STR(BILINEAR);
    CASE_RETURN_SHADER_STR(BILINEAR2);
    CASE_RETURN_SHADER_STR(BILINEAR3);
    CASE_RETURN_SHADER_STR(BILINEAR4);
    CASE_RETURN_SHADER_STR(BILINEAR2X2);
    CASE_RETURN_SHADER_STR(BICUBIC_UPSCALE);
    CASE_RETURN_SHADER_STR(BICUBIC_HALF_1D);
    CASE_RETURN_SHADER_STR(PLANAR_CHANNEL_0);
    CASE_RETURN_SHADER_STR(PLANAR_CHANNEL_1);
    CASE_RETURN_SHADER_STR(PLANAR_CHANNEL_2);
    CASE_RETURN_SHADER_STR(PLANAR_CHANNEL_3);
    CASE_RETURN_SHADER_STR(I422_NV61_MRT);
    CASE_RETURN_SHADER_STR(DEINTERLEAVE_PAIRWISE_MRT);
#undef CASE_RETURN_SHADER_STR
  }
}

// static
bool GLScaler::AreAllGLExtensionsPresent(
    gpu::gles2::GLES2Interface* gl,
    const std::vector<std::string>& names) {
  DCHECK(gl);
  if (const auto* extensions = gl->GetString(GL_EXTENSIONS)) {
    const std::string extensions_string =
        " " + std::string(reinterpret_cast<const char*>(extensions)) + " ";
    for (const std::string& name : names) {
      if (extensions_string.find(" " + name + " ") == std::string::npos) {
        return false;
      }
    }
    return true;
  }
  return false;
}

GLScaler::Parameters::Parameters() = default;
GLScaler::Parameters::Parameters(const Parameters& other) = default;
GLScaler::Parameters::~Parameters() = default;

// static
const GLfloat GLScaler::ShaderProgram::kVertexAttributes[16] = {
    -1.0f, -1.0f, 0.0f, 0.0f,  // vertex 0
    1.0f,  -1.0f, 1.0f, 0.0f,  // vertex 1
    -1.0f, 1.0f,  0.0f, 1.0f,  // vertex 2
    1.0f,  1.0f,  1.0f, 1.0f,  // vertex 3
};

GLScaler::ShaderProgram::ShaderProgram(
    gpu::gles2::GLES2Interface* gl,
    GLScaler::Shader shader,
    GLenum texture_type,
    const gfx::ColorTransform* color_transform,
    const GLenum swizzle[2])
    : gl_(gl),
      shader_(shader),
      texture_type_(texture_type),
      program_(gl_->CreateProgram()) {
  DCHECK(program_);

  std::basic_ostringstream<GLchar> vertex_header;
  std::basic_ostringstream<GLchar> fragment_directives;
  std::basic_ostringstream<GLchar> fragment_header;
  std::basic_ostringstream<GLchar> shared_variables;
  std::basic_ostringstream<GLchar> vertex_main;
  std::basic_ostringstream<GLchar> fragment_main;

  vertex_header
      << ("precision highp float;\n"
          "attribute vec2 a_position;\n"
          "attribute vec2 a_texcoord;\n"
          "uniform vec4 src_rect;\n");

  fragment_header << "precision mediump float;\n";
  switch (texture_type_) {
    case GL_FLOAT:
      fragment_header << "precision highp sampler2D;\n";
      break;
    case GL_HALF_FLOAT_OES:
      fragment_header << "precision mediump sampler2D;\n";
      break;
    default:
      fragment_header << "precision lowp sampler2D;\n";
      break;
  }
  fragment_header << "uniform sampler2D s_texture;\n";

  if (color_transform && shader_ != Shader::PLANAR_CHANNEL_3) {
    const std::string& source = color_transform->GetShaderSource();
    // Assumption: gfx::ColorTransform::GetShaderSource() should provide a
    // function named DoColorConversion() that takes a vec3 argument and returns
    // a vec3.
    DCHECK_NE(source.find("DoColorConversion"), std::string::npos);
    fragment_header << source;
  }

  vertex_main
      << ("  gl_Position = vec4(a_position, 0.0, 1.0);\n"
          "  vec2 texcoord = src_rect.xy + a_texcoord * src_rect.zw;\n");

  switch (shader_) {
    case Shader::BILINEAR:
      shared_variables << "varying highp vec2 v_texcoord;\n";
      vertex_main << "  v_texcoord = texcoord;\n";
      fragment_main << "  vec4 sample = texture2D(s_texture, v_texcoord);\n";
      if (color_transform) {
        fragment_main << "  sample.rgb = DoColorConversion(sample.rgb);\n";
      }
      if (swizzle[0] == GL_BGRA_EXT) {
        fragment_main << "  sample.rb = sample.br;\n";
      }
      fragment_main << "  gl_FragColor = sample;\n";
      break;

    case Shader::BILINEAR2:
      // This is equivialent to two passes of the BILINEAR shader above. It can
      // be used to scale an image down 1.0x-2.0x in either dimension, or
      // exactly 4x.
      shared_variables << "varying highp vec4 v_texcoords;\n";
      vertex_header << "uniform vec2 scaling_vector;\n";
      vertex_main
          << ("  vec2 step = scaling_vector / 4.0;\n"
              "  v_texcoords.xy = texcoord + step;\n"
              "  v_texcoords.zw = texcoord - step;\n");
      fragment_main
          << ("  vec4 blended = (texture2D(s_texture, v_texcoords.xy) +\n"
              "                  texture2D(s_texture, v_texcoords.zw)) /\n"
              "                 2.0;\n");
      if (color_transform) {
        fragment_main << "  blended.rgb = DoColorConversion(blended.rgb);\n";
      }
      if (swizzle[0] == GL_BGRA_EXT) {
        fragment_main << "  blended.rb = blended.br;\n";
      }
      fragment_main << "  gl_FragColor = blended;\n";
      break;

    case Shader::BILINEAR3:
      // This is kind of like doing 1.5 passes of the BILINEAR shader. It can be
      // used to scale an image down 1.5x-3.0x, or exactly 6x.
      shared_variables
          << ("varying highp vec4 v_texcoords0;\n"
              "varying highp vec2 v_texcoords1;\n");
      vertex_header << "uniform vec2 scaling_vector;\n";
      vertex_main
          << ("  vec2 step = scaling_vector / 3.0;\n"
              "  v_texcoords0.xy = texcoord + step;\n"
              "  v_texcoords0.zw = texcoord;\n"
              "  v_texcoords1 = texcoord - step;\n");
      fragment_main
          << ("  vec4 blended = (texture2D(s_texture, v_texcoords0.xy) +\n"
              "                  texture2D(s_texture, v_texcoords0.zw) +\n"
              "                  texture2D(s_texture, v_texcoords1)) / 3.0;\n");
      if (color_transform) {
        fragment_main << "  blended.rgb = DoColorConversion(blended.rgb);\n";
      }
      if (swizzle[0] == GL_BGRA_EXT) {
        fragment_main << "  blended.rb = blended.br;\n";
      }
      fragment_main << "  gl_FragColor = blended;\n";
      break;

    case Shader::BILINEAR4:
      // This is equivialent to three passes of the BILINEAR shader above. It
      // can be used to scale an image down 2.0x-4.0x or exactly 8x.
      shared_variables << "varying highp vec4 v_texcoords[2];\n";
      vertex_header << "uniform vec2 scaling_vector;\n";
      vertex_main
          << ("  vec2 step = scaling_vector / 8.0;\n"
              "  v_texcoords[0].xy = texcoord - step * 3.0;\n"
              "  v_texcoords[0].zw = texcoord - step;\n"
              "  v_texcoords[1].xy = texcoord + step;\n"
              "  v_texcoords[1].zw = texcoord + step * 3.0;\n");
      fragment_main
          << ("  vec4 blended = (\n"
              "      texture2D(s_texture, v_texcoords[0].xy) +\n"
              "      texture2D(s_texture, v_texcoords[0].zw) +\n"
              "      texture2D(s_texture, v_texcoords[1].xy) +\n"
              "      texture2D(s_texture, v_texcoords[1].zw)) / 4.0;\n");
      if (color_transform) {
        fragment_main << "  blended.rgb = DoColorConversion(blended.rgb);\n";
      }
      if (swizzle[0] == GL_BGRA_EXT) {
        fragment_main << "  blended.rb = blended.br;\n";
      }
      fragment_main << "  gl_FragColor = blended;\n";
      break;

    case Shader::BILINEAR2X2:
      // This is equivialent to four passes of the BILINEAR shader above, two in
      // each dimension. It can be used to scale an image down 1.0x-2.0x in both
      // X and Y directions. Or, it could be used to scale an image down by
      // exactly 4x in both dimensions.
      shared_variables << "varying highp vec4 v_texcoords[2];\n";
      vertex_header << "uniform vec2 scaling_vector;\n";
      vertex_main
          << ("  vec2 step = scaling_vector / 4.0;\n"
              "  v_texcoords[0].xy = texcoord + vec2(step.x, step.y);\n"
              "  v_texcoords[0].zw = texcoord + vec2(step.x, -step.y);\n"
              "  v_texcoords[1].xy = texcoord + vec2(-step.x, step.y);\n"
              "  v_texcoords[1].zw = texcoord + vec2(-step.x, -step.y);\n");
      fragment_main
          << ("  vec4 blended = (\n"
              "      texture2D(s_texture, v_texcoords[0].xy) +\n"
              "      texture2D(s_texture, v_texcoords[0].zw) +\n"
              "      texture2D(s_texture, v_texcoords[1].xy) +\n"
              "      texture2D(s_texture, v_texcoords[1].zw)) / 4.0;\n");
      if (color_transform) {
        fragment_main << "  blended.rgb = DoColorConversion(blended.rgb);\n";
      }
      if (swizzle[0] == GL_BGRA_EXT) {
        fragment_main << "  blended.rb = blended.br;\n";
      }
      fragment_main << "  gl_FragColor = blended;\n";
      break;

    case Shader::BICUBIC_UPSCALE:
      // When scaling up, 4 texture reads are necessary. However, some
      // instructions can be saved because the parameter passed to the bicubic
      // function will be in a known range. Also, when sampling the bicubic
      // function like this, the sum is always exactly one, so normalization can
      // be skipped as well.
      shared_variables << "varying highp vec2 v_texcoord;\n";
      vertex_main << "  v_texcoord = texcoord;\n";
      fragment_header
          << ("uniform highp vec2 src_pixelsize;\n"
              "uniform highp vec2 scaling_vector;\n"
              "const float a = -0.5;\n"
              // This function is equivialent to calling the bicubic
              // function with x-1, x, 1-x and 2-x (assuming
              // 0 <= x < 1). The following is the Catmull-Rom spline.
              // See: http://wikipedia.org/wiki/Cubic_Hermite_spline
              "vec4 filt4(float x) {\n"
              "  return vec4(x * x * x, x * x, x, 1) *\n"
              "         mat4(       a,      -2.0 * a,   a, 0.0,\n"
              "               a + 2.0,      -a - 3.0, 0.0, 1.0,\n"
              "              -a - 2.0, 3.0 + 2.0 * a,  -a, 0.0,\n"
              "                    -a,             a, 0.0, 0.0);\n"
              "}\n"
              "mat4 pixels_x(highp vec2 pos, highp vec2 step) {\n"
              "  return mat4(texture2D(s_texture, pos - step),\n"
              "              texture2D(s_texture, pos),\n"
              "              texture2D(s_texture, pos + step),\n"
              "              texture2D(s_texture, pos + step * 2.0));\n"
              "}\n");
      fragment_main
          << ("  highp vec2 pixel_pos = v_texcoord * src_pixelsize - \n"
              "      scaling_vector / 2.0;\n"
              "  highp float frac = fract(dot(pixel_pos, scaling_vector));\n"
              "  highp vec2 base = \n"
              "      (floor(pixel_pos) + vec2(0.5)) / src_pixelsize;\n"
              "  highp vec2 step = scaling_vector / src_pixelsize;\n"
              "  vec4 blended = pixels_x(base, step) * filt4(frac);\n");
      if (color_transform) {
        fragment_main << "  blended.rgb = DoColorConversion(blended.rgb);\n";
      }
      if (swizzle[0] == GL_BGRA_EXT) {
        fragment_main << "  blended.rb = blended.br;\n";
      }
      fragment_main << "  gl_FragColor = blended;\n";
      break;

    case Shader::BICUBIC_HALF_1D:
      // This scales down an image by exactly half in one dimension. The
      // bilinear lookup reduces the number of texture reads from 8 to 4.
      shared_variables << "varying highp vec4 v_texcoords[2];\n";
      vertex_header
          << ("uniform vec2 scaling_vector;\n"
              "const float CenterDist = 99.0 / 140.0;\n"
              "const float LobeDist = 11.0 / 4.0;\n");
      vertex_main
          << ("  vec2 step = scaling_vector / 2.0;\n"
              "  v_texcoords[0].xy = texcoord - LobeDist * step;\n"
              "  v_texcoords[0].zw = texcoord - CenterDist * step;\n"
              "  v_texcoords[1].xy = texcoord + CenterDist * step;\n"
              "  v_texcoords[1].zw = texcoord + LobeDist * step;\n");
      fragment_header
          << ("const float CenterWeight = 35.0 / 64.0;\n"
              "const float LobeWeight = -3.0 / 64.0;\n");
      fragment_main
          << ("  vec4 blended = \n"
              // Lobe pixels
              "      (texture2D(s_texture, v_texcoords[0].xy) +\n"
              "       texture2D(s_texture, v_texcoords[1].zw)) *\n"
              "          LobeWeight +\n"
              // Center pixels
              "      (texture2D(s_texture, v_texcoords[0].zw) +\n"
              "       texture2D(s_texture, v_texcoords[1].xy)) *\n"
              "          CenterWeight;\n");
      if (color_transform) {
        fragment_main << "  blended.rgb = DoColorConversion(blended.rgb);\n";
      }
      if (swizzle[0] == GL_BGRA_EXT) {
        fragment_main << "  blended.rb = blended.br;\n";
      }
      fragment_main << "  gl_FragColor = blended;\n";
      break;

    case Shader::PLANAR_CHANNEL_0:
    case Shader::PLANAR_CHANNEL_1:
    case Shader::PLANAR_CHANNEL_2:
    case Shader::PLANAR_CHANNEL_3: {
      // Select one color channel, and pack 4 pixels into one output quad. See
      // header file for diagram.
      shared_variables << "varying highp vec4 v_texcoords[2];\n";
      vertex_header << "uniform vec2 scaling_vector;\n";
      vertex_main
          << ("  vec2 step = scaling_vector / 4.0;\n"
              "  v_texcoords[0].xy = texcoord - step * 1.5;\n"
              "  v_texcoords[0].zw = texcoord - step * 0.5;\n"
              "  v_texcoords[1].xy = texcoord + step * 0.5;\n"
              "  v_texcoords[1].zw = texcoord + step * 1.5;\n");
      std::basic_string<GLchar> convert_open;
      std::basic_string<GLchar> convert_close;
      if (color_transform && shader_ != Shader::PLANAR_CHANNEL_3) {
        convert_open = "DoColorConversion(";
        convert_close = ".rgb)";
      }
      const char selector = "rgba"[static_cast<int>(shader_) -
                                   static_cast<int>(Shader::PLANAR_CHANNEL_0)];
      fragment_main << "  vec4 packed_quad = vec4(\n"
                    << "      " << convert_open
                    << "texture2D(s_texture, v_texcoords[0].xy)"
                    << convert_close << '.' << selector << ",\n"
                    << "      " << convert_open
                    << "texture2D(s_texture, v_texcoords[0].zw)"
                    << convert_close << '.' << selector << ",\n"
                    << "      " << convert_open
                    << "texture2D(s_texture, v_texcoords[1].xy)"
                    << convert_close << '.' << selector << ",\n"
                    << "      " << convert_open
                    << "texture2D(s_texture, v_texcoords[1].zw)"
                    << convert_close << '.' << selector << ");\n";
      if (swizzle[0] == GL_BGRA_EXT) {
        fragment_main << "  packed_quad.rb = packed_quad.br;\n";
      }
      fragment_main << "  gl_FragColor = packed_quad;\n";
      break;
    }

    case Shader::I422_NV61_MRT:
      // I422 sampling, delivered via two output textures (NV61 format). See
      // header file for diagram.
      shared_variables << "varying highp vec4 v_texcoords[2];\n";
      vertex_header << "uniform vec2 scaling_vector;\n";
      vertex_main
          << ("  vec2 step = scaling_vector / 4.0;\n"
              "  v_texcoords[0].xy = texcoord - step * 1.5;\n"
              "  v_texcoords[0].zw = texcoord - step * 0.5;\n"
              "  v_texcoords[1].xy = texcoord + step * 0.5;\n"
              "  v_texcoords[1].zw = texcoord + step * 1.5;\n");
      fragment_directives << "#extension GL_EXT_draw_buffers : enable\n";
      fragment_main
          << ("  vec3 pixel0 = texture2D(s_texture, v_texcoords[0].xy).rgb;\n"
              "  vec3 pixel1 = texture2D(s_texture, v_texcoords[0].zw).rgb;\n"
              "  vec3 pixel01 = (pixel0 + pixel1) / 2.0;\n"
              "  vec3 pixel2 = texture2D(s_texture, v_texcoords[1].xy).rgb;\n"
              "  vec3 pixel3 = texture2D(s_texture, v_texcoords[1].zw).rgb;\n"
              "  vec3 pixel23 = (pixel2 + pixel3) / 2.0;\n");
      if (color_transform) {
        fragment_main
            << ("  pixel0 = DoColorConversion(pixel0);\n"
                "  pixel1 = DoColorConversion(pixel1);\n"
                "  pixel01 = DoColorConversion(pixel01);\n"
                "  pixel2 = DoColorConversion(pixel2);\n"
                "  pixel3 = DoColorConversion(pixel3);\n"
                "  pixel23 = DoColorConversion(pixel23);\n");
      }
      if (swizzle[0] == GL_BGRA_EXT) {
        fragment_main
            << ("  gl_FragData[0] = \n"
                "      vec4(pixel2.r, pixel1.r, pixel0.r, pixel3.r);\n");
      } else {
        fragment_main
            << ("  gl_FragData[0] = \n"
                "      vec4(pixel0.r, pixel1.r, pixel2.r, pixel3.r);\n");
      }
      if (swizzle[1] == GL_BGRA_EXT) {
        fragment_main
            << ("  gl_FragData[1] = \n"
                "      vec4(pixel23.g, pixel01.b, pixel01.g, pixel23.b);\n");
      } else {
        fragment_main
            << ("  gl_FragData[1] = \n"
                "      vec4(pixel01.g, pixel01.b, pixel23.g, pixel23.b);\n");
      }
      break;

    case Shader::DEINTERLEAVE_PAIRWISE_MRT:
      // Sample two pixels and unswizzle them. There's no need to do vertical
      // scaling with math, since the bilinear interpolation in the sampler
      // takes care of that.
      shared_variables << "varying highp vec4 v_texcoords;\n";
      vertex_header << "uniform vec2 scaling_vector;\n";
      vertex_main
          << ("  vec2 step = scaling_vector / 2.0;\n"
              "  v_texcoords.xy = texcoord - step * 0.5;\n"
              "  v_texcoords.zw = texcoord + step * 0.5;\n");
      fragment_directives << "#extension GL_EXT_draw_buffers : enable\n";
      DCHECK(!color_transform);
      fragment_main
          << ("  vec4 lo_uvuv = texture2D(s_texture, v_texcoords.xy);\n"
              "  vec4 hi_uvuv = texture2D(s_texture, v_texcoords.zw);\n"
              "  vec4 uuuu = vec4(lo_uvuv.rb, hi_uvuv.rb);\n"
              "  vec4 vvvv = vec4(lo_uvuv.ga, hi_uvuv.ga);\n");
      if (swizzle[0] == GL_BGRA_EXT) {
        fragment_main << "  uuuu.rb = uuuu.br;\n";
      }
      fragment_main << "  gl_FragData[0] = uuuu;\n";
      if (swizzle[1] == GL_BGRA_EXT) {
        fragment_main << "  vvvv.rb = vvvv.br;\n";
      }
      fragment_main << "  gl_FragData[1] = vvvv;\n";
      break;
  }

  // Helper function to compile the shader source and log the GLSL compiler's
  // results.
  const auto CompileShaderFromSource =
      [](GLES2Interface* gl, const std::basic_string<GLchar>& source,
         GLenum type) -> GLuint {
    VLOG(2) << __func__ << ": Compiling shader " << type
            << " with source:" << std::endl
            << source;
    const GLuint shader = gl->CreateShader(type);
    const GLchar* source_data = source.data();
    const GLint length = base::checked_cast<GLint>(source.size());
    gl->ShaderSource(shader, 1, &source_data, &length);
    gl->CompileShader(shader);
    GLint compile_status = GL_FALSE;
    gl->GetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);

    // Fetch logs and forward them to the system logger. If compilation failed,
    // clean-up and return 0 for error.
    if (compile_status != GL_TRUE || VLOG_IS_ON(2)) {
      GLint log_length = 0;
      gl->GetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
      std::string log;
      if (log_length > 0) {
        std::unique_ptr<GLchar[]> tmp(new GLchar[log_length]);
        GLsizei returned_log_length = 0;
        gl->GetShaderInfoLog(shader, log_length, &returned_log_length,
                             tmp.get());
        log.assign(tmp.get(), returned_log_length);
      }
      if (log.empty()) {
        log = "<<NO LOG>>";
      }
      if (compile_status != GL_TRUE) {
        LOG(ERROR) << __func__ << ": Compilation of shader " << type
                   << " failed:" << std::endl
                   << log;
        gl->DeleteShader(shader);
        return 0;
      }
      VLOG(2) << __func__ << ": Compilation of shader " << type
              << " succeeded:" << std::endl
              << log;
    }
    return shader;
  };

  // Compile the vertex shader and attach it to the program.
  const std::string shared_variables_str = shared_variables.str();
  const GLuint vertex_shader =
      CompileShaderFromSource(gl_,
                              vertex_header.str() + shared_variables_str +
                                  "void main() {\n" + vertex_main.str() + "}\n",
                              GL_VERTEX_SHADER);
  if (vertex_shader == 0) {
    return;
  }
  gl_->AttachShader(program_, vertex_shader);
  gl_->DeleteShader(vertex_shader);

  // Compile the fragment shader and attach to |program_|.
  const GLuint fragment_shader = CompileShaderFromSource(
      gl_,
      fragment_directives.str() + fragment_header.str() + shared_variables_str +
          "void main() {\n" + fragment_main.str() + "}\n",
      GL_FRAGMENT_SHADER);
  if (fragment_shader == 0) {
    return;
  }
  gl_->AttachShader(program_, fragment_shader);
  gl_->DeleteShader(fragment_shader);

  // Link the program.
  gl_->LinkProgram(program_);
  GLint link_status = GL_FALSE;
  gl_->GetProgramiv(program_, GL_LINK_STATUS, &link_status);
  if (link_status != GL_TRUE) {
    LOG(ERROR) << "Failed to link shader program.";
    return;
  }

#define DCHECK_RESOLVED_LOCATION(member)                                  \
  DCHECK(member != -1 || gl_->GetGraphicsResetStatusKHR() != GL_NO_ERROR) \
      << "Failed to get " #member " in program, or GPU process crashed."

  // Resolve the locations of the global variables.
  position_location_ = gl_->GetAttribLocation(program_, "a_position");
  DCHECK_RESOLVED_LOCATION(position_location_);
  texcoord_location_ = gl_->GetAttribLocation(program_, "a_texcoord");
  DCHECK_RESOLVED_LOCATION(texcoord_location_);
  texture_location_ = gl_->GetUniformLocation(program_, "s_texture");
  DCHECK_RESOLVED_LOCATION(texture_location_);
  src_rect_location_ = gl_->GetUniformLocation(program_, "src_rect");
  DCHECK_RESOLVED_LOCATION(src_rect_location_);
  switch (shader_) {
    case Shader::BILINEAR:
      break;

    case Shader::BILINEAR2:
    case Shader::BILINEAR3:
    case Shader::BILINEAR4:
    case Shader::BILINEAR2X2:
    case Shader::BICUBIC_HALF_1D:
    case Shader::PLANAR_CHANNEL_0:
    case Shader::PLANAR_CHANNEL_1:
    case Shader::PLANAR_CHANNEL_2:
    case Shader::PLANAR_CHANNEL_3:
    case Shader::I422_NV61_MRT:
    case Shader::DEINTERLEAVE_PAIRWISE_MRT:
      scaling_vector_location_ =
          gl_->GetUniformLocation(program_, "scaling_vector");
      DCHECK_RESOLVED_LOCATION(scaling_vector_location_);
      break;

    case Shader::BICUBIC_UPSCALE:
      src_pixelsize_location_ =
          gl_->GetUniformLocation(program_, "src_pixelsize");
      DCHECK_RESOLVED_LOCATION(src_pixelsize_location_);
      scaling_vector_location_ =
          gl_->GetUniformLocation(program_, "scaling_vector");
      DCHECK_RESOLVED_LOCATION(scaling_vector_location_);
      break;
  }

#undef DCHECK_RESOLVED_LOCATION
}

GLScaler::ShaderProgram::~ShaderProgram() {
  gl_->DeleteProgram(program_);
}

void GLScaler::ShaderProgram::UseProgram(const gfx::Size& src_texture_size,
                                         const gfx::RectF& src_rect,
                                         const gfx::Size& dst_size,
                                         GLScaler::Axis primary_axis,
                                         bool flip_y) {
  gl_->UseProgram(program_);

  // OpenGL defines the last parameter to VertexAttribPointer as type
  // "const GLvoid*" even though it is actually an offset into the buffer
  // object's data store and not a pointer to the client's address space.
  const void* offsets[2] = {nullptr,
                            reinterpret_cast<const void*>(2 * sizeof(GLfloat))};

  gl_->VertexAttribPointer(position_location_, 2, GL_FLOAT, GL_FALSE,
                           4 * sizeof(GLfloat), offsets[0]);
  gl_->EnableVertexAttribArray(position_location_);

  gl_->VertexAttribPointer(texcoord_location_, 2, GL_FLOAT, GL_FALSE,
                           4 * sizeof(GLfloat), offsets[1]);
  gl_->EnableVertexAttribArray(texcoord_location_);

  // Always sample from the first texture unit.
  gl_->Uniform1i(texture_location_, 0);

  // Convert |src_rect| from pixel coordinates to texture coordinates. The
  // source texture coordinates are in the range [0.0,1.0] for each dimension,
  // but the sampling rect may slightly "spill" outside that range (e.g., for
  // scaler overscan).
  GLfloat src_rect_texcoord[4] = {
      src_rect.x() / src_texture_size.width(),
      src_rect.y() / src_texture_size.height(),
      src_rect.width() / src_texture_size.width(),
      src_rect.height() / src_texture_size.height(),
  };
  if (flip_y) {
    src_rect_texcoord[1] += src_rect_texcoord[3];
    src_rect_texcoord[3] *= -1.0f;
  }
  gl_->Uniform4fv(src_rect_location_, 1, src_rect_texcoord);

  // Set shader-specific uniform inputs. The |scaling_vector| is the ratio of
  // the number of source pixels sampled per dest pixels output. It is used by
  // the shader programs to locate distinct texels from the source texture, and
  // sample them at the appropriate offset to produce each output texel.
  switch (shader_) {
    case Shader::BILINEAR:
      break;

    case Shader::BILINEAR2:
    case Shader::BILINEAR3:
    case Shader::BILINEAR4:
    case Shader::BICUBIC_HALF_1D:
    case Shader::PLANAR_CHANNEL_0:
    case Shader::PLANAR_CHANNEL_1:
    case Shader::PLANAR_CHANNEL_2:
    case Shader::PLANAR_CHANNEL_3:
    case Shader::I422_NV61_MRT:
    case Shader::DEINTERLEAVE_PAIRWISE_MRT:
      switch (primary_axis) {
        case HORIZONTAL:
          gl_->Uniform2f(scaling_vector_location_,
                         src_rect_texcoord[2] / dst_size.width(), 0.0);
          break;
        case VERTICAL:
          gl_->Uniform2f(scaling_vector_location_, 0.0,
                         src_rect_texcoord[3] / dst_size.height());
          break;
      }
      break;

    case Shader::BILINEAR2X2:
      gl_->Uniform2f(scaling_vector_location_,
                     src_rect_texcoord[2] / dst_size.width(),
                     src_rect_texcoord[3] / dst_size.height());
      break;

    case Shader::BICUBIC_UPSCALE:
      gl_->Uniform2f(src_pixelsize_location_, src_texture_size.width(),
                     src_texture_size.height());
      // For this shader program, the |scaling_vector| has an alternate meaning:
      // It is only being used to select whether bicubic sampling is stepped in
      // the X or the Y direction.
      gl_->Uniform2f(scaling_vector_location_,
                     primary_axis == HORIZONTAL ? 1.0 : 0.0,
                     primary_axis == VERTICAL ? 1.0 : 0.0);
      break;
  }
}

GLScaler::ScalerStage::ScalerStage(gpu::gles2::GLES2Interface* gl,
                                   GLScaler::Shader shader,
                                   GLScaler::Axis primary_axis,
                                   const gfx::Vector2d& scale_from,
                                   const gfx::Vector2d& scale_to)
    : gl_(gl),
      shader_(shader),
      primary_axis_(primary_axis),
      scale_from_(scale_from),
      scale_to_(scale_to) {
  DCHECK(gl_);
}

GLScaler::ScalerStage::~ScalerStage() {
  if (dest_framebuffer_) {
    gl_->DeleteFramebuffers(1, &dest_framebuffer_);
  }
  if (intermediate_texture_) {
    gl_->DeleteTextures(1, &intermediate_texture_);
  }
}

void GLScaler::ScalerStage::ScaleToMultipleOutputs(
    GLuint src_texture,
    gfx::Size src_texture_size,
    const gfx::Vector2d& src_offset,
    GLuint dest_texture_0,
    GLuint dest_texture_1,
    const gfx::Rect& output_rect) {
  if (output_rect.IsEmpty())
    return;  // No work to do.

  // Make a recursive call to the "input" ScalerStage to produce an intermediate
  // texture for this stage to source from. Adjust src_* variables to use the
  // intermediate texture as input.
  //
  // If there is no input stage, simply modify |src_rect| to account for the
  // overall |src_offset| and Y-flip.
  gfx::RectF src_rect = ToSourceRect(output_rect);
  if (input_stage_) {
    const gfx::Rect input_rect = ToInputRect(src_rect);
    EnsureIntermediateTextureDefined(input_rect.size());
    input_stage_->ScaleToMultipleOutputs(src_texture, src_texture_size,
                                         src_offset, intermediate_texture_, 0,
                                         input_rect);
    src_texture = intermediate_texture_;
    src_texture_size = intermediate_texture_size_;
    DCHECK(!is_flipped_source_);
    src_rect -= input_rect.OffsetFromOrigin();
  } else {
    if (is_flipped_source_) {
      src_rect.set_x(src_rect.x() + src_offset.x());
      src_rect.set_y(src_texture_size.height() - src_rect.bottom() -
                     src_offset.y());
    } else {
      src_rect += src_offset;
    }
  }

  // Attach the output texture(s) to the framebuffer.
  if (!dest_framebuffer_) {
    gl_->GenFramebuffers(1, &dest_framebuffer_);
  }
  gl_->BindFramebuffer(GL_FRAMEBUFFER, dest_framebuffer_);
  gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            dest_texture_0, 0);
  if (dest_texture_1 > 0) {
    gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + 1,
                              GL_TEXTURE_2D, dest_texture_1, 0);
  }

  // Bind to the source texture and set the texture sampler to use bilinear
  // filtering and clamp-to-edge, as required by all shader programs.
  //
  // It would be better to stash the existing parameter values, and restore them
  // back later. However, glGetTexParameteriv() currently requires a blocking
  // call to the GPU service, which is extremely costly performance-wise.
  gl_->ActiveTexture(GL_TEXTURE0);
  gl_->BindTexture(GL_TEXTURE_2D, src_texture);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Prepare the shader program for drawing.
  DCHECK(program_);
  program_->UseProgram(src_texture_size, src_rect, output_rect.size(),
                       primary_axis_, flip_output_);

  // Execute the draw.
  gl_->Viewport(0, 0, output_rect.width(), output_rect.height());
  const GLenum buffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT0 + 1};
  if (dest_texture_1 > 0) {
    gl_->DrawBuffersEXT(2, buffers);
  }
  gl_->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  if (dest_texture_1 > 0) {
    // Set the draw buffers back, to not disrupt external operations.
    gl_->DrawBuffersEXT(1, buffers);
  }

  gl_->BindTexture(GL_TEXTURE_2D, 0);
  gl_->BindFramebuffer(GL_FRAMEBUFFER, 0);
}

gfx::RectF GLScaler::ScalerStage::ToSourceRect(
    const gfx::Rect& output_rect) const {
  return gfx::ScaleRect(gfx::RectF(output_rect),
                        float{scale_from_.x()} / scale_to_.x(),
                        float{scale_from_.y()} / scale_to_.y());
}

gfx::Rect GLScaler::ScalerStage::ToInputRect(gfx::RectF source_rect) const {
  int overscan_x = 0;
  int overscan_y = 0;
  switch (shader_) {
    case Shader::BILINEAR:
    case Shader::BILINEAR2:
    case Shader::BILINEAR3:
    case Shader::BILINEAR4: {
      // These shaders sample 1 or more points along the primary axis, and only
      // 1 point in the other direction, in order to produce each output pixel.
      // The amount of overscan is always 0 or 1 pixel along the primary axis,
      // and this can be determined by looking at the upper-left-most source
      // texture sampling point: If this point is to the left of the middle of
      // the upper-left-most source pixel, the texture sampler will also read
      // the pixel to the left of that (for linear interpolation). Similar
      // behavior can occur towards the right, upwards, and downwards at the
      // source boundaries.
      int threshold;
      switch (shader_) {
        default:
          threshold = 1;
          break;
        case Shader::BILINEAR2:
          threshold = 2;
          break;
        case Shader::BILINEAR3:
          threshold = 3;
          break;
        case Shader::BILINEAR4:
          threshold = 4;
          break;
      }
      switch (primary_axis_) {
        case HORIZONTAL:
          if (scale_from_.x() < threshold * scale_to_.x()) {
            overscan_x = 1;
          }
          if (scale_from_.y() < scale_to_.y()) {
            overscan_y = 1;
          }
          break;
        case VERTICAL:
          if (scale_from_.x() < scale_to_.x()) {
            overscan_x = 1;
          }
          if (scale_from_.y() < threshold * scale_to_.y()) {
            overscan_y = 1;
          }
          break;
      }
      break;
    }

    case Shader::BILINEAR2X2:
      // This shader samples 2 points along both axes, and the overscan is 0 or
      // 1 pixel in both directions (same explanation as for the other BILINEAR
      // shaders).
      if (scale_from_.x() < 2 * scale_to_.x()) {
        overscan_x = 1;
      }
      if (scale_from_.y() < 2 * scale_to_.y()) {
        overscan_y = 1;
      }
      break;

    case Shader::BICUBIC_UPSCALE:
      // For each output pixel, this shader always reads 2 pixels about the
      // source position in one dimension, and has no overscan in the other
      // dimension.
      if (scale_from_.x() < scale_to_.x()) {
        DCHECK_EQ(HORIZONTAL, primary_axis_);
        overscan_x = 2;
      } else if (scale_from_.y() < scale_to_.y()) {
        DCHECK_EQ(VERTICAL, primary_axis_);
        overscan_y = 2;
      } else if (scale_from_ == scale_to_) {
        // Special case: When not scaling, the math in the shader will resolve
        // to just outputting the value for a single source pixel. The shader
        // will sample surrounding pixels, but then apply a zero weight to them
        // during convolution. Thus, there is effectively no overscan.
        NOTREACHED();  // This is a crazy-expensive way to do a 1:1 copy!
      } else {
        NOTREACHED();  // Downscaling is meaningless.
      }
      break;

    case Shader::BICUBIC_HALF_1D: {
      // For each output pixel, this shader always reads 4 pixels about the
      // source position in one dimension, and has no overscan in the other
      // dimension. However, since the source position always has a distance
      // >= 1 inside the "logical" bounds, there can never be more than 3 pixels
      // of overscan.
      if (scale_from_.x() == 2 * scale_to_.x()) {
        DCHECK_EQ(HORIZONTAL, primary_axis_);
        overscan_x = 3;
      } else if (scale_from_.y() == 2 * scale_to_.y()) {
        DCHECK_EQ(VERTICAL, primary_axis_);
        overscan_y = 3;
      } else {
        // Anything but a half-downscale in one dimension is meaningless.
        NOTREACHED();
      }
      break;
    }

    case Shader::PLANAR_CHANNEL_0:
    case Shader::PLANAR_CHANNEL_1:
    case Shader::PLANAR_CHANNEL_2:
    case Shader::PLANAR_CHANNEL_3:
    case Shader::I422_NV61_MRT:
      // All of these sample 4x1 source pixels to produce each output "pixel."
      // There is no overscan. They can also be combined with a bilinear
      // downscale, but not an upscale.
      DCHECK_GE(scale_from_.x(), 4 * scale_to_.x());
      DCHECK_EQ(HORIZONTAL, primary_axis_);
      break;

    case Shader::DEINTERLEAVE_PAIRWISE_MRT:
      // This shader samples 2x1 source pixels to produce each output "pixel."
      // There is no overscan. It can also be combined with a bilinear
      // downscale, but not an upscale.
      DCHECK_GE(scale_from_.x(), 2 * scale_to_.x());
      DCHECK_EQ(HORIZONTAL, primary_axis_);
      break;
  }

  source_rect.Inset(-overscan_x, -overscan_y);
  return gfx::ToEnclosingRect(source_rect);
}

void GLScaler::ScalerStage::EnsureIntermediateTextureDefined(
    const gfx::Size& size) {
  // Reallocate a new texture, if needed.
  if (!intermediate_texture_) {
    gl_->GenTextures(1, &intermediate_texture_);
  }
  if (intermediate_texture_size_ != size) {
    gl_->BindTexture(GL_TEXTURE_2D, intermediate_texture_);
    // Note: Not setting the filter or wrap parameters on the texture here
    // because that will be done in ScaleToMultipleOutputs() anyway.
    gl_->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width(), size.height(), 0,
                    GL_RGBA, program_->texture_type(), nullptr);
    intermediate_texture_size_ = size;
  }
}

std::ostream& operator<<(std::ostream& out, const GLScaler& scaler) {
  if (!scaler.chain_) {
    return (out << "[GLScaler NOT configured]");
  }

  out << "Output";
  const GLScaler::ScalerStage* const final_stage = scaler.chain_.get();
  for (auto* stage = final_stage; stage; stage = stage->input_stage()) {
    out << u8" ← {" << GLScaler::GetShaderName(stage->shader());
    if (stage->shader_program()) {
      switch (stage->shader_program()->texture_type()) {
        case GL_FLOAT:
          out << "/highp";
          break;
        case GL_HALF_FLOAT_OES:
          out << "/mediump";
          break;
        default:
          out << "/lowp";
          break;
      }
    }
    if (stage->flip_output()) {
      out << "+flip_y";
    }
    if (stage->scale_from() == stage->scale_to()) {
      out << " copy";
    } else {
      out << ' ' << stage->scale_from().ToString() << " to "
          << stage->scale_to().ToString();
    }
    if (!stage->input_stage() &&
        scaler.params_.source_color_space != scaler.scaling_color_space_) {
      out << ", with color x-form "
          << scaler.params_.source_color_space.ToString() << " to "
          << scaler.scaling_color_space_.ToString();
    }
    if (stage == final_stage) {
      if (scaler.params_.output_color_space != scaler.scaling_color_space_) {
        out << ", with color x-form to "
            << scaler.params_.output_color_space.ToString();
      }
      for (int i = 0; i < 2; ++i) {
        if (scaler.params_.swizzle[i] != GL_RGBA) {
          out << ", with swizzle(" << i << ')';
        }
      }
    }
    out << '}';
  }
  out << u8" ← Source";
  return out;
}

}  // namespace viz
