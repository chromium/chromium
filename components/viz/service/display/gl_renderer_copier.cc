// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/gl_renderer_copier.h"

#include <cstring>
#include <utility>

#include "base/bind.h"
#include "base/process/memory.h"
#include "base/stl_util.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/gl_i420_converter.h"
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
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

// Syntactic sugar to DCHECK that two sizes are equal.
#define DCHECK_SIZE_EQ(a, b)                                \
  DCHECK((a) == (b)) << #a " != " #b ": " << (a).ToString() \
                     << " != " << (b).ToString()

namespace viz {

using ResultFormat = CopyOutputRequest::ResultFormat;

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

// Sets the fields of |params| to scale/transform the image in the source
// framebuffer to meet the requirements of the |request|.
void PopulateScalerParameters(const CopyOutputRequest& request,
                              const gfx::ColorSpace& source_color_space,
                              const gfx::ColorSpace& output_color_space,
                              bool flipped_source,
                              GLScaler::Parameters* params) {
  params->scale_from = request.scale_from();
  params->scale_to = request.scale_to();
  params->source_color_space = source_color_space;
  params->output_color_space = output_color_space;
  // For downscaling, use the GOOD quality setting (appropriate for
  // thumbnailing); and, for upscaling, use the BEST quality.
  const bool is_downscale_in_both_dimensions =
      request.scale_to().x() < request.scale_from().x() &&
      request.scale_to().y() < request.scale_from().y();
  params->quality = is_downscale_in_both_dimensions
                        ? GLScaler::Parameters::Quality::GOOD
                        : GLScaler::Parameters::Quality::BEST;
  params->is_flipped_source = flipped_source;
}

}  // namespace

