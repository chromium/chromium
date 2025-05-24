// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_DEVICE_BACKING_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_DEVICE_BACKING_H_

#include <d3d11.h>
#include <dcomp.h>
#include <dxgi1_3.h>
#include <wrl/client.h>

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SharedMemory;
}

namespace viz {

// Allocates and owns a SharedMemory backing for multiple SoftwareOutputDevices.
// The backing will be big enough to hold the largest size returned by a
// client's GetViewportPixelSize().
class VIZ_SERVICE_EXPORT OutputDeviceBacking {
 public:
  class Client {
   public:
    virtual const gfx::Size& GetViewportPixelSize() const = 0;
    virtual void ReleaseCanvas() = 0;

   protected:
    virtual ~Client() = default;
  };

  OutputDeviceBacking();

  OutputDeviceBacking(const OutputDeviceBacking&) = delete;
  OutputDeviceBacking& operator=(const OutputDeviceBacking&) = delete;

  virtual ~OutputDeviceBacking();

  void RegisterClient(Client* client);
  void UnregisterClient(Client* client);

  // Called when a client has resized. Clients should call Resize() after being
  // registered when they have a valid size. Will potentially invalidate
  // SharedMemory and call ReleaseCanvas() on clients.
  void ClientResized();

  // Requests a SharedMemory segment large enough to fit |viewport_size|. Will
  // return null if |viewport_size| is too large to safely allocate
  // a shared memory region.
  base::UnsafeSharedMemoryRegion* GetSharedMemoryRegion(
      const gfx::Size& viewport_size);

  // Returns the maximum size in bytes needed for the largest viewport from
  // registered clients.
  size_t GetMaxViewportBytes();

  // Retrieve the ID3D11Device and IDCompositionDevice to use for presentation.
  virtual HRESULT GetOrCreateDXObjects(
      Microsoft::WRL::ComPtr<ID3D11Device>* d3d11_device,
      Microsoft::WRL::ComPtr<IDXGIFactory2>* dxgi_factory,
      Microsoft::WRL::ComPtr<IDCompositionDevice>* dcomp_device);

  // Returns the D3D11 staging texture or creates one if it doesn't exist.
  virtual Microsoft::WRL::ComPtr<ID3D11Texture2D> GetOrCreateStagingTexture();

  // Returns the size needed for the largest viewport from registered clients,
  // in pixels.
  gfx::Size GetMaxViewportSize() const;

 private:
  std::vector<raw_ptr<Client, VectorExperimental>> clients_;
  base::UnsafeSharedMemoryRegion region_;
  size_t created_shm_bytes_ = 0;

  // The following are used to cache resources that are common to all
  // software output device backings.
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  Microsoft::WRL::ComPtr<IDXGIFactory2> dxgi_factory_;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_staging_texture_;
  Microsoft::WRL::ComPtr<IDCompositionDevice> dcomp_device_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_DEVICE_BACKING_H_
