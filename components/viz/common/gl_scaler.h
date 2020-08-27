// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_GL_SCALER_H_
#define COMPONENTS_VIZ_COMMON_GL_SCALER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/viz_common_export.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

namespace gfx {
class ColorTransform;
}  // namespace gfx

namespace viz {

class ContextProvider;

// A high-performance texture scaler for use with an OpenGL ES 2.0 context. It
// can be configured to operate at different quality levels, manages/converts
// color spaces, and optionally re-arranges/formats data in output textures for
// use with more-efficient texture readback pipelines.
class VIZ_COMMON_EXPORT GLScaler : public ContextLostObserver {
 public:
  struct VIZ_COMMON_EXPORT Parameters {
    // Relative scale from/to factors. Both of these must be non-zero.
    gfx::Vector2d scale_from = gfx::Vector2d(1, 1);
    gfx::Vector2d scale_to = gfx::Vector2d(1, 1);

    // The color space of the source texture and the desired color space of the
    // output texture. If |source_color_space| is not set (or invalid), sRGB is
    // assumed. If |output_color_space| is not set (or invalid), the source
    // color space is assumed.
    gfx::ColorSpace source_color_space;
    gfx::ColorSpace output_color_space;

    // Enable color management heuristics, using higher precision texture and
    // gamma-aware scaling?
    //
    // When disabled, the gamma of the source color space and other concerns are
    // ignored and 8-bit precision is used.
    //
    // When enabled, scaling occurs in a linear color space with 16-bit floats.
    // This produces excellent results for virtually all color spaces while
    // typically requiring twice the memory and execution resources. The caller
    // must ensure the GL context supports the use of GL_RGBA16F format
    // textures.
    //
    // Relevant reading: http://www.ericbrasseur.org/gamma.html
    bool enable_precise_color_management = false;

    // Selects the trade-off between quality and speed.
    enum class Quality : int8_t {
      // Bilinear single pass. Fastest possible. Do not use this unless the GL
      // implementation is so slow that the other quality options won't work.
      FAST,

      // Bilinear upscale + N * 50% bilinear downscales. This is still fast
      // enough for general-purpose use, and image quality is nearly as good as
      // BEST when downscaling.
      GOOD,

      // Bicubic upscale + N * 50% bicubic downscales. Produces very good
      // quality scaled images, but it's 2-8x slower than the "GOOD" quality.
      BEST,
    } quality = Quality::GOOD;

    // Is the source texture Y-flipped (i.e., the origin is the lower-left
    // corner and not the upper-left corner)? Most GL textures are Y-flipped.
    // This information is required so that the scaler can correctly compute the
    // sampling region.
    bool is_flipped_source = true;

    // Should the output be vertically flipped? Usually, this is used when the
    // source is not Y-flipped, but the destination texture needs to be. Or, it
    // can be used to draw the final output upside-down to avoid having to copy
    // the rows in reverse order after a glReadPixels().
    bool flip_output = false;

    // Optionally rearrange the image data for export. Generally, this is used
    // to make later readback steps more efficient (e.g., using glReadPixels()
    // will produce the raw bytes in their correct locations).
    //
    // Output textures are assumed to be using one of the 4-channel RGBA
    // formats. While it may be more "proper" to use a single-component texture
    // format for the planar-oriented image data, not all GL implementations
    // support the use of those formats. However, all must support at least
    // GL_RGBA. Therefore, each RGBA pixel is treated as a generic "vec4" (a
    // quad of values).
    //
    // When using this feature, it is usually necessary to adjust the
    // |output_rect| passed to Scale() or ScaleToMultipleOutputs(). See notes
    // below.
    enum class ExportFormat : int8_t {
      // Do not rearrange the image data:
      //
      //   (interleaved quads)     (interleaved quads)
      //   RGBA RGBA RGBA RGBA     RGBA RGBA RGBA RGBA
      //   RGBA RGBA RGBA RGBA --> RGBA RGBA RGBA RGBA
      //   RGBA RGBA RGBA RGBA     RGBA RGBA RGBA RGBA
      INTERLEAVED_QUADS,

