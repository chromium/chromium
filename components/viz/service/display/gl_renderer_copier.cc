// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/gl_renderer_copier.h"

#include <cstring>
#include <utility>

#include "base/bind.h"
#include "base/containers/cxx20_erase.h"
#include "base/memory/raw_ptr.h"
#include "base/process/memory.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/gl_i420_converter.h"
#include "components/viz/common/gl_nv12_converter.h"
#include "components/viz/common/gl_scaler.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/service/display/texture_deleter.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/geometry/size.h"

// Syntactic sugar to DCHECK that two sizes are equal.
#define DCHECK_SIZE_EQ(a, b)                                \
  DCHECK((a) == (b)) << #a " != " #b ": " << (a).ToString() \
                     << " != " << (b).ToString()

namespace viz {

using ResultFormat = CopyOutputRequest::ResultFormat;
using ResultDestination = CopyOutputRequest::ResultDestination;

namespace {

constexpr int kRGBABytesPerPixel = 4;

// Returns the source property of the |request|, if it is set. Otherwise,
// returns an empty token. This is needed because CopyOutputRequest will crash
// if source() is called when !has_source().
base::UnguessableToken SourceOf(const CopyOutputRequest& request) {
  return request.has_source() ? request.source() : base::UnguessableToken();
}

// Creates a new texture, binds it to the GL_TEXTURE_2D target, and initializes
// its default parameters.
GLuint CreateDefaultTexture2D(gpu::gles2::GLES2Interface* gl) {
  GLuint result = 0;
  gl->GenTextures(1, &result);
  gl->BindTexture(GL_TEXTURE_2D, result);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return result;
}

// Creates or re-creates a texture, only if needed, to ensure a texture of the
// given |required| size is defined. |texture| and |size| are I/O parameters,
// read to determine what to do and updated if any changes are made.
void EnsureTextureDefinedWithSize(gpu::gles2::GLES2Interface* gl,
                                  const gfx::Size& required,
                                  GLuint* texture,
                                  gfx::Size* size) {
  if (*texture != 0 && *size == required)
    return;
  if (*texture == 0) {
    *texture = CreateDefaultTexture2D(gl);
  } else {
    gl->BindTexture(GL_TEXTURE_2D, *texture);
  }
  gl->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, required.width(), required.height(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  *size = required;
}

// Returns parameters for the scaler to scale/transform the image in the source
// framebuffer to meet the requirements of the |request|.
GLScaler::Parameters CreateScalerParameters(
    const CopyOutputRequest& request,
    const gfx::ColorSpace& source_color_space,
    const gfx::ColorSpace& output_color_space,
    bool flipped_source) {
  GLScaler::Parameters params;

  params.scale_from = request.scale_from();
  params.scale_to = request.scale_to();
  params.source_color_space = source_color_space;
  params.output_color_space = output_color_space;
  // For downscaling, use the GOOD quality setting (appropriate for
  // thumbnailing); and, for upscaling, use the BEST quality.
  const bool is_downscale_in_both_dimensions =
      request.scale_to().x() < request.scale_from().x() &&
      request.scale_to().y() < request.scale_from().y();
  params.quality = is_downscale_in_both_dimensions
                       ? GLScaler::Parameters::Quality::GOOD
                       : GLScaler::Parameters::Quality::BEST;
  params.is_flipped_source = flipped_source;

  return params;
}

// Returns the specified offset in the form of a pointer for OpenGL's
// `glReadPixels` call. This is not a valid memory address and the pointer
// should never be dereferenced.
uint8_t* GetOffsetPointer(int offset) {
  uint8_t* result = reinterpret_cast<uint8_t*>(0);
  result += offset;
  return result;
}

}  // namespace

GLRendererCopier::GLRendererCopier(ContextProvider* context_provider,
                                   TextureDeleter* texture_deleter)
    : context_provider_(context_provider), texture_deleter_(texture_deleter) {}

GLRendererCopier::~GLRendererCopier() {
  for (auto& entry : cache_)
    entry.second->Free(context_provider_->ContextGL());
}

void GLRendererCopier::CopyFromTextureOrFramebuffer(
    std::unique_ptr<CopyOutputRequest> request,
    const copy_output::RenderPassGeometry& geometry,
    GLenum internal_format,
    GLuint framebuffer_texture,
    const gfx::Size& framebuffer_texture_size,
    bool flipped_source,
    const gfx::ColorSpace& framebuffer_color_space) {
  const gfx::Rect& result_rect = geometry.result_selection;

  // If we can't convert |color_space| to a SkColorSpace for SkBitmap copy
  // requests (e.g. PIECEWISE_HDR), fallback to a color transform to sRGB
  // before returning the copy result.
  gfx::ColorSpace dest_color_space = framebuffer_color_space;
  if (!framebuffer_color_space.ToSkColorSpace() &&
      request->result_format() == ResultFormat::RGBA &&
      request->result_destination() == ResultDestination::kSystemMemory) {
    dest_color_space = gfx::ColorSpace::CreateSRGB();
  }
  // Fast-Path: If no transformation is necessary and no new textures need to be
  // generated, read-back directly from the currently-bound framebuffer.
  if (request->result_format() == ResultFormat::RGBA &&
      request->result_destination() == ResultDestination::kSystemMemory &&
      framebuffer_color_space == dest_color_space && !request->is_scaled()) {
    StartReadbackFromFramebuffer(std::move(request), geometry.readback_offset,
                                 flipped_source, false, result_rect,
                                 dest_color_space);
    return;
  }

  gfx::Rect sampling_rect = geometry.sampling_bounds;

  const base::UnguessableToken requester = SourceOf(*request);
  std::unique_ptr<ReusableThings> things =
      TakeReusableThingsOrCreate(requester);

  // Determine the source texture: This is either the one attached to the
  // framebuffer, or a copy made from the framebuffer. Its format will be the
  // same as |internal_format|.
  //
  // TODO(crbug/767221): All of this (including some texture copies) wouldn't be
  // necessary if we could query whether the currently-bound framebuffer has a
  // texture attached to it, and just source from that texture directly (i.e.,
  // using glGetFramebufferAttachmentParameteriv() and
  // glGetTexLevelParameteriv(GL_TEXTURE_WIDTH/HEIGHT)).
  GLuint source_texture;
  gfx::Size source_texture_size;
  if (framebuffer_texture != 0) {
    source_texture = framebuffer_texture;
    source_texture_size = framebuffer_texture_size;
  } else {
    auto* const gl = context_provider_->ContextGL();
    if (things->fb_copy_texture == 0) {
      things->fb_copy_texture = CreateDefaultTexture2D(gl);
      things->fb_copy_texture_internal_format = static_cast<GLenum>(GL_NONE);
      things->fb_copy_texture_size = gfx::Size();
    } else {
      gl->BindTexture(GL_TEXTURE_2D, things->fb_copy_texture);
    }
    if (things->fb_copy_texture_internal_format == internal_format &&
        things->fb_copy_texture_size == sampling_rect.size()) {
      // Copy the framebuffer pixels without redefining the texture.
      gl->CopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, sampling_rect.x(),
                            sampling_rect.y(), sampling_rect.width(),
                            sampling_rect.height());
    } else {
      // Copy the framebuffer pixels into a newly-defined texture.
      gl->CopyTexImage2D(GL_TEXTURE_2D, 0, internal_format, sampling_rect.x(),
                         sampling_rect.y(), sampling_rect.width(),
                         sampling_rect.height(), 0);
      things->fb_copy_texture_internal_format = internal_format;
      things->fb_copy_texture_size = sampling_rect.size();
    }
    source_texture = things->fb_copy_texture;
    source_texture_size = sampling_rect.size();
    sampling_rect.set_origin(gfx::Point());
  }

