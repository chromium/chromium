// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_dawn_d3d11_blt_mode.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/service/dawn_context_provider.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_utils.h"
#include "third_party/skia/include/gpu/graphite/dawn/DawnTypes.h"
#include "ui/gl/vsync_provider_win.h"

namespace viz {

SkiaOutputDeviceDawnD3D11BltMode::SkiaOutputDeviceDawnD3D11BltMode(
    scoped_refptr<gpu::SharedContextState> context_state,
    gfx::SurfaceOrigin origin,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback,
    base::PassKey<SkiaOutputDeviceDawn> pass_key)
    : SkiaOutputDeviceDawn(std::move(context_state),
                           origin,
                           memory_tracker,
                           std::move(did_swap_buffer_complete_callback),
                           pass_key) {
  capabilities_.supports_post_sub_buffer = true;
}

SkiaOutputDeviceDawnD3D11BltMode::~SkiaOutputDeviceDawnD3D11BltMode() =
    default;

bool SkiaOutputDeviceDawnD3D11BltMode::Initialize(
    gpu::SurfaceHandle surface_handle) {
  CHECK_EQ(context_state_->dawn_context_provider()->backend_type(),
           wgpu::BackendType::D3D11);

  surface_handle_ = surface_handle;
  vsync_provider_ = std::make_unique<gl::VSyncProviderWin>(surface_handle_);

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      context_state_->dawn_context_provider()->GetD3D11Device();
  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  CHECK_EQ(d3d11_device.As(&dxgi_device), S_OK);
  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  CHECK_EQ(dxgi_device->GetAdapter(&dxgi_adapter), S_OK);
  CHECK_EQ(dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory_)), S_OK);

  // Use the BitBlt swap effect so this swap chain can present directly into
  // the browser's window without needing a child window: BitBlt-model swap
  // chains, unlike flip model, don't require the presented-into HWND to be
  // owned by the GPU process. This is also the model ANGLE uses.
  DXGI_SWAP_CHAIN_DESC1 desc = {};
  desc.Width = 1;
  desc.Height = 1;
  desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.BufferUsage =
      DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
  desc.BufferCount = 1;
  desc.Scaling = DXGI_SCALING_STRETCH;
  desc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;
  desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

  HRESULT hr = dxgi_factory_->CreateSwapChainForHwnd(
      d3d11_device.Get(), surface_handle_, &desc, nullptr, nullptr,
      &swap_chain_);
  if (FAILED(hr)) {
    LOG(ERROR) << "CreateSwapChainForHwnd failed: "
               << logging::SystemErrorCodeToString(hr);
    return false;
  }
  dxgi_factory_->MakeWindowAssociation(surface_handle_,
                                       DXGI_MWA_NO_ALT_ENTER);

  return WrapBackbuffer();
}

bool SkiaOutputDeviceDawnD3D11BltMode::ResizeBackbuffer() {
  texture_ = nullptr;
  shared_texture_memory_ = nullptr;
  backbuffer_.Reset();
  initialized_ = false;

  HRESULT hr = swap_chain_->ResizeBuffers(
      1, size_.width(), size_.height(), DXGI_FORMAT_B8G8R8A8_UNORM, 0);
  if (FAILED(hr)) {
    LOG(ERROR) << "ResizeBuffers failed: "
               << logging::SystemErrorCodeToString(hr);
    return false;
  }

  return WrapBackbuffer();
}

wgpu::Texture SkiaOutputDeviceDawnD3D11BltMode::AcquireSwapChainTexture() {
  wgpu::SharedTextureMemoryD3DSwapchainBeginState swapchain_begin_state = {};
  swapchain_begin_state.isSwapchain = true;

  wgpu::SharedTextureMemoryD3D11BeginState d3d11_begin_state = {};
  d3d11_begin_state.requiresEndAccessFence = false;
  swapchain_begin_state.nextInChain = &d3d11_begin_state;

  wgpu::SharedTextureMemoryBeginAccessDescriptor desc = {};
  desc.initialized = initialized_;
  desc.nextInChain = &swapchain_begin_state;

  if (!shared_texture_memory_.BeginAccess(texture_, &desc)) {
    LOG(ERROR) << "Failed to begin access for backbuffer texture.";
    return nullptr;
  }
  return texture_;
}

void SkiaOutputDeviceDawnD3D11BltMode::ReleaseSwapChainTexture() {
  wgpu::SharedTextureMemoryEndAccessState end_state = {};
  shared_texture_memory_.EndAccess(texture_, &end_state);
  initialized_ = end_state.initialized;
}

void SkiaOutputDeviceDawnD3D11BltMode::PresentImpl(
    const std::optional<gfx::Rect>& rect) {
  if (rect) {
    TRACE_EVENT1("viz", "SkiaOutputDeviceDawnD3D11BltMode::Present1", "rect",
                rect->ToString());
    RECT dirty_rect = {rect->x(), rect->y(), rect->right(), rect->bottom()};
    DXGI_PRESENT_PARAMETERS params = {};
    params.DirtyRectsCount = 1;
    params.pDirtyRects = &dirty_rect;
    swap_chain_->Present1(0, 0, &params);
  } else {
    TRACE_EVENT0("viz", "SkiaOutputDeviceDawnD3D11BltMode::Present");
    swap_chain_->Present(0, 0);
  }
}

bool SkiaOutputDeviceDawnD3D11BltMode::WrapBackbuffer() {
  HRESULT hr = swap_chain_->GetBuffer(0, IID_PPV_ARGS(&backbuffer_));
  if (FAILED(hr)) {
    LOG(ERROR) << "GetBuffer(0) failed: "
               << logging::SystemErrorCodeToString(hr);
    return false;
  }

  shared_texture_memory_ = gpu::CreateDawnSharedTextureMemory(
      context_state_->dawn_context_provider()->GetDevice(), backbuffer_);
  if (!shared_texture_memory_) {
    LOG(ERROR) << "Failed to create shared texture memory for backbuffer.";
    return false;
  }

  constexpr wgpu::TextureUsage kUsage =
      wgpu::TextureUsage::RenderAttachment |
      wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopySrc |
      wgpu::TextureUsage::CopyDst;
  texture_ = gpu::CreateDawnSharedTexture(shared_texture_memory_, kUsage,
                                          /*internal_usage=*/
                                          wgpu::TextureUsage::None,
                                          /*view_formats=*/{});
  if (!texture_) {
    LOG(ERROR) << "Failed to create Dawn texture for backbuffer.";
    return false;
  }
  return true;
}

}  // namespace viz
