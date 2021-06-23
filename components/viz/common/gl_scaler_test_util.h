// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_GL_SCALER_TEST_UTIL_H_
#define COMPONENTS_VIZ_COMMON_GL_SCALER_TEST_UTIL_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_transform.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class Rect;
}

namespace viz {

// A collection of utility functions used in the GLScaler-related pixel tests.
class GLScalerTestUtil {
 public:
  struct ColorBar {
    SkIRect rect;
    SkColor color;
  };

  // The patterns that can be created by CreateCyclicalTestImage().
  enum CyclicalPattern {
    HORIZONTAL_STRIPES,
    VERTICAL_STRIPES,
    STAGGERED,
  };

  // Returns an SkBitmap with pixels allocated, having a RGBA format with
  // unpremultiplied alpha.
  static SkBitmap AllocateRGBABitmap(const gfx::Size& size);

  // Returns a |value| in the range [0.0,1.0] as an unsigned integer in the
  // range [0,255].
  static uint8_t ToClamped255(float value);

  // Return the SMPTE color bars that make up a test image, scaled to an image
  // of the given |size|.
  static std::vector<ColorBar> GetScaledSMPTEColorBars(const gfx::Size& size);

  // Create a SMPTE color bar test image, scaled to the given |size|.
  static SkBitmap CreateSMPTETestImage(const gfx::Size& size);

  // Returns true if the given |image| looks similar-enough to a scaled version
  // of a SMPTE color bar test image that was originally of |src_size| and
  // cropped to |src_rect|. |fuzzy_pixels| is used to ignore a border of pixels
  // surrounding each color bar, since the scaling algorithms may blend
  // in-between colors where the bars touch. |max_color_diff| controls what
  // "similar-enough" means: 0 for "exact," or otherwise some positive value
  // specifying the maximum color value difference; and is updated with the
  // the actual maximum.
  static bool LooksLikeSMPTETestImage(const SkBitmap& image,
                                      const gfx::Size& src_size,
                                      const gfx::Rect& src_rect,
                                      int fuzzy_pixels,
                                      int* max_color_diff);

  // Returns an image of the given |size| with the colors in |cycle| used to
  // generate a striped or staggered |pattern|. |rotation| specifies which index
  // in the |cycle| to start with.
  static SkBitmap CreateCyclicalTestImage(const gfx::Size& size,
                                          CyclicalPattern pattern,
                                          const std::vector<SkColor>& cycle,
                                          size_t rotation);

  // Returns the RGB/YUV color spaces used by default when color space
  // conversion is requested.
  static gfx::ColorSpace DefaultRGBColorSpace();
  static gfx::ColorSpace DefaultYUVColorSpace();

  // Performs an in-place transform of the given |image| from
  // DefaultRGBColorSpace() to DefaultYUVColorSpace(). The color channels (plus
  // one alpha) remain interleaved (i.e., no pixel blending or format transform
  // is being done).
  static void ConvertBitmapToYUV(SkBitmap* image);

  // Performs an in-place swizzling of the red and blue color channels in the
  // given |image|.
  static void SwizzleBitmap(SkBitmap* image);

  // Returns a bitmap consisting of one color channel from every 4 pixels in a
  // |source| bitmap packed into a single quad. Thus, the resulting bitmap will
  // have 1/4 the width of the source bitmap. This is used to create the
  // expected output of the single-channel export shaders, and thus can be
  // considered a reference implementation of that.
  static SkBitmap CreatePackedPlanarBitmap(const SkBitmap& source, int channel);

  // Performs the inverse operation to CreatePackedPlanarBitmap(). This takes
  // all of the data in |plane| and uses it to populate a single color channel
  // of all the pixels of |out|. The |plane| can be a full-size or half-size
  // (subsampled) plane.
  static void UnpackPlanarBitmap(const SkBitmap& plane,
                                 int channel,
                                 SkBitmap* out);

  // Returns the |source| bitmap, but with its content vertically flipped.
  static SkBitmap CreateVerticallyFlippedBitmap(const SkBitmap& source);

  // Loads a PNG test image from the test directory, and converts it to the same
  // SkImageInfo format used by AllocateRGBABitmap() (i.e., GL_RGBA byte order).
  static SkBitmap LoadPNGTestImage(const std::string& basename);

  // The area and color of the bars in a 1920x1080 HD SMPTE color bars test
  // image (https://commons.wikimedia.org/wiki/File:SMPTE_Color_Bars_16x9.svg).
  // The gray linear gradient bar is defined as half solid 0-level black and
  // half solid full-intensity white).
  static const ColorBar kSMPTEColorBars[30];
  static constexpr gfx::Size kSMPTEFullSize = gfx::Size(1920, 1080);

#ifdef SK_CPU_BENDIAN
  // Bit shift offsets (within a uint32_t RGBA quad) to access each color
  // channel's byte.
  static constexpr int kRedShift = 24;
  static constexpr int kGreenShift = 16;
  static constexpr int kBlueShift = 8;
  static constexpr int kAlphaShift = 0;
#else
  static constexpr int kRedShift = 0;
  static constexpr int kGreenShift = 8;
  static constexpr int kBlueShift = 16;
  static constexpr int kAlphaShift = 24;
#endif
};

// A helper for tests to create textures, and download/upload RGBA textures
// to/from SkBitmaps. All textures created by this helper will be deleted when
// its destructor is invoked.
class GLScalerTestTextureHelper {
 public:
  // |gl| context must outlive this instance.
  explicit GLScalerTestTextureHelper(gpu::gles2::GLES2Interface* gl);

  ~GLScalerTestTextureHelper();

  // Creates a fully-defined RGBA texture of the given |size|, returning its GL
  // name.
  GLuint CreateTexture(const gfx::Size& size);

  // Uploads the given RGBA |bitmap| to the GPU into a new texture, returning
  // its GL name.
  GLuint UploadTexture(const SkBitmap& bitmap);

  // Reads-back the |texture| which is of the given |size|, returning the result
  // as a RGBA SkBitmap.
  SkBitmap DownloadTexture(GLuint texture, const gfx::Size& size);

 private:
  gpu::gles2::GLES2Interface* const gl_;
  std::vector<GLuint> textures_to_delete_;

  DISALLOW_COPY_AND_ASSIGN(GLScalerTestTextureHelper);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_GL_SCALER_TEST_UTIL_H_