  // Revert the Y-flipping of the sampling rect coordinates for GLScaler, which
  // always assumes the source offset is assuming a origin-at-top-left
  // coordinate space.
  if (flipped_source) {
    sampling_rect.set_y(source_texture_size.height() - sampling_rect.bottom());
  }

  switch (request->result_format()) {
    case ResultFormat::RGBA:

      switch (request->result_destination()) {
        case ResultDestination::kSystemMemory:
          EnsureTextureDefinedWithSize(
              context_provider_->ContextGL(), result_rect.size(),
              &things->result_texture, &things->result_texture_size);
          RenderResultTexture(*request, flipped_source, framebuffer_color_space,
                              dest_color_space, source_texture,
                              source_texture_size, sampling_rect, result_rect,
                              things->result_texture, things.get());
          StartReadbackFromTexture(std::move(request), result_rect,
                                   dest_color_space, things.get());
          break;
        case ResultDestination::kNativeTextures:
          RenderAndSendTextureResult(std::move(request), flipped_source,
                                     framebuffer_color_space, dest_color_space,
                                     source_texture, source_texture_size,
                                     sampling_rect, result_rect, things.get());
          break;
      }

      break;

    case ResultFormat::I420_PLANES: {
      // The optimized single-copy path, provided by GLPixelBufferI420Result,
      // requires that the result be accessed via a task in the same task runner
      // sequence as the GLRendererCopier. Since I420_PLANES requests are meant
      // to be VIZ-internal, this is an acceptable limitation to enforce.
      if (!request->SendsResultsInCurrentSequence()) {
        request->set_result_task_runner(base::SequencedTaskRunnerHandle::Get());
      }

      const gfx::Rect aligned_rect = RenderI420Textures(
          *request, flipped_source, framebuffer_color_space, source_texture,
          source_texture_size, sampling_rect, result_rect, things.get());
      StartI420ReadbackFromTextures(std::move(request), aligned_rect,
                                    result_rect, things.get());
      break;
    }

    case ResultFormat::NV12_PLANES: {
      // The optimized single-copy path, provided by GLPixelBufferNV12Result,
      // requires that the result be accessed via a task in the same task runner
      // sequence as the GLRendererCopier. Since NV12_PLANES requests are meant
      // to be VIZ-internal, this is an acceptable limitation to enforce.
      if (!request->SendsResultsInCurrentSequence()) {
        request->set_result_task_runner(base::SequencedTaskRunnerHandle::Get());
      }

      const gfx::Rect aligned_rect = RenderNV12Textures(
          *request, flipped_source, framebuffer_color_space, source_texture,
          source_texture_size, sampling_rect, result_rect, things.get());
      StartNV12ReadbackFromTextures(std::move(request), aligned_rect,
                                    result_rect, things.get());

      break;
    }
  }