GLRendererCopier::GLRendererCopier(
    scoped_refptr<ContextProvider> context_provider,
    TextureDeleter* texture_deleter)
    : context_provider_(std::move(context_provider)),
      texture_deleter_(texture_deleter) {}

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
    const gfx::ColorSpace& color_space) {
  const gfx::Rect& result_rect = geometry.result_selection;

  // Fast-Path: If no transformation is necessary and no new textures need to be
  // generated, read-back directly from the currently-bound framebuffer.
  if (request->result_format() == ResultFormat::RGBA_BITMAP &&
      !request->is_scaled()) {
    StartReadbackFromFramebuffer(std::move(request), geometry.readback_offset,
                                 flipped_source, false, result_rect,
                                 color_space);
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
    case ResultFormat::RGBA_BITMAP:
      EnsureTextureDefinedWithSize(context_provider_->ContextGL(),
                                   result_rect.size(), &things->result_texture,
                                   &things->result_texture_size);
      RenderResultTexture(*request, flipped_source, color_space, source_texture,
                          source_texture_size, sampling_rect, result_rect,
                          things->result_texture, things.get());
      StartReadbackFromTexture(std::move(request), result_rect, color_space,
                               things.get());
      break;

    case ResultFormat::RGBA_TEXTURE:
      RenderAndSendTextureResult(
          std::move(request), flipped_source, color_space, source_texture,
          source_texture_size, sampling_rect, result_rect, things.get());
      break;

    case ResultFormat::I420_PLANES:
      // The optimized single-copy path, provided by GLPixelBufferI420Result,
      // requires that the result be accessed via a task in the same task runner
      // sequence as the GLRendererCopier. Since I420_PLANES requests are meant
      // to be VIZ-internal, this is an acceptable limitation to enforce.
      DCHECK(request->SendsResultsInCurrentSequence() || async_gl_task_runner_);

      const gfx::Rect aligned_rect = RenderI420Textures(
          *request, flipped_source, color_space, source_texture,
          source_texture_size, sampling_rect, result_rect, things.get());
      StartI420ReadbackFromTextures(std::move(request), aligned_rect,
                                    result_rect, things.get());
      break;
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
    GLuint source_texture,
    const gfx::Size& source_texture_size,
    const gfx::Rect& sampling_rect,
    const gfx::Rect& result_rect,
    GLuint result_texture,
    ReusableThings* things) {
  DCHECK_NE(request.result_format(), ResultFormat::I420_PLANES);

  GLScaler::Parameters params;
  PopulateScalerParameters(request, source_color_space, source_color_space,
                           flipped_source, &params);
  if (request.result_format() == ResultFormat::RGBA_BITMAP) {
    // Render the result in top-down row order, and swizzle, within the GPU so
    // these things don't have to be done, less efficiently, on the CPU later.
    params.flip_output = flipped_source;
    params.swizzle[0] =
        ShouldSwapRedAndBlueForBitmapReadback() ? GL_BGRA_EXT : GL_RGBA;
  } else {
    // Texture results are always in bottom-up row order.
    DCHECK_EQ(request.result_format(), ResultFormat::RGBA_TEXTURE);
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

  // Compute required Y/U/V texture sizes and re-define them, if necessary. See
  // class comments for GLI420Converter for an explanation of how planar data is
  // packed into RGBA textures.
  const gfx::Rect aligned_rect = GLI420Converter::ToAlignedRect(result_rect);
  const gfx::Size required_luma_size(aligned_rect.width() / kRGBABytesPerPixel,
                                     aligned_rect.height());
  const gfx::Size required_chroma_size(required_luma_size.width() / 2,
                                       required_luma_size.height() / 2);
  gfx::Size u_texture_size(things->y_texture_size.width() / 2,
                           things->y_texture_size.height() / 2);
  gfx::Size v_texture_size = u_texture_size;
  auto* const gl = context_provider_->ContextGL();
  EnsureTextureDefinedWithSize(gl, required_luma_size, &things->yuv_textures[0],
                               &things->y_texture_size);
  EnsureTextureDefinedWithSize(gl, required_chroma_size,
                               &things->yuv_textures[1], &u_texture_size);
  EnsureTextureDefinedWithSize(gl, required_chroma_size,
                               &things->yuv_textures[2], &v_texture_size);

  GLI420Converter::Parameters params;
  PopulateScalerParameters(request, source_color_space,
                           gfx::ColorSpace::CreateREC709(), flipped_source,
                           &params);
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

void GLRendererCopier::StartReadbackFromTexture(
    std::unique_ptr<CopyOutputRequest> request,
    const gfx::Rect& result_rect,
    const gfx::ColorSpace& color_space,
    ReusableThings* things) {
  DCHECK_EQ(request->result_format(), ResultFormat::RGBA_BITMAP);

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

class GLPixelBufferRGBAResult : public CopyOutputResult {
 public:
  GLPixelBufferRGBAResult(const gfx::Rect& result_rect,
                          const gfx::ColorSpace& color_space,
                          scoped_refptr<ContextProvider> context_provider,
                          GLuint transfer_buffer,
                          bool is_upside_down,
                          bool swap_red_and_blue)
      : CopyOutputResult(CopyOutputResult::Format::RGBA_BITMAP, result_rect),
        color_space_(color_space),
        context_provider_(std::move(context_provider)),
        transfer_buffer_(transfer_buffer),
        is_upside_down_(is_upside_down),
        swap_red_and_blue_(swap_red_and_blue) {}

  ~GLPixelBufferRGBAResult() final {
    if (transfer_buffer_)
      context_provider_->ContextGL()->DeleteBuffers(1, &transfer_buffer_);
  }

  bool ReadRGBAPlane(uint8_t* dest, int stride) const final {
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
        for (int y = 0; y < size().height();
             ++y, src += src_bytes_per_row, dest += stride) {
          memcpy(dest, src, src_bytes_per_row);
        }
      }
      gl->UnmapBufferCHROMIUM(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM);
    }
    gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);
    return !!pixels;
  }

  gfx::ColorSpace GetRGBAColorSpace() const final { return color_space_; }

  const SkBitmap& AsSkBitmap() const final {
    if (rect().IsEmpty())
      return *cached_bitmap();  // Return "null" bitmap for empty result.

    if (cached_bitmap()->readyToDraw())
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
  const scoped_refptr<ContextProvider> context_provider_;
  mutable GLuint transfer_buffer_;
  const bool is_upside_down_;
  const bool swap_red_and_blue_;
};

// Manages the execution of one asynchronous framebuffer readback and contains
// all the relevant state needed to complete a copy request. The constructor
// initiates the operation, and then at some later point either: 1) the Finish()
// method is invoked; or 2) the instance will be destroyed (cancelled) because
// the GL context is going away. Either way, the GL objects created for this
// workflow are properly cleaned-up.
//
// Motivation: In case #2, it's possible GLRendererCopier will have been
// destroyed before Finish(). However, since there are no dependencies on
// GLRendererCopier to finish the copy request, there's no reason to mess around
// with a complex WeakPtr-to-GLRendererCopier scheme.
class ReadPixelsWorkflow {
 public:
  // Saves all revelant state and initiates the GL asynchronous read-pixels
  // workflow.
  ReadPixelsWorkflow(std::unique_ptr<CopyOutputRequest> copy_request,
                     const gfx::Vector2d& readback_offset,
                     bool flipped_source,
                     bool swap_red_and_blue,
                     const gfx::Rect& result_rect,
                     const gfx::ColorSpace& color_space,
                     scoped_refptr<ContextProvider> context_provider,
                     GLenum readback_format)
      : copy_request_(std::move(copy_request)),
        flipped_source_(flipped_source),
        swap_red_and_blue_(swap_red_and_blue),
        result_rect_(result_rect),
        color_space_(color_space),
        context_provider_(std::move(context_provider)) {
    DCHECK(readback_format == GL_RGBA || readback_format == GL_BGRA_EXT);

    auto* const gl = context_provider_->ContextGL();

    // Create a buffer for the pixel transfer.
    gl->GenBuffers(1, &transfer_buffer_);
    gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, transfer_buffer_);
    gl->BufferData(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM,
                   kRGBABytesPerPixel * result_rect.size().GetArea(), nullptr,
                   GL_STREAM_READ);

    // Execute an asynchronous read-pixels operation, with a query that triggers
    // when Finish() should be run.
    gl->GenQueriesEXT(1, &query_);
    gl->BeginQueryEXT(GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM, query_);
    gl->ReadPixels(readback_offset.x(), readback_offset.y(),
                   result_rect.width(), result_rect.height(), readback_format,
                   GL_UNSIGNED_BYTE, nullptr);
    gl->EndQueryEXT(GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM);
    gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);
  }

  // The destructor is called when the callback that owns this instance is
  // destroyed. That will happen either with or without a call to Finish(),
  // but either way everything will be clean-up appropriately.
  ~ReadPixelsWorkflow() {
    auto* const gl = context_provider_->ContextGL();
    gl->DeleteQueriesEXT(1, &query_);
    if (transfer_buffer_)
      gl->DeleteBuffers(1, &transfer_buffer_);
  }

  GLuint query() const { return query_; }

  // Callback for the asynchronous glReadPixels(). The pixels are read from the
  // transfer buffer, and a CopyOutputResult is sent to the requestor.
  void Finish() {
    auto result = std::make_unique<GLPixelBufferRGBAResult>(
        result_rect_, color_space_, context_provider_, transfer_buffer_,
        flipped_source_, swap_red_and_blue_);
    transfer_buffer_ = 0;  // Ownerhip was transferred to the result.
    if (!copy_request_->SendsResultsInCurrentSequence()) {
      // Force readback into a SkBitmap now, because after PostTask we don't
      // have access to |context_provider_|.
      result->AsSkBitmap();
    }
    copy_request_->SendResult(std::move(result));
  }

 private:
  const std::unique_ptr<CopyOutputRequest> copy_request_;
  const bool flipped_source_;
  const bool swap_red_and_blue_;
  const gfx::Rect result_rect_;
  const gfx::ColorSpace color_space_;
  const scoped_refptr<ContextProvider> context_provider_;
  GLuint transfer_buffer_ = 0;
  GLuint query_ = 0;
};

}  // namespace

