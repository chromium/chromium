// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_dawn.h"

#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "gpu/command_buffer/service/dawn_context_provider.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "third_party/skia/include/gpu/graphite/BackendTexture.h"
#include "third_party/skia/include/gpu/graphite/Surface.h"
#include "third_party/skia/include/gpu/graphite/dawn/DawnTypes.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/vsync_provider_win.h"

#if BUILDFLAG(IS_ANDROID)
#include "gpu/ipc/common/gpu_surface_lookup.h"
#endif

namespace viz {
namespace {

// TODO(crbug.com/dawn/286): Dawn requires that surface format is BGRA8Unorm for
// desktop and RGBA8Unorm for Android. Use GetPreferredSurfaceFormat when ready.
#if BUILDFLAG(IS_ANDROID)
constexpr SkColorType kSurfaceColorType = kRGBA_8888_SkColorType;
constexpr wgpu::TextureFormat kSwapChainFormat =
    wgpu::TextureFormat::RGBA8Unorm;
#else
constexpr SkColorType kSurfaceColorType = kBGRA_8888_SkColorType;
constexpr wgpu::TextureFormat kSwapChainFormat =
    wgpu::TextureFormat::BGRA8Unorm;
#endif

constexpr wgpu::TextureUsage kUsage =
    wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding |
    wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst;

}  // namespace

std::unique_ptr<SkiaOutputDeviceDawn> SkiaOutputDeviceDawn::Create(
    scoped_refptr<gpu::SharedContextState> context_state,
    gfx::SurfaceOrigin origin,
    gpu::SurfaceHandle surface_handle,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback) {
  auto output_device = std::make_unique<SkiaOutputDeviceDawn>(
      context_state, origin, memory_tracker,
      std::move(did_swap_buffer_complete_callback), PassKey());

  if (!output_device->Initialize(surface_handle)) {
    return nullptr;
  }

  return output_device;
}

SkiaOutputDeviceDawn::SkiaOutputDeviceDawn(
    scoped_refptr<gpu::SharedContextState> context_state,
    gfx::SurfaceOrigin origin,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback,
    base::PassKey<SkiaOutputDeviceDawn>)
    : SkiaOutputDevice(
          /*gr_context=*/nullptr,
          context_state->graphite_context(),
          memory_tracker,
          did_swap_buffer_complete_callback),
      context_state_(std::move(context_state)) {
  capabilities_.output_surface_origin = origin;
  capabilities_.uses_default_gl_framebuffer = false;
  capabilities_.supports_post_sub_buffer = false;

  capabilities_.sk_color_type_map[SinglePlaneFormat::kRGBA_8888] =
      kSurfaceColorType;
  capabilities_.sk_color_type_map[SinglePlaneFormat::kRGBX_8888] =
      kSurfaceColorType;
  capabilities_.sk_color_type_map[SinglePlaneFormat::kBGRA_8888] =
      kSurfaceColorType;
  capabilities_.sk_color_type_map[SinglePlaneFormat::kBGRX_8888] =
      kSurfaceColorType;
}

bool SkiaOutputDeviceDawn::Initialize(gpu::SurfaceHandle surface_handle) {
  wgpu::SurfaceDescriptor surface_desc;

#if BUILDFLAG(IS_WIN)
  gpu::SurfaceHandle window_handle_to_draw_to;

  // Only D3D swapchain requires that the rendering windows are owned by the
  // process that's currently doing the rendering.
  switch (context_state_->dawn_context_provider()->backend_type()) {
    case wgpu::BackendType::D3D11:
    case wgpu::BackendType::D3D12:
      child_window_.Initialize();
      window_handle_to_draw_to = child_window_.window();
      break;
    default:
      window_handle_to_draw_to = surface_handle;
  }

  vsync_provider_ =
      std::make_unique<gl::VSyncProviderWin>(window_handle_to_draw_to);

  // Create the wgpu::Surface from our HWND.
  wgpu::SurfaceSourceWindowsHWND hwnd_desc;
  hwnd_desc.hwnd = window_handle_to_draw_to;
  hwnd_desc.hinstance = GetModuleHandle(nullptr);

  surface_desc.nextInChain = &hwnd_desc;
#endif

#if BUILDFLAG(IS_ANDROID)
  auto surface_record =
      gpu::GpuSurfaceLookup::GetInstance()->AcquireJavaSurface(surface_handle);
  // Should only reach here if surface control is disabled. In which case
  // browser should not be sending ScopedJavaSurfaceControl variant.
  CHECK(absl::holds_alternative<gl::ScopedJavaSurface>(
      surface_record.surface_variant));
  auto& scoped_java_surface =
      absl::get<gl::ScopedJavaSurface>(surface_record.surface_variant);
  android_native_window_ = gl::ScopedANativeWindow(scoped_java_surface);

  wgpu::SurfaceSourceAndroidNativeWindow android_native_window_desc;
  android_native_window_desc.window = android_native_window_.a_native_window();
  surface_desc.nextInChain = &android_native_window_desc;
#endif

  auto* context_provider = context_state_->dawn_context_provider();
  CHECK(context_provider && context_provider->GetDevice());

  surface_ = context_provider->GetInstance().CreateSurface(&surface_desc);

  wgpu::SurfaceCapabilities caps;
  wgpu::Status result =
      surface_.GetCapabilities(context_provider->GetAdapter(), &caps);
  if (result == wgpu::Status::Error) {
    // With Dawn/Vulkan the Vulkan surface is created lazily when needed, like
    // here for GetCapabilities(), and not when `surface_` is created.
    LOG(ERROR) << "Surface::GetCapabilities() failed";
    return false;
  }

  // Verify `surface_` supports all the required usage for the swap chain.
  CHECK_EQ(~caps.usages & kUsage, 0);

  return true;
}

SkiaOutputDeviceDawn::~SkiaOutputDeviceDawn() = default;

bool SkiaOutputDeviceDawn::Reshape(const ReshapeParams& params) {
  DCHECK_EQ(params.transform, gfx::OVERLAY_TRANSFORM_NONE);

  size_ = params.GfxSize();
  sk_color_space_ = params.image_info.refColorSpace();
  sample_count_ = params.sample_count;

#if BUILDFLAG(IS_WIN)
  if (child_window_.window()) {
    child_window_.Resize(size_);
  }
#endif

  wgpu::SurfaceConfiguration config;
  config.device = context_state_->dawn_context_provider()->GetDevice();
  config.format = kSwapChainFormat;
  config.usage = kUsage;
  config.viewFormatCount = 0;
  config.viewFormats = nullptr;
  config.alphaMode = wgpu::CompositeAlphaMode::Auto;
  config.width = size_.width();
  config.height = size_.height();
  config.presentMode = wgpu::PresentMode::Mailbox;
  surface_.Configure(&config);

  return true;
}

void SkiaOutputDeviceDawn::Present(const std::optional<gfx::Rect>& update_rect,
                                   BufferPresentedCallback feedback,
                                   OutputSurfaceFrame frame) {
  DCHECK(!update_rect);
  StartSwapBuffers({});
  surface_.Present();
  FinishSwapBuffers(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_ACK),
                    gfx::Size(size_.width(), size_.height()), std::move(frame));

  base::TimeTicks timestamp = base::TimeTicks::Now();
  base::TimeTicks vsync_timebase;
  base::TimeDelta vsync_interval;
  uint32_t flags = 0;
  // TODO(rivr): Add an async path for getting vsync parameters. The sync
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
  wgpu::SurfaceTexture texture;
  surface_.GetCurrentTexture(&texture);
  auto backend_texture =
      skgpu::graphite::BackendTextures::MakeDawn(texture.texture.Get());

  SkSurfaceProps surface_props;
  sk_surface_ = SkSurfaces::WrapBackendTexture(
      context_state_->gpu_main_graphite_recorder(), backend_texture,
      kSurfaceColorType, sk_color_space_, &surface_props);
  return sk_surface_.get();
}

void SkiaOutputDeviceDawn::EndPaint() {
  CHECK(sk_surface_);
  if (GrDirectContext* direct_context =
          GrAsDirectContext(sk_surface_->recordingContext())) {
    direct_context->flush(sk_surface_.get(),
                          SkSurfaces::BackendSurfaceAccess::kPresent, {});
  }
  sk_surface_.reset();
}

}  // namespace viz