  StashReusableThingsOrDelete(requester, std::move(things));
}

void GLRendererCopier::FreeUnusedCachedResources() {
  ++purge_counter_;

  // Purge all cache entries that should no longer be kept alive, freeing any
  // resources they held.
  const auto IsTooOld = [this](const decltype(cache_)::value_type& entry) {
    return static_cast<int32_t>(purge_counter_ -
                                entry.second->purge_count_at_last_use) >=
           kKeepalivePeriod;
  };
  for (auto& entry : cache_) {
    if (IsTooOld(entry))
      entry.second->Free(context_provider_->ContextGL());
  }
  base::EraseIf(cache_, IsTooOld);
}

void GLRendererCopier::RenderResultTexture(
    const CopyOutputRequest& request,
    bool flipped_source,
    const gfx::ColorSpace& source_color_space,
    const gfx::ColorSpace& dest_color_space,
    GLuint source_texture,
    const gfx::Size& source_texture_size,
    const gfx::Rect& sampling_rect,
    const gfx::Rect& result_rect,
    GLuint result_texture,
    ReusableThings* things) {
  DCHECK_EQ(request.result_format(), ResultFormat::RGBA);

  GLScaler::Parameters params = CreateScalerParameters(
      request, source_color_space, dest_color_space, flipped_source);

  if (request.result_destination() == ResultDestination::kSystemMemory) {
    // Render the result in top-down row order, and swizzle, within the GPU so
    // these things don't have to be done, less efficiently, on the CPU later.
    params.flip_output = flipped_source;
    params.swizzle[0] =
        ShouldSwapRedAndBlueForBitmapReadback() ? GL_BGRA_EXT : GL_RGBA;
  } else {
    // Texture results are always in bottom-up row order.
    DCHECK_EQ(request.result_destination(), ResultDestination::kNativeTextures);
    params.flip_output = !flipped_source;
    DCHECK_EQ(params.swizzle[0], static_cast<GLenum>(GL_RGBA));
  }

  if (!things->scaler)
    things->scaler = std::make_unique<GLScaler>(context_provider_);
  if (!GLScaler::ParametersAreEquivalent(params, things->scaler->params())) {
    const bool is_configured = things->scaler->Configure(params);
    // GLRendererCopier should never use illegal or unsupported options, nor
    // be using GLScaler with an invalid GL context.
    DCHECK(is_configured);
  }

  const bool success = things->scaler->Scale(
      source_texture, source_texture_size, sampling_rect.OffsetFromOrigin(),
      result_texture, result_rect);
  DCHECK(success);
}

gfx::Rect GLRendererCopier::RenderI420Textures(
    const CopyOutputRequest& request,
    bool flipped_source,
    const gfx::ColorSpace& source_color_space,
    GLuint source_texture,
    const gfx::Size& source_texture_size,
    const gfx::Rect& sampling_rect,
    const gfx::Rect& result_rect,
    ReusableThings* things) {
  DCHECK_EQ(request.result_format(), ResultFormat::I420_PLANES);

  auto* const gl = context_provider_->ContextGL();

  // Compute required Y/U/V texture sizes and re-define them, if necessary. See
  // class comments for GLI420Converter for an explanation of how planar data is
  // packed into RGBA textures.
  const gfx::Rect aligned_rect = GLI420Converter::ToAlignedRect(result_rect);
  const gfx::Size required_luma_size(aligned_rect.width() / kRGBABytesPerPixel,
                                     aligned_rect.height());
  const gfx::Size required_chroma_size(required_luma_size.width() / 2,
                                       required_luma_size.height() / 2);

  EnsureTextureDefinedWithSize(gl, required_luma_size, &things->yuv_textures[0],
                               &things->texture_sizes[0]);
  EnsureTextureDefinedWithSize(gl, required_chroma_size,
                               &things->yuv_textures[1],
                               &things->texture_sizes[1]);
  EnsureTextureDefinedWithSize(gl, required_chroma_size,
                               &things->yuv_textures[2],
                               &things->texture_sizes[2]);

  GLI420Converter::Parameters params =
      CreateScalerParameters(request, source_color_space,
                             gfx::ColorSpace::CreateREC709(), flipped_source);
  // I420 readback assumes content is in top-down row order. Also, set the
  // output swizzle to match the readback format so that image bitmaps don't
  // have to be byte-order-swizzled on the CPU later.
  params.flip_output = flipped_source;
  params.swizzle[0] = GetOptimalReadbackFormat();

  if (!things->i420_converter) {
    things->i420_converter =
        std::make_unique<GLI420Converter>(context_provider_);
  }
  if (!GLI420Converter::ParametersAreEquivalent(
          params, things->i420_converter->params())) {
    const bool is_configured = things->i420_converter->Configure(params);
    // GLRendererCopier should never use illegal or unsupported options, nor
    // be using GLI420Converter with an invalid GL context.
    DCHECK(is_configured);
  }

  const bool success = things->i420_converter->Convert(
      source_texture, source_texture_size, sampling_rect.OffsetFromOrigin(),
      aligned_rect, things->yuv_textures.data());
  DCHECK(success);

  return aligned_rect;
}

gfx::Rect GLRendererCopier::RenderNV12Textures(
    const CopyOutputRequest& request,
    bool flipped_source,
    const gfx::ColorSpace& source_color_space,
    GLuint source_texture,
    const gfx::Size& source_texture_size,
    const gfx::Rect& sampling_rect,
    const gfx::Rect& result_rect,
    ReusableThings* things) {
  DCHECK_EQ(request.result_format(), ResultFormat::NV12_PLANES);

  auto* const gl = context_provider_->ContextGL();

  // Compute required Y/UV texture sizes and re-define them, if necessary. See
  // class comments for GLNV12Converter for an explanation of how planar data is
  // packed into RGBA textures.
  const gfx::Rect aligned_rect = GLNV12Converter::ToAlignedRect(result_rect);

  const gfx::Size required_luma_size(aligned_rect.width() / kRGBABytesPerPixel,
                                     aligned_rect.height());
  const gfx::Size required_chroma_size(required_luma_size.width(),
                                       required_luma_size.height() / 2);

  EnsureTextureDefinedWithSize(gl, required_luma_size, &things->yuv_textures[0],
                               &things->texture_sizes[0]);

  EnsureTextureDefinedWithSize(gl, required_chroma_size,
                               &things->yuv_textures[1],
                               &things->texture_sizes[1]);

  GLNV12Converter::Parameters params =
      CreateScalerParameters(request, source_color_space,
                             gfx::ColorSpace::CreateREC709(), flipped_source);

  // NV12 readback assumes content is in top-down row order. Also, set the
  // output swizzle to match the readback format so that image bitmaps don't
  // have to be byte-order-swizzled on the CPU later.
  params.flip_output = flipped_source;
  params.swizzle[0] = GetOptimalReadbackFormat();

  if (!things->nv12_converter) {
    things->nv12_converter =
        std::make_unique<GLNV12Converter>(context_provider_);
  }
  if (!GLNV12Converter::ParametersAreEquivalent(
          params, things->nv12_converter->params())) {
    const bool is_configured = things->nv12_converter->Configure(params);
    // GLRendererCopier should never use illegal or unsupported options, nor
    // be using GLNV12Converter with an invalid GL context.
    DCHECK(is_configured);
  }

  const bool success = things->nv12_converter->Convert(
      source_texture, source_texture_size, sampling_rect.OffsetFromOrigin(),
      aligned_rect, things->yuv_textures.data());
  DCHECK(success);

  return aligned_rect;
}

void GLRendererCopier::StartReadbackFromTexture(
    std::unique_ptr<CopyOutputRequest> request,
    const gfx::Rect& result_rect,
    const gfx::ColorSpace& color_space,
    ReusableThings* things) {
  DCHECK_EQ(request->result_format(), ResultFormat::RGBA);
  DCHECK_EQ(request->result_destination(), ResultDestination::kSystemMemory);

  auto* const gl = context_provider_->ContextGL();
  if (things->readback_framebuffer == 0) {
    gl->GenFramebuffers(1, &things->readback_framebuffer);
  }
  gl->BindFramebuffer(GL_FRAMEBUFFER, things->readback_framebuffer);
  gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           things->result_texture, 0);
  StartReadbackFromFramebuffer(std::move(request), gfx::Vector2d(), false,
                               ShouldSwapRedAndBlueForBitmapReadback(),
                               result_rect, color_space);
}

namespace {

// This is the type of CopyOutputResult we send for RGBA readback. The
// constructor is called during on GLRendererCopier::FinishReadPixelsWorkflow(),
// thus it always have access to the GLContext. The ReadRGBAPlane and destructor
// are called asynchronously, and thus might not have access to the GLContext if
// it has been destroyed in the meantime. We use the WeakPtr to the
// GLRendererCopier as an indicator that the GLContext is still alive. If the
// access to the GLContext is lost, we treat the copy output as failed.
class GLPixelBufferRGBAResult final : public CopyOutputResult {
 public:
  GLPixelBufferRGBAResult(const gfx::Rect& result_rect,
                          const gfx::ColorSpace& color_space,
                          base::WeakPtr<GLRendererCopier> copier_weak_ptr,
                          ContextProvider* context_provider,
                          GLuint transfer_buffer,
                          bool is_upside_down,
                          bool swap_red_and_blue)
      : CopyOutputResult(CopyOutputResult::Format::RGBA,
                         CopyOutputResult::Destination::kSystemMemory,
                         result_rect,
                         /*needs_lock_for_bitmap=*/false),
        color_space_(color_space),
        copier_weak_ptr_(std::move(copier_weak_ptr)),
        context_provider_(std::move(context_provider)),
        transfer_buffer_(transfer_buffer),
        is_upside_down_(is_upside_down),
        swap_red_and_blue_(swap_red_and_blue) {}

  ~GLPixelBufferRGBAResult() final {
    if (transfer_buffer_ && copier_weak_ptr_) {
      context_provider_->ContextGL()->DeleteBuffers(1, &transfer_buffer_);
    }
  }

