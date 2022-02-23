// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_GL_NV12_CONVERTER_H_
#define COMPONENTS_VIZ_COMMON_GL_NV12_CONVERTER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/viz/common/gl_scaler.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/viz_common_export.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class Rect;
class Vector2d;
}  // namespace gfx

namespace viz {

class ContextProvider;

// A convenience wrapper around GLScaler that also reformats the scaler's output
// from interleaved RGBA to NV12 planes. The NV12 format consists of two
// planes of image data: the Y (luma) plane at full size, plus interleaved UV
// (chroma) plane at half-width and half-height. There are two possible modes of
// operation (auto-detected at runtime):
//
// The faster, multiple rendering target (MRT) path: If the platform supports
// MRTs (most of the GPUs in use today), scaling and conversion is a two step
// process:
//
//   Step 1: Produce NV61 format output, a luma plane and a UV-interleaved
//   image. The luma plane is the same as the desired NV12 luma plane. Note,
//   that the UV image is of half-width but not yet half-height.
//
//               (interleaved quads)
//     RGBA RGBA RGBA RGBA RGBA RGBA RGBA RGBA
//     RGBA RGBA RGBA RGBA RGBA RGBA RGBA RGBA
//     RGBA RGBA RGBA RGBA RGBA RGBA RGBA RGBA
//     RGBA RGBA RGBA RGBA RGBA RGBA RGBA RGBA
//       |
//       |     (luma plane)  (chroma, interleaved)
//       |       YYYY YYYY      UVUV UVUV
//       +---> { YYYY YYYY  +   UVUV UVUV }
//               YYYY YYYY      UVUV UVUV
//               YYYY YYYY      UVUV UVUV
//
//   Step 2: Downscales the chroma plane.
//
//     (chroma, interleaved)     (chroma, interleaved)
//           UVUV UVUV
//           UVUV UVUV     -->   UVUV UVUV
//           UVUV UVUV           UVUV UVUV
//           UVUV UVUV
//
// The non-MRT path: For platforms that can only render to a single target at a
// time. This first scales the source to its final size and color-converts,
// transforming an RGBA input into a YUVx output. Then, it scans the YUVA image
// two times to generate each of the Y+UV planes.
//
// Texture packing: OpenGLES2 treats all of the input and output textures as
// RGBA format. See comments for the Convert() method, which explains how the
// planar image data is packed into GL_RGBA textures, how the output textures
// should be sized, and why there are alignment requirements when specifying the
// output rect.
class VIZ_COMMON_EXPORT GLNV12Converter final : public ContextLostObserver {
 public:
  // GLNV12Converter uses the exact same parameters as GLScaler.
  using Parameters = GLScaler::Parameters;

  explicit GLNV12Converter(ContextProvider* context_provider);
  ~GLNV12Converter() final;

  // Returns true if the GL context provides the necessary support for enabling
  // precise color management (see Parameters::enable_precise_color_management).
  bool SupportsPreciseColorManagement() const {
    return step1_.SupportsPreciseColorManagement();
  }

  // [Re]Configure the converter with the given |new_params|. Returns true on
  // success, or false on failure. If |new_params| does not specify an
  // |output_color_space|, it will be default to REC709.
  [[nodiscard]] bool Configure(const Parameters& new_params);

  // Returns the currently-configured and resolved Parameters. Results are
  // undefined if Configure() has never been called successfully.
  const Parameters& params() const { return params_; }

