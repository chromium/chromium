// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/viz/test/gl_scaler_test_util.h"

#include <algorithm>
#include <cmath>
#include <ostream>

#include "base/check_op.h"
#include "base/notreached.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/geometry/rect.h"

namespace viz {

using ColorBar = GLScalerTestUtil::ColorBar;

// static
SkBitmap GLScalerTestUtil::AllocateRGBABitmap(const gfx::Size& size) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::Make(size.width(), size.height(),
                                       kRGBA_8888_SkColorType,
                                       kUnpremul_SkAlphaType));
  return bitmap;
}

// static
uint8_t GLScalerTestUtil::ToClamped255(float value) {
  value = std::fma(value, 255.0f, 0.5f /* rounding */);
  return base::saturated_cast<uint8_t>(value);
}

// static
std::vector<ColorBar> GLScalerTestUtil::GetScaledSMPTEColorBars(
    const gfx::Size& size) {
  std::vector<ColorBar> rects;
  const SkScalar scale_x =
      static_cast<SkScalar>(size.width()) / kSMPTEFullSize.width();
  const SkScalar scale_y =
      static_cast<SkScalar>(size.height()) / kSMPTEFullSize.height();
  for (const auto& cr : kSMPTEColorBars) {
    const SkRect rect =
        SkRect{cr.rect.fLeft * scale_x, cr.rect.fTop * scale_y,
               cr.rect.fRight * scale_x, cr.rect.fBottom * scale_y};
    rects.push_back(ColorBar{rect.round(), cr.color});
  }
  return rects;
}

// static
SkBitmap GLScalerTestUtil::CreateSMPTETestImage(const gfx::Size& size) {
  SkBitmap result = AllocateRGBABitmap(size);

  // Set all pixels to a color that should not exist in the result. Later, a
  // sanity-check will ensure all pixels have been overwritten.
  constexpr SkColor4f kDeadColor = SkColor4f{0.678f, 0.745f, 0.937f, 0.871f};
  result.eraseColor(kDeadColor);

  // Set the pixels corresponding to each color bar.
  for (const auto& cr : GetScaledSMPTEColorBars(size)) {
    result.erase(cr.color, cr.rect);
  }

  // Validate that every pixel in the result bitmap has been touched by one of
  // the color bars.
  for (int y = 0; y < result.height(); ++y) {
    for (int x = 0; x < result.width(); ++x) {
      if (result.getColor4f(x, y) == kDeadColor) {
        NOTREACHED_IN_MIGRATION()
            << "TEST BUG: Error creating SMPTE test image. Bad size ("
            << size.ToString() << ")?";
        return result;
      }
    }
  }

  return result;
}

// static
bool GLScalerTestUtil::LooksLikeSMPTETestImage(const SkBitmap& image,
                                               const gfx::Size& src_size,
                                               const gfx::Rect& src_rect,
                                               int fuzzy_pixels,
                                               float* max_color_diff) {
  if (image.width() <= 0 || image.height() <= 0) {
    return false;
  }

  const SkScalar offset_x = static_cast<SkScalar>(src_rect.x());
  const SkScalar offset_y = static_cast<SkScalar>(src_rect.y());
  const SkScalar scale_x =
      static_cast<SkScalar>(image.width()) / src_rect.width();
  const SkScalar scale_y =
      static_cast<SkScalar>(image.height()) / src_rect.height();
  float measured_max_diff = 0.0f;
  for (const auto& cr : GetScaledSMPTEColorBars(src_size)) {
    const SkIRect offset_rect = cr.rect.makeOffset(-offset_x, -offset_y);
    const SkIRect rect =
        SkRect{offset_rect.fLeft * scale_x, offset_rect.fTop * scale_y,
               offset_rect.fRight * scale_x, offset_rect.fBottom * scale_y}
            .round()
            .makeInset(fuzzy_pixels, fuzzy_pixels);
    for (int y = std::max(0, rect.fTop),
             y_end = std::min(image.height(), rect.fBottom);
         y < y_end; ++y) {
      for (int x = std::max(0, rect.fLeft),
               x_end = std::min(image.width(), rect.fRight);
           x < x_end; ++x) {
        const SkColor4f actual = image.getColor4f(x, y);
        measured_max_diff =
            std::max({measured_max_diff, std::abs((cr.color.fR) - (actual.fR)),
                      std::abs((cr.color.fG) - (actual.fG)),
                      std::abs((cr.color.fB) - (actual.fB)),
                      std::abs((cr.color.fA) - (actual.fA))});
      }
    }
  }

  if (max_color_diff) {
    const int threshold = *max_color_diff;
    *max_color_diff = measured_max_diff;
    return measured_max_diff <= threshold;
  }
  return measured_max_diff == 0.0f;
}