  bool ReadRGBAPlane(uint8_t* dest, int stride) const final {
    // If the GLRendererCopier is gone, this implies the display compositor
    // which contains the GLContext is gone. Regard this copy output readback as
    // failed.
    if (!copier_weak_ptr_)
      return false;

    const int src_bytes_per_row = size().width() * kRGBABytesPerPixel;
    DCHECK_GE(stride, src_bytes_per_row);

    // No need to read from GPU memory if a cached bitmap already exists.
    if (rect().IsEmpty() || cached_bitmap()->readyToDraw())
      return CopyOutputResult::ReadRGBAPlane(dest, stride);

    auto* const gl = context_provider_->ContextGL();
    gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, transfer_buffer_);
    const uint8_t* pixels = static_cast<uint8_t*>(gl->MapBufferCHROMIUM(
        GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, GL_READ_ONLY));
    if (pixels) {
      if (is_upside_down_) {
        dest += (size().height() - 1) * stride;
        stride = -stride;
      }
      const uint8_t* src = pixels;
      if (swap_red_and_blue_) {
        for (int y = 0; y < size().height();
             ++y, src += src_bytes_per_row, dest += stride) {
          for (int x = 0; x < kRGBABytesPerPixel * size().width();
               x += kRGBABytesPerPixel) {
            dest[x + 2] = src[x + 0];
            dest[x + 1] = src[x + 1];
            dest[x + 0] = src[x + 2];
            dest[x + 3] = src[x + 3];
          }
        }
      } else {
        libyuv::CopyPlane(src, src_bytes_per_row, dest, stride,
                          src_bytes_per_row, size().height());
      }
      gl->UnmapBufferCHROMIUM(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM);
    }
    gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);
    return !!pixels;
  }

  gfx::ColorSpace GetRGBAColorSpace() const final { return color_space_; }

  // This method is always called on the same sequence as the GLRendererCopier.
  // This method will be inside Viz and has access to the WeakPtr of the
  // GLRendererCopier to check whether we still have the access to an alive
  // GLContext.
  const SkBitmap& AsSkBitmap() const final {
    if (rect().IsEmpty())
      return *cached_bitmap();  // Return "null" bitmap for empty result.

    if (cached_bitmap()->readyToDraw())
      return *cached_bitmap();

    if (!copier_weak_ptr_)
      return *cached_bitmap();

    SkBitmap result_bitmap;
    // size() was clamped to render pass or framebuffer size. If we can't
    // allocate it then OOM.
    auto info = SkImageInfo::MakeN32Premul(
        size().width(), size().height(), GetRGBAColorSpace().ToSkColorSpace());
    if (!result_bitmap.tryAllocPixels(info, info.minRowBytes()))
      base::TerminateBecauseOutOfMemory(info.computeMinByteSize());

    ReadRGBAPlane(static_cast<uint8_t*>(result_bitmap.getPixels()),
                  result_bitmap.rowBytes());
    *cached_bitmap() = result_bitmap;
    // Now that we have a cached bitmap, no need to read from GPU memory
    // anymore.
    context_provider_->ContextGL()->DeleteBuffers(1, &transfer_buffer_);
    transfer_buffer_ = 0;

    return *cached_bitmap();
  }

 private:
  const gfx::ColorSpace color_space_;
  base::WeakPtr<GLRendererCopier> copier_weak_ptr_;
  raw_ptr<ContextProvider> context_provider_;
  mutable GLuint transfer_buffer_;
  const bool is_upside_down_;
  const bool swap_red_and_blue_;
};
}  // namespace

GLRendererCopier::ReadPixelsWorkflow::ReadPixelsWorkflow(
    std::unique_ptr<CopyOutputRequest> copy_request,
    const gfx::Vector2d& readback_offset,
    bool flipped_source,
    bool swap_red_and_blue,
    const gfx::Rect& result_rect,
    const gfx::ColorSpace& color_space,
    ContextProvider* context_provider,
    GLenum readback_format)
    : copy_request(std::move(copy_request)),
      flipped_source(flipped_source),
      swap_red_and_blue(swap_red_and_blue),
      result_rect(result_rect),
      color_space(color_space),
      context_provider_(context_provider) {
  DCHECK(readback_format == GL_RGBA || readback_format == GL_BGRA_EXT);
  DCHECK(context_provider_);
  auto* const gl = context_provider_->ContextGL();

  // Create a buffer for the pixel transfer.
  gl->GenBuffers(1, &transfer_buffer);
  gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, transfer_buffer);
  gl->BufferData(
      GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM,
      (result_rect.size().GetCheckedArea() * kRGBABytesPerPixel).ValueOrDie(),
      nullptr, GL_STREAM_READ);

  // Execute an asynchronous read-pixels operation, with a query that triggers
  // when Finish() should be run.
  gl->GenQueriesEXT(1, &query_);
  gl->BeginQueryEXT(GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM, query_);
  gl->ReadPixels(readback_offset.x(), readback_offset.y(), result_rect.width(),
                 result_rect.height(), readback_format, GL_UNSIGNED_BYTE,
                 nullptr);
  gl->EndQueryEXT(GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM);
  gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);
}

GLRendererCopier::ReadPixelsWorkflow::~ReadPixelsWorkflow() {
  auto* const gl = context_provider_->ContextGL();
  gl->DeleteQueriesEXT(1, &query_);
  if (transfer_buffer)
    gl->DeleteBuffers(1, &transfer_buffer);
}

// Callback for the asynchronous glReadPixels(). The pixels are read from the
// transfer buffer, and a CopyOutputResult is sent to the requestor. This would
// mark this workflow as finished, and the workflow will be cleared later.
void GLRendererCopier::FinishReadPixelsWorkflow(ReadPixelsWorkflow* workflow) {
  auto result = std::make_unique<GLPixelBufferRGBAResult>(
      workflow->result_rect, workflow->color_space, weak_factory_.GetWeakPtr(),
      context_provider_, workflow->transfer_buffer, workflow->flipped_source,
      workflow->swap_red_and_blue);
  workflow->transfer_buffer = 0;  // Ownerhip was transferred to the result.
  if (!workflow->copy_request->SendsResultsInCurrentSequence()) {
    // Force readback into a SkBitmap now, because after PostTask we don't
    // have access to |context_provider_|.
    auto scoped_bitmap = result->ScopedAccessSkBitmap();
    auto bitmap = scoped_bitmap.bitmap();
  }
  workflow->copy_request->SendResult(std::move(result));
  const auto it =
      std::find_if(read_pixels_workflows_.begin(), read_pixels_workflows_.end(),
                   [workflow](auto& ptr) { return ptr.get() == workflow; });
  DCHECK(it != read_pixels_workflows_.end());
  read_pixels_workflows_.erase(it);
}

