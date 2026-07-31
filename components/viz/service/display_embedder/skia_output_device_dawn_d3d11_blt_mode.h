// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_DAWN_D3D11_BLT_MODE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_DAWN_D3D11_BLT_MODE_H_

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include "components/viz/service/display_embedder/skia_output_device_dawn.h"
#include "gpu/ipc/common/surface_handle.h"

namespace viz {

// Presents via a DXGI BitBlt-model (DXGI_SWAP_EFFECT_SEQUENTIAL) swap chain,
// single-buffered, presenting directly into the browser's window. Unlike
// flip model, BitBlt-model swap chains don't require the presented-into
// HWND to be owned by the GPU process, so no child window is needed. Only
// supported for the Dawn D3D11 backend. This class supports partial
// rendering: it presents only the damaged rect via IDXGISwapChain1::Present1
// when one is provided.
class SkiaOutputDeviceDawnD3D11BltMode : public SkiaOutputDeviceDawn {
 public:
  SkiaOutputDeviceDawnD3D11BltMode(
      scoped_refptr<gpu::SharedContextState> context_state,
      gfx::SurfaceOrigin origin,
      gpu::MemoryTracker* memory_tracker,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback,
      base::PassKey<SkiaOutputDeviceDawn> pass_key);

  ~SkiaOutputDeviceDawnD3D11BltMode() override;

  bool Initialize(gpu::SurfaceHandle surface_handle) override;

 protected:
  bool ResizeBackbuffer() override;
  wgpu::Texture AcquireSwapChainTexture() override;
  void ReleaseSwapChainTexture() override;
  void PresentImpl(const std::optional<gfx::Rect>& rect) override;

 private:
  bool WrapBackbuffer();

  gpu::SurfaceHandle surface_handle_ = gpu::kNullSurfaceHandle;
  Microsoft::WRL::ComPtr<IDXGIFactory2> dxgi_factory_;
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> backbuffer_;
  wgpu::SharedTextureMemory shared_texture_memory_;
  wgpu::Texture texture_;
  // Whether the backbuffer contents are initialized. False for a freshly
  // (re)created backbuffer until the first PresentImpl().
  bool initialized_ = false;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_DAWN_D3D11_BLT_MODE_H_
