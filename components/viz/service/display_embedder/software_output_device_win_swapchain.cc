// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/software_output_device_win_swapchain.h"

#include <utility>

#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "components/viz/service/gl/exit_code.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/skia_utils_win.h"

namespace viz {

namespace {

constexpr DXGI_FORMAT kDXGISwapChainFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

D3D11_BOX ToD3D11Box(const gfx::Rect& gfx_rect) {
  D3D11_BOX d3d11_box = {.left = static_cast<UINT>(gfx_rect.x()),
                         .top = static_cast<UINT>(gfx_rect.y()),
                         .front = 0,
                         .right = static_cast<UINT>(gfx_rect.right()),
                         .bottom = static_cast<UINT>(gfx_rect.bottom()),
                         .back = 1};
  return d3d11_box;
}

// CHECKs if the HRESULT is not DXGI_ERROR_DEVICE_REMOVED or the device was
// removed due to application error.
void CheckDeviceRemoved(HRESULT hr,
                        ID3D11Device* device,
                        std::string_view context) {
  LOG(ERROR) << base::StrCat(
      {context, ": ", logging::SystemErrorCodeToString(hr)});
  CHECK_EQ(hr, DXGI_ERROR_DEVICE_REMOVED);
  hr = device->GetDeviceRemovedReason();
  // Filter out results that include physical device removals and internal
  // errors as these are not in the application's control.
  CHECK(hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET ||
        hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR);
}

}  // namespace

SoftwareOutputDeviceWinSwapChain::SoftwareOutputDeviceWinSwapChain(
    HWND hwnd,
    HWND& child_hwnd,
    OutputDeviceBacking* output_backing)
    : SoftwareOutputDeviceWinBase(hwnd), output_backing_(output_backing) {
  child_window_.Initialize();
  child_hwnd = child_window_.window();
  output_backing_->RegisterClient(this);
}

SoftwareOutputDeviceWinSwapChain::~SoftwareOutputDeviceWinSwapChain() {
  output_backing_->UnregisterClient(this);
  // If DWM.exe crashes, the Chromium window will become black until the next
  // commit. Therefore clear the root visual manually and proactively commit.
  dcomp_root_visual_.Reset();
  dcomp_target_.Reset();
  if (dcomp_device_) {
    dcomp_device_->Commit();
  }
}

bool SoftwareOutputDeviceWinSwapChain::UpdateWindowSize(
    const gfx::Size& viewport_pixel_size) {
  // Update the size of the child window.
  return SetWindowPos(child_window_.window(), nullptr, 0, 0,
                      viewport_pixel_size.width(), viewport_pixel_size.height(),
                      SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOCOPYBITS |
                          SWP_NOOWNERZORDER | SWP_NOZORDER);
}

bool SoftwareOutputDeviceWinSwapChain::ResizeDelegated(
    const gfx::Size& viewport_pixel_size) {
  // Update window size.
  if (!UpdateWindowSize(viewport_pixel_size)) {
    return false;
  }

  // If the swapchain already exists, resize it instead of creating a new one.
  if (dxgi_swapchain_) {
    DCHECK(d3d11_device_);
    DCHECK(d3d11_device_context_);

    HRESULT hr = dxgi_swapchain_->ResizeBuffers(2, viewport_pixel_size.width(),
                                                viewport_pixel_size.height(),
                                                kDXGISwapChainFormat, 0);
    if (FAILED(hr)) {
      // If ResizeBuffers fails, the swapchain and window sizes will be out of
      // sync, causing unexpected behavior such as permanent gutters or
      // clipping. Therefore, terminate the GPU process to refresh state.
      RestartGpuProcessForContextLoss(
          base::StringPrintf("IDXGISwapChain::ResizeBuffers failed: %s",
                             logging::SystemErrorCodeToString(hr)));
    }
  } else {
    // Defer the creation of DirectX related objects to when they're needed
    // rather than on creation of the object. This allows for a retry in the
    // following resize call if a transient error prevents their initial
    // creation. Furthermore it prevents the object from persisting in a
    // semi-initialized state.
    DCHECK(!d3d11_device_);
    DCHECK(!d3d11_device_context_);
    Microsoft::WRL::ComPtr<IDXGIFactory2> dxgi_factory;
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
    Microsoft::WRL::ComPtr<IDCompositionDevice> dcomp_device;
    HRESULT hr = output_backing_->GetOrCreateDXObjects(
        &d3d11_device, &dxgi_factory, &dcomp_device);
    if (FAILED(hr)) {
      // If the error code is E_ACCESSDENIED, it indicates that the browser is
      // running in session 0, which is non-interactive. There would be no
      // rendering in this case, and there is no need to terminate the GPU
      // process.
      CHECK_EQ(hr, E_ACCESSDENIED);
      return false;
    }

    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_device_context;
    d3d11_device->GetImmediateContext(&d3d11_device_context);

    // Set up the visual tree.
    Microsoft::WRL::ComPtr<IDCompositionTarget> dcomp_target;
    hr = dcomp_device->CreateTargetForHwnd(child_window_.window(), TRUE,
                                           &dcomp_target);
    if (FAILED(hr)) {
      // Destroying the parent window will automatically destroy all child
      // windows. Since the GPU process manages the child window, it needs to be
      // prepared for the window handle to become invalid at any point. The
      // child window may not be valid in scenarios such as the parent window
      // being closed immediately prior to this code being executed, so ignore
      // cases where hr == E_INVALIDARG, which is empirically found to be
      // returned when the window is not valid. This will ensure that the
      // following CHECK still hits for more meaningful errors.
      CHECK_EQ(hr, E_INVALIDARG);
      return false;
    }

    Microsoft::WRL::ComPtr<IDCompositionVisual> dcomp_root_visual;
    hr = dcomp_device->CreateVisual(&dcomp_root_visual);
    CHECK_EQ(hr, S_OK);

    hr = dcomp_target->SetRoot(dcomp_root_visual.Get());
    CHECK_EQ(hr, S_OK);

    // Create swapchain.
    DXGI_SWAP_CHAIN_DESC1 dxgi_swapchain_desc = {
        .Width = static_cast<UINT>(viewport_pixel_size.width()),
        .Height = static_cast<UINT>(viewport_pixel_size.height()),
        .Format = kDXGISwapChainFormat,
        .Stereo = FALSE,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .BufferUsage = 0,
        .BufferCount = 2,
        .Scaling = DXGI_SCALING_STRETCH,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
        // TODO(crbug.com/384897625) Consider changing alpha mode based on
        // whether current frame has alpha.
        .AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED,
        .Flags = 0};
    Microsoft::WRL::ComPtr<IDXGISwapChain1> dxgi_swapchain;
    hr = dxgi_factory->CreateSwapChainForComposition(
        d3d11_device.Get(), &dxgi_swapchain_desc, nullptr, &dxgi_swapchain);
    if (FAILED(hr)) {
      // An error in CreateSwapChainForComposition is observed to happen mostly
      // due to OOM issues. This causes the DXGI device to be removed resulting
      // in an error somewhere else. If this occurs, terminate the GPU process
      // here to refresh state.
      RestartGpuProcessForContextLoss(base::StringPrintf(
          "IDXGIFactory2::CreateSwapChainForComposition failed: %s",
          logging::SystemErrorCodeToString(hr)));
    }

    // Set swapchain as root visual content.
    hr = dcomp_root_visual->SetContent(dxgi_swapchain.Get());
    CHECK_EQ(hr, S_OK);

    hr = dcomp_device->Commit();
    CHECK_EQ(hr, S_OK);

    // Once all of the resources have been allocated into local variables
    // copy them as a group to the member variables so the object is never
    // in a half baked state.
    d3d11_device_ = std::move(d3d11_device);
    d3d11_device_context_ = std::move(d3d11_device_context);
    dxgi_swapchain_ = std::move(dxgi_swapchain);
    dcomp_device_ = std::move(dcomp_device);
    dcomp_target_ = std::move(dcomp_target);
    dcomp_root_visual_ = std::move(dcomp_root_visual);
  }
  return true;
}

SkCanvas* SoftwareOutputDeviceWinSwapChain::BeginPaintDelegated() {
  // It is expected that the `d3d11_device_context_` exists by the time this
  // function is called. If it does not, it is likely that the resize failed due
  // to a possible issue with the child window.
  if (!d3d11_device_context_) {
    return nullptr;
  }

  CHECK(!d3d11_staging_texture_);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_staging_texture =
      output_backing_->GetOrCreateStagingTexture();
  if (!d3d11_staging_texture) {
    return nullptr;
  }

  D3D11_MAPPED_SUBRESOURCE mapped_subresource{0};
  HRESULT hr =
      d3d11_device_context_->Map(d3d11_staging_texture.Get(), 0,
                                 D3D11_MAP_READ_WRITE, 0, &mapped_subresource);
  if (FAILED(hr)) {
    CheckDeviceRemoved(hr, d3d11_device_.Get(), "ID3D11DeviceContext::Map");
    return nullptr;
  }

  D3D11_TEXTURE2D_DESC d3d11_texture_desc;
  d3d11_staging_texture->GetDesc(&d3d11_texture_desc);

  DCHECK_LE(static_cast<unsigned int>(viewport_pixel_size_.width()),
            d3d11_texture_desc.Width);
  DCHECK_LE(static_cast<unsigned int>(viewport_pixel_size_.height()),
            d3d11_texture_desc.Height);

  sk_canvas_ = skia::CreatePlatformCanvasWithPixels(
      d3d11_texture_desc.Width, d3d11_texture_desc.Height, false,
      static_cast<uint8_t*>(mapped_subresource.pData),
      mapped_subresource.RowPitch, skia::CRASH_ON_FAILURE);
  d3d11_staging_texture_ = std::move(d3d11_staging_texture);
  return sk_canvas_.get();
}

void SoftwareOutputDeviceWinSwapChain::EndPaintDelegated(
    const gfx::Rect& damage) {
  if (!sk_canvas_) {
    return;
  }

  sk_canvas_.reset();
  // The staging texture must be non-null if there was an `sk_canvas_`.
  CHECK(d3d11_staging_texture_);
  d3d11_device_context_->Unmap(d3d11_staging_texture_.Get(), 0);

  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
  HRESULT hr = dxgi_swapchain_->GetBuffer(0, IID_PPV_ARGS(&d3d11_texture));
  CHECK_EQ(hr, S_OK);

  // Copy the newest damage rendered to the staging texture to the next rendered
  // frame.
  const D3D11_BOX d3d11_box = ToD3D11Box(damage);
  d3d11_device_context_->CopySubresourceRegion(
      d3d11_texture.Get(), 0, damage.x(), damage.y(), 0,
      d3d11_staging_texture_.Get(), 0, &d3d11_box);

  RECT damage_rect_win = damage.ToRECT();
  DXGI_PRESENT_PARAMETERS present_parameters = {
      .DirtyRectsCount = 1, .pDirtyRects = &damage_rect_win};
  hr = dxgi_swapchain_->Present1(0, 0, &present_parameters);
  d3d11_staging_texture_.Reset();
  // DXGI_STATUS_OCCLUDED does not indicate anything wrong with the present;
  // only that the window is not visible at present time.
  if (FAILED(hr) && hr != DXGI_STATUS_OCCLUDED) {
    base::debug::Alias(&present_parameters);
    CheckDeviceRemoved(hr, d3d11_device_.Get(), "IDXGISwapChain1::Present1");
    return;
  }
}

void SoftwareOutputDeviceWinSwapChain::NotifyClientResized() {
  output_backing_->ClientResized();
}

void SoftwareOutputDeviceWinSwapChain::ReleaseCanvas() {
  // |sk_canvas_| is the only thing we retain that relies on the shared staging
  // texture from OutputDeviceBacking. We don't expect this be called between
  // BeginPaintDelegated() and EndPaintDelegated(), however, so we just check
  // that the canvas isn't present.
  CHECK(!sk_canvas_);
}

const gfx::Size& SoftwareOutputDeviceWinSwapChain::GetViewportPixelSize()
    const {
  return viewport_pixel_size_;
}

}  // namespace viz
