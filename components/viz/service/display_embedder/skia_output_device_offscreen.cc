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
    bool flipped,
    bool has_alpha,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDevice(false /*need_swap_semaphore */,
                       did_swap_buffer_complete_callback),
      context_state_(context_state),
      has_alpha_(has_alpha) {
  capabilities_.flipped_output_surface = flipped;
  capabilities_.supports_post_sub_buffer = true;
}

SkiaOutputDeviceOffscreen::~SkiaOutputDeviceOffscreen() {
  DiscardBackbuffer();
}

bool SkiaOutputDeviceOffscreen::Reshape(const gfx::Size& size,
                                        float device_scale_factor,
                                        const gfx::ColorSpace& color_space,
                                        bool has_alpha,
                                        gfx::OverlayTransform transform) {
  DCHECK_EQ(transform, gfx::OVERLAY_TRANSFORM_NONE);

  DiscardBackbuffer();
  size_ = size;
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
  FinishSwapBuffers(gfx::SwapResult::SWAP_ACK,
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

  if (has_alpha_) {
    backend_texture_ = context_state_->gr_context()->createBackendTexture(
        size_.width(), size_.height(), kSurfaceColorType, GrMipMapped::kNo,
        GrRenderable::kYes);
  } else {
    is_emulated_rgbx_ = true;
    // Initialize alpha channel to opaque.
    backend_texture_ = context_state_->gr_context()->createBackendTexture(
        size_.width(), size_.height(), kSurfaceColorType, SkColors::kBlack,
        GrMipMapped::kNo, GrRenderable::kYes);
  }
  DCHECK(backend_texture_.isValid());
}

void SkiaOutputDeviceOffscreen::DiscardBackbuffer() {
  if (backend_texture_.isValid()) {
    sk_surface_.reset();
    DeleteGrBackendTexture(context_state_.get(), &backend_texture_);
    backend_texture_ = GrBackendTexture();
  }
}

SkSurface* SkiaOutputDeviceOffscreen::BeginPaint() {
  DCHECK(backend_texture_.isValid());
  if (!sk_surface_) {
    sk_surface_ = SkSurface::MakeFromBackendTexture(
        context_state_->gr_context(), backend_texture_,
        capabilities_.flipped_output_surface ? kTopLeft_GrSurfaceOrigin
                                             : kBottomLeft_GrSurfaceOrigin,
        0 /* sampleCount */, kSurfaceColorType, sk_color_space_,
        nullptr /* surfaceProps */);
  }
  return sk_surface_.get();
}

void SkiaOutputDeviceOffscreen::EndPaint(const GrBackendSemaphore& semaphore) {
}

}  // namespace viz