void GLRendererCopier::StartReadbackFromFramebuffer(
    std::unique_ptr<CopyOutputRequest> request,
    const gfx::Vector2d& readback_offset,
    bool flipped_source,
    bool swapped_red_and_blue,
    const gfx::Rect& result_rect,
    const gfx::ColorSpace& color_space) {
  DCHECK_EQ(request->result_format(), ResultFormat::RGBA_BITMAP);

  auto workflow = std::make_unique<ReadPixelsWorkflow>(
      std::move(request), readback_offset, flipped_source,
      ShouldSwapRedAndBlueForBitmapReadback() != swapped_red_and_blue,
      result_rect, color_space, context_provider_, GetOptimalReadbackFormat());
  const GLuint query = workflow->query();
  context_provider_->ContextSupport()->SignalQuery(
      query, base::BindOnce(&ReadPixelsWorkflow::Finish, std::move(workflow)));
}

void GLRendererCopier::RenderAndSendTextureResult(
    std::unique_ptr<CopyOutputRequest> request,
    bool flipped_source,
    const gfx::ColorSpace& color_space,
    GLuint source_texture,
    const gfx::Size& source_texture_size,
    const gfx::Rect& sampling_rect,
    const gfx::Rect& result_rect,
    ReusableThings* things) {
  DCHECK_EQ(request->result_format(), ResultFormat::RGBA_TEXTURE);

  auto* sii = context_provider_->SharedImageInterface();
  gpu::Mailbox mailbox =
      sii->CreateSharedImage(ResourceFormat::RGBA_8888, result_rect.size(),
                             color_space, gpu::SHARED_IMAGE_USAGE_GLES2);
  auto* gl = context_provider_->ContextGL();
  gl->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());
  GLuint texture = gl->CreateAndTexStorage2DSharedImageCHROMIUM(mailbox.name);
  gl->BeginSharedImageAccessDirectCHROMIUM(
      texture, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  RenderResultTexture(*request, flipped_source, color_space, source_texture,
                      source_texture_size, sampling_rect, result_rect, texture,
                      things);
  gl->EndSharedImageAccessDirectCHROMIUM(texture);
  gl->DeleteTextures(1, &texture);
  gpu::SyncToken sync_token;
  gl->GenSyncTokenCHROMIUM(sync_token.GetData());

  // Create a callback that deletes what was created in this GL context.
  // Note: There's no need to try to pool/re-use the result texture from here,
  // since only clients that are trying to re-invent video capture would see any
  // significant performance benefit. Instead, such clients should use the video
  // capture services provided by VIZ.
  auto release_callback =
      texture_deleter_->GetReleaseCallback(context_provider_, mailbox);

  request->SendResult(std::make_unique<CopyOutputTextureResult>(
      result_rect, mailbox, sync_token, color_space,
      std::move(release_callback)));
}

