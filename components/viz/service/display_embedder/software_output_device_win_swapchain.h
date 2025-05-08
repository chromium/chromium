// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_DEVICE_WIN_SWAPCHAIN_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_DEVICE_WIN_SWAPCHAIN_H_

#include <d3d11.h>
#include <dcomp.h>
#include <dxgi1_3.h>
#include <wrl/client.h>

#include <memory>

#include "components/viz/service/display_embedder/output_device_backing.h"
#include "components/viz/service/display_embedder/software_output_device_win_base.h"
#include "ui/gl/child_window_win.h"

namespace viz {

// SoftwareOutputDevice implementation in which Skia draws to a DXGI swap chain.
// Using DXGI swap chains guarantees that alpha blending happens correctly and
// consistently with the window behind. SoftwareOutputDeviceWinSwapChain creates
// a trivial dcomp tree that only contains a swap chain. I.e. it is not intended
// to support video overlays or any other feature provided by
// `SkiaOutputDeviceDComp`.
class VIZ_SERVICE_EXPORT SoftwareOutputDeviceWinSwapChain
    : public SoftwareOutputDeviceWinBase,
      public OutputDeviceBacking::Client {
 public:
  SoftwareOutputDeviceWinSwapChain(HWND hwnd,
                                   HWND& child_hwnd,
                                   OutputDeviceBacking* backing);
  ~SoftwareOutputDeviceWinSwapChain() override;

  // SoftwareOutputDeviceWinBase implementation.
  bool ResizeDelegated(const gfx::Size& viewport_pixel_size) override;
  SkCanvas* BeginPaintDelegated() override;
  void EndPaintDelegated(const gfx::Rect& rect) override;
  void NotifyClientResized() override;

  // OutputDeviceBacking::Client implementation.
  const gfx::Size& GetViewportPixelSize() const override;
  void ReleaseCanvas() override;

  bool HasSwapChainForTesting() const { return !!dxgi_swapchain_; }

  bool HasDeviceContextForTesting() const { return !!d3d11_device_context_; }

 protected:
  virtual bool UpdateWindowSize(const gfx::Size& viewport_pixel_size);

 private:
  raw_ptr<OutputDeviceBacking> const output_backing_;
  gl::ChildWindowWin child_window_;
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_device_context_;
  Microsoft::WRL::ComPtr<IDXGISwapChain1> dxgi_swapchain_;
  Microsoft::WRL::ComPtr<IDCompositionDevice> dcomp_device_;
  Microsoft::WRL::ComPtr<IDCompositionTarget> dcomp_target_;
  Microsoft::WRL::ComPtr<IDCompositionVisual> dcomp_root_visual_;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_staging_texture_;

  std::unique_ptr<SkCanvas> sk_canvas_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_DEVICE_WIN_SWAPCHAIN_H_