// static
SkBitmap GLScalerTestUtil::CreateCyclicalTestImage(
    const gfx::Size& size,
    CyclicalPattern pattern,
    const std::vector<SkColor4f>& cycle,
    size_t rotation) {
  CHECK(!cycle.empty());

  // Map SkColors to RGBA data. Also, applies the cycle |rotation| to simplify
  // the rest of the code below.
  std::vector<uint32_t> cycle_as_rgba(cycle.size());
  for (size_t i = 0; i < cycle.size(); ++i) {
    const SkColor4f color = cycle[(i + rotation) % cycle.size()];
    cycle_as_rgba[i] = ((static_cast<int>(color.fR * 255) << kRedShift) |
                        (static_cast<int>(color.fG * 255) << kGreenShift) |
                        (static_cast<int>(color.fB * 255) << kBlueShift) |
                        (static_cast<int>(color.fA * 255) << kAlphaShift));
  }

  SkBitmap result = AllocateRGBABitmap(size);
  switch (pattern) {
    case HORIZONTAL_STRIPES:
      for (int y = 0; y < size.height(); ++y) {
        uint32_t* const pixels = result.getAddr32(0, y);
        const uint32_t stripe_rgba = cycle_as_rgba[y % cycle_as_rgba.size()];
        for (int x = 0; x < size.width(); ++x) {
          pixels[x] = stripe_rgba;
        }
      }
      break;

    case VERTICAL_STRIPES:
      for (int y = 0; y < size.height(); ++y) {
        uint32_t* const pixels = result.getAddr32(0, y);
        for (int x = 0; x < size.width(); ++x) {
          pixels[x] = cycle_as_rgba[x % cycle_as_rgba.size()];
        }
      }
      break;

    case STAGGERED:
      for (int y = 0; y < size.height(); ++y) {
        uint32_t* const pixels = result.getAddr32(0, y);
        for (int x = 0; x < size.width(); ++x) {
          pixels[x] = cycle_as_rgba[(x + y) % cycle_as_rgba.size()];
        }
      }
      break;
  }

  return result;
}

// static
gfx::ColorSpace GLScalerTestUtil::DefaultRGBColorSpace() {
  return gfx::ColorSpace::CreateSRGB();
}

// static
gfx::ColorSpace GLScalerTestUtil::DefaultYUVColorSpace() {
  return gfx::ColorSpace::CreateREC709();
}

// static
void GLScalerTestUtil::ConvertRGBABitmapToYUV(SkBitmap* image) {
  CHECK_EQ(image->colorType(), kRGBA_8888_SkColorType);

  const auto transform = gfx::ColorTransform::NewColorTransform(
      DefaultRGBColorSpace(), DefaultYUVColorSpace());

  // Loop, transforming one row of pixels at a time.
  std::vector<gfx::ColorTransform::TriStim> stims(image->width());
  for (int y = 0; y < image->height(); ++y) {
    uint32_t* const pixels = image->getAddr32(0, y);
    for (int x = 0; x < image->width(); ++x) {
      stims[x].set_x(((pixels[x] >> kRedShift) & 0xff) / 255.0f);
      stims[x].set_y(((pixels[x] >> kGreenShift) & 0xff) / 255.0f);
      stims[x].set_z(((pixels[x] >> kBlueShift) & 0xff) / 255.0f);
    }
    transform->Transform(stims.data(), stims.size());
    for (int x = 0; x < image->width(); ++x) {
      pixels[x] = ((ToClamped255(stims[x].x()) << kRedShift) |
                   (ToClamped255(stims[x].y()) << kGreenShift) |
                   (ToClamped255(stims[x].z()) << kBlueShift) |
                   (((pixels[x] >> kAlphaShift) & 0xff) << kAlphaShift));
    }
  }
}

// static
SkBitmap GLScalerTestUtil::CopyAndConvertToRGBA(const SkBitmap& bitmap) {
  SkBitmap result;
  result.allocPixels(SkImageInfo::Make(
      bitmap.dimensions(), kRGBA_8888_SkColorType, kPremul_SkAlphaType));

  SkPixmap pixmap;
  bool success = bitmap.peekPixels(&pixmap) && result.writePixels(pixmap, 0, 0);
  CHECK(success);

  return result;
}

