// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_offscreen.h"

#include <utility>

#include "gpu/command_buffer/service/skia_utils.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace viz {

namespace {

// Some Vulkan drivers do not support kRGB_888x_SkColorType. Always use
// kRGBA_8888_SkColorType instead and initialize surface to opaque as necessary.
constexpr SkColorType kSurfaceColorType = kRGBA_8888_SkColorType;

}  // namespace

SkiaOutputDeviceOffscreen::SkiaOutputDeviceOffscreen(
    scoped_refptr<gpu::SharedContextState> context_state,
    gfx::SurfaceOrigin origin,
    bool has_alpha,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDevice(context_state->gr_context(),
                       memory_tracker,
                       did_swap_buffer_complete_callback),
      context_state_(context_state),
      has_alpha_(has_alpha) {
  capabilities_.uses_default_gl_framebuffer = false;
  capabilities_.output_surface_origin = origin;
  capabilities_.supports_post_sub_buffer = true;

  // TODO(https://crbug.com/1108406): use the right color types base on GPU
  // capabilities.
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::RGBA_8888)] =
      kSurfaceColorType;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::RGBX_8888)] =
      kSurfaceColorType;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::BGRA_8888)] =
      kSurfaceColorType;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::BGRX_8888)] =
      kSurfaceColorType;
}

SkiaOutputDeviceOffscreen::~SkiaOutputDeviceOffscreen() {
  DiscardBackbuffer();
}

bool SkiaOutputDeviceOffscreen::Reshape(const gfx::Size& size,
                                        float device_scale_factor,
                                        const gfx::ColorSpace& color_space,
                                        gfx::BufferFormat format,
                                        gfx::OverlayTransform transform) {
  DCHECK_EQ(transform, gfx::OVERLAY_TRANSFORM_NONE);

  DiscardBackbuffer();
  size_ = size;
  format_ = format;
  sk_color_space_ = color_space.ToSkColorSpace();
  EnsureBackbuffer();
  return true;
}

void SkiaOutputDeviceOffscreen::SwapBuffers(
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  // Reshape should have been called first.
  DCHECK(backend_texture_.isValid());

  StartSwapBuffers(std::move(feedback));
  FinishSwapBuffers(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_ACK),
                    gfx::Size(size_.width(), size_.height()),
                    std::move(latency_info));
}

void SkiaOutputDeviceOffscreen::PostSubBuffer(
    const gfx::Rect& rect,
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  return SwapBuffers(std::move(feedback), std::move(latency_info));
}

void SkiaOutputDeviceOffscreen::EnsureBackbuffer() {
  // Ignore EnsureBackbuffer if Reshape has not been called yet.
  if (size_.IsEmpty())
    return;

  auto format_index = static_cast<int>(format_);
  const auto& sk_color_type = capabilities_.sk_color_types[format_index];
  DCHECK(sk_color_type != kUnknown_SkColorType)
      << "SkColorType is invalid for format: " << format_index;

  if (has_alpha_) {
    backend_texture_ = context_state_->gr_context()->createBackendTexture(
        size_.width(), size_.height(), sk_color_type, GrMipMapped::kNo,
        GrRenderable::kYes);
  } else {
    is_emulated_rgbx_ = true;
    // Initialize alpha channel to opaque.
    backend_texture_ = context_state_->gr_context()->createBackendTexture(
        size_.width(), size_.height(), sk_color_type, SkColors::kBlack,
        GrMipMapped::kNo, GrRenderable::kYes);
  }
  DCHECK(backend_texture_.isValid());

  DCHECK(!backbuffer_estimated_size_);
  if (backend_texture_.backend() == GrBackendApi::kVulkan) {
    GrVkImageInfo vk_image_info;
    bool result = backend_texture_.getVkImageInfo(&vk_image_info);
    DCHECK(result);
    backbuffer_estimated_size_ = vk_image_info.fAlloc.fSize;
  } else {
    auto info = SkImageInfo::Make(size_.width(), size_.height(),
                                  kSurfaceColorType, kUnpremul_SkAlphaType);
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
  }
}

SkSurface* SkiaOutputDeviceOffscreen::BeginPaint(
    std::vector<GrBackendSemaphore>* end_semaphores) {
  DCHECK(backend_texture_.isValid());
  if (!sk_surface_) {
    // LegacyFontHost will get LCD text and skia figures out what type to use.
    SkSurfaceProps surface_props(0 /* flags */,
                                 SkSurfaceProps::kLegacyFontHost_InitType);
    sk_surface_ = SkSurface::MakeFromBackendTexture(
        context_state_->gr_context(), backend_texture_,
        capabilities_.output_surface_origin == gfx::SurfaceOrigin::kTopLeft
            ? kTopLeft_GrSurfaceOrigin
            : kBottomLeft_GrSurfaceOrigin,
        0 /* sampleCount */, kSurfaceColorType, sk_color_space_,
        &surface_props);
  }
  return sk_surface_.get();
}

void SkiaOutputDeviceOffscreen::EndPaint() {}

}  // namespace viz