namespace {

// Specialization of CopyOutputResult which reads I420 plane data from a GL
// pixel buffer object, and automatically deletes the pixel buffer object at
// destruction time. This provides an optimal one-copy data flow, from the pixel
// buffer into client-provided memory.
class GLPixelBufferI420Result : public CopyOutputResult {
 public:
  // |aligned_rect| identifies the region of result pixels in the pixel buffer,
  // while the |result_rect| is the subregion that is exposed to the client.
  GLPixelBufferI420Result(
      const gfx::Rect& aligned_rect,
      const gfx::Rect& result_rect,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_refptr<ContextProvider> context_provider,
      GLuint transfer_buffer)
      : CopyOutputResult(CopyOutputResult::Format::I420_PLANES, result_rect),
        aligned_rect_(aligned_rect),
        task_runner_(task_runner),
        context_provider_(std::move(context_provider)),
        transfer_buffer_(transfer_buffer) {
    auto* const gl = context_provider_->ContextGL();
    gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, transfer_buffer_);
    pixels_ = static_cast<uint8_t*>(gl->MapBufferCHROMIUM(
        GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, GL_READ_ONLY));
    gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);
  }

  ~GLPixelBufferI420Result() final {
    auto cleanup = base::BindOnce(
        [](scoped_refptr<ContextProvider> context_provider,
           GLuint transfer_buffer) {
          auto* const gl = context_provider->ContextGL();
          gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM,
                         transfer_buffer);
          gl->UnmapBufferCHROMIUM(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM);
          gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);
          gl->DeleteBuffers(1, &transfer_buffer);
        },
        context_provider_, transfer_buffer_);
    if (task_runner_) {
      task_runner_->PostTask(FROM_HERE, std::move(cleanup));
    } else {
      std::move(cleanup).Run();
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

    uint8_t* pixels = pixels_;
    if (pixels) {
      const auto CopyPlane = [](const uint8_t* src, int src_stride,
                                int row_bytes, int num_rows, uint8_t* out,
                                int out_stride) {
        for (int i = 0; i < num_rows;
             ++i, src += src_stride, out += out_stride) {
          memcpy(out, src, row_bytes);
        }
      };
      const int y_stride = aligned_rect_.width();
      const gfx::Vector2d result_offset =
          rect().OffsetFromOrigin() - aligned_rect_.OffsetFromOrigin();
      const int y_start_offset =
          result_offset.y() * y_stride + result_offset.x();
      CopyPlane(pixels + y_start_offset, y_stride, size().width(),
                size().height(), y_out, y_out_stride);
      pixels += y_stride * aligned_rect_.height();
      const int chroma_stride = aligned_rect_.width() / 2;
      const int chroma_start_offset =
          ((result_offset.y() / 2) * chroma_stride) + (result_offset.x() / 2);
      const int chroma_height = (size().height() + 1) / 2;
      CopyPlane(pixels + chroma_start_offset, chroma_stride, chroma_row_bytes,
                chroma_height, u_out, u_out_stride);
      pixels += chroma_stride * (aligned_rect_.height() / 2);
      CopyPlane(pixels + chroma_start_offset, chroma_stride, chroma_row_bytes,
                chroma_height, v_out, v_out_stride);
    }
    return !!pixels;
  }

 private:
  const gfx::Rect aligned_rect_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const scoped_refptr<ContextProvider> context_provider_;
  const GLuint transfer_buffer_;
  uint8_t* pixels_;
};