// static
void GLScalerTestUtil::SwizzleBitmap(SkBitmap* image) {
  for (int y = 0; y < image->height(); ++y) {
    uint32_t* const pixels = image->getAddr32(0, y);
    for (int x = 0; x < image->width(); ++x) {
      pixels[x] = ((((pixels[x] >> kBlueShift) & 0xff) << kRedShift) |
                   (((pixels[x] >> kGreenShift) & 0xff) << kGreenShift) |
                   (((pixels[x] >> kRedShift) & 0xff) << kBlueShift) |
                   (((pixels[x] >> kAlphaShift) & 0xff) << kAlphaShift));
    }
  }
}

// static
SkBitmap GLScalerTestUtil::CreatePackedPlanarBitmap(const SkBitmap& source,
                                                    int channel) {
  CHECK_EQ(source.width() % 4, 0);
  SkBitmap result =
      AllocateRGBABitmap(gfx::Size(source.width() / 4, source.height()));

  constexpr int kShiftForChannel[4] = {kRedShift, kGreenShift, kBlueShift,
                                       kAlphaShift};
  const int shift = kShiftForChannel[channel];
  for (int y = 0; y < result.height(); ++y) {
    const uint32_t* const src = source.getAddr32(0, y);
    uint32_t* const dst = result.getAddr32(0, y);
    for (int x = 0; x < result.width(); ++x) {
      //     (src[0..3])         (dst)
      // RGBA RGBA RGBA RGBA --> RRRR   (if channel is 0)
      dst[x] = ((((src[x * 4 + 0] >> shift) & 0xff) << kRedShift) |
                (((src[x * 4 + 1] >> shift) & 0xff) << kGreenShift) |
                (((src[x * 4 + 2] >> shift) & 0xff) << kBlueShift) |
                (((src[x * 4 + 3] >> shift) & 0xff) << kAlphaShift));
    }
  }
  return result;
}

// static
void GLScalerTestUtil::UnpackPlanarBitmap(const SkBitmap& plane,
                                          int channel,
                                          SkBitmap* out) {
  // The heuristic below auto-adapts to subsampled plane sizes. However, there
  // are two cricital requirements: 1) |plane| cannot be empty; 2) |plane| must
  // have a size that cleanly unpacks to |out|'s size.
  CHECK_GT(plane.width(), 0);
  CHECK_GT(plane.height(), 0);
  const int col_sampling_ratio = out->width() / plane.width();
  CHECK_EQ(out->width() % plane.width(), 0)
      << " out->width()=" << out->width()
      << ", plane.width()=" << plane.width();
  CHECK_GT(col_sampling_ratio, 0);
  const int row_sampling_ratio = out->height() / plane.height();
  CHECK_EQ(out->height() % plane.height(), 0);
  CHECK_GT(row_sampling_ratio, 0);
  const int ch_sampling_ratio = col_sampling_ratio / 4;
  CHECK_GT(ch_sampling_ratio, 0);

  // These determine which single byte in each of |out|'s uint32_t-valued pixels
  // will be modified.
  constexpr int kShiftForChannel[4] = {kRedShift, kGreenShift, kBlueShift,
                                       kAlphaShift};
  const int output_shift = kShiftForChannel[channel];
  const uint32_t output_retain_mask = ~(UINT32_C(0xff) << output_shift);

  // Iterate over the pixels of |out|, sampling each of the 4 components of each
  // of |plane|'s pixels.
  for (int y = 0; y < out->height(); ++y) {
    const uint32_t* const src = plane.getAddr32(0, y / row_sampling_ratio);
    uint32_t* const dst = out->getAddr32(0, y);
    for (int x = 0; x < out->width(); ++x) {
      // Zero-out the existing byte (e.g., if channel==1, then "RGBA" → "R0BA").
      dst[x] &= output_retain_mask;

      // From |src|, grab one of "XYZW". Then, copy it to the target byte in
      // |dst| (e.g., if x_src_ch=3, then grab "W" from |src|, and |dst| changes
      // from "R0BA" to "RWBA").
      const int x_src = x / col_sampling_ratio;
      const int x_src_ch = (x / ch_sampling_ratio) % 4;
      dst[x] |= ((src[x_src] >> kShiftForChannel[x_src_ch]) & 0xff)
                << output_shift;
    }
  }
}