  // Scales a portion of |src_texture|, then format-converts it to two NV12
  // planes, placing the results into |yuv_textures| at offset (0, 0). Returns
  // true to indicate success, or false if this GLNV12Converter is not valid.
  //
  // |src_texture_size| is the full, allocated size of the |src_texture|. This
  // is required for computing texture coordinate transforms (and only because
  // the OpenGL ES 2.0 API lacks the ability to query this info).
  //
  // |src_offset| is the offset in the source texture corresponding to point
  // (0,0) in the source/output coordinate spaces. This prevents the need for
  // extra texture copies just to re-position the source coordinate system.
  //
  // |aligned_output_rect| selects the region to draw (in the scaled, not the
  // source, coordinate space). This is used to save work in cases where only a
  // portion needs to be re-scaled. Because of the way the planar image data is
  // packed in the output textures, the output rect's coordinates must be
  // aligned (see ToAlignedRect() below).
  //
  // The |yuv_textures| are packed with planar data. Depending on the plane,
  // the packing is as follows:
  // - for Y plane, each RGBA quad contains four pixel values: R is pixel 0,
  //   G is pixel 1, and so on.
  // - for UV plane, each RGBA quad contains 2 pixel values: RG are UV values
  //   for pixel 0, BA are UV values for pixel 1, and so on.
  // This makes it trivial to read-back the textures from a pixel buffer as a
  // sequence of unsigned bytes. Thus, the output texture for the Y plane should
  // be defined as GL_RGBA and be at least 1/4 the width of that specified in
  // |aligned_output_rect|. Similarly, the output texture for the UV plane
  // should be defined as GL_RGBA and have at least 1/4 the width [1]
  // and 1/2 the height [2] of |aligned_output_rect|.
  //
  // [1] 1/4 width  = 1/4     // we pack 4 values per pixel
  //                * 1/2     // chroma planes are subsampled
  //                * 2       // we pack 2 chroma planes
  //                * width
  // [2] 1/2 height = 1/2     // chroma planes are subsampled
  //                * height
  //
  // WARNING: The output will always be placed at (0, 0) in the output textures,
  // and not at |aligned_output_rect.origin()|.
  //
  // Note that the |src_texture| will have the min/mag filter set to GL_LINEAR
  // and wrap_s/t set to CLAMP_TO_EDGE in this call.
  bool Convert(GLuint src_texture,
               const gfx::Size& src_texture_size,
               const gfx::Vector2d& src_offset,
               const gfx::Rect& aligned_output_rect,
               const GLuint yuv_textures[2]);

  // Returns the smallest Rect that encloses |rect| and lays on aligned
  // boundaries, as required by the |aligned_output_rect| argument passed to
  // Convert(). The horizontal coordinates will always be a multiple of 4, and
  // the vertical coordinates a multiple of 2.
  static gfx::Rect ToAlignedRect(const gfx::Rect& rect);

  // Returns true if configuring a GLNV12Converter with either |a| or |b| will
  // produce identical behaviors and results.
  static bool ParametersAreEquivalent(const Parameters& a, const Parameters& b);

  static std::unique_ptr<GLNV12Converter> CreateConverterForTest(
      ContextProvider* context_provider,
      bool allow_mrt_path);

 private:
  GLNV12Converter(ContextProvider* context_provider, bool allow_mrt_path);

  bool is_using_mrt_path() const { return !step3_; }

  // Creates or re-defines the intermediate texture, to ensure a texture of the
  // given |required| size is defined.
  void EnsureIntermediateTextureDefined(const gfx::Size& required);

  // ContextLostObserver implementation.
  void OnContextLost() final;

  // The provider of the GL context. This is non-null while the GL context is
  // valid and GLNV12Converter is observing for context loss.
  raw_ptr<ContextProvider> context_provider_;

  // Scales the source content and produces either:
  //   * MRT path: NV61-format output in two textures.
  //   * Non-MRT path: YUVA interleaved output in one texture.
  GLScaler step1_;

  // Holds the results from executing the first-stage |scaler_|, and is read by
  // the other scalers:
  //   * MRT path: This holds the UV-interleaved data (2nd rendering target).
  //   * Non-MRT path: The scaled YUVA interleaved data.
  GLuint intermediate_texture_ = 0;
  gfx::Size intermediate_texture_size_;

  // Step 2 operation using the |intermediate_texture_| as input:
  //   * MRT path: Scales the height by half.
  //   * Non-MRT path: Extracts the luma plane.
  GLScaler step2_;

  // Steps 3 is used by the non-MRT path only, to extract the interleaved chroma
  // planes from |intermediate_texture_|.
  std::unique_ptr<GLScaler> step3_;

  // The Parameters that were provided to the last successful Configure() call.
  Parameters params_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_GL_NV12_CONVERTER_H_