void GLRendererCopier::StartReadbackFromFramebuffer(
    std::unique_ptr<CopyOutputRequest> request,
    const gfx::Vector2d& readback_offset,
    bool flipped_source,
    bool swapped_red_and_blue,
    const gfx::Rect& result_rect,
    const gfx::ColorSpace& color_space) {
  DCHECK_EQ(request->result_format(), ResultFormat::RGBA);
  DCHECK_EQ(request->result_destination(), ResultDestination::kSystemMemory);

  read_pixels_workflows_.emplace_back(std::make_unique<ReadPixelsWorkflow>(
      std::move(request), readback_offset, flipped_source,
      ShouldSwapRedAndBlueForBitmapReadback() != swapped_red_and_blue,
      result_rect, color_space, context_provider_, GetOptimalReadbackFormat()));
  context_provider_->ContextSupport()->SignalQuery(
      read_pixels_workflows_.back()->query(),
      base::BindOnce(&GLRendererCopier::FinishReadPixelsWorkflow,
                     weak_factory_.GetWeakPtr(),
                     read_pixels_workflows_.back().get()));
}

void GLRendererCopier::RenderAndSendTextureResult(
    std::unique_ptr<CopyOutputRequest> request,
    bool flipped_source,
    const gfx::ColorSpace& source_color_space,
    const gfx::ColorSpace& dest_color_space,
    GLuint source_texture,
    const gfx::Size& source_texture_size,
    const gfx::Rect& sampling_rect,
    const gfx::Rect& result_rect,
    ReusableThings* things) {
  DCHECK_EQ(request->result_format(), ResultFormat::RGBA);
  DCHECK_EQ(request->result_destination(), ResultDestination::kNativeTextures);

  auto* sii = context_provider_->SharedImageInterface();
  gpu::Mailbox mailbox = sii->CreateSharedImage(
      ResourceFormat::RGBA_8888, result_rect.size(), dest_color_space,
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
      gpu::SHARED_IMAGE_USAGE_GLES2, gpu::kNullSurfaceHandle);
  auto* gl = context_provider_->ContextGL();
  gl->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());
  GLuint texture = gl->CreateAndTexStorage2DSharedImageCHROMIUM(mailbox.name);
  gl->BeginSharedImageAccessDirectCHROMIUM(
      texture, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  RenderResultTexture(*request, flipped_source, source_color_space,
                      dest_color_space, source_texture, source_texture_size,
                      sampling_rect, result_rect, texture, things);
  gl->EndSharedImageAccessDirectCHROMIUM(texture);
  gl->DeleteTextures(1, &texture);
  gpu::SyncToken sync_token;
  gl->GenSyncTokenCHROMIUM(sync_token.GetData());

  // Create a callback that deletes what was created in this GL context.
  // Note: There's no need to try to pool/re-use the result texture from here,
  // since only clients that are trying to re-invent video capture would see any
  // significant performance benefit. Instead, such clients should use the video
  // capture services provided by VIZ.
  CopyOutputResult::ReleaseCallbacks release_callbacks;
  release_callbacks.push_back(
      texture_deleter_->GetReleaseCallback(context_provider_.get(), mailbox));

  request->SendResult(std::make_unique<CopyOutputTextureResult>(
      CopyOutputResult::Format::RGBA, result_rect,
      CopyOutputResult::TextureResult(mailbox, sync_token, dest_color_space),
      std::move(release_callbacks)));
}

namespace {

// Specialization of CopyOutputResult which reads I420 plane data from a GL
// pixel buffer object, and automatically deletes the pixel buffer object at
// destruction time. This provides an optimal one-copy data flow, from the pixel
// buffer into client-provided memory.
class GLPixelBufferI420Result final : public CopyOutputResult {
 public:
  // |aligned_rect| identifies the region of result pixels in the pixel buffer,
  // while the |result_rect| is the subregion that is exposed to the client.
  GLPixelBufferI420Result(const gfx::Rect& aligned_rect,
                          const gfx::Rect& result_rect,
                          base::WeakPtr<GLRendererCopier> copier_weak_ptr,
                          ContextProvider* context_provider,
                          GLuint transfer_buffer)
      : CopyOutputResult(CopyOutputResult::Format::I420_PLANES,
                         CopyOutputResult::Destination::kSystemMemory,
                         result_rect,
                         /*needs_lock_for_bitmap=*/false),
        aligned_rect_(aligned_rect),
        copier_weak_ptr_(copier_weak_ptr),
        context_provider_(context_provider),
        transfer_buffer_(transfer_buffer) {
    auto* const gl = context_provider_->ContextGL();
    gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, transfer_buffer_);
    pixels_ = static_cast<uint8_t*>(gl->MapBufferCHROMIUM(
        GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, GL_READ_ONLY));
    gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);
  }

  ~GLPixelBufferI420Result() final {
    if (copier_weak_ptr_) {
      auto* const gl = context_provider_->ContextGL();
      gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, transfer_buffer_);
      gl->UnmapBufferCHROMIUM(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM);
      gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);
      gl->DeleteBuffers(1, &transfer_buffer_);
    }
  }

  bool ReadI420Planes(uint8_t* y_out,
                      int y_out_stride,
                      uint8_t* u_out,
                      int u_out_stride,
                      uint8_t* v_out,
                      int v_out_stride) const final {
    DCHECK_GE(y_out_stride, size().width());
    const int chroma_row_bytes = (size().width() + 1) / 2;

    DCHECK_GE(u_out_stride, chroma_row_bytes);
    DCHECK_GE(v_out_stride, chroma_row_bytes);
    if (!copier_weak_ptr_)
      return false;

    uint8_t* pixels = pixels_;
    if (pixels) {
      const int y_stride = aligned_rect_.width();
      const gfx::Vector2d result_offset =
          rect().OffsetFromOrigin() - aligned_rect_.OffsetFromOrigin();
      const int y_start_offset =
          result_offset.y() * y_stride + result_offset.x();
      libyuv::CopyPlane(pixels + y_start_offset, y_stride, y_out, y_out_stride,
                        size().width(), size().height());
      pixels += y_stride * aligned_rect_.height();
      const int chroma_stride = aligned_rect_.width() / 2;
      const int chroma_start_offset =
          ((result_offset.y() / 2) * chroma_stride) + (result_offset.x() / 2);
      const int chroma_height = (size().height() + 1) / 2;
      libyuv::CopyPlane(pixels + chroma_start_offset, chroma_stride, u_out,
                        u_out_stride, chroma_row_bytes, chroma_height);
      pixels += chroma_stride * (aligned_rect_.height() / 2);
      libyuv::CopyPlane(pixels + chroma_start_offset, chroma_stride, v_out,
                        v_out_stride, chroma_row_bytes, chroma_height);
    }
    return !!pixels;
  }

 private:
  const gfx::Rect aligned_rect_;
  base::WeakPtr<GLRendererCopier> copier_weak_ptr_;
  const raw_ptr<ContextProvider> context_provider_;
  const GLuint transfer_buffer_;
  raw_ptr<uint8_t> pixels_;
};