// static
void GLScalerTestUtil::UnpackUVBitmap(const SkBitmap& plane, SkBitmap* out) {
  CHECK_GT(plane.width(), 0);
  CHECK_GT(plane.height(), 0);

  // The format of data in |plane| is as follows:
  //
  //    UVUV UVUV UVUV
  //    UVUV UVUV UVUV
  //    UVUV UVUV UVUV
  //
  // One row of source of size |plane.width()| contains information about
  // 2 * |plane.width()| texels, and we want to sample them to populate
  // one row of |out->width()|, so we'll sample at the rate of
  //
  //     col_sampling_ratio = out->width() / (2 * plane.width())
  //
  // This will allow us to find which "half-texel" in the source row to look at
  // for a corresponding texel in the output.

  const int col_sampling_ratio = out->width() / (2 * plane.width());
  CHECK_EQ(out->width() % (2 * plane.width()), 0);
  CHECK_GT(col_sampling_ratio, 0);

  const int row_sampling_ratio = out->height() / plane.height();
  CHECK_EQ(out->height() % plane.height(), 0);
  CHECK_GT(row_sampling_ratio, 0);

  // These determine which single byte in each of |out|'s uint32_t-valued pixels
  // will be modified.
  constexpr int kShiftForChannel[4] = {kRedShift, kGreenShift, kBlueShift,
                                       kAlphaShift};
  constexpr uint32_t zero_green_mask = ~(UINT32_C(0xff) << kGreenShift);
  constexpr uint32_t zero_blue_mask = ~(UINT32_C(0xff) << kBlueShift);
  constexpr uint32_t zero_green_blue_mask = zero_green_mask & zero_blue_mask;

  // Iterate over all the pixels of |out|, calculate where the data for that
  // said pixel is.
  for (int y = 0; y < out->height(); ++y) {
    const uint32_t* const src = plane.getAddr32(0, y / row_sampling_ratio);
    uint32_t* const dst = out->getAddr32(0, y);
    for (int x = 0; x < out->width(); ++x) {
      // Zero-out the existing byte (e.g., "RGBA" → "R00A").
      dst[x] &= zero_green_blue_mask;

      // Find which half-texel to look at:
      const int src_half_texel = x / col_sampling_ratio;
      // The |src_half_texel| belongs to a texel:
      const int src_texel = src_half_texel / 2;
      // The |src_half_texel| spans 2 channels and starts at channel:
      const int src_channel = 2 * (src_half_texel % 2);

      // Grab the 2 consecutive channels, starting at |src_channel|:
      dst[x] |= ((src[src_texel] >> kShiftForChannel[src_channel]) & 0xff)
                << kGreenShift;
      dst[x] |= ((src[src_texel] >> kShiftForChannel[src_channel + 1]) & 0xff)
                << kBlueShift;
    }
  }
}

// static
SkBitmap GLScalerTestUtil::CreateVerticallyFlippedBitmap(
    const SkBitmap& source) {
  SkBitmap bitmap;
  bitmap.allocPixels(source.info());
  CHECK_EQ(bitmap.rowBytes(), source.rowBytes());
  for (int y = 0; y < bitmap.height(); ++y) {
    const int src_y = bitmap.height() - y - 1;
    memcpy(bitmap.getAddr32(0, y), source.getAddr32(0, src_y),
           bitmap.rowBytes());
  }
  return bitmap;
}

