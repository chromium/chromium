// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_offscreen.h"

#include <memory>
#include <utility>

#include "base/check_is_test.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GpuTypes.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/gpu/graphite/Surface.h"
#include "third_party/skia/include/gpu/graphite/TextureInfo.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "third_party/skia/include/gpu/ganesh/vk/GrVkBackendSurface.h"
#endif

namespace viz {

SkiaOutputDeviceOffscreen::SkiaOutputDeviceOffscreen(
    scoped_refptr<gpu::SharedContextState> context_state,
    gfx::SurfaceOrigin origin,
    bool has_alpha,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDevice(context_state->gr_context(),
                       context_state->graphite_context(),
                       memory_tracker,
                       did_swap_buffer_complete_callback),
      context_state_(context_state),
      has_alpha_(has_alpha) {
  capabilities_.uses_default_gl_framebuffer = false;
  capabilities_.output_surface_origin = origin;
  capabilities_.supports_post_sub_buffer = true;

  // Some Vulkan drivers do not support kRGB_888x_SkColorType. Always use
  // kRGBA/BGRA_8888_SkColorType instead and initialize surface to opaque as
  // necessary.
  // TODO(crbug.com/40141277): use the right color types base on GPU
  // capabilities.
  capabilities_.sk_color_type_map[SinglePlaneFormat::kRGBA_8888] =
      kRGBA_8888_SkColorType;
  capabilities_.sk_color_type_map[SinglePlaneFormat::kRGBX_8888] =
      kRGBA_8888_SkColorType;
  capabilities_.sk_color_type_map[SinglePlaneFormat::kBGRA_8888] =
      kBGRA_8888_SkColorType;
  capabilities_.sk_color_type_map[SinglePlaneFormat::kBGRX_8888] =
      kBGRA_8888_SkColorType;
}

SkiaOutputDeviceOffscreen::~SkiaOutputDeviceOffscreen() {
  DiscardBackbuffer();
}

bool SkiaOutputDeviceOffscreen::Reshape(const ReshapeParams& params) {
  DCHECK_EQ(params.transform, gfx::OVERLAY_TRANSFORM_NONE);
  DiscardBackbuffer();
  size_ = params.GfxSize();
  if (size_.width() > capabilities_.max_texture_size ||
      size_.height() > capabilities_.max_texture_size) {
    LOG(ERROR) << "The requested size (" << size_.ToString()
               << ") exceeds the max texture size ("
               << capabilities_.max_texture_size << ")";
    return false;
  }
  sk_color_type_ = params.image_info.colorType();
  sk_color_space_ = params.image_info.refColorSpace();
  sample_count_ = params.sample_count;
  EnsureBackbuffer();
  return true;
}

void SkiaOutputDeviceOffscreen::Present(
    const std::optional<gfx::Rect>& update_rect,
    BufferPresentedCallback feedback,
    OutputSurfaceFrame frame) {
  // Reshape should have been called first.
  DCHECK(backend_texture_.isValid() || graphite_texture_.isValid());

  StartSwapBuffers(std::move(feedback));
  FinishSwapBuffers(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_ACK),
                    gfx::Size(size_.width(), size_.height()), std::move(frame));
}

void SkiaOutputDeviceOffscreen::EnsureBackbuffer() {
  // Ignore EnsureBackbuffer if Reshape has not been called yet.
  if (size_.IsEmpty()) {
    return;
  }

  CHECK(!backbuffer_estimated_size_);
  if (gr_context_) {
    auto backend_format = context_state_->gr_context()->defaultBackendFormat(
        sk_color_type_, GrRenderable::kYes);
#if BUILDFLAG(IS_MAC)
    DCHECK_EQ(context_state_->gr_context_type(), gpu::GrContextType::kGL);
    // Because SkiaOutputSurface may use IOSurface, we need to ensure that we
    // are using the correct texture target for IOSurfaces (which depends on the
    // GL implementation). Otherwise the validateSurface will fail because of
    // the textureType mismatch.
    backend_format = GrBackendFormats::MakeGL(
        GrBackendFormats::AsGLFormatEnum(backend_format),
        gpu::GetTextureTargetForIOSurfaces());
#endif
    DCHECK(backend_format.isValid())
        << "GrBackendFormat is invalid for color_type: " << sk_color_type_;

    if (has_alpha_) {
      backend_texture_ = context_state_->gr_context()->createBackendTexture(
          size_.width(), size_.height(), backend_format, skgpu::Mipmapped::kNo,
          GrRenderable::kYes);
    } else {
      is_emulated_rgbx_ = true;
      // Initialize alpha channel to opaque.
      backend_texture_ = context_state_->gr_context()->createBackendTexture(
          size_.width(), size_.height(), backend_format, SkColors::kBlack,
          skgpu::Mipmapped::kNo, GrRenderable::kYes);
    }
    DCHECK(backend_texture_.isValid());

    if (backend_texture_.backend() == GrBackendApi::kVulkan) {
#if BUILDFLAG(ENABLE_VULKAN)
      GrVkImageInfo vk_image_info;
      bool result =
          GrBackendTextures::GetVkImageInfo(backend_texture_, &vk_image_info);
      DCHECK(result);
      backbuffer_estimated_size_ = vk_image_info.fAlloc.fSize;
#else
      DCHECK(false);
#endif
    } else {
      auto info = SkImageInfo::Make(size_.width(), size_.height(),
                                    sk_color_type_, kUnpremul_SkAlphaType);
      size_t estimated_size = info.computeMinByteSize();
      backbuffer_estimated_size_ = estimated_size;
    }
  } else {
    CHECK(graphite_context_);
    if (!has_alpha_) {
      is_emulated_rgbx_ = true;
    }
    // Get backend texture info needed for creating backend textures for
    // offscreen.
    skgpu::graphite::TextureInfo texture_info = gpu::GraphiteBackendTextureInfo(
        context_state_->gr_context_type(),
        SkColorTypeToSinglePlaneSharedImageFormat(sk_color_type_),
        /*readonly=*/false,
        /*plane_index=*/0,
        /*is_yuv_plane=*/false, /*mipmapped=*/false,
        /*scanout_dcomp_surface=*/false,
        /*supports_multiplanar_rendering=*/false,
        /*supports_multiplanar_copy=*/false);
    graphite_texture_ =
        context_state_->gpu_main_graphite_recorder()->createBackendTexture(
            gfx::SizeToSkISize(size_), texture_info);
    CHECK(graphite_texture_.isValid());
    auto info = SkImageInfo::Make(size_.width(), size_.height(), sk_color_type_,
                                  kUnpremul_SkAlphaType);
    size_t estimated_size = info.computeMinByteSize();
    backbuffer_estimated_size_ = estimated_size;
  }
  memory_type_tracker_->TrackMemAlloc(backbuffer_estimated_size_);
}

void SkiaOutputDeviceOffscreen::DiscardBackbuffer() {
  if (backend_texture_.isValid()) {
    sk_surface_.reset();
    DeleteGrBackendTexture(context_state_.get(), &backend_texture_);
    backend_texture_ = GrBackendTexture();
    memory_type_tracker_->TrackMemFree(backbuffer_estimated_size_);
    backbuffer_estimated_size_ = 0u;
  } else if (graphite_texture_.isValid()) {
    CHECK(graphite_context_);
    sk_surface_.reset();
    graphite_context_->deleteBackendTexture(graphite_texture_);
    graphite_texture_ = skgpu::graphite::BackendTexture();
    memory_type_tracker_->TrackMemFree(backbuffer_estimated_size_);
    backbuffer_estimated_size_ = 0u;
  }
}

SkSurface* SkiaOutputDeviceOffscreen::BeginPaint(
    std::vector<GrBackendSemaphore>* end_semaphores) {
  DCHECK(backend_texture_.isValid() || graphite_texture_.isValid());
  if (!sk_surface_) {
    SkSurfaceProps surface_props;
    if (gr_context_) {
      sk_surface_ = SkSurfaces::WrapBackendTexture(
          context_state_->gr_context(), backend_texture_,
          capabilities_.output_surface_origin == gfx::SurfaceOrigin::kTopLeft
              ? kTopLeft_GrSurfaceOrigin
              : kBottomLeft_GrSurfaceOrigin,
          sample_count_, sk_color_type_, sk_color_space_, &surface_props);
    } else {
      CHECK(graphite_context_);
      sk_surface_ = SkSurfaces::WrapBackendTexture(
          context_state_->gpu_main_graphite_recorder(), graphite_texture_,
          sk_color_type_, sk_color_space_, &surface_props);
    }
  }
  return sk_surface_.get();
}

void SkiaOutputDeviceOffscreen::EndPaint() {}

void SkiaOutputDeviceOffscreen::ReadbackForTesting(
    base::OnceCallback<void(SkBitmap)> callback) {
  CHECK_IS_TEST();

  struct ReadPixelsContext {
    std::unique_ptr<const SkImage::AsyncReadResult> async_result;
    bool finished = false;
    static void OnReadPixelsDone(
        void* raw_ctx,
        std::unique_ptr<const SkImage::AsyncReadResult> async_result) {
      ReadPixelsContext* context =
          reinterpret_cast<ReadPixelsContext*>(raw_ctx);
      context->async_result = std::move(async_result);
      context->finished = true;
    }
  };

  ReadPixelsContext context;
  if (auto* graphite_context = context_state_->graphite_context()) {
    graphite_context->asyncRescaleAndReadPixels(
        sk_surface_.get(), sk_surface_->imageInfo(),
        SkIRect::MakeSize(sk_surface_->imageInfo().dimensions()),
        SkImage::RescaleGamma::kSrc, SkImage::RescaleMode::kRepeatedLinear,
        &ReadPixelsContext::OnReadPixelsDone, &context);
  } else {
    CHECK(context_state_->gr_context());
    sk_surface_->asyncRescaleAndReadPixels(
        sk_surface_->imageInfo(),
        SkIRect::MakeSize(sk_surface_->imageInfo().dimensions()),
        SkImage::RescaleGamma::kSrc, SkImage::RescaleMode::kRepeatedLinear,
        &ReadPixelsContext::OnReadPixelsDone, &context);
  }

  context_state_->FlushAndSubmit(true);
  CHECK(context.finished);
  CHECK(context.async_result);

  CHECK_EQ(1, context.async_result->count());
  const SkPixmap src_pixmap(sk_surface_->imageInfo(),
                            const_cast<void*>(context.async_result->data(0)),
                            context.async_result->rowBytes(0));

  // Copy the pixels so we don't need to keep |context.async_result| alive.
  SkBitmap bitmap;
  bitmap.allocPixels(src_pixmap.info());
  CHECK(bitmap.writePixels(src_pixmap));

  std::move(callback).Run(std::move(bitmap));
}

}  // namespace viz