      // Select one color channel, packing each of 4 pixels' values into the 4
      // elements of one output quad.
      //
      // For example, for CHANNEL_0:
      //
      //             (interleaved quads)              (channel 0)
      //   RGBA RGBA RGBA RGBA RGBA RGBA RGBA RGBA     RRRR RRRR
      //   RGBA RGBA RGBA RGBA RGBA RGBA RGBA RGBA --> RRRR RRRR
      //   RGBA RGBA RGBA RGBA RGBA RGBA RGBA RGBA     RRRR RRRR
      //
      // Note: Because of this packing, the horizontal coordinates of the
      // |output_rect| used with Scale() should be divided by 4.
      CHANNEL_0,
      CHANNEL_1,
      CHANNEL_2,
      CHANNEL_3,

      // I422 sampling, delivered via two output textures (NV61 format): The
      // first texture is produced the same as CHANNEL_0, while the second
      // texture contains CHANNEL_1 and CHANNEL_2 at half-width interleaved and
      // full-height. For example, if this is combined with RGB→YUV color space
      // conversion:
      //
      //              (interleaved quads)
      //    RGBA RGBA RGBA RGBA RGBA RGBA RGBA RGBA
      //    RGBA RGBA RGBA RGBA RGBA RGBA RGBA RGBA
      //    RGBA RGBA RGBA RGBA RGBA RGBA RGBA RGBA
      //      |
      //      |     (luma plane)  (chroma, interleaved)
      //      |       YYYY YYYY      UVUV UVUV
      //      +---> { YYYY YYYY  +   UVUV UVUV }
      //              YYYY YYYY      UVUV UVUV
      //
      // Note: Because of this packing, the horizontal coordinates of the
      // |output_rect| used with ScaleToMultipleOutputs() should be divided by
      // 4.
      // Note 2: This requires a GL context that supports multiple render
      // targets with at least two draw buffers.
      NV61,

      // Deinterleave into two output textures.
      //
      //  UVUV UVUV       UUUU   VVVV
      //  UVUV UVUV --> { UUUU + VVVV }
      //  UVUV UVUV       UUUU   VVVV
      //
      // Note: Because of this packing, the horizontal coordinates of the
      // |output_rect| used with ScaleToMultipleOutputs() should be divided by
      // 2.
      // Note 2: This requires a GL context that supports multiple render
      // targets with at least two draw buffers.
      DEINTERLEAVE_PAIRWISE,
    } export_format = ExportFormat::INTERLEAVED_QUADS;

    // Optionally swizzle the ordering of the values in each output quad. If the
    // output of the scaler is not going to be read back (e.g., used with
    // glReadPixels()), simply leave these unchanged. Otherwise, changing this
    // allows a read-back pipeline to use the native format of the platform to
    // avoid having to perform extra "BGRA⇄RGBA swizzle" memcpy's. Usually, this
    // should match the format to be used with glReadPixels(), and that should
    // match the GL_IMPLEMENTATION_COLOR_READ_FORMAT.
    GLenum swizzle[2] = {
        GL_RGBA,  // For |dest_texture_0|.
        GL_RGBA,  // For |dest_texture_1|.
    };

    Parameters();
    Parameters(const Parameters& other);
    ~Parameters();
  };

  explicit GLScaler(ContextProvider* context_provider);

  ~GLScaler() final;

  // Returns true if the GL context provides the necessary support for enabling
  // precise color management (see Parameters::enable_precise_color_management).
  bool SupportsPreciseColorManagement() const;

  // Returns the maximum number of simultaneous drawing buffers supported by the
  // GL context. Certain Parameters can only be used when this is more than 1.
  int GetMaxDrawBuffersSupported() const;

  // [Re]Configure the scaler with the given |new_params|. Returns true on
  // success, or false on failure.
  bool Configure(const Parameters& new_params) WARN_UNUSED_RESULT;

  // Returns the currently-configured and resolved Parameters. Note that these
  // Parameters might not be exactly the same as those that were passed to
  // Configure() because some properties (e.g., color spaces) are auto-resolved;
  // however, ParametersAreEquivalent() will still return true. Results are
  // undefined if Configure() has never been called successfully.
  const Parameters& params() const { return params_; }

