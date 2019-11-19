// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_DEVICE_BACKING_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_DEVICE_BACKING_H_

#include <memory>
#include <vector>

#include "base/macros.h"
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
  ~OutputDeviceBacking();

  void RegisterClient(Client* client);
  void UnregisterClient(Client* client);

  // Called when a client has resized. Clients should call Resize() after being
  // registered when they have a valid size. Will potential invalidate
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

 private:
  std::vector<Client*> clients_;
  base::UnsafeSharedMemoryRegion region_;
  size_t created_shm_bytes_ = 0;

  DISALLOW_COPY_AND_ASSIGN(OutputDeviceBacking);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_DEVICE_BACKING_H_
