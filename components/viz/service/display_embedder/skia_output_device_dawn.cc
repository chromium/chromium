// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_dawn.h"

#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "components/viz/common/gpu/dawn_context_provider.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/dawn/src/include/dawn_native/D3D12Backend.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/vsync_provider_win.h"

namespace viz {

namespace {

// Some Vulkan drivers do not support kRGB_888x_SkColorType. Always use
// kRGBA_8888_SkColorType instead and initialize surface to opaque as necessary.
constexpr SkColorType kSurfaceColorType = kRGBA_8888_SkColorType;
constexpr wgpu::TextureFormat kSwapChainFormat =
    wgpu::TextureFormat::RGBA8Unorm;

constexpr wgpu::TextureUsage kUsage =
    wgpu::TextureUsage::OutputAttachment | wgpu::TextureUsage::CopySrc;

}  // namespace

SkiaOutputDeviceDawn::SkiaOutputDeviceDawn(
    DawnContextProvider* context_provider,
    gfx::AcceleratedWidget widget,
    gfx::SurfaceOrigin origin,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDevice(context_provider->GetGrContext(),
                       memory_tracker,
                       did_swap_buffer_complete_callback),
      context_provider_(context_provider),
      child_window_(widget) {
  capabilities_.output_surface_origin = origin;
  capabilities_.uses_default_gl_framebuffer = false;
  capabilities_.supports_post_sub_buffer = false;

  // TODO(https://crbug.com/1108406): use buffer format from Reshape().
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::RGBA_8888)] =
      kSurfaceColorType;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::RGBX_8888)] =
      kSurfaceColorType;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::BGRA_8888)] =
      kSurfaceColorType;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::BGRX_8888)] =
      kSurfaceColorType;
  vsync_provider_ = std::make_unique<gl::VSyncProviderWin>(widget);
  child_window_.Initialize();
}

SkiaOutputDeviceDawn::~SkiaOutputDeviceDawn() = default;

gpu::SurfaceHandle SkiaOutputDeviceDawn::GetChildSurfaceHandle() const {
  return child_window_.window();
}

bool SkiaOutputDeviceDawn::Reshape(const gfx::Size& size,
                                   float device_scale_factor,
                                   const gfx::ColorSpace& color_space,
                                   gfx::BufferFormat format,
                                   gfx::OverlayTransform transform) {
  DCHECK_EQ(transform, gfx::OVERLAY_TRANSFORM_NONE);

  size_ = size;
  sk_color_space_ = color_space.ToSkColorSpace();

  CreateSwapChainImplementation();
  wgpu::SwapChainDescriptor desc;
  desc.implementation = reinterpret_cast<int64_t>(&swap_chain_implementation_);
  // TODO(sgilhuly): Use a wgpu::Surface in this call once the Surface-based
  // SwapChain API is ready.
  swap_chain_ = context_provider_->GetDevice().CreateSwapChain(nullptr, &desc);
  if (!swap_chain_)
    return false;
  swap_chain_.Configure(kSwapChainFormat, kUsage, size_.width(),
                        size_.height());
  return true;
}

void SkiaOutputDeviceDawn::SwapBuffers(BufferPresentedCallback feedback,
                                       OutputSurfaceFrame frame) {
  StartSwapBuffers({});
  swap_chain_.Present();
  FinishSwapBuffers(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_ACK),
                    gfx::Size(size_.width(), size_.height()), std::move(frame));

  base::TimeTicks timestamp = base::TimeTicks::Now();
  base::TimeTicks vsync_timebase;
  base::TimeDelta vsync_interval;
  uint32_t flags = 0;
  // TODO(sgilhuly): Add an async path for getting vsync parameters. The sync
  // path is sufficient for VSyncProviderWin.
  if (vsync_provider_ && vsync_provider_->GetVSyncParametersIfAvailable(
                             &vsync_timebase, &vsync_interval)) {
    // Assume the buffer will be presented at the next vblank.
    timestamp = timestamp.SnappedToNextTick(vsync_timebase, vsync_interval);
    // kHWClock allows future timestamps to be accepted.
    flags =
        gfx::PresentationFeedback::kVSync | gfx::PresentationFeedback::kHWClock;
  }
  std::move(feedback).Run(
      gfx::PresentationFeedback(timestamp, vsync_interval, flags));
}

SkSurface* SkiaOutputDeviceDawn::BeginPaint(
    std::vector<GrBackendSemaphore>* end_semaphores) {
  GrDawnRenderTargetInfo info;
  info.fTextureView = swap_chain_.GetCurrentTextureView();
  info.fFormat = kSwapChainFormat;
  info.fLevelCount = 1;
  GrBackendRenderTarget backend_target(
      size_.width(), size_.height(), /*sampleCnt=*/0, /*stencilBits=*/0, info);
  DCHECK(backend_target.isValid());
  SkSurfaceProps surface_props =
      skia::LegacyDisplayGlobals::GetSkSurfaceProps();
  sk_surface_ = SkSurface::MakeFromBackendRenderTarget(
      context_provider_->GetGrContext(), backend_target,
      capabilities_.output_surface_origin == gfx::SurfaceOrigin::kTopLeft
          ? kTopLeft_GrSurfaceOrigin
          : kBottomLeft_GrSurfaceOrigin,
      kSurfaceColorType, sk_color_space_, &surface_props);
  return sk_surface_.get();
}

void SkiaOutputDeviceDawn::EndPaint() {
  GrFlushInfo flush_info;
  sk_surface_->flush(SkSurface::BackendSurfaceAccess::kPresent, flush_info);
  sk_surface_.reset();
}

void SkiaOutputDeviceDawn::CreateSwapChainImplementation() {
  swap_chain_implementation_ = dawn_native::d3d12::CreateNativeSwapChainImpl(
      context_provider_->GetDevice().Get(), child_window_.window());
}

}  // namespace viz