  // Scales a portion of |src_texture| and draws the result into |dest_texture|
  // at offset (0, 0). Returns true to indicate success, or false if this
  // GLScaler is not valid.
  //
  // |src_texture_size| is the full, allocated size of the |src_texture|. This
  // is required for computing texture coordinate transforms (and only because
  // the OpenGL ES 2.0 API lacks the ability to query this info).
  //
  // |src_offset| is the offset in the source texture corresponding to point
  // (0,0) in the source/output coordinate spaces. This prevents the need for
  // extra texture copies just to re-position the source coordinate system.
  //
  // |output_rect| selects the region to draw (in the scaled, not the source,
  // coordinate space). This is used to save work in cases where only a portion
  // needs to be re-scaled. The implementation will back-compute, internally, to
  // determine the region of the |src_texture| to sample.
  //
  // WARNING: The output will always be placed at (0, 0) in the |dest_texture|,
  // and not at |output_rect.origin()|.
  //
  // Note that the |src_texture| will have the min/mag filter set to GL_LINEAR
  // and wrap_s/t set to CLAMP_TO_EDGE in this call.
  bool Scale(GLuint src_texture,
             const gfx::Size& src_texture_size,
             const gfx::Vector2d& src_offset,
             GLuint dest_texture,
             const gfx::Rect& output_rect) WARN_UNUSED_RESULT {
    return ScaleToMultipleOutputs(src_texture, src_texture_size, src_offset,
                                  dest_texture, 0, output_rect);
  }

  // Same as above, but for use cases where there are two output textures drawn
  // (see Parameters::ExportFormat).
  bool ScaleToMultipleOutputs(GLuint src_texture,
                              const gfx::Size& src_texture_size,
                              const gfx::Vector2d& src_offset,
                              GLuint dest_texture_0,
                              GLuint dest_texture_1,
                              const gfx::Rect& output_rect) WARN_UNUSED_RESULT;

  // Returns true if from:to represent the same scale ratio as that specified in
  // |params|.
  static bool ParametersHasSameScaleRatio(const Parameters& params,
                                          const gfx::Vector2d& from,
                                          const gfx::Vector2d& to);

  // Returns true if configuring a GLScaler with either |a| or |b| will produce
  // identical behaviors and results.
  static bool ParametersAreEquivalent(const Parameters& a, const Parameters& b);

 private:
  friend class GLScalerOverscanPixelTest;
  friend class GLScalerShaderPixelTest;
  friend VIZ_COMMON_EXPORT std::ostream& operator<<(std::ostream&,
                                                    const GLScaler&);

  using GLES2Interface = gpu::gles2::GLES2Interface;

  enum Axis { HORIZONTAL = 0, VERTICAL = 1 };

  // The shaders used by each stage in the scaling pipeline.
  enum class Shader : int8_t {
    BILINEAR,
    BILINEAR2,
    BILINEAR3,
    BILINEAR4,
    BILINEAR2X2,
    BICUBIC_UPSCALE,
    BICUBIC_HALF_1D,
    PLANAR_CHANNEL_0,
    PLANAR_CHANNEL_1,
    PLANAR_CHANNEL_2,
    PLANAR_CHANNEL_3,
    I422_NV61_MRT,
    DEINTERLEAVE_PAIRWISE_MRT,
  };

  // A cached, re-usable shader program that performs one step in the scaling
  // pipeline.
  class VIZ_COMMON_EXPORT ShaderProgram {
   public:
    ShaderProgram(GLES2Interface* gl,
                  Shader shader,
                  GLenum texture_type,
                  const gfx::ColorTransform* color_transform,
                  const GLenum swizzle[2]);
    ~ShaderProgram();

    Shader shader() const { return shader_; }
    GLenum texture_type() const { return texture_type_; }

