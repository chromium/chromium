// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_offscreen.h"

#include <utility>

#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/ganesh/vk/GrVkBackendSurface.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/gpu/graphite/Surface.h"
#include "third_party/skia/include/gpu/graphite/TextureInfo.h"

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
  // TODO(https://crbug.com/1108406): use the right color types base on GPU
  // capabilities.
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::RGBA_8888)] =
      kRGBA_8888_SkColorType;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::RGBX_8888)] =
      kRGBA_8888_SkColorType;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::BGRA_8888)] =
      kBGRA_8888_SkColorType;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::BGRX_8888)] =
      kBGRA_8888_SkColorType;
}

SkiaOutputDeviceOffscreen::~SkiaOutputDeviceOffscreen() {
  DiscardBackbuffer();
}

bool SkiaOutputDeviceOffscreen::Reshape(const SkImageInfo& image_info,
                                        const gfx::ColorSpace& color_space,
                                        int sample_count,
                                        float device_scale_factor,
                                        gfx::OverlayTransform transform) {
  DCHECK_EQ(transform, gfx::OVERLAY_TRANSFORM_NONE);
  DiscardBackbuffer();
  size_ = gfx::SkISizeToSize(image_info.dimensions());
  sk_color_type_ = image_info.colorType();
  sk_color_space_ = image_info.refColorSpace();
  sample_count_ = sample_count;
  EnsureBackbuffer();
  return true;
}

void SkiaOutputDeviceOffscreen::Present(
    const absl::optional<gfx::Rect>& update_rect,
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
    if (has_alpha_) {
      backend_texture_ = context_state_->gr_context()->createBackendTexture(
          size_.width(), size_.height(), sk_color_type_, GrMipMapped::kNo,
          GrRenderable::kYes);
    } else {
      is_emulated_rgbx_ = true;
      // Initialize alpha channel to opaque.
      backend_texture_ = context_state_->gr_context()->createBackendTexture(
          size_.width(), size_.height(), sk_color_type_, SkColors::kBlack,
          GrMipMapped::kNo, GrRenderable::kYes);
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
    skgpu::graphite::TextureInfo texture_info = gpu::GetGraphiteTextureInfo(
        context_state_->gr_context_type(),
        SkColorTypeToSinglePlaneSharedImageFormat(sk_color_type_));
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
    SkSurfaceProps surface_props{0, kUnknown_SkPixelGeometry};
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

}  // namespace viz