// The area and color of the bars in a 1920x1080 HD SMPTE color bars test image
// (https://commons.wikimedia.org/wiki/File:SMPTE_Color_Bars_16x9.svg). The gray
// linear gradient bar is defined as half solid 0-level black and half solid
// full-intensity white).
const ColorBar GLScalerTestUtil::kSMPTEColorBars[30] = {
    {{0, 240, 630}, SkColor4f{0.4f, 0.4f, 0.4f, 1.0f}},
    {{240, 0, 445, 630}, SkColor4f{0.749f, 0.749f, 0.749f, 1.0f}},
    {{0, 651, 445, 630}, SkColor4f{0.749f, 0.749f, 0.0f, 1.0f}},
    {{0, 857, 651, 630}, SkColor4f{0.0f, 0.749f, 0.749f, 1.0f}},
    {{0, 857, 630, 1063}, SkColor4f{0.0f, 0.749f, 0.0f, 1.0f}},
    {{0, 1269, 630, 1063}, SkColor4f{0.749f, 0.0f, 0.749f, 1.0f}},
    {{0, 1475, 1269, 630}, SkColor4f{0.749f, 0.0f, 0.0f, 1.0f}},
    {{0, 1475, 1680, 630}, SkColor4f{0.0f, 0.0f, 0.749f, 1.0f}},
    {{1680, 0, 1920, 630}, SkColor4f{0.4f, 0.4f, 0.4f, 1.0f}},
    {{0, 240, 630, 720}, SkColor4f{0.0f, 1.0f, 1.0f, 1.0f}},
    {{240, 445, 630, 720}, SkColor4f{0.0f, 0.129f, 0.298f, 1.0f}},
    {{1680, 445, 630, 720}, SkColor4f{0.749f, 0.749f, 0.749f, 1.0f}},
    {{1680, 1920, 630, 720}, SkColor4f{0.0f, 0.0f, 1.0f, 1.0f}},
    {{0, 240, 810, 720}, SkColor4f{1.0f, 1.0f, 0.0f, 1.0f}},
    {{240, 810, 445, 720}, SkColor4f{0.196f, 0.0f, 0.416f, 1.0f}},
    {{720, 810, 445, 1063}, SkColor4f{0.0f, 0.0f, 0.0f, 1.0f}},
    {{720, 810, 1680, 1063}, SkColor4f{1.0f, 1.0f, 1.0f, 1.0f}},
    {{1680, 810, 1920, 720}, SkColor4f{1.0f, 0.0f, 0.0f, 1.0f}},
    {{0, 240, 810, 1080}, SkColor4f{0.149f, 0.149f, 0.149f, 1.0f}},
    {{240, 810, 1080, 549}, SkColor4f{0.0f, 0.0f, 0.0f, 1.0f}},
    {{960, 810, 1080, 549}, SkColor4f{1.0f, 1.0f, 1.0f, 1.0f}},
    {{960, 810, 1131, 1080}, SkColor4f{0.0f, 0.0f, 0.0f, 1.0f}},
    {{1200, 810, 1131, 1080}, SkColor4f{0.0f, 0.0f, 0.0f, 1.0f}},
    {{1200, 810, 1268, 1080}, SkColor4f{0.0f, 0.0f, 0.0f, 1.0f}},
    {{1080, 1337, 810, 1268}, SkColor4f{0.02f, 0.02f, 0.02f, 1.0f}},
    {{1080, 1337, 810, 1405}, SkColor4f{0.0f, 0.0f, 0.0f, 1.0f}},
    {{1080, 1474, 810, 1405}, SkColor4f{0.039f, 0.039f, 0.039f, 1.0f}},
    {{1680, 1474, 810, 1080}, SkColor4f{0.0f, 0.0f, 0.0f, 1.0f}},
    {{1680, 810, 1080, 1920}, SkColor4f{0.149f, 0.149f, 0.149f, 1.0f}},
};

constexpr gfx::Size GLScalerTestUtil::kSMPTEFullSize;

GLScalerTestTextureHelper::GLScalerTestTextureHelper(
    gpu::gles2::GLES2Interface* gl)
    : gl_(gl) {
  CHECK(gl_);
}

GLScalerTestTextureHelper::~GLScalerTestTextureHelper() {
  gl_->DeleteTextures(textures_to_delete_.size(), textures_to_delete_.data());
  textures_to_delete_.clear();
}

GLuint GLScalerTestTextureHelper::CreateTexture(const gfx::Size& size) {
  GLuint texture = 0;
  gl_->GenTextures(1, &texture);
  gl_->BindTexture(GL_TEXTURE_2D, texture);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl_->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width(), size.height(), 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  gl_->BindTexture(GL_TEXTURE_2D, 0);

  if (texture) {
    textures_to_delete_.push_back(texture);
  }

  return texture;
}

GLuint GLScalerTestTextureHelper::UploadTexture(const SkBitmap& bitmap) {
  CHECK_EQ(bitmap.colorType(), kRGBA_8888_SkColorType);

  GLuint texture = 0;
  gl_->GenTextures(1, &texture);
  gl_->BindTexture(GL_TEXTURE_2D, texture);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl_->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap.width(), bitmap.height(), 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, bitmap.getAddr32(0, 0));
  gl_->BindTexture(GL_TEXTURE_2D, 0);

  if (texture) {
    textures_to_delete_.push_back(texture);
  }

  return texture;
}

SkBitmap GLScalerTestTextureHelper::DownloadTexture(GLuint texture,
                                                    const gfx::Size& size) {
  GLuint framebuffer = 0;
  gl_->GenFramebuffers(1, &framebuffer);
  gl_->BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            texture, 0);
  SkBitmap result = GLScalerTestUtil::AllocateRGBABitmap(size);
  gl_->ReadPixels(0, 0, size.width(), size.height(), GL_RGBA, GL_UNSIGNED_BYTE,
                  result.getAddr32(0, 0));
  gl_->DeleteFramebuffers(1, &framebuffer);
  return result;
}

}  // namespace viz