    // UseProgram must be called with GL_ARRAY_BUFFER bound to a vertex
    // attribute buffer. |src_texture_size| is the size of the entire source
    // texture, regardless of which region is to be sampled. |src_rect| is the
    // source region, not including overscan pixels past the edges.
    // |primary_axis| determines whether multiple texture samplings occur in one
    // direction or the other (for some shaders). Note that this cannot
    // necessarily be determined by just comparing the src and dst sizes.
    // |flip_y| causes the |src_rect| to be scanned upside-down, to produce a
    // vertically-flipped result.
    void UseProgram(const gfx::Size& src_texture_size,
                    const gfx::RectF& src_rect,
                    const gfx::Size& dst_size,
                    Axis primary_axis,
                    bool flip_y);

    // GL_ARRAY_BUFFER data that must be bound when drawing with a
    // ShaderProgram. These are the vertex attributes that will sweep the entire
    // source area when executing the program. They represent triangle strip
    // coordinates: The first two columns are (x,y) values interpolated to
    // produce the vertex coordinates in object space, while the latter two
    // columns are (s,t) values interpolated to produce the texture coordinates
    // that correspond to the vertex coordinates.
    static const GLfloat kVertexAttributes[16];

   private:
    GLES2Interface* const gl_;
    const Shader shader_;
    const GLenum texture_type_;

    // A program for copying a source texture into a destination texture.
    const GLuint program_;

    // The location of the position in the program.
    GLint position_location_ = -1;
    // The location of the texture coordinate in the program.
    GLint texcoord_location_ = -1;
    // The location of the source texture in the program.
    GLint texture_location_ = -1;
    // The location of the texture coordinate of the source rectangle in the
    // program.
    GLint src_rect_location_ = -1;
    // Location of size of source image in pixels.
    GLint src_pixelsize_location_ = -1;
    // Location of vector for scaling ratio between source and dest textures.
    GLint scaling_vector_location_ = -1;

    DISALLOW_COPY_AND_ASSIGN(ShaderProgram);
  };

  // One scaling stage in a chain of scaler pipeline stages. Each ScalerStage
  // owns the previous ScalerStage in the chain: At execution time, a "working
  // backwards" approach is used: The previous "input" stage renders an
  // intermediate result that will be used as input for the current stage.
  //
  // Each ScalerStage caches textures and framebuffers to avoid reallocating
  // them for each separate image scaling, which can be expensive on some
  // platforms/drivers.
  class VIZ_COMMON_EXPORT ScalerStage {
   public:
    ScalerStage(GLES2Interface* gl,
                Shader shader,
                Axis primary_axis,
                const gfx::Vector2d& scale_from,
                const gfx::Vector2d& scale_to);
    ~ScalerStage();

    Shader shader() const { return shader_; }
    const gfx::Vector2d& scale_from() const { return scale_from_; }
    const gfx::Vector2d& scale_to() const { return scale_to_; }

    ScalerStage* input_stage() const { return input_stage_.get(); }
    void set_input_stage(std::unique_ptr<ScalerStage> stage) {
      input_stage_ = std::move(stage);
    }
    std::unique_ptr<ScalerStage> take_input_stage() {
      return std::move(input_stage_);
    }

    ShaderProgram* shader_program() const { return program_; }
    void set_shader_program(ShaderProgram* program) { program_ = program; }

    bool is_flipped_source() const { return is_flipped_source_; }
    void set_is_flipped_source(bool flipped) { is_flipped_source_ = flipped; }

    bool flip_output() const { return flip_output_; }
    void set_flip_output(bool flip) { flip_output_ = flip; }

    void ScaleToMultipleOutputs(GLuint src_texture,
                                gfx::Size src_texture_size,
                                const gfx::Vector2d& src_offset,
                                GLuint dest_texture_0,
                                GLuint dest_texture_1,
                                const gfx::Rect& output_rect);

   private:
    friend class GLScalerOverscanPixelTest;

    // Returns the given |output_rect| mapped to the input stage's coordinate
    // system.
    gfx::RectF ToSourceRect(const gfx::Rect& output_rect) const;

    // Returns the given |source_rect| padded to include the overscan pixels the
    // shader program will access.
    gfx::Rect ToInputRect(gfx::RectF source_rect) const;

