// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_dawn.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/dawn_context_provider.h"

#if defined(OS_WIN)
#include <dawn_native/D3D12Backend.h>
#elif defined(OS_LINUX)
#include <dawn_native/VulkanBackend.h>
#endif

namespace viz {

namespace {

// Some Vulkan drivers do not support kRGB_888x_SkColorType. Always use
// kRGBA_8888_SkColorType instead and initialize surface to opaque as necessary.
constexpr SkColorType kSurfaceColorType = kRGBA_8888_SkColorType;
constexpr dawn::TextureFormat kSwapChainFormat =
    dawn::TextureFormat::RGBA8Unorm;

constexpr dawn::TextureUsage kUsage =
    dawn::TextureUsage::OutputAttachment | dawn::TextureUsage::CopySrc;

}  // namespace

SkiaOutputDeviceDawn::SkiaOutputDeviceDawn(
    DawnContextProvider* context_provider,
    gfx::AcceleratedWidget widget,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDevice(/*need_swap_semaphore=*/false,
                       did_swap_buffer_complete_callback),
      context_provider_(context_provider),
      widget_(widget) {
  capabilities_.supports_post_sub_buffer = false;
}

SkiaOutputDeviceDawn::~SkiaOutputDeviceDawn() = default;

bool SkiaOutputDeviceDawn::Reshape(const gfx::Size& size,
                                   float device_scale_factor,
                                   const gfx::ColorSpace& color_space,
                                   bool has_alpha,
                                   gfx::OverlayTransform transform) {
  DCHECK_EQ(transform, gfx::OVERLAY_TRANSFORM_NONE);

  DiscardBackbuffer();
  size_ = size;
  sk_color_space_ = color_space.ToSkColorSpace();

  CreateSwapChainImplementation();
  dawn::SwapChainDescriptor desc;
  desc.implementation = reinterpret_cast<int64_t>(&swap_chain_implementation_);
  swap_chain_ = context_provider_->GetDevice().CreateSwapChain(&desc);
  if (!swap_chain_)
    return false;
  swap_chain_.Configure(kSwapChainFormat, kUsage, size_.width(),
                        size_.height());

  EnsureBackbuffer();
  return true;
}

void SkiaOutputDeviceDawn::SwapBuffers(
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  StartSwapBuffers(std::move(feedback));
  swap_chain_.Present(texture_);
  texture_ = swap_chain_.GetNextTexture();
  FinishSwapBuffers(gfx::SwapResult::SWAP_ACK,
                    gfx::Size(size_.width(), size_.height()),
                    std::move(latency_info));
}

SkSurface* SkiaOutputDeviceDawn::BeginPaint() {
  GrDawnImageInfo info;
  info.fTexture = texture_;
  info.fFormat = kSwapChainFormat;
  info.fLevelCount = 1;
  GrBackendTexture backend_texture(size_.width(), size_.height(), info);
  DCHECK(backend_texture.isValid());
  sk_surface_ = SkSurface::MakeFromBackendTextureAsRenderTarget(
      context_provider_->GetGrContext(), backend_texture,
      !capabilities_.flipped_output_surface ? kTopLeft_GrSurfaceOrigin
                                            : kBottomLeft_GrSurfaceOrigin,
      /*sampleCount=*/0, kSurfaceColorType, sk_color_space_,
      /*surfaceProps=*/nullptr);
  return sk_surface_.get();
}

void SkiaOutputDeviceDawn::EndPaint(const GrBackendSemaphore& semaphore) {
  GrFlushInfo flush_info;
  sk_surface_->flush(SkSurface::BackendSurfaceAccess::kPresent, flush_info);
  sk_surface_.reset();
}

void SkiaOutputDeviceDawn::EnsureBackbuffer() {
  if (swap_chain_)
    texture_ = swap_chain_.GetNextTexture();
}

void SkiaOutputDeviceDawn::DiscardBackbuffer() {
  texture_ = nullptr;
}

void SkiaOutputDeviceDawn::CreateSwapChainImplementation() {
#if defined(OS_WIN)
  swap_chain_implementation_ = dawn_native::d3d12::CreateNativeSwapChainImpl(
      context_provider_->GetDevice().Get(), widget_);
#else
  NOTREACHED();
  ALLOW_UNUSED_LOCAL(widget_);
#endif
}

}  // namespace viz