// Like the ReadPixelsWorkflow, except for I420 planes readback. Because there
// are three separate glReadPixels operations that may complete in any order, a
// ReadI420PlanesWorkflow will receive notifications from three separate "GL
// query" callbacks. It is only after all three operations have completed that a
// fully-assembled CopyOutputResult can be sent.
//
// Please see class comments for ReadPixelsWorkflow for discussion about how GL
// context loss is handled during the workflow.
//
// Also, see class comments for GLI420Converter for an explanation of how planar
// data is packed into RGBA textures.
class ReadI420PlanesWorkflow
    : public base::RefCountedThreadSafe<ReadI420PlanesWorkflow> {
 public:
  ReadI420PlanesWorkflow(
      std::unique_ptr<CopyOutputRequest> copy_request,
      const gfx::Rect& aligned_rect,
      const gfx::Rect& result_rect,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_refptr<ContextProvider> context_provider)
      : copy_request_(std::move(copy_request)),
        aligned_rect_(aligned_rect),
        result_rect_(result_rect),
        task_runner_(std::move(task_runner)),
        context_provider_(std::move(context_provider)) {
    // Create a buffer for the pixel transfer: A single buffer is used and will
    // contain the Y plane, then the U plane, then the V plane.
    auto* const gl = context_provider_->ContextGL();
    gl->GenBuffers(1, &transfer_buffer_);
    gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, transfer_buffer_);
    const int y_plane_bytes = kRGBABytesPerPixel * y_texture_size().GetArea();
    const int chroma_plane_bytes =
        kRGBABytesPerPixel * chroma_texture_size().GetArea();
    gl->BufferData(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM,
                   y_plane_bytes + 2 * chroma_plane_bytes, nullptr,
                   GL_STREAM_READ);
    data_offsets_ = {0, y_plane_bytes, y_plane_bytes + chroma_plane_bytes};
    gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);

    // Generate the three queries used for determining when each of the plane
    // readbacks has completed.
    gl->GenQueriesEXT(3, queries_.data());
  }

  void BindTransferBuffer() {
    DCHECK_NE(transfer_buffer_, 0u);
    context_provider_->ContextGL()->BindBuffer(
        GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, transfer_buffer_);
  }

  void StartPlaneReadback(int plane, GLenum readback_format) {
    DCHECK_NE(queries_[plane], 0u);
    auto* const gl = context_provider_->ContextGL();
    gl->BeginQueryEXT(GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM, queries_[plane]);
    const gfx::Size& size =
        plane == 0 ? y_texture_size() : chroma_texture_size();
    // Note: While a PIXEL_PACK_BUFFER is bound, OpenGL interprets the last
    // argument to ReadPixels() as a byte offset within the buffer instead of
    // an actual pointer in system memory.
    uint8_t* offset_in_buffer = 0;
    offset_in_buffer += data_offsets_[plane];
    gl->ReadPixels(0, 0, size.width(), size.height(), readback_format,
                   GL_UNSIGNED_BYTE, offset_in_buffer);
    gl->EndQueryEXT(GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM);
    context_provider_->ContextSupport()->SignalQuery(
        queries_[plane],
        base::BindOnce(&ReadI420PlanesWorkflow::OnFinishedPlane, this, plane));
  }

  void UnbindTransferBuffer() {
    context_provider_->ContextGL()->BindBuffer(
        GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);
  }

 private:
  friend class base::RefCountedThreadSafe<ReadI420PlanesWorkflow>;

  ~ReadI420PlanesWorkflow() {
    auto* const gl = context_provider_->ContextGL();
    if (transfer_buffer_ != 0)
      gl->DeleteBuffers(1, &transfer_buffer_);
    for (GLuint& query : queries_) {
      if (query != 0)
        gl->DeleteQueriesEXT(1, &query);
    }
  }

  gfx::Size y_texture_size() const {
    return gfx::Size(aligned_rect_.width() / kRGBABytesPerPixel,
                     aligned_rect_.height());
  }

  gfx::Size chroma_texture_size() const {
    return gfx::Size(aligned_rect_.width() / kRGBABytesPerPixel / 2,
                     aligned_rect_.height() / 2);
  }

  void OnFinishedPlane(int plane) {
    context_provider_->ContextGL()->DeleteQueriesEXT(1, &queries_[plane]);
    queries_[plane] = 0;

    // If all three readbacks have completed, send the result.
    if (queries_ == std::array<GLuint, 3>{{0, 0, 0}}) {
      copy_request_->SendResult(std::make_unique<GLPixelBufferI420Result>(
          aligned_rect_, result_rect_, task_runner_, context_provider_,
          transfer_buffer_));
      transfer_buffer_ = 0;  // Ownership was transferred to the result.
    }
  }

  const std::unique_ptr<CopyOutputRequest> copy_request_;
  const gfx::Rect aligned_rect_;
  const gfx::Rect result_rect_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const scoped_refptr<ContextProvider> context_provider_;
  GLuint transfer_buffer_;
  std::array<int, 3> data_offsets_;
  std::array<GLuint, 3> queries_;
};

}  // namespace