    // Generates the intermediate texture and/or re-defines it if its size has
    // changed.
    void EnsureIntermediateTextureDefined(const gfx::Size& size);

    GLES2Interface* const gl_;
    const Shader shader_;
    const Axis primary_axis_;
    const gfx::Vector2d scale_from_;
    const gfx::Vector2d scale_to_;

    std::unique_ptr<ScalerStage> input_stage_;
    ShaderProgram* program_ = nullptr;
    bool is_flipped_source_ = false;
    bool flip_output_ = false;

    GLuint intermediate_texture_ = 0;
    gfx::Size intermediate_texture_size_;
    GLuint dest_framebuffer_ = 0;

    DISALLOW_COPY_AND_ASSIGN(ScalerStage);
  };

  // ContextLostObserver implementation.
  void OnContextLost() final;

  // Returns a cached ShaderProgram, creating one on-demand if necessary.
  ShaderProgram* GetShaderProgram(Shader shader,
                                  GLenum texture_type,
                                  const gfx::ColorTransform* color_transform,
                                  const GLenum swizzle[2]);

  // Create a scaling chain using the bilinear shaders.
  static std::unique_ptr<ScalerStage> CreateAGoodScalingChain(
      gpu::gles2::GLES2Interface* gl,
      const gfx::Vector2d& scale_from,
      const gfx::Vector2d& scale_to);

  // Create a scaling chain using the bicubic shaders.
  static std::unique_ptr<ScalerStage> CreateTheBestScalingChain(
      gpu::gles2::GLES2Interface* gl,
      const gfx::Vector2d& scale_from,
      const gfx::Vector2d& scale_to);

  // Modifies |chain| by appending an export stage, to rearrange the image data
  // according to the requested |export_format|. In some cases, this will delete
  // the final stage in |chain| before appending the export stage.
  static std::unique_ptr<ScalerStage> MaybeAppendExportStage(
      gpu::gles2::GLES2Interface* gl,
      std::unique_ptr<ScalerStage> chain,
      Parameters::ExportFormat export_format);

  // Returns the other of the two axes.
  static Axis TheOtherAxis(Axis axis);

  // Returns the name of the |shader| in string form, for logging purposes.
  static const char* GetShaderName(Shader shader);

  // Returns true if the given |gl| context mentions all of |names| in its
  // extensions string.
  static bool AreAllGLExtensionsPresent(gpu::gles2::GLES2Interface* gl,
                                        const std::vector<std::string>& names);

  // The provider of the GL context. This is non-null while the GL context is
  // valid and GLScaler is observing for context loss.
  ContextProvider* context_provider_;

  // Set by Configure() to the resolved set of Parameters.
  Parameters params_;

  // If set to true, half-float textures are supported. This is lazy-initialized
  // by SupportsPreciseColorManagement().
  mutable base::Optional<bool> supports_half_floats_;

  // The maximum number of simultaneous draw buffers, lazy-initialized by
  // GetMaxDrawBuffersSupported(). -1 means "not yet known."
  mutable int max_draw_buffers_ = -1;

  // Cache of ShaderPrograms. The cache key consists of fields that correspond
  // to the arguments of GetShaderProgram(): the shader, the texture format, the
  // source and output color spaces (color transform), and the two swizzles.
  using ShaderCacheKey = std::
      tuple<Shader, GLenum, gfx::ColorSpace, gfx::ColorSpace, GLenum, GLenum>;
  std::map<ShaderCacheKey, ShaderProgram> shader_programs_;

  // The GL_ARRAY_BUFFER that holds the vertices and the texture coordinates
  // data for sweeping the source area when a ScalerStage draws a quad (to
  // execute its shader program).
  GLuint vertex_attributes_buffer_ = 0;

  // The chain of ScalerStages.
  std::unique_ptr<ScalerStage> chain_;

  // The color space in which the scaling stages operate.
  gfx::ColorSpace scaling_color_space_;

  DISALLOW_COPY_AND_ASSIGN(GLScaler);
};

// For logging.
VIZ_COMMON_EXPORT std::ostream& operator<<(std::ostream& out,
                                           const GLScaler& scaler);

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_GL_SCALER_H_