// Specialization of CopyOutputResult which reads NV12 plane data from a GL
// pixel buffer object, and automatically deletes the pixel buffer object at
// destruction time. This provides an optimal one-copy data flow, from the pixel
// buffer into client-provided memory.
class GLPixelBufferNV12Result final : public CopyOutputResult {
 public:
  // |aligned_rect| identifies the region of result pixels in the pixel buffer,
  // while the |result_rect| is the subregion that is exposed to the client.
  GLPixelBufferNV12Result(const gfx::Rect& aligned_rect,
                          const gfx::Rect& result_rect,
                          base::WeakPtr<GLRendererCopier> copier_weak_ptr,
                          ContextProvider* context_provider,
                          GLuint transfer_buffer)
      : CopyOutputResult(CopyOutputResult::Format::NV12_PLANES,
                         CopyOutputResult::Destination::kSystemMemory,
                         result_rect,
                         /*needs_lock_for_bitmap=*/false),
        aligned_rect_(aligned_rect),
        copier_weak_ptr_(copier_weak_ptr),
        context_provider_(context_provider),
        transfer_buffer_(transfer_buffer) {
    auto* const gl = context_provider_->ContextGL();
    gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, transfer_buffer_);
    pixels_ = static_cast<uint8_t*>(gl->MapBufferCHROMIUM(
        GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, GL_READ_ONLY));
    gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);
  }

  ~GLPixelBufferNV12Result() final {
    if (copier_weak_ptr_) {
      auto* const gl = context_provider_->ContextGL();
      gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, transfer_buffer_);
      gl->UnmapBufferCHROMIUM(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM);
      gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);
      gl->DeleteBuffers(1, &transfer_buffer_);
    }
  }

  bool ReadNV12Planes(uint8_t* y_out,
                      int y_out_stride,
                      uint8_t* uv_out,
                      int uv_out_stride) const final {
    DCHECK_GE(y_out_stride, size().width());
    const int chroma_row_bytes = 2 * ((size().width() + 1) / 2);
    DCHECK_GE(uv_out_stride, chroma_row_bytes);
    if (!copier_weak_ptr_)
      return false;

    uint8_t* pixels = pixels_;

    if (pixels) {
      const int y_stride = aligned_rect_.width();
      const gfx::Vector2d result_offset =
          rect().OffsetFromOrigin() - aligned_rect_.OffsetFromOrigin();
      const int y_start_offset =
          result_offset.y() * y_stride + result_offset.x();
      libyuv::CopyPlane(pixels + y_start_offset, y_stride, y_out, y_out_stride,
                        size().width(), size().height());
      pixels += y_stride * aligned_rect_.height();
      const int chroma_stride = aligned_rect_.width();
      const int chroma_start_offset =
          ((result_offset.y() / 2) * chroma_stride) +
          2 * (result_offset.x() / 2);
      const int chroma_height = (size().height() + 1) / 2;
      libyuv::CopyPlane(pixels + chroma_start_offset, chroma_stride, uv_out,
                        uv_out_stride, chroma_row_bytes, chroma_height);
    }
    return !!pixels;
  }

 private:
  const gfx::Rect aligned_rect_;
  base::WeakPtr<GLRendererCopier> copier_weak_ptr_;
  const raw_ptr<ContextProvider> context_provider_;
  const GLuint transfer_buffer_;
  raw_ptr<uint8_t> pixels_ = nullptr;
};

}  // namespace

GLRendererCopier::ReadI420PlanesWorkflow::ReadI420PlanesWorkflow(
    std::unique_ptr<CopyOutputRequest> copy_request,
    const gfx::Rect& aligned_rect,
    const gfx::Rect& result_rect,
    base::WeakPtr<GLRendererCopier> copier_weak_ptr,
    ContextProvider* context_provider)
    : copy_request(std::move(copy_request)),
      aligned_rect(aligned_rect),
      result_rect(result_rect),
      copier_weak_ptr_(copier_weak_ptr),
      context_provider_(context_provider) {
  // Create a buffer for the pixel transfer: A single buffer is used and will
  // contain the Y plane, then the U plane, then the V plane.
  auto* const gl = context_provider_->ContextGL();
  gl->GenBuffers(1, &transfer_buffer);
  gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, transfer_buffer);
  base::CheckedNumeric<int> y_plane_bytes =
      y_texture_size().GetCheckedArea() * kRGBABytesPerPixel;
  base::CheckedNumeric<int> chroma_plane_bytes =
      chroma_texture_size().GetCheckedArea() * kRGBABytesPerPixel;
  gl->BufferData(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM,
                 (y_plane_bytes + chroma_plane_bytes * 2).ValueOrDie(), nullptr,
                 GL_STREAM_READ);
  data_offsets_ = {0, y_plane_bytes.ValueOrDie(),
                   (y_plane_bytes + chroma_plane_bytes).ValueOrDie()};
  gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);

  // Generate the three queries used for determining when each of the plane
  // readbacks has completed.
  gl->GenQueriesEXT(3, queries.data());
}

void GLRendererCopier::ReadI420PlanesWorkflow::BindTransferBuffer() {
  DCHECK_NE(transfer_buffer, 0u);
  context_provider_->ContextGL()->BindBuffer(
      GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, transfer_buffer);
}

void GLRendererCopier::ReadI420PlanesWorkflow::StartPlaneReadback(
    int plane,
    GLenum readback_format) {
  DCHECK_NE(queries[plane], 0u);
  auto* const gl = context_provider_->ContextGL();
  gl->BeginQueryEXT(GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM, queries[plane]);
  const gfx::Size& size = plane == 0 ? y_texture_size() : chroma_texture_size();
  // Note: While a PIXEL_PACK_BUFFER is bound, OpenGL interprets the last
  // argument to ReadPixels() as a byte offset within the buffer instead of
  // an actual pointer in system memory.
  uint8_t* offset_in_buffer = GetOffsetPointer(data_offsets_[plane]);
  gl->ReadPixels(0, 0, size.width(), size.height(), readback_format,
                 GL_UNSIGNED_BYTE, offset_in_buffer);
  gl->EndQueryEXT(GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM);
  context_provider_->ContextSupport()->SignalQuery(
      queries[plane],
      base::BindOnce(&GLRendererCopier::FinishReadI420PlanesWorkflow,
                     copier_weak_ptr_, this, plane));
}

void GLRendererCopier::ReadI420PlanesWorkflow::UnbindTransferBuffer() {
  context_provider_->ContextGL()->BindBuffer(
      GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);
}

GLRendererCopier::ReadI420PlanesWorkflow::~ReadI420PlanesWorkflow() {
  auto* const gl = context_provider_->ContextGL();
  if (transfer_buffer != 0)
    gl->DeleteBuffers(1, &transfer_buffer);
  for (GLuint& query : queries) {
    if (query != 0)
      gl->DeleteQueriesEXT(1, &query);
  }
}

gfx::Size GLRendererCopier::ReadI420PlanesWorkflow::y_texture_size() const {
  return gfx::Size(aligned_rect.width() / kRGBABytesPerPixel,
                   aligned_rect.height());
}

