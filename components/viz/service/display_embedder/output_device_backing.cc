// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/output_device_backing.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "components/viz/common/resources/resource_sizes.h"

namespace viz {
namespace {

// If a window is larger than this in bytes, don't even try to create a backing
// bitmap for it.
constexpr size_t kMaxBitmapSizeBytes = 4 * (16384 * 8192);

constexpr DXGI_FORMAT kDXGISwapChainFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

// Finds the size in bytes to hold |viewport_size| pixels. If |viewport_size| is
// a valid size this will return true and |out_bytes| will contain the size in
// bytes. If |viewport_size| is not a valid size then this will return false.
bool GetViewportSizeInBytes(const gfx::Size& viewport_size, size_t* out_bytes) {
  size_t bytes;
  if (!ResourceSizes::MaybeSizeInBytes(viewport_size,
                                       SinglePlaneFormat::kRGBA_8888, &bytes)) {
    return false;
  }
  if (bytes > kMaxBitmapSizeBytes)
    return false;
  *out_bytes = bytes;
  return true;
}

}  // namespace

OutputDeviceBacking::OutputDeviceBacking() = default;

OutputDeviceBacking::~OutputDeviceBacking() {
  DCHECK(clients_.empty());
}

void OutputDeviceBacking::ClientResized() {
  if (d3d11_staging_texture_) {
    D3D11_TEXTURE2D_DESC d3d11_texture_desc;
    d3d11_staging_texture_->GetDesc(&d3d11_texture_desc);
    // Check if we already have a staging texture that matches the max
    // viewport size.
    if (GetMaxViewportSize() !=
        gfx::Size(d3d11_texture_desc.Width, d3d11_texture_desc.Height)) {
      d3d11_staging_texture_.Reset();
    }
  }

  // If the max viewport size doesn't change then nothing here changes.
  if (GetMaxViewportBytes() == created_shm_bytes_)
    return;

  // Otherwise we need to allocate a new shared memory region and clients
  // should re-request it.
  for (OutputDeviceBacking::Client* client : clients_) {
    client->ReleaseCanvas();
  }

  region_ = {};
  created_shm_bytes_ = 0;
}

void OutputDeviceBacking::RegisterClient(Client* client) {
  clients_.push_back(client);
}

void OutputDeviceBacking::UnregisterClient(Client* client) {
  DCHECK(base::Contains(clients_, client));
  std::erase(clients_, client);
  ClientResized();
}

base::UnsafeSharedMemoryRegion* OutputDeviceBacking::GetSharedMemoryRegion(
    const gfx::Size& viewport_size) {
  // If |viewport_size| is empty or too big don't try to allocate SharedMemory.
  size_t viewport_bytes;
  if (!GetViewportSizeInBytes(viewport_size, &viewport_bytes))
    return nullptr;

  // Allocate a new SharedMemory segment that can fit the largest viewport.
  if (!region_.IsValid()) {
    size_t max_viewport_bytes = GetMaxViewportBytes();
    DCHECK_LE(viewport_bytes, max_viewport_bytes);

    base::debug::Alias(&max_viewport_bytes);
    region_ = base::UnsafeSharedMemoryRegion::Create(max_viewport_bytes);
    if (!region_.IsValid()) {
      LOG(ERROR) << "Shared memory region create failed on "
                 << max_viewport_bytes << " bytes";
      return nullptr;
    }
    created_shm_bytes_ = max_viewport_bytes;
  } else {
    // Clients must call Resize() for new |viewport_size|.
    DCHECK_LE(viewport_bytes, created_shm_bytes_);
  }

  return &region_;
}

size_t OutputDeviceBacking::GetMaxViewportBytes() {
  // Minimum byte size is 1 because creating a 0-byte-long SharedMemory fails.
  size_t max_bytes = 1;
  for (OutputDeviceBacking::Client* client : clients_) {
    size_t current_bytes;
    if (!GetViewportSizeInBytes(client->GetViewportPixelSize(), &current_bytes))
      continue;
    max_bytes = std::max(max_bytes, current_bytes);
  }
  return max_bytes;
}

gfx::Size OutputDeviceBacking::GetMaxViewportSize() const {
  gfx::Size size;
  for (OutputDeviceBacking::Client* client : clients_) {
    size.SetToMax(client->GetViewportPixelSize());
  }
  return size;
}

HRESULT OutputDeviceBacking::GetOrCreateDXObjects(
    Microsoft::WRL::ComPtr<ID3D11Device>* d3d11_device_out,
    Microsoft::WRL::ComPtr<IDXGIFactory2>* dxgi_factory_out,
    Microsoft::WRL::ComPtr<IDCompositionDevice>* dcomp_device_out) {
  if (!d3d11_device_) {
    DCHECK(!dxgi_factory_);

    Microsoft::WRL::ComPtr<IDXGIFactory2> dxgi_factory;
    HRESULT hr = ::CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory));
    if (FAILED(hr)) {
      LOG(ERROR) << "CreateDXGIFactory1 failed: "
                 << logging::SystemErrorCodeToString(hr);
      return hr;
    }

    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
    hr = ::D3D11CreateDevice(
        NULL, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_SINGLETHREADED,
        nullptr, 0, D3D11_SDK_VERSION, &d3d11_device, nullptr, nullptr);
    if (FAILED(hr)) {
      LOG(ERROR) << "D3D11CreateDevice failed: "
                 << logging::SystemErrorCodeToString(hr);
      return hr;
    }

    Microsoft::WRL::ComPtr<IDCompositionDevice> dcomp_device;
    hr = ::DCompositionCreateDevice(nullptr, IID_PPV_ARGS(&dcomp_device));
    if (FAILED(hr)) {
      LOG(ERROR) << "DCompositionCreateDevice failed: "
                 << logging::SystemErrorCodeToString(hr);
      return hr;
    }

    // Once all of the resources have been allocated into local variables
    // copy them as a group to member variables so the object is never in a half
    // baked state.
    d3d11_device_ = std::move(d3d11_device);
    dxgi_factory_ = std::move(dxgi_factory);
    dcomp_device_ = std::move(dcomp_device);
  }

  *d3d11_device_out = d3d11_device_;
  *dxgi_factory_out = dxgi_factory_;
  *dcomp_device_out = dcomp_device_;
  return S_OK;
}

Microsoft::WRL::ComPtr<ID3D11Texture2D>
OutputDeviceBacking::GetOrCreateStagingTexture() {
  if (!d3d11_staging_texture_) {
    gfx::Size texture_dimensions = GetMaxViewportSize();
    D3D11_TEXTURE2D_DESC d3d11_texture_desc = {
        .Width = static_cast<UINT>(texture_dimensions.width()),
        .Height = static_cast<UINT>(texture_dimensions.height()),
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = kDXGISwapChainFormat,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .Usage = D3D11_USAGE_STAGING,
        .BindFlags = 0,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ,
        .MiscFlags = 0};
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_staging_texture;
    const HRESULT hr = d3d11_device_->CreateTexture2D(
        &d3d11_texture_desc, nullptr, &d3d11_staging_texture);
    if (FAILED(hr)) {
      LOG(ERROR)
          << "ID3D11Texture2D::CreateTexture2D (for staging texture) failed: "
          << logging::SystemErrorCodeToString(hr);
      return nullptr;
    }
    d3d11_staging_texture_ = std::move(d3d11_staging_texture);
  }

  return d3d11_staging_texture_;
}

}  // namespace viz
