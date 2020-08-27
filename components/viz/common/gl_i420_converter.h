// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_GL_I420_CONVERTER_H_
#define COMPONENTS_VIZ_COMMON_GL_I420_CONVERTER_H_

#include <memory>

#include "base/macros.h"
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
// from interleaved RGBA to I420 planes. The I420 format consists of three
// planes of image data: the Y (luma) plane at full size, plus U and V (chroma)
// planes at half-width and half-height. There are two possible modes of
// operation (auto-detected at runtime):
//
// The faster, multiple rendering target (MRT) path: If the platform supports
// MRTs (most of the GPUs in use today), scaling and conversion is a two step
// process:
//
//   Step 1: Produce NV61 format output, a luma plane and a UV-interleaved
//   image. The luma plane is the same as the desired I420 luma plane. Note,
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
//   Step 2: Derives the two I420 chroma planes from the UV-interleaved image
//   from Step 1. This step separates the U and V pixels into separate planes,
//   and also scales the height by half. This produces the desired I420 chroma
//   planes.
//
//     (chroma, interleaved)     (two chroma planes)
//           UVUV UVUV
//           UVUV UVUV       -->   { UUUU + VVVV }
//           UVUV UVUV               UUUU   VVVV
//           UVUV UVUV
//
// The non-MRT path: For platforms that can only render to a single target at a
// time. This first scales the source to its final size and color-converts,
// transforming an RGBA input into a YUVA output. Then, it scans the YUVA image
// three times to generate each of the Y+U+V planes.
//
// Texture packing: OpenGLES2 treats all of the input and output textures as
// RGBA format. See comments for the Convert() method, which explains how the
// planar image data is packed into GL_RGBA textures, how the output textures
// should be sized, and why there are alignment requirements when specifying the
// output rect.
class VIZ_COMMON_EXPORT GLI420Converter : public ContextLostObserver {
 public:
  // GLI420Converter uses the exact same parameters as GLScaler.
  using Parameters = GLScaler::Parameters;

  explicit GLI420Converter(ContextProvider* context_provider);
  ~GLI420Converter() final;

  // Returns true if the GL context provides the necessary support for enabling
  // precise color management (see Parameters::enable_precise_color_management).
  bool SupportsPreciseColorManagement() const {
    return step1_.SupportsPreciseColorManagement();
  }

  // [Re]Configure the converter with the given |new_params|. Returns true on
  // success, or false on failure. If |new_params| does not specify an
  // |output_color_space|, it will be default to REC709.
  bool Configure(const Parameters& new_params) WARN_UNUSED_RESULT;

  // Returns the currently-configured and resolved Parameters. Results are
  // undefined if Configure() has never been called successfully.
  const Parameters& params() const { return params_; }

  // Scales a portion of |src_texture|, then format-converts it to three I420
  // planes, placing the results into |yuv_textures| at offset (0, 0). Returns
  // true to indicate success, or false if this GLI420Converter is not valid.
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
  // The |yuv_textures| are packed with planar data, meaning that each RGBA quad
  // contains four pixel values: R is pixel 0, G is pixel 1, and so on. This
  // makes it trivial to read-back the textures from a pixel buffer as a
  // sequence of unsigned bytes. Thus, the output texture for the Y plane should
  // be defined as GL_RGBA and be at least 1/4 the width of that specified in
  // |aligned_output_rect|. Similarly, the output textures for the U and V
  // planes should be defined as GL_RGBA and have at least 1/8 the width and 1/2
  // the height of |aligned_output_rect|.
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
               const GLuint yuv_textures[3]);

  // Returns the smallest Rect that encloses |rect| and lays on aligned
  // boundaries, as required by the |aligned_output_rect| argument passed to
  // Convert(). The horizontal coordinates will always be a multiple of 8, and
  // the vertical coordinates a multiple of 2.
  static gfx::Rect ToAlignedRect(const gfx::Rect& rect);

  // Returns true if configuring a GLI420Converter with either |a| or |b| will
  // produce identical behaviors and results.
  static bool ParametersAreEquivalent(const Parameters& a, const Parameters& b);

 private:
  friend class GLI420ConverterPixelTest;

  GLI420Converter(ContextProvider* context_provider, bool allow_mrt_path);

  bool is_using_mrt_path() const { return !step3_; }

  // Creates or re-defines the intermediate texture, to ensure a texture of the
  // given |required| size is defined.
  void EnsureIntermediateTextureDefined(const gfx::Size& required);

  // ContextLostObserver implementation.
  void OnContextLost() final;

  // The provider of the GL context. This is non-null while the GL context is
  // valid and GLI420Converter is observing for context loss.
  ContextProvider* context_provider_;

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
  //   * MRT path: Separates-out the U and V planes (and scales height by half).
  //   * Non-MRT path: Extracts the luma plane.
  GLScaler step2_;

  // Steps 3 and 4 are used by the non-MRT path only, to extract the two chroma
  // planes from |intermediate_texture_|.
  std::unique_ptr<GLScaler> step3_;
  std::unique_ptr<GLScaler> step4_;

  // The Parameters that were provided to the last successful Configure() call.
  Parameters params_;

  DISALLOW_COPY_AND_ASSIGN(GLI420Converter);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_GL_I420_CONVERTER_H_