gfx::Size GLRendererCopier::ReadI420PlanesWorkflow::chroma_texture_size()
    const {
  return gfx::Size(aligned_rect.width() / kRGBABytesPerPixel / 2,
                   aligned_rect.height() / 2);
}

GLRendererCopier::ReadNV12PlanesWorkflow::ReadNV12PlanesWorkflow(
    std::unique_ptr<CopyOutputRequest> copy_request,
    const gfx::Rect& aligned_rect,
    const gfx::Rect& result_rect,
    base::WeakPtr<GLRendererCopier> copier_weak_ptr,
    ContextProvider* context_provider)
    : copy_request_(std::move(copy_request)),
      aligned_rect_(aligned_rect),
      result_rect_(result_rect),
      copier_weak_ptr_(copier_weak_ptr),
      context_provider_(context_provider) {
  // Create a buffer for the pixel transfer: A single buffer is used and will
  // contain the Y plane, then the UV plane.
  auto* const gl = context_provider_->ContextGL();
  gl->GenBuffers(1, &transfer_buffer_);
  gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, transfer_buffer_);
  base::CheckedNumeric<int> y_plane_bytes =
      y_texture_size().GetCheckedArea() * kRGBABytesPerPixel;
  base::CheckedNumeric<int> chroma_plane_bytes =
      chroma_texture_size().GetCheckedArea() * kRGBABytesPerPixel;
  gl->BufferData(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM,
                 (y_plane_bytes + chroma_plane_bytes).ValueOrDie(), nullptr,
                 GL_STREAM_READ);
  data_offsets_ = {0, y_plane_bytes.ValueOrDie()};
  gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);

  // Generate the two queries used for determining when each of the plane
  // readbacks has completed.
  gl->GenQueriesEXT(2, queries_.data());
}

void GLRendererCopier::ReadNV12PlanesWorkflow::BindTransferBuffer() {
  DCHECK_NE(transfer_buffer_, 0u);
  context_provider_->ContextGL()->BindBuffer(
      GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, transfer_buffer_);
}

void GLRendererCopier::ReadNV12PlanesWorkflow::StartPlaneReadback(
    int plane,
    GLenum readback_format) {
  DCHECK_NE(queries_[plane], 0u);
  auto* const gl = context_provider_->ContextGL();
  gl->BeginQueryEXT(GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM, queries_[plane]);
  const gfx::Size& size = plane == 0 ? y_texture_size() : chroma_texture_size();
  // Note: While a PIXEL_PACK_BUFFER is bound, OpenGL interprets the last
  // argument to ReadPixels() as a byte offset within the buffer instead of
  // an actual pointer in system memory.
  uint8_t* offset_in_buffer = GetOffsetPointer(data_offsets_[plane]);
  gl->ReadPixels(0, 0, size.width(), size.height(), readback_format,
                 GL_UNSIGNED_BYTE, offset_in_buffer);
  gl->EndQueryEXT(GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM);
  context_provider_->ContextSupport()->SignalQuery(
      queries_[plane],
      base::BindOnce(&GLRendererCopier::FinishReadNV12PlanesWorkflow,
                     copier_weak_ptr_, this, plane));
}

void GLRendererCopier::ReadNV12PlanesWorkflow::UnbindTransferBuffer() {
  context_provider_->ContextGL()->BindBuffer(
      GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);
}

GLRendererCopier::ReadNV12PlanesWorkflow::~ReadNV12PlanesWorkflow() {
  auto* const gl = context_provider_->ContextGL();
  if (transfer_buffer_ != 0)
    gl->DeleteBuffers(1, &transfer_buffer_);
  for (GLuint& query : queries_) {
    if (query != 0)
      gl->DeleteQueriesEXT(1, &query);
  }
}

gfx::Size GLRendererCopier::ReadNV12PlanesWorkflow::y_texture_size() const {
  return gfx::Size(aligned_rect_.width() / kRGBABytesPerPixel,
                   aligned_rect_.height());
}

gfx::Size GLRendererCopier::ReadNV12PlanesWorkflow::chroma_texture_size()
    const {
  return gfx::Size(aligned_rect_.width() / kRGBABytesPerPixel,
                   aligned_rect_.height() / 2);
}

void GLRendererCopier::StartI420ReadbackFromTextures(
    std::unique_ptr<CopyOutputRequest> request,
    const gfx::Rect& aligned_rect,
    const gfx::Rect& result_rect,
    ReusableThings* things) {
  DCHECK_EQ(request->result_format(), ResultFormat::I420_PLANES);

  auto* const gl = context_provider_->ContextGL();
  if (things->yuv_readback_framebuffers[0] == 0) {
    gl->GenFramebuffers(3, things->yuv_readback_framebuffers.data());
  } else if (things->yuv_readback_framebuffers[2] == 0) {
    gl->GenFramebuffers(1, &things->yuv_readback_framebuffers[2]);
  }

  // Execute three asynchronous read-pixels operations, one for each plane. The
  // CopyOutputRequest is passed to the ReadI420PlanesWorkflow, which will send
  // the CopyOutputResult once all readback operations are complete.
  read_i420_workflows_.emplace_back(std::make_unique<ReadI420PlanesWorkflow>(
      std::move(request), aligned_rect, result_rect, weak_factory_.GetWeakPtr(),
      context_provider_));
  ReadI420PlanesWorkflow* workflow = read_i420_workflows_.back().get();
  workflow->BindTransferBuffer();
  for (int plane = 0; plane < 3; ++plane) {
    gl->BindFramebuffer(GL_FRAMEBUFFER,
                        things->yuv_readback_framebuffers[plane]);
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, things->yuv_textures[plane], 0);
    workflow->StartPlaneReadback(plane, GetOptimalReadbackFormat());
  }
  workflow->UnbindTransferBuffer();
}

void GLRendererCopier::FinishReadI420PlanesWorkflow(
    ReadI420PlanesWorkflow* workflow,
    int plane) {
  context_provider_->ContextGL()->DeleteQueriesEXT(1,
                                                   &workflow->queries[plane]);
  workflow->queries[plane] = 0;

  // If all three readbacks have completed, send the result.
  if (workflow->queries == std::array<GLuint, 3>{{0, 0, 0}}) {
    workflow->copy_request->SendResult(
        std::make_unique<GLPixelBufferI420Result>(
            workflow->aligned_rect, workflow->result_rect,
            weak_factory_.GetWeakPtr(), context_provider_,
            workflow->transfer_buffer));
    workflow->transfer_buffer = 0;  // Ownership was transferred to the result.
    const auto it =
        std::find_if(read_i420_workflows_.begin(), read_i420_workflows_.end(),
                     [workflow](auto& ptr) { return ptr.get() == workflow; });
    DCHECK(it != read_i420_workflows_.end());
    read_i420_workflows_.erase(it);
  }
}