void GLRendererCopier::StartI420ReadbackFromTextures(
    std::unique_ptr<CopyOutputRequest> request,
    const gfx::Rect& aligned_rect,
    const gfx::Rect& result_rect,
    ReusableThings* things) {
  DCHECK_EQ(request->result_format(), ResultFormat::I420_PLANES);

  auto* const gl = context_provider_->ContextGL();
  if (things->yuv_readback_framebuffers[0] == 0)
    gl->GenFramebuffers(3, things->yuv_readback_framebuffers.data());

  // Execute three asynchronous read-pixels operations, one for each plane. The
  // CopyOutputRequest is passed to the ReadI420PlanesWorkflow, which will send
  // the CopyOutputResult once all readback operations are complete.
  const auto workflow = base::MakeRefCounted<ReadI420PlanesWorkflow>(
      std::move(request), aligned_rect, result_rect, async_gl_task_runner_,
      context_provider_);
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
  if (yuv_textures[0] != 0) {
    gl->DeleteTextures(3, yuv_textures.data());
    yuv_textures = {0, 0, 0};
    y_texture_size = gfx::Size();
  }
  if (yuv_readback_framebuffers[0] != 0) {
    gl->DeleteFramebuffers(3, yuv_readback_framebuffers.data());
    yuv_readback_framebuffers = {0, 0, 0};
  }
}

}  // namespace viz