void GLRendererCopier::StartNV12ReadbackFromTextures(
    std::unique_ptr<CopyOutputRequest> request,
    const gfx::Rect& aligned_rect,
    const gfx::Rect& result_rect,
    ReusableThings* things) {
  DCHECK_EQ(request->result_format(), ResultFormat::NV12_PLANES);

  auto* const gl = context_provider_->ContextGL();
  if (things->yuv_readback_framebuffers[0] == 0)
    gl->GenFramebuffers(2, things->yuv_readback_framebuffers.data());

  // Execute two asynchronous read-pixels operations, one for each plane. The
  // CopyOutputRequest is passed to the ReadNV12PlanesWorkflow, which will send
  // the CopyOutputResult once all readback operations are complete.
  read_nv12_workflows_.push_back(std::make_unique<ReadNV12PlanesWorkflow>(
      std::move(request), aligned_rect, result_rect, weak_factory_.GetWeakPtr(),
      context_provider_));
  ReadNV12PlanesWorkflow* workflow = read_nv12_workflows_.back().get();
  workflow->BindTransferBuffer();
  for (int plane = 0; plane < 2; ++plane) {
    gl->BindFramebuffer(GL_FRAMEBUFFER,
                        things->yuv_readback_framebuffers[plane]);
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, things->yuv_textures[plane], 0);
    workflow->StartPlaneReadback(plane, GetOptimalReadbackFormat());
  }
  workflow->UnbindTransferBuffer();
}

void GLRendererCopier::FinishReadNV12PlanesWorkflow(
    ReadNV12PlanesWorkflow* workflow,
    int plane) {
  GLuint query = workflow->query(plane);
  context_provider_->ContextGL()->DeleteQueriesEXT(1, &query);
  workflow->MarkQueryCompleted(plane);

  // If both readbacks have completed, send the result.
  if (workflow->IsCompleted()) {
    workflow->TakeRequest()->SendResult(
        std::make_unique<GLPixelBufferNV12Result>(
            workflow->aligned_rect(), workflow->result_rect(),
            weak_factory_.GetWeakPtr(), context_provider_,
            workflow->TakeTransferBuffer()));
    const auto it =
        std::find_if(read_nv12_workflows_.begin(), read_nv12_workflows_.end(),
                     [workflow](auto& ptr) { return ptr.get() == workflow; });
    DCHECK(it != read_nv12_workflows_.end());
    read_nv12_workflows_.erase(it);
  }
}

std::unique_ptr<GLRendererCopier::ReusableThings>
GLRendererCopier::TakeReusableThingsOrCreate(
    const base::UnguessableToken& requester) {
  if (!requester.is_empty()) {
    const auto it = cache_.find(requester);
    if (it != cache_.end()) {
      auto things = std::move(it->second);
      cache_.erase(it);
      return things;
    }
  }

  return std::make_unique<ReusableThings>();
}

void GLRendererCopier::StashReusableThingsOrDelete(
    const base::UnguessableToken& requester,
    std::unique_ptr<ReusableThings> things) {
  if (requester.is_empty()) {
    things->Free(context_provider_->ContextGL());
  } else {
    things->purge_count_at_last_use = purge_counter_;
    cache_[requester] = std::move(things);
  }
}

GLenum GLRendererCopier::GetOptimalReadbackFormat() {
  if (optimal_readback_format_ != GL_NONE)
    return optimal_readback_format_;

  // Preconditions: GetOptimalReadbackFormat() requires a valid context and a
  // complete framebuffer set up. The latter must be guaranteed by all possible
  // callers of this method.
  auto* const gl = context_provider_->ContextGL();
  if (gl->GetGraphicsResetStatusKHR() != GL_NO_ERROR)
    return GL_RGBA;  // No context: Just return a sane default.
  DCHECK(gl->CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

  // If the GL implementation internally uses the GL_BGRA_EXT+GL_UNSIGNED_BYTE
  // format+type combination, then consider that the optimal readback
  // format+type. Otherwise, use GL_RGBA+GL_UNSIGNED_BYTE, which all platforms
  // must support, per the GLES 2.0 spec.
  GLint type = 0;
  GLint readback_format = 0;
  gl->GetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &type);
  if (type == GL_UNSIGNED_BYTE)
    gl->GetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &readback_format);
  if (readback_format != GL_BGRA_EXT)
    readback_format = GL_RGBA;

  optimal_readback_format_ = static_cast<GLenum>(readback_format);
  return optimal_readback_format_;
}

bool GLRendererCopier::ShouldSwapRedAndBlueForBitmapReadback() {
  const bool skbitmap_is_bgra = (kN32_SkColorType == kBGRA_8888_SkColorType);
  const bool readback_will_be_bgra =
      (GetOptimalReadbackFormat() == GL_BGRA_EXT);
  return skbitmap_is_bgra != readback_will_be_bgra;
}

GLRendererCopier::ReusableThings::ReusableThings() = default;

GLRendererCopier::ReusableThings::~ReusableThings() {
  // Ensure all resources were freed by this point. Resources aren't explicity
  // freed here, in the destructor, because some require access to the GL
  // context. See Free().
  DCHECK_EQ(fb_copy_texture, 0u);
  DCHECK(!scaler);
  DCHECK_EQ(result_texture, 0u);
  DCHECK_EQ(readback_framebuffer, 0u);
  DCHECK(!i420_converter);
  constexpr std::array<GLuint, 3> kAllZeros = {0, 0, 0};
  DCHECK(yuv_textures == kAllZeros);
  DCHECK(yuv_readback_framebuffers == kAllZeros);
}

void GLRendererCopier::ReusableThings::Free(gpu::gles2::GLES2Interface* gl) {
  if (fb_copy_texture != 0) {
    gl->DeleteTextures(1, &fb_copy_texture);
    fb_copy_texture = 0;
    fb_copy_texture_internal_format = static_cast<GLenum>(GL_NONE);
    fb_copy_texture_size = gfx::Size();
  }
  scaler.reset();
  if (result_texture != 0) {
    gl->DeleteTextures(1, &result_texture);
    result_texture = 0;
    result_texture_size = gfx::Size();
  }
  if (readback_framebuffer != 0) {
    gl->DeleteFramebuffers(1, &readback_framebuffer);
    readback_framebuffer = 0;
  }

  i420_converter.reset();
  nv12_converter.reset();

  if (yuv_textures[0] != 0) {
    // We have some cached textures, check if there's 2 or 3 & delete them:
    int num_textures = yuv_textures[2] != 0 ? 3 : 2;
    gl->DeleteTextures(num_textures, yuv_textures.data());
    yuv_textures = {0, 0, 0};
    texture_sizes = {};
  }
  if (yuv_readback_framebuffers[0] != 0) {
    // We have some cached readback buffers, check if there's 2 or 3 & delete
    // them:
    int num_readback_buffers = yuv_readback_framebuffers[2] != 0 ? 3 : 2;
    gl->DeleteFramebuffers(num_readback_buffers,
                           yuv_readback_framebuffers.data());
    yuv_readback_framebuffers = {0, 0, 0};
  }
}

}  // namespace viz
